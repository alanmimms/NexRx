/**
 * @file main.cpp
 * @brief NexRx Application - SDL2/OpenGL with Lua GUI
 */

#include "FontRenderer.hpp"
#include "AudioEngine.hpp"
#include "WaterfallRenderer.hpp"
#include "RateAdaptiveBuffer.hpp"
#include "BasebandFilter.hpp"

// Twin integration (TCP/UDP to digital twin or STM32)
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
#include <cstdlib>
#include <cmath>
#include <complex>
#include <mutex>
#include <algorithm>
#include <vector>

#ifdef __APPLE__
#include <unistd.h>  // for chdir()
#endif

// Denormal float handling - prevents artifacts with weak signals
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
#include <xmmintrin.h>  // _MM_SET_FLUSH_ZERO_MODE (SSE)
#include <pmmintrin.h>  // _MM_SET_DENORMALS_ZERO_MODE (SSE3)
#define HAVE_SSE_DENORMAL_CONTROL 1
#endif

// Forward declarations
class App;
static App* gApp = nullptr;

/**
 * @brief Input state passed to Lua each frame
 */
struct InputState {
    int mouseX = 0;
    int mouseY = 0;
    bool mouseDown[3] = {false, false, false};  // Left, middle, right
    bool mouseClicked[3] = {false, false, false};
    bool mouseReleased[3] = {false, false, false};
    int mouseWheel = 0;

    // Keyboard (simplified for now)
    bool keyDown[512] = {false};
    bool keyPressed[512] = {false};
    bool keyReleased[512] = {false};

    // Modifiers
    bool shiftDown = false;
    bool ctrlDown = false;
    bool altDown = false;

    // Text input buffer for current frame
    std::string textInput;

    void beginFrame() {
        for (int i = 0; i < 3; ++i) {
            mouseClicked[i] = false;
            mouseReleased[i] = false;
        }
        for (int i = 0; i < 512; ++i) {
            keyPressed[i] = false;
            keyReleased[i] = false;
        }
        mouseWheel = 0;
        textInput.clear();
    }
};

/**
 * @brief Main application class
 */
class App {
public:
    App() = default;
    ~App() { shutdown(); }

    bool init(const std::string& title, bool vsyncEnabled = true) {
        // ==========================================================================
        // Phase 1: Load configuration from Lua/SetBox BEFORE creating window
        // ==========================================================================
        if (!initLuaConfig()) {
            std::cerr << "Failed to load configuration" << std::endl;
            return false;
        }

        // Read window size from SetBox (with fallback defaults)
        int windowWidth = 1280;
        int windowHeight = 850;
        float fontSize = 16.0f;

        sol::function getNumber = lua_["setbox"]["getNumber"];
        if (getNumber.valid()) {
            windowWidth = static_cast<int>(getNumber("windowWidth", 1280).get<double>());
            windowHeight = static_cast<int>(getNumber("windowHeight", 850).get<double>());
            fontSize = static_cast<float>(getNumber("fontSize", 16.0).get<double>());
        }

        // ==========================================================================
        // Phase 2: Initialize SDL and create window with config values
        // ==========================================================================
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
            std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
            return false;
        }

        // OpenGL attributes - use compatibility profile for immediate mode
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

        // Create window with size from config
        window_ = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            windowWidth, windowHeight,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
        );

        if (!window_) {
            std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
            return false;
        }

        // Create OpenGL context
        glContext_ = SDL_GL_CreateContext(window_);
        if (!glContext_) {
            std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << std::endl;
            return false;
        }

        // VSync - can be disabled for VMs where vsync causes scheduling issues
        if (vsyncEnabled) {
            SDL_GL_SetSwapInterval(1);
            std::cout << "VSync: enabled" << std::endl;
        } else {
            SDL_GL_SetSwapInterval(0);
            std::cout << "VSync: disabled (--no-vsync)" << std::endl;
        }
        vsyncEnabled_ = vsyncEnabled;

        // Get actual window size
        SDL_GetWindowSize(window_, &windowWidth_, &windowHeight_);

        // Setup OpenGL state
        glViewport(0, 0, windowWidth_, windowHeight_);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

        // Enable blending for UI
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // ==========================================================================
        // Phase 3: Load font using paths from SetBox config
        // ==========================================================================
        sol::object fontPathsObj = lua_["setbox"]["get"]("fontPaths");
        if (fontPathsObj.is<sol::table>()) {
            sol::table fontPaths = fontPathsObj.as<sol::table>();
            for (auto& kv : fontPaths) {
                if (kv.second.is<std::string>()) {
                    std::string path = kv.second.as<std::string>();
                    if (font_.loadFont(path.c_str(), fontSize)) {
                        std::cout << "Loaded font: " << path << std::endl;
                        break;
                    }
                }
            }
        }

        if (!font_.isLoaded()) {
            std::cerr << "Warning: Could not load any font from fontPaths config" << std::endl;
        }

        // Initialize audio engine
        if (!audio_.init(48000, 2)) {
            std::cerr << "Warning: Could not initialize audio" << std::endl;
        }

        // Audio buffer for network jitter absorption
        nexrx::BufferConfig audioConfig;
        audioConfig.capacity = 32768;        // ~680ms at 48kHz (jitter buffer)
        audioConfig.targetFillRatio = 0.5f;
        audioConfig.lowThreshold = 0.10f;    
        audioConfig.highThreshold = 0.90f;   
        audioConfig.maxStretchRatio = 1.0f;  
        audioConfig.enableAdaptation = false; 
        audioBuffer_.configure(audioConfig);

        demod_.setSampleRate(96000.0f);  // Input sample rate
        demod_.setMode(Demodulator::Mode::USB);
        demod_.setBfoOffset(700.0f);

        audio_.setCallback([this](float* output, uint32_t frameCount, uint32_t channels) {
            constexpr float audioGain = 5000.0f;
            const float volume = audioVolume_.load(std::memory_order_relaxed);
            thread_local std::vector<float> tempBuffer;
            tempBuffer.resize(frameCount);
            audioBuffer_.read(std::span<float>(tempBuffer.data(), frameCount));
            for (uint32_t i = 0; i < frameCount; ++i) {
                float sample = std::tanh(tempBuffer[i] * audioGain * volume);
                output[i * channels] = sample;
                if (channels > 1) {
                    output[i * channels + 1] = sample;
                }
            }
        });

        // Initialize Lua
        if (!initLua()) {
            return false;
        }

        running_ = true;
        return true;
    }

    void shutdown() {
        saveAudioCapture();
        if (twinConnected_) {
            twinHost_.stopReceiving();
            twinHost_.shutdown();
            twinConnected_ = false;
        }
        audio_.shutdown();
        if (glContext_) SDL_GL_DeleteContext(glContext_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    void run() {
        constexpr uint32_t TARGET_FRAME_TIME_MS = 16;
        while (running_) {
            uint32_t frameStart = SDL_GetTicks();
            input_.beginFrame();
            pollEvents();
            uint32_t now = SDL_GetTicks();
            float dt = (now - lastFrameTime_) / 1000.0f;
            lastFrameTime_ = now;
            float currentTime = now / 1000.0f;
            audioDropTracker_.update(audioBuffer_.stats().dropsOverflow.load(std::memory_order_relaxed), currentTime);
            iqDropTracker_.update(twinHost_.framesDropped(), currentTime);
            callLuaUpdate(dt);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            setup2DProjection();
            callLuaDraw();
            SDL_GL_SwapWindow(window_);
            if (!vsyncEnabled_) {
                uint32_t frameTime = SDL_GetTicks() - frameStart;
                if (frameTime < TARGET_FRAME_TIME_MS) SDL_Delay(TARGET_FRAME_TIME_MS - frameTime);
            }
        }
    }

    void quit() { running_ = false; }
    int windowWidth() const { return windowWidth_; }
    int windowHeight() const { return windowHeight_; }
    const InputState& input() const { return input_; }

    void drawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
        glColor4f(r, g, b, a);
        glBegin(GL_QUADS);
        glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
        glEnd();
    }

    void drawRectOutline(float x, float y, float w, float h, float r, float g, float b, float a, float thickness) {
        glColor4f(r, g, b, a); glLineWidth(thickness);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
        glEnd();
    }

    void setClearColor(float r, float g, float b) { glClearColor(r, g, b, 1.0f); }
    float drawText(float x, float y, const std::string& text, float r, float g, float b, float a) { return font_.drawText(x, y, text, r, g, b, a); }
    float measureText(const std::string& text) { return font_.measureText(text); }
    float getLineHeight() { return font_.lineHeight(); }

    void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a, float thickness) {
        glColor4f(r, g, b, a); glLineWidth(thickness);
        glBegin(GL_LINES); glVertex2f(x1, y1); glVertex2f(x2, y2); glEnd();
    }

    void drawCircle(float cx, float cy, float radius, float r, float g, float b, float a, int segments = 32) {
        glColor4f(r, g, b, a); glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy);
        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * 3.14159265f * i / segments;
            glVertex2f(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
        }
        glEnd();
    }

    void drawCircleOutline(float cx, float cy, float radius, float r, float g, float b, float a, float thickness, int segments = 32) {
        glColor4f(r, g, b, a); glLineWidth(thickness); glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float angle = 2.0f * 3.14159265f * i / segments;
            glVertex2f(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
        }
        glEnd();
    }

    void drawRoundedRect(float x, float y, float w, float h, float radius, float r, float g, float b, float a) {
        glColor4f(r, g, b, a);
        radius = std::min(radius, std::min(w, h) / 2.0f);
        if (radius < 0.5f) { drawRect(x, y, w, h, r, g, b, a); return; }
        const int cornerSegments = 8; constexpr float PI = 3.14159265f;
        glBegin(GL_TRIANGLE_FAN); glVertex2f(x + w/2, y + h/2);
        glVertex2f(x + radius, y); glVertex2f(x + w - radius, y);
        for (int i = 0; i <= cornerSegments; ++i) {
            float angle = -PI/2.0f + (PI/2.0f) * i / cornerSegments;
            glVertex2f(x + w - radius + std::cos(angle) * radius, y + radius + std::sin(angle) * radius);
        }
        glVertex2f(x + w, y + h - radius);
        for (int i = 0; i <= cornerSegments; ++i) {
            float angle = 0.0f + (PI/2.0f) * i / cornerSegments;
            glVertex2f(x + w - radius + std::cos(angle) * radius, y + h - radius + std::sin(angle) * radius);
        }
        glVertex2f(x + radius, y + h);
        for (int i = 0; i <= cornerSegments; ++i) {
            float angle = PI/2.0f + (PI/2.0f) * i / cornerSegments;
            glVertex2f(x + radius + std::cos(angle) * radius, y + h - radius + std::sin(angle) * radius);
        }
        glVertex2f(x, y + radius);
        for (int i = 0; i <= cornerSegments; ++i) {
            float angle = PI + (PI/2.0f) * i / cornerSegments;
            glVertex2f(x + radius + std::cos(angle) * radius, y + radius + std::sin(angle) * radius);
        }
        glVertex2f(x + radius, y);
        glEnd();
    }

private:
    bool initLuaConfig() {
        lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::io, sol::lib::os);
        std::string path = lua_["package"]["path"]; path += ";lua/?.lua;lua/?/init.lua"; lua_["package"]["path"] = path;
        try { lua_.safe_script_file("lua/setbox.lua", sol::script_pass_on_error); } catch (const std::exception& e) { std::cerr << "Failed to load lua/setbox.lua: " << e.what() << std::endl; return false; }
#ifdef _WIN32
        lua_["setbox"]["addTag"]("platform.Windows");
#elif __APPLE__
        lua_["setbox"]["addTag"]("platform.macOS");
#else
        lua_["setbox"]["addTag"]("platform.Linux");
#endif
        if (!lua_["setbox"].valid() || !lua_["rule"].valid()) { std::cerr << "Error: setbox module invalid" << std::endl; return false; }
        sol::function loadFile = lua_["setbox"]["loadFile"];
        if (loadFile.valid()) {
            if (!loadFile("config/default.lua").get<bool>()) std::cerr << "Warning: Failed to load config/default.lua" << std::endl;
            loadFile("config/settings.lua");
        } else { std::cerr << "Error: setbox.loadFile not available" << std::endl; return false; }
        luaConfigLoaded_ = true; return true;
    }

    bool initLua() {
        if (!luaConfigLoaded_) {
            lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::table, sol::lib::math, sol::lib::io, sol::lib::os);
            std::string path = lua_["package"]["path"]; path += ";lua/?.lua;lua/?/init.lua"; lua_["package"]["path"] = path;
        }
        lua_.set_function("quit", [this]() { quit(); });
        lua_.set_function("getWindowSize", [this]() { return std::make_tuple(windowWidth_, windowHeight_); });
        lua_.set_function("setWindowSize", [this](int width, int height) {
            if (width > 0 && height > 0 && window_) { SDL_SetWindowSize(window_, width, height); windowWidth_ = width; windowHeight_ = height; return true; }
            return false;
        });
        lua_.set_function("getMousePos", [this]() { return std::make_tuple(input_.mouseX, input_.mouseY); });
        lua_.set_function("isMouseDown", [this](int button) { if (button >= 0 && button < 3) return input_.mouseDown[button]; return false; });
        lua_.set_function("isMouseClicked", [this](int button) { if (button >= 0 && button < 3) return input_.mouseClicked[button]; return false; });
        lua_.set_function("isMouseReleased", [this](int button) { if (button >= 0 && button < 3) return input_.mouseReleased[button]; return false; });
        lua_.set_function("isKeyDown", [this](int key) { if (key >= 0 && key < 512) return input_.keyDown[key]; return false; });
        lua_.set_function("isKeyPressed", [this](int key) { if (key >= 0 && key < 512) return input_.keyPressed[key]; return false; });
        lua_.set_function("getMouseWheel", [this]() { return input_.mouseWheel; });
        lua_.set_function("isShiftDown", [this]() { return input_.shiftDown; });
        lua_.set_function("isCtrlDown", [this]() { return input_.ctrlDown; });
        lua_.set_function("isAltDown", [this]() { return input_.altDown; });
        lua_.set_function("getTextInput", [this]() { return input_.textInput; });
        lua_.set_function("drawRect", [this](float x, float y, float w, float h, float r, float g, float b, float a) { drawRect(x, y, w, h, r, g, b, a); });
        lua_.set_function("drawRectOutline", [this](float x, float y, float w, float h, float r, float g, float b, float a, float thickness) { drawRectOutline(x, y, w, h, r, g, b, a, thickness); });
        lua_.set_function("setClearColor", [this](float r, float g, float b) { setClearColor(r, g, b); });
        lua_.set_function("drawText", [this](float x, float y, const std::string& text, float r, float g, float b, float a) { return drawText(x, y, text, r, g, b, a); });
        lua_.set_function("measureText", [this](const std::string& text) { return measureText(text); });
        lua_.set_function("getLineHeight", [this]() { return getLineHeight(); });
        lua_.set_function("drawLine", [this](float x1, float y1, float x2, float y2, float r, float g, float b, float a, float thickness) { drawLine(x1, y1, x2, y2, r, g, b, a, thickness); });
        lua_.set_function("drawCircle", [this](float cx, float cy, float radius, float r, float g, float b, float a) { drawCircle(cx, cy, radius, r, g, b, a); });
        lua_.set_function("drawCircleOutline", [this](float cx, float cy, float radius, float r, float g, float b, float a, float thickness) { drawCircleOutline(cx, cy, radius, r, g, b, a, thickness); });
        lua_.set_function("drawRoundedRect", [this](float x, float y, float w, float h, float radius, float r, float g, float b, float a) { drawRoundedRect(x, y, w, h, radius, r, g, b, a); });

        if (!luaConfigLoaded_) { try { lua_.safe_script_file("lua/setbox.lua", sol::script_pass_on_error); } catch (...) { return false; } }

        lua_["audio"] = lua_.create_table();
        lua_["audio"]["start"] = [this]() { return audio_.start(); };
        lua_["audio"]["stop"] = [this]() { audio_.stop(); };
        lua_["audio"]["isPlaying"] = [this]() { return audio_.isPlaying(); };
        lua_["audio"]["setVolume"] = [this](float v) { audioVolume_.store(v, std::memory_order_relaxed); };
        lua_["audio"]["getVolume"] = [this]() { return audioVolume_.load(std::memory_order_relaxed); };
        lua_["audio"]["setMuted"] = [this](bool m) { audio_.setMuted(m); };
        lua_["audio"]["isMuted"] = [this]() { return audio_.isMuted(); };
        lua_["audio"]["setTestTone"] = [this](bool en, sol::optional<float> f) { audio_.setTestTone(en, f.value_or(440.0f)); };
        lua_["audio"]["isTestToneEnabled"] = [this]() { return audio_.isTestToneEnabled(); };
        lua_["audio"]["getSampleRate"] = [this]() { return audio_.getSampleRate(); };
        lua_["audio"]["isInitialized"] = [this]() { return audio_.isInitialized(); };
        lua_["audio"]["startRecording"] = [this](sol::optional<std::string> p) { startWavRecording(p.value_or("")); };
        lua_["audio"]["stopRecording"] = [this]() { stopWavRecording(); };
        lua_["audio"]["isRecording"] = [this]() { return isWavRecording(); };
        lua_["audio"]["getRecordingSamples"] = [this]() { return getWavSampleCount(); };
        lua_["audio"]["getRecordingDuration"] = [this]() { return static_cast<double>(getWavSampleCount()) / 48000.0; };

        lua_["waterfall"] = lua_.create_table();
        lua_["waterfall"]["init"] = [this](int w, int h) { return waterfall_.init(w, h); };
        lua_["waterfall"]["addRow"] = [this](sol::table d) {
            std::vector<float> row; row.reserve(d.size());
            for (size_t i = 1; i <= d.size(); ++i) row.push_back(d[i].get_or(waterfall_.getMinDb()));
            waterfall_.addRow(row.data(), static_cast<int>(row.size()));
        };
        lua_["waterfall"]["render"] = [this](float x, float y, float w, float h) { waterfall_.render(x, y, w, h); };
        lua_["waterfall"]["renderSpectrum"] = [this](sol::table d, float x, float y, float w, float h) {
            std::vector<float> spec; spec.reserve(d.size());
            for (size_t i = 1; i <= d.size(); ++i) spec.push_back(d[i].get_or(waterfall_.getMinDb()));
            waterfall_.renderSpectrum(spec.data(), static_cast<int>(spec.size()), x, y, w, h);
        };
        lua_["waterfall"]["setColormapData"] = [this](sol::table stops) {
            std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>> gradient;
            for (size_t i = 1; i <= stops.size(); ++i) {
                sol::table s = stops[i];
                gradient.push_back({s[1].get_or(0.0f), (uint8_t)s[2].get_or(0), (uint8_t)s[3].get_or(0), (uint8_t)s[4].get_or(0)});
            }
            waterfall_.setColormapData(gradient);
        };
        lua_["waterfall"]["setRange"] = [this](float min, float max) { waterfall_.setRange(min, max); };
        lua_["waterfall"]["getMinDb"] = [this]() { return waterfall_.getMinDb(); };
        lua_["waterfall"]["getMaxDb"] = [this]() { return waterfall_.getMaxDb(); };
        lua_["waterfall"]["isInitialized"] = [this]() { return waterfall_.isInitialized(); };
        lua_["waterfall"]["getWidth"] = [this]() { return waterfall_.getWidth(); };
        lua_["waterfall"]["getHeight"] = [this]() { return waterfall_.getHeight(); };

        lua_["hw"] = lua_.create_table();
        lua_["hw"]["connect"] = [this](sol::optional<std::string> h, sol::optional<int> cp, sol::optional<int> sp) {
            nexrx::TwinConfig config; config.host = h.value_or("127.0.0.1");
            config.controlPort = static_cast<uint16_t>(cp.value_or(5000));
            config.streamPort = static_cast<uint16_t>(sp.value_or(5001)); config.verbose = true;
            if (twinHost_.initialize(config)) {
                twinHost_.setFrameCallback([this](const nexrx::IQFrame& frame) { processIQFrame(frame); });
                twinHost_.startStream();
                if (twinHost_.startReceiving()) { twinConnected_ = true; return true; }
            }
            return false;
        };
        lua_["hw"]["disconnect"] = [this]() { twinHost_.stopReceiving(); twinHost_.shutdown(); twinConnected_ = false; };
        lua_["hw"]["isConnected"] = [this]() { return twinConnected_ && twinHost_.isConnected(); };
        lua_["hw"]["getFramesReceived"] = [this]() { return static_cast<double>(twinHost_.framesReceived()); };
        lua_["hw"]["getFramesDropped"] = [this]() { return static_cast<double>(twinHost_.framesDropped()); };
        lua_["hw"]["getIqDropRate"] = [this]() { return static_cast<double>(iqDropTracker_.dropsPerSecond()); };
        lua_["hw"]["getSpectrum"] = [this](sol::this_state s) {
            computeSpectrum(); sol::state_view lua(s); sol::table res = lua.create_table();
            std::lock_guard<std::mutex> lock(spectrumMutex_);
            for (size_t i = 0; i < spectrumData_.size(); ++i) res[i + 1] = spectrumData_[i];
            return res;
        };
        lua_["hw"]["poll"] = [this]() { return twinHost_.pollFrames(100); };
        lua_["hw"]["startStream"] = [this]() { return twinConnected_ ? twinHost_.startStream() : false; };
        lua_["hw"]["stopStream"] = [this]() { return twinConnected_ ? twinHost_.stopStream() : false; };
        lua_["hw"]["setQsdOffset"] = [this](double k) { if (twinConnected_) { twinHost_.sendCommand("SET_QSD_OFFSET " + std::to_string(k)); qsdOffsetKhz_ = k; } };
        lua_["hw"]["getQsdOffset"] = [this]() { return qsdOffsetKhz_; };
        lua_["hw"]["setAttenuation"] = [this](double db) { if (twinConnected_) { twinHost_.sendCommand("SET_ATTEN_TOTAL " + std::to_string(db)); attenDb_ = db; } };
        lua_["hw"]["getAttenuation"] = [this]() { return attenDb_; };

        lua_["rx"] = lua_.create_table();
        lua_["rx"]["setModeId"] = [this](int id) { if (id >= 0 && id <= 3) demod_.setMode(static_cast<Demodulator::Mode>(id)); };
        lua_["rx"]["getModeId"] = [this]() { return (int)demod_.getMode(); };
        lua_["rx"]["setBfo"] = [this](float hz) { demod_.setBfoOffset(hz); };
        lua_["rx"]["getBfo"] = [this]() { return demod_.getBfoOffset(); };
        lua_["rx"]["setBandpassEnabled"] = [this](bool en) { basebandFilter_.setBandpassEnabled(en); };
        lua_["rx"]["setBandpassCenter"] = [this](float hz) { basebandFilter_.setBandpassCenter(hz); };
        lua_["rx"]["setBandpassWidth"] = [this](float hz) { basebandFilter_.setBandpassWidth(hz); };
        lua_["rx"]["setNotchEnabled"] = [this](bool en) { basebandFilter_.setNotchEnabled(en); };
        lua_["rx"]["setNotchCenter"] = [this](float hz) { basebandFilter_.setNotchCenter(hz); };
        lua_["rx"]["setNotchWidth"] = [this](float hz) { basebandFilter_.setNotchWidth(hz); };
        lua_["rx"]["recomputeFilters"] = [this]() { return basebandFilter_.recompute(); };
        lua_["rx"]["setLmsMu"] = [this](float mu) { lmsMu_ = std::clamp(mu, 0.0001f, 1.0f); };
        lua_["rx"]["getLmsMu"] = [this]() { return lmsMu_; };
        lua_["rx"]["setNrEnabled"] = [this](bool en) { (void)en; };
        lua_["rx"]["setNbEnabled"] = [this](bool en) { (void)en; };
        lua_["rx"]["setMute"] = [this](bool en) { audio_.setMuted(en); };
        lua_["rx"]["setAgcEnabled"] = [this](bool en) { (void)en; };
        lua_["rx"]["getAudioStats"] = [this]() { const auto& s = audioBuffer_.stats(); return std::make_tuple((double)s.samplesWritten.load(), (double)s.samplesRead.load(), (double)s.underruns.load(), (double)s.dropsOverflow.load(), (double)audioBuffer_.getFillRatio()); };
        lua_["rx"]["getAudioDropRate"] = [this]() { return (double)audioDropTracker_.dropsPerSecond(); };
        lua_["rx"]["getAudioBufferFill"] = [this]() { return (double)audioBuffer_.getFillRatio(); };
        lua_["rx"]["setVfo"] = [this](double f) { if (twinConnected_) twinHost_.setLO(f); };
        lua_["rx"]["getSignalRms"] = [this]() { return signalLevelRms_.load(std::memory_order_relaxed); };

        try { lua_.safe_script_file("lua/property_handlers.lua", sol::script_pass_on_error); } catch (...) {}
        sol::function loadF = lua_["setbox"]["loadFile"];
        if (loadF.valid()) {
            loadF("config/modes.lua"); loadF("config/colormaps.lua"); loadF("config/bands.lua"); loadF("config/events.lua"); loadF("config/constraints.lua");
        }
        sol::function regCB = lua_["setbox"]["onPropertyChange"]; if (regCB.valid()) regCB(lua_["onPropertyChange"]);
        try { auto res = lua_.safe_script_file("lua/main.lua", sol::script_pass_on_error); if (!res.valid()) return false; } catch (...) { return false; }
        sol::function iFn = lua_["init"]; if (iFn.valid()) try { iFn(); } catch (...) {}
        return true;
    }

    void pollEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: running_ = false; break;
                case SDL_WINDOWEVENT: if (event.window.event == SDL_WINDOWEVENT_RESIZED) { windowWidth_ = event.window.data1; windowHeight_ = event.window.data2; glViewport(0, 0, windowWidth_, windowHeight_); } break;
                case SDL_MOUSEMOTION: input_.mouseX = event.motion.x; input_.mouseY = event.motion.y; break;
                case SDL_MOUSEBUTTONDOWN: if (event.button.button <= 3) { int idx = event.button.button - 1; input_.mouseDown[idx] = true; input_.mouseClicked[idx] = true; } break;
                case SDL_MOUSEBUTTONUP: if (event.button.button <= 3) { int idx = event.button.button - 1; input_.mouseDown[idx] = false; input_.mouseReleased[idx] = true; } break;
                case SDL_MOUSEWHEEL: input_.mouseWheel = event.wheel.y; break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.scancode < 512) { input_.keyDown[event.key.keysym.scancode] = true; if (!event.key.repeat) input_.keyPressed[event.key.keysym.scancode] = true; }
                    input_.shiftDown = (event.key.keysym.mod & KMOD_SHIFT) != 0; input_.ctrlDown = (event.key.keysym.mod & KMOD_CTRL) != 0; input_.altDown = (event.key.keysym.mod & KMOD_ALT) != 0;
                    break;
                case SDL_KEYUP:
                    if (event.key.keysym.scancode < 512) { input_.keyDown[event.key.keysym.scancode] = false; input_.keyReleased[event.key.keysym.scancode] = true; }
                    input_.shiftDown = (event.key.keysym.mod & KMOD_SHIFT) != 0; input_.ctrlDown = (event.key.keysym.mod & KMOD_CTRL) != 0; input_.altDown = (event.key.keysym.mod & KMOD_ALT) != 0;
                    break;
                case SDL_TEXTINPUT: input_.textInput += event.text.text; break;
            }
        }
    }

    void setup2DProjection() { glViewport(0, 0, windowWidth_, windowHeight_); glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1); glMatrixMode(GL_MODELVIEW); glLoadIdentity(); }
    void callLuaUpdate(float dt) { sol::function f = lua_["update"]; if (f.valid()) try { f(dt); } catch (...) {} }
    void callLuaDraw() { sol::function f = lua_["draw"]; if (f.valid()) try { f(); } catch (...) {} }

    void processIQFrame(const nexrx::IQFrame& frame) {
        static uint64_t fCount = 0; static float s0_r = 0, s1_r = 0, s2_r = 0; static auto lP = std::chrono::steady_clock::now();
        fCount++; float i0, q0, i1, q1, i2, q2; frame.qsd[0].toFloat(i0, q0); frame.qsd[1].toFloat(i1, q1); frame.qsd[2].toFloat(i2, q2);
        s0_r += i0*i0 + q0*q0; s1_r += i1*i1 + q1*q1; s2_r += i2*i2 + q2*q2;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - lP).count() >= 1.0) {
            std::cout << "[DSP] QSD RMS: 0=" << std::sqrt(s0_r/fCount) << ", 1=" << std::sqrt(s1_r/fCount) << ", 2=" << std::sqrt(s2_r/fCount) << " | LMS: (" << lmsW0_r_ << "," << lmsW0_i_ << "), (" << lmsW1_r_ << "," << lmsW1_i_ << ")" << std::endl;
            fCount = 0; s0_r = 0; s1_r = 0; s2_r = 0; lP = now;
        }
        constexpr float sampleRate = 96000.0f; float k_hz = static_cast<float>(qsdOffsetKhz_ * 1000.0);
        if (k_hz != lastShiftK_) { float pInc = 2.0f * 3.14159265f * k_hz / sampleRate; shiftCosD_ = std::cos(pInc); shiftSinD_ = std::sin(pInc); lastShiftK_ = k_hz; }
        float i0_s = i0 * shiftCos0_ + q0 * shiftSin0_, q0_s = q0 * shiftCos0_ - i0 * shiftSin0_;
        float i1_s = i1 * shiftCos1_ - q1 * shiftSin1_, q1_s = q1 * shiftCos1_ + i1 * shiftSin1_;
        float c0 = shiftCos0_ * shiftCosD_ - shiftSin0_ * shiftSinD_, s0 = shiftSin0_ * shiftCosD_ + shiftCos0_ * shiftSinD_;
        shiftCos0_ = c0; shiftSin0_ = s0;
        float c1 = shiftCos1_ * shiftCosD_ - shiftSin1_ * shiftSinD_, s1 = shiftSin1_ * shiftCosD_ + shiftCos1_ * shiftSinD_;
        shiftCos1_ = c1; shiftSin1_ = s1;
        static uint32_t rnC = 0; if ((++rnC & 0xFFFF) == 0) { auto rn = [](float& c, float& s) { float m = std::sqrt(c*c+s*s); if (m>0) { c/=m; s/=m; } }; rn(shiftCos0_, shiftSin0_); rn(shiftCos1_, shiftSin1_); }
        float out_i = (lmsW0_r_ * i0_s - lmsW0_i_ * q0_s) + (lmsW1_r_ * i1_s - lmsW1_i_ * q1_s);
        float out_q = (lmsW0_r_ * q0_s + lmsW0_i_ * i0_s) + (lmsW1_r_ * q1_s + lmsW1_i_ * i1_s);
        float err_i = i2 - out_i, err_q = q2 - out_q;
        float p0 = i0_s*i0_s + q0_s*q0_s + 1e-12f, p1 = i1_s*i1_s + q1_s*q1_s + 1e-12f;
        lmsW0_r_ += (lmsMu_ * (err_i * i0_s + err_q * q0_s)) / p0; lmsW0_i_ += (lmsMu_ * (err_q * i0_s - err_i * q0_s)) / p0;
        lmsW1_r_ += (lmsMu_ * (err_i * i1_s + err_q * q1_s)) / p1; lmsW1_i_ += (lmsMu_ * (err_q * i1_s - err_i * q1_s)) / p1;
        auto clW = [](float& wr, float& wi) { float m = std::sqrt(wr*wr+wi*wi); if (m>4.0f) { wr*=4.0f/m; wi*=4.0f/m; } }; clW(lmsW0_r_, lmsW0_i_); clW(lmsW1_r_, lmsW1_i_);
        float i_f = out_i, q_f = out_q; basebandFilter_.process(i_f, q_f);
        float m_sq = i_f*i_f + q_f*q_f; signalAccumulator_ += m_sq; signalSampleCount_++;
        if (signalSampleCount_ >= SIGNAL_AVG_SAMPLES) { signalLevelRms_.store(std::sqrt(signalAccumulator_/signalSampleCount_), std::memory_order_relaxed); signalAccumulator_=0; signalSampleCount_=0; }
        if (iqBuffer_.size() < FFT_SIZE*2) { std::lock_guard<std::mutex> l(spectrumMutex_); if (iqBuffer_.size() < FFT_SIZE*2) iqBuffer_.resize(FFT_SIZE*2, 0.0f); }
        size_t pos = iqBufferWritePos_.load(std::memory_order_relaxed); iqBuffer_[pos*2] = out_i; iqBuffer_[pos*2+1] = out_q; iqBufferWritePos_.store((pos+1)%FFT_SIZE, std::memory_order_release);
        float aOut = demod_.process(i_f, q_f);
        if (!audioDecimateSkip_) {
            audioBuffer_.write(aOut);
            if (audioCaptureBuffer_.size() < 48000*5) audioCaptureBuffer_.push_back(std::tanh(aOut * 20000.0f * audioVolume_.load()));
            if (wavRecording_ && wavBuffer_.size() < wavMaxSamples_) wavBuffer_.push_back(std::tanh(aOut * 20000.0f * audioVolume_.load()));
        }
        audioDecimateSkip_ = !audioDecimateSkip_;
    }

    void saveAudioCapture() {
        if (audioCaptureBuffer_.empty()) return;
        std::ofstream f("/tmp/audio_capture.raw", std::ios::binary);
        if (f) f.write((char*)audioCaptureBuffer_.data(), audioCaptureBuffer_.size()*sizeof(float));
    }

    void startWavRecording(const std::string& p) { if (!p.empty()) wavFilePath_ = p; wavBuffer_.clear(); wavBuffer_.reserve(wavMaxSamples_); wavRecording_ = true; }
    void stopWavRecording() { if (!wavRecording_) return; wavRecording_ = false; if (!wavBuffer_.empty()) saveWavFile(wavFilePath_, wavBuffer_, 48000); }
    bool isWavRecording() const { return wavRecording_; }
    size_t getWavSampleCount() const { return wavBuffer_.size(); }

    void saveWavFile(const std::string& p, const std::vector<float>& s, int r) {
        std::ofstream f(p, std::ios::binary); if (!f) return;
        std::vector<int16_t> d(s.size()); for (size_t i=0; i<s.size(); ++i) d[i] = (int16_t)(std::clamp(s[i], -1.0f, 1.0f)*32767.0f);
        uint32_t ds = d.size()*2, fs = 36+ds; uint16_t nc = 1, bps = 16; uint32_t br = r*nc*bps/8; uint16_t ba = nc*bps/8;
        f.write("RIFF", 4); f.write((char*)&fs, 4); f.write("WAVE", 4); f.write("fmt ", 4);
        uint32_t fts = 16; f.write((char*)&fts, 4); uint16_t af = 1; f.write((char*)&af, 2);
        f.write((char*)&nc, 2); f.write((char*)&r, 4); f.write((char*)&br, 4); f.write((char*)&ba, 2); f.write((char*)&bps, 2);
        f.write("data", 4); f.write((char*)&ds, 4); f.write((char*)d.data(), ds);
    }

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
    static constexpr size_t FFT_SIZE = 1024; bool twinConnected_ = false; double qsdOffsetKhz_ = 12.0; double attenDb_ = 0.0;
    float shiftCos0_ = 1.0f, shiftSin0_ = 0.0f, shiftCos1_ = 1.0f, shiftSin1_ = 0.0f, shiftCosD_ = 1.0f, shiftSinD_ = 0.0f, lastShiftK_ = -1.0f;
    float lmsW0_r_ = 0.5f, lmsW0_i_ = 0.0f, lmsW1_r_ = 0.5f, lmsW1_i_ = 0.0f, lmsMu_ = 0.05f;
    Demodulator demod_; nexrx::BasebandFilter basebandFilter_{96000.0f}; nexrx::RateAdaptiveBuffer<float> audioBuffer_; std::atomic<float> audioVolume_{0.0316f};
    bool audioDecimateSkip_ = false; std::vector<float> audioCaptureBuffer_; bool wavRecording_ = false; std::string wavFilePath_ = "/tmp/nexrx_audio.wav";
    std::vector<float> wavBuffer_; size_t wavMaxSamples_ = 48000*60; nexrx::DropRateTracker audioDropTracker_, iqDropTracker_; float statsUpdateTime_ = 0.0f;
    std::atomic<float> signalLevelRms_{0.0f}; float signalAccumulator_ = 0.0f; size_t signalSampleCount_ = 0; static constexpr size_t SIGNAL_AVG_SAMPLES = 4800;
    int windowWidth_ = 0, windowHeight_ = 0; bool running_ = false, vsyncEnabled_ = true, luaConfigLoaded_ = false; uint32_t lastFrameTime_ = 0; InputState input_;
};

int main(int argc, char* argv[]) {
#ifdef HAVE_SSE_DENORMAL_CONTROL
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
    bool dv = false; for (int i=1; i<argc; ++i) if (std::string(argv[i]) == "--no-vsync" || std::string(argv[i]) == "-n") dv = true;
    std::cout << "NexRx Application Starting..." << std::endl;
#ifdef __APPLE__
    if (argv[0]) { std::string eP(argv[0]); auto p = eP.find(".app/Contents/MacOS/"); if (p != std::string::npos) chdir((eP.substr(0, p) + ".app/Contents/Resources").c_str()); }
#endif
    App app; gApp = &app; if (!app.init("NexRx", !dv)) return 1;
    std::cout << "Running main loop (Ctrl+Q to quit)..." << std::endl; app.run(); return 0;
}
