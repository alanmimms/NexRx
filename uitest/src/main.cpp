/**
 * @file main.cpp
 * @brief UITest Playground Entry Point
 */

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

  GUIEngine gui;

  if (!gui.init("NexRx UI Test Playground")) {
    std::cerr << "Failed to initialize GUI engine" << std::endl;
    return 1;
  }

  std::cout << "Starting NexRx UI Test..." << std::endl;
  gui.run();

  return 0;
}
