#include "TwinConn.hpp"
#include <cbor.h>
#include <iostream>
#include <cmath>

namespace nexrx {

TwinConn::~TwinConn() {
  shutdown();
}

bool TwinConn::initialize(const TwinConfig& cfg) {
  if (connected) {
    return true;
  }
  config = cfg;
  frameBuffer.reserve(cfg.frameBufferSize);

  TCPControlClientConfig controlConfig;
  controlConfig.host = cfg.host;
  controlConfig.port = cfg.controlPort;
  control = std::make_unique<TCPControlClient>(controlConfig);
  if (!control->connect()) {
    return false;
  }

  UDPStreamClientConfig streamConfig;
  streamConfig.port = cfg.streamPort;
  streamConfig.receiveBufferSize = cfg.receiveBufferSize;
  stream = std::make_unique<UDPStreamClient>(streamConfig);
  if (!stream->connect()) {
    return false;
  }

  connected = true;
  return true;
}

void TwinConn::shutdown() {
  stopReceiving();
  if (control && connected) {
    sendCBORRequest("GBYE", {});
    control->disconnect();
  }
  if (stream) {
    stream->disconnect();
  }
  connected = false;
}

bool TwinConn::startReceiving() {
  if (!connected || receiving) {
    return false;
  }
  stopRequested = false;
  receiving = true;
  receiveThread = std::thread(&TwinConn::receiveLoop, this);
  return true;
}

void TwinConn::stopReceiving() {
  if (!receiving) {
    return;
  }
  stopRequested = true;
  if (receiveThread.joinable()) {
    receiveThread.join();
  }
  receiving = false;
}

size_t TwinConn::pollFrames(size_t maxFrames) {
  if (!connected || !stream) {
    return 0;
  }
  frameBuffer.clear();
  size_t count = 0;
  while (count < maxFrames) {
    auto res = stream->read(std::chrono::milliseconds(0));
    if (!res.ok()) {
      break;
    }
    
    IQFrame frame = res.value;
    lastSequenceReceived = frame.sequence;
    lastFrameReceived = frame;
    ++framesReceivedCount;
    ++count;
    frameBuffer.push_back(frame);

    std::lock_guard<std::mutex> lock(callbackMutex);
    if (frameCallback) {
      frameCallback(frame);
    }
  }
  std::lock_guard<std::mutex> lock(callbackMutex);
  if (!frameBuffer.empty() && batchCallback) {
    batchCallback(frameBuffer);
  }
  return count;
}

void TwinConn::receiveLoop() {
  while (!stopRequested) {
    if (pollFrames(100) == 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }
}

std::vector<uint8_t> TwinConn::processResponse(const std::vector<uint8_t>& data, const std::string& cmdId) {
  if (data.empty()) {
    return {};
  }
  CborParser parser;
  CborValue it, array;
  if (cbor_parser_init(data.data(), data.size(), 0, &parser, &it) != CborNoError) {
    return {};
  }
  cbor_value_enter_container(&it, &array);
  int64_t status;
  cbor_value_get_int64(&array, &status);
  if (status != 0) {
    return {};
  }
  return data;
}

std::vector<uint8_t> TwinConn::sendCBORRequest(const std::string& cmdId, const std::vector<uint8_t>& request) {
  auto res = control->sendRequest(request, std::chrono::milliseconds(500));
  if (!res.ok()) {
    return {};
  }
  return processResponse(res.value, cmdId);
}

bool TwinConn::setVFO(double freqHz, double offsetHz) {
  uint8_t buf[128];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 3);
  cbor_encode_text_stringz(&arr, "SVFO");
  cbor_encode_double(&arr, freqHz);
  cbor_encode_double(&arr, offsetHz);
  cbor_encoder_close_container(&enc, &arr);
  return !sendCBORRequest("SVFO", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)}).empty();
}

bool TwinConn::setAtten(int dbValue) {
  uint8_t buf[128];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 2);
  cbor_encode_text_stringz(&arr, "SATT");
  cbor_encode_int(&arr, dbValue);
  cbor_encoder_close_container(&enc, &arr);
  return !sendCBORRequest("SATT", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)}).empty();
}

bool TwinConn::setPGAGain(int code) {
  uint8_t buf[128];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 2);
  cbor_encode_text_stringz(&arr, "SPGA");
  cbor_encode_int(&arr, code);
  cbor_encoder_close_container(&enc, &arr);
  return !sendCBORRequest("SPGA", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)}).empty();
}

bool TwinConn::setAGCMode(int mode) {
  uint8_t buf[128];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 2);
  cbor_encode_text_stringz(&arr, "SAGC");
  cbor_encode_int(&arr, mode);
  cbor_encoder_close_container(&enc, &arr);
  return !sendCBORRequest("SAGC", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)}).empty();
}

bool TwinConn::setISGFreq(double freqHz) {
  uint8_t buf[128];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 2);
  cbor_encode_text_stringz(&arr, "SIFQ");
  cbor_encode_double(&arr, freqHz);
  cbor_encoder_close_container(&enc, &arr);
  return !sendCBORRequest("SIFQ", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)}).empty();
}

bool TwinConn::setISGEnable(bool enabled) {
  uint8_t buf[64];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 2);
  cbor_encode_text_stringz(&arr, "SIEN");
  cbor_encode_boolean(&arr, enabled);
  cbor_encoder_close_container(&enc, &arr);
  return !sendCBORRequest("SIEN", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)}).empty();
}

bool TwinConn::setPreselectorInd(bool shorted) {
  uint8_t buf[64];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 2);
  cbor_encode_text_stringz(&arr, "SPRI");
  cbor_encode_boolean(&arr, shorted);
  cbor_encoder_close_container(&enc, &arr);
  return !sendCBORRequest("SPRI", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)}).empty();
}

bool TwinConn::setPreselectorCap(int index, bool enabled) {
  uint8_t buf[64];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 3);
  cbor_encode_text_stringz(&arr, "SPRC");
  cbor_encode_int(&arr, index);
  cbor_encode_boolean(&arr, enabled);
  cbor_encoder_close_container(&enc, &arr);
  return !sendCBORRequest("SPRC", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)}).empty();
}

bool TwinConn::setQsdOffsetKHz(double khz) {
  (void)khz;
  // This is an app-side state change, but we could notify hardware if needed
  return true;
}

bool TwinConn::startStream() {
  uint8_t buf[64];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 1);
  cbor_encode_text_stringz(&arr, "STM[");
  cbor_encoder_close_container(&enc, &arr);
  return !sendCBORRequest("STM[", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)}).empty();
}

bool TwinConn::stopStream() {
  uint8_t buf[64];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 1);
  cbor_encode_text_stringz(&arr, "]STM");
  cbor_encoder_close_container(&enc, &arr);
  return !sendCBORRequest("]STM", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)}).empty();
}

uint64_t TwinConn::getTimestamp() {
  uint8_t buf[64];
  CborEncoder enc, arr;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encoder_create_array(&enc, &arr, 1);
  cbor_encode_text_stringz(&arr, "GTIM");
  cbor_encoder_close_container(&enc, &arr);
  auto res = sendCBORRequest("GTIM", {buf, buf + cbor_encoder_get_buffer_size(&enc, buf)});
  if (res.empty()) {
    return 0;
  }
  return 0; // TODO: Implement CBOR decode
}

} // namespace nexrx
