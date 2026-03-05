// NexRx App - Cross-platform Socket Abstraction Implementation
//
// Copyright 2026 NexRx Project - MIT License

#include "Socket.hpp"

namespace nexrx::net {

//======================================================================
// WinsockInit Implementation
//======================================================================

#ifdef _WIN32

WinsockInit::WinsockInit() {
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  initialized = (result == 0);
}

WinsockInit::~WinsockInit() {
  if (initialized) {
    WSACleanup();
  }
}

#else

// POSIX: no initialization needed
WinsockInit::WinsockInit() : initialized(true) {}
WinsockInit::~WinsockInit() {}

#endif

//======================================================================
// Socket Utility Functions
//======================================================================

bool setNonBlocking(socket_t sock, bool nonBlocking) {
#ifdef _WIN32
  u_long mode = nonBlocking ? 1 : 0;
  return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  if (nonBlocking) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }
  return fcntl(sock, F_SETFL, flags) == 0;
#endif
}

bool setNoDelay(socket_t sock, bool noDelay) {
  int flag = noDelay ? 1 : 0;
  return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
                    reinterpret_cast<const char*>(&flag), sizeof(flag)) == 0;
}

bool setReuseAddr(socket_t sock, bool reuse) {
  int flag = reuse ? 1 : 0;
  return setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                    reinterpret_cast<const char*>(&flag), sizeof(flag)) == 0;
}

bool setSocketTimeout(socket_t sock, int timeoutMS) {
#ifdef _WIN32
  DWORD timeout = static_cast<DWORD>(timeoutMS);
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeout), sizeof(timeout)) != 0) {
    return false;
  }
  if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
                 reinterpret_cast<const char*>(&timeout), sizeof(timeout)) != 0) {
    return false;
  }
#else
  struct timeval tv;
  tv.tv_sec = timeoutMS / 1000;
  tv.tv_usec = (timeoutMS % 1000) * 1000;
  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
    return false;
  }
  if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
    return false;
  }
#endif
  return true;
}

bool parseIPv4(const std::string& host, uint16_t port, sockaddr_in& addr) {
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (host.empty() || host == "0.0.0.0") {
    addr.sin_addr.s_addr = INADDR_ANY;
    return true;
  }

  return inet_pton(AF_INET, host.c_str(), &addr.sin_addr) == 1;
}

std::string socketErrorString(int errorCode) {
#ifdef _WIN32
  char* msg = nullptr;
  FormatMessageA(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr,
    errorCode,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    reinterpret_cast<LPSTR>(&msg),
    0,
    nullptr
  );
  if (msg) {
    std::string result(msg);
    LocalFree(msg);
    // Remove trailing newlines
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
      result.pop_back();
    }
    return result;
  }
  return "Unknown error " + std::to_string(errorCode);
#else
  return std::strerror(errorCode);
#endif
}

} // namespace nexrx::net
