#pragma once
#include <stdint.h>
#include <vector>
#include <algorithm>

// This supports MONO_VTILED bitmaps for monochrome LCD. It's very
// made-for-purpose and limited. Text drawing must be done such that y
// % 8 == 0 so the glyph blitter doesn't have to deal with crossing
// LCD RAM pages.
class FrameBuffer {
 public:
  FrameBuffer(void *bits, unsigned w=192, unsigned h=64) {}

  struct Point {
    unsigned x;
    unsigned y;
  };

  struct Glyph {
    uint16_t bitOffset;
    uint8_t inkW;
    uint8_t advance;
  };


  struct Font {

    Font(unsigned hPages, std::vector<Glyph> glyphs, std::vector<uint8_t> bits)
        : hPages(hPages), 
          glyphs(std::move(glyphs)), 
          bits(std::move(bits)) {}

    uint8_t hPages;
    std::vector<Glyph> glyphs;
    std::vector<uint8_t> bits;
  };


  void drawString(const char *stringP, unsigned x, unsigned y, Font &font) {

    for (; *stringP; ++stringP) {
      const uint8_t chX = ((*stringP < ' ' || *stringP > '~') ? '?' : *stringP) - ' ';
      const Glyph &glyph = font.glyphs[chX];

      for (unsigned page=0; page < font.hPages; ++page) {
	const unsigned glyphByteX = glyph.bitOffset + page * glyph.inkW;
	bits[(y + page)*width + x] |= font.bits[glyphByteX];
      }

      x += glyph.advance;
    }
  }
  

  unsigned measureString(const char *stringP, Font &font) {
    unsigned totalWidth = 0;

    for (; *stringP; ++stringP) {
      const uint8_t chX = ((*stringP < ' ' || *stringP > '~') ? '?' : *stringP) - ' ';
      const Glyph &glyph = font.glyphs[chX];
      totalWidth += glyph.advance;
    }

    return totalWidth;
  }


  void drawFilledRect(unsigned x, unsigned y, unsigned w, unsigned h) {
    doRect(x, y, w, h,
	   [](uint8_t &pageByte, uint8_t mask) { pageByte |= mask; });
  }


  void eraseRect(unsigned x, unsigned y, unsigned w, unsigned h) {
    doRect(x, y, w, h,
	   [](uint8_t &pageByte, uint8_t mask) { pageByte &= ~mask; });
  }


  void drawLine(unsigned x, unsigned y, unsigned w, unsigned h) {
    if (w == 0 || h == 0) return;

    if (h == 0) {			// Horizontal line
      const uint8_t mask = maskForY(y);
      const unsigned page = y >> 3;

      for (unsigned xOffset=0; xOffset < w; ++xOffset) {
	bits[page * width + xOffset] |= mask;
      }
    } else if (width == 0) {	// Vertical line
      const uint8_t mask = maskForY(y);

      for (unsigned yOffset=0; yOffset < h; ++yOffset) {
	const unsigned page = (y + yOffset) >> 3;
	bits[page * width + x] |= mask;
      }
    } else {			// Bresenham line
      int m = 2 * h;
      int slopeErr = m - w;

      for (int xOffset=0, thisY=y; xOffset < w; ++xOffset) {
	const unsigned page = thisY >> 3;
	const uint8_t mask = maskForY(thisY);
	bits[page * width + x + xOffset] |= mask;

	slopeErr += m;

	if (slopeErr >= 0) {
	  ++thisY;
	  slopeErr -= 2 * w;
	}
      }
    }
  }


  uint8_t *bits;
  unsigned width;
  unsigned height;

 protected:

  // Returns a bit mask the pixel in the vertical strip of pixels in a
  // framebuffer byte for y, considered modulo 8.
  uint8_t maskForY(unsigned y) {
    return 1 << (y & 7);
  }

  // Returns a bit mask for the vertical strip of pixels in a single
  // framebuffer byte for the range [startY, lastY] where each of
  // these ordinates is considered modulo 8.
  uint8_t maskForRange(unsigned startY, unsigned lastY) {
    const uint8_t startMask = 0xFF << (startY & 7);
    const uint8_t lastMask = 0xFF >> (7 ^ (lastY & 7));
    return startMask & lastMask;
  }

  template<typename BltOp>
  void doRect(unsigned x, unsigned y, unsigned w, unsigned h, BltOp bltOp) {
    const auto [startX, startY, lastX, lastY] = makeCanonical(x, y, w, h);

    if (w == 0 || h == 0) return;

    for (unsigned x=startX; x <= lastX; ++x) {

      for (unsigned y=startY; ; ++y) {
	const uint8_t mask = maskForRange(y, lastY);
	if (mask == 0) return;
	bltOp(bits[x], mask);
      }
    }
  }

  struct Canonicalized {
    uint8_t startX;
    uint8_t startY;
    uint8_t lastX;
    uint8_t lastY;
  };

  Canonicalized makeCanonical(unsigned x, unsigned y, unsigned w, unsigned h) {
    return Canonicalized(x, y, std::min(x + w, width - 1), std::min(y + h, height - 1));
  }
};
