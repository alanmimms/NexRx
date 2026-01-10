/**
 * @file main.cpp
 * @brief NexRx Application - SDL2/OpenGL with Lua GUI
 */

#include "setbox/SetBox.hpp"
#include "FontRenderer.hpp"
#include "AudioEngine.hpp"
#include "WaterfallRenderer.hpp"

// Twin integration
#ifdef NEXRX_REMOTE_TWIN
    // Windows/macOS: connect to twin over network
    #include "net/Socket.hpp"
    #include "twin/HostApp.hpp"
#else
    // Linux: twin runs locally
    #include "host/HostApp.hpp"
#endif
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

#ifdef __APPLE__
#include <unistd.h>  // for chdir()
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

    bool init(int width, int height, const std::string& title) {
        // Initialize SDL
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

        // Create window
        window_ = SDL_CreateWindow(
            title.c_str(),
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
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

        // VSync
        SDL_GL_SetSwapInterval(1);

        // Get actual window size
        SDL_GetWindowSize(window_, &windowWidth_, &windowHeight_);

        // Setup OpenGL state
        glViewport(0, 0, windowWidth_, windowHeight_);
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

        // Enable blending for UI
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Initialize font renderer
        // Try common font paths (platform-specific)
        const char* fontPaths[] = {
#ifdef _WIN32
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/tahoma.ttf",
#elif defined(__APPLE__)
            "/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/Helvetica.ttc",
            "/Library/Fonts/Arial.ttf",
#else
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
#endif
            "fonts/DejaVuSans.ttf",
            nullptr
        };

        for (const char** path = fontPaths; *path; ++path) {
            if (font_.loadFont(*path, 16.0f)) {
                std::cout << "Loaded font: " << *path << std::endl;
                break;
            }
        }

        if (!font_.isLoaded()) {
            std::cerr << "Warning: Could not load any font" << std::endl;
        }

        // Initialize audio engine
        if (!audio_.init(48000, 2)) {
            std::cerr << "Warning: Could not initialize audio" << std::endl;
        }

        // Initialize audio ring buffer and set RX audio callback
        audioRingBuffer_.resize(AUDIO_BUFFER_SIZE, 0.0f);
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

            // Get current buffer state once (more efficient than per-sample atomics)
            size_t readPos = audioReadPos_.load(std::memory_order_acquire);
            size_t writePos = audioWritePos_.load(std::memory_order_acquire);

            for (uint32_t i = 0; i < frameCount; ++i) {
                float sample = 0.0f;

                if (readPos != writePos) {
                    sample = audioRingBuffer_[readPos] * audioGain * volume;
                    readPos = (readPos + 1) % AUDIO_BUFFER_SIZE;
                    audioSamplesRead_.fetch_add(1, std::memory_order_relaxed);
                } else {
                    // Buffer underrun - count it
                    audioUnderruns_.fetch_add(1, std::memory_order_relaxed);
                }

                // Soft clip (only activates on very loud signals)
                sample = std::tanh(sample);

                // Output to both channels (mono to stereo)
                output[i * channels] = sample;
                if (channels > 1) {
                    output[i * channels + 1] = sample;
                }
            }

            // Update read position once at end
            audioReadPos_.store(readPos, std::memory_order_release);
        });

        // Initialize Lua
        if (!initLua()) {
            return false;
        }

        running_ = true;
        return true;
    }

    void shutdown() {
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
        while (running_) {
            input_.beginFrame();
            pollEvents();

            // Update timing
            uint32_t now = SDL_GetTicks();
            float dt = (now - lastFrameTime_) / 1000.0f;
            lastFrameTime_ = now;

            // Call Lua update
            callLuaUpdate(dt);

            // Clear and draw
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Set up 2D projection
            setup2DProjection();

            // Call Lua draw
            callLuaDraw();

            SDL_GL_SwapWindow(window_);
        }
    }

    void quit() { running_ = false; }

    // Accessors for Lua
    int windowWidth() const { return windowWidth_; }
    int windowHeight() const { return windowHeight_; }
    const InputState& input() const { return input_; }
    NexRx::SetBox::SetBoxEngine& setbox() { return setbox_; }

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
    bool initLua() {
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

        // Expose SetBox engine
        lua_["setbox"] = lua_.create_table();
        lua_["setbox"]["setActiveTags"] = [this](sol::table tags) {
            NexRx::SetBox::TagSet tagSet;
            for (auto& [k, v] : tags) {
                if (v.is<std::string>()) {
                    tagSet.add(v.as<std::string>());
                }
            }
            setbox_.setActiveTags(tagSet);
        };

        lua_["setbox"]["addTag"] = [this](const std::string& tag) {
            setbox_.addTag(tag);
        };

        lua_["setbox"]["removeTag"] = [this](const std::string& tag) {
            setbox_.removeTag(tag);
        };

        lua_["setbox"]["getString"] = [this](const std::string& name, const std::string& defaultVal) {
            return setbox_.getString(name, defaultVal);
        };

        lua_["setbox"]["getNumber"] = [this](const std::string& name, double defaultVal) {
            return setbox_.getNumber(name, defaultVal);
        };

        lua_["setbox"]["getBool"] = [this](const std::string& name, bool defaultVal) {
            return setbox_.getBool(name, defaultVal);
        };

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

        lua_["waterfall"]["setColormap"] = [this](const std::string& name) {
            if (name == "viridis") waterfall_.setColormap(WaterfallColormap::Viridis);
            else if (name == "plasma") waterfall_.setColormap(WaterfallColormap::Plasma);
            else if (name == "inferno") waterfall_.setColormap(WaterfallColormap::Inferno);
            else if (name == "magma") waterfall_.setColormap(WaterfallColormap::Magma);
            else if (name == "green") waterfall_.setColormap(WaterfallColormap::GreenPhosphor);
            else if (name == "blue") waterfall_.setColormap(WaterfallColormap::BlueHot);
            else if (name == "grayscale") waterfall_.setColormap(WaterfallColormap::Grayscale);
        };

        lua_["waterfall"]["setRange"] = [this](float minDb, float maxDb) {
            waterfall_.setRange(minDb, maxDb);
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

        // Expose twin connection to Lua
        lua_["twin"] = lua_.create_table();

        lua_["twin"]["connect"] = [this](
            sol::optional<std::string> host,
            sol::optional<int> controlPort,
            sol::optional<int> streamPort)
        {
            nexrx::HostConfig config;
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

                if (twinHost_.startReceiving()) {
                    twinConnected_ = true;
                    std::cout << "[Twin] Connected" << std::endl;
                    return true;
                }
            }
            std::cerr << "[Twin] Failed to connect" << std::endl;
            return false;
        };

        lua_["twin"]["disconnect"] = [this]() {
            twinHost_.stopReceiving();
            twinHost_.shutdown();
            twinConnected_ = false;
            std::cout << "[Twin] Disconnected" << std::endl;
        };

        lua_["twin"]["isConnected"] = [this]() {
            return twinConnected_ && twinHost_.isConnected();
        };

        lua_["twin"]["getFramesReceived"] = [this]() {
            return static_cast<double>(twinHost_.framesReceived());
        };

        lua_["twin"]["getFramesDropped"] = [this]() {
            return static_cast<double>(twinHost_.framesDropped());
        };

        lua_["twin"]["getSpectrum"] = [this](sol::this_state s) {
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

        lua_["twin"]["poll"] = [this]() {
            // Poll for frames if not using background thread
            return twinHost_.pollFrames(100);
        };

        // Expose receiver controls to Lua
        lua_["rx"] = lua_.create_table();

        lua_["rx"]["setMode"] = [this](const std::string& mode) {
            if (mode == "usb" || mode == "USB") {
                demod_.setMode(Demodulator::Mode::USB);
            } else if (mode == "lsb" || mode == "LSB") {
                demod_.setMode(Demodulator::Mode::LSB);
            } else if (mode == "am" || mode == "AM") {
                demod_.setMode(Demodulator::Mode::AM);
            } else if (mode == "cw" || mode == "CW") {
                demod_.setMode(Demodulator::Mode::CW);
            }
        };

        lua_["rx"]["getMode"] = [this]() {
            switch (demod_.getMode()) {
                case Demodulator::Mode::USB: return std::string("USB");
                case Demodulator::Mode::LSB: return std::string("LSB");
                case Demodulator::Mode::AM:  return std::string("AM");
                case Demodulator::Mode::CW:  return std::string("CW");
                default: return std::string("USB");
            }
        };

        lua_["rx"]["setBfo"] = [this](float hz) {
            demod_.setBfoOffset(hz);
        };

        lua_["rx"]["getBfo"] = [this]() {
            return demod_.getBfoOffset();
        };

        lua_["rx"]["getAudioStats"] = [this]() {
            // Return audio buffer stats for debugging
            // Returns: written, read, underruns
            return std::make_tuple(
                static_cast<double>(audioSamplesWritten_.load()),
                static_cast<double>(audioSamplesRead_.load()),
                static_cast<double>(audioUnderruns_.load())
            );
        };

        lua_["rx"]["setVfo"] = [this](double freq_hz) {
            // Send VFO frequency via TCP control to twin
            if (twinConnected_) {
                twinHost_.setLO(freq_hz);
            }
        };

        lua_["rx"]["getSignalLevel"] = [this]() {
            // Return signal level: RMS voltage, dBm, S-units
            float rms = signalLevelRms_.load(std::memory_order_relaxed);

            // Convert to dBm (assuming 50 ohm reference)
            // P = V^2 / R, dBm = 10*log10(P / 1mW)
            // dBm = 10*log10(V^2 / 50 / 0.001) = 20*log10(V) + 10*log10(1/0.05) = 20*log10(V) + 13
            float dBm = -120.0f;  // Floor
            if (rms > 1e-9f) {
                dBm = 20.0f * std::log10(rms) + 13.0f;
            }

            // S-meter: S9 = -73 dBm, each S-unit = 6 dB
            // S = (dBm + 73) / 6 + 9, clamped to 0-9+
            float sUnits = (dBm + 73.0f) / 6.0f + 9.0f;
            sUnits = std::max(0.0f, sUnits);

            // Also return dB over S9 for readings above S9
            float dBoverS9 = dBm + 73.0f;

            return std::make_tuple(rms, dBm, sUnits, dBoverS9);
        };

        // Load SetBox base config
        if (!setbox_.loadFile("config/base/defaults.lua")) {
            std::cerr << "Warning: Failed to load defaults.lua: " << setbox_.lastError() << std::endl;
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
                    // ESC to quit (only if not in text entry - let Lua handle it)
                    // if (event.key.keysym.sym == SDLK_ESCAPE) {
                    //     running_ = false;
                    // }
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
    NexRx::SetBox::SetBoxEngine setbox_;
    FontRenderer font_;
    AudioEngine audio_;
    WaterfallRenderer waterfall_;

    // Twin integration
    nexrx::HostApp twinHost_;
    std::mutex spectrumMutex_;
    std::vector<float> spectrumData_;      // Latest spectrum (dB values)
    std::vector<float> iqBuffer_;          // Ring buffer for FFT
    size_t iqBufferWritePos_ = 0;
    static constexpr size_t FFT_SIZE = 1024;
    bool twinConnected_ = false;

    // Demodulator and audio output
    Demodulator demod_;
    std::vector<float> audioRingBuffer_;
    std::atomic<size_t> audioWritePos_{0};
    std::atomic<size_t> audioReadPos_{0};
    std::atomic<float> audioVolume_{0.0316f};  // Volume applied before soft-clip
    static constexpr size_t AUDIO_BUFFER_SIZE = 16384;  // ~340ms at 48kHz
    bool audioDecimateSkip_ = false;  // For 96kHz→48kHz decimation
    std::atomic<uint64_t> audioSamplesWritten_{0};
    std::atomic<uint64_t> audioSamplesRead_{0};
    std::atomic<uint64_t> audioUnderruns_{0};

    // Signal level metering (S-meter)
    std::atomic<float> signalLevelRms_{0.0f};    // RMS voltage level
    float signalAccumulator_ = 0.0f;             // Sum of squared magnitudes
    size_t signalSampleCount_ = 0;
    static constexpr size_t SIGNAL_AVG_SAMPLES = 4800;  // ~50ms at 96kHz

    int windowWidth_ = 0;
    int windowHeight_ = 0;
    bool running_ = false;
    uint32_t lastFrameTime_ = 0;

    InputState input_;

    // Twin I/Q processing
    void processIQFrame(const nexrx::IQFrame& frame) {
        // Extract I/Q from QSD0 (first channel) and convert to float
        float i_f, q_f;
        frame.qsd[0].toFloat(i_f, q_f);

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

        // Add to spectrum ring buffer (with lock)
        {
            std::lock_guard<std::mutex> lock(spectrumMutex_);
            if (iqBuffer_.size() < FFT_SIZE * 2) {
                iqBuffer_.resize(FFT_SIZE * 2, 0.0f);
            }

            iqBuffer_[iqBufferWritePos_ * 2] = i_f;
            iqBuffer_[iqBufferWritePos_ * 2 + 1] = q_f;
            iqBufferWritePos_ = (iqBufferWritePos_ + 1) % FFT_SIZE;
        }

        // Demodulate EVERY sample (filter needs continuous input for proper state)
        float audio = demod_.process(i_f, q_f);

        // Write to audio buffer, decimating from 96kHz to 48kHz (every other sample)
        if (!audioDecimateSkip_) {
            // Write to audio ring buffer
            size_t writePos = audioWritePos_.load(std::memory_order_relaxed);
            size_t nextPos = (writePos + 1) % AUDIO_BUFFER_SIZE;
            size_t readPos = audioReadPos_.load(std::memory_order_acquire);

            // Only write if buffer not full
            if (nextPos != readPos) {
                audioRingBuffer_[writePos] = audio;
                audioWritePos_.store(nextPos, std::memory_order_release);
                audioSamplesWritten_.fetch_add(1, std::memory_order_relaxed);
            }
        }
        audioDecimateSkip_ = !audioDecimateSkip_;
    }

    // Simple DFT for spectrum (replace with FFT library for performance)
    void computeSpectrum() {
        std::lock_guard<std::mutex> lock(spectrumMutex_);

        if (iqBuffer_.size() < FFT_SIZE * 2) return;

        spectrumData_.resize(FFT_SIZE);

        // Pre-compute Hann window (reduces spectral leakage)
        static std::vector<float> window;
        if (window.size() != FFT_SIZE) {
            window.resize(FFT_SIZE);
            for (size_t n = 0; n < FFT_SIZE; ++n) {
                window[n] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * n / (FFT_SIZE - 1)));
            }
        }

        // Simple magnitude spectrum via DFT
        // For real performance, use FFTW or similar
        for (size_t k = 0; k < FFT_SIZE; ++k) {
            float re = 0.0f, im = 0.0f;

            for (size_t n = 0; n < FFT_SIZE; ++n) {
                size_t idx = (iqBufferWritePos_ + n) % FFT_SIZE;
                float i_val = iqBuffer_[idx * 2] * window[n];
                float q_val = iqBuffer_[idx * 2 + 1] * window[n];

                // Complex input: i + j*q
                float angle = -2.0f * 3.14159265f * k * n / FFT_SIZE;
                float cos_a = std::cos(angle);
                float sin_a = std::sin(angle);

                // (i + jq) * (cos - j*sin) = i*cos + q*sin + j(q*cos - i*sin)
                re += i_val * cos_a + q_val * sin_a;
                im += q_val * cos_a - i_val * sin_a;
            }

            // Magnitude in dB (compensate for Hann window coherent gain of 0.5)
            float mag = std::sqrt(re * re + im * im) / FFT_SIZE * 2.0f;
            float db = (mag > 1e-10f) ? 20.0f * std::log10(mag) : -100.0f;

            // FFT shift: put DC in center
            size_t outIdx = (k + FFT_SIZE / 2) % FFT_SIZE;
            spectrumData_[outIdx] = db;
        }
    }
};

int main(int argc, char* argv[]) {
    (void)argc;
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

    if (!app.init(1280, 720, "NexRx")) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }

    std::cout << "Running main loop (ESC to quit)..." << std::endl;
    app.run();

    std::cout << "Shutting down..." << std::endl;
    gApp = nullptr;

    return 0;
}
