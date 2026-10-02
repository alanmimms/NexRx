#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <unistd.h>
#include "test-engine.hpp"
#include "remote-dev.hpp"

// External test function declarations
namespace nexrx {
  TestStatus stream_chk(RemoteDevice& device, std::string& message);
  TestStatus pga_chk(RemoteDevice& device, std::string& message);
  TestStatus iq_bal(RemoteDevice& device, std::string& message);
}

int main(int argc, char* argv[]) {
  std::string host = "127.0.0.1";
  bool doFullScan = false;
  int timeoutSec = 15;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: " << argv[0] << " [hostname/IP] [options]\n"
                << "Options:\n"
                << "  --all-masks    Generate full 4096-state log\n"
                << "  --timeout SEC  Timeout in seconds (default: 15)\n"
                << "  [hostname/IP]  Default: 127.0.0.1\n";
      return 0;
    } else if (arg == "--all-masks") {
      doFullScan = true;
    } else if (arg == "--timeout" && i + 1 < argc) {
      timeoutSec = std::stoi(argv[++i]);
    } else {
      host = arg;
    }
  }

  std::cout << "==================================================" << std::endl;
  std::cout << "   NexRx Test & Calibration Suite v0.1.0" << std::endl;
  std::cout << "==================================================" << std::endl;

  std::mutex watchdogMutex;
  std::condition_variable watchdogCv;
  bool testFinished = false;
  std::thread watchdog;

  if (timeoutSec > 0) {
    watchdog = std::thread([timeoutSec, &watchdogMutex, &watchdogCv, &testFinished]() {
      std::unique_lock<std::mutex> lock(watchdogMutex);
      if (!watchdogCv.wait_for(lock, std::chrono::seconds(timeoutSec), [&] { return testFinished; })) {
        std::cerr << "\n[TEST WATCHDOG] Timed out after " << timeoutSec << " seconds! Forcing exit." << std::endl;
        _exit(124);
      }
    });
  }

  nexrx::RemoteDevice device;
  std::cout << "Connecting to " << host << "... " << std::flush;
  bool connected = false;
  for (int attempt = 0; attempt < 50; ++attempt) {
    if (device.connect(host, 5000, 5001)) {
      connected = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  if (!connected) {
    std::cerr << "FAILED" << std::endl;
    {
      std::lock_guard<std::mutex> lock(watchdogMutex);
      testFinished = true;
    }
    watchdogCv.notify_all();
    if (watchdog.joinable()) {
      watchdog.join();
    }
    return 1;
  }
  std::cout << "CONNECTED" << std::endl;

  nexrx::TestEngine engine;
  engine.addTest("UDP Streaming Integrity", nexrx::stream_chk);
  engine.addTest("PGA Linearity", nexrx::pga_chk);
  engine.addTest("OSD Image Rejection", nexrx::iq_bal);

  engine.runAll(device);

  device.disconnect();

  {
    std::lock_guard<std::mutex> lock(watchdogMutex);
    testFinished = true;
  }
  watchdogCv.notify_all();
  if (watchdog.joinable()) {
    watchdog.join();
  }

  bool allPassed = true;
  for (const auto& r : engine.getResults()) {
    if (r.status != nexrx::TestStatus::Passed) {
      allPassed = false;
      break;
    }
  }
  return allPassed ? 0 : 1;
}
