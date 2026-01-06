/**
 * @file main.cpp
 * @brief NexRx Application - SDL2/OpenGL with Lua GUI
 */

#include "setbox/SetBox.hpp"
#include "FontRenderer.hpp"
#include "AudioEngine.hpp"

#include <SDL.h>
#include <SDL_opengl.h>

#include <sol/sol.hpp>

#include <iostream>
#include <string>
#include <cstdlib>
#include <cmath>

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
        // Try common font paths
        const char* fontPaths[] = {
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSans.ttf",
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

        // Initialize Lua
        if (!initLua()) {
            return false;
        }

        running_ = true;
        return true;
    }

    void shutdown() {
        // Shutdown audio first
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
        const int cornerSegments = 8;

        glBegin(GL_TRIANGLE_FAN);
        // Center
        glVertex2f(x + w/2, y + h/2);

        // Top-left corner
        for (int i = cornerSegments; i >= 0; --i) {
            float angle = 3.14159265f/2.0f + (3.14159265f/2.0f) * i / cornerSegments;
            glVertex2f(x + radius + std::cos(angle) * radius, y + radius + std::sin(angle) * radius);
        }

        // Top-right corner
        for (int i = cornerSegments; i >= 0; --i) {
            float angle = (3.14159265f/2.0f) * i / cornerSegments;
            glVertex2f(x + w - radius + std::cos(angle) * radius, y + radius + std::sin(angle) * radius);
        }

        // Bottom-right corner
        for (int i = cornerSegments; i >= 0; --i) {
            float angle = -((3.14159265f/2.0f) * i / cornerSegments);
            glVertex2f(x + w - radius + std::cos(angle) * radius, y + h - radius + std::sin(angle) * radius);
        }

        // Bottom-left corner
        for (int i = cornerSegments; i >= 0; --i) {
            float angle = 3.14159265f + (3.14159265f/2.0f) * i / cornerSegments;
            glVertex2f(x + radius + std::cos(angle) * radius, y + h - radius + std::sin(angle) * radius);
        }

        // Close back to top-left
        glVertex2f(x, y + radius);

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
            audio_.setVolume(volume);
        };

        lua_["audio"]["getVolume"] = [this]() {
            return audio_.getVolume();
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
                    // ESC to quit
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running_ = false;
                    }
                    break;

                case SDL_KEYUP:
                    if (event.key.keysym.scancode < 512) {
                        input_.keyDown[event.key.keysym.scancode] = false;
                        input_.keyReleased[event.key.keysym.scancode] = true;
                    }
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

    int windowWidth_ = 0;
    int windowHeight_ = 0;
    bool running_ = false;
    uint32_t lastFrameTime_ = 0;

    InputState input_;
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "NexRx Application Starting..." << std::endl;

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
