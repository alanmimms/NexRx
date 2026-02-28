#include <iostream>
#include <vector>
#include <string>
#include "test-engine.hpp"
#include "remote-dev.hpp"

// External test function declarations
namespace nexrx {
    TestStatus stream_chk(RemoteDevice& device, std::string& message);
    TestStatus presel_cal(RemoteDevice& device, std::string& message);
    TestStatus pga_chk(RemoteDevice& device, std::string& message);
    TestStatus iq_bal(RemoteDevice& device, std::string& message);
    TestStatus full_scan(RemoteDevice& device, std::string& message);
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    bool doFullScan = false;

    for (int i=1; i<argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [hostname/IP] [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --all-masks    Generate full 4096-state preselector log" << std::endl;
            std::cout << "  [hostname/IP]  Default: 127.0.0.1" << std::endl;
            return 0;
        } else if (arg == "--all-masks") {
            doFullScan = true;
        } else {
            host = arg;
        }
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "   NexRx Test & Calibration Suite v0.1.0" << std::endl;
    std::cout << "==================================================" << std::endl;
    
    nexrx::RemoteDevice device;
    std::cout << "Connecting to " << host << "... " << std::flush;
    if (!device.connect(host, 5000, 5001)) {
        std::cerr << "FAILED" << std::endl;
        return 1;
    }
    std::cout << "CONNECTED" << std::endl;

    nexrx::TestEngine engine;
    engine.addTest("UDP Streaming Integrity", nexrx::stream_chk);
    if (doFullScan) {
        engine.addTest("Preselector Full Atlas", nexrx::full_scan);
    } else {
        engine.addTest("Preselector Response", nexrx::presel_cal);
    }
    engine.addTest("PGA Linearity", nexrx::pga_chk);
    engine.addTest("QSD Image Rejection", nexrx::iq_bal);
    
    engine.runAll(device);
    
    device.disconnect();
    return 0;
}
