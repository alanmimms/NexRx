/**
 * @file GUIEngine.hpp
 * @brief Raylib/Lua GUI Management (UITest Playground)
 */

#pragma once

#include "raylib.h"
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>

#include "FontRenderer.hpp"

class GUIEngine {
public:
  GUIEngine();
  ~GUIEngine();

  bool init(const std::string& title, bool vsyncEnabled = true);
  void run();
  void shutdown();

private:
  void update(float dt);
  void render();
  
  sol::state lua;
  FontRenderer font;
  
  int windowWidth, windowHeight;
  bool running = false;
};
