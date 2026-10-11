#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include <os/netlog.h>
#include <switch.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
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

// One client at a time; a new connection replaces nothing until the current
// one is closed by the PC.
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
                svcSleepThread(50'000'000);
                continue;
            }
            for (size_t offset = 0; offset < chunk.size();)
            {
                const ssize_t n = send(client, chunk.data() + offset, chunk.size() - offset, 0);
                if (n <= 0)
                {
                    open = false;
                    break;
                }
                offset += size_t(n);
                sent += uint64_t(n);
            }
        }
        close(client);
    }
}
}

void Start(const char* setting)
{
    if (!setting || !*setting || std::strcmp(setting, "0") == 0)
    {
        g_state = Off;
        std::lock_guard lock(g_mutex);
        std::string().swap(g_history);
        return;
    }
    if (const int port = std::atoi(setting); port > 1 && port < 65536)
        g_port = port;

    // Small socket buffers: the log only goes out, and the memory comes from
    // the same heap as guest memory.
    SocketInitConfig config = *socketGetDefaultInitConfig();
    config.tcp_rx_buf_size = 0x1000;
    config.tcp_rx_buf_max_size = 0;
    config.tcp_tx_buf_max_size = 0x20000;
    config.sb_efficiency = 1;
    config.num_bsd_sessions = 2;
    if (const Result rc = socketInitialize(&config); R_FAILED(rc))
    {
        g_state = Off;
        std::fprintf(stderr, "netlog: socketInitialize failed (%#x)\n", rc);
        return;
    }
    if (R_FAILED(threadCreate(&g_thread, Serve, nullptr, nullptr, 0x10000, 0x3B, -2)) ||
        R_FAILED(threadStart(&g_thread)))
    {
        g_state = Off;
        std::fprintf(stderr, "netlog: cannot start its thread\n");
        return;
    }
    g_state = On;
    std::fprintf(stderr, "netlog: listening on port %d (tools/switch/netlog.py <console IP>)\n", g_port);
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
