/**
 * @file GUIEngine.hpp
 * @brief SDL/OpenGL/Lua GUI Management
 */

#pragma once

#include <raylib.h>
#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <string>
#include <map>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

#include "AudioEngine.hpp"
#include "WaterfallRenderer.hpp"
#include "RadioSource.hpp"
#include "TwinConn.hpp"
#include "DSPEngine.hpp"

class GUIEngine {
public:
  GUIEngine(DSPEngine& dsp);
  ~GUIEngine();

  bool init(const std::string& title, bool vsyncEnabled = true);
  void run();
  void shutdown();

  // Getters for Lua Bridge
  DSPEngine& getDSP() { return dsp_; }
  AudioEngine& getAudio() { return audio; }
  WaterfallRenderer& getWaterfall() { return waterfall; }
  
  nexrx::RadioSource* getRadioSource() { return radioSource.get(); }
  
  bool connectTwin(const std::string& host, int cp, int sp);
  void disconnectTwin();
  bool isTwinConnected() const { return twinConnected.load(); }
  sol::object getTwinState(sol::this_state s);
  
  void postTwinCommand(const std::string& name, std::function<void()> cmd);
  double getLastVFOHz() const { return lastVFOHz; }
  void setLastVFOHz(double f) { lastVFOHz = f; }

private:
  void update(float dt);
  void render();
  
  void startCommandThread();
  void stopCommandThread();

  DSPEngine& dsp_;
  
  sol::state lua;
  sol::table uiModule;
  
  AudioEngine audio;
  WaterfallRenderer waterfall;
  std::unique_ptr<nexrx::RadioSource> radioSource;
  
  int windowWidth, windowHeight;
  bool running = false;
  double lastVFOHz = 14.2e6;
  std::chrono::steady_clock::time_point lastStatePollTime;
  std::atomic<bool> twinConnected{false};

  // Coalescing Command Queue
  std::map<std::string, std::function<void()>> pendingCommands;
  std::mutex cmdMutex;
  std::thread commandThread;
  std::atomic<bool> commandThreadRunning{false};
};
