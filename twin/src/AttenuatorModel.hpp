#pragma once
#include <atomic>
#include <cmath>
#include <algorithm>

namespace nexrx {

class AttenuatorModel {
public:
    void setAtten3dB(bool enabled) { atten3dB_.store(enabled, std::memory_order_relaxed); }
    void setAtten6dB(bool enabled) { atten6dB_.store(enabled, std::memory_order_relaxed); }
    void setAtten12dB(bool enabled) { atten12dB_.store(enabled, std::memory_order_relaxed); }
    void setAtten24dB(bool enabled) { atten24dB_.store(enabled, std::memory_order_relaxed); }

    bool getAtten3dB() const { return atten3dB_.load(std::memory_order_relaxed); }
    bool getAtten6dB() const { return atten6dB_.load(std::memory_order_relaxed); }
    bool getAtten12dB() const { return atten12dB_.load(std::memory_order_relaxed); }
    bool getAtten24dB() const { return atten24dB_.load(std::memory_order_relaxed); }

    double getTotalDb() const {
        double total = 0.0;
        if (atten3dB_.load(std::memory_order_relaxed))  total += 3.0;
        if (atten6dB_.load(std::memory_order_relaxed))  total += 6.0;
        if (atten12dB_.load(std::memory_order_relaxed)) total += 12.0;
        if (atten24dB_.load(std::memory_order_relaxed)) total += 24.0;
        return total;
    }

    double getVoltageGain() const { return std::pow(10.0, -getTotalDb() / 20.0); }

    void setTotalDb(double target_db) {
        int steps = std::max(0, std::min(15, (int)std::round(target_db / 3.0)));
        double actual_db = steps * 3.0;
        atten24dB_.store(actual_db >= 24.0, std::memory_order_relaxed); if (actual_db >= 24.0) actual_db -= 24.0;
        atten12dB_.store(actual_db >= 12.0, std::memory_order_relaxed); if (actual_db >= 12.0) actual_db -= 12.0;
        atten6dB_.store(actual_db >= 6.0, std::memory_order_relaxed); if (actual_db >= 6.0) actual_db -= 6.0;
        atten3dB_.store(actual_db >= 3.0, std::memory_order_relaxed);
    }

private:
    std::atomic<bool> atten3dB_{false}, atten6dB_{false}, atten12dB_{false}, atten24dB_{false};
};

} // namespace nexrx
