/**
 * @file WaterfallRenderer.cpp
 * @brief Raylib-based waterfall display implementation
 */

#include "WaterfallRenderer.hpp"
#include <rlgl.h>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace {

// Pack RGBA values into a single uint32_t (Raylib format: R8G8B8A8)
constexpr uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
  return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | 
         (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
}

uint32_t lerpColor(uint32_t c1, uint32_t c2, float t) {
  uint8_t r1 = c1 & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = (c1 >> 16) & 0xFF;
  uint8_t r2 = c2 & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = (c2 >> 16) & 0xFF;
  uint8_t r = static_cast<uint8_t>(r1 + (r2 - r1) * t);
  uint8_t g = static_cast<uint8_t>(g1 + (g2 - g1) * t);
  uint8_t b = static_cast<uint8_t>(b1 + (b2 - b1) * t);
  return packRGBA(r, g, b);
}

std::vector<uint32_t> buildGradient(const std::vector<std::pair<float, uint32_t>>& stops) {
  std::vector<uint32_t> lut(256);
  if (stops.empty()) {
    std::fill(lut.begin(), lut.end(), packRGBA(255, 255, 255));
    return lut;
  }
  if (stops.size() == 1) {
    std::fill(lut.begin(), lut.end(), stops[0].second);
    return lut;
  }
  for (int i = 0; i < 256; ++i) {
    float t = i / 255.0f;
    size_t idx = 0;
    while (idx < stops.size() - 1 && stops[idx + 1].first < t) {
      ++idx;
    }
    if (idx >= stops.size() - 1) {
      lut[i] = stops.back().second;
    } else {
      float t0 = stops[idx].first;
      float t1 = stops[idx + 1].first;
      float localT = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0f;
      lut[i] = lerpColor(stops[idx].second, stops[idx + 1].second, std::clamp(localT, 0.0f, 1.0f));
    }
  }
  return lut;
}

} // anonymous namespace

WaterfallRenderer::WaterfallRenderer() {
  colormapLUT.assign(256, packRGBA(255, 255, 255));
}

WaterfallRenderer::~WaterfallRenderer() {
  shutdown();
}

bool WaterfallRenderer::init(int widthIn, int heightIn) {
  if (initialized) {
    shutdown();
  }

  width = widthIn;
  height = heightIn;

  rows.resize(height);
  for (auto& row : rows) {
    row.assign(width, minDB);
  }
  topRow = 0;

  textureData.assign(width * height, packRGBA(0, 0, 0));

  // Load an empty image to create the texture
  Image image = {
    textureData.data(),
    width,
    height,
    1,
    PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
  };
  waterfallTexture = LoadTextureFromImage(image);
  SetTextureFilter(waterfallTexture, TEXTURE_FILTER_BILINEAR);
  SetTextureWrap(waterfallTexture, TEXTURE_WRAP_CLAMP);

  initDefaultColormap();
  initialized = true;
  textureDirty = true;

  return true;
}

void WaterfallRenderer::shutdown() {
  if (waterfallTexture.id != 0) {
    UnloadTexture(waterfallTexture);
    waterfallTexture.id = 0;
  }
  rows.clear();
  textureData.clear();
  initialized = false;
}

void WaterfallRenderer::addRow(const float* data, int count) {
  if (!initialized || !data || count <= 0) return;

  topRow = (topRow + 1) % height;
  auto& row = rows[topRow];
  int copyCount = std::min(count, width);
  for (int i = 0; i < copyCount; ++i) row[i] = data[i];
  for (int i = copyCount; i < width; ++i) row[i] = minDB;

  textureDirty = true;
}

void WaterfallRenderer::horizontalShift(int bins) {
  if (!initialized || bins == 0) return;
  
  for (auto& row : rows) {
    if (bins > 0) {
      if (bins >= width) {
        std::fill(row.begin(), row.end(), minDB);
      } else {
        std::move_backward(row.begin(), row.end() - bins, row.end());
        std::fill(row.begin(), row.begin() + bins, minDB);
      }
    } else {
      int absBins = -bins;
      if (absBins >= width) {
        std::fill(row.begin(), row.end(), minDB);
      } else {
        std::move(row.begin() + absBins, row.end(), row.begin());
        std::fill(row.end() - absBins, row.end(), minDB);
      }
    }
  }
  textureDirty = true;
}

void WaterfallRenderer::render(float x, float y, float w, float h, float zoom, float center) {
  if (!initialized) return;

  if (textureDirty) updateTexture();

  float halfSpan = 0.5f / zoom;
  float s1 = std::clamp(center - halfSpan, 0.0f, 1.0f);
  float s2 = std::clamp(center + halfSpan, 0.0f, 1.0f);

  Rectangle source = { s1 * waterfallTexture.width, 0, (s2 - s1) * waterfallTexture.width, (float)waterfallTexture.height };
  Rectangle dest = { x, y, w, h };
  DrawTexturePro(waterfallTexture, source, dest, {0, 0}, 0, WHITE);

  // Center line
  DrawLineEx({x + w * 0.5f, y}, {x + w * 0.5f, y + h}, 2.0f, Fade(WHITE, 0.4f));
}

void WaterfallRenderer::renderSpectrum(const float* data, int count, float x, float y, float w, float h) {
  if (!data || count <= 0) return;

  // Background
  DrawRectangleRec({x, y, w, h}, Color{13, 13, 20, 255});

  // Grid
  Color gridColor = Color{51, 51, 64, 204};
  for (int i = 1; i < 5; ++i) {
    float gy = y + h * i / 5.0f;
    DrawLineEx({x, gy}, {x + w, gy}, 1.0f, gridColor);
  }
  for (int i = 1; i < 10; ++i) {
    if (i == 5) continue;
    float gx = x + w * i / 10.0f;
    DrawLineEx({gx, y}, {gx, y + h}, 1.0f, gridColor);
  }

  // Center line
  DrawLineEx({x + w * 0.5f, y}, {x + w * 0.5f, y + h}, 2.0f, Fade(WHITE, 0.8f));

  float range = maxDB - minDB;
  if (range <= 0) range = 1.0f;
  float binWidth = w / count;

  // Use rlgl for the complex filled spectrum area
  rlBegin(RL_TRIANGLES);
  for (int i = 0; i < count - 1; ++i) {
    float db1 = std::clamp(data[i], minDB, maxDB);
    float norm1 = (db1 - minDB) / range;
    float px1 = x + (static_cast<float>(i) + 0.5f) * binWidth;
    float py1 = y + h * (1.0f - norm1);

    float db2 = std::clamp(data[i+1], minDB, maxDB);
    float norm2 = (db2 - minDB) / range;
    float px2 = x + (static_cast<float>(i+1) + 0.5f) * binWidth;
    float py2 = y + h * (1.0f - norm2);

    uint32_t c1 = dbToColor(db1);
    Color color1 = { (unsigned char)(c1 & 0xFF), (unsigned char)((c1 >> 8) & 0xFF), (unsigned char)((c1 >> 16) & 0xFF), 150 };
    uint32_t c2 = dbToColor(db2);
    Color color2 = { (unsigned char)(c2 & 0xFF), (unsigned char)((c2 >> 8) & 0xFF), (unsigned char)((c2 >> 16) & 0xFF), 150 };

    // Triangle 1
    rlColor4ub(color1.r/2, color1.g/2, color1.b/2, 150); rlVertex2f(px1, y + h);
    rlColor4ub(color1.r, color1.g, color1.b, 150);       rlVertex2f(px1, py1);
    rlColor4ub(color2.r, color2.g, color2.b, 150);       rlVertex2f(px2, py2);

    // Triangle 2
    rlColor4ub(color1.r/2, color1.g/2, color1.b/2, 150); rlVertex2f(px1, y + h);
    rlColor4ub(color2.r, color2.g, color2.b, 150);       rlVertex2f(px2, py2);
    rlColor4ub(color2.r/2, color2.g/2, color2.b/2, 150); rlVertex2f(px2, y + h);
  }
  rlEnd();

  // Spectrum line
  for (int i = 0; i < count - 1; ++i) {
    float db1 = std::clamp(data[i], minDB, maxDB);
    float norm1 = (db1 - minDB) / range;
    float px1 = x + (static_cast<float>(i) + 0.5f) * binWidth;
    float py1 = y + h * (1.0f - norm1);

    float db2 = std::clamp(data[i+1], minDB, maxDB);
    float norm2 = (db2 - minDB) / range;
    float px2 = x + (static_cast<float>(i+1) + 0.5f) * binWidth;
    float py2 = y + h * (1.0f - norm2);

    uint32_t c = dbToColor(db1);
    Color color = { (unsigned char)(c & 0xFF), (unsigned char)((c >> 8) & 0xFF), (unsigned char)((c >> 16) & 0xFF), 255 };
    DrawLineEx({px1, py1}, {px2, py2}, 1.5f, color);
  }
}

void WaterfallRenderer::setColormapData(const std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>>& stops) {
  if (stops.empty()) return;
  std::vector<std::pair<float, uint32_t>> gradientStops;
  gradientStops.reserve(stops.size());
  for (const auto& [pos, r, g, b] : stops) gradientStops.push_back({pos, packRGBA(r, g, b)});
  colormapLUT = buildGradient(gradientStops);
  textureDirty = true;
}

void WaterfallRenderer::setRange(float minDBIn, float maxDBIn) {
  minDB = minDBIn; maxDB = maxDBIn;
  textureDirty = true;
}

void WaterfallRenderer::initDefaultColormap() {
  colormapLUT = buildGradient({{0.0f, packRGBA(0, 0, 0)}, {1.0f, packRGBA(255, 255, 255)}});
}

void WaterfallRenderer::updateTexture() {
  if (!initialized) return;
  for (int row = 0; row < height; ++row) {
    // Map texture row to circular buffer:
    // row 0 (top) = newest (topRow)
    // row height-1 (bottom) = oldest
    int bufferRow = (topRow - row + height) % height;
    const auto& rowData = rows[bufferRow];
    for (int col = 0; col < width; ++col) {
        textureData[row * width + col] = dbToColor(rowData[col]);
    }
  }
  UpdateTexture(waterfallTexture, textureData.data());
  textureDirty = false;
}

uint32_t WaterfallRenderer::dbToColor(float db) {
  if (colormapLUT.empty()) return packRGBA(255, 255, 255);
  float range = maxDB - minDB;
  float normalized = (range > 0) ? std::clamp((db - minDB) / range, 0.0f, 1.0f) : 0.0f;
  int index = static_cast<int>(normalized * 255);
  return colormapLUT[std::clamp(index, 0, 255)];
}
