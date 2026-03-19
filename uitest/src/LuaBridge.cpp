#include "LuaBridge.hpp"
#include <raylib.h>
#include <vector>
#include <string>

Font LuaBridge::currentFont = { 0 };

static Color tableToColor(sol::table color) {
  if (color.size() < 3) return BLACK;
  float r = color[1];
  float g = color[2];
  float b = color[3];
  float a = (color.size() >= 4) ? (float)color[4] : 1.0f;
  return Color{
    static_cast<unsigned char>(r * 255),
    static_cast<unsigned char>(g * 255),
    static_cast<unsigned char>(b * 255),
    static_cast<unsigned char>(a * 255)
  };
}

void LuaBridge::registerWithLua(sol::state& lua) {
  sol::table bridge = lua.create_table();
  bridge["drawRect"] = &LuaBridge::drawRect;
  bridge["drawRectLines"] = &LuaBridge::drawRectLines;
  bridge["drawLine"] = &LuaBridge::drawLine;
  bridge["drawText"] = &LuaBridge::drawText;
  bridge["measureText"] = &LuaBridge::measureText;
  bridge["traceLog"] = &LuaBridge::traceLog;
  lua["bridge"] = bridge;

  lua["LOG_ALL"] = 0; lua["LOG_TRACE"] = 1; lua["LOG_DEBUG"] = 2; lua["LOG_INFO"] = 3;
  lua["LOG_WARNING"] = 4; lua["LOG_ERROR"] = 5; lua["LOG_FATAL"] = 6; lua["LOG_NONE"] = 7;

  lua.set_function("print", [](sol::variadic_args args, sol::this_state L) {
    sol::state_view sv(L);
    std::string s = "";
    for (auto arg : args) {
      if (!s.empty()) s += " ";
      s += sv["tostring"](arg).template get<std::string>();
    }
    TraceLog(LOG_INFO, "%s", s.c_str());
  });
}

void LuaBridge::setFont(Font font) { currentFont = font; }
void LuaBridge::drawRect(float x, float y, float w, float h, sol::table color) { DrawRectangle((int)x, (int)y, (int)w, (int)h, tableToColor(color)); }
void LuaBridge::drawRectLines(float x, float y, float w, float h, float thickness, sol::table color) { DrawRectangleLinesEx({x, y, w, h}, thickness, tableToColor(color)); }
void LuaBridge::drawLine(float x1, float y1, float x2, float y2, float thickness, sol::table color) { DrawLineEx({x1, y1}, {x2, y2}, thickness, tableToColor(color)); }
void LuaBridge::drawText(const char* text, float x, float y, int fontSize, sol::table color) {
  if (currentFont.texture.id == 0) DrawText(text, (int)x, (int)y, fontSize, tableToColor(color));
  else DrawTextEx(currentFont, text, {x, y}, (float)fontSize, 1.0f, tableToColor(color));
}
float LuaBridge::measureText(const char* text, int fontSize) {
  if (currentFont.texture.id == 0) return (float)MeasureText(text, fontSize);
  return MeasureTextEx(currentFont, text, (float)fontSize, 1.0f).x;
}

void LuaBridge::traceLog(int logLevel, sol::variadic_args args, sol::this_state L) {
  if (args.size() < 2) return;
  sol::state_view sv(L);
  sol::function format = sv["string"]["format"];
  std::string formatted;
  try {
    if (args.size() == 2) formatted = sv["tostring"](args[1]).template get<std::string>();
    else {
      std::vector<sol::object> formatArgs;
      for (size_t i = 1; i < args.size(); ++i) formatArgs.push_back(args[i]);
      formatted = format(sol::as_args(formatArgs));
    }
    TraceLog(logLevel, "DO NOT PUT THIS BACK FOR NOW! %s", formatted.c_str());
  } catch (...) { TraceLog(LOG_ERROR, "bridge.traceLog format error"); }
}
