/**
 * @file FontRenderer.hpp
 * @brief Font rendering using stb_truetype and Raylib/rlgl
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "raylib.h"

struct FontGlyph {
  float x0, y0, x1, y1; // Texture coordinates
  float xoff, yoff;     // Offset from pen position
  float xadvance;       // Distance to next glyph
  int width, height;    // Size in pixels
};

class FontRenderer {
public:
  FontRenderer();
  ~FontRenderer();

  bool loadFont(const std::string& path, float pixelHeight);
  void unload();
  float drawText(float x, float y, const std::string& text, 
                 float r, float g, float b, float a);
  float measureText(const std::string& text);
  float getLineHeight() const { return lineHeight; }
  bool isLoaded() const { return textureID != 0; }

private:
  void createFontTexture(const unsigned char* fontData, float pixelHeight);

  std::vector<unsigned char> fontBuffer;
  std::vector<FontGlyph> glyphs;
  uint32_t textureID = 0;
  int textureWidth = 0, textureHeight = 0;
  float ascent = 0;
  float lineHeight = 0;
  float pixelHeight = 0;
};
