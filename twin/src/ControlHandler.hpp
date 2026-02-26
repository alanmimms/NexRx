#pragma once

#include "transport/TcpControlTransport.hpp"
#include "AttenuatorModel.hpp"

#include <atomic>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <map>

namespace nexrx {

class PreselectorModel {
public:
    void setCap(int idx, bool enabled) { if (idx >= 0 && idx < 11) caps_[idx].store(enabled, std::memory_order_relaxed); }
    void setInd(bool enabled) { l1_.store(enabled, std::memory_order_relaxed); }
    bool getCap(int idx) const { return (idx >= 0 && idx < 11) ? caps_[idx].load(std::memory_order_relaxed) : false; }
    bool getInd() const { return l1_.load(std::memory_order_relaxed); }
private:
    std::atomic<bool> caps_[11];
    std::atomic<bool> l1_{false};
};

class ControlHandler {
public:
    ControlHandler(double f0, double f1, double f2, AttenuatorModel* atten = nullptr, PreselectorModel* presel = nullptr)
        : attenuator_(atten), presel_(presel), streaming_(false), running_(false),
          connected_(false), reconnected_(false) {
        qsd_freq_hz_[0].store(f0, std::memory_order_relaxed);
        qsd_freq_hz_[1].store(f1, std::memory_order_relaxed);
        qsd_freq_hz_[2].store(f2, std::memory_order_relaxed);
        bist_enabled_.store(false);
        bist_freq_hz_.store(14201000.0);
    }

    ~ControlHandler() { stop(); }

    void start(TcpControlTransport* control, bool verbose) {
        control_ = control;
        verbose_ = verbose;
        running_ = true;
        connected_ = true;
        thread_ = std::thread(&ControlHandler::run, this);
    }

    void stop() { running_ = false; if (thread_.joinable()) thread_.join(); }

    double getQsdVfo(int idx) const { if (idx < 0 || idx >= 3) return 0; return qsd_freq_hz_[idx].load(std::memory_order_relaxed); }
    void setQsdVfo(int idx, double freq) { if (idx >= 0 && idx < 3) qsd_freq_hz_[idx].store(freq, std::memory_order_relaxed); }

    bool isStreaming() const { return streaming_.load(std::memory_order_acquire); }
    void setStreaming(bool s) { streaming_.store(s, std::memory_order_release); }
    bool isConnected() const { return connected_.load(std::memory_order_acquire); }

    bool isBistEnabled() const { return bist_enabled_.load(std::memory_order_relaxed); }
    double getBistFreq() const { return bist_freq_hz_.load(std::memory_order_relaxed); }

    std::string consumeReconnect() {
        std::lock_guard<std::mutex> lock(reconnectMutex_);
        if (reconnected_) { reconnected_ = false; return newClientIP_; }
        return "";
    }

private:
    void run() {
        while (running_) {
            auto result = control_->receiveRequest(std::chrono::milliseconds(100));
            if (!result.ok()) {
                if (result.error == TransportError::Closed) {
                    if (connected_.load()) {
                        if (verbose_) std::cout << "\n[Control] Client disconnected" << std::endl;
                        connected_.store(false, std::memory_order_release);
                        streaming_.store(false, std::memory_order_release);
                    }
                    if (control_->acceptClient(std::chrono::milliseconds(0))) {
                        std::string newIP = control_->peerIP();
                        if (verbose_) std::cout << "[Control] New client connected from " << control_->peerAddress() << std::endl;
                        { std::lock_guard<std::mutex> lock(reconnectMutex_); newClientIP_ = newIP; reconnected_ = true; }
                        connected_.store(true, std::memory_order_release);
                    }
                }
                continue;
            }
            std::string request(result.value.begin(), result.value.end());
            std::string response = handleCommand(request);
            std::vector<uint8_t> resp(response.begin(), response.end());
            control_->sendResponse(resp);
        }
    }

    std::string handleCommand(const std::string& cmd) {
        std::istringstream iss(cmd);
        std::string verb; iss >> verb;

        if (verb == "SET_QSD_VFO") {
            int idx; double freq;
            if (iss >> idx >> freq) { 
                if (idx >= 0 && idx < 3) { 
                    qsd_freq_hz_[idx].store(freq, std::memory_order_relaxed); 
                    if (verbose_) std::cout << "[Control] SET_QSD_VFO " << idx << " " << std::fixed << std::setprecision(0) << freq << " Hz" << std::endl;
                    return "OK\n"; 
                } 
            }
            return "ERROR invalid index\n";
        }
        else if (verb == "START_STREAM") { 
            if (verbose_) std::cout << "[Control] START_STREAM" << std::endl;
            streaming_.store(true, std::memory_order_release); return "OK\n"; 
        }
        else if (verb == "STOP_STREAM") { 
            if (verbose_) std::cout << "[Control] STOP_STREAM" << std::endl;
            streaming_.store(false, std::memory_order_release); return "OK\n"; 
        }
        else if (verb == "SET_PRESEL_C" && presel_) {
            int idx, val; if (iss >> idx >> val) { 
                presel_->setCap(idx, val != 0); 
                if (verbose_) std::cout << "[Control] SET_PRESEL_C " << idx << " " << val << std::endl;
                return "OK\n"; 
            }
        }
        else if (verb == "SET_PRESEL_L" && presel_) {
            int val; if (iss >> val) { 
                presel_->setInd(val != 0); 
                if (verbose_) std::cout << "[Control] SET_PRESEL_L " << val << std::endl;
                return "OK\n"; 
            }
        }
        else if (verb == "SET_ATTEN" && attenuator_) {
            int db, val; if (iss >> db >> val) {
                bool en = (val != 0);
                switch (db) {
                    case 3:  attenuator_->setAtten3dB(en); break;
                    case 6:  attenuator_->setAtten6dB(en); break;
                    case 12: attenuator_->setAtten12dB(en); break;
                    case 24: attenuator_->setAtten24dB(en); break;
                    default: return "ERROR invalid bit\n";
                }
                if (verbose_) std::cout << "[Control] SET_ATTEN " << db << " " << val << " (Total: " << attenuator_->getTotalDb() << " dB)" << std::endl;
                return "OK\n";
            }
        }
        else if (verb == "SET_ATTEN_TOTAL" && attenuator_) {
            double db; if (iss >> db) { 
                attenuator_->setTotalDb(db); 
                if (verbose_) std::cout << "[Control] SET_ATTEN_TOTAL " << db << " dB" << std::endl;
                return "OK\n"; 
            }
        }
        else if (verb == "SET_BIST_ENABLE") {
            int val; if (iss >> val) { 
                bist_enabled_.store(val != 0); 
                if (verbose_) std::cout << "[Control] SET_BIST_ENABLE " << val << std::endl;
                return "OK\n"; 
            }
        }
        else if (verb == "SET_BIST_FREQ") {
            double freq; if (iss >> freq) { 
                bist_freq_hz_.store(freq); 
                if (verbose_) std::cout << "[Control] SET_BIST_FREQ " << std::fixed << std::setprecision(0) << freq << " Hz" << std::endl;
                return "OK\n"; 
            }
        }
        else if (verb == "SET_CALIBRATION") {
            std::string type, data; if (iss >> type) { std::getline(iss >> std::ws, data); calibrations_[type] = data; return "OK\n"; }
        }
        else if (verb == "GET_CALIBRATION") {
            std::string type; if (iss >> type) { auto it = calibrations_.find(type); return (it != calibrations_.end() ? it->second : "") + "\n"; }
        }
        else if (verb == "SET_LO") {
            double f; if (iss >> f) { 
                qsd_freq_hz_[2].store(f, std::memory_order_relaxed); 
                if (verbose_) std::cout << "[Control] SET_LO " << std::fixed << std::setprecision(0) << f << " Hz (Legacy)" << std::endl;
                return "OK\n"; 
            }
        }
        return "ERROR\n";
    }

    TcpControlTransport* control_ = nullptr;
    bool verbose_ = false;
    std::atomic<double> qsd_freq_hz_[3];
    AttenuatorModel* attenuator_ = nullptr;
    PreselectorModel* presel_ = nullptr;
    std::atomic<bool> bist_enabled_;
    std::atomic<double> bist_freq_hz_;
    std::map<std::string, std::string> calibrations_;
    std::atomic<bool> streaming_;
    std::atomic<bool> running_;
    std::atomic<bool> connected_;
    std::atomic<bool> reconnected_;
    std::string newClientIP_;
    std::mutex reconnectMutex_;
    std::thread thread_;
};

} // namespace nexrx
