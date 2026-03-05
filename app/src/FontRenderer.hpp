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
  float getLineHeight() const { return lineHeight; }

  // Check if font is loaded
  bool isLoaded() const { return textureID != 0; }

private:
  void createFontTexture(const unsigned char* fontData, float pixelHeight);

  unsigned int textureID = 0;
  int textureWidth = 0;
  int textureHeight = 0;
  float pixelHeight = 0;
  float lineHeight = 0;
  float ascent = 0;

  // Glyph data for ASCII 32-126
  std::vector<FontGlyph> glyphs;

  // Packed character data from stb_truetype
  std::vector<unsigned char> fontBuffer;
};
