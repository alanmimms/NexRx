#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <raylib.h>
#include <string>
#include <functional>

class GUIEngine; // Forward declaration

class AppLuaBridge {
public:
  static void registerWithLua(sol::state& lua, GUIEngine* engine);
  static void setFont(Font font);

  // Drawing primitives (Raylib-based)
  static void drawRect(float x, float y, float w, float h, sol::table color);
  static void drawRectLines(float x, float y, float w, float h, float thickness, sol::table color);
  static void drawRoundedRect(float x, float y, float w, float h, float radius, sol::table color);
  static void drawCircle(float x, float y, float radius, sol::table color);
  static void drawCircleOutline(float x, float y, float radius, float thickness, sol::table color);
  static void drawLine(float x1, float y1, float x2, float y2, float thickness, sol::table color);
  static void drawText(const char* text, float x, float y, int fontSize, sol::table color);
  static float measureText(const char* text, int fontSize);
  static void traceLog(int logLevel, sol::variadic_args args, sol::this_state L);

  // New function to get engine version
  static float getEngineVersion();

private:
  static Font currentFont;
};
