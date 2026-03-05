/**
 * @file main.cpp
 * @brief Entry point for NexRx Digital Twin
 *
 * @copyright 2026 NexRx Project - MIT License
 */

#include "orchestrator/Orchestrator.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <getopt.h>
#include <csignal>

using namespace nexrx;

static Orchestrator* gOrchestrator = nullptr;

void signalHandler(int signum) {
  std::cout << "\nInterrupt signal (" << signum << ") received.\n";
  if (gOrchestrator) {
    gOrchestrator->stop();
  }
}

void printUsage(const char* progName) {
  std::cout << "Usage: " << progName << " [options] netlist_file\n"
            << "Options:\n"
            << "  -h, --help           Show this help\n"
            << "  -v, --verbose        Enable verbose output\n"
            << "  -r, --realtime       Run in real-time mode (sync with wall clock)\n"
            << "  -t, --timestep NS    Set simulation time step in ns (default: 10)\n"
            << "  -s, --samplerate HZ  Set ADC sample rate in Hz (default: 96000)\n"
            << "  -d, --duration S     Set simulation duration in seconds\n";
}

int main(int argc, char* argv[]) {
  std::signal(SIGINT, signalHandler);

  OrchestratorConfig config;
  double duration = 0.0;
  std::string netlistPath;

  static struct option longOptions[] = {
    {"help",       no_argument,       nullptr, 'h'},
    {"verbose",    no_argument,       nullptr, 'v'},
    {"realtime",   no_argument,       nullptr, 'r'},
    {"timestep",   required_argument, nullptr, 't'},
    {"samplerate", required_argument, nullptr, 's'},
    {"duration",   required_argument, nullptr, 'd'},
    {nullptr,      0,                 nullptr, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "hvrt:s:d:", longOptions, nullptr)) != -1) {
    switch (opt) {
      case 'h': printUsage(argv[0]); return 0;
      case 'v': config.verbose = true; break;
      case 'r': config.realTimeMode = true; break;
      case 't': config.simulationTimeStepNS = std::stod(optarg); break;
      case 's': config.adcSampleRateHz = std::stod(optarg); break;
      case 'd': duration = std::stod(optarg); break;
      default: printUsage(argv[0]); return 1;
    }
  }

  if (optind >= argc) {
    std::cerr << "Error: No netlist file specified\n";
    printUsage(argv[0]);
    return 1;
  }
  netlistPath = argv[optind];
  config.netlistPath = netlistPath;

  std::cout << "=== NexRx Digital Twin ===" << std::endl;
  std::cout << "Loading netlist: " << netlistPath << std::endl;

  Orchestrator orchestrator;
  gOrchestrator = &orchestrator;

  if (!orchestrator.initialize(config)) {
    std::cerr << "Failed to initialize orchestrator" << std::endl;
    return 1;
  }

  uint64_t lastPrintSample = 0;
  orchestrator.setADCSampleCallback([&](double timeS, const std::vector<double>&) {
    uint64_t sampleIndex = orchestrator.getStats().adcSamplesGenerated;
    if (config.verbose && (sampleIndex - lastPrintSample >= 9600)) {
      std::cout << "  ADC sample " << sampleIndex
                << " at t=" << timeS * 1e3 << " ms" << std::endl;
      lastPrintSample = sampleIndex;
    }
  });

  bool success;
  if (duration > 0) {
    std::cout << "Running for " << duration << " seconds..." << std::endl;
    success = orchestrator.runFor(duration);
  } else {
    std::cout << "Running to completion..." << std::endl;
    success = orchestrator.runToCompletion();
  }

  const auto& stats = orchestrator.getStats();
  std::cout << "\n--- Results ---" << std::endl;
  std::cout << "  Success: " << (success ? "yes" : "no") << std::endl;
  std::cout << "  Total steps: " << stats.totalSteps << std::endl;
  std::cout << "  ADC samples: " << stats.adcSamplesGenerated << std::endl;
  std::cout << "  Simulated time: " << stats.simTimeS * 1e3 << " ms" << std::endl;
  std::cout << "  Wall clock: " << stats.wallTimeS << " s" << std::endl;
  std::cout << "  Speed factor: " << stats.speedFactor << "x real-time" << std::endl;

  gOrchestrator = nullptr;
  return success ? 0 : 1;
}
