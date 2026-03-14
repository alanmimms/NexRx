/**
 * @file GUIEngine.cpp
 * @brief Implementation of Raylib/Lua GUI (UITest Playground)
 */

#include "GUIEngine.hpp"
#include <iostream>
#include <tuple>
#include <cmath>
#include <algorithm>

extern std::atomic<bool> gRunning;

GUIEngine::GUIEngine() {}

GUIEngine::~GUIEngine() {
  shutdown();
}

bool GUIEngine::init(const std::string& title, bool vsyncEnabled) {
    // 1. Initial Lua setup
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math, sol::lib::os, sol::lib::debug, sol::lib::coroutine, sol::lib::io);
    
    lua["basePath"] = "lua/";
    std::string p = lua["package"]["path"];
    p += ";lua/?.lua;lua/?/init.lua";
    lua["package"]["path"] = p;

    // Load setbox and add platform tags
    try {
      lua.safe_script_file("lua/SetBox.lua");
#ifdef _WIN32
      lua["setbox"]["addTag"]("platform.Windows");
#elif __APPLE__
      lua["setbox"]["addTag"]("platform.macOS");
#else
      lua["setbox"]["addTag"]("platform.Linux");
#endif
    } catch (sol::error& e) { std::cerr << "SetBox core load error: " << e.what() << std::endl; return false; }

    // 2. Raylib Setup
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 850, title.c_str());
    if (vsyncEnabled) SetTargetFPS(60); else SetTargetFPS(0);
    
    windowWidth = GetScreenWidth();
    windowHeight = GetScreenHeight();
    
    // 3. Bindings
    lua["getWindowSize"] = []() { return std::make_tuple(GetScreenWidth(), GetScreenHeight()); };
    lua["getMousePos"] = []() { return std::make_tuple(GetMouseX(), GetMouseY()); };
    lua["isMouseDown"] = [](int b) { return IsMouseButtonDown(b); };
    lua["isKeyDown"] = [](int k) { return IsKeyDown(k); };
    lua["isShiftDown"] = []() { return IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT); };
    lua["isCtrlDown"] = []() { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); };
    lua["isAltDown"] = []() { return IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT); };
    lua["getMouseWheel"] = []() { return GetMouseWheelMove(); };
    lua["isMouseClicked"] = [](int b) { return IsMouseButtonPressed(b); };
    lua["isMouseReleased"] = [](int b) { return IsMouseButtonReleased(b); };
    
    lua["measureText"] = [this](std::string t) { return font.measureText(t.c_str()); };
    lua["getLineHeight"] = [this]() { return font.getLineHeight(); };
    
    lua["drawRect"] = [](float x, float y, float w, float h, float r, float g, float b, float a) {
      DrawRectangleRec({x, y, w, h}, {(unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255)});
    };
    lua["drawRectOutline"] = [](float x, float y, float w, float h, float r, float g, float b, float a, float t) {
      DrawRectangleLinesEx({x, y, w, h}, t, {(unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255)});
    };
    lua["drawRoundedRect"] = [](float x, float y, float w, float h, float rad, float r, float g, float b, float a) {
      // Raylib's DrawRectangleRounded takes a roundness factor from 0.0 to 1.0 relative to min(w,h)
      float roundness = (rad * 2.0f) / std::min(w, h);
      DrawRectangleRounded({x, y, w, h}, roundness, 20, {(unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255)});
    };
    lua["drawText"] = [this](float x, float y, std::string t, float r, float g, float b, float a) {
      return font.drawText(x, y, t, r, g, b, a);
    };
    lua["drawLine"] = [](float x1, float y1, float x2, float y2, float r, float g, float b, float a, float t) {
      DrawLineEx({x1, y1}, {x2, y2}, t, {(unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255)});
    };
    lua["drawCircle"] = [](float cx, float cy, float rad, float r, float g, float b, float a) {
      DrawCircleV({cx, cy}, rad, {(unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255)});
    };
    lua["drawCircleOutline"] = [](float cx, float cy, float rad, float r, float g, float b, float a, float t) {
      DrawCircleLinesV({cx, cy}, rad, {(unsigned char)(r*255), (unsigned char)(g*255), (unsigned char)(b*255), (unsigned char)(a*255)});
    };
    lua["setClearColor"] = [](float r, float g, float b) { /* handled in render */ };

    // Placeholder tables for UI consistency
    lua["audio"] = lua.create_table();
    lua["hw"] = lua.create_table();
    lua["rx"] = lua.create_table();
    lua["waterfall"] = lua.create_table();
    lua["dispatch"] = lua.create_table();

    // 4. Load Main Lua
    try {
      sol::protected_function_result res = lua.safe_script_file("lua/Main.lua");
      if (!res.valid()) { sol::error err = res; std::cerr << "[GUIEngine] Lua load error: " << err.what() << std::endl; return false; }
    } catch (sol::error& e) { std::cerr << "[GUIEngine] Lua catch error: " << e.what() << std::endl; return false; }

    // 5. Font Initialization
    float fontSize = 16.0f;
    sol::protected_function getNumF = lua["setbox"]["getNumber"];
    if (getNumF.valid()) {
      auto res = getNumF("fontSize");
      if (res.valid()) fontSize = (float)res.get<double>();
    }

    sol::protected_function getF = lua["setbox"]["get"];
    if (getF.valid()) {
      auto fP_res = getF("fontPaths");
      if (fP_res.valid() && fP_res.get<sol::object>().is<sol::table>()) {
        sol::table fP = fP_res.get<sol::table>();
        for (auto& kv : fP) {
          if (kv.second.is<std::string>() && font.loadFont(kv.second.as<std::string>().c_str(), fontSize)) break;
        }
      }
    }
    if (!font.isLoaded()) std::cerr << "WARNING: No font loaded!" << std::endl;

    // 6. Lua init()
    std::cout << "[GUIEngine] Calling Lua init()..." << std::endl;
    sol::protected_function initFn = lua["init"];
    if (initFn.valid()) {
      auto res = initFn();
      if (!res.valid()) { sol::error err = res; std::cerr << "Lua init() error: " << err.what() << std::endl; return false; }
    }

    running = true;
    return true;
}

void GUIEngine::run() {
    while (!WindowShouldClose() && running && gRunning.load()) {
      float dt = GetFrameTime();
      update(dt);
      render();
    }
}

void GUIEngine::update(float dt) {
    sol::protected_function updateFn = lua["update"];
    if (updateFn.valid()) { 
        auto res = updateFn(dt); 
        if (!res.valid()) { sol::error err = res; std::cerr << "Lua update() error: " << err.what() << std::endl; running = false; } 
    }
}

void GUIEngine::render() {
    BeginDrawing();
    ClearBackground({25, 25, 38, 255}); // Default background color

    sol::protected_function drawFn = lua["draw"];
    if (drawFn.valid()) { 
        auto res = drawFn(); 
        if (!res.valid()) { 
            sol::error err = res; 
            std::cerr << "[GUIEngine] Lua draw() error: " << err.what() << std::endl; 
            running = false; 
        } 
    }
    
    EndDrawing();
}

void GUIEngine::shutdown() {
    running = false; 
    font.unload();
    CloseWindow();
}
