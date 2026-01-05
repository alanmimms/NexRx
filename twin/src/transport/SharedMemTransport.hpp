// NexRx Digital Twin - Shared Memory Transport
//
// Lock-free SPSC ring buffer over POSIX shared memory for I/Q data.
// Producer: Xyce simulation / ADC sampler
// Consumer: Host application / DSP pipeline
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#include "Transport.hpp"
#include "IQFrame.hpp"

#include <atomic>
#include <string>

namespace nexrx {

//======================================================================
// Shared Memory Transport Configuration
//======================================================================
struct SharedMemConfig {
    std::string name = "/nexrx_iq";     // Shared memory name (must start with /)
    size_t capacity = 8192;              // Number of frames (8192 = ~85ms at 96kHz)
    bool create = false;                 // true = create (producer), false = open (consumer)
};

//======================================================================
// Shared Memory Stream Transport
//
// Implements StreamTransport using POSIX shared memory with a lock-free
// single-producer single-consumer (SPSC) ring buffer.
//
// Usage:
//   Producer (simulation):
//     SharedMemTransport tx(SharedMemConfig{.name="/nexrx_iq", .create=true});
//     tx.connect();
//     tx.write(frame);
//
//   Consumer (host app):
//     SharedMemTransport rx(SharedMemConfig{.name="/nexrx_iq", .create=false});
//     rx.connect();
//     auto frame = rx.read();
//======================================================================
class SharedMemTransport : public StreamTransport {
public:
    explicit SharedMemTransport(const SharedMemConfig& config);
    ~SharedMemTransport() override;

    // Non-copyable, movable
    SharedMemTransport(const SharedMemTransport&) = delete;
    SharedMemTransport& operator=(const SharedMemTransport&) = delete;
    SharedMemTransport(SharedMemTransport&& other) noexcept;
    SharedMemTransport& operator=(SharedMemTransport&& other) noexcept;

    // Transport interface
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    std::string name() const override;

    // StreamTransport interface
    TransportError write(const IQFrame& frame) override;
    TransportError writeBatch(std::span<const IQFrame> frames) override;
    Result<IQFrame> read(std::chrono::milliseconds timeout) override;
    Result<std::vector<IQFrame>> readBatch(size_t maxFrames, std::chrono::milliseconds timeout) override;
    size_t available() const override;
    size_t capacity() const override;
    void clear() override;

    // Statistics
    uint64_t overruns() const;
    uint64_t underruns() const;
    uint64_t writeCount() const;
    uint64_t readCount() const;

private:
    void cleanup();
    size_t computeAvailable() const;

    SharedMemConfig config_;
    int shm_fd_ = -1;
    void* mapped_ = nullptr;
    size_t mapped_size_ = 0;

    // Pointers into shared memory
    IQRingBufferHeader* header_ = nullptr;
    IQFrame* frames_ = nullptr;
};

} // namespace nexrx
