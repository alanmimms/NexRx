// NexRx App - Cross-platform Socket Abstraction
//
// Thin wrapper over POSIX and Winsock2 socket APIs.
// Provides portable socket types and functions.
//
// Copyright 2026 NexRx Project - MIT License

#pragma once

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>

    // Socket type
    using socket_t = SOCKET;
    constexpr socket_t SOCKET_INVALID = INVALID_SOCKET;

    // Error retrieval
    inline int socket_errno() { return WSAGetLastError(); }

    // Socket operations
    inline int socket_close(socket_t s) { return closesocket(s); }

    // Poll (Windows version)
    inline int socket_poll(WSAPOLLFD* fds, unsigned long nfds, int timeout) {
        return WSAPoll(fds, nfds, timeout);
    }

    // MSG_NOSIGNAL doesn't exist on Windows (SIGPIPE doesn't exist)
    #ifndef MSG_NOSIGNAL
        #define MSG_NOSIGNAL 0
    #endif

    // Poll structure alias
    using pollfd_t = WSAPOLLFD;

#else
    // POSIX systems (Linux, macOS)
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <poll.h>
    #include <fcntl.h>
    #include <errno.h>

    // Socket type
    using socket_t = int;
    constexpr socket_t SOCKET_INVALID = -1;

    // Error retrieval
    inline int socket_errno() { return errno; }

    // Socket operations
    inline int socket_close(socket_t s) { return close(s); }

    // Poll (POSIX version)
    inline int socket_poll(pollfd* fds, nfds_t nfds, int timeout) {
        return poll(fds, nfds, timeout);
    }

    // Poll structure alias
    using pollfd_t = pollfd;

#endif

#include <cstring>
#include <string>

namespace nexrx::net {

//======================================================================
// RAII Winsock Initializer
//
// On Windows, WSAStartup must be called before any socket operations.
// This class handles initialization and cleanup automatically.
// On POSIX systems, this is a no-op.
//
// Usage: Create one instance at application startup.
//======================================================================
class WinsockInit {
public:
    WinsockInit();
    ~WinsockInit();

    // Check if initialization succeeded
    bool ok() const { return ok_; }

    // Non-copyable, non-movable
    WinsockInit(const WinsockInit&) = delete;
    WinsockInit& operator=(const WinsockInit&) = delete;

private:
    bool ok_ = false;
};

//======================================================================
// Socket Utility Functions
//======================================================================

// Set socket to non-blocking mode
bool setNonBlocking(socket_t sock, bool nonBlocking = true);

// Set TCP_NODELAY (disable Nagle's algorithm)
bool setNoDelay(socket_t sock, bool noDelay = true);

// Set SO_REUSEADDR
bool setReuseAddr(socket_t sock, bool reuse = true);

// Set socket timeouts (send and receive)
bool setSocketTimeout(socket_t sock, int timeoutMs);

// Convert IPv4 address string to sockaddr_in
bool parseIPv4(const std::string& host, uint16_t port, sockaddr_in& addr);

// Get error message for socket error code
std::string socketErrorString(int errorCode);

// Get last socket error as string
inline std::string lastSocketError() {
    return socketErrorString(socket_errno());
}

} // namespace nexrx::net
