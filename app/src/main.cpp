/**
 * @file main.cpp
 * @brief NexRx Application Entry Point
 */

#include "DspEngine.hpp"
#include "GuiEngine.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
  DspEngine dsp;
  GuiEngine gui(dsp);

  if (!gui.init("NexRx SDR")) {
    std::cerr << "Failed to initialize GUI engine" << std::endl;
    return 1;
  }

  std::cout << "Starting NexRx..." << std::endl;
  gui.run();

  return 0;
}
