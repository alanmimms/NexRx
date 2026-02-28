#include "test-engine.hpp"
#include <iostream>
#include <chrono>

namespace nexrx {

void TestEngine::addTest(const std::string& name, TestFunc func) {
    tests_.push_back({name, func});
}

void TestEngine::runAll(RemoteDevice& device) {
    results_.clear();
    std::cout << "\nStarting NexRx Test Suite..." << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    for (const auto& test : tests_) {
        std::cout << "[ RUN      ] " << test.name << std::flush;
        
        TestResult result;
        result.name = test.name;
        result.status = TestStatus::Running;
        
        auto start = std::chrono::steady_clock::now();
        std::string msg;
        try {
            result.status = test.func(device, msg);
        } catch (const std::exception& e) {
            result.status = TestStatus::Failed;
            msg = std::string("Exception: ") + e.what();
        } catch (...) {
            result.status = TestStatus::Failed;
            msg = "Unknown exception";
        }
        auto end = std::chrono::steady_clock::now();
        
        result.message = msg;
        result.durationSec = std::chrono::duration<double>(end - start).count();
        results_.push_back(result);

        if (result.status == TestStatus::Passed) {
            std::cout << "\r[  PASSED  ] " << test.name << " (" << result.durationSec << "s)" << std::endl;
        } else if (result.status == TestStatus::Failed) {
            std::cout << "\r[  FAILED  ] " << test.name << " (" << result.durationSec << "s)" << std::endl;
            if (!msg.empty()) std::cout << "             Error: " << msg << std::endl;
        } else {
            std::cout << "\r[  SKIPPED ] " << test.name << std::endl;
        }
    }

    std::cout << "--------------------------------------------------" << std::endl;
    int passed = 0, failed = 0;
    for (const auto& r : results_) {
        if (r.status == TestStatus::Passed) passed++;
        else if (r.status == TestStatus::Failed) failed++;
    }
    std::cout << "SUMMARY: " << passed << " passed, " << failed << " failed." << std::endl;
}

} // namespace nexrx
