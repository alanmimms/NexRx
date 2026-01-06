/**
 * @file FontRenderer.hpp
 * @brief Simple font rendering using stb_truetype and OpenGL
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

struct FontGlyph {
    float x0, y0, x1, y1;  // Texture coordinates
    float xoff, yoff;       // Offset from cursor
    float xadvance;         // How much to advance cursor
    int width, height;      // Glyph dimensions
};

class FontRenderer {
public:
    FontRenderer();
    ~FontRenderer();

    // Load a TTF font file at specified pixel height
    bool loadFont(const std::string& path, float pixelHeight);

    // Render text at position, returns width of rendered text
    float drawText(float x, float y, const std::string& text,
                   float r, float g, float b, float a);

    // Measure text without drawing
    float measureText(const std::string& text);

    // Get line height
    float lineHeight() const { return lineHeight_; }

    // Check if font is loaded
    bool isLoaded() const { return textureId_ != 0; }

private:
    void createFontTexture(const unsigned char* fontData, float pixelHeight);

    unsigned int textureId_ = 0;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    float pixelHeight_ = 0;
    float lineHeight_ = 0;
    float ascent_ = 0;

    // Glyph data for ASCII 32-126
    std::vector<FontGlyph> glyphs_;

    // Packed character data from stb_truetype
    std::vector<unsigned char> fontBuffer_;
};
