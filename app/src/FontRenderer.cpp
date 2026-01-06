/**
 * @file FontRenderer.cpp
 * @brief Font rendering implementation using stb_truetype
 */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include "FontRenderer.hpp"

#include <SDL_opengl.h>
#include <fstream>
#include <iostream>
#include <cmath>

FontRenderer::FontRenderer() {
    glyphs_.resize(96);  // ASCII 32-127
}

FontRenderer::~FontRenderer() {
    if (textureId_ != 0) {
        glDeleteTextures(1, &textureId_);
    }
}

bool FontRenderer::loadFont(const std::string& path, float pixelHeight) {
    // Read font file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open font file: " << path << std::endl;
        return false;
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    fontBuffer_.resize(fileSize);
    if (!file.read(reinterpret_cast<char*>(fontBuffer_.data()), fileSize)) {
        std::cerr << "Failed to read font file: " << path << std::endl;
        return false;
    }
    file.close();

    pixelHeight_ = pixelHeight;
    createFontTexture(fontBuffer_.data(), pixelHeight);

    return textureId_ != 0;
}

void FontRenderer::createFontTexture(const unsigned char* fontData, float pixelHeight) {
    // Initialize font
    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, fontData, stbtt_GetFontOffsetForIndex(fontData, 0))) {
        std::cerr << "Failed to initialize font" << std::endl;
        return;
    }

    // Get font metrics
    float scale = stbtt_ScaleForPixelHeight(&font, pixelHeight);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

    ascent_ = ascent * scale;
    lineHeight_ = (ascent - descent + lineGap) * scale;

    // Calculate texture size needed
    // Use a simple row-based packing
    textureWidth_ = 512;
    textureHeight_ = 512;

    std::vector<unsigned char> bitmap(textureWidth_ * textureHeight_, 0);

    // Pack glyphs into texture
    int penX = 1;
    int penY = 1;
    int rowHeight = 0;

    for (int c = 32; c < 128; ++c) {
        int glyphIndex = c - 32;

        // Get glyph metrics
        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);

        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&font, c, scale, scale, &x0, &y0, &x1, &y1);

        int glyphWidth = x1 - x0;
        int glyphHeight = y1 - y0;

        // Check if we need to move to next row
        if (penX + glyphWidth + 1 >= textureWidth_) {
            penX = 1;
            penY += rowHeight + 1;
            rowHeight = 0;
        }

        // Check if we've run out of space
        if (penY + glyphHeight + 1 >= textureHeight_) {
            std::cerr << "Font texture too small!" << std::endl;
            break;
        }

        // Render glyph to bitmap
        if (glyphWidth > 0 && glyphHeight > 0) {
            stbtt_MakeCodepointBitmap(&font,
                bitmap.data() + penY * textureWidth_ + penX,
                glyphWidth, glyphHeight,
                textureWidth_,
                scale, scale, c);
        }

        // Store glyph info
        FontGlyph& g = glyphs_[glyphIndex];
        g.x0 = static_cast<float>(penX) / textureWidth_;
        g.y0 = static_cast<float>(penY) / textureHeight_;
        g.x1 = static_cast<float>(penX + glyphWidth) / textureWidth_;
        g.y1 = static_cast<float>(penY + glyphHeight) / textureHeight_;
        g.xoff = static_cast<float>(x0);
        g.yoff = static_cast<float>(y0);
        g.xadvance = advance * scale;
        g.width = glyphWidth;
        g.height = glyphHeight;

        penX += glyphWidth + 1;
        rowHeight = std::max(rowHeight, glyphHeight);
    }

    // Create OpenGL texture
    glGenTextures(1, &textureId_);
    glBindTexture(GL_TEXTURE_2D, textureId_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, textureWidth_, textureHeight_,
                 0, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

float FontRenderer::drawText(float x, float y, const std::string& text,
                              float r, float g, float b, float a) {
    if (textureId_ == 0) return 0;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureId_);
    glColor4f(r, g, b, a);

    float startX = x;
    y += ascent_;  // Adjust for baseline

    glBegin(GL_QUADS);
    for (char c : text) {
        if (c < 32 || c >= 128) continue;

        const FontGlyph& g = glyphs_[c - 32];

        float x0 = x + g.xoff;
        float y0 = y + g.yoff;
        float x1 = x0 + g.width;
        float y1 = y0 + g.height;

        glTexCoord2f(g.x0, g.y0); glVertex2f(x0, y0);
        glTexCoord2f(g.x1, g.y0); glVertex2f(x1, y0);
        glTexCoord2f(g.x1, g.y1); glVertex2f(x1, y1);
        glTexCoord2f(g.x0, g.y1); glVertex2f(x0, y1);

        x += g.xadvance;
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);

    return x - startX;
}

float FontRenderer::measureText(const std::string& text) {
    if (textureId_ == 0) return 0;

    float width = 0;
    for (char c : text) {
        if (c < 32 || c >= 128) continue;
        width += glyphs_[c - 32].xadvance;
    }
    return width;
}
