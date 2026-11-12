#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)

#include "ipc_transport.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace discord_rpc
{
std::vector<std::string> IpcEndpoints()
{
    std::vector<std::string> result;
#ifdef _WIN32
    for (int i = 0; i < 10; ++i)
        result.push_back("\\\\.\\pipe\\discord-ipc-" + std::to_string(i));
#else
    std::vector<std::string> roots;
    for (const auto* name : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"})
        if (const auto* value = std::getenv(name); value && *value)
            roots.emplace_back(value);
    roots.emplace_back("/tmp");
    for (auto root : roots)
    {
        while (root.size() > 1 && root.back() == '/') root.pop_back();
        for (const auto* suffix : {"", "/app/com.discordapp.Discord", "/app/com.discordapp.DiscordCanary",
            "/app/com.discordapp.DiscordPTB", "/snap.discord", "/snap.discord-canary"})
        {
            for (int i = 0; i < 10; ++i)
            {
                auto path = root + suffix + "/discord-ipc-" + std::to_string(i);
                if (std::find(result.begin(), result.end(), path) == result.end())
                    result.push_back(std::move(path));
            }
        }
    }
#endif
    return result;
}

uint32_t ProcessId()
{
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return uint32_t(getpid());
#endif
}

IpcTransport::~IpcTransport() { Close(); }

bool IpcTransport::IsOpen() const
{
#ifdef _WIN32
    return handle_ != INVALID_HANDLE_VALUE;
#else
    return fd_ >= 0;
#endif
}

void IpcTransport::Close()
{
#ifdef _WIN32
    if (IsOpen()) CloseHandle(handle_);
    handle_ = INVALID_HANDLE_VALUE;
#else
    if (IsOpen()) close(fd_);
    fd_ = -1;
#endif
}

bool IpcTransport::Connect(const std::vector<std::string>& endpoints, const ConnectReporter& report)
{
    Close();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    for (const auto& endpoint : endpoints)
    {
#ifdef _WIN32
        handle_ = CreateFileA(endpoint.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED | SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr);
        error_ = IsOpen() ? 0 : GetLastError();
        if (report) report(endpoint, error_);
        if (IsOpen()) return true;
#else
        sockaddr_un address{};
        if (endpoint.size() >= sizeof(address.sun_path))
        {
            error_ = ENAMETOOLONG;
            if (report) report(endpoint, error_);
            continue;
        }
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, endpoint.c_str(), endpoint.size() + 1);
        fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0)
        {
            error_ = errno;
            if (report) report(endpoint, error_);
            continue;
        }
        const auto flags = fcntl(fd_, F_GETFL, 0);
        if (flags < 0 || fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            error_ = errno;
            Close();
            if (report) report(endpoint, error_);
            continue;
        }
        fcntl(fd_, F_SETFD, FD_CLOEXEC);
#ifdef SO_NOSIGPIPE
        int noSignal = 1;
        setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
#endif
        if (connect(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0)
        {
            error_ = 0;
            if (report) report(endpoint, error_);
            return true;
        }
        error_ = errno;
        if (errno == EINPROGRESS)
        {
            pollfd descriptor{fd_, POLLOUT, 0};
            if (poll(&descriptor, 1, 10) > 0)
            {
                int error = 0;
                socklen_t length = sizeof(error);
                if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &length) == 0)
                {
                    error_ = error;
                    if (error == 0)
                    {
                        if (report) report(endpoint, error_);
                        return true;
                    }
                }
                else error_ = errno;
            }
        }
        Close();
        if (report) report(endpoint, error_);
#endif
        if (std::chrono::steady_clock::now() >= deadline) break;
    }
    return false;
}

#ifdef _WIN32
namespace
{
int Transfer(HANDLE pipe, void* bytes, DWORD length, bool write, std::chrono::milliseconds timeout, uint32_t& error)
{
    OVERLAPPED operation{};
    operation.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!operation.hEvent) { error = GetLastError(); return -1; }
    DWORD count = 0;
    const auto complete = write ? WriteFile(pipe, bytes, length, &count, &operation) :
        ReadFile(pipe, bytes, length, &count, &operation);
    bool timedOut = false;
    bool success = complete != FALSE;
    error = success ? 0 : GetLastError();
    if (!success && error == ERROR_IO_PENDING)
    {
        const auto wait = WaitForSingleObject(operation.hEvent, DWORD(timeout.count()));
        timedOut = wait == WAIT_TIMEOUT;
        if (wait != WAIT_OBJECT_0)
            CancelIoEx(pipe, &operation);
        success = GetOverlappedResult(pipe, &operation, &count, TRUE) != FALSE;
        error = success ? 0 : GetLastError();
    }
    CloseHandle(operation.hEvent);
    if (success) return count ? int(count) : -1;
    if (timedOut) error = WAIT_TIMEOUT;
    return timedOut ? 0 : -1;
}
}
#endif

int IpcTransport::Read(std::span<uint8_t> bytes, std::chrono::milliseconds timeout)
{
    if (!IsOpen()) return -1;
#ifdef _WIN32
    return Transfer(handle_, bytes.data(), DWORD(bytes.size()), false, timeout, error_);
#else
    pollfd descriptor{fd_, POLLIN, 0};
    const auto ready = poll(&descriptor, 1, int(timeout.count()));
    if (!ready || (ready < 0 && errno == EINTR)) return 0;
    if (ready < 0) { error_ = errno; return -1; }
    const auto count = recv(fd_, bytes.data(), bytes.size(), 0);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return 0;
    error_ = count < 0 ? uint32_t(errno) : 0;
    return count > 0 ? int(count) : -1;
#endif
}

bool IpcTransport::Write(std::span<const uint8_t> bytes, std::chrono::milliseconds timeout)
{
    if (!IsOpen()) return false;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!bytes.empty())
    {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
        if (remaining.count() <= 0)
        {
#ifdef _WIN32
            error_ = WAIT_TIMEOUT;
#else
            error_ = ETIMEDOUT;
#endif
            return false;
        }
#ifdef _WIN32
        const auto count = Transfer(handle_, const_cast<uint8_t*>(bytes.data()), DWORD(bytes.size()), true, remaining, error_);
        if (count <= 0) return false;
#else
        pollfd descriptor{fd_, POLLOUT, 0};
        const auto ready = poll(&descriptor, 1, int(remaining.count()));
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) { error_ = ready < 0 ? errno : ETIMEDOUT; return false; }
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        const auto count = send(fd_, bytes.data(), bytes.size(), flags);
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) continue;
        if (count <= 0) { error_ = count < 0 ? uint32_t(errno) : 0; return false; }
#endif
        bytes = bytes.subspan(size_t(count));
    }
    return true;
}
}

#endif
