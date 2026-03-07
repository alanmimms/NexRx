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
#include <cbor.h>
#include "transport/IQFrame.hpp"
#include "Demodulator.hpp"

#include <SDL.h>
#include <SDL_opengl.h>
#include <sol/sol.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#include <mutex>
#include <atomic>
#include <queue>
#include <deque>
#include <thread>
#include <condition_variable>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace nexrx;

struct DspDiagnostics {
  std::atomic<float> signalRms{0.0f};
  std::atomic<float> maxRaw{0.0f};
  std::atomic<float> maxAudio{0.0f};
  std::atomic<float> lmsWeightR{1.0f};
  std::atomic<float> lmsWeightI{0.0f};
  std::atomic<uint64_t> framesProcessed{0};
};

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

class App {
public:
  static constexpr int FFT_SIZE = 1024;

  App() {
    iqBuffer.assign(FFT_SIZE * 2, 0.0f);
    spectrumData.assign(FFT_SIZE, -100.0f);
    lmsMu = 0.001f;
  }
  ~App() { shutdown(); }

  bool init(const std::string& title, bool vsyncEnabled = true) {
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

    // 2. SDL/GL Setup
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0) return false;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
                              1280, 850, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!window) return false;

    glContext = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(vsyncEnabled ? 1 : 0);
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    
    glViewport(0, 0, windowWidth, windowHeight);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // 3. Bindings
    lua["getWindowSize"] = [this]() { return std::make_tuple(windowWidth, windowHeight); };
    lua["getMousePos"] = [this]() { return std::make_tuple(input.mouseX, input.mouseY); };
    lua["isMouseDown"] = [this](int b) { if (b >= 0 && b < 3) return input.mouseDown[b]; return false; };
    lua["isKeyDown"] = [this](int k) { if (k >= 0 && k < 512) return input.keyDown[k]; return false; };
    lua["isShiftDown"] = [this]() { return input.shiftDown; };
    lua["isCtrlDown"] = [this]() { return input.ctrlDown; };
    lua["isAltDown"] = [this]() { return input.altDown; };
    lua["getMouseWheel"] = [this]() { return input.mouseWheel; };
    lua["isMouseClicked"] = [this](int b) { if (b >= 0 && b < 3) return input.mouseClicked[b]; return false; };
    lua["isMouseReleased"] = [this](int b) { if (b >= 0 && b < 3) return input.mouseReleased[b]; return false; };
    lua["measureText"] = [this](std::string t) { return font.measureText(t.c_str()); };
    lua["getLineHeight"] = [this]() { return font.getLineHeight(); };
    
    lua["drawRect"] = [](float x, float y, float w, float h, float r, float g, float b, float a) {
      glColor4f(r, g, b, a); glBegin(GL_QUADS);
      glVertex2f(x, y); glVertex2f(x + w, y);
      glVertex2f(x + w, y + h); glVertex2f(x, y + h);
      glEnd();
    };
    lua["drawRectOutline"] = [](float x, float y, float w, float h, float r, float g, float b, float a, float t) {
      glColor4f(r, g, b, a); glLineWidth(t);
      glBegin(GL_LINE_LOOP);
      glVertex2f(x, y); glVertex2f(x + w, y);
      glVertex2f(x + w, y + h); glVertex2f(x, y + h);
      glEnd();
    };
    lua["drawRoundedRect"] = [this](float x, float y, float w, float h, float rad, float r, float g, float b, float a) {
      glColor4f(r, g, b, a);
      rad = std::min(rad, std::min(w, h) / 2.0f);
      if (rad < 0.5f) {
        glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
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
    };
    lua["drawText"] = [this](float x, float y, std::string t, float r, float g, float b, float a) {
      return font.drawText(x, y, t, r, g, b, a);
    };
    lua["drawLine"] = [](float x1, float y1, float x2, float y2, float r, float g, float b, float a, float t) {
      glColor4f(r, g, b, a); glLineWidth(t);
      glBegin(GL_LINES); glVertex2f(x1, y1); glVertex2f(x2, y2); glEnd();
    };
    lua["drawCircle"] = [](float cx, float cy, float rad, float r, float g, float b, float a) {
      glColor4f(r, g, b, a); glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy);
      for (int i = 0; i <= 32; ++i) {
        float ang = 2.0f * 3.14159265f * i / 32;
        glVertex2f(cx + std::cos(ang)*rad, cy + std::sin(ang)*rad);
      }
      glEnd();
    };
    lua["drawCircleOutline"] = [](float cx, float cy, float rad, float r, float g, float b, float a, float t) {
      glColor4f(r, g, b, a); glLineWidth(t); glBegin(GL_LINE_LOOP);
      for (int i = 0; i < 32; ++i) {
        float ang = 2.0f * 3.14159265f * i / 32;
        glVertex2f(cx + std::cos(ang)*rad, cy + std::sin(ang)*rad);
      }
      glEnd();
    };
    lua["setClearColor"] = [](float r, float g, float b) { glClearColor(r, g, b, 1.0f); };

    // 4. Object tables
    sol::table audioTable = lua.create_table();
    audioTable["start"] = [this]() { return audio.start(); };
    audioTable["stop"] = [this]() { audio.stop(); };
    audioTable["setVolume"] = [this](float db) {
      float linear = (db <= -60.0f) ? 0.0f : std::pow(10.0f, db / 20.0f);
      audio.setVolume(linear);
    };
    audioTable["isInitialized"] = [this]() { return audio.isInitialized(); };
    audioTable["setTestTone"] = [this](bool en, float freq) { audio.setTestTone(en, freq); };
    lua["audio"] = audioTable;

    sol::table hwTable = lua.create_table();
    hwTable["connect"] = [this](std::string h, int cp, int sp) {
      TwinConfig c; c.host = h; c.controlPort = cp; c.streamPort = sp;
      if (twinHost.initialize(c)) {
        twinHost.setFrameCallback([this](const nexrx::IQFrame& f) { processIQFrame(f); });
        if (twinHost.startReceiving()) {
          twinConnected.store(true); twinHost.startStream(); return true;
        }
      }
      return false;
    };
    hwTable["disconnect"] = [this]() { if (twinConnected.load()) { postCommand([this]() { twinHost.stopReceiving(); twinHost.shutdown(); }); twinConnected.store(false); } };
    hwTable["isConnected"] = [this]() { return twinConnected.load(); };
    hwTable["getSpectrum"] = [this](sol::this_state s) { 
      computeSpectrum(); sol::state_view lView(s); sol::table res = lView.create_table(); 
      std::lock_guard<std::mutex> l(spectrumMutex); 
      for (size_t i = 0; i < spectrumData.size(); ++i) res[i + 1] = spectrumData[i]; 
      return res; 
    };
    hwTable["getState"] = [this](sol::this_state s) -> sol::object {
      if (!twinConnected.load()) return sol::make_object(s, sol::nil);
      auto stateCBOR = twinHost.getState();
      if (stateCBOR.empty()) return sol::make_object(s, sol::nil);
      CborParser parser; CborValue it;
      if (cbor_parser_init(stateCBOR.data(), stateCBOR.size(), 0, &parser, &it) != CborNoError) return sol::make_object(s, sol::nil);
      sol::state_view lView(s); sol::table res = lView.create_table();
      CborValue mapIt; cbor_value_enter_container(&it, &mapIt);
      while (!cbor_value_at_end(&mapIt)) {
        char key[64]; size_t len = sizeof(key) - 1;
        if (cbor_value_get_type(&mapIt) == CborTextStringType) {
          if (cbor_value_copy_text_string(&mapIt, key, &len, &mapIt) != CborNoError) break;
          key[len] = '\0';
        } else { cbor_value_advance(&mapIt); continue; }
        if (cbor_value_at_end(&mapIt)) break;
        if (cbor_value_is_double(&mapIt)) { double v; cbor_value_get_double(&mapIt, &v); res[key] = v; }
        else if (cbor_value_is_integer(&mapIt)) { int64_t v; cbor_value_get_int64(&mapIt, &v); res[key] = v; }
        else if (cbor_value_is_boolean(&mapIt)) { bool v; cbor_value_get_boolean(&mapIt, &v); res[key] = v; }
        cbor_value_advance(&mapIt);
      }
      return res;
    };
    hwTable["getFramesReceived"] = [this]() { return twinHost.getFramesReceived(); };
    hwTable["setVFO"] = [this](double f, double k) { if (twinConnected.load()) { postCommand([this, f, k]() { twinHost.setVFO(f, k); }); } };
    hwTable["setAttenuation"] = [this](int db) { if (twinConnected.load()) { postCommand([this, db]() { twinHost.setAtten(db); }); } };
    hwTable["setAGCMode"] = [this](int m) { if (twinConnected.load()) { postCommand([this, m]() { twinHost.setAGCMode(m); }); } };
    hwTable["setIsgFreq"] = [this](double f) { if (twinConnected.load()) { postCommand([this, f]() { twinHost.setISGFreq(f); }); } };
    hwTable["setIsgEnable"] = [this](bool en) { if (twinConnected.load()) { postCommand([this, en]() { twinHost.setISGEnable(en); }); } };
    hwTable["setPreselectorInd"] = [this](uint32_t mask) { if (twinConnected.load()) { postCommand([this, mask]() { twinHost.setPreselectorL(mask); }); } };
    hwTable["setPreselectorCap"] = [this](uint32_t mask) { if (twinConnected.load()) { postCommand([this, mask]() { twinHost.setPreselectorCap(mask); }); } };
    hwTable["setPreselectorEnabled"] = [this](bool en) { if (twinConnected.load()) { postCommand([this, en]() { twinHost.setPreselectorEnabled(en); }); } };
    hwTable["setRfGain"] = [this](double db) { rfGainDB.store((float)db); };
    hwTable["setQsdOffset"] = [this](double k) { qsdOffsetKhz = k; if (twinConnected.load()) { postCommand([this, k]() { twinHost.setVFO(lastVFOHz, k * 1000.0); }); } };
    lua["hw"] = hwTable;

    sol::table rxTable = lua.create_table();
    rxTable["setModeId"] = [this](int id) { if (id >= 0 && id <= 3) demod.setMode((Demodulator::Mode)id); };
    rxTable["setBandpassEnabled"] = [this](bool en) { basebandFilter.setBandpassEnabled(en); };
    rxTable["setBandpassCenter"] = [this](float hz) { basebandFilter.setBandpassCenter(hz); };
    rxTable["setBandpassWidth"] = [this](float hz) { basebandFilter.setBandpassWidth(hz); };
    rxTable["setNotchEnabled"] = [this](bool en) { basebandFilter.setNotchEnabled(en); };
    rxTable["setNotchCenter"] = [this](float hz) { basebandFilter.setNotchCenter(hz); };
    rxTable["setNotchWidth"] = [this](float hz) { basebandFilter.setNotchWidth(hz); };
    rxTable["setAgcEnabled"] = [](bool en) {}; rxTable["setNrEnabled"] = [](bool en) {}; rxTable["setNbEnabled"] = [](bool en) {};
    rxTable["setMute"] = [this](bool en) { audio.setMuted(en); };
    rxTable["setDemodFilterEnabled"] = [this](bool en) { demod.setFilterEnabled(en); };
    rxTable["setLmsMu"] = [this](float mu) { lmsMu = std::clamp(mu, 0.0001f, 1.0f); };
    rxTable["setVfo"] = [this](double f) {
      lastVFOHz = f; shiftCos0 = 1.0f; shiftSin0 = 0.0f; shiftCos1 = 1.0f; shiftSin1 = 0.0f;
      lmsW0_r = 1.0f; lmsW0_i = 0.0f; sampleBlockCounter = 0; lmsAcc_r = 0.0f; lmsAcc_i = 0.0f;
      if (twinConnected.load()) { postCommand([this, f]() { twinHost.setVFO(f, qsdOffsetKhz * 1000.0); }); }
    };
    rxTable["getSignalRms"] = [this]() { return dspDiag.signalRms.load(); };
    lua["rx"] = rxTable;

    sol::table waterfallTable = lua.create_table();
    waterfallTable["init"] = [this](int b, int r) { return waterfall.init(b, r); };
    waterfallTable["setRange"] = [this](float min, float max) { waterfall.setRange(min, max); };
    waterfallTable["isInitialized"] = [this]() { return waterfall.isInitialized(); };
    waterfallTable["addRow"] = [this](sol::object obj) {
      if (!obj.is<sol::table>()) return;
      sol::table t = obj.as<sol::table>();
      std::vector<float> data; data.reserve(t.size());
      for (size_t i = 1; i <= t.size(); ++i) { sol::object item = t[i]; data.push_back(item.is<float>() ? item.as<float>() : -100.0f); }
      waterfall.addRow(data.data(), (int)data.size());
    };
    waterfallTable["renderSpectrum"] = [this](sol::object obj, float x, float y, float w, float h) {
      if (!obj.is<sol::table>()) return;
      sol::table t = obj.as<sol::table>();
      std::vector<float> data; data.reserve(t.size());
      for (size_t i = 1; i <= t.size(); ++i) { sol::object item = t[i]; data.push_back(item.is<float>() ? item.as<float>() : -100.0f); }
      waterfall.renderSpectrum(data.data(), (int)data.size(), x, y, w, h);
    };
    waterfallTable["render"] = [this](float x, float y, float w, float h) { waterfall.render(x, y, w, h); };
    waterfallTable["setColormapData"] = [this](sol::object obj) {
      if (!obj.is<sol::table>()) return;
      sol::table t = obj.as<sol::table>();
      std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>> grad;
      for (size_t i = 1; i <= t.size(); ++i) {
        sol::table s = t[i]; grad.push_back({s[1].get_or(0.0f), (uint8_t)s[2].get_or(0), (uint8_t)s[3].get_or(0), (uint8_t)s[4].get_or(0)});
      }
      waterfall.setColormapData(grad);
    };
    lua["waterfall"] = waterfallTable;

    sol::table dispatchTable = lua.create_table();
    dispatchTable["enableHardware"] = [this]() { twinConnected.store(true); };
    dispatchTable["updateWaterfall"] = [this](sol::object obj) {
      if (!obj.is<sol::table>()) return;
      sol::table t = obj.as<sol::table>();
      std::vector<float> data; data.reserve(t.size());
      for (size_t i = 1; i <= t.size(); ++i) { sol::object item = t[i]; data.push_back(item.is<float>() ? item.as<float>() : -100.0f); }
      waterfall.addRow(data.data(), (int)data.size());
    };
    dispatchTable["renderSpectrum"] = [this](sol::object obj, float x, float y, float w, float h) {
      if (!obj.is<sol::table>()) return;
      sol::table t = obj.as<sol::table>();
      std::vector<float> data; data.reserve(t.size());
      for (size_t i = 1; i <= t.size(); ++i) { sol::object item = t[i]; data.push_back(item.is<float>() ? item.as<float>() : -100.0f); }
      waterfall.renderSpectrum(data.data(), (int)data.size(), x, y, w, h);
    };
    dispatchTable["renderWaterfall"] = [this](float x, float y, float w, float h) { waterfall.render(x, y, w, h); };
    dispatchTable["setRxActive"] = [this](bool active) {
      if (active) { audioBuffer.clear(); if (twinConnected.load()) twinHost.startStream(); }
      else { if (twinConnected.load()) twinHost.stopStream(); }
    };
    lua["dispatch"] = dispatchTable;

    // 5. Load Main Lua
    try {
      lua.safe_script_file("lua/Main.lua");
    } catch (sol::error& e) { std::cerr << "Lua main load error: " << e.what() << std::endl; return false; }

    // 6. Font Initialization (must be after SetBox populated by main.lua)
    float fontSize = 16.0f;
    sol::function getNumF = lua["setbox"]["getNumber"];
    if (getNumF.valid()) fontSize = (float)getNumF("fontSize", 16.0).get<double>();

    sol::object fP = lua["setbox"]["get"]("fontPaths");
    if (fP.is<sol::table>()) {
      for (auto& kv : fP.as<sol::table>()) {
        if (kv.second.is<std::string>() && font.loadFont(kv.second.as<std::string>().c_str(), fontSize)) break;
      }
    }
    
    if (!font.isLoaded()) std::cerr << "WARNING: No font loaded!" << std::endl;

    if (!audio.init(48000, 2)) return false;
    iqBuffer.assign(FFT_SIZE * 2, 0.0f);
    audioBuffer.configure(BufferConfig{32768});
    demod.setSampleRate(96000.0f);

    audio.setCallback([this](float* out, uint32_t fC, uint32_t ch) {
      thread_local std::vector<float> tmp;
      tmp.resize(fC);
      size_t read = audioBuffer.read(std::span<float>(tmp.data(), fC));
      for (uint32_t i = 0; i < fC; ++i) {
        float s = tmp[i];
        out[i*ch] = s;
        if (ch > 1) out[i*ch+1] = s;
      }
    });

    if (!waterfall.init(FFT_SIZE, 256)) return false;

    // 7. Lua init()
    sol::protected_function initFn = lua["init"];
    if (initFn.valid()) {
      auto res = initFn();
      if (!res.valid()) { sol::error err = res; std::cerr << "Lua init() error: " << err.what() << std::endl; return false; }
    }

    startCommandThread();
    running = true;
    return true;
  }

  void run() {
    uint32_t lastTime = SDL_GetTicks();
    while (running) {
      uint32_t currentTime = SDL_GetTicks();
      float dt = (currentTime - lastTime) / 1000.0f; lastTime = currentTime;
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) running = false;
        else if (event.type == SDL_WINDOWEVENT) {
          if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            windowWidth = event.window.data1;
            windowHeight = event.window.data2;
            glViewport(0, 0, windowWidth, windowHeight);
          }
        }
        else if (event.type == SDL_MOUSEMOTION) { input.mouseX = event.motion.x; input.mouseY = event.motion.y; }
        else if (event.type == SDL_MOUSEWHEEL) input.mouseWheel += event.wheel.y;
        else if (event.type == SDL_MOUSEBUTTONDOWN) { if (event.button.button <= 3) { input.mouseDown[event.button.button-1] = true; input.mouseClicked[event.button.button-1] = true; } }
        else if (event.type == SDL_MOUSEBUTTONUP) { if (event.button.button <= 3) { input.mouseDown[event.button.button-1] = false; input.mouseReleased[event.button.button-1] = true; } }
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) { if (event.key.keysym.scancode < 512) input.keyDown[event.key.keysym.scancode] = (event.type == SDL_KEYDOWN); }
      }
      SDL_Keymod mod = SDL_GetModState(); input.shiftDown = (mod & KMOD_SHIFT) != 0; input.ctrlDown = (mod & KMOD_CTRL) != 0; input.altDown = (mod & KMOD_ALT) != 0;

      sol::protected_function updateFn = lua["update"];
      if (updateFn.valid()) { auto res = updateFn(dt); if (!res.valid()) { sol::error err = res; std::cerr << "Lua update() error: " << err.what() << std::endl; running = false; } }

      glClear(GL_COLOR_BUFFER_BIT);
      glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
      glMatrixMode(GL_MODELVIEW); glLoadIdentity();

      sol::protected_function drawFn = lua["draw"];
      if (drawFn.valid()) { auto res = drawFn(); if (!res.valid()) { sol::error err = res; std::cerr << "Lua draw() error: " << err.what() << std::endl; running = false; } }
      SDL_GL_SwapWindow(window);
      processCommands();
      input.beginFrame();
    }
  }

  void shutdown() {
    running = false; stopCommandThread();
    twinHost.shutdown();
    if (glContext) SDL_GL_DeleteContext(glContext);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
  }

private:
  void startCommandThread() {
    commandThreadRunning = true;
    commandThread = std::thread([this]() {
      while (commandThreadRunning) {
        std::function<void()> cmd;
        { std::lock_guard<std::mutex> l(cmdMutex); if (!commandQueue.empty()) { cmd = commandQueue.front(); commandQueue.pop(); } }
        if (cmd) cmd(); else std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    });
  }
  void stopCommandThread() { commandThreadRunning = false; if (commandThread.joinable()) commandThread.join(); }
  void postCommand(std::function<void()> cmd) { std::lock_guard<std::mutex> l(cmdMutex); commandQueue.push(cmd); }
  void processCommands() { std::lock_guard<std::mutex> l(cmdMutex); while (!commandQueue.empty()) { commandQueue.front()(); commandQueue.pop(); } }

  void processIQFrame(const nexrx::IQFrame& frame) {
    float i0, q0, i1, q1, i2, q2;
    frame.qsd[0].toFloat(i0, q0); frame.qsd[1].toFloat(i1, q1); frame.qsd[2].toFloat(i2, q2);

    constexpr float sampleRate = 96000.0f;
    float k_hz = static_cast<float>(qsdOffsetKhz * 1000.0);
    if (k_hz != lastShiftK) {
      float phaseInc = 2.0f * 3.14159265f * k_hz / sampleRate;
      shiftCosD = std::cos(phaseInc); shiftSinD = std::sin(phaseInc);
      lastShiftK = k_hz;
    }

    float c0 = shiftCos0 * shiftCosD - shiftSin0 * shiftSinD;
    float s0 = shiftSin0 * shiftCosD + shiftCos0 * shiftSinD;
    shiftCos0 = c0; shiftSin0 = s0;

    float c1 = shiftCos1 * shiftCosD - shiftSin1 * shiftSinD;
    float s1 = shiftSin1 * shiftCosD + shiftCos1 * shiftSinD;
    shiftCos1 = c1; shiftSin1 = s1;

    float i0_s = i0 * shiftCos0 + q0 * shiftSin0;
    float q0_s = q0 * shiftCos0 - i0 * shiftSin0;
    float i1_s = i1 * shiftCos1 - q1 * shiftSin1;
    float q1_s = q1 * shiftCos1 + i1 * shiftSin1;

    if ((dspDiag.framesProcessed.load() & 0x3FFF) == 0) {
      auto renorm = [](float& c, float& s) { float mag = std::sqrt(c*c+s*s); if (mag > 0) { c /= mag; s /= mag; } };
      renorm(shiftCos0, shiftSin0); renorm(shiftCos1, shiftSin1);
    }

    float w1_i_r = lmsW0_r * i1_s - lmsW0_i * q1_s;
    float w1_i_q = lmsW0_r * q1_s + lmsW0_i * i1_s;
    float error_i = i0_s - w1_i_r;
    float error_q = q0_s - w1_i_q;

    lmsAcc_r += (error_i * i1_s + error_q * q1_s);
    lmsAcc_i += (error_q * i1_s - error_i * q1_s);

    if (++sampleBlockCounter >= 32) {
      lmsW0_r += lmsMu * lmsAcc_r; lmsW0_i += lmsMu * lmsAcc_i;
      float magSq = lmsW0_r * lmsW0_r + lmsW0_i * lmsW0_i;
      if (magSq > 4.0f) { float scale = 2.0f / std::sqrt(magSq); lmsW0_r *= scale; lmsW0_i *= scale; }
      dspDiag.lmsWeightR.store(lmsW0_r); dspDiag.lmsWeightI.store(lmsW0_i);
      lmsAcc_r = 0.0f; lmsAcc_i = 0.0f; sampleBlockCounter = 0;
    }

    float iF = 0.5f * i0_s + 0.5f * w1_i_r;
    float qF = 0.5f * q0_s + 0.5f * w1_i_q;

    float rfGain = std::pow(10.0f, rfGainDB.load() / 20.0f);
    iF *= rfGain; qF *= rfGain;
    float maxR = std::max(std::abs(iF), std::abs(qF));
    if (maxR > dspDiag.maxRaw.load()) dspDiag.maxRaw.store(maxR);

    basebandFilter.recompute(); basebandFilter.process(iF, qF);
    size_t pos = iqBufferWritePos.load(std::memory_order_relaxed); 
    iqBuffer[pos*2] = iF; iqBuffer[pos*2+1] = qF;
    iqBufferWritePos.store((pos+1)%FFT_SIZE, std::memory_order_release);

    float aOut = demod.process(iF, qF);
    dspDiag.signalRms.store(demod.getSignalLevelRMS());
    if (std::abs(aOut) > dspDiag.maxAudio.load()) dspDiag.maxAudio.store(std::abs(aOut));

    if (!audioDecimateSkip) audioBuffer.write(aOut);
    audioDecimateSkip = !audioDecimateSkip;
    dspDiag.framesProcessed++;
  }
  void fftInPlace(float* re, float* im, size_t n) {
    for (size_t i=1, j=0; i<n; ++i) {
      size_t bit=n>>1;
      for (; j&bit; bit>>=1) j^=bit;
      j^=bit;
      if (i<j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (size_t len=2; len<=n; len<<=1) {
      float ang = -2.0f*3.14159265f/len;
      float wRe = std::cos(ang), wIm = std::sin(ang);
      for (size_t i=0; i<n; i+=len) {
        float cR = 1.0f, cI = 0.0f;
        for (size_t j=0; j<len/2; ++j) {
          size_t u = i+j, v = i+j+len/2;
          float tR = cR*re[v] - cI*im[v], tI = cR*im[v] + cI*re[v];
          re[v] = re[u]-tR; im[v] = im[u]-tI;
          re[u] += tR; im[u] += tI;
          float nR = cR*wRe - cI*wIm;
          cI = cR*wIm + cI*wRe; cR = nR;
        }
      }
    }
  }

  void computeSpectrum() {
    static std::vector<float> win;
    if (win.size() != FFT_SIZE) {
      win.resize(FFT_SIZE);
      for (size_t n=0; n<FFT_SIZE; ++n) win[n] = 0.5f*(1.0f-std::cos(2.0f*3.14159265f*n/(FFT_SIZE-1)));
    }
    static std::vector<float> fRe(FFT_SIZE), fIm(FFT_SIZE), avgS;
    static bool avgI = false;
    {
      std::lock_guard<std::mutex> l(spectrumMutex);
      if (iqBuffer.size() < FFT_SIZE*2) return;
      size_t wP = iqBufferWritePos.load(std::memory_order_acquire);
      for (size_t n=0; n<FFT_SIZE; ++n) {
        size_t idx = (wP+n)%FFT_SIZE;
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
    if (!avgI || avgS.size() != FFT_SIZE) { avgS = lS; avgI = true; } 
    else { for (size_t k=0; k<FFT_SIZE; ++k) avgS[k] = 0.3f*lS[k] + 0.7f*avgS[k]; }
    { std::lock_guard<std::mutex> l(spectrumMutex); spectrumData = avgS; }
  }

  SDL_Window* window = nullptr; SDL_GLContext glContext = nullptr; sol::state lua;
  FontRenderer font; AudioEngine audio; WaterfallRenderer waterfall; nexrx::TwinConn twinHost;
  std::mutex spectrumMutex; std::vector<float> spectrumData; std::vector<float> iqBuffer;
  std::atomic<size_t> iqBufferWritePos{0};
  
  std::atomic<bool> twinConnected{false};
  std::atomic<float> rfGainDB{20.0f};
  float sidetoneFreq = 700.0f;
  double qsdOffsetKhz = 12.0; double lastVFOHz = 14.2e6;
  Demodulator demod; BasebandFilter basebandFilter{96000}; DspDiagnostics dspDiag;
  float shiftCos0 = 1.0f, shiftSin0 = 0.0f, shiftCos1 = 1.0f, shiftSin1 = 0.0f;
  float shiftCosD = 1.0f, shiftSinD = 0.0f;
  float lastShiftK = -1.0f;
  float lmsW0_r = 1.0f, lmsW0_i = 0.0f; float lmsMu = 0.01f; uint32_t sampleBlockCounter = 0;
  float lmsAcc_r = 0.0f, lmsAcc_i = 0.0f;
  bool audioDecimateSkip = false;
  RateAdaptiveBuffer<float> audioBuffer; InputState input;
  std::queue<std::function<void()>> commandQueue; std::mutex cmdMutex;
  std::thread commandThread; std::atomic<bool> commandThreadRunning{false};
  bool running = false; int windowWidth, windowHeight;
};

int main(int argc, char* argv[]) {
  App app;
  if (!app.init("NexRx SDR")) return 1;
  app.run();
  return 0;
}
