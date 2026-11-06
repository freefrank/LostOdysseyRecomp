#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include <os/netlog.h>
#include <switch.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

namespace os::netlog
{
namespace
{
constexpr int kDefaultPort = 28780;
// Replayed to a client that connects after the game started.
constexpr size_t kHistory = 1 << 20;

enum State : int { Undecided, On, Off };
std::atomic<int> g_state{Undecided};
std::mutex g_mutex;
std::string g_history; // the newest bytes, between kHistory and 2 * kHistory
uint64_t g_end = 0;    // bytes ever written
int g_port = kDefaultPort;
Thread g_thread;

// The PC closed the connection, or it failed.
bool Closed(int client)
{
    char data[64];
    const ssize_t n = recv(client, data, sizeof(data), MSG_DONTWAIT);
    return n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK);
}

// One client at a time; the next one waits until the current one closes.
void Serve(void*)
{
    const int server = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(uint16_t(g_port));
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (server < 0 || bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        listen(server, 1) != 0)
    {
        std::fprintf(stderr, "netlog: cannot listen on port %d\n", g_port);
        return;
    }
    std::string chunk;
    for (;;)
    {
        const int client = accept(server, nullptr, nullptr);
        if (client < 0)
        {
            svcSleepThread(1'000'000'000);
            continue;
        }
        // A dead link (Wi-Fi drop, PC asleep) fails the send instead of
        // blocking this thread, and every later client, for minutes.
        const timeval sendTimeout{5, 0};
        setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, sizeof(sendTimeout));
        uint64_t sent;
        {
            std::lock_guard lock(g_mutex);
            sent = g_end - g_history.size();
        }
        for (bool open = true; open;)
        {
            {
                std::lock_guard lock(g_mutex);
                const uint64_t first = g_end - g_history.size();
                if (sent < first)
                    sent = first; // fell more than the history behind: skip the dropped part
                chunk.assign(g_history, size_t(sent - first), std::string::npos);
            }
            if (chunk.empty())
            {
                // Nothing to send, so only a read notices that the PC left.
                pollfd event{client, POLLIN, 0};
                if (poll(&event, 1, 50) > 0 && Closed(client))
                    break;
                continue;
            }
            for (size_t offset = 0; offset < chunk.size();)
            {
                const ssize_t n = send(client, chunk.data() + offset, chunk.size() - offset, 0);
                if (n <= 0)
                {
                    std::fprintf(stderr, "netlog: send failed (errno %d), dropping the connection\n", errno);
                    open = false;
                    break;
                }
                offset += size_t(n);
                sent += uint64_t(n);
            }
        }
        // Drop what is still unsent right away: its buffers come from a pool
        // sized for about one connection, and the next client needs them.
        const linger abort{1, 0};
        setsockopt(client, SOL_SOCKET, SO_LINGER, &abort, sizeof(abort));
        close(client);
    }
}
}

bool Start(const char* setting, bool debugServer)
{
    const bool netlog = setting && *setting && std::strcmp(setting, "0") != 0;
    if (!netlog && !debugServer)
    {
        g_state = Off;
        std::lock_guard lock(g_mutex);
        std::string().swap(g_history);
        return false;
    }
    if (const int port = netlog ? std::atoi(setting) : 0; port > 1 && port < 65536)
        g_port = port;

    // The socket service's buffers come from a transfer memory block taken from the same heap as guest memory:
    // sb_efficiency * page_round(tcp_tx_max + tcp_rx_max + udp_tx + udp_rx) (libnx bsdInitialize), whatever the
    // number of sockets. libnx's default is 4 * 0x8d000 = 2.2 MiB.
    // The network log alone: small buffers, the log only goes out. 2 * 0x2e000 = 368 KiB.
    SocketInitConfig config = *socketGetDefaultInitConfig();
    config.tcp_rx_buf_size = 0x1000;
    config.tcp_rx_buf_max_size = 0;
    config.tcp_tx_buf_max_size = 0x20000;
    config.sb_efficiency = 2; // room for a closing connection's buffers next to the new one
    config.num_bsd_sessions = 2;
    if (debugServer)
    {
        // The debug server also receives (uploads: a 110 MB NRO), and up to 4 connections and the network log can
        // send at once (the log stream, a screenshot, a file): 128 KiB windows both ways, 4 buffers of each.
        // 4 * 0x4d000 = 1.2 MiB, only while it is turned on. One session per thread that can block in the socket
        // service at the same time (the listener, 4 connections; the network log's 2), so none waits for another.
        config.tcp_rx_buf_size = 0x4000;
        config.tcp_rx_buf_max_size = 0x20000;
        config.sb_efficiency = 4;
        config.num_bsd_sessions = (netlog ? 2 : 0) + 1 + 4;
    }
    const auto page = [](uint32_t size) { return (size + 0xFFF) & ~0xFFFu; };
    const uint32_t transferBytes = config.sb_efficiency * page(
        (config.tcp_tx_buf_max_size ? config.tcp_tx_buf_max_size : config.tcp_tx_buf_size) +
        (config.tcp_rx_buf_max_size ? config.tcp_rx_buf_max_size : config.tcp_rx_buf_size) +
        config.udp_tx_buf_size + config.udp_rx_buf_size);
    if (const Result rc = socketInitialize(&config); R_FAILED(rc))
    {
        g_state = Off;
        std::fprintf(stderr, "netlog: socketInitialize failed (%#x)\n", rc);
        return false;
    }
    std::fprintf(stderr, "network: socket service up (%u KiB of socket buffers, %u sessions)\n", transferBytes >> 10,
        config.num_bsd_sessions);
    // The debug server's log stream reads the history too.
    g_state = On;
    if (!netlog)
        return true;
    // Above the game's threads (0x2C): Horizon runs strictly by priority, and a
    // busy game thread on the same core would starve a lower one. It sleeps
    // between sends, so it costs the game little.
    if (R_FAILED(threadCreate(&g_thread, Serve, nullptr, nullptr, 0x10000, 0x2B, -2)) ||
        R_FAILED(threadStart(&g_thread)))
    {
        std::fprintf(stderr, "netlog: cannot start its thread\n");
        if (!debugServer)
            g_state = Off; // nobody reads the history
        return debugServer;
    }
    std::fprintf(stderr, "netlog: listening on port %d (tools/switch/netlog.py <console IP>)\n", g_port);
    return true;
}

uint64_t Read(uint64_t& position, std::string& text)
{
    std::lock_guard lock(g_mutex);
    const uint64_t first = g_end - g_history.size();
    const uint64_t dropped = position < first ? first - position : 0;
    const uint64_t from = std::max(position, first);
    text.append(g_history, size_t(std::min(from, g_end) - first), std::string::npos);
    position = g_end;
    return dropped;
}

void Write(const char* data, size_t size)
{
    // A crash inside the locked section reaches here again on the same thread
    // through the crash handler's stderr flush.
    thread_local bool writing = false;
    if (!size || writing || g_state.load(std::memory_order_relaxed) == Off)
        return;
    writing = true;
    {
        std::lock_guard lock(g_mutex);
        g_history.append(data, size);
        g_end += size;
        if (g_history.size() > 2 * kHistory)
            g_history.erase(0, g_history.size() - kHistory);
    }
    writing = false;
}
}
#endif
