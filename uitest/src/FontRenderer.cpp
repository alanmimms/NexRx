/**
 * @file FontRenderer.cpp
 * @brief Font rendering implementation using stb_truetype and Raylib/rlgl
 */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include "FontRenderer.hpp"
#include "rlgl.h"

#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

FontRenderer::FontRenderer() {
  glyphs.resize(96);  // ASCII 32-127
}

FontRenderer::~FontRenderer() {
  unload();
}

void FontRenderer::unload() {
  if (textureID != 0) {
    rlUnloadTexture(textureID);
    textureID = 0;
  }
}

bool FontRenderer::loadFont(const std::string& path, float pixelHeightIn) {
  std::cout << "[FontRenderer] Loading font: " << path << std::endl;
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) {
    std::cerr << "[FontRenderer] Failed to open font file: " << path << " (errno=" << errno << ")" << std::endl;
    return false;
  }

  fseek(f, 0, SEEK_END);
  size_t size = ftell(f);
  fseek(f, 0, SEEK_SET);

  fontBuffer.resize(size);
  size_t read = fread(fontBuffer.data(), 1, size, f);
  fclose(f);

  if (read != size) {
    std::cerr << "[FontRenderer] Failed to read entire font file: " << path << std::endl;
    return false;
  }

  std::cout << "[FontRenderer] Read " << size << " bytes, initializing font..." << std::endl;
  pixelHeight = pixelHeightIn;
  createFontTexture(fontBuffer.data(), pixelHeight);

  if (textureID == 0) {
      std::cerr << "[FontRenderer] Failed to create font texture" << std::endl;
  } else {
      std::cout << "[FontRenderer] Font loaded successfully, textureID=" << textureID << std::endl;
  }

  return textureID != 0;
}

void FontRenderer::createFontTexture(const unsigned char* fontData, float pixelHeightIn) {
  // Initialize font
  stbtt_fontinfo font;
  if (!stbtt_InitFont(&font, fontData, stbtt_GetFontOffsetForIndex(fontData, 0))) {
    std::cerr << "Failed to initialize font" << std::endl;
    return;
  }

  // Get font metrics
  float scale = stbtt_ScaleForPixelHeight(&font, pixelHeightIn);
  int ascentVal, descent, lineGap;
  stbtt_GetFontVMetrics(&font, &ascentVal, &descent, &lineGap);

  ascent = ascentVal * scale;
  lineHeight = (ascentVal - descent + lineGap) * scale;

  textureWidth = 512;
  textureHeight = 512;

  // Use RGBA for maximum compatibility with default shaders
  std::vector<unsigned char> bitmap(textureWidth * textureHeight * 4, 0);

  int penX = 1;
  int penY = 1;
  int rowHeight = 0;

  for (int c = 32; c < 128; ++c) {
    int glyphIndex = c - 32;

    int advance, lsb;
    stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);

    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(&font, c, scale, scale, &x0, &y0, &x1, &y1);

    int glyphWidth = x1 - x0;
    int glyphHeight = y1 - y0;

    if (penX + glyphWidth + 1 >= textureWidth) {
      penX = 1;
      penY += rowHeight + 1;
      rowHeight = 0;
    }

    if (penY + glyphHeight + 1 >= textureHeight) {
      std::cerr << "Font texture too small!" << std::endl;
      break;
    }

    if (glyphWidth > 0 && glyphHeight > 0) {
      std::vector<unsigned char> temp(glyphWidth * glyphHeight);
      stbtt_MakeCodepointBitmap(&font, temp.data(), glyphWidth, glyphHeight, glyphWidth, scale, scale, c);

      for (int y = 0; y < glyphHeight; ++y) {
        for (int x = 0; x < glyphWidth; ++x) {
          int dstIdx = ((penY + y) * textureWidth + (penX + x)) * 4;
          unsigned char val = temp[y * glyphWidth + x];
          bitmap[dstIdx] = 255;     // R
          bitmap[dstIdx + 1] = 255; // G
          bitmap[dstIdx + 2] = 255; // B
          bitmap[dstIdx + 3] = val; // A
        }
      }
    }

    FontGlyph& g = glyphs[glyphIndex];
    g.x0 = static_cast<float>(penX) / textureWidth;
    g.y0 = static_cast<float>(penY) / textureHeight;
    g.x1 = static_cast<float>(penX + glyphWidth) / textureWidth;
    g.y1 = static_cast<float>(penY + glyphHeight) / textureHeight;
    g.xoff = static_cast<float>(x0);
    g.yoff = static_cast<float>(y0);
    g.xadvance = advance * scale;
    g.width = glyphWidth;
    g.height = glyphHeight;

    penX += glyphWidth + 1;
    rowHeight = std::max(rowHeight, glyphHeight);
  }

  // Create Raylib/rlgl texture
  textureID = rlLoadTexture(bitmap.data(), textureWidth, textureHeight, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
  rlTextureParameters(textureID, RL_TEXTURE_MAG_FILTER, RL_TEXTURE_FILTER_LINEAR);
  rlTextureParameters(textureID, RL_TEXTURE_MIN_FILTER, RL_TEXTURE_FILTER_LINEAR);
  rlTextureParameters(textureID, RL_TEXTURE_WRAP_S, RL_TEXTURE_WRAP_CLAMP);
  rlTextureParameters(textureID, RL_TEXTURE_WRAP_T, RL_TEXTURE_WRAP_CLAMP);
}

float FontRenderer::drawText(float x, float y, const std::string& text,
                             float r, float g, float b, float a) {
  if (textureID == 0) return 0;

  // rlSetTexture is usually enough, but we call it to ensure batch flush if needed
  rlSetTexture(textureID);
  rlBegin(RL_QUADS);
  rlColor4f(r, g, b, a);

  float startX = x;
  float baselineY = y + ascent;

  for (char c : text) {
    if (c < 32 || c >= 128) continue;

    const FontGlyph& g = glyphs[c - 32];

    float x0 = x + g.xoff;
    float y0 = baselineY + g.yoff;
    float x1 = x0 + g.width;
    float y1 = y0 + g.height;

    rlTexCoord2f(g.x0, g.y0); rlVertex2f(x0, y0);
    rlTexCoord2f(g.x0, g.y1); rlVertex2f(x0, y1);
    rlTexCoord2f(g.x1, g.y1); rlVertex2f(x1, y1);
    rlTexCoord2f(g.x1, g.y0); rlVertex2f(x1, y0);

    x += g.xadvance;
  }
  rlEnd();
  rlSetTexture(0);

  return x - startX;
}

float FontRenderer::measureText(const std::string& text) {
  if (textureID == 0) return 0;

  float width = 0;
  for (char c : text) {
    if (c < 32 || c >= 128) continue;
    width += glyphs[c - 32].xadvance;
  }
  return width;
}
