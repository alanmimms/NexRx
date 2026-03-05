#include "ControlHandler.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace nexrx {

ControlHandler::ControlHandler(double f0, double f1, double f2, 
                               AttenuatorModel* atten, 
                               PreselectorModel* presel, 
                               PGAModel* pga)
  : attenuator(atten)
  , presel(presel)
  , pga(pga)
  , streaming(false)
  , running(false)
  , connected(false)
  , reconnected(false) {
  qsdFreqHz[0].store(f0, std::memory_order_relaxed);
  qsdFreqHz[1].store(f1, std::memory_order_relaxed);
  qsdFreqHz[2].store(f2, std::memory_order_relaxed);
  isgEnabled.store(false);
  isgFreqHz.store(14201000.0);
  agcMode.store(0);
  
  if (presel) {
    presel->autoTune(f2);
  }
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
  char cmd[16];
  size_t cmdLen = sizeof(cmd);
  cbor_value_copy_text_string(&arrayIt, cmd, &cmdLen, &arrayIt);
  std::string sCmd(cmd);

  if (verbose_) {
    std::cout << "[Control] Received command: " << sCmd << std::endl;
  }

  if (sCmd == CMD_SET_VFO) {
    double f, k;
    cbor_value_get_double(&arrayIt, &f);
    cbor_value_advance(&arrayIt);
    cbor_value_get_double(&arrayIt, &k);
    qsdFreqHz[0].store(f - k);
    qsdFreqHz[1].store(f + k);
    qsdFreqHz[2].store(f);
    if (presel) {
      presel->autoTune(f);
    }
    return encodeResponse(0, "OK");
  } 
  else if (sCmd == CMD_SET_ATTEN) {
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
  else if (sCmd == CMD_SET_PGA_GAIN) {
    uint64_t code;
    cbor_value_get_uint64(&arrayIt, &code);
    if (pga) {
      pga->setGainCode((int)code);
    }
    return encodeResponse(0, "OK");
  }
  else if (sCmd == CMD_START_STREAM) {
    if (verbose_) std::cout << "[Control] STREAMING START requested" << std::endl;
    streaming.store(true);
    return encodeResponse(0, "OK");
  }
  else if (sCmd == CMD_STOP_STREAM) {
    if (verbose_) std::cout << "[Control] STREAMING STOP requested" << std::endl;
    streaming.store(false);
    return encodeResponse(0, "OK");
  }
  else if (sCmd == CMD_GET_TIMESTAMP) {
    return encodeResponse(0, std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  }
  else if (sCmd == CMD_SET_ISG_FREQ) {
    double f;
    cbor_value_get_double(&arrayIt, &f);
    isgFreqHz.store(f);
    return encodeResponse(0, "OK");
  }
  else if (sCmd == CMD_SET_ISG_ENABLE) {
    bool en;
    cbor_value_get_boolean(&arrayIt, &en);
    isgEnabled.store(en);
    return encodeResponse(0, "OK");
  }
  else if (sCmd == CMD_SET_PRESEL_IND) {
    bool en;
    cbor_value_get_boolean(&arrayIt, &en);
    if (presel) {
      presel->setInd(0, en);
    }
    return encodeResponse(0, "OK");
  }
  else if (sCmd == CMD_SET_PRESEL_CAP) {
    uint64_t idx;
    bool en;
    cbor_value_get_uint64(&arrayIt, &idx);
    cbor_value_advance(&arrayIt);
    cbor_value_get_boolean(&arrayIt, &en);
    if (presel) {
      presel->setCap((int)idx, en);
    }
    return encodeResponse(0, "OK");
  }

  return encodeResponse(1, "Unknown: " + sCmd);
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
