/**
 * @file WaterfallRenderer.cpp
 * @brief Waterfall display implementation
 */

#include "WaterfallRenderer.hpp"

#include <SDL_opengl.h>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace {

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

  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  initDefaultColormap();
  initialized = true;
  textureDirty = true;

  return true;
}

void WaterfallRenderer::shutdown() {
  if (textureID != 0) {
    glDeleteTextures(1, &textureID);
    textureID = 0;
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

void WaterfallRenderer::render(float x, float y, float w, float h) {
  if (!initialized) return;

  if (textureDirty) updateTexture();

  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, textureID);
  glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f); glVertex2f(x, y);
  glTexCoord2f(1.0f, 0.0f); glVertex2f(x + w, y);
  glTexCoord2f(1.0f, 1.0f); glVertex2f(x + w, y + h);
  glTexCoord2f(0.0f, 1.0f); glVertex2f(x, y + h);
  glEnd();

  glDisable(GL_TEXTURE_2D);

  glLineWidth(2.0f);
  glColor4f(1.0f, 1.0f, 1.0f, 0.4f); 
  glBegin(GL_LINES);
  glVertex2f(x + w * 0.5f, y);
  glVertex2f(x + w * 0.5f, y + h);
  glEnd();
  glLineWidth(1.0f);
}

void WaterfallRenderer::renderSpectrum(const float* data, int count, float x, float y, float w, float h) {
  if (!data || count <= 0) return;

  // Background
  glColor4f(0.05f, 0.05f, 0.08f, 1.0f);
  glBegin(GL_QUADS);
  glVertex2f(x, y); glVertex2f(x + w, y);
  glVertex2f(x + w, y + h); glVertex2f(x, y + h);
  glEnd();

  // Grid
  glColor4f(0.2f, 0.2f, 0.25f, 0.8f);
  glBegin(GL_LINES);
  for (int i = 1; i < 5; ++i) {
    float gy = y + h * i / 5.0f;
    glVertex2f(x, gy); glVertex2f(x + w, gy);
  }
  for (int i = 1; i < 10; ++i) {
    if (i == 5) continue;
    float gx = x + w * i / 10.0f;
    glVertex2f(gx, y); glVertex2f(gx, y + h);
  }
  glEnd();

  // Center line
  glLineWidth(2.0f);
  glColor4f(1.0f, 1.0f, 1.0f, 0.8f); 
  glBegin(GL_LINES);
  float gxc = x + w * 0.5f;
  glVertex2f(gxc, y); glVertex2f(gxc, y + h);
  glEnd();
  glLineWidth(1.0f);

  float range = maxDB - minDB;
  if (range <= 0) range = 1.0f;
  float binWidth = w / count;

  // Spectrum fill
  glBegin(GL_TRIANGLE_STRIP);
  for (int i = 0; i < count; ++i) {
    float db = std::clamp(data[i], minDB, maxDB);
    float norm = (db - minDB) / range;
    float px = x + (static_cast<float>(i) + 0.5f) * binWidth;
    float py = y + h * (1.0f - norm);

    uint32_t color = dbToColor(db);
    float r = (color & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 16) & 0xFF) / 255.0f;

    glColor4f(r * 0.5f, g * 0.5f, b * 0.5f, 0.6f);
    glVertex2f(px, y + h);
    glColor4f(r, g, b, 0.8f);
    glVertex2f(px, py);
  }
  glEnd();

  // Spectrum line
  glLineWidth(1.5f);
  glBegin(GL_LINE_STRIP);
  for (int i = 0; i < count; ++i) {
    float db = std::clamp(data[i], minDB, maxDB);
    float norm = (db - minDB) / range;
    float px = x + (static_cast<float>(i) + 0.5f) * binWidth;
    float py = y + h * (1.0f - norm);

    uint32_t color = dbToColor(db);
    float r = (color & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 16) & 0xFF) / 255.0f;
    glColor4f(r, g, b, 1.0f);
    glVertex2f(px, py);
  }
  glEnd();
  glLineWidth(1.0f);
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
    int bufferRow = (topRow - row + height) % height;
    const auto& rowData = rows[bufferRow];
    for (int col = 0; col < width; ++col) textureData[row * width + col] = dbToColor(rowData[col]);
  }
  glBindTexture(GL_TEXTURE_2D, textureID);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, textureData.data());
  textureDirty = false;
}

uint32_t WaterfallRenderer::dbToColor(float db) {
  if (colormapLUT.empty()) return packRGBA(255, 255, 255);
  float range = maxDB - minDB;
  float normalized = (range > 0) ? std::clamp((db - minDB) / range, 0.0f, 1.0f) : 0.0f;
  int index = static_cast<int>(normalized * 255);
  return colormapLUT[std::clamp(index, 0, 255)];
}
