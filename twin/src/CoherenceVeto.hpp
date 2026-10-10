#pragma once

#include <vector>
#include <complex>
#include <cmath>
#include <algorithm>

namespace nexrx {

template <int BLOCK_SIZE = 512>
class CoherenceVeto {
  static_assert((BLOCK_SIZE & (BLOCK_SIZE - 1)) == 0, "BLOCK_SIZE must be a power of 2");
  static constexpr int HOP_SIZE = BLOCK_SIZE / 2;

public:
  explicit CoherenceVeto(bool enabled = true, double exponent = 2.0)
    : vetoEnabled(enabled)
    , vetoExponent(exponent) {
    constexpr double piVal = 3.14159265358979323846;
    window.resize(BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE; ++i) {
      window[i] = std::sin(piVal * (static_cast<double>(i) + 0.5) / static_cast<double>(BLOCK_SIZE));
    }

    int log2N = 0;
    while ((1 << log2N) < BLOCK_SIZE) {
      log2N++;
    }

    bitRev.resize(BLOCK_SIZE);
    for (int i = 0; i < BLOCK_SIZE; ++i) {
      int rev = 0;
      for (int b = 0; b < log2N; ++b) {
        if ((i & (1 << b)) != 0) {
          rev |= (1 << (log2N - 1 - b));
        }
      }
      bitRev[i] = rev;
    }

    twiddlesForward.reserve(BLOCK_SIZE);
    twiddlesInverse.reserve(BLOCK_SIZE);
    for (int len = 2; len <= BLOCK_SIZE; len <<= 1) {
      double ang = 2.0 * piVal / static_cast<double>(len);
      for (int j = 0; j < len / 2; ++j) {
        twiddlesForward.push_back(std::complex<double>(std::cos(-ang * j), std::sin(-ang * j)));
        twiddlesInverse.push_back(std::complex<double>(std::cos(ang * j), std::sin(ang * j)));
      }
    }

    history0.assign(HOP_SIZE, std::complex<double>(0.0, 0.0));
    history1.assign(HOP_SIZE, std::complex<double>(0.0, 0.0));
    overlapBuf.assign(BLOCK_SIZE, std::complex<double>(0.0, 0.0));

    fftBlock0.resize(BLOCK_SIZE);
    fftBlock1.resize(BLOCK_SIZE);
    fftCombined.resize(BLOCK_SIZE);
  }

  void setEnabled(bool enabled) {
    vetoEnabled = enabled;
  }

  bool isEnabled() const {
    return vetoEnabled;
  }

  void setExponent(double exp) {
    vetoExponent = exp;
  }

  void reset() {
    std::fill(history0.begin(), history0.end(), std::complex<double>(0.0, 0.0));
    std::fill(history1.begin(), history1.end(), std::complex<double>(0.0, 0.0));
    std::fill(overlapBuf.begin(), overlapBuf.end(), std::complex<double>(0.0, 0.0));
  }

  void processBatch(const double* s0I, const double* s0Q,
                    const double* s1I, const double* s1Q,
                    double* outI, double* outQ, int count) {
    if (!vetoEnabled) {
      // Fallback to standard coherent summation
      for (int s = 0; s < count; ++s) {
        outI[s] = (s0I[s] + s1I[s]) * 0.5;
        outQ[s] = (s0Q[s] + s1Q[s]) * 0.5;
      }
      return;
    }

    int nHops = count / HOP_SIZE;
    for (int h = 0; h < nHops; ++h) {
      int hopOffset = h * HOP_SIZE;

      for (int n = 0; n < BLOCK_SIZE; ++n) {
        std::complex<double> val0;
        std::complex<double> val1;

        if (n < HOP_SIZE) {
          if (h == 0) {
            val0 = history0[n];
            val1 = history1[n];
          } else {
            int prevIdx = hopOffset - HOP_SIZE + n;
            val0 = std::complex<double>(s0I[prevIdx], s0Q[prevIdx]);
            val1 = std::complex<double>(s1I[prevIdx], s1Q[prevIdx]);
          }
        } else {
          int curIdx = hopOffset + (n - HOP_SIZE);
          val0 = std::complex<double>(s0I[curIdx], s0Q[curIdx]);
          val1 = std::complex<double>(s1I[curIdx], s1Q[curIdx]);
        }

        double w = window[n];
        fftBlock0[n] = val0 * w;
        fftBlock1[n] = val1 * w;
      }

      runFFT(fftBlock0, false);
      runFFT(fftBlock1, false);

      for (int k = 0; k < BLOCK_SIZE; ++k) {
        double m0 = std::abs(fftBlock0[k]);
        double m1 = std::abs(fftBlock1[k]);

        // Dual-OSD cross-channel coherence metric
        constexpr double epsilon = 1e-12;
        double denom = m0 * m0 + m1 * m1 + epsilon;
        double coherence = (2.0 * m0 * m1) / denom;

        double mask = std::pow(coherence, vetoExponent);
        std::complex<double> avg = (fftBlock0[k] + fftBlock1[k]) * 0.5;
        fftCombined[k] = avg * mask;
      }

      runFFT(fftCombined, true);

      for (int n = 0; n < BLOCK_SIZE; ++n) {
        overlapBuf[n] += fftCombined[n] * window[n];
      }

      for (int n = 0; n < HOP_SIZE; ++n) {
        int outIdx = hopOffset + n;
        outI[outIdx] = overlapBuf[n].real();
        outQ[outIdx] = overlapBuf[n].imag();
      }

      for (int n = 0; n < HOP_SIZE; ++n) {
        overlapBuf[n] = overlapBuf[n + HOP_SIZE];
        overlapBuf[n + HOP_SIZE] = std::complex<double>(0.0, 0.0);
      }
    }

    // Save trailing samples to history for the next batch
    int lastHopTail = count - HOP_SIZE;
    for (int n = 0; n < HOP_SIZE; ++n) {
      history0[n] = std::complex<double>(s0I[lastHopTail + n], s0Q[lastHopTail + n]);
      history1[n] = std::complex<double>(s1I[lastHopTail + n], s1Q[lastHopTail + n]);
    }
  }

private:
  void runFFT(std::vector<std::complex<double>>& data, bool invert) {
    for (int i = 0; i < BLOCK_SIZE; ++i) {
      int rev = bitRev[i];
      if (i < rev) {
        std::swap(data[i], data[rev]);
      }
    }

    const auto& twiddles = invert ? twiddlesInverse : twiddlesForward;
    int twiddleIdx = 0;

    for (int len = 2; len <= BLOCK_SIZE; len <<= 1) {
      int halfLen = len / 2;
      for (int i = 0; i < BLOCK_SIZE; i += len) {
        for (int j = 0; j < halfLen; ++j) {
          const auto& w = twiddles[twiddleIdx + j];
          std::complex<double> u = data[i + j];
          std::complex<double> v = data[i + j + halfLen] * w;
          data[i + j] = u + v;
          data[i + j + halfLen] = u - v;
        }
      }
      twiddleIdx += halfLen;
    }

    if (invert) {
      constexpr double invScale = 1.0 / static_cast<double>(BLOCK_SIZE);
      for (int i = 0; i < BLOCK_SIZE; ++i) {
        data[i] *= invScale;
      }
    }
  }

  bool vetoEnabled{true};
  double vetoExponent{2.0};

  std::vector<double> window;
  std::vector<int> bitRev;
  std::vector<std::complex<double>> twiddlesForward;
  std::vector<std::complex<double>> twiddlesInverse;

  std::vector<std::complex<double>> history0;
  std::vector<std::complex<double>> history1;
  std::vector<std::complex<double>> overlapBuf;

  std::vector<std::complex<double>> fftBlock0;
  std::vector<std::complex<double>> fftBlock1;
  std::vector<std::complex<double>> fftCombined;
};

} // namespace nexrx
