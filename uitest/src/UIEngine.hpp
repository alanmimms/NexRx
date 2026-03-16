#pragma once

#include <sol/sol.hpp>
#include "LuaBridge.hpp"

class UIEngine {
public:
  UIEngine();
  ~UIEngine();

  void init();
  void update();
  void render();
  bool shouldClose();

private:
  sol::state lua;
  sol::table uiModule;
};
