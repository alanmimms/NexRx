#include "UIEngine.hpp"
#include <raylib.h>

int main() {
  UIEngine engine;
  engine.init();

  while (!engine.shouldClose()) {
    engine.update();
    engine.render();
  }

  return 0;
}
