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
    bool mouseDown[3] = {false, false, false}, mouseClicked[3] = {false, false, false}, mouseReleased[3] = {false, false, false};
    bool keyDown[512] = {false}, keyPressed[512] = {false};
    bool shiftDown = false, ctrlDown = false, altDown = false;
    std::string textInput;
    void beginFrame() {
        for (int i = 0; i < 3; ++i) { mouseClicked[i] = false; mouseReleased[i] = false; }
        for (int i = 0; i < 512; ++i) keyPressed[i] = false;
        mouseWheel = 0; textInput.clear();
    }
};

class App {
public:
    App() = default;
    ~App() { shutdown(); }

    bool init(const std::string& title, bool vsyncEnabled = true) {
        if (!initLuaConfig()) return false;
        int w = 1280, h = 850; float fS = 16.0f;
        sol::function getNum = lua_["setbox"]["getNumber"];
        if (getNum.valid()) { w = (int)getNum("windowWidth", 1280).get<double>(); h = (int)getNum("windowHeight", 850).get<double>(); fS = (float)getNum("fontSize", 16.0).get<double>(); }
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) return false;
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        window_ = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
        if (!window_) return false;
        glContext_ = SDL_GL_CreateContext(window_);
        SDL_GL_SetSwapInterval(vsyncEnabled ? 1 : 0);
        SDL_GetWindowSize(window_, &windowWidth_, &windowHeight_);
        glViewport(0, 0, windowWidth_, windowHeight_); glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        sol::object fP = lua_["setbox"]["get"]("fontPaths");
        if (fP.is<sol::table>()) { for (auto& kv : fP.as<sol::table>()) { if (kv.second.is<std::string>() && font_.loadFont(kv.second.as<std::string>().c_str(), fS)) break; } }
        if (!audio_.init(48000, 2)) return false;
        nexrx::BufferConfig aC; aC.capacity = 32768; aC.targetFillRatio = 0.5f; aC.enableAdaptation = false; audioBuffer_.configure(aC);
        demod_.setSampleRate(96000.0f); demod_.setMode(Demodulator::Mode::USB); demod_.setBfoOffset(700.0f);
        audio_.setCallback([this](float* out, uint32_t fC, uint32_t ch) {
            constexpr float g = 1000000.0f; const float vol = audioVolume_.load();
            thread_local std::vector<float> tmp; tmp.resize(fC); audioBuffer_.read(std::span<float>(tmp.data(), fC));
            for (uint32_t i = 0; i < fC; ++i) { float s = std::tanh(tmp[i] * g * vol); out[i*ch] = s; if (ch > 1) out[i*ch+1] = s; }
        });
        startCommandThread();
        if (!initLua()) return false;
        running_ = true; return true;
    }

    void shutdown() {
        stopCommandThread();
        if (twinConnected_.load()) { twinHost_.stopReceiving(); twinHost_.shutdown(); twinConnected_.store(false); }
        audio_.shutdown();
        if (glContext_) SDL_GL_DeleteContext(glContext_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    void startCommandThread() {
        commandThreadRunning_ = true;
        commandThread_ = std::thread([this]() {
            while (commandThreadRunning_) {
                std::function<void()> cmd;
                { std::lock_guard<std::mutex> l(commandMutex_); if (!commandQueue_.empty()) { cmd = commandQueue_.front(); commandQueue_.pop_front(); } }
                if (cmd) cmd();
                else std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    void stopCommandThread() { commandThreadRunning_ = false; if (commandThread_.joinable()) commandThread_.join(); }
    void postCommand(std::function<void()> cmd) { std::lock_guard<std::mutex> l(commandMutex_); commandQueue_.push_back(cmd); }

    void run() {
        while (running_) {
            uint32_t fS = SDL_GetTicks();
            input_.beginFrame(); pollEvents();
            uint32_t now = SDL_GetTicks(); float dt = (now - lastFrameTime_) / 1000.0f; lastFrameTime_ = now;
            callLuaUpdate(dt);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); setup2DProjection(); callLuaDraw();
            SDL_GL_SwapWindow(window_);
            uint32_t elapsed = SDL_GetTicks() - fS;
            if (elapsed < 16) SDL_Delay(16 - elapsed);
        }
    }

    void drawRect(float x, float y, float w, float h, float r, float g, float b, float a) { glColor4f(r, g, b, a); glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd(); }
    void drawRectOutline(float x, float y, float w, float h, float r, float g, float b, float a, float t) { glColor4f(r, g, b, a); glLineWidth(t); glBegin(GL_LINE_LOOP); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd(); }
    void drawCircle(float cx, float cy, float rad, float r, float g, float b, float a, int seg = 32) { glColor4f(r, g, b, a); glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy); for (int i = 0; i <= seg; ++i) { float ang = 2.0f * 3.14159265f * i / seg; glVertex2f(cx + cosf(ang)*rad, cy + sinf(ang)*rad); } glEnd(); }
    void drawCircleOutline(float cx, float cy, float rad, float r, float g, float b, float a, float t, int seg = 32) { glColor4f(r, g, b, a); glLineWidth(t); glBegin(GL_LINE_LOOP); for (int i = 0; i < seg; ++i) { float ang = 2.0f * 3.14159265f * i / seg; glVertex2f(cx + cosf(ang)*rad, cy + sinf(ang)*rad); } glEnd(); }
    void setClearColor(float r, float g, float b) { glClearColor(r, g, b, 1.0f); }
    float drawText(float x, float y, const std::string& t, float r, float g, float b, float a) { return font_.drawText(x, y, t, r, g, b, a); }
    float measureText(const std::string& t) { return font_.measureText(t); }
    float getLineHeight() { return font_.lineHeight(); }
    void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a, float t) { glColor4f(r, g, b, a); glLineWidth(t); glBegin(GL_LINES); glVertex2f(x1, y1); glVertex2f(x2, y2); glEnd(); }
    void drawRoundedRect(float x, float y, float w, float h, float rad, float r, float g, float b, float a) {
        glColor4f(r, g, b, a); rad = std::min(rad, std::min(w, h) / 2.0f); if (rad < 0.5f) { drawRect(x, y, w, h, r, g, b, a); return; }
        const int seg = 8; constexpr float PI = 3.14159265f; glBegin(GL_TRIANGLE_FAN); glVertex2f(x + w/2, y + h/2); glVertex2f(x + rad, y); glVertex2f(x + w - rad, y);
        for (int i = 0; i <= seg; ++i) { float ang = -PI/2.0f + (PI/2.0f) * i / seg; glVertex2f(x + w - rad + cosf(ang)*rad, y + rad + std::sin(ang) * rad); }
        glVertex2f(x + w, y + h - rad); for (int i = 0; i <= seg; ++i) { float ang = 0.0f + (PI/2.0f) * i / seg; glVertex2f(x + w - rad + cosf(ang)*rad, y + h - rad + std::sin(ang) * rad); }
        glVertex2f(x + rad, y + h); for (int i = 0; i <= seg; ++i) { float ang = PI/2.0f + (PI/2.0f) * i / seg; glVertex2f(x + rad + std::cos(ang) * rad, y + h - rad + std::sin(ang) * rad); }
        glVertex2f(x, y + rad); for (int i = 0; i <= seg; ++i) { float ang = PI + (PI/2.0f) * i / seg; glVertex2f(x + rad + std::cos(ang) * rad, y + rad + std::sin(ang) * rad); }
        glVertex2f(x + rad, y); glEnd();
    }

private:
    bool initLuaConfig() {
        lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::io, sol::lib::os);
        std::string p = lua_["package"]["path"]; p += ";lua/?.lua;lua/?/init.lua"; lua_["package"]["path"] = p;
        try { lua_.safe_script_file("lua/setbox.lua", sol::script_pass_on_error); } catch (...) { return false; }
#ifdef _WIN32
        lua_["setbox"]["addTag"]("platform.Windows");
#elif __APPLE__
        lua_["setbox"]["addTag"]("platform.macOS");
#else
        lua_["setbox"]["addTag"]("platform.Linux");
#endif
        sol::function loadF = lua_["setbox"]["loadFile"]; if (loadF.valid()) { loadF("config/default.lua"); loadF("config/settings.lua"); }
        luaConfigLoaded_ = true; return true;
    }

    bool initLua() {
        if (!luaConfigLoaded_) { lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::io, sol::lib::os); }
        lua_.set_function("quit", [this]() { running_ = false; });
        lua_.set_function("getWindowSize", [this]() { return std::make_tuple(windowWidth_, windowHeight_); });
        lua_.set_function("getMousePos", [this]() { return std::make_tuple(input_.mouseX, input_.mouseY); });
        lua_.set_function("isMouseDown", [this](int b) { if (b >= 0 && b < 3) return input_.mouseDown[b]; return false; });
        lua_.set_function("isMouseClicked", [this](int b) { if (b >= 0 && b < 3) return input_.mouseClicked[b]; return false; });
        lua_.set_function("isMouseReleased", [this](int b) { if (b >= 0 && b < 3) return input_.mouseReleased[b]; return false; });
        lua_.set_function("isKeyDown", [this](int k) { if (k >= 0 && k < 512) return input_.keyDown[k]; return false; });
        lua_.set_function("getMouseWheel", [this]() { return input_.mouseWheel; });
        lua_.set_function("isShiftDown", [this]() { return input_.shiftDown; });
        lua_.set_function("isCtrlDown", [this]() { return input_.ctrlDown; });
        lua_.set_function("isAltDown", [this]() { return input_.altDown; });
        lua_.set_function("getTextInput", [this]() { return input_.textInput; });
        lua_.set_function("drawRect", [this](float x, float y, float w, float h, float r, float g, float b, float a) { drawRect(x, y, w, h, r, g, b, a); });
        lua_.set_function("drawRectOutline", [this](float x, float y, float w, float h, float r, float g, float b, float a, float t) { drawRectOutline(x, y, w, h, r, g, b, a, t); });
        lua_.set_function("drawCircle", [this](float cx, float cy, float rad, float r, float g, float b, float a) { drawCircle(cx, cy, rad, r, g, b, a); });
        lua_.set_function("drawCircleOutline", [this](float cx, float cy, float rad, float r, float g, float b, float a, float t) { drawCircleOutline(cx, cy, rad, r, g, b, a, t); });
        lua_.set_function("setClearColor", [this](float r, float g, float b) { setClearColor(r, g, b); });
        lua_.set_function("drawText", [this](float x, float y, const std::string& t, float r, float g, float b, float a) { return drawText(x, y, t, r, g, b, a); });
        lua_.set_function("measureText", [this](const std::string& t) { return measureText(t); });
        lua_.set_function("getLineHeight", [this]() { return getLineHeight(); });
        lua_.set_function("drawLine", [this](float x1, float y1, float x2, float y2, float r, float g, float b, float a, float t) { drawLine(x1, y1, x2, y2, r, g, b, a, t); });
        lua_.set_function("drawRoundedRect", [this](float x, float y, float w, float h, float rad, float r, float g, float b, float a) { drawRoundedRect(x, y, w, h, rad, r, g, b, a); });

        lua_["audio"] = lua_.create_table();
        lua_["audio"]["start"] = [this]() { return audio_.start(); };
        lua_["audio"]["stop"] = [this]() { audio_.stop(); };
        lua_["audio"]["setVolume"] = [this](float v) { std::cout << "[CPP] Setting audio linear gain to " << std::fixed << std::setprecision(4) << v << std::endl; audioVolume_.store(v, std::memory_order_relaxed); };
        lua_["audio"]["isInitialized"] = [this]() { return audio_.isInitialized(); };

        lua_["waterfall"] = lua_.create_table();
        lua_["waterfall"]["init"] = [this](int w, int h) { return waterfall_.init(w, h); };
        lua_["waterfall"]["addRow"] = [this](sol::table d) { std::vector<float> row; row.reserve(d.size()); for (size_t i = 1; i <= d.size(); ++i) row.push_back(d[i].get_or(waterfall_.getMinDb())); waterfall_.addRow(row.data(), (int)row.size()); };
        lua_["waterfall"]["render"] = [this](float x, float y, float w, float h) { waterfall_.render(x, y, w, h); };
        lua_["waterfall"]["renderSpectrum"] = [this](sol::table d, float x, float y, float w, float h) { std::vector<float> spec; spec.reserve(d.size()); for (size_t i = 1; i <= d.size(); ++i) spec.push_back(d[i].get_or(waterfall_.getMinDb())); waterfall_.renderSpectrum(spec.data(), (int)spec.size(), x, y, w, h); };
        lua_["waterfall"]["setColormapData"] = [this](sol::table stops) { std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>> grad; for (size_t i = 1; i <= stops.size(); ++i) { sol::table s = stops[i]; grad.push_back({s[1].get_or(0.0f), (uint8_t)s[2].get_or(0), (uint8_t)s[3].get_or(0), (uint8_t)s[4].get_or(0)}); } waterfall_.setColormapData(grad); };
        lua_["waterfall"]["setRange"] = [this](float min, float max) { waterfall_.setRange(min, max); };
        lua_["waterfall"]["isInitialized"] = [this]() { return waterfall_.isInitialized(); };

        lua_["hw"] = lua_.create_table();
        lua_["hw"]["connect"] = [this](sol::optional<std::string> h, sol::optional<int> cp, sol::optional<int> sp) { nexrx::TwinConfig c; c.host = h.value_or("127.0.0.1"); c.controlPort = (uint16_t)cp.value_or(5000); c.streamPort = (uint16_t)sp.value_or(5001); if (twinHost_.initialize(c)) { twinHost_.setFrameCallback([this](const nexrx::IQFrame& f) { processIQFrame(f); }); twinHost_.startStream(); if (twinHost_.startReceiving()) { twinConnected_.store(true); return true; } } return false; };
        lua_["hw"]["disconnect"] = [this]() { 
            postCommand([this]() { twinHost_.stopReceiving(); twinHost_.shutdown(); }); 
            twinConnected_.store(false); 
        };
        lua_["hw"]["isConnected"] = [this]() { return twinConnected_.load() && twinHost_.isConnected(); };
        lua_["hw"]["getSpectrum"] = [this](sol::this_state s) { computeSpectrum(); sol::state_view lua(s); sol::table res = lua.create_table(); std::lock_guard<std::mutex> l(spectrumMutex_); for (size_t i = 0; i < spectrumData_.size(); ++i) res[i + 1] = spectrumData_[i]; return res; };
        lua_["hw"]["startStream"] = [this]() { if (twinConnected_.load()) { audioBuffer_.clear(); return twinHost_.startStream(); } return false; };
        lua_["hw"]["stopStream"] = [this]() { if (twinConnected_.load()) return twinHost_.stopStream(); return false; };
        lua_["hw"]["setQsdVfo"] = [this](int i, double f) { if (twinConnected_.load()) { std::cout << "[CPP] HW: SET_QSD_VFO " << i << " " << std::fixed << std::setprecision(0) << f << std::endl; postCommand([this, i, f]() { twinHost_.setQsdVfo(i, f); }); } };
        lua_["hw"]["setAttenuation"] = [this](double db) { if (twinConnected_.load()) { std::cout << "[CPP] HW: SET_ATTEN " << db << std::endl; postCommand([this, db]() { twinHost_.setAtten((int)db, true); }); } attenDb_ = db; };
        lua_["hw"]["setPreselectorCap"] = [this](int i, bool en) { if (twinConnected_.load()) { std::cout << "[CPP] HW: SET_PRESEL_C " << i << " " << en << std::endl; postCommand([this, i, en]() { twinHost_.setPreselectorCap(i, en); }); } };
        lua_["hw"]["setPreselectorInd"] = [this](bool en) { if (twinConnected_.load()) { std::cout << "[CPP] HW: SET_PRESEL_L " << en << std::endl; postCommand([this, en]() { twinHost_.setPreselectorInd(0, en); }); } };
        lua_["hw"]["setIsgEnable"] = [this](bool en) { if (twinConnected_.load()) { std::cout << "[CPP] HW: SET_ISG_ENABLE " << en << std::endl; postCommand([this, en]() { twinHost_.setIsgEnable(en); }); } };
        lua_["hw"]["setIsgFreq"] = [this](double f) { if (twinConnected_.load()) { std::cout << "[CPP] HW: SET_ISG_FREQ " << std::fixed << std::setprecision(0) << f << std::endl; postCommand([this, f]() { twinHost_.setIsgFreq(f); }); } };
        lua_["hw"]["setCodecConfig"] = [this](int rate, double gain) { if (twinConnected_.load()) { std::vector<int> chMap = {0, 1, 2, 3, 4, 5, 6, 7}; postCommand([this, rate, chMap, gain]() { twinHost_.setCodecConfig(rate, chMap, gain, 0); }); } };
        lua_["hw"]["setQsdOffset"] = [this](double kHz) { qsdOffsetKhz_ = kHz; };

        lua_["rx"] = lua_.create_table();
        lua_["rx"]["setModeId"] = [this](int id) { if (id >= 0 && id <= 3) demod_.setMode((Demodulator::Mode)id); };
        lua_["rx"]["setBfo"] = [this](float hz) { demod_.setBfoOffset(hz); };
        lua_["rx"]["setBandpassEnabled"] = [this](bool en) { basebandFilter_.setBandpassEnabled(en); };
        lua_["rx"]["setBandpassCenter"] = [this](float hz) { basebandFilter_.setBandpassCenter(hz); };
        lua_["rx"]["setBandpassWidth"] = [this](float hz) { basebandFilter_.setBandpassWidth(hz); };
        lua_["rx"]["setNotchEnabled"] = [this](bool en) { basebandFilter_.setNotchEnabled(en); };
        lua_["rx"]["setNotchCenter"] = [this](float hz) { basebandFilter_.setNotchCenter(hz); };
        lua_["rx"]["setNotchWidth"] = [this](float hz) { basebandFilter_.setNotchWidth(hz); };
        lua_["rx"]["setLmsMu"] = [this](float mu) { lmsMu_ = std::clamp(mu, 0.0001f, 1.0f); };
        lua_["rx"]["setMute"] = [this](bool en) { audio_.setMuted(en); };
        lua_["rx"]["setVfo"] = [this](double f) { 
            std::cout << "[CPP] Tuning VFO to " << std::fixed << std::setprecision(0) << f << " Hz" << std::endl; 
            lastVfoHz_ = f; 
            
            // Reset phase rotators and LMS
            shiftCos0_ = 1.0f; shiftSin0_ = 0.0f;
            shiftCos1_ = 1.0f; shiftSin1_ = 0.0f;
            lmsW0_r_ = 0.0f; lmsW0_i_ = 0.0f;
            lmsW1_r_ = 0.0f; lmsW1_i_ = 0.0f;

            if (twinConnected_.load()) { 
                double k = qsdOffsetKhz_ * 1000.0; 
                postCommand([this, f, k]() { 
                    twinHost_.setQsdVfo(0, f - k); 
                    twinHost_.setQsdVfo(1, f + k); 
                    twinHost_.setQsdVfo(2, f); 
                }); 
            } 
        };
        lua_["rx"]["getSignalRms"] = [this]() { return signalLevelRms_.load(); };

        try { auto res = lua_.safe_script_file("lua/main.lua", sol::script_pass_on_error); if (!res.valid()) { sol::error err = res; std::cerr << "Lua load error: " << err.what() << std::endl; return false; } } catch (...) { return false; }
        sol::function initFn = lua_["init"]; if (initFn.valid()) { auto res = initFn(); if (!res.valid()) { sol::error err = res; std::cerr << "init() error: " << err.what() << std::endl; } }
        return true;
    }

    void pollEvents() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT: running_ = false; break;
                case SDL_WINDOWEVENT: if (e.window.event == SDL_WINDOWEVENT_RESIZED) { windowWidth_ = e.window.data1; windowHeight_ = e.window.data2; glViewport(0, 0, windowWidth_, windowHeight_); } break;
                case SDL_MOUSEMOTION: input_.mouseX = e.motion.x; input_.mouseY = e.motion.y; break;
                case SDL_MOUSEBUTTONDOWN: if (e.button.button <= 3) { int idx = e.button.button - 1; input_.mouseDown[idx] = true; input_.mouseClicked[idx] = true; } break;
                case SDL_MOUSEBUTTONUP: if (e.button.button <= 3) { int idx = e.button.button - 1; input_.mouseDown[idx] = false; input_.mouseReleased[idx] = true; } break;
                case SDL_MOUSEWHEEL: input_.mouseWheel = e.wheel.y; break;
                case SDL_KEYDOWN: if (e.key.keysym.scancode < 512) { input_.keyDown[e.key.keysym.scancode] = true; if (!e.key.repeat) input_.keyPressed[e.key.keysym.scancode] = true; } input_.shiftDown = (e.key.keysym.mod & KMOD_SHIFT) != 0; input_.ctrlDown = (e.key.keysym.mod & KMOD_CTRL) != 0; input_.altDown = (e.key.keysym.mod & KMOD_ALT) != 0; break;
                case SDL_KEYUP: if (e.key.keysym.scancode < 512) input_.keyDown[e.key.keysym.scancode] = false; input_.shiftDown = (e.key.keysym.mod & KMOD_SHIFT) != 0; input_.ctrlDown = (e.key.keysym.mod & KMOD_CTRL) != 0; input_.altDown = (e.key.keysym.mod & KMOD_ALT) != 0; break;
                case SDL_TEXTINPUT: input_.textInput += e.text.text; break;
            }
        }
    }

    void setup2DProjection() { glViewport(0, 0, windowWidth_, windowHeight_); glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1); glMatrixMode(GL_MODELVIEW); glLoadIdentity(); }
    void callLuaUpdate(float dt) { sol::function f = lua_["update"]; if (f.valid()) { auto res = f(dt); if (!res.valid()) { sol::error err = res; std::cerr << "update() error: " << err.what() << std::endl; } } }
    void callLuaDraw() { sol::function f = lua_["draw"]; if (f.valid()) { auto res = f(); if (!res.valid()) { sol::error err = res; std::cerr << "draw() error: " << err.what() << std::endl; } } }

    void processIQFrame(const nexrx::IQFrame& frame) {
        static uint64_t fCount = 0; static float s0_r = 0, s1_r = 0, s2_r = 0; static auto lP = std::chrono::steady_clock::now();
        fCount++; float i0, q0, i1, q1, i2, q2; frame.qsd[0].toFloat(i0, q0); frame.qsd[1].toFloat(i1, q1); frame.qsd[2].toFloat(i2, q2);
        
        // Calculate RMS on RAW input samples
        s0_r += i0*i0 + q0*q0; s1_r += i1*i1 + q1*q1; s2_r += i2*i2 + q2*q2;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - lP).count() >= 1.0) {
            std::cout << "[DSP] QSD RAW RMS: 0=" << std::sqrt(s0_r/fCount) << ", 1=" << std::sqrt(s1_r/fCount) << ", 2=" << std::sqrt(s2_r/fCount) << " | LMS: (" << lmsW0_r_ << "," << lmsW0_i_ << "), (" << lmsW1_r_ << "," << lmsW1_i_ << ")" << std::endl;
            fCount = 0; s0_r = 0; s1_r = 0; s2_r = 0; lP = now;
        }
        
        constexpr float sampleRate = 96000.0f; float k_hz = (float)qsdOffsetKhz_ * 1000.0f;
        if (k_hz != lastShiftK_) { float p = 2.0f * 3.14159265f * k_hz / sampleRate; shiftCosD_ = std::cos(p); shiftSinD_ = std::sin(p); lastShiftK_ = k_hz; }
        
        // Shift side QSDs to center frequency
        // float i0_s = i0 * shiftCos0_ + q0 * shiftSin0_, q0_s = q0 * shiftCos0_ - i0 * shiftSin0_;
        // float i1_s = i1 * shiftCos1_ - q1 * shiftSin1_, q1_s = q1 * shiftCos1_ + i1 * shiftSin1_;
        
        // Advance phase rotators
        float c0 = shiftCos0_ * shiftCosD_ - shiftSin0_ * shiftSinD_, s0 = shiftSin0_ * shiftCosD_ + shiftCos0_ * shiftSinD_;
        shiftCos0_ = c0; shiftSin0_ = s0;
        float c1 = shiftCos1_ * shiftCosD_ - shiftSin1_ * shiftSinD_, s1 = shiftSin1_ * shiftCosD_ + shiftCos1_ * shiftSinD_;
        shiftCos1_ = c1; shiftSin1_ = s1;
        
        static uint32_t rnC = 0; if ((++rnC & 0xFFFF) == 0) { auto rn = [](float& c, float& s) { float m = std::sqrt(c*c+s*s); if (m>0) { c/=m; s/=m; } }; rn(shiftCos0_, shiftSin0_); rn(shiftCos1_, shiftSin1_); }
        
        // PRIMARY AUDIO PATH: Use center QSD (i2) directly
        float i_f = i2, q_f = q2;
        
        // Update spectrum buffer with RAW samples (before selectivity filtering)
        if (iqBuffer_.size() < FFT_SIZE*2) { std::lock_guard<std::mutex> l(spectrumMutex_); if (iqBuffer_.size() < FFT_SIZE*2) iqBuffer_.resize(FFT_SIZE*2, 0.0f); }
        size_t pos = iqBufferWritePos_.load(std::memory_order_relaxed); iqBuffer_[pos*2] = i_f; iqBuffer_[pos*2+1] = q_f; iqBufferWritePos_.store((pos+1)%FFT_SIZE, std::memory_order_release);

        // Now apply selectivity filtering for the audio path
        basebandFilter_.process(i_f, q_f);
        
        float aOut = demod_.process(i_f, q_f);
        
        // S-meter level from Demodulator (which tracks audible bandwidth)
        signalLevelRms_.store(demod_.getSignalLevelRms(), std::memory_order_relaxed);
        
        if (!audioDecimateSkip_) { audioBuffer_.write(aOut); if (audioCaptureBuffer_.size() < 48000*5) audioCaptureBuffer_.push_back(std::tanh(aOut * 20000.0f * audioVolume_.load())); }
        audioDecimateSkip_ = !audioDecimateSkip_;
    }

    void saveAudioCapture() { if (audioCaptureBuffer_.empty()) return; std::ofstream f("/tmp/audio_capture.raw", std::ios::binary); if (f) f.write((char*)audioCaptureBuffer_.data(), audioCaptureBuffer_.size()*sizeof(float)); }

    void fftInPlace(float* re, float* im, size_t n) {
        for (size_t i=1, j=0; i<n; ++i) { size_t bit=n>>1; for (; j&bit; bit>>=1) j^=bit; j^=bit; if (i<j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); } }
        for (size_t len=2; len<=n; len<<=1) {
            float ang = -2.0f*3.14159265f/len, wRe = std::cos(ang), wIm = std::sin(ang);
            for (size_t i=0; i<n; i+=len) {
                float cR = 1.0f, cI = 0.0f;
                for (size_t j=0; j<len/2; ++j) {
                    size_t u = i+j, v = i+j+len/2;
                    float tR = cR*re[v] - cI*im[v], tI = cR*im[v] + cI*re[v];
                    re[v] = re[u]-tR; im[v] = im[u]-tI; re[u] += tR; im[u] += tI;
                    float nR = cR*wRe - cI*wIm; cI = cR*wIm + cI*wRe; cR = nR;
                }
            }
        }
    }

    void computeSpectrum() {
        static std::vector<float> win; if (win.size() != FFT_SIZE) { win.resize(FFT_SIZE); for (size_t n=0; n<FFT_SIZE; ++n) win[n] = 0.5f*(1.0f-std::cos(2.0f*3.14159265f*n/(FFT_SIZE-1))); }
        static std::vector<float> fRe(FFT_SIZE), fIm(FFT_SIZE), avgS; static bool avgI = false;
        {
            std::lock_guard<std::mutex> l(spectrumMutex_); if (iqBuffer_.size() < FFT_SIZE*2) return;
            size_t wP = iqBufferWritePos_.load(std::memory_order_acquire), rS = (wP + 8)%FFT_SIZE;
            for (size_t n=0; n<FFT_SIZE; ++n) { size_t idx = (rS+n)%FFT_SIZE; fRe[n] = iqBuffer_[idx*2]*win[n]; fIm[n] = iqBuffer_[idx*2+1]*win[n]; }
        }
        fftInPlace(fRe.data(), fIm.data(), FFT_SIZE); std::vector<float> lS(FFT_SIZE);
        for (size_t k=0; k<FFT_SIZE; ++k) {
            float mag = std::sqrt(fRe[k]*fRe[k] + fIm[k]*fIm[k])/FFT_SIZE*2.0f;
            lS[(k+FFT_SIZE/2)%FFT_SIZE] = (mag > 1e-10f) ? 20.0f*std::log10(mag) : -100.0f;
        }
        if (!avgI || avgS.size() != FFT_SIZE) { avgS = lS; avgI = true; }
        else { for (size_t k=0; k<FFT_SIZE; ++k) avgS[k] = 0.3f*lS[k] + 0.7f*avgS[k]; }
        { std::lock_guard<std::mutex> l(spectrumMutex_); spectrumData_ = avgS; }
    }

    SDL_Window* window_ = nullptr; SDL_GLContext glContext_ = nullptr; sol::state lua_; FontRenderer font_; AudioEngine audio_; WaterfallRenderer waterfall_;
    nexrx::TwinConn twinHost_; std::mutex spectrumMutex_; std::vector<float> spectrumData_; std::vector<float> iqBuffer_; std::atomic<size_t> iqBufferWritePos_{0};
    static constexpr size_t FFT_SIZE = 1024; std::atomic<bool> twinConnected_{false}; double qsdOffsetKhz_ = 12.0; double attenDb_ = 0.0;
    float shiftCos0_ = 1.0f, shiftSin0_ = 0.0f, shiftCos1_ = 1.0f, shiftSin1_ = 0.0f, shiftCosD_ = 1.0f, shiftSinD_ = 0.0f, lastShiftK_ = -1.0f;
    float lmsW0_r_ = 0.0f, lmsW0_i_ = 0.0f, lmsW1_r_ = 0.0f, lmsW1_i_ = 0.0f, lmsMu_ = 0.05f;
    Demodulator demod_; nexrx::BasebandFilter basebandFilter_{96000.0f}; nexrx::RateAdaptiveBuffer<float> audioBuffer_; std::atomic<float> audioVolume_{0.0316f};
    bool audioDecimateSkip_ = false; std::vector<float> audioCaptureBuffer_; std::mutex commandMutex_; std::deque<std::function<void()>> commandQueue_; std::thread commandThread_; std::atomic<bool> commandThreadRunning_{false};
    nexrx::DropRateTracker audioDropTracker_, iqDropTracker_; std::atomic<float> signalLevelRms_{0.0f}; float signalAccumulator_ = 0.0f; size_t signalSampleCount_ = 0; static constexpr size_t SIGNAL_AVG_SAMPLES = 4800;
    int windowWidth_ = 0, windowHeight_ = 0; bool running_ = false, vsyncEnabled_ = true, luaConfigLoaded_ = false; uint32_t lastFrameTime_ = 0; InputState input_; double lastVfoHz_ = 14200000.0;
};

int main(int argc, char* argv[]) {
#ifdef HAVE_SSE_DENORMAL_CONTROL
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
    bool dv = false; for (int i=1; i<argc; ++i) if (std::string(argv[i]) == "--no-vsync" || std::string(argv[i]) == "-n") dv = true;
    App app; gApp = &app; if (!app.init("NexRx", !dv)) return 1; app.run(); return 0;
}
