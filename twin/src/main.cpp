/**
 * @file main.cpp
 * @brief Entry point for NexRx Digital Twin (Modular Refactor)
 *
 * @copyright 2026 NexRx Project - MIT License
 */

#include "orchestrator/Orchestrator.hpp"

#include <iostream>
#include <string>
#include <cstdlib>
#include <csignal>
#include <getopt.h>

using namespace nexrx;

// Global orchestrator pointer for signal handling
static Orchestrator* gOrchestrator = nullptr;

void signalHandler(int signum) {
    std::cout << "\n[Twin] Caught signal " << signum << ", stopping..." << std::endl;
    if (gOrchestrator) {
        gOrchestrator->stop();
    }
}

void printUsage(const char* progName) {
    std::cout << "NexRx Digital Twin - Software-in-the-Loop Simulator\n";
    std::cout << "\nUsage: " << progName << " [options] <netlist.cir>\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -h, --help           Show this help message\n";
    std::cout << "  -v, --verbose        Enable verbose output\n";
    std::cout << "  -r, --realtime       Enable real-time mode (throttle to wall clock)\n";
    std::cout << "  -t, --timestep <ns>  Set base time step in nanoseconds (default: 1.0)\n";
    std::cout << "  -s, --samplerate <Hz> Set ADC sample rate (default: 96000)\n";
    std::cout << "  -d, --duration <s>   Run for specified duration (default: run to completion)\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << progName << " -v netlists/test_rc.cir\n";
}

int main(int argc, char* argv[]) {
    OrchestratorConfig config;
    double duration = -1.0; 
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

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "=== NexRx Digital Twin ===" << std::endl;
    std::cout << "Loading netlist: " << netlistPath << std::endl;

    Orchestrator orchestrator;
    gOrchestrator = &orchestrator;

    if (!orchestrator.initialize(config)) {
        std::cerr << "Failed to initialize orchestrator" << std::endl;
        return 1;
    }

    uint64_t lastPrintSample = 0;
    orchestrator.setADCSampleCallback([&](double timeS, uint64_t sampleIndex) {
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
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "  Status: " << (success ? "Success" : "Stopped/Error") << std::endl;
    std::cout << "  Total steps: " << stats.totalSteps << std::endl;
    std::cout << "  ADC samples: " << stats.adcSamplesGenerated << std::endl;
    std::cout << "  Simulated time: " << stats.simulatedTimeS * 1e3 << " ms" << std::endl;
    std::cout << "  Wall clock: " << stats.wallClockTimeS << " s" << std::endl;
    std::cout << "  Speed factor: " << stats.speedFactor << "x real-time" << std::endl;

    auto voltage = orchestrator.getNodeVoltage("out");
    if (voltage) {
        std::cout << "  Final V(out): " << *voltage << " V" << std::endl;
    }

    gOrchestrator = nullptr;
    return success ? 0 : 1;
}
