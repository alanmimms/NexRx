// NexRx Digital Twin - Transport Abstract Interface
//
// Defines the common interface for Host↔Firmware communication.
// Implementations: SharedMemTransport (I/Q data), UnixSocketTransport (RPC)
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <chrono>

namespace nexrx {

//======================================================================
// Forward declarations
//======================================================================
struct IQFrame;

//======================================================================
// Transport result type
//======================================================================
enum class TransportError {
    None = 0,
    NotConnected,
    Timeout,
    BufferFull,
    BufferEmpty,
    InvalidData,
    IoError,
    Closed
};

template<typename T>
struct Result {
    T value;
    TransportError error;

    bool ok() const { return error == TransportError::None; }
    explicit operator bool() const { return ok(); }
};

//======================================================================
// Abstract Transport Interface
//======================================================================
class Transport {
public:
    virtual ~Transport() = default;

    // Connection lifecycle
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // Human-readable name for logging
    virtual std::string name() const = 0;
};

//======================================================================
// RPC Transport Interface
// For control messages (Request/Response pattern)
//======================================================================
class RpcTransport : public Transport {
public:
    // Send a serialized request, receive serialized response
    // Returns empty optional on timeout or error
    virtual Result<std::vector<uint8_t>> sendRequest(
        std::span<const uint8_t> request,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{1000}
    ) = 0;

    // For the server side: receive a request
    virtual Result<std::vector<uint8_t>> receiveRequest(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{1000}
    ) = 0;

    // For the server side: send a response
    virtual TransportError sendResponse(std::span<const uint8_t> response) = 0;
};

//======================================================================
// Streaming Transport Interface
// For high-bandwidth unidirectional data (I/Q samples)
//======================================================================
class StreamTransport : public Transport {
public:
    // Writer side: push frames to the stream
    virtual TransportError write(const IQFrame& frame) = 0;
    virtual TransportError writeBatch(std::span<const IQFrame> frames) = 0;

    // Reader side: pull frames from the stream
    virtual Result<IQFrame> read(
        std::chrono::milliseconds timeout = std::chrono::milliseconds{100}
    ) = 0;

    // Read multiple frames at once (more efficient)
    virtual Result<std::vector<IQFrame>> readBatch(
        size_t maxFrames,
        std::chrono::milliseconds timeout = std::chrono::milliseconds{100}
    ) = 0;

    // Check how many frames are available without blocking
    virtual size_t available() const = 0;

    // Buffer management
    virtual size_t capacity() const = 0;
    virtual void clear() = 0;
};

//======================================================================
// Callback-based async interface (optional extension)
//======================================================================
using IQFrameCallback = std::function<void(const IQFrame&)>;
using RequestCallback = std::function<std::vector<uint8_t>(std::span<const uint8_t>)>;

class AsyncTransport {
public:
    virtual ~AsyncTransport() = default;

    // Register callback for incoming I/Q frames
    virtual void setIQFrameCallback(IQFrameCallback callback) = 0;

    // Register callback for incoming RPC requests (server mode)
    virtual void setRequestCallback(RequestCallback callback) = 0;

    // Start/stop async processing
    virtual void startAsync() = 0;
    virtual void stopAsync() = 0;
};

} // namespace nexrx
