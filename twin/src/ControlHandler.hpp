#pragma once

#include "transport/TcpControlTransport.hpp"
#include "AttenuatorModel.hpp"

#include <cbor.h>
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
    PreselectorModel() {
        for (int i=0; i<11; ++i) caps_[i].store(false, std::memory_order_relaxed);
        l1_shorted_.store(false, std::memory_order_relaxed);
    }
    void setCap(int idx, bool enabled) { if (idx >= 0 && idx < 11) caps_[idx].store(enabled, std::memory_order_relaxed); }
    void setInd(int idx, bool enabled) { if (idx == 0) l1_shorted_.store(enabled, std::memory_order_relaxed); }
    bool getCap(int idx) const { return (idx >= 0 && idx < 11) ? caps_[idx].load(std::memory_order_relaxed) : false; }
    bool isL1Shorted() const { return l1_shorted_.load(std::memory_order_relaxed); }
private:
    std::atomic<bool> caps_[11];
    std::atomic<bool> l1_shorted_;
};

class PgaModel {
public:
    PgaModel() {
        gain_.store(0.0, std::memory_order_relaxed);
    }
    void setGain(double db) { gain_.store(db, std::memory_order_relaxed); }
    double getGain() const { return gain_.load(std::memory_order_relaxed); }
private:
    std::atomic<double> gain_;
};

struct CodecConfig {
    std::atomic<int> sampleRate{96000};
    std::atomic<int> channelMap[8];
    std::atomic<double> gain{0.0};
    std::atomic<int> filterType{0};

    CodecConfig() {
        for (int i = 0; i < 8; ++i) channelMap[i].store(i);
    }
};

class ControlHandler {
public:
    // Command Strings
    static constexpr const char* CMD_SET_QSD_VFO     = "SVFO";
    static constexpr const char* CMD_SET_ATTEN       = "SATT";
    static constexpr const char* CMD_SET_PRESEL_C    = "SPRC";
    static constexpr const char* CMD_SET_PRESEL_L    = "SPRL";
    static constexpr const char* CMD_SET_PGA_GAIN    = "SPGA";
    static constexpr const char* CMD_START_STREAM    = "STM[";
    static constexpr const char* CMD_STOP_STREAM     = "]STM";
    static constexpr const char* CMD_GET_STATUS      = "GSTS";
    static constexpr const char* CMD_GET_CONFIG      = "GCNF";
    static constexpr const char* CMD_SET_ISG_ENABLE  = "SIEN";
    static constexpr const char* CMD_SET_ISG_FREQ    = "SIFQ";
    static constexpr const char* CMD_SET_ISG_MODE    = "SIMO";
    static constexpr const char* CMD_SET_CALIBRATION = "SCAL";
    static constexpr const char* CMD_GET_CALIBRATION = "GCAL";
    static constexpr const char* CMD_SET_CODEC       = "SCOD";
    static constexpr const char* CMD_DISCONNECT      = "GBYE";

    ControlHandler(double f0, double f1, double f2, AttenuatorModel* atten = nullptr, PreselectorModel* presel = nullptr, PgaModel* pga = nullptr)
        : attenuator_(atten), presel_(presel), pga_(pga), streaming_(false), running_(false),
          connected_(false), reconnected_(false) {
        qsd_freq_hz_[0].store(f0, std::memory_order_relaxed);
        qsd_freq_hz_[1].store(f1, std::memory_order_relaxed);
        qsd_freq_hz_[2].store(f2, std::memory_order_relaxed);
        isg_enabled_.store(false);
        isg_freq_hz_.store(14201000.0);
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

    bool isIsgEnabled() const { return isg_enabled_.load(std::memory_order_relaxed); }
    double getIsgFreq() const { return isg_freq_hz_.load(std::memory_order_relaxed); }

    std::string consumeReconnect() {
        std::lock_guard<std::mutex> lock(reconnectMutex_);
        if (reconnected_) { reconnected_ = false; return newClientIP_; }
        return "";
    }

    void getCodecConfig(int& rate, std::vector<int>& channelMap) const {
        rate = codec_.sampleRate.load(std::memory_order_relaxed);
        channelMap.clear();
        for (int i = 0; i < 8; ++i) channelMap.push_back(codec_.channelMap[i].load(std::memory_order_relaxed));
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
            
            std::vector<uint8_t> response = handleCborCommand(result.value);
            control_->sendResponse(response);
        }
    }

    std::vector<uint8_t> handleCborCommand(const std::vector<uint8_t>& request) {
        CborParser parser;
        CborValue it;
        if (cbor_parser_init(request.data(), request.size(), 0, &parser, &it) != CborNoError) {
            return encodeResponse(1, "CBOR Parse Error");
        }

        if (!cbor_value_is_array(&it)) return encodeResponse(1, "Request must be array");

        CborValue arrayIt;
        cbor_value_enter_container(&it, &arrayIt);

        char cmd[16]; size_t cmdLen = sizeof(cmd);
        if (!cbor_value_is_text_string(&arrayIt)) return encodeResponse(1, "Invalid command ID");
        cbor_value_copy_text_string(&arrayIt, cmd, &cmdLen, &arrayIt);

        if (!cbor_value_is_array(&arrayIt)) return encodeResponse(1, "Arguments must be array");
        CborValue argsIt;
        cbor_value_enter_container(&arrayIt, &argsIt);

        std::string sCmd(cmd);

        if (sCmd == CMD_SET_QSD_VFO) {
            uint64_t idx; double freq;
            if (!cbor_value_is_unsigned_integer(&argsIt)) return encodeResponse(1, "Invalid index");
            cbor_value_get_uint64(&argsIt, &idx); cbor_value_advance(&argsIt);
            if (cbor_value_is_integer(&argsIt)) { int64_t f; cbor_value_get_int64(&argsIt, &f); freq = (double)f; }
            else if (cbor_value_is_double(&argsIt)) { cbor_value_get_double(&argsIt, &freq); }
            else return encodeResponse(1, "Invalid frequency");
            
            if (idx < 3) {
                qsd_freq_hz_[idx].store(freq, std::memory_order_relaxed);
                if (verbose_) std::cout << "[Control] SET_QSD_VFO " << idx << " " << std::fixed << std::setprecision(0) << freq << " Hz" << std::endl;
                return encodeResponse(0, "OK");
            }
            return encodeResponse(1, "Invalid QSD index");
        }
        else if (sCmd == CMD_START_STREAM) {
            if (verbose_) std::cout << "[Control] START_STREAM" << std::endl;
            streaming_.store(true, std::memory_order_release);
            return encodeResponse(0, "OK");
        }
        else if (sCmd == CMD_STOP_STREAM) {
            if (verbose_) std::cout << "[Control] STOP_STREAM" << std::endl;
            streaming_.store(false, std::memory_order_release);
            return encodeResponse(0, "OK");
        }
        else if (sCmd == CMD_SET_PRESEL_C) {
            uint64_t idx; bool en;
            if (!cbor_value_is_unsigned_integer(&argsIt)) return encodeResponse(1, "Invalid index");
            cbor_value_get_uint64(&argsIt, &idx); cbor_value_advance(&argsIt);
            if (!cbor_value_is_boolean(&argsIt)) return encodeResponse(1, "Invalid enable");
            cbor_value_get_boolean(&argsIt, &en);
            if (presel_) {
                presel_->setCap((int)idx, en);
                if (verbose_) std::cout << "[Control] SET_PRESEL_C " << idx << " " << en << std::endl;
                return encodeResponse(0, "OK");
            }
            return encodeResponse(1, "No preselector");
        }
        else if (sCmd == CMD_SET_PRESEL_L) {
            uint64_t idx; bool en;
            if (!cbor_value_is_unsigned_integer(&argsIt)) return encodeResponse(1, "Invalid index");
            cbor_value_get_uint64(&argsIt, &idx); cbor_value_advance(&argsIt);
            if (!cbor_value_is_boolean(&argsIt)) return encodeResponse(1, "Invalid enable");
            cbor_value_get_boolean(&argsIt, &en);
            if (presel_) {
                presel_->setInd((int)idx, en);
                if (verbose_) std::cout << "[Control] SET_PRESEL_L " << idx << " " << en << std::endl;
                return encodeResponse(0, "OK");
            }
            return encodeResponse(1, "No preselector");
        }
        else if (sCmd == CMD_SET_ATTEN) {
            uint64_t db; bool en;
            if (!cbor_value_is_unsigned_integer(&argsIt)) return encodeResponse(1, "Invalid dB value");
            cbor_value_get_uint64(&argsIt, &db); cbor_value_advance(&argsIt);
            if (!cbor_value_is_boolean(&argsIt)) return encodeResponse(1, "Invalid enable");
            cbor_value_get_boolean(&argsIt, &en);
            if (attenuator_) {
                if (db == 3) attenuator_->setAtten3dB(en);
                else if (db == 6) attenuator_->setAtten6dB(en);
                else if (db == 12) attenuator_->setAtten12dB(en);
                else if (db == 24) attenuator_->setAtten24dB(en);
                else return encodeResponse(1, "Invalid attenuator bit");
                if (verbose_) std::cout << "[Control] SET_ATTEN " << db << " " << en << std::endl;
                return encodeResponse(0, "OK");
            }
            return encodeResponse(1, "No attenuator");
        }
        else if (sCmd == CMD_SET_ISG_ENABLE) {
            bool en;
            if (!cbor_value_is_boolean(&argsIt)) return encodeResponse(1, "Invalid enable");
            cbor_value_get_boolean(&argsIt, &en);
            isg_enabled_.store(en);
            if (verbose_) std::cout << "[Control] SET_ISG_ENABLE " << en << std::endl;
            return encodeResponse(0, "OK");
        }
        else if (sCmd == CMD_SET_ISG_FREQ) {
            double freq;
            if (cbor_value_is_integer(&argsIt)) { int64_t f; cbor_value_get_int64(&argsIt, &f); freq = (double)f; }
            else if (cbor_value_is_double(&argsIt)) { cbor_value_get_double(&argsIt, &freq); }
            else return encodeResponse(1, "Invalid frequency");
            
            isg_freq_hz_.store(freq);
            if (freq == 0.0) isg_enabled_.store(false);
            else isg_enabled_.store(true);

            if (verbose_) std::cout << "[Control] SET_ISG_FREQ " << freq << (freq == 1.0 ? " (Noise)" : " Hz") << std::endl;
            return encodeResponse(0, "OK");
        }
        else if (sCmd == CMD_SET_PGA_GAIN) {
            double gain;
            if (cbor_value_is_integer(&argsIt)) { int64_t g; cbor_value_get_int64(&argsIt, &g); gain = (double)g; }
            else if (cbor_value_is_double(&argsIt)) { cbor_value_get_double(&argsIt, &gain); }
            else return encodeResponse(1, "Invalid gain");
            if (pga_) {
                pga_->setGain(gain);
                if (verbose_) std::cout << "[Control] SET_PGA_GAIN " << gain << " dB" << std::endl;
                return encodeResponse(0, "OK");
            }
            return encodeResponse(1, "No PGA model");
        }
        else if (sCmd == CMD_SET_CODEC) {
            uint64_t rate;
            if (!cbor_value_is_unsigned_integer(&argsIt)) return encodeResponse(1, "Invalid rate");
            cbor_value_get_uint64(&argsIt, &rate); cbor_value_advance(&argsIt);
            if (!cbor_value_is_array(&argsIt)) return encodeResponse(1, "Invalid channel map array");
            CborValue mapIt; cbor_value_enter_container(&argsIt, &mapIt);
            int i = 0;
            while (!cbor_value_at_end(&mapIt) && i < 8) {
                uint64_t ch; cbor_value_get_uint64(&mapIt, &ch);
                codec_.channelMap[i++].store((int)ch);
                cbor_value_advance(&mapIt);
            }
            cbor_value_leave_container(&argsIt, &mapIt);
            double gain;
            if (cbor_value_is_integer(&argsIt)) { int64_t g; cbor_value_get_int64(&argsIt, &g); gain = (double)g; }
            else if (cbor_value_is_double(&argsIt)) { cbor_value_get_double(&argsIt, &gain); }
            else gain = 0.0;
            cbor_value_advance(&argsIt);
            uint64_t filter;
            if (cbor_value_is_unsigned_integer(&argsIt)) cbor_value_get_uint64(&argsIt, &filter); else filter = 0;
            codec_.sampleRate.store((int)rate);
            codec_.gain.store(gain);
            codec_.filterType.store((int)filter);
            if (verbose_) std::cout << "[Control] SET_CODEC rate=" << rate << " gain=" << gain << " filter=" << filter << std::endl;
            return encodeResponse(0, "OK");
        }
        else if (sCmd == CMD_SET_CALIBRATION) {
            char type[64]; size_t len = sizeof(type);
            if (!cbor_value_is_text_string(&argsIt)) return encodeResponse(1, "Invalid type");
            cbor_value_copy_text_string(&argsIt, type, &len, &argsIt);
            if (!cbor_value_is_text_string(&argsIt)) return encodeResponse(1, "Invalid JSON data");
            char data[4096]; size_t dlen = sizeof(data);
            cbor_value_copy_text_string(&argsIt, data, &dlen, &argsIt);
            calibrations_[type] = data;
            if (verbose_) std::cout << "[Control] SET_CALIBRATION " << type << std::endl;
            return encodeResponse(0, "OK");
        }
        else if (sCmd == CMD_GET_CALIBRATION) {
            char type[64]; size_t len = sizeof(type);
            if (!cbor_value_is_text_string(&argsIt)) return encodeResponse(1, "Invalid type");
            cbor_value_copy_text_string(&argsIt, type, &len, &argsIt);
            auto it_cal = calibrations_.find(type);
            return encodeResponse(0, it_cal != calibrations_.end() ? it_cal->second : "{}");
        }
        else if (sCmd == CMD_GET_STATUS) {
            std::ostringstream ss;
            ss << "{\"lo\":" << std::fixed << std::setprecision(1) << qsd_freq_hz_[2].load() 
               << ",\"streaming\":" << (streaming_.load() ? "true" : "false")
               << ",\"isg_enabled\":" << (isg_enabled_.load() ? "true" : "false") << "}";
            return encodeResponse(0, ss.str());
        }
        else if (sCmd == CMD_GET_CONFIG) {
            std::ostringstream ss;
            ss << "{\"version\":\"0.1.0\",\"capacities\":{"
               << "\"preselector\":{\"capacitors\":11,\"inductors\":1},"
               << "\"attenuator_pads\":[3,6,12,24],"
               << "\"qsd\":{\"vfo_min_hz\":100000,\"vfo_max_hz\":60000000}}}";
            return encodeResponse(0, ss.str());
        }
        else if (sCmd == CMD_DISCONNECT) {
            if (verbose_) std::cout << "[Control] Client sent GBYE" << std::endl;
            return encodeResponse(0, "BYE");
        }
        
        return encodeResponse(1, "Unknown Command: " + sCmd);
    }

    std::vector<uint8_t> encodeResponse(int status, const std::string& payload) {
        uint8_t buffer[8192];
        CborEncoder encoder, array;
        cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
        cbor_encoder_create_array(&encoder, &array, 2);
        cbor_encode_int(&array, status);
        cbor_encode_text_stringz(&array, payload.c_str());
        cbor_encoder_close_container(&encoder, &array);
        size_t len = cbor_encoder_get_buffer_size(&encoder, buffer);
        return std::vector<uint8_t>(buffer, buffer + len);
    }

    TcpControlTransport* control_ = nullptr;
    bool verbose_ = false;
    std::atomic<double> qsd_freq_hz_[3];
    AttenuatorModel* attenuator_ = nullptr;
    PreselectorModel* presel_ = nullptr;
    PgaModel* pga_ = nullptr;
    std::atomic<bool> isg_enabled_;
    std::atomic<double> isg_freq_hz_;
    CodecConfig codec_;
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
