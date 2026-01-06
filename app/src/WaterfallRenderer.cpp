/**
 * @file WaterfallRenderer.cpp
 * @brief Waterfall display implementation
 */

#include "WaterfallRenderer.hpp"

#include <SDL_opengl.h>
#include <cmath>
#include <algorithm>
#include <iostream>

// Colormap data (256 entries each, RGBA packed as 0xAABBGGRR for OpenGL)
namespace {

// Helper to pack RGBA
constexpr uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (a << 24) | (b << 16) | (g << 8) | r;
}

// Interpolate between two colors
uint32_t lerpColor(uint32_t c1, uint32_t c2, float t) {
    uint8_t r1 = c1 & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = (c1 >> 16) & 0xFF;
    uint8_t r2 = c2 & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = (c2 >> 16) & 0xFF;
    uint8_t r = r1 + (r2 - r1) * t;
    uint8_t g = g1 + (g2 - g1) * t;
    uint8_t b = b1 + (b2 - b1) * t;
    return packRGBA(r, g, b);
}

// Build colormap from color stops
std::vector<uint32_t> buildGradient(const std::vector<std::pair<float, uint32_t>>& stops) {
    std::vector<uint32_t> lut(256);
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;
        // Find the two stops we're between
        size_t idx = 0;
        while (idx < stops.size() - 1 && stops[idx + 1].first < t) {
            ++idx;
        }
        if (idx >= stops.size() - 1) {
            lut[i] = stops.back().second;
        } else {
            float t0 = stops[idx].first;
            float t1 = stops[idx + 1].first;
            float localT = (t - t0) / (t1 - t0);
            lut[i] = lerpColor(stops[idx].second, stops[idx + 1].second, localT);
        }
    }
    return lut;
}

} // anonymous namespace

WaterfallRenderer::WaterfallRenderer() = default;

WaterfallRenderer::~WaterfallRenderer() {
    shutdown();
}

bool WaterfallRenderer::init(int width, int height) {
    if (initialized_) {
        shutdown();
    }

    width_ = width;
    height_ = height;

    // Initialize row buffer
    rows_.resize(height);
    for (auto& row : rows_) {
        row.resize(width, minDb_);
    }
    topRow_ = 0;

    // Initialize texture data
    textureData_.resize(width * height, packRGBA(0, 0, 0));

    // Create OpenGL texture
    glGenTextures(1, &textureId_);
    glBindTexture(GL_TEXTURE_2D, textureId_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData_.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Build colormap
    buildColormap();

    initialized_ = true;
    textureDirty_ = true;

    std::cout << "Waterfall initialized: " << width << "x" << height << std::endl;
    return true;
}

void WaterfallRenderer::shutdown() {
    if (textureId_ != 0) {
        glDeleteTextures(1, &textureId_);
        textureId_ = 0;
    }
    rows_.clear();
    textureData_.clear();
    initialized_ = false;
}

void WaterfallRenderer::addRow(const float* data, int count) {
    if (!initialized_) return;

    // Move to next row (circular buffer)
    topRow_ = (topRow_ + 1) % height_;

    // Copy data to the new top row
    auto& row = rows_[topRow_];
    int copyCount = std::min(count, width_);
    for (int i = 0; i < copyCount; ++i) {
        row[i] = data[i];
    }
    // Fill remaining with min dB if data is shorter
    for (int i = copyCount; i < width_; ++i) {
        row[i] = minDb_;
    }

    textureDirty_ = true;
}

void WaterfallRenderer::render(float x, float y, float w, float h) {
    if (!initialized_) return;

    // Update texture if needed
    if (textureDirty_) {
        updateTexture();
    }

    // Render textured quad - texture is already in correct order from updateTexture()
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureId_);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
    // Simple quad - texture row 0 is newest, row height-1 is oldest
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x, y);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x + w, y);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x + w, y + h);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x, y + h);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

void WaterfallRenderer::renderSpectrum(const float* data, int count, float x, float y, float w, float h) {
    if (!data || count <= 0) return;

    // Draw background
    glColor4f(0.05f, 0.05f, 0.08f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();

    // Draw grid lines
    glColor4f(0.2f, 0.2f, 0.25f, 1.0f);
    glBegin(GL_LINES);
    // Horizontal grid lines (dB levels)
    for (int i = 1; i < 5; ++i) {
        float gy = y + h * i / 5.0f;
        glVertex2f(x, gy);
        glVertex2f(x + w, gy);
    }
    // Vertical grid lines
    for (int i = 1; i < 10; ++i) {
        float gx = x + w * i / 10.0f;
        glVertex2f(gx, y);
        glVertex2f(gx, y + h);
    }
    glEnd();

    // Draw spectrum line
    float range = maxDb_ - minDb_;
    float binWidth = w / count;

    // Fill under the curve
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i < count; ++i) {
        float db = std::clamp(data[i], minDb_, maxDb_);
        float normalized = (db - minDb_) / range;
        float px = x + (i + 0.5f) * binWidth;
        float py = y + h * (1.0f - normalized);

        // Color based on level
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

    // Draw spectrum line on top
    glLineWidth(1.5f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < count; ++i) {
        float db = std::clamp(data[i], minDb_, maxDb_);
        float normalized = (db - minDb_) / range;
        float px = x + (i + 0.5f) * binWidth;
        float py = y + h * (1.0f - normalized);

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

void WaterfallRenderer::setColormap(WaterfallColormap colormap) {
    colormap_ = colormap;
    buildColormap();
    textureDirty_ = true;
}

void WaterfallRenderer::setRange(float minDb, float maxDb) {
    minDb_ = minDb;
    maxDb_ = maxDb;
    textureDirty_ = true;
}

void WaterfallRenderer::buildColormap() {
    switch (colormap_) {
        case WaterfallColormap::Viridis:
            colormapLUT_ = buildGradient({
                {0.0f, packRGBA(68, 1, 84)},
                {0.25f, packRGBA(59, 82, 139)},
                {0.5f, packRGBA(33, 145, 140)},
                {0.75f, packRGBA(94, 201, 98)},
                {1.0f, packRGBA(253, 231, 37)}
            });
            break;

        case WaterfallColormap::Plasma:
            colormapLUT_ = buildGradient({
                {0.0f, packRGBA(13, 8, 135)},
                {0.25f, packRGBA(126, 3, 168)},
                {0.5f, packRGBA(204, 71, 120)},
                {0.75f, packRGBA(248, 149, 64)},
                {1.0f, packRGBA(240, 249, 33)}
            });
            break;

        case WaterfallColormap::Inferno:
            colormapLUT_ = buildGradient({
                {0.0f, packRGBA(0, 0, 4)},
                {0.25f, packRGBA(87, 16, 110)},
                {0.5f, packRGBA(188, 55, 84)},
                {0.75f, packRGBA(249, 142, 9)},
                {1.0f, packRGBA(252, 255, 164)}
            });
            break;

        case WaterfallColormap::Magma:
            colormapLUT_ = buildGradient({
                {0.0f, packRGBA(0, 0, 4)},
                {0.25f, packRGBA(81, 18, 124)},
                {0.5f, packRGBA(183, 55, 121)},
                {0.75f, packRGBA(254, 159, 109)},
                {1.0f, packRGBA(252, 253, 191)}
            });
            break;

        case WaterfallColormap::GreenPhosphor:
            colormapLUT_ = buildGradient({
                {0.0f, packRGBA(0, 10, 0)},
                {0.3f, packRGBA(0, 60, 0)},
                {0.6f, packRGBA(0, 180, 0)},
                {0.8f, packRGBA(100, 255, 100)},
                {1.0f, packRGBA(200, 255, 200)}
            });
            break;

        case WaterfallColormap::BlueHot:
            colormapLUT_ = buildGradient({
                {0.0f, packRGBA(0, 0, 20)},
                {0.3f, packRGBA(0, 50, 150)},
                {0.6f, packRGBA(50, 150, 255)},
                {0.8f, packRGBA(200, 220, 255)},
                {1.0f, packRGBA(255, 255, 255)}
            });
            break;

        case WaterfallColormap::Grayscale:
        default:
            colormapLUT_ = buildGradient({
                {0.0f, packRGBA(0, 0, 0)},
                {1.0f, packRGBA(255, 255, 255)}
            });
            break;
    }
}

void WaterfallRenderer::updateTexture() {
    if (!initialized_) return;

    // Convert all rows to RGBA using colormap
    for (int row = 0; row < height_; ++row) {
        // Calculate which row in our circular buffer this corresponds to
        // Row 0 in texture = topRow_ (newest)
        // Row height_-1 = oldest
        int bufferRow = (topRow_ - row + height_) % height_;
        const auto& rowData = rows_[bufferRow];

        for (int col = 0; col < width_; ++col) {
            float db = rowData[col];
            textureData_[row * width_ + col] = dbToColor(db);
        }
    }

    // Upload to GPU
    glBindTexture(GL_TEXTURE_2D, textureId_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, textureData_.data());

    textureDirty_ = false;
}

uint32_t WaterfallRenderer::dbToColor(float db) {
    float range = maxDb_ - minDb_;
    float normalized = std::clamp((db - minDb_) / range, 0.0f, 1.0f);
    int index = static_cast<int>(normalized * 255);
    return colormapLUT_[index];
}
