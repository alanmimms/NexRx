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
        // Rate adaptation is DISABLED because it causes aliasing artifacts:
        // - readWithSkip() drops samples without anti-alias filtering
        // - readWithStretch() uses linear interpolation (pitch shift artifacts)
        // With twin running at 6.6x realtime, buffer stays stable without adaptation.
        nexrx::BufferConfig audioConfig;
        audioConfig.capacity = 32768;        // ~680ms at 48kHz (jitter buffer)
        audioConfig.targetFillRatio = 0.5f;
        audioConfig.lowThreshold = 0.10f;    // Below this would stretch (disabled)
        audioConfig.highThreshold = 0.90f;   // Above this would skip (disabled)
        audioConfig.maxStretchRatio = 1.0f;  // No stretching
        audioConfig.enableAdaptation = false; // Disabled - prevents aliasing
        audioBuffer_.configure(audioConfig);

        demod_.setSampleRate(96000.0f);  // Input sample rate
        demod_.setMode(Demodulator::Mode::USB);
        demod_.setBfoOffset(700.0f);

        audio_.setCallback([this](float* output, uint32_t frameCount, uint32_t channels) {
            // Audio gain - RF signals are µV level relative to 1.65V ADC full scale
            // S9 = 50µV → 50e-6 / 1.65 = 3e-5 normalized
            // High base gain allows volume slider to have range both up and down
            constexpr float audioGain = 500000.0f;

            // Get volume once per callback (atomic read)
            const float volume = audioVolume_.load(std::memory_order_relaxed);

            // Read from rate-adaptive buffer (handles underruns gracefully)
            // Use thread-local temp buffer to avoid allocation
            thread_local std::vector<float> tempBuffer;
            tempBuffer.resize(frameCount);

            // Read with rate adaptation (stretches if buffer low, skips if high)
            audioBuffer_.read(std::span<float>(tempBuffer.data(), frameCount));

            for (uint32_t i = 0; i < frameCount; ++i) {
                float sample = tempBuffer[i] * audioGain * volume;

                // Soft clip (only activates on very loud signals)
                sample = std::tanh(sample);

                // Output to both channels (mono to stereo)
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
        // Save captured audio for debugging
        saveAudioCapture();

        // Shutdown twin connection
        if (twinConnected_) {
            twinHost_.stopReceiving();
            twinHost_.shutdown();
            twinConnected_ = false;
        }

        // Shutdown audio
        audio_.shutdown();

        if (glContext_) {
            SDL_GL_DeleteContext(glContext_);
            glContext_ = nullptr;
        }
        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        SDL_Quit();
    }

    void run() {
        constexpr uint32_t TARGET_FRAME_TIME_MS = 16;  // ~60 FPS cap when vsync disabled

        while (running_) {
            uint32_t frameStart = SDL_GetTicks();

            input_.beginFrame();
            pollEvents();

            // Update timing
            uint32_t now = SDL_GetTicks();
            float dt = (now - lastFrameTime_) / 1000.0f;
            lastFrameTime_ = now;

            // Update drop rate trackers (once per second)
            float currentTime = now / 1000.0f;
            audioDropTracker_.update(
                audioBuffer_.stats().dropsOverflow.load(std::memory_order_relaxed),
                currentTime);
            iqDropTracker_.update(
                twinHost_.framesDropped(),
                currentTime);

            // Call Lua update
            callLuaUpdate(dt);

            // Clear and draw
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Set up 2D projection
            setup2DProjection();

            // Call Lua draw
            callLuaDraw();

            SDL_GL_SwapWindow(window_);

            // When vsync disabled, manually cap frame rate to avoid spinning CPU
            if (!vsyncEnabled_) {
                uint32_t frameTime = SDL_GetTicks() - frameStart;
                if (frameTime < TARGET_FRAME_TIME_MS) {
                    SDL_Delay(TARGET_FRAME_TIME_MS - frameTime);
                }
            }
        }
    }

    void quit() { running_ = false; }

    // Accessors for Lua
    int windowWidth() const { return windowWidth_; }
    int windowHeight() const { return windowHeight_; }
    const InputState& input() const { return input_; }

    // Drawing primitives (called from Lua)
    void drawRect(float x, float y, float w, float h, float r, float g, float b, float a) {
        glColor4f(r, g, b, a);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        glEnd();
    }

    void drawRectOutline(float x, float y, float w, float h, float r, float g, float b, float a, float thickness) {
        glColor4f(r, g, b, a);
        glLineWidth(thickness);
        glBegin(GL_LINE_LOOP);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        glEnd();
    }

    void setClearColor(float r, float g, float b) {
        glClearColor(r, g, b, 1.0f);
    }

    // Text drawing
    float drawText(float x, float y, const std::string& text, float r, float g, float b, float a) {
        return font_.drawText(x, y, text, r, g, b, a);
    }

    float measureText(const std::string& text) {
        return font_.measureText(text);
    }

    float getLineHeight() {
        return font_.lineHeight();
    }

    // Additional drawing primitives
    void drawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a, float thickness) {
        glColor4f(r, g, b, a);
        glLineWidth(thickness);
        glBegin(GL_LINES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glEnd();
    }

    void drawCircle(float cx, float cy, float radius, float r, float g, float b, float a, int segments = 32) {
        glColor4f(r, g, b, a);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(cx, cy);
        for (int i = 0; i <= segments; ++i) {
            float angle = 2.0f * 3.14159265f * i / segments;
            glVertex2f(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
        }
        glEnd();
    }

    void drawCircleOutline(float cx, float cy, float radius, float r, float g, float b, float a, float thickness, int segments = 32) {
        glColor4f(r, g, b, a);
        glLineWidth(thickness);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float angle = 2.0f * 3.14159265f * i / segments;
            glVertex2f(cx + std::cos(angle) * radius, cy + std::sin(angle) * radius);
        }
        glEnd();
    }

    void drawRoundedRect(float x, float y, float w, float h, float radius, float r, float g, float b, float a) {
        glColor4f(r, g, b, a);

        // Clamp radius
        radius = std::min(radius, std::min(w, h) / 2.0f);
        if (radius < 0.5f) {
            // No rounding, just draw a simple rect
            glBegin(GL_QUADS);
            glVertex2f(x, y);
            glVertex2f(x + w, y);
            glVertex2f(x + w, y + h);
            glVertex2f(x, y + h);
            glEnd();
            return;
        }

        const int cornerSegments = 8;
        constexpr float PI = 3.14159265f;

        glBegin(GL_TRIANGLE_FAN);
        // Center
        glVertex2f(x + w/2, y + h/2);

        // Start at top-left of top edge, go clockwise
        // Top edge (left to right)
        glVertex2f(x + radius, y);
        glVertex2f(x + w - radius, y);

        // Top-right corner
        for (int i = 0; i <= cornerSegments; ++i) {
            float angle = -PI/2.0f + (PI/2.0f) * i / cornerSegments;
            glVertex2f(x + w - radius + std::cos(angle) * radius, y + radius + std::sin(angle) * radius);
        }

        // Right edge (top to bottom)
        glVertex2f(x + w, y + h - radius);

        // Bottom-right corner
        for (int i = 0; i <= cornerSegments; ++i) {
            float angle = 0.0f + (PI/2.0f) * i / cornerSegments;
            glVertex2f(x + w - radius + std::cos(angle) * radius, y + h - radius + std::sin(angle) * radius);
        }

        // Bottom edge (right to left)
        glVertex2f(x + radius, y + h);

        // Bottom-left corner
        for (int i = 0; i <= cornerSegments; ++i) {
            float angle = PI/2.0f + (PI/2.0f) * i / cornerSegments;
            glVertex2f(x + radius + std::cos(angle) * radius, y + h - radius + std::sin(angle) * radius);
        }

        // Left edge (bottom to top)
        glVertex2f(x, y + radius);

        // Top-left corner
        for (int i = 0; i <= cornerSegments; ++i) {
            float angle = PI + (PI/2.0f) * i / cornerSegments;
            glVertex2f(x + radius + std::cos(angle) * radius, y + radius + std::sin(angle) * radius);
        }

        // Close back to start
        glVertex2f(x + radius, y);

        glEnd();
    }

private:
    // ==========================================================================
    // initLuaConfig() - Early Lua initialization for configuration
    // Called BEFORE window/OpenGL creation to read window size, font paths, etc.
    // ==========================================================================
    bool initLuaConfig() {
        lua_.open_libraries(
            sol::lib::base,
            sol::lib::package,
            sol::lib::string,
            sol::lib::table,
            sol::lib::math,
            sol::lib::io,
            sol::lib::os
        );

        // Set up package path
        std::string path = lua_["package"]["path"];
        path += ";lua/?.lua;lua/?/init.lua";
        lua_["package"]["path"] = path;

        // Load SetBox module (pure Lua configuration system)
        try {
            lua_.safe_script_file("lua/setbox.lua", sol::script_pass_on_error);
        } catch (const std::exception& e) {
            std::cerr << "Failed to load lua/setbox.lua: " << e.what() << std::endl;
            return false;
        }

        // Verify setbox module loaded correctly
        if (!lua_["setbox"].valid() || !lua_["rule"].valid()) {
            std::cerr << "Error: setbox module did not export setbox table or rule function" << std::endl;
            return false;
        }

        // Load configuration files
        sol::function loadFile = lua_["setbox"]["loadFile"];
        if (loadFile.valid()) {
            if (!loadFile("config/default.lua").get<bool>()) {
                std::cerr << "Warning: Failed to load config/default.lua" << std::endl;
            }
            // Settings file is optional - user customizations
            loadFile("config/settings.lua");  // OK if missing
        } else {
            std::cerr << "Error: setbox.loadFile not available" << std::endl;
            return false;
        }

        luaConfigLoaded_ = true;
        return true;
    }

    bool initLua() {
        // If config already loaded (from initLuaConfig), skip those steps
        if (!luaConfigLoaded_) {
            lua_.open_libraries(
                sol::lib::base,
                sol::lib::package,
                sol::lib::string,
                sol::lib::table,
                sol::lib::math,
                sol::lib::io,
                sol::lib::os
            );

            // Set up package path
            std::string path = lua_["package"]["path"];
            path += ";lua/?.lua;lua/?/init.lua";
            lua_["package"]["path"] = path;
        }

        // Expose app functions to Lua
        lua_.set_function("quit", [this]() { quit(); });

        // Window info
        lua_.set_function("getWindowSize", [this]() {
            return std::make_tuple(windowWidth_, windowHeight_);
        });

        // Input
        lua_.set_function("getMousePos", [this]() {
            return std::make_tuple(input_.mouseX, input_.mouseY);
        });

        lua_.set_function("isMouseDown", [this](int button) {
            if (button >= 0 && button < 3) return input_.mouseDown[button];
            return false;
        });

        lua_.set_function("isMouseClicked", [this](int button) {
            if (button >= 0 && button < 3) return input_.mouseClicked[button];
            return false;
        });

        lua_.set_function("isKeyDown", [this](int key) {
            if (key >= 0 && key < 512) return input_.keyDown[key];
            return false;
        });

        lua_.set_function("isKeyPressed", [this](int key) {
            if (key >= 0 && key < 512) return input_.keyPressed[key];
            return false;
        });

        lua_.set_function("getMouseWheel", [this]() {
            return input_.mouseWheel;
        });

        lua_.set_function("isShiftDown", [this]() {
            return input_.shiftDown;
        });

        lua_.set_function("isCtrlDown", [this]() {
            return input_.ctrlDown;
        });

        lua_.set_function("isAltDown", [this]() {
            return input_.altDown;
        });

        lua_.set_function("getTextInput", [this]() {
            return input_.textInput;
        });

        // Drawing primitives
        lua_.set_function("drawRect", [this](float x, float y, float w, float h,
                                              float r, float g, float b, float a) {
            drawRect(x, y, w, h, r, g, b, a);
        });

        lua_.set_function("drawRectOutline", [this](float x, float y, float w, float h,
                                                     float r, float g, float b, float a, float thickness) {
            drawRectOutline(x, y, w, h, r, g, b, a, thickness);
        });

        lua_.set_function("setClearColor", [this](float r, float g, float b) {
            setClearColor(r, g, b);
        });

        // Text drawing
        lua_.set_function("drawText", [this](float x, float y, const std::string& text,
                                              float r, float g, float b, float a) {
            return drawText(x, y, text, r, g, b, a);
        });

        lua_.set_function("measureText", [this](const std::string& text) {
            return measureText(text);
        });

        lua_.set_function("getLineHeight", [this]() {
            return getLineHeight();
        });

        // Additional drawing primitives
        lua_.set_function("drawLine", [this](float x1, float y1, float x2, float y2,
                                              float r, float g, float b, float a, float thickness) {
            drawLine(x1, y1, x2, y2, r, g, b, a, thickness);
        });

        lua_.set_function("drawCircle", [this](float cx, float cy, float radius,
                                                float r, float g, float b, float a) {
            drawCircle(cx, cy, radius, r, g, b, a);
        });

        lua_.set_function("drawCircleOutline", [this](float cx, float cy, float radius,
                                                       float r, float g, float b, float a, float thickness) {
            drawCircleOutline(cx, cy, radius, r, g, b, a, thickness);
        });

        lua_.set_function("drawRoundedRect", [this](float x, float y, float w, float h, float radius,
                                                     float r, float g, float b, float a) {
            drawRoundedRect(x, y, w, h, radius, r, g, b, a);
        });

        // ==========================================================================
        // SetBox Configuration (already loaded by initLuaConfig if luaConfigLoaded_)
        // ==========================================================================
        if (!luaConfigLoaded_) {
            // Load SetBox if not already done
            try {
                lua_.safe_script_file("lua/setbox.lua", sol::script_pass_on_error);
            } catch (const std::exception& e) {
                std::cerr << "Failed to load lua/setbox.lua: " << e.what() << std::endl;
                return false;
            }

            if (!lua_["setbox"].valid() || !lua_["rule"].valid()) {
                std::cerr << "Error: setbox module did not export setbox table or rule function" << std::endl;
                return false;
            }
        }

        // Expose audio engine to Lua
        lua_["audio"] = lua_.create_table();

        lua_["audio"]["start"] = [this]() {
            return audio_.start();
        };

        lua_["audio"]["stop"] = [this]() {
            audio_.stop();
        };

        lua_["audio"]["isPlaying"] = [this]() {
            return audio_.isPlaying();
        };

        lua_["audio"]["setVolume"] = [this](float volume) {
            audioVolume_.store(volume, std::memory_order_relaxed);
        };

        lua_["audio"]["getVolume"] = [this]() {
            return audioVolume_.load(std::memory_order_relaxed);
        };

        lua_["audio"]["setMuted"] = [this](bool muted) {
            audio_.setMuted(muted);
        };

        lua_["audio"]["isMuted"] = [this]() {
            return audio_.isMuted();
        };

        lua_["audio"]["setTestTone"] = [this](bool enabled, sol::optional<float> frequency) {
            audio_.setTestTone(enabled, frequency.value_or(440.0f));
        };

        lua_["audio"]["isTestToneEnabled"] = [this]() {
            return audio_.isTestToneEnabled();
        };

        lua_["audio"]["getSampleRate"] = [this]() {
            return audio_.getSampleRate();
        };

        lua_["audio"]["isInitialized"] = [this]() {
            return audio_.isInitialized();
        };

        // WAV recording control
        lua_["audio"]["startRecording"] = [this](sol::optional<std::string> path) {
            startWavRecording(path.value_or(""));
        };

        lua_["audio"]["stopRecording"] = [this]() {
            stopWavRecording();
        };

        lua_["audio"]["isRecording"] = [this]() {
            return isWavRecording();
        };

        lua_["audio"]["getRecordingSamples"] = [this]() {
            return getWavSampleCount();
        };

        lua_["audio"]["getRecordingDuration"] = [this]() {
            return static_cast<double>(getWavSampleCount()) / 48000.0;
        };

        // Expose waterfall renderer to Lua
        lua_["waterfall"] = lua_.create_table();

        lua_["waterfall"]["init"] = [this](int width, int height) {
            return waterfall_.init(width, height);
        };

        lua_["waterfall"]["addRow"] = [this](sol::table data) {
            std::vector<float> rowData;
            rowData.reserve(data.size());
            for (size_t i = 1; i <= data.size(); ++i) {
                rowData.push_back(data[i].get_or(waterfall_.getMinDb()));
            }
            waterfall_.addRow(rowData.data(), static_cast<int>(rowData.size()));
        };

        lua_["waterfall"]["render"] = [this](float x, float y, float w, float h) {
            waterfall_.render(x, y, w, h);
        };

        lua_["waterfall"]["renderSpectrum"] = [this](sol::table data, float x, float y, float w, float h) {
            std::vector<float> specData;
            specData.reserve(data.size());
            for (size_t i = 1; i <= data.size(); ++i) {
                specData.push_back(data[i].get_or(waterfall_.getMinDb()));
            }
            waterfall_.renderSpectrum(specData.data(), static_cast<int>(specData.size()), x, y, w, h);
        };

        // setColormapData from Lua gradient stops - Lua owns colormap definitions
        // Input: table of {position, r, g, b} where position is 0-1, RGB are 0-255
        lua_["waterfall"]["setColormapData"] = [this](sol::table stops) {
            std::vector<std::tuple<float, uint8_t, uint8_t, uint8_t>> gradientStops;
            for (size_t i = 1; i <= stops.size(); ++i) {
                sol::table stop = stops[i];
                float pos = stop[1].get_or(0.0f);
                uint8_t r = static_cast<uint8_t>(stop[2].get_or(0));
                uint8_t g = static_cast<uint8_t>(stop[3].get_or(0));
                uint8_t b = static_cast<uint8_t>(stop[4].get_or(0));
                gradientStops.push_back({pos, r, g, b});
            }
            waterfall_.setColormapData(gradientStops);
        };

        lua_["waterfall"]["setRange"] = [this](float minDb, float maxDb) {
            waterfall_.setRange(minDb, maxDb);
        };

        lua_["waterfall"]["getMinDb"] = [this]() {
            return waterfall_.getMinDb();
        };

        lua_["waterfall"]["getMaxDb"] = [this]() {
            return waterfall_.getMaxDb();
        };

        lua_["waterfall"]["isInitialized"] = [this]() {
            return waterfall_.isInitialized();
        };

        lua_["waterfall"]["getWidth"] = [this]() {
            return waterfall_.getWidth();
        };

        lua_["waterfall"]["getHeight"] = [this]() {
            return waterfall_.getHeight();
        };

        // Expose hardware interface to Lua ("hw" = hardware abstraction)
        // "twin" is a hardware emulator; this interface works with any backend
        lua_["hw"] = lua_.create_table();

        lua_["hw"]["connect"] = [this](
            sol::optional<std::string> host,
            sol::optional<int> controlPort,
            sol::optional<int> streamPort)
        {
            nexrx::TwinConfig config;
            config.host = host.value_or("127.0.0.1");
            config.controlPort = static_cast<uint16_t>(controlPort.value_or(5000));
            config.streamPort = static_cast<uint16_t>(streamPort.value_or(5001));
            config.verbose = true;

            std::cout << "[Twin] Connecting to " << config.host
                      << " (TCP:" << config.controlPort
                      << ", UDP:" << config.streamPort << ")..." << std::endl;

            if (twinHost_.initialize(config)) {
                // Set callback to process incoming frames
                twinHost_.setFrameCallback([this](const nexrx::IQFrame& frame) {
                    processIQFrame(frame);
                });

                // Request twin to start streaming UDP data
                if (!twinHost_.startStream()) {
                    std::cerr << "[Twin] Failed to start stream" << std::endl;
                }

                if (twinHost_.startReceiving()) {
                    twinConnected_ = true;
                    std::cout << "[Twin] Connected" << std::endl;
                    return true;
                }
            }
            std::cerr << "[Twin] Failed to connect" << std::endl;
            return false;
        };

        lua_["hw"]["disconnect"] = [this]() {
            twinHost_.stopReceiving();
            twinHost_.shutdown();
            twinConnected_ = false;
            std::cout << "[Twin] Disconnected" << std::endl;
        };

        lua_["hw"]["isConnected"] = [this]() {
            return twinConnected_ && twinHost_.isConnected();
        };

        lua_["hw"]["getFramesReceived"] = [this]() {
            return static_cast<double>(twinHost_.framesReceived());
        };

        lua_["hw"]["getFramesDropped"] = [this]() {
            return static_cast<double>(twinHost_.framesDropped());
        };

        lua_["hw"]["getIqDropRate"] = [this]() {
            // Return IQ frame drops per second
            return static_cast<double>(iqDropTracker_.dropsPerSecond());
        };

        lua_["hw"]["getSpectrum"] = [this](sol::this_state s) {
            // Compute spectrum from accumulated I/Q data
            computeSpectrum();

            sol::state_view lua(s);
            sol::table result = lua.create_table();

            std::lock_guard<std::mutex> lock(spectrumMutex_);
            for (size_t i = 0; i < spectrumData_.size(); ++i) {
                result[i + 1] = spectrumData_[i];
            }
            return result;
        };

        lua_["hw"]["poll"] = [this]() {
            // Poll for frames if not using background thread
            return twinHost_.pollFrames(100);
        };

        lua_["hw"]["setQsdOffset"] = [this](double k_khz) {
            // Set QSD offset (k) via TCP control
            // QSD0 = f-k, QSD1 = f+k, QSD2 = f (sextature)
            if (twinConnected_) {
                std::string cmd = "SET_QSD_OFFSET " + std::to_string(k_khz);
                twinHost_.sendCommand(cmd);
                qsdOffsetKhz_ = k_khz;
            }
        };

        lua_["hw"]["getQsdOffset"] = [this]() {
            return qsdOffsetKhz_;
        };

        // Attenuator control (0-45 dB in 3 dB steps)
        lua_["hw"]["setAttenuation"] = [this](double db) {
            // Set total attenuation - twin will round to nearest valid value
            if (twinConnected_) {
                std::string cmd = "SET_ATTEN_TOTAL " + std::to_string(db);
                twinHost_.sendCommand(cmd);
                attenDb_ = db;
            }
        };

        lua_["hw"]["setAttenuator"] = [this](int db, bool enabled) {
            // Set individual attenuator (3, 6, 12, or 24 dB)
            if (twinConnected_ && (db == 3 || db == 6 || db == 12 || db == 24)) {
                std::string cmd = "SET_ATTEN " + std::to_string(db) + " " + (enabled ? "1" : "0");
                twinHost_.sendCommand(cmd);
            }
        };

        lua_["hw"]["getAttenuation"] = [this]() {
            return attenDb_;
        };

        // Expose receiver controls to Lua
        lua_["rx"] = lua_.create_table();

        // Mode control - Lua passes integer mode ID, no string parsing in C++
        // Mode IDs: 0=USB, 1=LSB, 2=AM, 3=CW (matches Demodulator::Mode enum)
        lua_["rx"]["setModeId"] = [this](int modeId) {
            if (modeId >= 0 && modeId <= 3) {
                demod_.setMode(static_cast<Demodulator::Mode>(modeId));
            }
        };

        lua_["rx"]["getModeId"] = [this]() {
            return static_cast<int>(demod_.getMode());
        };

        lua_["rx"]["setBfo"] = [this](float hz) {
            demod_.setBfoOffset(hz);
        };

        lua_["rx"]["getBfo"] = [this]() {
            return demod_.getBfoOffset();
        };

        // Baseband filter control (FIR bandpass and notch)
        lua_["rx"]["setBandpassEnabled"] = [this](bool en) {
            basebandFilter_.setBandpassEnabled(en);
        };
        lua_["rx"]["setBandpassCenter"] = [this](float hz) {
            basebandFilter_.setBandpassCenter(hz);
        };
        lua_["rx"]["setBandpassWidth"] = [this](float hz) {
            basebandFilter_.setBandpassWidth(hz);
        };
        lua_["rx"]["setBandpassTaps"] = [this](int taps) {
            basebandFilter_.setBandpassTaps(taps);
        };
        lua_["rx"]["getBandpassEnabled"] = [this]() {
            return basebandFilter_.bandpassEnabled();
        };
        lua_["rx"]["getBandpassCenter"] = [this]() {
            return basebandFilter_.bandpassCenter();
        };
        lua_["rx"]["getBandpassWidth"] = [this]() {
            return basebandFilter_.bandpassWidth();
        };
        lua_["rx"]["getBandpassTaps"] = [this]() {
            return basebandFilter_.bandpassTaps();
        };

        lua_["rx"]["setNotchEnabled"] = [this](bool en) {
            basebandFilter_.setNotchEnabled(en);
        };
        lua_["rx"]["setNotchCenter"] = [this](float hz) {
            basebandFilter_.setNotchCenter(hz);
        };
        lua_["rx"]["setNotchWidth"] = [this](float hz) {
            basebandFilter_.setNotchWidth(hz);
        };
        lua_["rx"]["getNotchEnabled"] = [this]() {
            return basebandFilter_.notchEnabled();
        };
        lua_["rx"]["getNotchCenter"] = [this]() {
            return basebandFilter_.notchCenter();
        };
        lua_["rx"]["getNotchWidth"] = [this]() {
            return basebandFilter_.notchWidth();
        };

        // Force filter coefficient recomputation (call after batch parameter changes)
        lua_["rx"]["recomputeFilters"] = [this]() {
            return basebandFilter_.recompute();
        };

        // LMS adaptive filter control
        lua_["rx"]["setLmsMu"] = [this](float mu) {
            lmsMu_ = std::clamp(mu, 0.0001f, 0.1f);
        };

        lua_["rx"]["getLmsMu"] = [this]() {
            return lmsMu_;
        };

        lua_["rx"]["getAudioStats"] = [this]() {
            // Return audio buffer stats for debugging
            // Returns: written, read, underruns, drops, fill_ratio
            const auto& stats = audioBuffer_.stats();
            return std::make_tuple(
                static_cast<double>(stats.samplesWritten.load()),
                static_cast<double>(stats.samplesRead.load()),
                static_cast<double>(stats.underruns.load()),
                static_cast<double>(stats.dropsOverflow.load()),
                static_cast<double>(audioBuffer_.getFillRatio())
            );
        };

        lua_["rx"]["getAudioDropRate"] = [this]() {
            // Return audio drops per second
            return static_cast<double>(audioDropTracker_.dropsPerSecond());
        };

        lua_["rx"]["getAudioBufferFill"] = [this]() {
            // Return buffer fill ratio (0.0 - 1.0)
            return static_cast<double>(audioBuffer_.getFillRatio());
        };

        lua_["rx"]["setVfo"] = [this](double freq_hz) {
            // Send VFO frequency via TCP control to twin
            if (twinConnected_) {
                twinHost_.setLO(freq_hz);
            }
        };

        // Signal level - C++ only provides raw RMS, Lua calculates dBm/S-units
        lua_["rx"]["getSignalRms"] = [this]() {
            return signalLevelRms_.load(std::memory_order_relaxed);
        };

        // Load property handlers (defines global onPropertyChange function)
        try {
            lua_.safe_script_file("lua/property_handlers.lua", sol::script_pass_on_error);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to load lua/property_handlers.lua: " << e.what() << std::endl;
        }

        // Load additional config files (default.lua and settings.lua already loaded by initLuaConfig)
        sol::function loadFile = lua_["setbox"]["loadFile"];
        if (loadFile.valid()) {
            // default.lua and settings.lua already loaded in initLuaConfig (for window size, fonts)
            if (!luaConfigLoaded_) {
                if (!loadFile("config/default.lua").get<bool>()) {
                    std::cerr << "Warning: Failed to load config/default.lua" << std::endl;
                }
                loadFile("config/settings.lua");  // Optional
            }
            // Always load these additional config files
            if (!loadFile("config/modes.lua").get<bool>()) {
                std::cerr << "Warning: Failed to load config/modes.lua" << std::endl;
            }
            if (!loadFile("config/colormaps.lua").get<bool>()) {
                std::cerr << "Warning: Failed to load config/colormaps.lua" << std::endl;
            }
            if (!loadFile("config/bands.lua").get<bool>()) {
                std::cerr << "Warning: Failed to load config/bands.lua" << std::endl;
            }
            if (!loadFile("config/events.lua").get<bool>()) {
                std::cerr << "Warning: Failed to load config/events.lua" << std::endl;
            }
        } else {
            std::cerr << "Error: setbox.loadFile not available" << std::endl;
            return false;
        }

        // Wire SetBox property changes to Lua handlers
        // This triggers onPropertyChange when config values change
        sol::function registerCallback = lua_["setbox"]["onPropertyChange"];
        if (registerCallback.valid()) {
            registerCallback(lua_["onPropertyChange"]);
        }

        // Load main Lua script
        try {
            auto result = lua_.safe_script_file("lua/main.lua", sol::script_pass_on_error);
            if (!result.valid()) {
                sol::error err = result;
                std::cerr << "Failed to load lua/main.lua: " << err.what() << std::endl;
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception loading lua/main.lua: " << e.what() << std::endl;
            return false;
        }

        // Call Lua init function if it exists
        sol::function initFn = lua_["init"];
        if (initFn.valid()) {
            try {
                initFn();
            } catch (const std::exception& e) {
                std::cerr << "Lua init() error: " << e.what() << std::endl;
            }
        }

        return true;
    }

    void pollEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running_ = false;
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        windowWidth_ = event.window.data1;
                        windowHeight_ = event.window.data2;
                        glViewport(0, 0, windowWidth_, windowHeight_);
                    }
                    break;

                case SDL_MOUSEMOTION:
                    input_.mouseX = event.motion.x;
                    input_.mouseY = event.motion.y;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button <= 3) {
                        int idx = event.button.button - 1;
                        input_.mouseDown[idx] = true;
                        input_.mouseClicked[idx] = true;
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if (event.button.button <= 3) {
                        int idx = event.button.button - 1;
                        input_.mouseDown[idx] = false;
                        input_.mouseReleased[idx] = true;
                    }
                    break;

                case SDL_MOUSEWHEEL:
                    input_.mouseWheel = event.wheel.y;
                    break;

                case SDL_KEYDOWN:
                    if (event.key.keysym.scancode < 512) {
                        input_.keyDown[event.key.keysym.scancode] = true;
                        if (!event.key.repeat) {
                            input_.keyPressed[event.key.keysym.scancode] = true;
                        }
                    }
                    // Update modifiers
                    input_.shiftDown = (event.key.keysym.mod & KMOD_SHIFT) != 0;
                    input_.ctrlDown = (event.key.keysym.mod & KMOD_CTRL) != 0;
                    input_.altDown = (event.key.keysym.mod & KMOD_ALT) != 0;
                    // Quit is handled by Lua (Ctrl+Q dispatches to app_quit handler)
                    break;

                case SDL_KEYUP:
                    if (event.key.keysym.scancode < 512) {
                        input_.keyDown[event.key.keysym.scancode] = false;
                        input_.keyReleased[event.key.keysym.scancode] = true;
                    }
                    // Update modifiers
                    input_.shiftDown = (event.key.keysym.mod & KMOD_SHIFT) != 0;
                    input_.ctrlDown = (event.key.keysym.mod & KMOD_CTRL) != 0;
                    input_.altDown = (event.key.keysym.mod & KMOD_ALT) != 0;
                    break;

                case SDL_TEXTINPUT:
                    input_.textInput += event.text.text;
                    break;
            }
        }
    }

    void setup2DProjection() {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, windowWidth_, windowHeight_, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void callLuaUpdate(float dt) {
        sol::function updateFn = lua_["update"];
        if (updateFn.valid()) {
            try {
                updateFn(dt);
            } catch (const std::exception& e) {
                std::cerr << "Lua update() error: " << e.what() << std::endl;
            }
        }
    }

    void callLuaDraw() {
        sol::function drawFn = lua_["draw"];
        if (drawFn.valid()) {
            try {
                drawFn();
            } catch (const std::exception& e) {
                std::cerr << "Lua draw() error: " << e.what() << std::endl;
            }
        }
    }

    SDL_Window* window_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    sol::state lua_;
    FontRenderer font_;
    AudioEngine audio_;
    WaterfallRenderer waterfall_;

    // Hardware integration
    nexrx::TwinConn twinHost_;
    std::mutex spectrumMutex_;
    std::vector<float> spectrumData_;      // Latest spectrum (dB values)
    std::vector<float> iqBuffer_;          // Ring buffer for FFT
    std::atomic<size_t> iqBufferWritePos_{0};
    static constexpr size_t FFT_SIZE = 1024;
    bool twinConnected_ = false;
    double qsdOffsetKhz_ = 12.0;           // QSD offset k in kHz (default 12kHz)
    double attenDb_ = 0.0;                 // RF attenuator setting (0-45 dB)
    // Incremental phase rotation for frequency shifters (avoids trig per sample)
    float shiftCos0_ = 1.0f, shiftSin0_ = 0.0f;  // Current phasor for QSD0
    float shiftCos1_ = 1.0f, shiftSin1_ = 0.0f;  // Current phasor for QSD1
    float shiftCosD_ = 1.0f, shiftSinD_ = 0.0f;  // Phase increment phasor
    float lastShiftK_ = -1.0f;                    // Last k value (to detect changes, init to invalid)

    // LMS Adaptive Image Rejection weights (complex: w = wr + j*wi)
    // Trained to make w0*QSD0 + w1*QSD1 ≈ QSD2 (the sextature reference)
    float lmsW0_r_ = 0.5f;                 // Weight for QSD0, real part
    float lmsW0_i_ = 0.0f;                 // Weight for QSD0, imaginary part
    float lmsW1_r_ = 0.5f;                 // Weight for QSD1, real part
    float lmsW1_i_ = 0.0f;                 // Weight for QSD1, imaginary part
    float lmsMu_ = 0.001f;                 // LMS learning rate (step size)

    // Demodulator and audio output
    Demodulator demod_;
    nexrx::BasebandFilter basebandFilter_{96000.0f};  // Complex FIR bandpass/notch
    nexrx::RateAdaptiveBuffer<float> audioBuffer_;
    std::atomic<float> audioVolume_{0.0316f};  // Volume applied before soft-clip
    bool audioDecimateSkip_ = false;  // For 96kHz→48kHz decimation
    std::vector<float> audioCaptureBuffer_;  // Debug: capture audio samples

    // WAV recording state
    bool wavRecording_ = false;
    std::string wavFilePath_ = "/tmp/nexrx_audio.wav";
    std::vector<float> wavBuffer_;
    size_t wavMaxSamples_ = 48000 * 60;  // Max 60 seconds at 48kHz

    // Drop rate tracking (computed once per second)
    nexrx::DropRateTracker audioDropTracker_;
    nexrx::DropRateTracker iqDropTracker_;
    float statsUpdateTime_ = 0.0f;

    // Signal level metering (S-meter)
    std::atomic<float> signalLevelRms_{0.0f};    // RMS voltage level
    float signalAccumulator_ = 0.0f;             // Sum of squared magnitudes
    size_t signalSampleCount_ = 0;
    static constexpr size_t SIGNAL_AVG_SAMPLES = 4800;  // ~50ms at 96kHz

    int windowWidth_ = 0;
    int windowHeight_ = 0;
    bool running_ = false;
    bool vsyncEnabled_ = true;
    bool luaConfigLoaded_ = false;  // Set by initLuaConfig() before window creation
    uint32_t lastFrameTime_ = 0;

    InputState input_;

    // Twin I/Q processing with LMS Adaptive Image Rejection
    void processIQFrame(const nexrx::IQFrame& frame) {
        // Frame rate monitoring - print every second
        static uint64_t frameCount = 0;
        static uint64_t audioWriteCount = 0;
        static auto lastPrint = std::chrono::steady_clock::now();
        frameCount++;

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - lastPrint).count() >= 1.0) {
            std::cout << "[Rate] frames/sec: " << frameCount
                      << ", audio writes/sec: " << audioWriteCount
                      << ", buffer fill: " << audioBuffer_.getFillRatio()
                      << ", IQ dropped: " << twinHost_.framesDropped()
                      << ", IQ overruns: " << twinHost_.bufferOverruns()
                      << std::endl;
            frameCount = 0;
            audioWriteCount = 0;
            lastPrint = now;
        }

        // Get all three QSD outputs
        // QSD0: 4×(f-k) quadrature sampling - baseband shifted UP by k
        // QSD1: 4×(f+k) quadrature sampling - baseband shifted DOWN by k
        // QSD2: 6×f sextature sampling - baseband at center (reference)
        float i0, q0, i1, q1, i2, q2;
        frame.qsd[0].toFloat(i0, q0);
        frame.qsd[1].toFloat(i1, q1);
        frame.qsd[2].toFloat(i2, q2);

        // Frequency-shift QSD0 and QSD1 to align with QSD2
        // QSD0 is at baseband+k, shift DOWN by k: multiply by exp(-j*2π*k*t)
        // QSD1 is at baseband-k, shift UP by k: multiply by exp(+j*2π*k*t)
        //
        // Using incremental phase rotation to avoid trig per sample:
        // phasor(n+1) = phasor(n) * phasorDelta
        constexpr float sampleRate = 96000.0f;
        float k_hz = static_cast<float>(qsdOffsetKhz_ * 1000.0);

        // Recalculate phase increment if k changed
        if (k_hz != lastShiftK_) {
            float phaseInc = 2.0f * 3.14159265f * k_hz / sampleRate;
            shiftCosD_ = std::cos(phaseInc);
            shiftSinD_ = std::sin(phaseInc);
            lastShiftK_ = k_hz;
        }

        // Shift QSD0 DOWN by k: (i0 + j*q0) * (cos - j*sin)
        float i0_s = i0 * shiftCos0_ + q0 * shiftSin0_;
        float q0_s = q0 * shiftCos0_ - i0 * shiftSin0_;

        // Shift QSD1 UP by k: (i1 + j*q1) * (cos + j*sin)
        float i1_s = i1 * shiftCos1_ - q1 * shiftSin1_;
        float q1_s = q1 * shiftCos1_ + i1 * shiftSin1_;

        // Advance oscillators via incremental rotation (no trig calls!)
        // phasor = phasor * delta: (c,s) * (cd,sd) = (c*cd - s*sd, s*cd + c*sd)
        float c0 = shiftCos0_ * shiftCosD_ - shiftSin0_ * shiftSinD_;
        float s0 = shiftSin0_ * shiftCosD_ + shiftCos0_ * shiftSinD_;
        shiftCos0_ = c0;
        shiftSin0_ = s0;

        float c1 = shiftCos1_ * shiftCosD_ - shiftSin1_ * shiftSinD_;
        float s1 = shiftSin1_ * shiftCosD_ + shiftCos1_ * shiftSinD_;
        shiftCos1_ = c1;
        shiftSin1_ = s1;

        // Periodically renormalize shift oscillators to prevent drift
        // Every ~65K samples at 96kHz = ~0.7 seconds
        static uint32_t renormCounter = 0;
        if ((++renormCounter & 0xFFFF) == 0) {
            auto renorm = [](float& c, float& s) {
                float mag = std::sqrt(c * c + s * s);
                if (mag > 0) { c /= mag; s /= mag; }
            };
            renorm(shiftCos0_, shiftSin0_);
            renorm(shiftCos1_, shiftSin1_);
        }

        // =====================================================================
        // LMS Adaptive Image Rejection (from doc/multiple-QSDs.md)
        //
        // Real signals align perfectly after digital rotation.
        // Image signals point in different directions in each mixer.
        // Use LMS to find complex weights that null the image.
        //
        // We use QSD2 (sextature) as reference - it has inherent 3rd harmonic
        // rejection from 6-phase sampling. Train weights w0, w1 such that:
        //   output = w0 * QSD0_shifted + w1 * QSD1_shifted ≈ QSD2
        //
        // This transfers QSD2's image/harmonic rejection to the output.
        // =====================================================================

        // Compute weighted output: output = w0 * qsd0 + w1 * qsd1
        // Complex multiply: (wr + j*wi) * (i + j*q) = (wr*i - wi*q) + j*(wr*q + wi*i)
        float out_i = (lmsW0_r_ * i0_s - lmsW0_i_ * q0_s) + (lmsW1_r_ * i1_s - lmsW1_i_ * q1_s);
        float out_q = (lmsW0_r_ * q0_s + lmsW0_i_ * i0_s) + (lmsW1_r_ * q1_s + lmsW1_i_ * i1_s);

        // Error: difference from reference (QSD2)
        float err_i = i2 - out_i;
        float err_q = q2 - out_q;

        // LMS weight update: w = w + mu * error * conj(input)
        // error * conj(input) = (err_i + j*err_q) * (in_i - j*in_q)
        //                     = (err_i*in_i + err_q*in_q) + j*(err_q*in_i - err_i*in_q)
        float update0_r = err_i * i0_s + err_q * q0_s;
        float update0_i = err_q * i0_s - err_i * q0_s;
        float update1_r = err_i * i1_s + err_q * q1_s;
        float update1_i = err_q * i1_s - err_i * q1_s;

        lmsW0_r_ += lmsMu_ * update0_r;
        lmsW0_i_ += lmsMu_ * update0_i;
        lmsW1_r_ += lmsMu_ * update1_r;
        lmsW1_i_ += lmsMu_ * update1_i;

        // Clamp weights to prevent runaway (should converge to ~0.5 each)
        auto clampWeight = [](float& wr, float& wi) {
            float mag = std::sqrt(wr * wr + wi * wi);
            if (mag > 2.0f) {
                wr *= 2.0f / mag;
                wi *= 2.0f / mag;
            }
        };
        clampWeight(lmsW0_r_, lmsW0_i_);
        clampWeight(lmsW1_r_, lmsW1_i_);

        // Use LMS output for display (best image rejection)
        float i_f = out_i;
        float q_f = out_q;

        // Apply baseband filters (bandpass and/or notch)
        basebandFilter_.process(i_f, q_f);

        // Use filtered output for audio
        float audio_i = i_f;
        float audio_q = q_f;

        // Signal level metering (RMS of I/Q magnitude)
        float mag_sq = i_f * i_f + q_f * q_f;
        signalAccumulator_ += mag_sq;
        signalSampleCount_++;
        if (signalSampleCount_ >= SIGNAL_AVG_SAMPLES) {
            float rms = std::sqrt(signalAccumulator_ / signalSampleCount_);
            signalLevelRms_.store(rms, std::memory_order_relaxed);
            signalAccumulator_ = 0.0f;
            signalSampleCount_ = 0;
        }

        // Add to spectrum ring buffer (lock-free for producer)
        // Only the FFT consumer needs the lock, and it copies data out
        if (iqBuffer_.size() < FFT_SIZE * 2) {
            // One-time init (rare path, ok to be slow)
            std::lock_guard<std::mutex> lock(spectrumMutex_);
            if (iqBuffer_.size() < FFT_SIZE * 2) {
                iqBuffer_.resize(FFT_SIZE * 2, 0.0f);
            }
        }

        // Lock-free write (single producer, consumer copies under lock)
        // Use unfiltered LMS output for spectrum display (show full bandwidth)
        size_t pos = iqBufferWritePos_.load(std::memory_order_relaxed);
        iqBuffer_[pos * 2] = out_i;
        iqBuffer_[pos * 2 + 1] = out_q;
        iqBufferWritePos_.store((pos + 1) % FFT_SIZE, std::memory_order_release);

        // Demodulate using LMS output (best image rejection for audio)
        float audio = demod_.process(audio_i, audio_q);

        // Write to audio buffer, decimating from 96kHz to 48kHz (every other sample)
        // RateAdaptiveBuffer handles overflow gracefully (drops evenly)
        if (!audioDecimateSkip_) {
            audioBuffer_.write(audio);
            audioWriteCount++;  // Count actual writes

            // Capture audio samples for debugging (first 5 seconds at 48kHz)
            // Apply gain to match what you hear (otherwise values are ~1e-6)
            if (audioCaptureBuffer_.size() < 48000 * 5) {
                constexpr float captureGain = 500000.0f;
                float volume = audioVolume_.load(std::memory_order_relaxed);
                float scaledAudio = std::tanh(audio * captureGain * volume);
                audioCaptureBuffer_.push_back(scaledAudio);
            }

            // WAV recording (controllable, up to 60 seconds)
            // Apply same gain as playback so recorded audio matches what you hear
            if (wavRecording_ && wavBuffer_.size() < wavMaxSamples_) {
                constexpr float recordGain = 500000.0f;  // Match playback gain
                float volume = audioVolume_.load(std::memory_order_relaxed);
                float scaledAudio = std::tanh(audio * recordGain * volume);  // With soft clip
                wavBuffer_.push_back(scaledAudio);
                // Debug: print first few samples
                if (wavBuffer_.size() <= 10) {
                    std::cout << "[WAV] sample " << wavBuffer_.size()
                              << ": raw=" << audio << " scaled=" << scaledAudio << std::endl;
                }
            }
        }
        audioDecimateSkip_ = !audioDecimateSkip_;
    }

    void saveAudioCapture() {
        if (audioCaptureBuffer_.empty()) return;

        // Write as raw 32-bit float PCM for easy analysis
        std::ofstream file("/tmp/audio_capture.raw", std::ios::binary);
        if (file) {
            file.write(reinterpret_cast<const char*>(audioCaptureBuffer_.data()),
                       audioCaptureBuffer_.size() * sizeof(float));
            std::cout << "[Debug] Saved " << audioCaptureBuffer_.size()
                      << " audio samples to /tmp/audio_capture.raw (48kHz float32)" << std::endl;

            // Also print stats
            float minVal = *std::min_element(audioCaptureBuffer_.begin(), audioCaptureBuffer_.end());
            float maxVal = *std::max_element(audioCaptureBuffer_.begin(), audioCaptureBuffer_.end());
            float sum = 0, sumSq = 0;
            for (float s : audioCaptureBuffer_) {
                sum += s;
                sumSq += s * s;
            }
            float mean = sum / audioCaptureBuffer_.size();
            float rms = std::sqrt(sumSq / audioCaptureBuffer_.size());
            std::cout << "[Debug] Audio stats: min=" << minVal << " max=" << maxVal
                      << " mean=" << mean << " rms=" << rms << std::endl;
        }
    }

    // WAV recording control
    void startWavRecording(const std::string& path = "") {
        if (!path.empty()) {
            wavFilePath_ = path;
        }
        wavBuffer_.clear();
        wavBuffer_.reserve(wavMaxSamples_);
        wavRecording_ = true;
        std::cout << "[Audio] Started WAV recording to " << wavFilePath_ << std::endl;
    }

    void stopWavRecording() {
        if (!wavRecording_) return;
        wavRecording_ = false;

        if (wavBuffer_.empty()) {
            std::cout << "[Audio] No samples recorded" << std::endl;
            return;
        }

        // Write WAV file
        saveWavFile(wavFilePath_, wavBuffer_, 48000);
    }

    bool isWavRecording() const { return wavRecording_; }
    size_t getWavSampleCount() const { return wavBuffer_.size(); }

    void saveWavFile(const std::string& path, const std::vector<float>& samples, int sampleRate) {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            std::cerr << "[Audio] Failed to open " << path << " for writing" << std::endl;
            return;
        }

        // Convert float samples to 16-bit PCM
        std::vector<int16_t> pcmData(samples.size());
        for (size_t i = 0; i < samples.size(); ++i) {
            float s = std::clamp(samples[i], -1.0f, 1.0f);
            pcmData[i] = static_cast<int16_t>(s * 32767.0f);
        }

        // WAV header
        uint32_t dataSize = pcmData.size() * sizeof(int16_t);
        uint32_t fileSize = 36 + dataSize;
        uint16_t numChannels = 1;
        uint16_t bitsPerSample = 16;
        uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
        uint16_t blockAlign = numChannels * bitsPerSample / 8;

        // RIFF header
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char*>(&fileSize), 4);
        file.write("WAVE", 4);

        // fmt chunk
        file.write("fmt ", 4);
        uint32_t fmtSize = 16;
        file.write(reinterpret_cast<const char*>(&fmtSize), 4);
        uint16_t audioFormat = 1;  // PCM
        file.write(reinterpret_cast<const char*>(&audioFormat), 2);
        file.write(reinterpret_cast<const char*>(&numChannels), 2);
        file.write(reinterpret_cast<const char*>(&sampleRate), 4);
        file.write(reinterpret_cast<const char*>(&byteRate), 4);
        file.write(reinterpret_cast<const char*>(&blockAlign), 2);
        file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

        // data chunk
        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&dataSize), 4);
        file.write(reinterpret_cast<const char*>(pcmData.data()), dataSize);

        float duration = static_cast<float>(samples.size()) / sampleRate;
        std::cout << "[Audio] Saved " << path << " (" << samples.size()
                  << " samples, " << std::fixed << std::setprecision(2)
                  << duration << "s)" << std::endl;

        // Print audio stats
        float minVal = *std::min_element(samples.begin(), samples.end());
        float maxVal = *std::max_element(samples.begin(), samples.end());
        float sumSq = 0;
        for (float s : samples) sumSq += s * s;
        float rms = std::sqrt(sumSq / samples.size());
        std::cout << "[Audio] Stats: min=" << minVal << " max=" << maxVal
                  << " rms=" << rms << std::endl;
    }

    // In-place radix-2 Cooley-Tukey FFT (O(N log N) instead of O(N²))
    // Much faster than DFT: ~10,000 ops vs ~1,000,000 ops for N=1024
    void fftInPlace(float* re, float* im, size_t n) {
        // Bit-reversal permutation
        for (size_t i = 1, j = 0; i < n; ++i) {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) {
                j ^= bit;
            }
            j ^= bit;
            if (i < j) {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }

        // Cooley-Tukey iterative FFT
        for (size_t len = 2; len <= n; len <<= 1) {
            float angle = -2.0f * 3.14159265f / len;
            float wRe = std::cos(angle);
            float wIm = std::sin(angle);

            for (size_t i = 0; i < n; i += len) {
                float curRe = 1.0f, curIm = 0.0f;

                for (size_t j = 0; j < len / 2; ++j) {
                    size_t u = i + j;
                    size_t v = i + j + len / 2;

                    // Butterfly: (u, v) = (u + w*v, u - w*v)
                    float tRe = curRe * re[v] - curIm * im[v];
                    float tIm = curRe * im[v] + curIm * re[v];

                    re[v] = re[u] - tRe;
                    im[v] = im[u] - tIm;
                    re[u] = re[u] + tRe;
                    im[u] = im[u] + tIm;

                    // Advance twiddle: cur *= w
                    float nextRe = curRe * wRe - curIm * wIm;
                    float nextIm = curRe * wIm + curIm * wRe;
                    curRe = nextRe;
                    curIm = nextIm;
                }
            }
        }
    }

    // Compute spectrum using fast FFT with exponential averaging
    void computeSpectrum() {
        // Pre-compute Hann window (reduces spectral leakage)
        static std::vector<float> window;
        if (window.size() != FFT_SIZE) {
            window.resize(FFT_SIZE);
            for (size_t n = 0; n < FFT_SIZE; ++n) {
                window[n] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * n / (FFT_SIZE - 1)));
            }
        }

        // Buffers for FFT (reused across calls)
        static std::vector<float> fftRe, fftIm;
        fftRe.resize(FFT_SIZE);
        fftIm.resize(FFT_SIZE);

        // Averaged spectrum state (persistent across calls)
        // Exponential moving average reduces noise floor variance from single-frame FFT
        static std::vector<float> avgSpectrum;
        static bool avgInitialized = false;

        // Copy IQ data under lock
        {
            std::lock_guard<std::mutex> lock(spectrumMutex_);
            if (iqBuffer_.size() < FFT_SIZE * 2) return;

            // Skip ahead a few samples from write position to avoid race condition.
            // The producer writes at writePos without holding the lock, so we need
            // to avoid reading samples that might be getting updated.
            // Skip 8 samples (~83µs at 96kHz) - enough margin for timing jitter.
            constexpr size_t SKIP_SAMPLES = 8;
            size_t writePos = iqBufferWritePos_.load(std::memory_order_acquire);
            size_t readStart = (writePos + SKIP_SAMPLES) % FFT_SIZE;

            for (size_t n = 0; n < FFT_SIZE; ++n) {
                size_t idx = (readStart + n) % FFT_SIZE;
                fftRe[n] = iqBuffer_[idx * 2] * window[n];
                fftIm[n] = iqBuffer_[idx * 2 + 1] * window[n];
            }
        }

        // Fast FFT (O(N log N) ~= 10,000 ops vs O(N²) = 1,000,000 ops)
        fftInPlace(fftRe.data(), fftIm.data(), FFT_SIZE);

        // Convert to dB magnitude spectrum
        std::vector<float> localSpectrum(FFT_SIZE);
        for (size_t k = 0; k < FFT_SIZE; ++k) {
            float mag = std::sqrt(fftRe[k] * fftRe[k] + fftIm[k] * fftIm[k]) / FFT_SIZE * 2.0f;
            float db = (mag > 1e-10f) ? 20.0f * std::log10(mag) : -100.0f;

            // FFT shift: put DC in center
            size_t outIdx = (k + FFT_SIZE / 2) % FFT_SIZE;
            localSpectrum[outIdx] = db;
        }

        // Apply exponential moving average (EMA) to reduce noise speckle
        // Single-frame FFT of noise has high variance (chi-squared distribution).
        // Averaging N frames reduces variance by factor of N.
        // EMA: avg[k] = alpha * new[k] + (1-alpha) * avg[k]
        // Alpha = 0.3 gives ~3 frame effective averaging (fast response, moderate smoothing)
        constexpr float alpha = 0.3f;

        if (!avgInitialized || avgSpectrum.size() != FFT_SIZE) {
            avgSpectrum = localSpectrum;  // First frame: no averaging
            avgInitialized = true;
        } else {
            for (size_t k = 0; k < FFT_SIZE; ++k) {
                avgSpectrum[k] = alpha * localSpectrum[k] + (1.0f - alpha) * avgSpectrum[k];
            }
        }

        // Brief lock to update output
        {
            std::lock_guard<std::mutex> lock(spectrumMutex_);
            spectrumData_ = avgSpectrum;
        }
    }
};

int main(int argc, char* argv[]) {
    // Enable flush-to-zero and denormals-are-zero on x86
    // This prevents denormalized floats from causing artifacts in DSP processing
    // when dealing with weak signals (high attenuation)
#ifdef HAVE_SSE_DENORMAL_CONTROL
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif

    // Parse command-line arguments
    bool disableVsync = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-vsync" || arg == "-n") {
            disableVsync = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: nexrx_app [options]\n"
                      << "Options:\n"
                      << "  --no-vsync, -n   Disable VSync (reduces input latency, may improve VM audio)\n"
                      << "  --help, -h       Show this help\n";
            return 0;
        }
    }

#ifndef __APPLE__
    (void)argv;
#endif

    std::cout << "NexRx Application Starting..." << std::endl;

#ifdef __APPLE__
    // On macOS, if running from an app bundle, change to Resources directory
    // so relative paths to config/ and lua/ work correctly
    if (argv[0]) {
        std::string exePath(argv[0]);
        auto pos = exePath.find(".app/Contents/MacOS/");
        if (pos != std::string::npos) {
            std::string resourcesPath = exePath.substr(0, pos) + ".app/Contents/Resources";
            if (chdir(resourcesPath.c_str()) == 0) {
                std::cout << "Changed to Resources directory: " << resourcesPath << std::endl;
            }
        }
    }
#endif

#ifdef _WIN32
    // Initialize Winsock on Windows
    nexrx::net::WinsockInit winsock;
    if (!winsock.ok()) {
        std::cerr << "Failed to initialize Winsock" << std::endl;
        return 1;
    }
#endif

    App app;
    gApp = &app;

    if (!app.init("NexRx", !disableVsync)) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }

    std::cout << "Running main loop (Ctrl+Q to quit)..." << std::endl;
    app.run();

    std::cout << "Shutting down..." << std::endl;
    gApp = nullptr;

    return 0;
}
