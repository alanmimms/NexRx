#include "UIEngine.hpp"
#include <raylib.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

static bool sQuitRequested = false;

void signalHandler(int signum) {
  if (signum == SIGINT || signum == SIGTERM) {
    sQuitRequested = true;
  }
}

UIEngine::UIEngine() {
}

UIEngine::~UIEngine() {
  SDL_Quit();
  CloseWindow();
}

void UIEngine::init() {
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(1280, 720, "NexRx UI Test");
  SetTargetFPS(60);

  Font font = LoadFont("fonts/DejaVuSans.ttf");
  if (font.texture.id == 0) {
    std::cerr << "Failed to load font fonts/DejaVuSans.ttf" << std::endl;
  } else {
    RenderBridge::setFont(font);
  }
  
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
  }

  lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math);
  lua.script("package.path = package.path .. ';lua/?.lua;?.lua'");
  
  RenderBridge::registerWithLua(lua);

  try {
    uiModule = lua.require_file("UI", "lua/UI.lua");
  } catch (const sol::error& e) {
    std::cerr << "Failed to load UI.lua: " << e.what() << std::endl;
  }
}

void UIEngine::update() {
  if (sQuitRequested) {
    // Raylib's WindowShouldClose will return true if we simulate escape or close
  }
}

bool UIEngine::shouldClose() {
  return sQuitRequested || WindowShouldClose();
}

void UIEngine::render() {
  double frameStartTime = GetTime();

  if (IsWindowResized() && uiModule != sol::nil) {
    sol::function resizeFunc = uiModule["onResize"];
    if (resizeFunc.valid()) {
      try {
        resizeFunc(GetScreenWidth(), GetScreenHeight());
      } catch (const sol::error& e) {
        std::cerr << "Lua onResize error: " << e.what() << std::endl;
      }
    }
  }

  BeginDrawing();
  ClearBackground(BLACK);

  if (uiModule != sol::nil) {
    sol::function renderFunc = uiModule["render"];
    if (renderFunc.valid()) {
      try {
        renderFunc(lua["bridge"], GetScreenWidth(), GetScreenHeight());
      } catch (const sol::error& e) {
        std::cerr << "Lua render error: " << e.what() << std::endl;
      }
    }
  }

  EndDrawing();

  double frameTime = GetTime() - frameStartTime;
  double targetFrameTime = 1.0 / 60.0;
  if (frameTime < targetFrameTime) {
    // We already have SetTargetFPS(60), which does its own waiting in EndDrawing().
    // But if we want manual control or higher precision:
    // double sleepTime = targetFrameTime - frameTime;
    // std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
  }
}
