#pragma once

#include <chrono>
#include <cstdint>
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
    bool Connect(const std::vector<std::string>& endpoints);
    void Close();
    bool IsOpen() const;
    // 0 is a timeout; -1 is a disconnected or failed connection.
    int Read(std::span<uint8_t> bytes, std::chrono::milliseconds timeout);
    bool Write(std::span<const uint8_t> bytes, std::chrono::milliseconds timeout);

private:
#ifdef _WIN32
    void* handle_ = reinterpret_cast<void*>(-1);
#else
    int fd_ = -1;
#endif
};
}
