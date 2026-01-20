// NexRx App - UDP Stream Client Implementation
//
// Uses tinycbor for efficient CBOR encoding/decoding.
//
// Copyright 2026 NexRx Project - MIT License

#include "UdpStreamClient.hpp"

#include <cbor.h>

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace {

// Boost thread priority for real-time audio processing
void boostThreadPriority() {
#ifdef _WIN32
    // Windows: set thread to high priority
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#else
    // POSIX: try to set real-time scheduling (may require privileges)
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO) / 2;
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
        // Fall back - requires root or CAP_SYS_NICE
    }
#endif
}

} // anonymous namespace

namespace nexrx {

UdpStreamClient::UdpStreamClient(const UdpStreamClientConfig& config)
    : config_(config)
{
    receiveBuffer_.resize(config_.receiveBufferSize);
}

UdpStreamClient::~UdpStreamClient() {
    disconnect();
}

bool UdpStreamClient::connect() {
    if (isConnected()) {
        return true;
    }

    // Create UDP socket
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ == SOCKET_INVALID) {
        return false;
    }

    // Allow address reuse
    net::setReuseAddr(socket_, true);

    // Bind to receive port
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
        socket_close(socket_);
        socket_ = SOCKET_INVALID;
        return false;
    }

    // Reset state
    writePos_.store(0, std::memory_order_relaxed);
    readPos_.store(0, std::memory_order_relaxed);
    lastSequence_.store(0, std::memory_order_relaxed);
    firstFrame_.store(true, std::memory_order_relaxed);
    packetsReceived_.store(0, std::memory_order_relaxed);
    framesReceived_.store(0, std::memory_order_relaxed);
    framesDropped_.store(0, std::memory_order_relaxed);
    bufferOverruns_.store(0, std::memory_order_relaxed);

    // Start receive thread
    running_.store(true, std::memory_order_release);
    receiveThread_ = std::thread(&UdpStreamClient::receiveLoop, this);

    return true;
}

void UdpStreamClient::disconnect() {
    // Stop receive thread
    running_.store(false, std::memory_order_release);
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }

    // Close socket
    if (socket_ != SOCKET_INVALID) {
        socket_close(socket_);
        socket_ = SOCKET_INVALID;
    }
}

bool UdpStreamClient::isConnected() const {
    return socket_ != SOCKET_INVALID;
}

std::string UdpStreamClient::name() const {
    return "UdpStreamClient:" + std::to_string(config_.port);
}

TransportError UdpStreamClient::write(const IQFrame& /*frame*/) {
    // Client doesn't write
    return TransportError::InvalidData;
}

TransportError UdpStreamClient::writeBatch(std::span<const IQFrame> /*frames*/) {
    // Client doesn't write
    return TransportError::InvalidData;
}

Result<IQFrame> UdpStreamClient::read(std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;

    do {
        size_t readPos = readPos_.load(std::memory_order_relaxed);
        size_t writePos = writePos_.load(std::memory_order_acquire);

        if (readPos != writePos) {
            IQFrame frame = receiveBuffer_[readPos];
            readPos_.store((readPos + 1) % config_.receiveBufferSize,
                          std::memory_order_release);
            return {frame, TransportError::None};
        }

        if (timeout.count() > 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    } while (timeout.count() > 0 && std::chrono::steady_clock::now() < deadline);

    // Check one more time for timeout=0 case
    size_t readPos = readPos_.load(std::memory_order_relaxed);
    size_t writePos = writePos_.load(std::memory_order_acquire);
    if (readPos != writePos) {
        IQFrame frame = receiveBuffer_[readPos];
        readPos_.store((readPos + 1) % config_.receiveBufferSize,
                      std::memory_order_release);
        return {frame, TransportError::None};
    }

    return {{}, TransportError::BufferEmpty};
}

Result<std::vector<IQFrame>> UdpStreamClient::readBatch(
    size_t maxFrames,
    std::chrono::milliseconds timeout
) {
    std::vector<IQFrame> frames;
    frames.reserve(maxFrames);

    auto deadline = std::chrono::steady_clock::now() + timeout;

    while (frames.size() < maxFrames) {
        size_t readPos = readPos_.load(std::memory_order_relaxed);
        size_t writePos = writePos_.load(std::memory_order_acquire);

        if (readPos != writePos) {
            frames.push_back(receiveBuffer_[readPos]);
            readPos_.store((readPos + 1) % config_.receiveBufferSize,
                          std::memory_order_release);
        } else if (timeout.count() == 0 || std::chrono::steady_clock::now() >= deadline) {
            break;
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    if (frames.empty()) {
        return {{}, TransportError::BufferEmpty};
    }

    return {std::move(frames), TransportError::None};
}

size_t UdpStreamClient::available() const {
    size_t writePos = writePos_.load(std::memory_order_acquire);
    size_t readPos = readPos_.load(std::memory_order_relaxed);

    if (writePos >= readPos) {
        return writePos - readPos;
    } else {
        return config_.receiveBufferSize - readPos + writePos;
    }
}

size_t UdpStreamClient::capacity() const {
    return config_.receiveBufferSize;
}

void UdpStreamClient::clear() {
    readPos_.store(writePos_.load(std::memory_order_acquire),
                   std::memory_order_release);
}

bool UdpStreamClient::sendHolePunch() {
    if (socket_ == SOCKET_INVALID || config_.serverHost.empty()) {
        return false;
    }

    // Set up server address
    sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(config_.serverPort);

    if (inet_pton(AF_INET, config_.serverHost.c_str(), &serverAddr.sin_addr) != 1) {
        return false;
    }

    // Send a valid CBOR hole punch packet to punch through NAT
    // Format: ["NXRQ", version, TYPE_HOLE_PUNCH, []] (empty frames)
    uint8_t buffer[64];
    CborEncoder encoder;
    cbor_encoder_init(&encoder, buffer, sizeof(buffer), 0);

    CborEncoder packetArray;
    cbor_encoder_create_array(&encoder, &packetArray, 4);

    cbor_encode_text_stringz(&packetArray, UdpProtocol::MAGIC);
    cbor_encode_uint(&packetArray, UdpProtocol::VERSION);
    cbor_encode_uint(&packetArray, UdpProtocol::TYPE_HOLE_PUNCH);

    // Empty frames array
    CborEncoder framesArray;
    cbor_encoder_create_array(&packetArray, &framesArray, 0);
    cbor_encoder_close_container(&packetArray, &framesArray);

    cbor_encoder_close_container(&encoder, &packetArray);

    size_t len = cbor_encoder_get_buffer_size(&encoder, buffer);

    int sent = sendto(socket_,
                      reinterpret_cast<const char*>(buffer),
                      static_cast<int>(len),
                      0,
                      reinterpret_cast<struct sockaddr*>(&serverAddr),
                      sizeof(serverAddr));

    return sent > 0;
}

void UdpStreamClient::receiveLoop() {
    // Boost thread priority for real-time audio processing
    boostThreadPriority();

    constexpr size_t MAX_PACKET_SIZE = 4096;
    uint8_t buffer[MAX_PACKET_SIZE];

    while (running_.load(std::memory_order_acquire)) {
        // Poll for data with timeout
        pollfd_t pfd;
        pfd.fd = socket_;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = socket_poll(&pfd, 1, 100);  // 100ms timeout
        if (ret <= 0) {
            continue;  // Error or timeout
        }

        // Receive packet
        sockaddr_in fromAddr;
        socklen_t fromLen = sizeof(fromAddr);

        int n = recvfrom(socket_,
                         reinterpret_cast<char*>(buffer),
                         static_cast<int>(sizeof(buffer)),
                         0,
                         reinterpret_cast<struct sockaddr*>(&fromAddr),
                         &fromLen);

        if (n <= 0) {
            continue;
        }

        // Parse CBOR packet
        CborParser parser;
        CborValue it;
        if (cbor_parser_init(buffer, static_cast<size_t>(n), 0, &parser, &it) != CborNoError) {
            continue;
        }

        // Expect outer array
        if (!cbor_value_is_array(&it)) {
            continue;
        }

        CborValue packetArray;
        if (cbor_value_enter_container(&it, &packetArray) != CborNoError) {
            continue;
        }

        // Magic string
        if (!cbor_value_is_text_string(&packetArray)) {
            continue;
        }
        char magic[8];
        size_t magic_len = sizeof(magic);
        if (cbor_value_copy_text_string(&packetArray, magic, &magic_len, &packetArray) != CborNoError) {
            continue;
        }
        if (strcmp(magic, UdpProtocol::MAGIC) != 0) {
            continue;
        }

        // Version
        if (!cbor_value_is_unsigned_integer(&packetArray)) {
            continue;
        }
        uint64_t version;
        cbor_value_get_uint64(&packetArray, &version);
        cbor_value_advance(&packetArray);
        if (version != UdpProtocol::VERSION) {
            continue;
        }

        // Type
        if (!cbor_value_is_unsigned_integer(&packetArray)) {
            continue;
        }
        uint64_t type;
        cbor_value_get_uint64(&packetArray, &type);
        cbor_value_advance(&packetArray);
        if (type != UdpProtocol::TYPE_IQ_DATA) {
            continue;  // Not I/Q data (might be hole punch)
        }

        // Frames array
        if (!cbor_value_is_array(&packetArray)) {
            continue;
        }

        CborValue framesArray;
        if (cbor_value_enter_container(&packetArray, &framesArray) != CborNoError) {
            continue;
        }

        packetsReceived_.fetch_add(1, std::memory_order_relaxed);

        // Process frames
        while (!cbor_value_at_end(&framesArray)) {
            if (!cbor_value_is_array(&framesArray)) {
                cbor_value_advance(&framesArray);
                continue;
            }

            CborValue frameArray;
            if (cbor_value_enter_container(&framesArray, &frameArray) != CborNoError) {
                cbor_value_advance(&framesArray);
                continue;
            }

            IQFrame frame;
            uint64_t u64val;
            int64_t i64val;

            // sequence
            if (!cbor_value_is_unsigned_integer(&frameArray)) goto next_frame;
            cbor_value_get_uint64(&frameArray, &u64val);
            frame.sequence = static_cast<uint32_t>(u64val);
            cbor_value_advance(&frameArray);

            // timestamp_ns
            if (!cbor_value_is_unsigned_integer(&frameArray)) goto next_frame;
            cbor_value_get_uint64(&frameArray, &u64val);
            frame.timestamp_ns = u64val;
            cbor_value_advance(&frameArray);

            // i0, q0, i1, q1, i2, q2
            for (int ch = 0; ch < 3; ++ch) {
                // i
                if (!cbor_value_is_integer(&frameArray)) goto next_frame;
                cbor_value_get_int64(&frameArray, &i64val);
                frame.qsd[ch].i = static_cast<int32_t>(i64val);
                cbor_value_advance(&frameArray);

                // q
                if (!cbor_value_is_integer(&frameArray)) goto next_frame;
                cbor_value_get_int64(&frameArray, &i64val);
                frame.qsd[ch].q = static_cast<int32_t>(i64val);
                cbor_value_advance(&frameArray);
            }

            frame.flags = 0;

            // Check for dropped frames via sequence number
            if (firstFrame_.load(std::memory_order_relaxed)) {
                firstFrame_.store(false, std::memory_order_relaxed);
                lastSequence_.store(frame.sequence, std::memory_order_relaxed);
            } else {
                uint32_t expected = lastSequence_.load(std::memory_order_relaxed) + 1;
                if (frame.sequence != expected) {
                    uint32_t dropped = frame.sequence - expected;
                    framesDropped_.fetch_add(dropped, std::memory_order_relaxed);
                }
                lastSequence_.store(frame.sequence, std::memory_order_relaxed);
            }

            // Add to ring buffer
            {
                size_t writePos = writePos_.load(std::memory_order_relaxed);
                size_t nextWritePos = (writePos + 1) % config_.receiveBufferSize;
                size_t readPos = readPos_.load(std::memory_order_acquire);

                if (nextWritePos == readPos) {
                    // Buffer full - overrun
                    bufferOverruns_.fetch_add(1, std::memory_order_relaxed);
                    goto next_frame;
                }

                receiveBuffer_[writePos] = frame;
                writePos_.store(nextWritePos, std::memory_order_release);
                framesReceived_.fetch_add(1, std::memory_order_relaxed);
            }

        next_frame:
            cbor_value_leave_container(&framesArray, &frameArray);
        }

        cbor_value_leave_container(&packetArray, &framesArray);
    }
}

} // namespace nexrx
