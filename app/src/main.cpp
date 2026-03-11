/**
 * @file main.cpp
 * @brief NexRx Application Entry Point
 */

#include "DSPEngine.hpp"
#include "GUIEngine.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
  DSPEngine dsp;
  GUIEngine gui(dsp);

  if (!gui.init("NexRx SDR")) {
    std::cerr << "Failed to initialize GUI engine" << std::endl;
    return 1;
  }

  std::cout << "Starting NexRx..." << std::endl;
  gui.run();

  return 0;
}
