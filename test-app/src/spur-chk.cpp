#include "test-engine.hpp"

namespace nexrx {

TestStatus spur_chk(RemoteDevice& device, std::string& message) {
    (void)device;
    message = "Not implemented yet";
    return TestStatus::Skipped;
}

} // namespace nexrx
