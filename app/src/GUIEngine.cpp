#include "GUIEngine.hpp"
#include "AppLuaBridge.hpp"
#include <iostream>
#include <csignal>
#include <cbor.h>

GUIEngine::GUIEngine(DSPEngine& dsp) : dsp_(dsp) {}

GUIEngine::~GUIEngine() {
  shutdown();
}

bool GUIEngine::init(const std::string& title, bool vsyncEnabled) {
  // 1. Raylib Setup
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | (vsyncEnabled ? FLAG_VSYNC_HINT : 0));
  InitWindow(1280, 850, title.c_str());
  SetTargetFPS(60);

  // 2. Load Font
  Font font = LoadFontEx("fonts/DejaVuSans.ttf", 32, NULL, 0);
  if (font.texture.id == 0) font = LoadFontEx("../fonts/DejaVuSans.ttf", 32, NULL, 0);
  if (font.texture.id == 0) font = LoadFontEx("uitest/fonts/DejaVuSans.ttf", 32, NULL, 0);
  if (font.texture.id == 0) font = LoadFontEx("../uitest/fonts/DejaVuSans.ttf", 32, NULL, 0);
  
  if (font.texture.id != 0) {
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    AppLuaBridge::setFont(font);
  }

  // 3. Audio & Waterfall init
  if (!audio.init(48000, 2)) return false;
  if (!waterfall.init(DSPEngine::FFT_SIZE, 256)) return false;

  // Configure audio buffer for low latency
  BufferConfig audioConfig;
  audioConfig.capacity = 8192;
  audioConfig.targetFillRatio = 0.2f;
  audioConfig.lowThreshold = 0.1f;
  audioConfig.highThreshold = 0.4f;
  audioConfig.enableAdaptation = true;
  dsp_.getAudioBuffer().configure(audioConfig);

  audio.setCallback([this](float* out, uint32_t fC, uint32_t ch) {
    thread_local std::vector<float> tmp;
    tmp.resize(fC);
    size_t read = dsp_.getAudioBuffer().read(std::span<float>(tmp.data(), fC));
    for (uint32_t i = 0; i < fC; ++i) {
      float s = (i < read) ? tmp[i] : 0.0f;
      for (uint32_t c = 0; c < ch; ++c) out[i*ch + c] = s;
    }
  });

  // 4. Lua Setup
  lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math, sol::lib::os, sol::lib::debug, sol::lib::io);
  lua["basePath"] = "lua/";
  lua["package"]["path"] = "lua/?.lua;lua/?/init.lua";
  
  AppLuaBridge::registerWithLua(lua, this);

  try {
    // Load SetBox first as it's a core dependency
    lua.safe_script_file("lua/SetBox.lua");
    
    // Load the main entry point
    lua.safe_script_file("lua/Main.lua");
    
    // In our new architecture, Main.lua should return the UI module with standard hooks
    uiModule = lua["UI"]; 
  } catch (const sol::error& e) {
    std::cerr << "Lua load error: " << e.what() << std::endl;
    return false;
  }

  // 5. App Initialization
  sol::protected_function initFn = lua["init"];
  if (initFn.valid()) {
    auto res = initFn();
    if (!res.valid()) { sol::error err = res; std::cerr << "Lua init() error: " << err.what() << std::endl; return false; }
  }

  startCommandThread();
  running = true;
  return true;
}

extern std::atomic<bool> gRunning;

void GUIEngine::run() {
  while (running && !WindowShouldClose() && gRunning.load()) {
    float dt = GetFrameTime();
    update(dt);
    render();
  }
}

void GUIEngine::update(float dt) {
  // 1. Poll input and dispatch to Lua UI if needed
  if (uiModule != sol::nil) {
    Vector2 mousePos = GetMousePosition();
    
    // Mouse Buttons & Wheel
    sol::function onMouseEvent = uiModule["onMouseEvent"];
    if (onMouseEvent.valid()) {
        int mods = 0;
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) mods |= 1;
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) mods |= 2;
        if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) mods |= 4;

        // Motion dispatch (ensure mouse coordinates are updated)
        onMouseEvent("motion", mousePos.x, mousePos.y, 0, false, mods);

        for (int b = 0; b < 3; b++) {
            if (IsMouseButtonPressed(b)) onMouseEvent("button", mousePos.x, mousePos.y, b, true, mods);
            else if (IsMouseButtonReleased(b)) onMouseEvent("button", mousePos.x, mousePos.y, b, false, mods);
        }

        // Mouse Wheel
        float wheel = GetMouseWheelMove();
        if (wheel != 0) onMouseEvent("wheel", mousePos.x, mousePos.y, (int)wheel, false, mods);
    }

    // Keys (Simplified: Raylib doesn't have a clean event queue for releases/repeats without more work)
    sol::function onKeyEvent = uiModule["onKeyEvent"];
    if (onKeyEvent.valid()) {
        // Handle character input (digits, letters, etc)
        int charCode = GetCharPressed();
        while (charCode > 0) {
            sol::function onTextInput = uiModule["onTextInput"];
            if (onTextInput.valid()) {
                char text[2] = {(char)charCode, 0};
                onTextInput(text);
            }
            charCode = GetCharPressed();
        }

        // Handle special keys (Arrow keys, ENTER, BACKSPACE, ESC)
        int keys[] = {KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN, KEY_ENTER, KEY_BACKSPACE, KEY_ESCAPE, KEY_F};
        for (int k : keys) {
            if (IsKeyPressed(k)) {
                // std::cout << "[C++] Key Pressed: " << k << std::endl;
                onKeyEvent(k, true, 0);
            }
            else if (IsKeyReleased(k)) {
                onKeyEvent(k, false, 0);
            }
        }
    }
  }

  // 2. Lua update hook
  sol::protected_function updateFn = lua["update"];
  if (updateFn.valid()) { 
    auto res = updateFn(dt); 
    if (!res.valid()) { sol::error err = res; std::cerr << "Lua update() error: " << err.what() << std::endl; running = false; } 
  }
}

void GUIEngine::render() {
  if (IsWindowResized() && uiModule != sol::nil) {
    sol::function resizeFunc = uiModule["onResize"];
    if (resizeFunc.valid()) resizeFunc(GetScreenWidth(), GetScreenHeight());
  }

  BeginDrawing();
  ClearBackground(BLACK);

  if (uiModule != sol::nil) {
    sol::function renderFunc = uiModule["render"];
    if (renderFunc.valid()) {
      try {
        renderFunc(GetScreenWidth(), GetScreenHeight());
      } catch (const sol::error& e) {
        std::cerr << "Lua render error: " << e.what() << std::endl;
      }
    }
  }

  EndDrawing();
}

void GUIEngine::shutdown() {
  running = false;
  stopCommandThread();
  if (radioSource) {
    radioSource->shutdown();
  }
  audio.shutdown();
  waterfall.shutdown();
  CloseWindow();
}

bool GUIEngine::connectTwin(const std::string& host, int cp, int sp) {
  nexrx::TwinConfig c; c.host = host; c.controlPort = cp; c.streamPort = sp;
  auto twin = std::make_unique<nexrx::TwinConn>();
  if (twin->initialize(c)) {
    twin->setFrameCallback([this](const nexrx::IQFrame& f) { dsp_.processIQFrame(f); });
    if (twin->startReceiving()) {
      twinConnected.store(true);
      twin->startStream();
      radioSource = std::move(twin);
      return true;
    }
  }
  return false;
}

void GUIEngine::disconnectTwin() {
  if (twinConnected.load() && radioSource) {
    postTwinCommand("SHUTDOWN", [this]() { 
      radioSource->stopReceiving(); 
      radioSource->shutdown(); 
      radioSource.reset();
    });
    twinConnected.store(false);
  }
}

sol::object GUIEngine::getTwinState(sol::this_state s) {
  if (!twinConnected.load()) return sol::make_object(s, sol::nil);
  auto src = getRadioSource();
  if (!src) return sol::make_object(s, sol::nil);
  
  auto now = std::chrono::steady_clock::now();
  if (now - lastStatePollTime > std::chrono::milliseconds(100)) {
    lastStatePollTime = now;
    postTwinCommand("POLL_STATE", [this](){ 
      if (auto src = getRadioSource()) src->pollStateAsync(); 
    });
  }

  auto stateCBOR = src->getState();
  if (stateCBOR.empty()) return sol::make_object(s, sol::nil);
  
  CborParser parser; CborValue it;
  if (cbor_parser_init(stateCBOR.data(), stateCBOR.size(), 0, &parser, &it) != CborNoError) return sol::make_object(s, sol::nil);
  
  sol::state_view lView(s); sol::table res = lView.create_table();
  CborValue mapIt;
  if (cbor_value_is_array(&it)) {
    CborValue arrayIt; cbor_value_enter_container(&it, &arrayIt); cbor_value_advance(&arrayIt);
    if (cbor_value_is_map(&arrayIt)) cbor_value_enter_container(&arrayIt, &mapIt);
    else return sol::make_object(s, sol::nil);
  } else if (cbor_value_is_map(&it)) cbor_value_enter_container(&it, &mapIt);
  else return sol::make_object(s, sol::nil);

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
}

void GUIEngine::startCommandThread() {
  commandThreadRunning = true;
  commandThread = std::thread([this]() {
    while (commandThreadRunning) {
      std::function<void()> cmd;
      {
        std::lock_guard<std::mutex> l(cmdMutex);
        if (!pendingCommands.empty()) {
          auto it = pendingCommands.begin();
          cmd = it->second;
          pendingCommands.erase(it);
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

void GUIEngine::stopCommandThread() { commandThreadRunning = false; if (commandThread.joinable()) commandThread.join(); }

void GUIEngine::postTwinCommand(const std::string& name, std::function<void()> cmd) {
  std::lock_guard<std::mutex> l(cmdMutex);
  pendingCommands[name] = cmd;
}
