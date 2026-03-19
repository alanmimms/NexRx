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
    LuaBridge::setFont(font);
  }
  
  if (SDL_Init(SDL_INIT_AUDIO) < 0) {
    std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
  }

  lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math);
  lua.script("package.path = package.path .. ';lua/?.lua;?.lua'");
  
  LuaBridge::registerWithLua(lua);

  try {
    uiModule = lua.require_file("UI", "lua/UI.lua");
  } catch (const sol::error& e) {
    std::cerr << "Failed to load UI.lua: " << e.what() << std::endl;
  }
}

void UIEngine::update() {
  if (sQuitRequested) return;
  pollEvents();
}

void UIEngine::pollEvents() {
  if (uiModule == sol::nil) return;

  // Mouse Motion
  Vector2 mousePos = GetMousePosition();
  sol::function onMouseMove = uiModule["onMouseMove"];
  if (onMouseMove.valid()) {
    onMouseMove(mousePos.x, mousePos.y);
  }

  // Modifiers
  int mods = 0;
  if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) mods |= 1;
  if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) mods |= 2;
  if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) mods |= 4;
  if (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) mods |= 8;

  // Mouse Buttons
  sol::function onMouseEvent = uiModule["onMouseEvent"];
  if (onMouseEvent.valid()) {
    for (int b = 0; b < 3; b++) {
      if (IsMouseButtonPressed(b)) {
        onMouseEvent("button", mousePos.x, mousePos.y, b, true, mods);
      } else if (IsMouseButtonReleased(b)) {
        onMouseEvent("button", mousePos.x, mousePos.y, b, false, mods);
      }
    }
  }

  // Keyboard
  sol::function onKeyEvent = uiModule["onKeyEvent"];
  if (onKeyEvent.valid()) {
    int key = GetKeyPressed();
    while (key > 0) {
      onKeyEvent(key, true, mods);
      key = GetKeyPressed();
    }
  }
}

bool UIEngine::shouldClose() {
  return sQuitRequested || WindowShouldClose();
}

void UIEngine::render() {
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
}
