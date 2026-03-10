/**
 * @file GuiEngine.cpp
 * @brief Implementation of SDL/OpenGL/Lua GUI
 */

#include "GuiEngine.hpp"
#include <iostream>
#include <tuple>
#include <cbor.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GuiEngine::GuiEngine(DspEngine& dsp) : dsp_(dsp) {}

GuiEngine::~GuiEngine() {
  shutdown();
}

bool GuiEngine::init(const std::string& title, bool vsyncEnabled) {
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
      glBegin(GL_TRIANGLE_FAN);
      glVertex2f(x + w/2, y + h/2);
      glVertex2f(x + rad, y);
      glVertex2f(x + w - rad, y);
      for (int i = 0; i <= seg; ++i) {
        float ang = -M_PI/2.0f + (M_PI/2.0f) * i / seg;
        glVertex2f(x + w - rad + cosf(ang)*rad, y + rad + sinf(ang) * rad);
      }
      glVertex2f(x + w, y + h - rad);
      for (int i = 0; i <= seg; ++i) {
        float ang = 0.0f + (M_PI/2.0f) * i / seg;
        glVertex2f(x + w - rad + cosf(ang)*rad, y + h - rad + sinf(ang) * rad);
      }
      glVertex2f(x + rad, y + h);
      for (int i = 0; i <= seg; ++i) {
        float ang = M_PI/2.0f + (M_PI/2.0f) * i / seg;
        glVertex2f(x + rad + cosf(ang) * rad, y + h - rad + sinf(ang) * rad);
      }
      glVertex2f(x, y + rad);
      for (int i = 0; i <= seg; ++i) {
        float ang = M_PI + (M_PI/2.0f) * i / seg;
        glVertex2f(x + rad + cosf(ang) * rad, y + rad + sinf(ang) * rad);
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
        float ang = 2.0f * M_PI * i / 32;
        glVertex2f(cx + std::cos(ang)*rad, cy + std::sin(ang)*rad);
      }
      glEnd();
    };
    lua["drawCircleOutline"] = [](float cx, float cy, float rad, float r, float g, float b, float a, float t) {
      glColor4f(r, g, b, a); glLineWidth(t); glBegin(GL_LINE_LOOP);
      for (int i = 0; i < 32; ++i) {
        float ang = 2.0f * M_PI * i / 32;
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
      nexrx::TwinConfig c; c.host = h; c.controlPort = cp; c.streamPort = sp;
      if (twinHost.initialize(c)) {
        twinHost.setFrameCallback([this](const nexrx::IQFrame& f) { dsp_.processIQFrame(f); });
        if (twinHost.startReceiving()) {
          twinConnected.store(true);
          twinHost.startStream();
          return true;
        }
      }
      return false;
    };
    hwTable["disconnect"] = [this]() { if (twinConnected.load()) { postCommand([this]() { twinHost.stopReceiving(); twinHost.shutdown(); }); twinConnected.store(false); } };
    hwTable["isConnected"] = [this]() { return twinConnected.load(); };
    hwTable["getSpectrum"] = [this](sol::this_state s) { 
      dsp_.computeSpectrum();
      sol::state_view lView(s); sol::table res = lView.create_table(); 
      std::vector<float> data = dsp_.getSpectrumData();
      for (size_t i = 0; i < data.size(); ++i) res[i + 1] = data[i]; 
      return res; 
    };
    hwTable["getState"] = [this](sol::this_state s) -> sol::object {
      if (!twinConnected.load()) return sol::make_object(s, sol::nil);
      
      // Trigger background poll if enough time has passed (100ms)
      auto now = std::chrono::steady_clock::now();
      if (now - lastStatePollTime > std::chrono::milliseconds(100)) {
          lastStatePollTime = now;
          postCommand([this](){ twinHost.pollStateAsync(); });
      }

      auto stateCBOR = twinHost.getState();
      if (stateCBOR.empty()) return sol::make_object(s, sol::nil);
      
      CborParser parser; CborValue it;
      if (cbor_parser_init(stateCBOR.data(), stateCBOR.size(), 0, &parser, &it) != CborNoError) return sol::make_object(s, sol::nil);
      
      sol::state_view lView(s); sol::table res = lView.create_table();
      CborValue mapIt;
      
      // Navigate to the map payload: array[0]=status, array[1]=map
      if (cbor_value_is_array(&it)) {
          CborValue arrayIt;
          cbor_value_enter_container(&it, &arrayIt);
          cbor_value_advance(&arrayIt); // Skip status
          if (cbor_value_is_map(&arrayIt)) {
              cbor_value_enter_container(&arrayIt, &mapIt);
          } else { return sol::make_object(s, sol::nil); }
      } else if (cbor_value_is_map(&it)) {
          cbor_value_enter_container(&it, &mapIt);
      } else { return sol::make_object(s, sol::nil); }

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
    hwTable["setPreselectorCap"] = [this](uint32_t m) { if (twinConnected.load()) { postCommand([this, m]() { twinHost.setPreselectorCap(m); }); } };
    hwTable["setPreselectorAuto"] = [this](bool en) { if (twinConnected.load()) { postCommand([this, en]() { twinHost.setPreselectorAuto(en); }); } };
    hwTable["setPreselectorEnabled"] = [this](bool en) { if (twinConnected.load()) { postCommand([this, en]() { twinHost.setPreselectorEnabled(en); }); } };

    hwTable["setRFGain"] = [this](double db) {
      dsp_.setRfGain((float)db);
      if (twinConnected.load()) {
        postCommand([this, db]() { twinHost.setPGAGain((int)(db / 6.0)); }); // PGA gain is in ~6dB steps
      }
    };
    hwTable["setQSDOffset"] = [this](double k) { dsp_.setQsdOffset(k); if (twinConnected.load()) { postCommand([this, k]() { twinHost.setVFO(lastVFOHz, k * 1000.0); }); } };
    hwTable["calibrate"] = [this]() {      dsp_.startManualCalibration();
      if (twinConnected.load()) {
        postCommand([this]() {
          // Send 14.201 MHz stimulus (1kHz above LO) for 2 seconds
          twinHost.sendCalibrationStimulus(14201000.0, 2000);
        });
      }
    };
    hwTable["setCalibration"] = [this](int ch, float g, float p, float ar, float ai) { dsp_.setCalibration(ch, g, p, ar, ai); };
    lua["hw"] = hwTable;

    sol::table rxTable = lua.create_table();
    rxTable["setModeId"] = [this](int id) { if (id >= 0 && id <= 3) dsp_.getDemod().setMode((Demodulator::Mode)id); };
    rxTable["setBfoOffset"] = [this](float hz) { dsp_.getDemod().setBfoOffset(hz); };
    rxTable["setBandpassEnabled"] = [this](bool en) { dsp_.getFilter().setBandpassEnabled(en); };
    rxTable["setBandpassCenter"] = [this](float hz) { dsp_.getFilter().setBandpassCenter(hz); };
    rxTable["setBandpassWidth"] = [this](float hz) { dsp_.getFilter().setBandpassWidth(hz); };
    rxTable["setNotchEnabled"] = [this](bool en) { dsp_.getFilter().setNotchEnabled(en); };
    rxTable["setNotchCenter"] = [this](float hz) { dsp_.getFilter().setNotchCenter(hz); };
    rxTable["setNotchWidth"] = [this](float hz) { dsp_.getFilter().setNotchWidth(hz); };
    rxTable["setAgcEnabled"] = [](bool en) {}; rxTable["setNrEnabled"] = [](bool en) {}; rxTable["setNbEnabled"] = [](bool en) {};
    rxTable["setMute"] = [this](bool en) { audio.setMuted(en); };
    rxTable["setDemodFilterEnabled"] = [this](bool en) { dsp_.getDemod().setFilterEnabled(en); };
    rxTable["setLmsMu"] = [this](float mu) { dsp_.setLmsMu(mu); };
    rxTable["setLmsEnabled"] = [this](bool en) { dsp_.setLmsEnabled(en); };
    rxTable["setMatrixBypass"] = [this](bool en) { dsp_.setMatrixBypass(en); };
    rxTable["setVFO"] = [this](double f) {
      lastVFOHz = f;
      dsp_.setVfo(f);
      if (twinConnected.load()) { postCommand([this, f]() { twinHost.setVFO(f, dsp_.getQsdOffset() * 1000.0); }); }
    };
    rxTable["getSignalRms"] = [this]() { return dsp_.getDiagnostics().signalRms.load(); };
    rxTable["getStats"] = [this]() {
      auto& d = dsp_.getDiagnostics();
      sol::table t = lua.create_table();
      t["frames"] = d.framesProcessed.load();
      t["rms"] = d.signalRms.load();
      t["maxRaw"] = d.maxRaw.load();
      t["maxAudio"] = d.maxAudio.load();
      t["w0_mag"] = d.lmsWeightR.load();
      t["w1_mag"] = d.lmsWeightI.load();
      t["gain0"] = d.gainErr0.load(); t["phase0"] = d.phaseErr0.load();
      t["gain1"] = d.gainErr1.load(); t["phase1"] = d.phaseErr1.load();
      t["gain2"] = d.gainErr2.load(); t["phase2"] = d.phaseErr2.load();
      t["align0"] = d.alignPhase0.load(); t["align1"] = d.alignPhase1.load();
      return t;
    };
    rxTable["isCalibrating"] = [this]() { return dsp_.isCalibrating(); };
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
    dispatchTable["enableHardware"] = [this]() { /* managed by connect */ };
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
      if (active) { dsp_.getAudioBuffer().clear(); twinHost.startStream(); }
      else { twinHost.stopStream(); }
    };
    lua["dispatch"] = dispatchTable;

    // 5. Load Main Lua
    try {
      lua.safe_script_file("lua/Main.lua");
    } catch (sol::error& e) { std::cerr << "Lua main load error: " << e.what() << std::endl; return false; }

    // 6. Font Initialization
    float fontSize = 16.0f;
    sol::function getNumF = lua["setbox"]["getNumber"];
    if (getNumF.valid()) fontSize = (float)getNumF("fontSize").get<double>();

    sol::object fP = lua["setbox"]["get"]("fontPaths");
    if (fP.is<sol::table>()) {
      for (auto& kv : fP.as<sol::table>()) {
        if (kv.second.is<std::string>() && font.loadFont(kv.second.as<std::string>().c_str(), fontSize)) break;
      }
    }
    if (!font.isLoaded()) std::cerr << "WARNING: No font loaded!" << std::endl;

    if (!audio.init(48000, 2)) return false;
    
    // Configure audio buffer for jitter absorption without artifact-prone adaptation
    BufferConfig audioConfig;
    audioConfig.capacity = 32768; // ~680ms jitter buffer
    audioConfig.enableAdaptation = false; 
    dsp_.getAudioBuffer().configure(audioConfig);

    audio.setCallback([this](float* out, uint32_t fC, uint32_t ch) {
      thread_local std::vector<float> tmp;
      tmp.resize(fC);
      size_t read = dsp_.getAudioBuffer().read(std::span<float>(tmp.data(), fC));
      for (uint32_t i = 0; i < fC; ++i) {
        float s = (i < read) ? tmp[i] : 0.0f;
        for (uint32_t c = 0; c < ch; ++c) {
          out[i*ch + c] = s;
        }
      }
    });

    if (!waterfall.init(DspEngine::FFT_SIZE, 256)) return false;

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

void GuiEngine::run() {
    uint32_t lastTime = SDL_GetTicks();
    while (running) {
      uint32_t currentTime = SDL_GetTicks();
      float dt = (currentTime - lastTime) / 1000.0f; lastTime = currentTime;
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) running = false;
        else if (event.type == SDL_WINDOWEVENT) {
          if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            windowWidth = event.window.data1; windowHeight = event.window.data2;
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

      update(dt);
      render();
    }
}

void GuiEngine::update(float dt) {
    sol::protected_function updateFn = lua["update"];
    if (updateFn.valid()) { 
        auto res = updateFn(dt); 
        if (!res.valid()) { sol::error err = res; std::cerr << "Lua update() error: " << err.what() << std::endl; running = false; } 
    }
}

void GuiEngine::render() {
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();

    sol::protected_function drawFn = lua["draw"];
    if (drawFn.valid()) { 
        auto res = drawFn(); 
        if (!res.valid()) { sol::error err = res; std::cerr << "Lua draw() error: " << err.what() << std::endl; running = false; } 
    }
    
    SDL_GL_SwapWindow(window);
    input.beginFrame();
}

void GuiEngine::shutdown() {
    running = false; 
    stopCommandThread();
    twinHost.shutdown();
    if (glContext) SDL_GL_DeleteContext(glContext);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void GuiEngine::startCommandThread() {
    commandThreadRunning = true;
    commandThread = std::thread([this]() {
      while (commandThreadRunning) {
        std::function<void()> cmd;
        { std::lock_guard<std::mutex> l(cmdMutex); if (!commandQueue.empty()) { cmd = commandQueue.front(); commandQueue.pop(); } }
        if (cmd) cmd(); else std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    });
}

void GuiEngine::stopCommandThread() { commandThreadRunning = false; if (commandThread.joinable()) commandThread.join(); }
void GuiEngine::postCommand(std::function<void()> cmd) { std::lock_guard<std::mutex> l(cmdMutex); commandQueue.push(cmd); }
void GuiEngine::processCommands() { std::lock_guard<std::mutex> l(cmdMutex); while (!commandQueue.empty()) { commandQueue.front()(); commandQueue.pop(); } }
