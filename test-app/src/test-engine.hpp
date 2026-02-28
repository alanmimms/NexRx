#pragma once
#include <string>
#include <vector>
#include <functional>
#include "remote-dev.hpp"

namespace nexrx {

enum class TestStatus {
    Pending,
    Running,
    Passed,
    Failed,
    Skipped
};

struct TestResult {
    std::string name;
    TestStatus status;
    std::string message;
    double durationSec;
};

class TestEngine {
public:
    using TestFunc = std::function<TestStatus(RemoteDevice&, std::string&)>;

    void addTest(const std::string& name, TestFunc func);
    void runAll(RemoteDevice& device);
    const std::vector<TestResult>& getResults() const { return results_; }

private:
    struct TestCase {
        std::string name;
        TestFunc func;
    };
    std::vector<TestCase> tests_;
    std::vector<TestResult> results_;
};

} // namespace nexrx
