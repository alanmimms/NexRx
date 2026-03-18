#pragma once

#include <sol/sol.hpp>
#include <raylib.h>

class RenderBridge {
public:
  static void registerWithLua(sol::state& lua);
  static void setFont(Font font);

  static void drawRect(float x, float y, float w, float h, sol::table color);
  static void drawRectLines(float x, float y, float w, float h, float thickness, sol::table color);
  static void drawLine(float x1, float y1, float x2, float y2, float thickness, sol::table color);
  static void drawText(const char* text, float x, float y, int fontSize, sol::table color);
  static float measureText(const char* text, int fontSize);

private:
  static Font currentFont;
};
