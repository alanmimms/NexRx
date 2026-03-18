#include "LuaBridge.hpp"
#include <raylib.h>

Font RenderBridge::currentFont = { 0 };

static Color tableToColor(sol::table color) {
  float r = color[1];
  float g = color[2];
  float b = color[3];
  float a = color[4];
  return Color{
    static_cast<unsigned char>(r * 255),
    static_cast<unsigned char>(g * 255),
    static_cast<unsigned char>(b * 255),
    static_cast<unsigned char>(a * 255)
  };
}

void RenderBridge::registerWithLua(sol::state& lua) {
  sol::table bridge = lua.create_table();
  bridge["drawRect"] = &RenderBridge::drawRect;
  bridge["drawRectLines"] = &RenderBridge::drawRectLines;
  bridge["drawLine"] = &RenderBridge::drawLine;
  bridge["drawText"] = &RenderBridge::drawText;
  bridge["measureText"] = &RenderBridge::measureText;
  lua["bridge"] = bridge;
}

void RenderBridge::setFont(Font font) {
  currentFont = font;
}

void RenderBridge::drawRect(float x, float y, float w, float h, sol::table color) {
  DrawRectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h), tableToColor(color));
}

void RenderBridge::drawRectLines(float x, float y, float w, float h, float thickness, sol::table color) {
  DrawRectangleLinesEx(Rectangle{x, y, w, h}, thickness, tableToColor(color));
}

void RenderBridge::drawLine(float x1, float y1, float x2, float y2, float thickness, sol::table color) {
  DrawLineEx(Vector2{x1, y1}, Vector2{x2, y2}, thickness, tableToColor(color));
}

void RenderBridge::drawText(const char* text, float x, float y, int fontSize, sol::table color) {
  if (currentFont.texture.id == 0) {
    DrawText(text, static_cast<int>(x), static_cast<int>(y), fontSize, tableToColor(color));
  } else {
    DrawTextEx(currentFont, text, Vector2{x, y}, static_cast<float>(fontSize), 1.0f, tableToColor(color));
  }
}

float RenderBridge::measureText(const char* text, int fontSize) {
  if (currentFont.texture.id == 0) {
    return static_cast<float>(MeasureText(text, fontSize));
  } else {
    return MeasureTextEx(currentFont, text, static_cast<float>(fontSize), 1.0f).x;
  }
}
