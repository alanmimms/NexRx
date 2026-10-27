#pragma once
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <zephyr/kernel.h>

// This supports MONO_VTILED bitmaps for monochrome LCD. It's very
// made-for-purpose and limited. Text drawing must be done such that y
// % 8 == 0 so the glyph blitter doesn't have to deal with crossing
// LCD RAM pages.
class FrameBuffer {
 public:
  static constexpr uint8_t WIDTH = 192;
  static constexpr uint8_t HEIGHT = 64;
  static constexpr unsigned FBSIZE = WIDTH * HEIGHT / 8;

 FrameBuffer(uint8_t *bitsP, uint8_t width=WIDTH, uint8_t height=HEIGHT)
   : bitsP(bitsP),
    width(width),
    height(height),
    sizeBytes(width*height/8)
    {}

  struct Point {
    uint8_t x;
    uint8_t y;
  };

  struct Glyph {
    uint16_t bitOffset;
    uint8_t inkW;
    uint8_t advance;
  };


  struct Font {

    Font(const uint8_t hPages, const std::vector<Glyph> glyphs, const uint8_t *bitsP)
        : hPages(hPages), 
          glyphs(std::move(glyphs)), 
      bitsP(bitsP) {}

    const uint8_t hPages;
    const std::vector<Glyph> glyphs;
    const uint8_t *bitsP;
  };

  // NOTE: this assumes y is a multiple of 8 for now.
  void drawString(const char *stringP, uint8_t x, uint8_t y, const Font &font) {

    for (const char *p=stringP; *p; ++p) {
      const uint8_t chX = ((*p < ' ' || *p > '~') ? '?' : *p) - ' ';
      const Glyph &glyph = font.glyphs[chX];
      unsigned glX = glyph.bitOffset;

      for (uint8_t inkPage=0; inkPage < font.hPages; ++inkPage) {
	const uint8_t page = (y >> 3) + inkPage;

	// Copy page strip for glyph to fb while clipping to right edge.
	for (uint8_t k=0; k < glyph.inkW; ++k, ++glX) {
	  if (x <= WIDTH) bitsP[x + k + page*WIDTH] |= font.bitsP[glX];
	}
      }

      x += glyph.advance;
    }
  }
  

  unsigned measureString(const char *stringP, const Font &font) {
    unsigned totalWidth = 0;

    for (; *stringP; ++stringP) {
      const uint8_t chX = ((*stringP < ' ' || *stringP > '~') ? '?' : *stringP) - ' ';
      const Glyph &glyph = font.glyphs[chX];
      totalWidth += glyph.advance;
    }

    return totalWidth;
  }


  void drawFilledRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    rectLoop(x, y, w, h,
	   [](uint8_t &pageByte, uint8_t mask) { pageByte |= mask; });
  }


  void invertRect(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    rectLoop(x, y, w, h,
	   [](uint8_t &pageByte, uint8_t mask) { pageByte ^= mask; });
  }


  void eraseRect(uint8_t x=0, uint8_t y=0, uint8_t w=WIDTH, uint8_t h=HEIGHT) {
    rectLoop(x, y, w, h,
	   [](uint8_t &pageByte, uint8_t mask) { pageByte &= ~mask; });
  }


  void drawLine(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    if (w == 0 || h == 0) return;

    if (h == 0) {			// Horizontal line
      const uint8_t mask = maskForY(y);
      const uint8_t page = y >> 3;

      for (uint8_t xOffset=0; xOffset < w; ++xOffset) {
	bitsP[page * width + xOffset] |= mask;
      }
    } else if (width == 0) {	// Vertical line
      const uint8_t mask = maskForY(y);

      for (uint8_t yOffset=0; yOffset < h; ++yOffset) {
	const uint8_t page = (y + yOffset) >> 3;
	bitsP[page * width + x] |= mask;
      }
    } else {			// Bresenham line
      int m = 2 * h;
      int slopeErr = m - w;

      for (uint8_t xOffset=0, thisY=y; xOffset < w; ++xOffset) {
	const uint8_t page = thisY >> 3;
	const uint8_t mask = maskForY(thisY);
	bitsP[page * width + x + xOffset] |= mask;

	slopeErr += m;

	if (slopeErr >= 0) {
	  ++thisY;
	  slopeErr -= 2 * w;
	}
      }
    }
  }


  uint8_t *bitsP;
  uint8_t width;
  uint8_t height;
  uint8_t sizeBytes;

 protected:

  // Returns a bit mask the pixel in the vertical strip of pixels in a
  // framebuffer byte for y, considered modulo 8.
  uint8_t maskForY(uint8_t y) {
    return 1 << (y & 7);
  }


  // Returns a bit mask for the vertical strip of pixels in a single
  // framebuffer byte for the range [startY, lastY] where each of
  // these ordinates is considered modulo 8.
  uint8_t maskForRange(uint8_t startY, uint8_t lastY) {
    const uint8_t startMask = 0xFF << (startY & 7);
    const uint8_t startEndBit = ((startY + 8) & -8) - 1;
    const uint8_t lastMask = 0xFF >> (7 ^ (std::min(startEndBit, lastY) & 7));
    return startMask & lastMask;
  }


  template<typename BltOp>
  void rectLoop(uint8_t x, uint8_t y, uint8_t w, uint8_t h, BltOp bltOp) {
    const auto [startX, startY, lastX, lastY] = makeCanonical(x, y, w, h);

    for (uint8_t thisY=startY; thisY <= lastY; thisY = (thisY+8) & -8) {
      const uint8_t page = thisY >> 3;
      const uint8_t mask = maskForRange(thisY, lastY);
      if (mask == 0) return;

      for (uint8_t thisX=startX; thisX <= lastX; ++thisX) {
	bltOp(bitsP[thisX + page*WIDTH], mask);
      }
    }
  }


  struct Canonicalized {
    uint8_t startX;
    uint8_t startY;
    uint8_t lastX;
    uint8_t lastY;
  };

  Canonicalized makeCanonical(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    return Canonicalized(x, y, std::min(x + w, width - 1), std::min(y + h, height - 1));
  }
};
