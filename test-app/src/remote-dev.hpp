#pragma once
#include <string>
#include <vector>
#include <memory>
#include "TwinConn.hpp"

namespace nexrx {

class RemoteDevice {
public:
    RemoteDevice() = default;
    bool connect(const std::string& host, int controlPort, int streamPort);
    void disconnect();
    bool isConnected() const;

    TwinConn& conn() { return conn_; }

private:
    TwinConn conn_;
};

} // namespace nexrx
