// NexRx Digital Twin - Shared Memory Transport Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "SharedMemTransport.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <thread>

namespace nexrx {

SharedMemTransport::SharedMemTransport(const SharedMemConfig& config)
    : config_(config)
{
}

SharedMemTransport::~SharedMemTransport() {
    disconnect();
}

SharedMemTransport::SharedMemTransport(SharedMemTransport&& other) noexcept
    : config_(std::move(other.config_))
    , shm_fd_(other.shm_fd_)
    , mapped_(other.mapped_)
    , mapped_size_(other.mapped_size_)
    , header_(other.header_)
    , frames_(other.frames_)
{
    other.shm_fd_ = -1;
    other.mapped_ = nullptr;
    other.mapped_size_ = 0;
    other.header_ = nullptr;
    other.frames_ = nullptr;
}

SharedMemTransport& SharedMemTransport::operator=(SharedMemTransport&& other) noexcept {
    if (this != &other) {
        disconnect();
        config_ = std::move(other.config_);
        shm_fd_ = other.shm_fd_;
        mapped_ = other.mapped_;
        mapped_size_ = other.mapped_size_;
        header_ = other.header_;
        frames_ = other.frames_;

        other.shm_fd_ = -1;
        other.mapped_ = nullptr;
        other.mapped_size_ = 0;
        other.header_ = nullptr;
        other.frames_ = nullptr;
    }
    return *this;
}

bool SharedMemTransport::connect() {
    if (isConnected()) {
        return true;
    }

    // Calculate total size needed
    mapped_size_ = sizeof(IQRingBufferHeader) + config_.capacity * sizeof(IQFrame);

    if (config_.create) {
        // Producer: create and initialize shared memory
        shm_fd_ = shm_open(config_.name.c_str(), O_CREAT | O_RDWR, 0666);
        if (shm_fd_ < 0) {
            return false;
        }

        if (ftruncate(shm_fd_, mapped_size_) < 0) {
            cleanup();
            return false;
        }

        mapped_ = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (mapped_ == MAP_FAILED) {
            mapped_ = nullptr;
            cleanup();
            return false;
        }

        // Initialize header
        header_ = reinterpret_cast<IQRingBufferHeader*>(mapped_);
        std::memset(header_, 0, sizeof(IQRingBufferHeader));
        header_->magic = IQRingBufferHeader::MAGIC;
        header_->version = IQRingBufferHeader::VERSION;
        header_->capacity = config_.capacity;
        header_->frame_size = sizeof(IQFrame);

        // Frames start after header
        frames_ = reinterpret_cast<IQFrame*>(
            reinterpret_cast<uint8_t*>(mapped_) + sizeof(IQRingBufferHeader)
        );

        // Value-initialize all frames
        for (size_t i = 0; i < config_.capacity; ++i) {
            frames_[i] = IQFrame{};
        }

    } else {
        // Consumer: open existing shared memory
        shm_fd_ = shm_open(config_.name.c_str(), O_RDWR, 0666);
        if (shm_fd_ < 0) {
            return false;
        }

        // Get actual size
        struct stat st;
        if (fstat(shm_fd_, &st) < 0) {
            cleanup();
            return false;
        }
        mapped_size_ = st.st_size;

        mapped_ = mmap(nullptr, mapped_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (mapped_ == MAP_FAILED) {
            mapped_ = nullptr;
            cleanup();
            return false;
        }

        header_ = reinterpret_cast<IQRingBufferHeader*>(mapped_);

        // Validate header
        if (!header_->isValid()) {
            cleanup();
            return false;
        }

        config_.capacity = header_->capacity;
        frames_ = reinterpret_cast<IQFrame*>(
            reinterpret_cast<uint8_t*>(mapped_) + sizeof(IQRingBufferHeader)
        );
    }

    return true;
}

void SharedMemTransport::disconnect() {
    cleanup();
}

void SharedMemTransport::cleanup() {
    if (mapped_ != nullptr) {
        munmap(mapped_, mapped_size_);
        mapped_ = nullptr;
    }

    if (shm_fd_ >= 0) {
        close(shm_fd_);
        if (config_.create) {
            shm_unlink(config_.name.c_str());
        }
        shm_fd_ = -1;
    }

    header_ = nullptr;
    frames_ = nullptr;
    mapped_size_ = 0;
}

bool SharedMemTransport::isConnected() const {
    return mapped_ != nullptr && header_ != nullptr;
}

std::string SharedMemTransport::name() const {
    return "SharedMem:" + config_.name;
}

TransportError SharedMemTransport::write(const IQFrame& frame) {
    if (!isConnected()) {
        return TransportError::NotConnected;
    }

    // Load current positions (relaxed is OK for single producer)
    uint64_t write_pos = std::atomic_ref(header_->write_pos).load(std::memory_order_relaxed);
    uint64_t read_pos = std::atomic_ref(header_->read_pos).load(std::memory_order_acquire);

    // Check if buffer is full
    if (write_pos - read_pos >= config_.capacity) {
        std::atomic_ref(header_->overruns).fetch_add(1, std::memory_order_relaxed);
        return TransportError::BufferFull;
    }

    // Write frame
    size_t idx = write_pos % config_.capacity;
    frames_[idx] = frame;

    // Update write position with release semantics
    std::atomic_ref(header_->write_pos).store(write_pos + 1, std::memory_order_release);
    std::atomic_ref(header_->write_count).fetch_add(1, std::memory_order_relaxed);

    return TransportError::None;
}

TransportError SharedMemTransport::writeBatch(std::span<const IQFrame> frames) {
    for (const auto& frame : frames) {
        auto err = write(frame);
        if (err != TransportError::None) {
            return err;
        }
    }
    return TransportError::None;
}

Result<IQFrame> SharedMemTransport::read(std::chrono::milliseconds timeout) {
    if (!isConnected()) {
        return {{}, TransportError::NotConnected};
    }

    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (true) {
        // Load current positions
        uint64_t write_pos = std::atomic_ref(header_->write_pos).load(std::memory_order_acquire);
        uint64_t read_pos = std::atomic_ref(header_->read_pos).load(std::memory_order_relaxed);

        // Check if data available
        if (read_pos < write_pos) {
            // Read frame
            size_t idx = read_pos % config_.capacity;
            IQFrame frame = frames_[idx];

            // Update read position
            std::atomic_ref(header_->read_pos).store(read_pos + 1, std::memory_order_release);
            std::atomic_ref(header_->read_count).fetch_add(1, std::memory_order_relaxed);

            return {frame, TransportError::None};
        }

        // Check timeout
        if (std::chrono::steady_clock::now() >= deadline) {
            std::atomic_ref(header_->underruns).fetch_add(1, std::memory_order_relaxed);
            return {{}, TransportError::Timeout};
        }

        // Brief sleep to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

Result<std::vector<IQFrame>> SharedMemTransport::readBatch(
    size_t maxFrames,
    std::chrono::milliseconds timeout
) {
    if (!isConnected()) {
        return {{}, TransportError::NotConnected};
    }

    std::vector<IQFrame> result;
    result.reserve(maxFrames);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (result.size() < maxFrames) {
        uint64_t write_pos = std::atomic_ref(header_->write_pos).load(std::memory_order_acquire);
        uint64_t read_pos = std::atomic_ref(header_->read_pos).load(std::memory_order_relaxed);

        if (read_pos < write_pos) {
            size_t idx = read_pos % config_.capacity;
            result.push_back(frames_[idx]);

            std::atomic_ref(header_->read_pos).store(read_pos + 1, std::memory_order_release);
            std::atomic_ref(header_->read_count).fetch_add(1, std::memory_order_relaxed);
        } else {
            // No more data available
            if (!result.empty()) {
                // Return what we have
                break;
            }

            // Check timeout
            if (std::chrono::steady_clock::now() >= deadline) {
                std::atomic_ref(header_->underruns).fetch_add(1, std::memory_order_relaxed);
                return {{}, TransportError::Timeout};
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    return {std::move(result), TransportError::None};
}

size_t SharedMemTransport::available() const {
    return computeAvailable();
}

size_t SharedMemTransport::computeAvailable() const {
    if (!isConnected()) {
        return 0;
    }

    uint64_t write_pos = std::atomic_ref(header_->write_pos).load(std::memory_order_acquire);
    uint64_t read_pos = std::atomic_ref(header_->read_pos).load(std::memory_order_relaxed);

    return static_cast<size_t>(write_pos - read_pos);
}

size_t SharedMemTransport::capacity() const {
    return config_.capacity;
}

void SharedMemTransport::clear() {
    if (!isConnected()) {
        return;
    }

    // Set read position to write position (discard all pending data)
    uint64_t write_pos = std::atomic_ref(header_->write_pos).load(std::memory_order_acquire);
    std::atomic_ref(header_->read_pos).store(write_pos, std::memory_order_release);
}

uint64_t SharedMemTransport::overruns() const {
    if (!isConnected()) return 0;
    return std::atomic_ref(header_->overruns).load(std::memory_order_relaxed);
}

uint64_t SharedMemTransport::underruns() const {
    if (!isConnected()) return 0;
    return std::atomic_ref(header_->underruns).load(std::memory_order_relaxed);
}

uint64_t SharedMemTransport::writeCount() const {
    if (!isConnected()) return 0;
    return std::atomic_ref(header_->write_count).load(std::memory_order_relaxed);
}

uint64_t SharedMemTransport::readCount() const {
    if (!isConnected()) return 0;
    return std::atomic_ref(header_->read_count).load(std::memory_order_relaxed);
}

} // namespace nexrx
