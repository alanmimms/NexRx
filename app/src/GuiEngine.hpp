/**
 * @file GuiEngine.hpp
 * @brief SDL/OpenGL/Lua GUI Management
 */

#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

#include "FontRenderer.hpp"
#include "AudioEngine.hpp"
#include "WaterfallRenderer.hpp"
#include "TwinConn.hpp"
#include "DspEngine.hpp"

struct InputState {
  int mouseX = 0, mouseY = 0, mouseWheel = 0;
  bool mouseDown[3] = {false, false, false};
  bool mouseClicked[3] = {false, false, false};
  bool mouseReleased[3] = {false, false, false};
  bool keyDown[512] = {false};
  bool shiftDown = false, ctrlDown = false, altDown = false;

  void beginFrame() {
    for (int i = 0; i < 3; ++i) {
      mouseClicked[i] = false;
      mouseReleased[i] = false;
    }
    mouseWheel = 0;
  }
};

class GuiEngine {
public:
  GuiEngine(DspEngine& dsp);
  ~GuiEngine();

  bool init(const std::string& title, bool vsyncEnabled = true);
  void run();
  void shutdown();

private:
  void update(float dt);
  void render();
  
  void startCommandThread();
  void stopCommandThread();
  void postCommand(std::function<void()> cmd);
  void processCommands();

  DspEngine& dsp_;
  
  SDL_Window* window = nullptr;
  SDL_GLContext glContext = nullptr;
  sol::state lua;
  
  FontRenderer font;
  AudioEngine audio;
  WaterfallRenderer waterfall;
  nexrx::TwinConn twinHost;
  
  InputState input;
  int windowWidth, windowHeight;
  bool running = false;
  double lastVFOHz = 14.2e6;
  std::atomic<bool> twinConnected{false};

  std::queue<std::function<void()>> commandQueue;
  std::mutex cmdMutex;
  std::thread commandThread;
  std::atomic<bool> commandThreadRunning{false};
};
