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
        for (int i=0; i<11; ++i) caps[i].store(false, std::memory_order_relaxed);
        l1Shorted.store(false, std::memory_order_relaxed);
    }
    void setCap(int idx, bool enabled) { 
        if (idx >= 0 && idx < 11) caps[idx].store(enabled, std::memory_order_relaxed); 
    }
    void setInd(int idx, bool enabled) { 
        if (idx == 0) l1Shorted.store(enabled, std::memory_order_relaxed); 
    }
    bool getCap(int idx) const { 
        return (idx >= 0 && idx < 11) ? caps[idx].load(std::memory_order_relaxed) : false; 
    }
    bool isL1Shorted() const { return l1Shorted.load(std::memory_order_relaxed); }
private:
    std::atomic<bool> caps[11];
    std::atomic<bool> l1Shorted;
};

class PGAModel {
public:
    PGAModel() { gainCode.store(0, std::memory_order_relaxed); }
    void setGainCode(int code) { gainCode.store(code, std::memory_order_relaxed); }
    int getGainCode() const { return gainCode.load(std::memory_order_relaxed); }
    double getGainDB() const { 
        /* Match MAX9939 simplified linear mapping for now */
        return static_cast<double>(gainCode.load()) * 4.0; 
    }
private:
    std::atomic<int> gainCode;
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
    static constexpr const char* CMD_SET_VFO         = "SVFO";
    static constexpr const char* CMD_SET_ATTEN       = "SATT";
    static constexpr const char* CMD_SET_PGA_GAIN    = "SPGA";
    static constexpr const char* CMD_SET_AGC_MODE    = "SAGC";
    static constexpr const char* CMD_START_STREAM    = "STM[";
    static constexpr const char* CMD_STOP_STREAM     = "]STM";
    static constexpr const char* CMD_GET_TIMESTAMP   = "GTIM";
    static constexpr const char* CMD_SET_ISG_FREQ    = "SIFQ";

    ControlHandler(double f0, double f1, double f2, 
                   AttenuatorModel* atten = nullptr, 
                   PreselectorModel* presel = nullptr, 
                   PGAModel* pga = nullptr)
        : attenuator(atten), presel(presel), pga(pga), streaming(false), 
          running(false), connected(false), reconnected(false) {
        qsdFreqHz[0].store(f0, std::memory_order_relaxed);
        qsdFreqHz[1].store(f1, std::memory_order_relaxed);
        qsdFreqHz[2].store(f2, std::memory_order_relaxed);
        isgEnabled.store(false);
        isgFreqHz.store(14201000.0);
        agcMode.store(0);
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

    double getQSDFreq(int idx) const { 
        if (idx < 0 || idx >= 3) return 0; 
        return qsdFreqHz[idx].load(std::memory_order_relaxed); 
    }

    bool isStreaming() const { return streaming.load(std::memory_order_acquire); }
    bool isConnected() const { return connected.load(std::memory_order_acquire); }
    bool isISGEnabled() const { return isgEnabled.load(std::memory_order_relaxed); }
    double getISGFreq() const { return isgFreqHz.load(std::memory_order_relaxed); }

    void getCodecConfig(int& rate, std::vector<int>& channelMap) const {
        rate = codec.sampleRate.load(std::memory_order_relaxed);
        channelMap.clear();
        for (int i = 0; i < 8; ++i) 
            channelMap.push_back(codec.channelMap[i].load(std::memory_order_relaxed));
    }

private:
    void run() {
        while (running_) {
            auto result = control_->receiveRequest(std::chrono::milliseconds(100));
            if (!result.ok()) {
                if (result.error == TransportError::Closed && connected_.load()) {
                    connected_.store(false, std::memory_order_release);
                    streaming.store(false, std::memory_order_release);
                }
                continue;
            }
            std::vector<uint8_t> response = handleCborCommand(result.value);
            control_->sendResponse(response);
        }
    }

    std::vector<uint8_t> handleCborCommand(const std::vector<uint8_t>& request) {
        CborParser parser; CborValue it, arrayIt;
        if (cbor_parser_init(request.data(), request.size(), 0, &parser, &it) != CborNoError)
            return encodeResponse(1, "CBOR Error");

        cbor_value_enter_container(&it, &arrayIt);
        char cmd[16]; size_t cmdLen = sizeof(cmd);
        cbor_value_copy_text_string(&arrayIt, cmd, &cmdLen, &arrayIt);
        std::string sCmd(cmd);

        if (sCmd == CMD_SET_VFO) {
            double f, k;
            cbor_value_get_double(&arrayIt, &f); cbor_value_advance(&arrayIt);
            cbor_value_get_double(&arrayIt, &k);
            qsdFreqHz[0].store(f - k); qsdFreqHz[1].store(f + k); qsdFreqHz[2].store(f);
            return encodeResponse(0, "OK");
        } 
        else if (sCmd == CMD_SET_ATTEN) {
            uint64_t db; cbor_value_get_uint64(&arrayIt, &db);
            if (attenuator) {
                attenuator->setAtten3dB(db & 0x01); attenuator->setAtten6dB(db & 0x02);
                attenuator->setAtten12dB(db & 0x04); attenuator->setAtten24dB(db & 0x08);
            }
            return encodeResponse(0, "OK");
        }
        else if (sCmd == CMD_SET_PGA_GAIN) {
            uint64_t code; cbor_value_get_uint64(&arrayIt, &code);
            if (pga) pga->setGainCode((int)code);
            return encodeResponse(0, "OK");
        }
        else if (sCmd == CMD_START_STREAM) {
            streaming.store(true); return encodeResponse(0, "OK");
        }
        else if (sCmd == CMD_STOP_STREAM) {
            streaming.store(false); return encodeResponse(0, "OK");
        }
        else if (sCmd == CMD_GET_TIMESTAMP) {
            return encodeResponse(0, std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        }

        return encodeResponse(1, "Unknown: " + sCmd);
    }

    std::vector<uint8_t> encodeResponse(int status, const std::string& payload) {
        uint8_t buffer[1024]; CborEncoder encoder, array;
        cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
        cbor_encoder_create_array(&encoder, &array, 2);
        cbor_encode_int(&array, status);
        cbor_encode_text_stringz(&array, payload.c_str());
        cbor_encoder_close_container(&encoder, &array);
        return std::vector<uint8_t>(buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer));
    }

    TcpControlTransport* control_ = nullptr;
    bool verbose_ = false;
    std::atomic<double> qsdFreqHz[3];
    AttenuatorModel* attenuator = nullptr;
    PreselectorModel* presel = nullptr;
    PGAModel* pga = nullptr;
    std::atomic<bool> isgEnabled;
    std::atomic<double> isgFreqHz;
    std::atomic<int> agcMode;
    CodecConfig codec;
    std::map<std::string, std::string> calibrations;
    std::atomic<bool> streaming;
    std::atomic<bool> running;
    std::atomic<bool> connected;
    std::atomic<bool> reconnected;
    std::string newClientIP;
    std::mutex reconnectMutex;
    std::thread thread_;
};

} // namespace nexrx
