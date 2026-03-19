#include "AppLuaBridge.hpp"
#include "GUIEngine.hpp"
#include <raylib.h>
#include <rlgl.h>
#include <vector>
#include <string>
#include <iostream>
#include <cbor.h>

Font AppLuaBridge::currentFont = { 0 };

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

void AppLuaBridge::registerWithLua(sol::state& lua, GUIEngine* engine) {
  // 1. Drawing Primitives (System table)
  sol::table system = lua.create_table();
  system["drawRect"] = &AppLuaBridge::drawRect;
  system["drawRectLines"] = &AppLuaBridge::drawRectLines;
  system["drawLine"] = &AppLuaBridge::drawLine;
  system["drawText"] = &AppLuaBridge::drawText;
  system["measureText"] = &AppLuaBridge::measureText;
  system["traceLog"] = &AppLuaBridge::traceLog;
  
  // Window info
  system["getWindowSize"] = [engine]() { return std::make_tuple(GetScreenWidth(), GetScreenHeight()); };
  
  lua["System"] = system;

  lua.set("drawRect", [](float x, float y, float w, float h, float r, float g, float b, float a) {
    DrawRectangleRec({x, y, w, h}, Color{ (unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255) });
  });
  lua.set("drawCircle", [](float x, float y, float radius, float r, float g, float b, float a) {
    DrawCircle((int)x, (int)y, radius, Color{ (unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255) });
  });
  lua.set("drawCircleOutline", [](float x, float y, float radius, float r, float g, float b, float a, float t) {
    DrawCircleLines((int)x, (int)y, radius, Color{ (unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255) });
  });
  lua.set("drawRectOutline", [](float x, float y, float w, float h, float r, float g, float b, float a, float t) {
    DrawRectangleLinesEx({x, y, w, h}, t, Color{ (unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255) });
  });
  lua.set("drawRoundedRect", [](float x, float y, float w, float h, float radius, float r, float g, float b, float a) {
    DrawRectangleRounded({x, y, w, h}, radius / (h/2.0f), 8, Color{ (unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255) });
  });
  lua.set("drawText", [](float x, float y, const char* text, float r, float g, float b, float a, sol::object fontSize) {
    int fs = 20;
    if (fontSize.is<int>()) fs = fontSize.as<int>();
    else if (fontSize.is<float>()) fs = (int)fontSize.as<float>();
    
    if (currentFont.texture.id == 0) DrawText(text, (int)x, (int)y, fs, Color{ (unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255) });
    else DrawTextEx(currentFont, text, {x, y}, (float)fs, 1.0f, Color{ (unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255) });
  });
  lua.set("measureText", [](const char* text, sol::object fontSize) {
    int fs = 20;
    if (fontSize.is<int>()) fs = fontSize.as<int>();
    else if (fontSize.is<float>()) fs = (int)fontSize.as<float>();

    if (currentFont.texture.id == 0) return (float)MeasureText(text, fs);
    return MeasureTextEx(currentFont, text, (float)fs, 1.0f).x;
  });
  lua.set("getLineHeight", []() { return 20.0f; });
  lua.set("getWindowSize", []() { return std::make_tuple(GetScreenWidth(), GetScreenHeight()); });
  lua.set("getMousePos", []() { Vector2 m = GetMousePosition(); return std::make_tuple(m.x, m.y); });
  lua.set("isMouseDown", [](int b) { return IsMouseButtonDown(b); });
  lua.set("isKeyDown", [](int k) { return IsKeyDown(k); });
  lua.set("isMouseClicked", [](int b) { return IsMouseButtonPressed(b); });
  lua.set("isMouseReleased", [](int b) { return IsMouseButtonReleased(b); });
  lua.set("getMouseWheel", []() { return GetMouseWheelMove(); });
  lua.set("setClearColor", [](float r, float g, float b) { ClearBackground(Color{ (unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), 255 }); });

  // 2. Hardware Bindings (hw table)
  sol::table hwTable = lua.create_table();
  hwTable["connect"] = [engine](std::string h, int cp, int sp) {
    return engine->connectTwin(h, cp, sp);
  };
  hwTable["disconnect"] = [engine]() { engine->disconnectTwin(); };
  hwTable["isConnected"] = [engine]() { return engine->isTwinConnected(); };
  hwTable["getSpectrum"] = [engine](sol::this_state s) { 
    engine->getDSP().computeSpectrum();
    sol::state_view lView(s); sol::table res = lView.create_table(); 
    std::vector<float> data = engine->getDSP().getSpectrumData();
    for (size_t i = 0; i < data.size(); ++i) res[i + 1] = data[i]; 
    return res; 
  };
  hwTable["getState"] = [engine](sol::this_state s) -> sol::object {
    return engine->getTwinState(s);
  };
  
  // Setters
  hwTable["setVFO"] = [engine](double f, double k) { engine->postTwinCommand([engine, f, k]() { engine->getTwinConn().setVFO(f, k); }); };
  hwTable["setAttenuation"] = [engine](int db) { engine->postTwinCommand([engine, db]() { engine->getTwinConn().setAtten(db); }); };
  hwTable["setAGCMode"] = [engine](int m) { engine->postTwinCommand([engine, m]() { engine->getTwinConn().setAGCMode(m); }); };
  hwTable["setIsgFreq"] = [engine](double f) { engine->postTwinCommand([engine, f]() { engine->getTwinConn().setISGFreq(f); }); };
  hwTable["setIsgEnable"] = [engine](bool en) { engine->postTwinCommand([engine, en]() { engine->getTwinConn().setISGEnable(en); }); };
  hwTable["setPreselectorInd"] = [engine](uint32_t mask) { engine->postTwinCommand([engine, mask]() { engine->getTwinConn().setPreselectorL(mask); }); };
  hwTable["setPreselectorCap"] = [engine](uint32_t m) { engine->postTwinCommand([engine, m]() { engine->getTwinConn().setPreselectorCap(m); }); };
  hwTable["setPreselectorAuto"] = [engine](bool en) { engine->postTwinCommand([engine, en]() { engine->getTwinConn().setPreselectorAuto(en); }); };
  hwTable["setPreselectorEnabled"] = [engine](bool en) { engine->postTwinCommand([engine, en]() { engine->getTwinConn().setPreselectorEnabled(en); }); };
  
  hwTable["setRFGain"] = [engine](double db) {
    engine->getDSP().setRfGain((float)db);
    engine->postTwinCommand([engine, db]() { engine->getTwinConn().setPGAGain((int)(db / 6.0)); }); 
  };
  hwTable["setQSDOffset"] = [engine](double k) { 
    engine->getDSP().setQsdOffset(k); 
    engine->postTwinCommand([engine, k]() { engine->getTwinConn().setVFO(engine->getLastVFOHz(), k * 1000.0); }); 
  };
  
  lua["hw"] = hwTable;

  // 3. RX Control (rx table)
  sol::table rxTable = lua.create_table();
  rxTable["setModeId"] = [engine](int id) { engine->getDSP().setModeId(id); };
  rxTable["setBfoOffset"] = [engine](float hz) { engine->getDSP().getDemod().setBfoOffset(hz); };
  rxTable["setBandpassEnabled"] = [engine](bool en) { engine->getDSP().getFilter().setBandpassEnabled(en); };
  rxTable["setBandpassCenter"] = [engine](float hz) { engine->getDSP().getFilter().setBandpassCenter(hz); };
  rxTable["setBandpassWidth"] = [engine](float hz) { engine->getDSP().getFilter().setBandpassWidth(hz); };
  rxTable["setMute"] = [engine](bool en) { engine->getAudio().setMuted(en); };
  rxTable["setVFO"] = [engine](double f) {
    engine->setLastVFOHz(f);
    engine->getDSP().setVfo(f);
    engine->postTwinCommand([engine, f]() { engine->getTwinConn().setVFO(f, engine->getDSP().getQsdOffset() * 1000.0); });
  };
  rxTable["getStats"] = [engine](sol::this_state s) {
    auto& d = engine->getDSP().getDiagnostics();
    sol::state_view lView(s); sol::table t = lView.create_table();
    t["rms"] = d.signalRms.load();
    t["maxAudio"] = d.maxAudio.load();
    return t;
  };
  lua["rx"] = rxTable;

  // 4. Waterfall / Specialized (waterfall table)
  sol::table waterfallTable = lua.create_table();
  waterfallTable["init"] = [engine](int b, int r) { return engine->getWaterfall().init(b, r); };
  waterfallTable["setRange"] = [engine](float min, float max) { engine->getWaterfall().setRange(min, max); };
  waterfallTable["addRow"] = [engine](sol::object obj) {
    if (!obj.is<sol::table>()) return;
    sol::table t = obj.as<sol::table>();
    std::vector<float> data; data.reserve(t.size());
    for (size_t i = 1; i <= t.size(); ++i) { sol::object item = t[i]; data.push_back(item.is<float>() ? item.as<float>() : -100.0f); }
    engine->getWaterfall().addRow(data.data(), (int)data.size());
  };
  waterfallTable["renderSpectrum"] = [engine](sol::object obj, float x, float y, float w, float h) {
    if (!obj.is<sol::table>()) return;
    sol::table t = obj.as<sol::table>();
    std::vector<float> data; data.reserve(t.size());
    for (size_t i = 1; i <= t.size(); ++i) { sol::object item = t[i]; data.push_back(item.is<float>() ? item.as<float>() : -100.0f); }
    
    // We must ensure Raylib's internal state is flushed before raw GL calls
    rlDrawRenderBatchActive(); 
    engine->getWaterfall().renderSpectrum(data.data(), (int)data.size(), x, y, w, h);
  };
  waterfallTable["render"] = [engine](float x, float y, float w, float h, float zoom, float center) { 
    rlDrawRenderBatchActive();
    engine->getWaterfall().render(x, y, w, h, zoom, center); 
  };
  waterfallTable["setColormapData"] = [engine](sol::object obj) {
    if (!obj.is<sol::table>()) return;
    sol::table t = obj.as<sol::table>();
    std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>> grad;
    for (size_t i = 1; i <= t.size(); ++i) {
      sol::table s = t[i]; 
      grad.push_back({s[1].get_or(0.0f), (uint8_t)s[2].get_or(0), (uint8_t)s[3].get_or(0), (uint8_t)s[4].get_or(0)});
    }
    engine->getWaterfall().setColormapData(grad);
  };
  lua["waterfall"] = waterfallTable;

  // 5. Audio Control (audio table)
  sol::table audioTable = lua.create_table();
  audioTable["isInitialized"] = [engine]() { return true; }; // Engine init is checked in C++
  audioTable["start"] = [engine]() { return engine->getAudio().start(); };
  audioTable["stop"] = [engine]() { engine->getAudio().stop(); };
  audioTable["setVolume"] = [engine](float db) {
    float linear = (db <= -60.0f) ? 0.0f : std::pow(10.0f, db / 20.0f);
    engine->getAudio().setVolume(linear);
  };
  lua["audio"] = audioTable;

  // 6. Common functions
  lua["LOG_ALL"] = 0; lua["LOG_TRACE"] = 1; lua["LOG_DEBUG"] = 2; lua["LOG_INFO"] = 3;
  lua["LOG_WARNING"] = 4; lua["LOG_ERROR"] = 5; lua["LOG_FATAL"] = 6; lua["LOG_NONE"] = 7;

  lua.set_function("print", [](sol::variadic_args args, sol::this_state L) {
    sol::state_view sv(L);
    std::string s = "";
    for (auto arg : args) {
      if (!s.empty()) s += " ";
      s += sv["tostring"](arg).template get<std::string>();
    }
    std::cout << "[LUA] " << s << std::endl;
  });
}

void AppLuaBridge::setFont(Font font) { currentFont = font; }

void AppLuaBridge::drawRect(float x, float y, float w, float h, sol::table color) { 
  DrawRectangleRec({x, y, w, h}, tableToColor(color)); 
}

void AppLuaBridge::drawRectLines(float x, float y, float w, float h, float thickness, sol::table color) { 
  DrawRectangleLinesEx({x, y, w, h}, thickness, tableToColor(color)); 
}

void AppLuaBridge::drawLine(float x1, float y1, float x2, float y2, float thickness, sol::table color) { 
  DrawLineEx({x1, y1}, {x2, y2}, thickness, tableToColor(color)); 
}

void AppLuaBridge::drawText(const char* text, float x, float y, int fontSize, sol::table color) {
  if (currentFont.texture.id == 0) DrawText(text, (int)x, (int)y, fontSize, tableToColor(color));
  else DrawTextEx(currentFont, text, {x, y}, (float)fontSize, 1.0f, tableToColor(color));
}

float AppLuaBridge::measureText(const char* text, int fontSize) {
  if (currentFont.texture.id == 0) return (float)MeasureText(text, fontSize);
  return MeasureTextEx(currentFont, text, (float)fontSize, 1.0f).x;
}

void AppLuaBridge::traceLog(int logLevel, sol::variadic_args args, sol::this_state L) {
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
    TraceLog(logLevel, "%s", formatted.c_str());
  } catch (...) { TraceLog(LOG_ERROR, "bridge.traceLog format error"); }
}
