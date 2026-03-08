// NexRx Digital Twin - SSB Signal Generator Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "SSBGenerator.hpp"
#include "TTSEngine.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <sstream>

namespace nexrx {

SSBGenerator::SSBGenerator(double carrierHz, double amplitudeV, Mode mode)
  : carrierHz(carrierHz)
  , amplitudeV(amplitudeV)
  , mode(mode) {
  initHilbertFilter();
}

SSBGenerator::~SSBGenerator() = default;

void SSBGenerator::initHilbertFilter() {
  // Design FIR Hilbert transformer using windowed sinc
  // Odd-length filter with zero at DC and Nyquist
  hilbertCoeffs.resize(HILBERT_TAPS);
  hilbertHistory.resize(HILBERT_TAPS, 0.0);

  int center = HILBERT_TAPS / 2;

  for (size_t i = 0; i < HILBERT_TAPS; ++i) {
    int n = static_cast<int>(i) - center;
    if (n == 0) {
      hilbertCoeffs[i] = 0.0;  // Zero at center
    } else if (n % 2 == 0) {
      hilbertCoeffs[i] = 0.0;  // Zero at even samples
    } else {
      // Hilbert: h[n] = 2/(pi*n) for odd n
      hilbertCoeffs[i] = 2.0 / (M_PI * n);
    }

    // Apply Blackman window
    double w = 0.42 - 0.5 * std::cos(2.0 * M_PI * i / (HILBERT_TAPS - 1))
                   + 0.08 * std::cos(4.0 * M_PI * i / (HILBERT_TAPS - 1));
    hilbertCoeffs[i] *= w;
  }
}

void SSBGenerator::setTones(const std::vector<double>& audioFreqsHz) {
  audioSource = AudioSource::Tones;
  tones.clear();

  if (audioFreqsHz.empty()) {
    return;
  }

  // Equal amplitude for all tones, normalized so sum = 1
  double amp = 1.0 / audioFreqsHz.size();
  for (double freq : audioFreqsHz) {
    tones.push_back({freq, amp});
  }
}

void SSBGenerator::setVoice(std::shared_ptr<TTSEngine> ttsIn, bool repeat) {
  audioSource = AudioSource::Voice;
  tts = std::move(ttsIn);
  voiceRepeat = repeat;
}

// Two-stage upsampling: 2x FFT (exact, no images) + 5x polyphase sinc
static std::vector<float> upsampleBuffer(const std::vector<float>& input, double inputRate, double outputRate, bool repeat) {
  if (input.empty() || inputRate <= 0 || outputRate <= inputRate) {
    return input;
  }

  size_t inputLen = input.size();
  size_t stage1Len = inputLen * 2;

  size_t fftSize = 1;
  while (fftSize < inputLen) {
    fftSize <<= 1;
  }

  std::vector<double> re(fftSize * 2, 0.0);
  std::vector<double> im(fftSize * 2, 0.0);

  for (size_t i = 0; i < inputLen; ++i) {
    re[i] = input[i];
  }

  auto fftInPlace = [](double* reIn, double* imIn, size_t n, bool inverse) {
    for (size_t i = 1, j = 0; i < n; ++i) {
      size_t bit = n >> 1;
      while (j & bit) {
        j ^= bit;
        bit >>= 1;
      }
      j ^= bit;
      if (i < j) {
        std::swap(reIn[i], reIn[j]);
        std::swap(imIn[i], imIn[j]);
      }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
      double angle = (inverse ? 2.0 : -2.0) * M_PI / len;
      double wRe = std::cos(angle), wIm = std::sin(angle);
      for (size_t i = 0; i < n; i += len) {
        double uRe = 1.0, uIm = 0.0;
        for (size_t j = 0; j < len / 2; ++j) {
          size_t a = i + j, b = i + j + len / 2;
          double tRe = reIn[b] * uRe - imIn[b] * uIm;
          double tIm = reIn[b] * uIm + imIn[b] * uRe;
          reIn[b] = reIn[a] - tRe;
          imIn[b] = imIn[a] - tIm;
          reIn[a] += tRe;
          imIn[a] += tIm;
          double newURe = uRe * wRe - uIm * wIm;
          uIm = uRe * wIm + uIm * wRe;
          uRe = newURe;
        }
      }
    }
    if (inverse) {
      double scale = 1.0 / n;
      for (size_t i = 0; i < n; ++i) {
        reIn[i] *= scale;
        imIn[i] *= scale;
      }
    }
  };

  fftInPlace(re.data(), im.data(), fftSize, false);

  std::vector<double> re2(fftSize * 2, 0.0);
  std::vector<double> im2(fftSize * 2, 0.0);
  
  // DC to just before Nyquist
  for (size_t k = 0; k < fftSize / 2; ++k) {
    re2[k] = re[k];
    im2[k] = im[k];
  }
  
  // Split Nyquist energy correctly between +fs/2 and -fs/2
  re2[fftSize / 2] = re[fftSize / 2] * 0.5;
  im2[fftSize / 2] = im[fftSize / 2] * 0.5;
  re2[fftSize * 2 - fftSize / 2] = re[fftSize / 2] * 0.5;
  im2[fftSize * 2 - fftSize / 2] = im[fftSize / 2] * 0.5;

  // Mirror negative frequencies
  for (size_t k = fftSize / 2 + 1; k < fftSize; ++k) {
    re2[fftSize * 2 - (fftSize - k)] = re[k];
    im2[fftSize * 2 - (fftSize - k)] = im[k];
  }

  fftInPlace(re2.data(), im2.data(), fftSize * 2, true);

  std::vector<float> stage1(stage1Len);
  for (size_t i = 0; i < stage1Len; ++i) {
    stage1[i] = static_cast<float>(re2[i] * 2.0);
  }

  const int upsample = 5;
  const int filterHalf = 12;
  const int taps = 2 * filterHalf + 1;
  const double kaiserBeta = 8.0;
  const double cutoff = 0.8 / upsample; // Lower cutoff to suppress Nyquist artifacts

  std::vector<std::vector<double>> polyphase(upsample, std::vector<double>(taps));
  std::vector<double> phaseNorm(upsample);

  auto besselI0 = [](double z) {
    double sum = 1.0, term = 1.0, z2 = z * z / 4.0;
    for (int k = 1; k < 25; ++k) {
      term *= z2 / (k * k);
      sum += term;
      if (term < 1e-12) {
        break;
      }
    }
    return sum;
  };
  double i0Beta = besselI0(kaiserBeta);

  for (int phase = 0; phase < upsample; ++phase) {
    double frac = phase / static_cast<double>(upsample);
    double weightSum = 0.0;
    for (int tap = 0; tap < taps; ++tap) {
      int k = tap - filterHalf;
      double t = k - frac;
      double sinc = (std::abs(t) < 1e-9) ? 1.0 : std::sin(M_PI * t * 2 * cutoff) / (M_PI * t);
      double x = 2.0 * tap / (taps - 1) - 1.0;
      double window = besselI0(kaiserBeta * std::sqrt(std::max(0.0, 1.0 - x * x))) / i0Beta;
      polyphase[phase][tap] = sinc * window;
      weightSum += polyphase[phase][tap];
    }
    phaseNorm[phase] = (weightSum > 0) ? 1.0 / weightSum : 0.0;
  }

  size_t outputLen = stage1Len * upsample;
  std::vector<float> output(outputLen);
  int stage1Size = static_cast<int>(stage1.size());

  for (size_t in = 0; in < stage1Len; ++in) {
    for (int phase = 0; phase < upsample; ++phase) {
      double sum = 0.0;
      const auto& coeffs = polyphase[phase];
      for (int tap = 0; tap < taps; ++tap) {
        int srcIdx = static_cast<int>(in) + tap - filterHalf;
        float srcSample;
        if (srcIdx < 0 || srcIdx >= stage1Size) {
          if (repeat) {
            srcIdx = ((srcIdx % stage1Size) + stage1Size) % stage1Size;
            srcSample = stage1[srcIdx];
          } else {
            srcSample = 0.0f;
          }
        } else {
          srcSample = stage1[srcIdx];
        }
        sum += srcSample * coeffs[tap];
      }
      output[in * upsample + phase] = static_cast<float>(sum * phaseNorm[phase]);
    }
  }

  return output;
}

void SSBGenerator::setAudioSamples(std::vector<float> samples, double sampleRate, bool repeat) {
  audioSource = AudioSource::Samples;
  samplesRepeat = repeat;

  resampleToInternalRate(samples, sampleRate);
  precomputeHilbert();

  if (samplesRepeat && !audioSamples.empty()) {
    applyLoopCrossfade();
  }

  audioSamples = upsampleBuffer(audioSamples, 48000.0, 480000.0, samplesRepeat);
  audioSamplesQ = upsampleBuffer(audioSamplesQ, 48000.0, 480000.0, samplesRepeat);
  audioSampleRate = 480000.0;

  std::fill(hilbertHistory.begin(), hilbertHistory.end(), 0.0);
  hilbertIndex = 0;
  lastSampleTime = -1.0;
}

void SSBGenerator::precomputeHilbert() {
  if (audioSamples.empty()) {
    audioSamplesQ.clear();
    return;
  }

  size_t n = audioSamples.size();
  size_t fftSize = 1;
  while (fftSize < n) {
    fftSize <<= 1;
  }

  std::vector<double> re(fftSize, 0.0);
  std::vector<double> im(fftSize, 0.0);

  for (size_t i = 0; i < fftSize; ++i) {
    re[i] = (i < n) ? audioSamples[i] : (samplesRepeat ? audioSamples[i % n] : 0.0);
  }

  auto fftInPlace = [](double* reIn, double* imIn, size_t n, bool inverse) {
    for (size_t i = 1, j = 0; i < n; ++i) {
      size_t bit = n >> 1;
      while (j & bit) {
        j ^= bit;
        bit >>= 1;
      }
      j ^= bit;
      if (i < j) {
        std::swap(reIn[i], reIn[j]);
        std::swap(imIn[i], imIn[j]);
      }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
      double angle = (inverse ? 2.0 : -2.0) * M_PI / len;
      double wRe = std::cos(angle), wIm = std::sin(angle);
      for (size_t i = 0; i < n; i += len) {
        double uRe = 1.0, uIm = 0.0;
        for (size_t j = 0; j < len / 2; ++j) {
          size_t a = i + j, b = i + j + len / 2;
          double tRe = reIn[b] * uRe - imIn[b] * uIm;
          double tIm = reIn[b] * uIm + imIn[b] * uRe;
          reIn[b] = reIn[a] - tRe;
          imIn[b] = imIn[a] - tIm;
          reIn[a] += tRe;
          imIn[a] += tIm;
          double newURe = uRe * wRe - uIm * wIm;
          uIm = uRe * wIm + uIm * wRe;
          uRe = newURe;
        }
      }
    }
    if (inverse) {
      double scale = 1.0 / n;
      for (size_t i = 0; i < n; ++i) {
        reIn[i] *= scale;
        imIn[i] *= scale;
      }
    }
  };

  fftInPlace(re.data(), im.data(), fftSize, false);

  for (size_t k = 1; k < fftSize / 2; ++k) {
    re[k] *= 2.0;
    im[k] *= 2.0;
  }
  for (size_t k = fftSize / 2 + 1; k < fftSize; ++k) {
    re[k] = 0.0;
    im[k] = 0.0;
  }

  fftInPlace(re.data(), im.data(), fftSize, true);

  audioSamplesQ.resize(n);
  for (size_t i = 0; i < n; ++i) {
    audioSamplesQ[i] = static_cast<float>(im[i]);
  }
}

void SSBGenerator::resampleToInternalRate(const std::vector<float>& input, double inputRate) {
  constexpr double internalRate = 48000.0;

  if (input.empty() || inputRate <= 0) {
    audioSamples.clear();
    audioSampleRate = internalRate;
    return;
  }

  if (std::abs(inputRate - internalRate) < 1.0) {
    audioSamples = input;
    audioSampleRate = internalRate;
    return;
  }

  double ratio = internalRate / inputRate;
  audioSampleRate = internalRate;

  auto fftInPlace = [](double* reIn, double* imIn, size_t n, bool inverse) {
    for (size_t i = 1, j = 0; i < n; ++i) {
      size_t bit = n >> 1;
      while (j & bit) {
        j ^= bit;
        bit >>= 1;
      }
      j ^= bit;
      if (i < j) {
        std::swap(reIn[i], reIn[j]);
        std::swap(imIn[i], imIn[j]);
      }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
      double angle = (inverse ? 2.0 : -2.0) * M_PI / len;
      double wRe = std::cos(angle), wIm = std::sin(angle);
      for (size_t i = 0; i < n; i += len) {
        double uRe = 1.0, uIm = 0.0;
        for (size_t j = 0; j < len / 2; ++j) {
          size_t a = i + j, b = i + j + len / 2;
          double tRe = reIn[b] * uRe - imIn[b] * uIm;
          double tIm = reIn[b] * uIm + imIn[b] * uRe;
          reIn[b] = reIn[a] - tRe;
          imIn[b] = imIn[a] - tIm;
          reIn[a] += tRe;
          imIn[a] += tIm;
          double newURe = uRe * wRe - uIm * wIm;
          uIm = uRe * wIm + uIm * wRe;
          uRe = newURe;
        }
      }
    }
    if (inverse) {
      double scale = 1.0 / n;
      for (size_t i = 0; i < n; ++i) {
        reIn[i] *= scale;
        imIn[i] *= scale;
      }
    }
  };

  size_t inputLen = input.size();
  std::vector<float> current = input;

  if (ratio >= 2.0) {
    size_t stage1Len = inputLen * 2;
    size_t fftSize = 1;
    while (fftSize < inputLen) {
      fftSize <<= 1;
    }

    std::vector<double> re(fftSize * 2, 0.0);
    std::vector<double> im(fftSize * 2, 0.0);
    for (size_t i = 0; i < inputLen; ++i) {
      re[i] = current[i];
    }

    fftInPlace(re.data(), im.data(), fftSize, false);

    std::vector<double> re2(fftSize * 2, 0.0);
    std::vector<double> im2(fftSize * 2, 0.0);
    for (size_t k = 0; k <= fftSize / 2; ++k) {
      re2[k] = re[k];
      im2[k] = im[k];
    }
    for (size_t k = fftSize / 2 + 1; k < fftSize; ++k) {
      re2[fftSize * 2 - (fftSize - k)] = re[k];
      im2[fftSize * 2 - (fftSize - k)] = im[k];
    }

    fftInPlace(re2.data(), im2.data(), fftSize * 2, true);

    current.resize(stage1Len);
    for (size_t i = 0; i < stage1Len; ++i) {
      current[i] = static_cast<float>(re2[i] * 2.0);
    }
    inputLen = stage1Len;
    ratio /= 2.0;
  }

  if (ratio > 1.01) {
    int upsample = static_cast<int>(ratio + 0.5);
    const int filterHalf = 12;
    const int taps = 2 * filterHalf + 1;
    const double kaiserBeta = 8.0;
    const double cutoff = 0.45;

    auto besselI0 = [](double z) {
      double sum = 1.0, term = 1.0, z2 = z * z / 4.0;
      for (int k = 1; k < 25; ++k) {
        term *= z2 / (k * k);
        sum += term;
        if (term < 1e-12) {
          break;
        }
      }
      return sum;
    };
    double i0Beta = besselI0(kaiserBeta);

    std::vector<std::vector<double>> polyphase(upsample, std::vector<double>(taps));
    std::vector<double> phaseNorm(upsample);

    for (int phase = 0; phase < upsample; ++phase) {
      double frac = phase / static_cast<double>(upsample);
      double weightSum = 0.0;
      for (int tap = 0; tap < taps; ++tap) {
        int k = tap - filterHalf;
        double t = k - frac;
        double sinc = (std::abs(t) < 1e-9) ? 1.0 : std::sin(M_PI * t * 2 * cutoff) / (M_PI * t);
        double x = 2.0 * tap / (taps - 1) - 1.0;
        double window = besselI0(kaiserBeta * std::sqrt(std::max(0.0, 1.0 - x * x))) / i0Beta;
        polyphase[phase][tap] = sinc * window;
        weightSum += polyphase[phase][tap];
      }
      phaseNorm[phase] = (weightSum > 0) ? 1.0 / weightSum : 0.0;
    }

    size_t outputLen = inputLen * upsample;
    audioSamples.resize(outputLen);
    int currentSize = static_cast<int>(current.size());

    for (size_t in = 0; in < inputLen; ++in) {
      for (int phase = 0; phase < upsample; ++phase) {
        double sum = 0.0;
        const auto& coeffs = polyphase[phase];
        for (int tap = 0; tap < taps; ++tap) {
          int srcIdx = static_cast<int>(in) + tap - filterHalf;
          float srcSample;
          if (srcIdx < 0 || srcIdx >= currentSize) {
            if (samplesRepeat) {
              srcIdx = ((srcIdx % currentSize) + currentSize) % currentSize;
              srcSample = current[srcIdx];
            } else {
              srcSample = 0.0f;
            }
          } else {
            srcSample = current[srcIdx];
          }
          sum += srcSample * coeffs[tap];
        }
        audioSamples[in * upsample + phase] = static_cast<float>(sum * phaseNorm[phase]);
      }
    }
  } else {
    audioSamples = current;
  }
}

void SSBGenerator::applyLoopCrossfade() {
  if (audioSamples.empty()) {
    return;
  }

  constexpr size_t crossfadeSamples = 960;
  size_t fadeLen = std::min(crossfadeSamples, audioSamples.size() / 4);

  if (fadeLen < 2) {
    return;
  }

  size_t n = audioSamples.size();

  float targetI = audioSamples[0];
  for (size_t i = 0; i < fadeLen; ++i) {
    double t = static_cast<double>(i) / (fadeLen - 1);
    double blend = 0.5 * (1.0 - std::cos(M_PI * t));

    size_t endIdx = n - fadeLen + i;
    float endVal = audioSamples[endIdx];

    audioSamples[endIdx] = static_cast<float>(endVal * (1.0 - blend) + targetI * blend);
  }

  if (audioSamplesQ.size() == n) {
    float targetQ = audioSamplesQ[0];
    for (size_t i = 0; i < fadeLen; ++i) {
      double t = static_cast<double>(i) / (fadeLen - 1);
      double blend = 0.5 * (1.0 - std::cos(M_PI * t));

      size_t endIdx = n - fadeLen + i;
      float endVal = audioSamplesQ[endIdx];

      audioSamplesQ[endIdx] = static_cast<float>(endVal * (1.0 - blend) + targetQ * blend);
    }
  }
}

void SSBGenerator::getAudioIQ(double timeS, double& i, double& q) const {
  switch (audioSource) {
    case AudioSource::Tones: {
      i = 0.0;
      q = 0.0;
      for (const auto& tone : tones) {
        double phase = 2.0 * M_PI * tone.freqHz * timeS;
        i += tone.amplitude * std::cos(phase);
        q += tone.amplitude * std::sin(phase);
      }
      break;
    }

    case AudioSource::Voice: {
      if (tts) {
        i = tts->getSample(timeS);
        q = hilbertFilter(timeS);
      } else {
        i = q = 0.0;
      }
      break;
    }

    case AudioSource::Samples: {
      if (audioSamples.empty() || audioSampleRate <= 0) {
        i = q = 0.0;
        break;
      }

      size_t n = audioSamples.size();
      uint64_t totalS = static_cast<uint64_t>(timeS * audioSampleRate + 0.5);
      size_t idx = samplesRepeat ? (totalS % n) : totalS;
      
      if (!samplesRepeat && idx >= n) {
        i = q = 0.0;
        break;
      }

      i = audioSamples[idx];
      q = audioSamplesQ[idx];
      break;
    }

    case AudioSource::None:
    default:
      i = q = 0.0;
      break;
  }
}

double SSBGenerator::hilbertFilter(double timeS) const {
  if (audioSource == AudioSource::Samples && !audioSamples.empty()) {
    double sampleTime = timeS * audioSampleRate;
    if (samplesRepeat) {
      sampleTime = std::fmod(sampleTime, static_cast<double>(audioSamples.size()));
    }

    double result = 0.0;
    int center = HILBERT_TAPS / 2;

    for (size_t i = 0; i < HILBERT_TAPS; ++i) {
      int offset = static_cast<int>(i) - center;
      double sampleIdx = sampleTime + offset;

      if (samplesRepeat) {
        while (sampleIdx < 0) {
          sampleIdx += audioSamples.size();
        }
        while (sampleIdx >= audioSamples.size()) {
          sampleIdx -= audioSamples.size();
        }
      } else {
        if (sampleIdx < 0 || sampleIdx >= audioSamples.size()) {
          continue;
        }
      }

      size_t idx0 = static_cast<size_t>(sampleIdx);
      size_t idx1 = (idx0 + 1) % audioSamples.size();
      double frac = sampleIdx - idx0;
      double sample = audioSamples[idx0] * (1.0 - frac) + audioSamples[idx1] * frac;

      result += hilbertCoeffs[i] * sample;
    }
    return result;
  }

  if (audioSource == AudioSource::Voice && tts) {
    constexpr double ttsSampleRate = 22050.0;
    double result = 0.0;
    int center = HILBERT_TAPS / 2;

    for (size_t i = 0; i < HILBERT_TAPS; ++i) {
      int offset = static_cast<int>(i) - center;
      double sampleTime = timeS + offset / ttsSampleRate;
      double sample = tts->getSample(sampleTime);
      result += hilbertCoeffs[i] * sample;
    }
    return result;
  }

  return 0.0;
}

double SSBGenerator::getSample(double timeS) const {
  if (audioSource == AudioSource::None) {
    return 0.0;
  }

  double audioI, audioQ;
  getAudioIQ(timeS, audioI, audioQ);

  double currentCarrierPhase = 2.0 * M_PI * carrierHz * timeS;
  double cosCarrier = std::cos(currentCarrierPhase);
  double sinCarrier = std::sin(currentCarrierPhase);

  double output;
  if (mode == Mode::USB) {
    output = audioI * cosCarrier - audioQ * sinCarrier;
  } else {
    output = audioI * cosCarrier + audioQ * sinCarrier;
  }

  return amplitudeV * output;
}

std::string SSBGenerator::description() const {
  std::ostringstream oss;
  oss << "SSB[" << carrierHz / 1e6 << "MHz, ";
  oss << (mode == Mode::USB ? "USB" : "LSB") << ", ";

  switch (audioSource) {
    case AudioSource::Tones:
      oss << tones.size() << " tone(s)";
      break;
    case AudioSource::Voice:
      oss << "espeak";
      break;
    case AudioSource::Samples:
      oss << "samples";
      break;
    default:
      oss << "none";
      break;
  }
  oss << "]";
  return oss.str();
}

void SSBGenerator::reset() {
  std::fill(hilbertHistory.begin(), hilbertHistory.end(), 0.0);
  hilbertIndex = 0;
  lastSampleTime = -1.0;
  lastTime = -1.0;
  carrierPhase = 0.0;

  if (tts) {
    tts->reset();
  }
}

void SSBGenerator::getRfIQ(double timeS, double& outI, double& outQ) const {
  if (audioSource == AudioSource::None) {
    outI = outQ = 0.0;
    return;
  }

  double audioI, audioQ;
  getAudioIQ(timeS, audioI, audioQ);

  double phase = 2.0 * M_PI * std::fmod(carrierHz * timeS, 1.0);
  double cosP = std::cos(phase);
  double sinP = std::sin(phase);

  if (mode == Mode::USB) {
    // (audioI + j*audioQ) * exp(j*phase) = (audioI*cos - audioQ*sin) + j(audioI*sin + audioQ*cos)
    outI = amplitudeV * (audioI * cosP - audioQ * sinP);
    outQ = amplitudeV * (audioI * sinP + audioQ * cosP);
  } else {
    // (audioI - j*audioQ) * exp(j*phase) = (audioI*cos + audioQ*sin) + j(audioI*sin - audioQ*cos)
    outI = amplitudeV * (audioI * cosP + audioQ * sinP);
    outQ = amplitudeV * (audioI * sinP - audioQ * cosP);
  }
}

} // namespace nexrx
