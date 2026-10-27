/**
 * @file RiverRenderer.cpp
 * @brief Raylib-based River and spectral display renderer
 */

#include "RiverRenderer.hpp"
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <cstring>

RiverRenderer::RiverRenderer() {
  initDefaultColormap();
}

RiverRenderer::~RiverRenderer() {
  shutdown();
}

bool RiverRenderer::init(int frequencyBins, int historySlices) {
  if (initialized) {
    shutdown();
  }

  freqBins = frequencyBins;
  historyDepth = historySlices;

  rows.resize(historyDepth);
  for (int i = 0; i < historyDepth; ++i) {
    rows[i].resize(freqBins, minDB);
  }
  topRow = 0;

  // River texture: width = historyDepth (time), height = freqBins (frequency)
  textureData.resize(historyDepth * freqBins, 0xFF000000);

  Image img = {
    .data = textureData.data(),
    .width = historyDepth,
    .height = freqBins,
    .mipmaps = 1,
    .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
  };

  riverTexture = LoadTextureFromImage(img);
  SetTextureFilter(riverTexture, TEXTURE_FILTER_BILINEAR);
  SetTextureWrap(riverTexture, TEXTURE_WRAP_CLAMP);

  textureDirty = true;
  initialized = true;
  return true;
}

void RiverRenderer::shutdown() {
  if (!initialized) {
    return;
  }

  if (riverTexture.id != 0) {
    UnloadTexture(riverTexture);
    riverTexture = { 0 };
  }

  rows.clear();
  textureData.clear();
  initialized = false;
}

void RiverRenderer::addRow(const float* data, int count) {
  if (!initialized || !data || count <= 0) {
    return;
  }

  topRow = (topRow + 1) % historyDepth;

  int copyCount = std::min(count, freqBins);
  std::memcpy(rows[topRow].data(), data, copyCount * sizeof(float));

  if (copyCount < freqBins) {
    std::fill(rows[topRow].begin() + copyCount, rows[topRow].end(), minDB);
  }

  textureDirty = true;
}

void RiverRenderer::horizontalShift(int bins) {
  if (!initialized || bins == 0) {
    return;
  }

  for (auto& row : rows) {
    if (bins > 0) {
      if (bins >= freqBins) {
        std::fill(row.begin(), row.end(), minDB);
      } else {
        std::move_backward(row.begin(), row.end() - bins, row.end());
        std::fill(row.begin(), row.begin() + bins, minDB);
      }
    } else {
      int absBins = -bins;
      if (absBins >= freqBins) {
        std::fill(row.begin(), row.end(), minDB);
      } else {
        std::move(row.begin() + absBins, row.end(), row.begin());
        std::fill(row.end() - absBins, row.end(), minDB);
      }
    }
  }
  textureDirty = true;
}

void RiverRenderer::updateRiverTexture() {
  if (!initialized || !textureDirty) {
    return;
  }

  // Texture layout:
  // X axis (column t in [0, historyDepth - 1]) = Time. t = historyDepth - 1 is NOW.
  // Y axis (row f in [0, freqBins - 1]) = Frequency. f = 0 is highest freq, f = freqBins - 1 is lowest freq.
  for (int t = 0; t < historyDepth; ++t) {
    int bufRow = (topRow - (historyDepth - 1 - t) + historyDepth) % historyDepth;
    const auto& row = rows[bufRow];

    for (int f = 0; f < freqBins; ++f) {
      int binIdx = freqBins - 1 - f;
      textureData[f * historyDepth + t] = dbToColor(row[binIdx]);
    }
  }

  UpdateTexture(riverTexture, textureData.data());
  textureDirty = false;
}

void RiverRenderer::renderRiver(float x, float y, float w, float h,
                                float timeSpan, float timeOffset,
                                float freqMinNorm, float freqMaxNorm) {
  if (!initialized) {
    return;
  }

  if (textureDirty) {
    updateRiverTexture();
  }

  float span = std::clamp(timeSpan, 0.01f, 1.0f);
  float offset = std::clamp(timeOffset, 0.0f, 1.0f - span);
  float fMin = std::clamp(freqMinNorm, 0.0f, 1.0f);
  float fMax = std::clamp(freqMaxNorm, fMin + 0.001f, 1.0f);

  // Time in texture coordinates:
  // t = 0 is oldest, t = historyDepth is NOW.
  // When offset == 0, we see the most recent 'span' fraction:
  float tEnd = (1.0f - offset) * (float)historyDepth;
  float tStart = tEnd - span * (float)historyDepth;

  // Frequency in texture coordinates:
  // Y = 0 is fMax = 1.0, Y = freqBins is fMin = 0.0
  float yStart = (1.0f - fMax) * (float)freqBins;
  float yEnd = (1.0f - fMin) * (float)freqBins;

  Rectangle source = { tStart, yStart, tEnd - tStart, yEnd - yStart };
  Rectangle dest = { x, y, w, h };

  DrawTexturePro(riverTexture, source, dest, { 0, 0 }, 0.0f, WHITE);
}

void RiverRenderer::renderLiveTrace(const float* data, int count,
                                    float x, float y, float w, float h,
                                    float freqMinNorm, float freqMaxNorm) {
  if (!data || count <= 0 || w <= 0.0f || h <= 0.0f) {
    return;
  }

  float fMin = std::clamp(freqMinNorm, 0.0f, 1.0f);
  float fMax = std::clamp(freqMaxNorm, fMin + 0.001f, 1.0f);

  int startBin = (int)(fMin * (float)(count - 1));
  int endBin = (int)(fMax * (float)(count - 1));
  if (endBin <= startBin) {
    endBin = startBin + 1;
  }

  int numPoints = endBin - startBin + 1;
  std::vector<Vector2> points;
  points.reserve(numPoints);

  float dbRange = maxDB - minDB;
  if (dbRange <= 0.001f) {
    dbRange = 1.0f;
  }

  for (int i = 0; i < numPoints; ++i) {
    int bin = startBin + i;
    float normFreq = (float)(bin - startBin) / (float)(numPoints - 1);
    // Vertical position: frequency increases upwards
    float py = y + h - (normFreq * h);

    float db = std::clamp(data[bin], minDB, maxDB);
    float normAmp = (db - minDB) / dbRange;
    // Horizontal position: amplitude extends rightwards
    float px = x + (normAmp * w);

    points.push_back({ px, py });
  }

  // Tektronix phosphor rendering:
  // 1. Wide low-alpha glow
  Color bloomColor = { 70, 255, 120, 75 };
  for (int i = 0; i < (int)points.size() - 1; ++i) {
    DrawLineEx(points[i], points[i + 1], 3.5f, bloomColor);
  }

  // 2. Crisp bright core
  Color coreColor = { 220, 255, 230, 240 };
  for (int i = 0; i < (int)points.size() - 1; ++i) {
    DrawLineEx(points[i], points[i + 1], 1.5f, coreColor);
  }
}

void RiverRenderer::render(float x, float y, float w, float h, float zoom, float center) {
  if (!initialized) {
    return;
  }

  // Legacy waterfall rendering: render River flipped horizontally/vertically for compatibility
  renderRiver(x, y, w, h, 1.0f / zoom, 0.0f, 0.0f, 1.0f);
}

void RiverRenderer::renderSpectrum(const float* data, int count, float x, float y, float w, float h) {
  if (!data || count <= 0) {
    return;
  }

  DrawRectangleRec({ x, y, w, h }, Color{ 13, 13, 20, 255 });

  Color gridColor = Color{ 51, 51, 64, 204 };
  for (int i = 1; i < 5; ++i) {
    float gy = y + h * (float)i / 5.0f;
    DrawLineEx({ x, gy }, { x + w, gy }, 1.0f, gridColor);
  }
  for (int i = 1; i < 10; ++i) {
    if (i == 5) continue;
    float gx = x + w * (float)i / 10.0f;
    DrawLineEx({ gx, y }, { gx, y + h }, 1.0f, gridColor);
  }

  float dbRange = maxDB - minDB;
  if (dbRange <= 0.0f) dbRange = 1.0f;

  std::vector<Vector2> points;
  points.reserve(count);
  for (int i = 0; i < count; ++i) {
    float normX = (float)i / (float)(count - 1);
    float px = x + normX * w;
    float db = std::clamp(data[i], minDB, maxDB);
    float normY = (db - minDB) / dbRange;
    float py = y + h - (normY * h);
    points.push_back({ px, py });
  }

  for (int i = 0; i < (int)points.size() - 1; ++i) {
    DrawLineEx(points[i], points[i + 1], 2.0f, Color{ 64, 160, 255, 255 });
  }
}

void RiverRenderer::setColormapData(const std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>>& stops) {
  if (stops.size() < 2) {
    return;
  }

  colormapLUT.resize(256);

  for (int i = 0; i < 256; ++i) {
    float pos = (float)i / 255.0f;

    size_t rightIdx = 0;
    while (rightIdx < stops.size() && std::get<0>(stops[rightIdx]) < pos) {
      ++rightIdx;
    }

    if (rightIdx == 0) {
      auto [p, r, g, b] = stops[0];
      colormapLUT[i] = (255 << 24) | (b << 16) | (g << 8) | r;
    } else if (rightIdx >= stops.size()) {
      auto [p, r, g, b] = stops.back();
      colormapLUT[i] = (255 << 24) | (b << 16) | (g << 8) | r;
    } else {
      auto [p0, r0, g0, b0] = stops[rightIdx - 1];
      auto [p1, r1, g1, b1] = stops[rightIdx];
      float t = (p1 > p0) ? (pos - p0) / (p1 - p0) : 0.0f;

      uint8_t r = (uint8_t)(r0 + t * (r1 - r0));
      uint8_t g = (uint8_t)(g0 + t * (g1 - g0));
      uint8_t b = (uint8_t)(b0 + t * (b1 - b0));

      colormapLUT[i] = (255 << 24) | (b << 16) | (g << 8) | r;
    }
  }

  textureDirty = true;
}

void RiverRenderer::setRange(float minDBVal, float maxDBVal) {
  minDB = minDBVal;
  maxDB = maxDBVal;
  textureDirty = true;
}

void RiverRenderer::initDefaultColormap() {
  std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>> defaultStops = {
    { 0.00f,   0,   0,  32 },
    { 0.25f,   0,  64, 128 },
    { 0.50f,   0, 180, 180 },
    { 0.75f, 255, 200,   0 },
    { 1.00f, 255, 255, 255 }
  };
  setColormapData(defaultStops);
}

uint32_t RiverRenderer::dbToColor(float db) {
  float norm = (db - minDB) / (maxDB - minDB);
  norm = std::clamp(norm, 0.0f, 1.0f);
  int idx = (int)(norm * 255.0f);
  return colormapLUT[idx];
}
