/**
 * @file main.cpp
 * @brief NexRx Application - SDL2/OpenGL with Lua GUI
 */

#include "FontRenderer.hpp"
#include "AudioEngine.hpp"
#include "WaterfallRenderer.hpp"
#include "RateAdaptiveBuffer.hpp"
#include "BasebandFilter.hpp"
#include "Socket.hpp"
#include "TwinConn.hpp"
#include "transport/IQFrame.hpp"
#include "Demodulator.hpp"

#include <SDL.h>
#include <SDL_opengl.h>
#include <sol/sol.hpp>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <complex>
#include <mutex>
#include <algorithm>
#include <vector>
#include <deque>
#include <functional>
#include <thread>
#include <atomic>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#include <xmmintrin.h>
#include <pmmintrin.h>
#define HAVE_SSE_DENORMAL_CONTROL 1
#endif

class App;
static App* gApp = nullptr;

struct InputState {
  int mouseX = 0, mouseY = 0, mouseWheel = 0;
  bool mouseDown[3] = {false, false, false};
  bool mouseClicked[3] = {false, false, false};
  bool mouseReleased[3] = {false, false, false};
  bool keyDown[512] = {false};
  bool keyPressed[512] = {false};
  bool shiftDown = false, ctrlDown = false, altDown = false;
  std::string textInput;

  void beginFrame() {
    for (int i = 0; i < 3; ++i) {
      mouseClicked[i] = false;
      mouseReleased[i] = false;
    }
    for (int i = 0; i < 512; ++i) {
      keyPressed[i] = false;
    }
    mouseWheel = 0;
    textInput.clear();
  }
};

class App {
public:
  App() {
    iqBuffer.assign(FFT_SIZE * 2, 0.0f);
    lmsMu = 0.001f;
  }
  ~App() { shutdown(); }

  bool init(const std::string& title, bool vsyncEnabled = true) {
    if (!initLuaConfig()) {
      return false;
    }
    int w = 1280, h = 850;
    float fS = 16.0f;
    sol::function getNum = lua["setbox"]["getNumber"];
    if (getNum.valid()) { 
      w = (int)getNum("windowWidth", 1280).get<double>(); 
      h = (int)getNum("windowHeight", 850).get<double>(); 
      fS = (float)getNum("fontSize", 16.0).get<double>(); 
      rfGainDB.store((float)getNum("rfGainDb", 20.0).get<double>());
      qsdOffsetKhz = getNum("qsdOffsetK", 12.0).get<double>();
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
      return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); 
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    
    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!window) {
      return false;
    }
    glContext = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(vsyncEnabled ? 1 : 0);
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    sol::object fP = lua["setbox"]["get"]("fontPaths");
    if (fP.is<sol::table>()) {
      for (auto& kv : fP.as<sol::table>()) {
        if (kv.second.is<std::string>() && font.loadFont(kv.second.as<std::string>().c_str(), fS)) {
          break;
        }
      }
    }
    
    if (!audio.init(48000, 2)) {
      return false;
    }
    
    iqBuffer.resize(FFT_SIZE * 2, 0.0f);
    
    nexrx::BufferConfig aC;
    aC.capacity = 32768;
    aC.targetFillRatio = 0.5f;
    aC.enableAdaptation = true;
    audioBuffer.configure(aC);
    
    demod.setSampleRate(96000.0f);
    
    if (getNum.valid()) {
      audioVolume.store((float)getNum("rxVolume", 0.5).get<double>());
      demod.setBfoOffset((float)getNum("sidetoneFreq", 700.0).get<double>());
    }
    
    sol::function getStr = lua["setbox"]["getString"];
    if (getStr.valid()) {
      std::string mode = getStr("defaultMode", "USB").get<std::string>();
      if (mode == "USB") demod.setMode(Demodulator::Mode::USB);
      else if (mode == "LSB") demod.setMode(Demodulator::Mode::LSB);
      else if (mode == "AM") demod.setMode(Demodulator::Mode::AM);
      else if (mode == "CW") demod.setMode(Demodulator::Mode::CW);
    }
    
    audio.setCallback([this](float* out, uint32_t fC, uint32_t ch) {
      const float vol = audioVolume.load();
      thread_local std::vector<float> tmp;
      tmp.resize(fC);
      size_t read = audioBuffer.read(std::span<float>(tmp.data(), fC));
      
      static uint32_t callCount = 0;
      if (callCount % 100 == 0) {
        if (read < fC && !audio.isTestToneEnabled() && twinConnected.load()) {
          std::cout << "[Audio] Buffer fills: read " << read << "/" << fC << " (Available: " << audioBuffer.available() << ")" << std::endl;
        }
      }
      callCount++;

      for (uint32_t i = 0; i < fC; ++i) {
        float s = tmp[i]; // Volume now applied in AudioEngine::processAudio
        out[i*ch] = s;
        if (ch > 1) {
          out[i*ch+1] = s;
        }
      }
    });
    
    startCommandThread();
    if (!initLua()) {
      return false;
    }
    running = true;
    return true;
  }

  void shutdown() {
    stopCommandThread();
    if (twinConnected.load()) {
      twinHost.stopReceiving();
      twinHost.shutdown();
      twinConnected.store(false);
    }
    audio.shutdown();
    if (glContext) {
      SDL_GL_DeleteContext(glContext);
    }
    if (window) {
      SDL_DestroyWindow(window);
    }
    SDL_Quit();
  }

  void startCommandThread() {
    commandThreadRunning = true;
    commandThread = std::thread([this]() {
      while (commandThreadRunning) {
        std::function<void()> cmd;
        {
          std::lock_guard<std::mutex> l(commandMutex);
          if (!commandQueue.empty()) {
            cmd = commandQueue.front();
            commandQueue.pop_front();
          }
        }
        if (cmd) {
          cmd();
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
      }
    });
  }

  void stopCommandThread() {
    commandThreadRunning = false;
    if (commandThread.joinable()) {
      commandThread.join();
    }
  }

  void postCommand(std::function<void()> cmd) {
    std::lock_guard<std::mutex> l(commandMutex);
    commandQueue.push_back(cmd);
  }

  void run() {
    while (running) {
      uint32_t fS = SDL_GetTicks();
      input.beginFrame();
      pollEvents();
      uint32_t now = SDL_GetTicks();
      float dt = (now - lastFrameTime) / 1000.0f;
      lastFrameTime = now;
      callLuaUpdate(dt);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      setup2DProjection();
      callLuaDraw();
      SDL_GL_SwapWindow(window);
      uint32_t elapsed = SDL_GetTicks() - fS;
      if (elapsed < 16) {
        SDL_Delay(16 - elapsed);
      }
    }
  }

  void drawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
  }

  void drawRectOutline(float x, float y, float w, float h, float r, float g, float b, float a, float t) {
    glColor4f(r, g, b, a);
    glLineWidth(t);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
  }

  void drawCircle(float cx, float cy, float rad, float r, float g, float b, float a, int seg = 32) {
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= seg; ++i) {
      float ang = 2.0f * 3.14159265f * i / seg;
      glVertex2f(cx + cosf(ang)*rad, cy + sinf(ang)*rad);
    }
    glEnd();
  }

  void drawCircleOutline(float cx, float cy, float rad, float r, float g, float b, float a, float t, int seg = 32) {
    glColor4f(r, g, b, a);
    glLineWidth(t);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < seg; ++i) {
      float ang = 2.0f * 3.14159265f * i / seg;
      glVertex2f(cx + cosf(ang)*rad, cy + sinf(ang)*rad);
    }
    glEnd();
  }

  void setClearColor(float r, float g, float b) {
    glClearColor(r, g, b, 1.0f);
  }

  float drawText(float x, float y, const std::string& t, float r, float g, float b, float a) {
    return font.drawText(x, y, t, r, g, b, a);
  }

  float measureText(const std::string& t) {
    return font.measureText(t);
  }

  float getLineHeight() {
    return font.getLineHeight();
  }

  void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a, float t) {
    glColor4f(r, g, b, a);
    glLineWidth(t);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
  }

  void drawRoundedRect(float x, float y, float w, float h, float rad, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    rad = std::min(rad, std::min(w, h) / 2.0f);
    if (rad < 0.5f) {
      drawRect(x, y, w, h, r, g, b, a);
      return;
    }
    const int seg = 8;
    constexpr float PI = 3.14159265f;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x + w/2, y + h/2);
    glVertex2f(x + rad, y);
    glVertex2f(x + w - rad, y);
    for (int i = 0; i <= seg; ++i) {
      float ang = -PI/2.0f + (PI/2.0f) * i / seg;
      glVertex2f(x + w - rad + cosf(ang)*rad, y + rad + std::sin(ang) * rad);
    }
    glVertex2f(x + w, y + h - rad);
    for (int i = 0; i <= seg; ++i) {
      float ang = 0.0f + (PI/2.0f) * i / seg;
      glVertex2f(x + w - rad + cosf(ang)*rad, y + h - rad + std::sin(ang) * rad);
    }
    glVertex2f(x + rad, y + h);
    for (int i = 0; i <= seg; ++i) {
      float ang = PI/2.0f + (PI/2.0f) * i / seg;
      glVertex2f(x + rad + std::cos(ang) * rad, y + h - rad + std::sin(ang) * rad);
    }
    glVertex2f(x, y + rad);
    for (int i = 0; i <= seg; ++i) {
      float ang = PI + (PI/2.0f) * i / seg;
      glVertex2f(x + rad + std::cos(ang) * rad, y + rad + std::sin(ang) * rad);
    }
    glVertex2f(x + rad, y);
    glEnd();
  }

private:
  bool initLuaConfig() {
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::io, sol::lib::os);
    std::string p = lua["package"]["path"];
    p += ";lua/?.lua;lua/?/init.lua";
    lua["package"]["path"] = p;
    try {
      lua.safe_script_file("lua/setbox.lua", sol::script_pass_on_error);
    } catch (...) {
      return false;
    }
#ifdef _WIN32
    lua["setbox"]["addTag"]("platform.Windows");
#elif __APPLE__
    lua["setbox"]["addTag"]("platform.macOS");
#else
    lua["setbox"]["addTag"]("platform.Linux");
#endif
    sol::function loadF = lua["setbox"]["loadFile"];
    if (loadF.valid()) {
      loadF("config/default.lua");
      loadF("config/settings.lua");
    }
    luaConfigLoaded = true;
    return true;
  }

  bool initLua() {
    if (!luaConfigLoaded) {
      lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::io, sol::lib::os);
    }
    lua["isShiftDown"] = [this]() { return input.shiftDown; };
    lua["isCtrlDown"] = [this]() { return input.ctrlDown; };
    lua["isAltDown"] = [this]() { return input.altDown; };
    lua.set_function("quit", [this]() { running = false; });
    lua.set_function("getWindowSize", [this]() { return std::make_tuple(windowWidth, windowHeight); });
    lua.set_function("getMousePos", [this]() { return std::make_tuple(input.mouseX, input.mouseY); });
    lua.set_function("isMouseDown", [this](int b) { if (b >= 0 && b < 3) return input.mouseDown[b]; return false; });
    lua.set_function("isMouseClicked", [this](int b) { if (b >= 0 && b < 3) return input.mouseClicked[b]; return false; });
    lua.set_function("isMouseReleased", [this](int b) { if (b >= 0 && b < 3) return input.mouseReleased[b]; return false; });
    lua.set_function("isKeyDown", [this](int k) { if (k >= 0 && k < 512) return input.keyDown[k]; return false; });
    lua.set_function("getMouseWheel", [this]() { return input.mouseWheel; });
    lua.set_function("getTextInput", [this]() { return input.textInput; });
    lua.set_function("drawRect", [this](float x, float y, float w, float h, float r, float g, float b, float a) { drawRect(x, y, w, h, r, g, b, a); });
    lua.set_function("drawRectOutline", [this](float x, float y, float w, float h, float r, float g, float b, float a, float t) { drawRectOutline(x, y, w, h, r, g, b, a, t); });
    lua.set_function("drawCircle", [this](float cx, float cy, float rad, float r, float g, float b, float a) { drawCircle(cx, cy, rad, r, g, b, a); });
    lua.set_function("drawCircleOutline", [this](float cx, float cy, float rad, float r, float g, float b, float a, float t, int seg = 32) { drawCircleOutline(cx, cy, rad, r, g, b, a, t, seg); });
    lua.set_function("setClearColor", [this](float r, float g, float b) { setClearColor(r, g, b); });
    lua.set_function("drawText", [this](float x, float y, const std::string& t, float r, float g, float b, float a) { return drawText(x, y, t, r, g, b, a); });
    lua.set_function("measureText", [this](const std::string& t) { return measureText(t); });
    lua.set_function("getLineHeight", [this]() { return getLineHeight(); });
    lua.set_function("drawLine", [this](float x1, float y1, float x2, float y2, float r, float g, float b, float a, float t) { drawLine(x1, y1, x2, y2, r, g, b, a, t); });
    lua.set_function("drawRoundedRect", [this](float x, float y, float w, float h, float rad, float r, float g, float b, float a) { drawRoundedRect(x, y, w, h, rad, r, g, b, a); });

    lua["audio"] = lua.create_table();
    lua["audio"]["start"] = [this]() { return audio.start(); };
    lua["audio"]["stop"] = [this]() { audio.stop(); };
    lua["audio"]["setVolume"] = [this](float v) { audio.setVolume(v); };
    lua["audio"]["isInitialized"] = [this]() { return audio.isInitialized(); };
    lua["audio"]["setTestTone"] = [this](bool en, float freq) { audio.setTestTone(en, freq); };

    lua["waterfall"] = lua.create_table();
    lua["waterfall"]["init"] = [this](int w, int h) { return waterfall.init(w, h); };
    lua["waterfall"]["addRow"] = [this](sol::table d) { std::vector<float> row; row.reserve(d.size()); for (size_t i = 1; i <= d.size(); ++i) row.push_back(d[i].get_or(waterfall.getMinDB())); waterfall.addRow(row.data(), (int)row.size()); };
    lua["waterfall"]["render"] = [this](float x, float y, float w, float h) { waterfall.render(x, y, w, h); };
    lua["waterfall"]["renderSpectrum"] = [this](sol::table d, float x, float y, float w, float h) { std::vector<float> spec; spec.reserve(d.size()); for (size_t i = 1; i <= d.size(); ++i) spec.push_back(d[i].get_or(waterfall.getMinDB())); waterfall.renderSpectrum(spec.data(), (int)spec.size(), x, y, w, h); };
    lua["waterfall"]["setColormapData"] = [this](sol::table stops) { std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>> grad; for (size_t i = 1; i <= stops.size(); ++i) { sol::table s = stops[i]; grad.push_back({s[1].get_or(0.0f), (uint8_t)s[2].get_or(0), (uint8_t)s[3].get_or(0), (uint8_t)s[4].get_or(0)}); } waterfall.setColormapData(grad); };
    lua["waterfall"]["setRange"] = [this](float min, float max) { waterfall.setRange(min, max); };
    lua["waterfall"]["isInitialized"] = [this]() { return waterfall.isInitialized(); };

    sol::table hw = lua.create_table();
    hw["connect"] = [this](sol::optional<std::string> h, sol::optional<int> cp, sol::optional<int> sp) {
      nexrx::TwinConfig c;
      c.host = h.value_or("127.0.0.1");
      c.controlPort = (uint16_t)cp.value_or(5000);
      c.streamPort = (uint16_t)sp.value_or(5001);
      
      // Initialize spectrum buffer
      iqBuffer.assign(FFT_SIZE * 2, 0.0f);
      iqBufferWritePos.store(0);

      if (twinHost.initialize(c)) {
        twinHost.setFrameCallback([this](const nexrx::IQFrame& f) { processIQFrame(f); });
        if (twinHost.startReceiving()) {
          twinConnected.store(true);
          twinHost.startStream();
          return true;
        }
      }
      return false;
    };
    hw["disconnect"] = [this]() { postCommand([this]() { twinHost.stopReceiving(); twinHost.shutdown(); }); twinConnected.store(false); };
    hw["isConnected"] = [this]() { return twinConnected.load() && twinHost.isConnected(); };
    hw["getSpectrum"] = [this](sol::this_state s) { computeSpectrum(); sol::state_view lView(s); sol::table res = lView.create_table(); std::lock_guard<std::mutex> l(spectrumMutex); for (size_t i = 0; i < spectrumData.size(); ++i) res[i + 1] = spectrumData[i]; return res; };
    hw["startStream"] = [this]() { if (twinConnected.load()) { audioBuffer.clear(); return twinHost.startStream(); } return false; };
    hw["stopStream"] = [this]() { if (twinConnected.load()) return twinHost.stopStream(); return false; };
    hw["getFramesReceived"] = [this]() { return twinHost.getFramesReceived(); };
    hw["getDspStats"] = [this](sol::this_state s) {
      sol::state_view lView(s);
      sol::table res = lView.create_table();
      res["signalRms"] = dspDiag.signalRms.load();
      res["maxRaw"] = dspDiag.maxRaw.load();
      res["maxAudio"] = dspDiag.maxAudio.load();
      res["lmsWeightR"] = dspDiag.lmsWeightR.load();
      res["lmsWeightI"] = dspDiag.lmsWeightI.load();
      res["framesProcessed"] = dspDiag.framesProcessed.load();
      return res;
    };
    hw["resetDspStats"] = [this]() {
      dspDiag.maxRaw.store(0);
      dspDiag.maxAudio.store(0);
    };
    
    hw["setVFO"] = [this](double f, double k) { if (twinConnected.load()) { postCommand([this, f, k]() { twinHost.setVFO(f, k); }); } };
    hw["setAttenuation"] = [this](int db) { if (twinConnected.load()) { postCommand([this, db]() { twinHost.setAtten(db); }); } };
    hw["setPGAGain"] = [this](int code) { if (twinConnected.load()) { postCommand([this, code]() { twinHost.setPGAGain(code); }); } };
    hw["setAGCMode"] = [this](int m) { if (twinConnected.load()) { postCommand([this, m]() { twinHost.setAGCMode(m); }); } };
    hw["setISGFreq"] = [this](double f) { if (twinConnected.load()) { postCommand([this, f]() { twinHost.setISGFreq(f); }); } };
    hw["setIsgFreq"] = hw["setISGFreq"];
    hw["setISGEnable"] = [this](bool en) { if (twinConnected.load()) { postCommand([this, en]() { twinHost.setISGEnable(en); }); } };
    hw["setIsgEnable"] = hw["setISGEnable"];
    hw["setPreselectorInd"] = [this](uint32_t mask) { if (twinConnected.load()) { postCommand([this, mask]() { twinHost.setPreselectorL(mask); }); } };
    hw["setPreselectorCap"] = [this](uint32_t mask) { if (twinConnected.load()) { postCommand([this, mask]() { twinHost.setPreselectorCap(mask); }); } };
    hw["setPreselectorEnabled"] = [this](bool en) { if (twinConnected.load()) { postCommand([this, en]() { twinHost.setPreselectorEnabled(en); }); } };
    hw["setTrMode"] = [this](int m) { if (twinConnected.load()) { postCommand([this, m]() { twinHost.setTrMode(m); }); } };
    hw["setQsdOffset"] = [this](double k) { qsdOffsetKhz = k; if (twinConnected.load()) { postCommand([this, k]() { twinHost.setVFO(lastVFOHz, k * 1000.0); }); } };
    hw["setRfGain"] = [this](double db) { rfGainDB.store(db); };
    lua["hw"] = hw;

    sol::table rx = lua.create_table();
    rx["setModeId"] = [this](int id) { if (id >= 0 && id <= 3) demod.setMode((Demodulator::Mode)id); };
    rx["setBfo"] = [this](float hz) { demod.setBfoOffset(hz); };
    rx["setBandpassEnabled"] = [this](bool en) { basebandFilter.setBandpassEnabled(en); };
    rx["setBandpassCenter"] = [this](float hz) { basebandFilter.setBandpassCenter(hz); };
    rx["setBandpassWidth"] = [this](float hz) { basebandFilter.setBandpassWidth(hz); };
    rx["setNotchEnabled"] = [this](bool en) { basebandFilter.setNotchEnabled(en); };
    rx["setNotchCenter"] = [this](float hz) { basebandFilter.setNotchCenter(hz); };
    rx["setNotchWidth"] = [this](float hz) { basebandFilter.setNotchWidth(hz); };
    rx["setAgcEnabled"] = [this](bool en) { /* TODO */ };
    rx["setNrEnabled"] = [this](bool en) { /* TODO */ };
    rx["setNbEnabled"] = [this](bool en) { /* TODO */ };
    rx["setLmsMu"] = [this](float mu) { lmsMu = std::clamp(mu, 0.0001f, 1.0f); };
    rx["setMute"] = [this](bool en) { audio.setMuted(en); };
    rx["setDemodFilterEnabled"] = [this](bool en) { demod.setFilterEnabled(en); };
    rx["setVfo"] = [this](double f) {
      lastVFOHz = f;
      // Reset phase rotators and LMS weights
      shiftCos0 = 1.0f; shiftSin0 = 0.0f;
      shiftCos1 = 1.0f; shiftSin1 = 0.0f;
      lmsW0_r = 1.0f; lmsW0_i = 0.0f;
      lmsW1_r = 1.0f; lmsW1_i = 0.0f;
      sampleBlockCounter = 0;
      lmsAcc_r = 0.0f; lmsAcc_i = 0.0f;
      
      if (twinConnected.load()) {
        double k = qsdOffsetKhz * 1000.0;
        postCommand([this, f, k]() { twinHost.setVFO(f, k); });
      }
    };
    rx["getSignalRms"] = [this]() { return signalLevelRMS.load(); };
    lua["rx"] = rx;

    try {
      auto res = lua.safe_script_file("lua/main.lua", sol::script_pass_on_error);
      if (!res.valid()) {
        sol::error err = res;
        std::cerr << "Lua error: " << err.what() << std::endl;
        return false;
      }
    } catch (...) {
      return false;
    }
    sol::function initFn = lua["init"];
    if (initFn.valid()) {
      auto res = initFn();
      if (!res.valid()) {
        sol::error err = res;
        std::cerr << "init() error: " << err.what() << std::endl;
      }
    }
    return true;
  }

  void pollEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT:
          running = false;
          break;
        case SDL_WINDOWEVENT:
          if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
            windowWidth = e.window.data1;
            windowHeight = e.window.data2;
            glViewport(0, 0, windowWidth, windowHeight);
          }
          break;
        case SDL_MOUSEMOTION:
          input.mouseX = e.motion.x;
          input.mouseY = e.motion.y;
          break;
        case SDL_MOUSEBUTTONDOWN:
          if (e.button.button <= 3) {
            int idx = e.button.button - 1;
            input.mouseDown[idx] = true;
            input.mouseClicked[idx] = true;
          }
          break;
        case SDL_MOUSEBUTTONUP:
          if (e.button.button <= 3) {
            int idx = e.button.button - 1;
            input.mouseDown[idx] = false;
            input.mouseReleased[idx] = true;
          }
          break;
        case SDL_MOUSEWHEEL:
          input.mouseWheel = e.wheel.y;
          break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
          if (e.key.keysym.scancode < 512) {
            input.keyDown[e.key.keysym.scancode] = (e.type == SDL_KEYDOWN);
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
              input.keyPressed[e.key.keysym.scancode] = true;
            }
          }
          break;
        case SDL_TEXTINPUT:
          input.textInput += e.text.text;
          break;
      }
    }
    // Robustly update modifiers using SDL state
    SDL_Keymod mod = SDL_GetModState();
    input.shiftDown = (mod & KMOD_SHIFT) != 0;
    input.ctrlDown = (mod & KMOD_CTRL) != 0;
    input.altDown = (mod & KMOD_ALT) != 0;
  }

  void setup2DProjection() {
    glViewport(0, 0, windowWidth, windowHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
  }

  void callLuaUpdate(float dt) {
    sol::function f = lua["update"];
    if (f.valid()) {
      auto res = f(dt);
      if (!res.valid()) {
        sol::error err = res;
        std::cerr << "update() error: " << err.what() << std::endl;
      }
    }
  }

  void callLuaDraw() {
    sol::function f = lua["draw"];
    if (f.valid()) {
      auto res = f();
      if (!res.valid()) {
        sol::error err = res;
        std::cerr << "draw() error: " << err.what() << std::endl;
      }
    }
  }

  struct DSPDiagnostics {
    std::atomic<float> signalRms{0.0f};
    std::atomic<float> maxRaw{0.0f};
    std::atomic<float> maxAudio{0.0f};
    std::atomic<float> lmsWeightR{1.0f};
    std::atomic<float> lmsWeightI{0.0f};
    std::atomic<uint32_t> framesProcessed{0};
  } dspDiag;

  void processIQFrame(const nexrx::IQFrame& frame) {
    float i0, q0, i1, q1, i2, q2;
    frame.qsd[0].toFloat(i0, q0);
    frame.qsd[1].toFloat(i1, q1);
    frame.qsd[2].toFloat(i2, q2);
    
    constexpr float sampleRate = 96000.0f;
    float k_hz = static_cast<float>(qsdOffsetKhz * 1000.0);

    if (k_hz != lastShiftK) {
      float phaseInc = 2.0f * 3.14159265f * k_hz / sampleRate;
      shiftCosD = std::cos(phaseInc);
      shiftSinD = std::sin(phaseInc);
      lastShiftK = k_hz;
    }

    float c0 = shiftCos0 * shiftCosD - shiftSin0 * shiftSinD;
    float s0 = shiftSin0 * shiftCosD + shiftCos0 * shiftSinD;
    shiftCos0 = c0; shiftSin0 = s0;

    float c1 = shiftCos1 * shiftCosD - shiftSin1 * shiftSinD;
    float s1 = shiftSin1 * shiftCosD + shiftCos1 * shiftSinD;
    shiftCos1 = c1; shiftSin1 = s1;

    // Mixer 0 (f-k): Target is at +k. Shift by -k.
    float i0_s = i0 * shiftCos0 + q0 * shiftSin0;
    float q0_s = q0 * shiftCos0 - i0 * shiftSin0;

    // Mixer 1 (f+k): Target is at -k. Shift by +k.
    float i1_s = i1 * shiftCos1 - q1 * shiftSin1;
    float q1_s = q1 * shiftCos1 + i1 * shiftSin1;

    if ((dspDiag.framesProcessed.load() & 0x3FFF) == 0) {
      auto renorm = [](float& c, float& s) {
        float mag = std::sqrt(c*c + s*s);
        if (mag > 0) { c /= mag; s /= mag; }
      };
      renorm(shiftCos0, shiftSin0);
      renorm(shiftCos1, shiftSin1);
    }

    // LMS should correlate the two shifted perspectives to align them
    float w1_i_r = lmsW0_r * i1_s - lmsW0_i * q1_s;
    float w1_i_q = lmsW0_r * q1_s + lmsW0_i * i1_s;

    float error_i = i0_s - w1_i_r;
    float error_q = q0_s - w1_i_q;

    // Update weights to minimize difference (correlation)
    lmsAcc_r += (error_i * i1_s + error_q * q1_s);
    lmsAcc_i += (error_q * i1_s - error_i * q1_s);
    
    if (++sampleBlockCounter >= 32) {
      lmsW0_r += lmsMu * lmsAcc_r;
      lmsW0_i += lmsMu * lmsAcc_i;
      
      float magSq = lmsW0_r * lmsW0_r + lmsW0_i * lmsW0_i;
      if (magSq > 4.0f) {
        float scale = 2.0f / std::sqrt(magSq);
        lmsW0_r *= scale; lmsW0_i *= scale;
      }
      
      dspDiag.lmsWeightR.store(lmsW0_r, std::memory_order_relaxed);
      dspDiag.lmsWeightI.store(lmsW0_i, std::memory_order_relaxed);
      lmsAcc_r = 0.0f; lmsAcc_i = 0.0f;
      sampleBlockCounter = 0;
    }

    // Fundamental combining: 0.5*S_low + 0.5*S_high
    float iF = 0.5f * i0_s + 0.5f * w1_i_r;
    float qF = 0.5f * q0_s + 0.5f * w1_i_q;
    
    float rfGain = std::pow(10.0f, rfGainDB.load() / 20.0f);
    iF *= rfGain; qF *= rfGain;

    float maxR = std::max(std::abs(iF), std::abs(qF));
    if (maxR > dspDiag.maxRaw.load()) dspDiag.maxRaw.store(maxR, std::memory_order_relaxed);

    basebandFilter.recompute();
    basebandFilter.process(iF, qF);

    size_t pos = iqBufferWritePos.load(std::memory_order_relaxed); 
    if (iqBuffer.size() >= FFT_SIZE*2) {
      iqBuffer[pos*2] = iF; iqBuffer[pos*2+1] = qF;
      iqBufferWritePos.store((pos+1)%FFT_SIZE, std::memory_order_release);
    }
    
    float aOut = demod.process(iF, qF);
    dspDiag.signalRms.store(demod.getSignalLevelRMS(), std::memory_order_relaxed);
    
    if (std::abs(aOut) > dspDiag.maxAudio.load()) dspDiag.maxAudio.store(std::abs(aOut), std::memory_order_relaxed);

    if (!audioDecimateSkip) { audioBuffer.write(aOut); }
    audioDecimateSkip = !audioDecimateSkip;
    dspDiag.framesProcessed.fetch_add(1, std::memory_order_relaxed);
  }

  void fftInPlace(float* re, float* im, size_t n) {
    for (size_t i=1, j=0; i<n; ++i) {
      size_t bit=n>>1;
      for (; j&bit; bit>>=1) {
        j^=bit;
      }
      j^=bit;
      if (i<j) {
        std::swap(re[i], re[j]);
        std::swap(im[i], im[j]);
      }
    }
    for (size_t len=2; len<=n; len<<=1) {
      float ang = -2.0f*3.14159265f/len;
      float wRe = std::cos(ang);
      float wIm = std::sin(ang);
      for (size_t i=0; i<n; i+=len) {
        float cR = 1.0f, cI = 0.0f;
        for (size_t j=0; j<len/2; ++j) {
          size_t u = i+j, v = i+j+len/2;
          float tR = cR*re[v] - cI*im[v];
          float tI = cR*im[v] + cI*re[v];
          re[v] = re[u]-tR;
          im[v] = im[u]-tI;
          re[u] += tR;
          im[u] += tI;
          float nR = cR*wRe - cI*wIm;
          cI = cR*wIm + cI*wRe;
          cR = nR;
        }
      }
    }
  }

  void computeSpectrum() {
    static std::vector<float> win;
    if (win.size() != FFT_SIZE) {
      win.resize(FFT_SIZE);
      for (size_t n=0; n<FFT_SIZE; ++n) {
        win[n] = 0.5f*(1.0f-std::cos(2.0f*3.14159265f*n/(FFT_SIZE-1)));
      }
    }
    static std::vector<float> fRe(FFT_SIZE), fIm(FFT_SIZE), avgS;
    static bool avgI = false;
    {
      std::lock_guard<std::mutex> l(spectrumMutex);
      if (iqBuffer.size() < FFT_SIZE*2) {
        return;
      }
      size_t wP = iqBufferWritePos.load(std::memory_order_acquire);
      size_t rS = (wP + 8)%FFT_SIZE;
      for (size_t n=0; n<FFT_SIZE; ++n) {
        size_t idx = (rS+n)%FFT_SIZE;
        fRe[n] = iqBuffer[idx*2]*win[n];
        fIm[n] = iqBuffer[idx*2+1]*win[n];
      }
    }
    fftInPlace(fRe.data(), fIm.data(), FFT_SIZE);
    std::vector<float> lS(FFT_SIZE);
    for (size_t k=0; k<FFT_SIZE; ++k) {
      float mag = std::sqrt(fRe[k]*fRe[k] + fIm[k]*fIm[k])/FFT_SIZE*2.0f;
      lS[(k+FFT_SIZE/2)%FFT_SIZE] = (mag > 1e-10f) ? 20.0f*std::log10(mag) : -100.0f;
    }
    if (!avgI || avgS.size() != FFT_SIZE) {
      avgS = lS;
      avgI = true;
    } else {
      for (size_t k=0; k<FFT_SIZE; ++k) {
        avgS[k] = 0.3f*lS[k] + 0.7f*avgS[k];
      }
    }
    {
      std::lock_guard<std::mutex> l(spectrumMutex);
      spectrumData = avgS;
    }
  }

  SDL_Window* window = nullptr;
  SDL_GLContext glContext = nullptr;
  sol::state lua;
  FontRenderer font;
  AudioEngine audio;
  WaterfallRenderer waterfall;
  nexrx::TwinConn twinHost;
  std::mutex spectrumMutex;
  std::vector<float> spectrumData;
  std::vector<float> iqBuffer;
  std::atomic<size_t> iqBufferWritePos{0};
  
  static constexpr size_t FFT_SIZE = 1024;
  std::atomic<bool> twinConnected{false};
  double qsdOffsetKhz = 0.0;
  std::atomic<float> rfGainDB{0.0f};
  
  float shiftCos0 = 1.0f, shiftSin0 = 0.0f;
  float shiftCos1 = 1.0f, shiftSin1 = 0.0f;
  float shiftCosD = 1.0f, shiftSinD = 0.0f;
  float lastShiftK = -1.0f;
  
  float lmsW0_r = 1.0f, lmsW0_i = 0.0f;
  float lmsW1_r = 1.0f, lmsW1_i = 0.0f;
  float lmsMu = 0.01f;
  uint32_t sampleBlockCounter = 0;
  float lmsAcc_r = 0.0f, lmsAcc_i = 0.0f;
  
  Demodulator demod;
  nexrx::BasebandFilter basebandFilter{96000.0f};
  nexrx::RateAdaptiveBuffer<float> audioBuffer;
  std::atomic<float> audioVolume{0.5f};
  
  bool audioDecimateSkip = false;
  std::mutex commandMutex;
  std::deque<std::function<void()>> commandQueue;
  std::thread commandThread;
  std::atomic<bool> commandThreadRunning{false};
  std::atomic<float> signalLevelRMS{0.0f};
  
  int windowWidth = 0, windowHeight = 0;
  bool running = false, luaConfigLoaded = false;
  uint32_t lastFrameTime = 0;
  double lastVFOHz = 14200000.0;
  InputState input;
};

int main(int argc, char* argv[]) {
#ifdef HAVE_SSE_DENORMAL_CONTROL
  _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
  _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
  bool dv = false;
  for (int i=1; i<argc; ++i) {
    if (std::string(argv[i]) == "--no-vsync" || std::string(argv[i]) == "-n") {
      dv = true;
    }
  }
  App app;
  gApp = &app;
  if (!app.init("NexRx", !dv)) {
    return 1;
  }
  app.run();
  return 0;
}
