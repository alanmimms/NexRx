/**
 * @file main.cpp
 * @brief NexRx Application Entry Point
 */

#include "DSPEngine.hpp"
#include "GUIEngine.hpp"
#include <iostream>
#include <atomic>
#include <csignal>

std::atomic<bool> gRunning{true};

void signalHandler(int signum) {
  if (signum == SIGINT) {
    std::cout << "\n[Main] SIGINT received, shutting down..." << std::endl;
    gRunning.store(false);
  }
}

int main(int argc, char* argv[]) {
  std::signal(SIGINT, signalHandler);

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
