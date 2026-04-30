#include "ControlHandler.hpp"
#include "AGCManager.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace nexrx {

ControlHandler::ControlHandler(double f0, double f1, double f2, 
                               AttenuatorModel* atten, 
                               FilterBankModel* filters, 
                               PGAModel* pga,
                               AGCManager* agc)
  : attenuator(atten)
  , filters(filters)
  , pga(pga)
  , agc(agc)
  , streaming(false)
  , running(false)
  , connected(false)
  , reconnected(false) {
  vfoHz.store(f2);
  qsdKHz.store(f1 - f2);
  qsdFreqHz[0].store(f0, std::memory_order_relaxed);
  qsdFreqHz[1].store(f1, std::memory_order_relaxed);
  qsdFreqHz[2].store(f2, std::memory_order_relaxed);
  isgEnabled.store(false);
  isgFreqHz.store(14201000.0);
  agcMode.store(0);
  trMode.store(0);
}

ControlHandler::~ControlHandler() {
  stop();
}

void ControlHandler::start(TCPControlTransport* ctrl, bool verbose) {
  control_ = ctrl;
  verbose_ = verbose;
  running = true;
  connected = true;
  thread_ = std::thread(&ControlHandler::run, this);
}

void ControlHandler::stop() {
  running = false;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void ControlHandler::run() {
  while (running) {
    auto result = control_->receiveRequest(std::chrono::milliseconds(100));
    if (!result.ok()) {
      if (result.error == TransportError::Closed && connected.load()) {
        if (verbose_) {
          std::cout << "[Control] Client connection closed" << std::endl;
        }
        connected.store(false, std::memory_order_release);
        streaming.store(false, std::memory_order_release);
      }
      continue;
    }
    std::vector<uint8_t> response = handleCborCommand(result.value);
    control_->sendResponse(response);
  }
}

std::vector<uint8_t> ControlHandler::handleCborCommand(const std::vector<uint8_t>& request) {
  CborParser parser;
  CborValue it, arrayIt;
  if (cbor_parser_init(request.data(), request.size(), 0, &parser, &it) != CborNoError) {
    return encodeResponse(1, "CBOR Error");
  }

  cbor_value_enter_container(&it, &arrayIt);
  uint64_t cmdID = 0;
  if (cbor_value_get_uint64(&arrayIt, &cmdID) != CborNoError) {
    return encodeResponse(1, "Command ID Error");
  }
  cbor_value_advance(&arrayIt);

  if (verbose_ && (uint32_t)cmdID != Control::CMD_GET_STATE) {
    char cmdChars[5] = {
      (char)((cmdID >> 24) & 0xFF),
      (char)((cmdID >> 16) & 0xFF),
      (char)((cmdID >> 8) & 0xFF),
      (char)(cmdID & 0xFF),
      0
    };
    std::printf("[Control] Received command: %s", cmdChars);
    
    // Log parameters without consuming them from the main iterator
    CborValue argIt = arrayIt;
    while (!cbor_value_at_end(&argIt)) {
        CborType type = cbor_value_get_type(&argIt);
        if (type == CborIntegerType) {
            int64_t val;
            cbor_value_get_int64(&argIt, &val);
            std::printf(" %lld", (long long)val);
        } else if (type == CborDoubleType) {
            double val;
            cbor_value_get_double(&argIt, &val);
            std::printf(" %.6g", val);
        } else if (type == CborBooleanType) {
            bool val;
            cbor_value_get_boolean(&argIt, &val);
            std::printf(" %s", val ? "true" : "false");
        } else if (type == CborTextStringType) {
            size_t len;
            cbor_value_get_string_length(&argIt, &len);
            std::vector<char> buf(len + 1);
            cbor_value_copy_text_string(&argIt, buf.data(), &len, nullptr);
            std::printf(" \"%s\"", buf.data());
        } else {
            std::printf(" <type %d>", type);
        }
        cbor_value_advance(&argIt);
    }
    std::printf("\n");
  }

  switch ((uint32_t)cmdID) {
    case Control::CMD_SET_VFO: {
      double f, k;
      cbor_value_get_double(&arrayIt, &f);
      cbor_value_advance(&arrayIt);
      cbor_value_get_double(&arrayIt, &k);
      
      vfoHz.store(f);
      qsdKHz.store(k);
      
      qsdFreqHz[0].store(f - k);
      qsdFreqHz[1].store(f + k);
      qsdFreqHz[2].store(f);
      return encodeResponse(0, "OK");
    } 
    case Control::CMD_SET_ATTEN: {
      uint64_t db;
      cbor_value_get_uint64(&arrayIt, &db);
      if (attenuator) {
        attenuator->setAtten3dB(db & 0x01);
        attenuator->setAtten6dB(db & 0x02);
        attenuator->setAtten12dB(db & 0x04);
        attenuator->setAtten24dB(db & 0x08);
      }
      return encodeResponse(0, "OK");
    }
    case Control::CMD_SET_PGA_GAIN: {
      uint64_t code;
      cbor_value_get_uint64(&arrayIt, &code);
      if (pga) {
        pga->setGainCode((int)code);
      }
      return encodeResponse(0, "OK");
    }
    case Control::CMD_SET_AGC_MODE: {
      uint64_t mode;
      cbor_value_get_uint64(&arrayIt, &mode);
      agcMode.store((int)mode);
      if (agc) {
        agc->setModeInt((int)mode);
      }
      return encodeResponse(0, "OK");
    }
    case Control::CMD_SET_TR_MODE: {
      uint64_t mode;
      cbor_value_get_uint64(&arrayIt, &mode);
      trMode.store((int)mode);
      return encodeResponse(0, "OK");
    }
    case Control::CMD_START_STREAM:
      if (verbose_) std::cout << "[Control] STREAMING START requested" << std::endl;
      streaming.store(true);
      return encodeResponse(0, "OK");
    case Control::CMD_STOP_STREAM:
      if (verbose_) std::cout << "[Control] STREAMING STOP requested" << std::endl;
      streaming.store(false);
      return encodeResponse(0, "OK");
    case Control::CMD_GET_TIMESTAMP:
      return encodeResponse(0, std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    case Control::CMD_SET_ISG_FREQ: {
      double f;
      cbor_value_get_double(&arrayIt, &f);
      isgFreqHz.store(f);
      return encodeResponse(0, "OK");
    }
    case Control::CMD_SET_ISG_ENABLE: {
      bool en;
      cbor_value_get_boolean(&arrayIt, &en);
      isgEnabled.store(en);
      return encodeResponse(0, "OK");
    }
    case Control::CMD_SET_HPF_BYPASS: {
      bool bypass;
      cbor_value_get_boolean(&arrayIt, &bypass);
      if (filters) {
        filters->setHpfBypass(bypass);
      }
      return encodeResponse(0, "OK");
    }
    case Control::CMD_SET_BPF_SELECT: {
      uint64_t index;
      cbor_value_get_uint64(&arrayIt, &index);
      if (filters) {
        filters->setBpfIndex((int)index);
      }
      return encodeResponse(0, "OK");
    }
    case Control::CMD_GET_STATE: {
      uint8_t buf[1024];
      CborEncoder enc, array, map;
      cbor_encoder_init(&enc, buf, sizeof(buf), 0);
      cbor_encoder_create_array(&enc, &array, 2);
      cbor_encode_int(&array, 0); // Status OK
      cbor_encoder_create_map(&array, &map, 9);

      cbor_encode_text_stringz(&map, "vfo");
      cbor_encode_double(&map, vfoHz.load());

      cbor_encode_text_stringz(&map, "atten");
      cbor_encode_double(&map, attenuator ? attenuator->getTotalAttenDB() : 0.0);

      cbor_encode_text_stringz(&map, "pga");
      cbor_encode_int(&map, pga ? pga->getGainCode() : 0);

      cbor_encode_text_stringz(&map, "agc");
      cbor_encode_int(&map, agcMode.load());

      cbor_encode_text_stringz(&map, "isgFreq");
      cbor_encode_double(&map, isgFreqHz.load());

      cbor_encode_text_stringz(&map, "isgEn");
      cbor_encode_boolean(&map, isgEnabled.load());

      cbor_encode_text_stringz(&map, "hpfBypass");
      cbor_encode_boolean(&map, filters ? filters->isHpfBypassed() : false);

      cbor_encode_text_stringz(&map, "bpfIndex");
      cbor_encode_int(&map, filters ? filters->getBpfIndex() : 0);

      cbor_encode_text_stringz(&map, "tr");
      cbor_encode_int(&map, trMode.load());

      cbor_encoder_close_container(&array, &map);
      cbor_encoder_close_container(&enc, &array);
      return {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)};
    }
    case Control::CMD_CAL_STIM: {
      double f;
      uint64_t durationMs;
      cbor_value_get_double(&arrayIt, &f);
      cbor_value_advance(&arrayIt);
      cbor_value_get_uint64(&arrayIt, &durationMs);
      
      calStimFreqHz.store(f);
      calStimEnabled.store(true);
      
      // Detached thread to turn off after duration
      std::thread([this, durationMs]() {
          std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
          calStimEnabled.store(false);
          if (verbose_) std::cout << "[Control] Calibration stimulus timed out" << std::endl;
      }).detach();
      
      if (verbose_) std::cout << "[Control] Calibration stimulus ENABLED at " << f << " Hz for " << durationMs << " ms" << std::endl;
      return encodeResponse(0, "OK");
    }
    case Control::CMD_GBYE: {
      connected.store(false);
      return encodeResponse(0, "BYE");
    }
    default:
      std::printf("[Control] Unknown command: 0x%08X\n", (uint32_t)cmdID);
      return encodeResponse(1, "Unknown command");
  }
}

std::vector<uint8_t> ControlHandler::encodeResponse(int status, const std::string& payload) {
  uint8_t buffer[1024];
  CborEncoder encoder, array;
  cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);
  cbor_encoder_create_array(&encoder, &array, 2);
  cbor_encode_int(&array, status);
  cbor_encode_text_stringz(&array, payload.c_str());
  cbor_encoder_close_container(&encoder, &array);
  return std::vector<uint8_t>(buffer, buffer + cbor_encoder_get_buffer_size(&encoder, buffer));
}

} // namespace nexrx
