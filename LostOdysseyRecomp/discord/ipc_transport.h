#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace discord_rpc
{
std::vector<std::string> IpcEndpoints();
uint32_t ProcessId();

class IpcTransport
{
public:
    ~IpcTransport();
    using ConnectReporter = std::function<void(const std::string&, uint32_t)>;
    bool Connect(const std::vector<std::string>& endpoints, const ConnectReporter& report = {});
    void Close();
    bool IsOpen() const;
    uint32_t LastError() const { return error_; }
    // 0 is a timeout; -1 is a disconnected or failed connection.
    int Read(std::span<uint8_t> bytes, std::chrono::milliseconds timeout);
    bool Write(std::span<const uint8_t> bytes, std::chrono::milliseconds timeout);

private:
    uint32_t error_ = 0;
#ifdef _WIN32
    void* handle_ = reinterpret_cast<void*>(-1);
#else
    int fd_ = -1;
#endif
};
}
