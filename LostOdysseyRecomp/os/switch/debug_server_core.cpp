// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Adapted from SwitchWakerHD's runtime/src/platform/debug_server.cpp (github.com/centollOS/SwitchWakerHD, commit
// cfd44c0a26182361820ac59859c1d94243ad8ea0, MPL-2.0); see debug_server_core.h for what changed.
//
// Debug server core (debug_server_core.h): one listening thread, one thread per connection (at most kMaxConns, joined
// by the listener once they end), commands looked up by name.
#include "debug_server_core.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace debugsrv
{
namespace
{
Config g_cfg;
std::atomic<bool> g_running{false};
int g_listen = -1;

struct Command
{
    std::string usage;
    Handler handler;
};
std::mutex g_cmdMu;
std::map<std::string, Command> g_cmds;

constexpr int kMaxConns = 4;
// A connection that sends nothing for this long is closed, so a forgotten client does not keep a slot (and a socket
// service session) for the rest of the session.
constexpr int kIdleSeconds = 600;
// A send that cannot go out for this long fails (the link dropped, the PC went to sleep) instead of blocking.
constexpr int kSendSeconds = 15;

// One connection: its socket and its thread. state: 0 free, 1 running, 2 ended (the listener joins it).
struct Slot
{
    std::atomic<int> state{0};
    int fd = -1;
#ifdef __SWITCH__
    ::Thread thread{};
#endif
};
Slot g_slots[kMaxConns];
std::mutex g_sockMu; // fd of the slots, closed by the connection or by stop

void say(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void say(const char* fmt, ...)
{
    if (!g_cfg.log)
        return;
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    g_cfg.log(line);
}

void wait_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

// ---------------------------------------------------------------- connections
struct Conn
{
    int fd = -1;
    std::string in; // bytes received and not consumed yet
};

bool send_all(int fd, const void* data, size_t n)
{
    const char* p = static_cast<const char*>(data);
    while (n)
    {
#ifdef MSG_NOSIGNAL
        const ssize_t w = send(fd, p, n, MSG_NOSIGNAL);
#else
        const ssize_t w = send(fd, p, n, 0);
#endif
        if (w <= 0)
            return false;
        p += w;
        n -= size_t(w);
    }
    return true;
}
bool send_header(int fd, bool ok, uint64_t n)
{
    char h[48];
    const int k = snprintf(h, sizeof h, "%s %llu\n", ok ? "ok" : "err", (unsigned long long)n);
    return send_all(fd, h, size_t(k));
}

bool fill(Conn& c)
{
    char buf[16384];
    const ssize_t r = recv(c.fd, buf, sizeof buf, 0);
    if (r <= 0)
        return false;
    c.in.append(buf, size_t(r));
    return true;
}
bool read_line(Conn& c, std::string& line)
{
    for (;;)
    {
        const size_t nl = c.in.find('\n');
        if (nl != std::string::npos)
        {
            line = c.in.substr(0, nl);
            c.in.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            return true;
        }
        if (c.in.size() > 4096 || !fill(c))
            return false;
    }
}

Args split(const std::string& line)
{
    Args out;
    size_t i = 0;
    while (i < line.size())
    {
        while (i < line.size() && line[i] == ' ')
            i++;
        if (i >= line.size())
            break;
        std::string a;
        if (line[i] == '"')
        {
            size_t e = line.find('"', i + 1);
            if (e == std::string::npos)
                e = line.size();
            a = line.substr(i + 1, e - i - 1);
            i = e + 1;
        }
        else
        {
            size_t e = line.find(' ', i);
            if (e == std::string::npos)
                e = line.size();
            a = line.substr(i, e - i);
            i = e;
        }
        out.push_back(std::move(a));
    }
    return out;
}

// A path of the client's: relative ones are inside the root; "/x" and "sdmc:/x" are absolute. No ".." anywhere, and
// writes only inside the root (a typo must not remove the system's files). Empty on a refused path.
std::string resolve(const std::string& p, bool write, std::string& why)
{
    for (size_t start = 0; start <= p.size();)
    {
        size_t end = p.find_first_of("/\\", start);
        if (end == std::string::npos)
            end = p.size();
        if (p.compare(start, end - start, "..") == 0)
        {
            why = "\"..\" is not allowed in a path";
            return {};
        }
        start = end + 1;
    }
    const bool absolute = !p.empty() && (p[0] == '/' || p.find(':') != std::string::npos);
    if (!absolute)
        return p.empty() ? g_cfg.root : g_cfg.root + "/" + p;
    const std::string full = p[0] == '/' ? g_cfg.absolutePrefix + p : p;
    if (write && full.compare(0, g_cfg.root.size() + 1, g_cfg.root + "/") != 0)
    {
        why = "writes only go inside " + g_cfg.root + " (" + full + ")";
        return {};
    }
    return full;
}

bool send_reply(Conn& c, const Reply& r)
{
    if (r.file.empty())
        return send_header(c.fd, r.ok, r.text.size()) && send_all(c.fd, r.text.data(), r.text.size());
    FILE* f = fopen(r.file.c_str(), "rb");
    if (!f)
    {
        std::string e = "cannot open " + r.file;
        if (r.file.size() > 4 && r.file.compare(r.file.size() - 4, 4, ".log") == 0)
            e += " (the running session's log is open for writing: logtext)";
        return send_header(c.fd, false, e.size()) && send_all(c.fd, e.data(), e.size());
    }
    struct stat st;
    const uint64_t size = fstat(fileno(f), &st) == 0 ? uint64_t(st.st_size) : 0;
    bool good = send_header(c.fd, true, size);
    std::vector<char> buf(64 << 10);
    uint64_t left = size;
    while (good && left)
    {
        const size_t n = fread(buf.data(), 1, size_t(std::min<uint64_t>(left, buf.size())), f);
        if (!n)
            break;
        good = send_all(c.fd, buf.data(), n);
        left -= n;
    }
    fclose(f);
    if (r.removeFile)
        remove(r.file.c_str());
    // A file that shrank while it was sent: the client is out of step, so the connection closes.
    return good && !left;
}

// "put <path> <size>": size bytes follow the line; written to <path>.part, then renamed over path, so a broken upload
// never replaces a good file
Reply do_put(Conn& c, const Args& a)
{
    if (a.size() != 2)
        return err("usage: put <path> <size>");
    const uint64_t size = strtoull(a[1].c_str(), nullptr, 10);
    std::string why;
    const std::string path = resolve(a[0], true, why), part = path + ".part";
    FILE* f = path.empty() ? nullptr : fopen(part.c_str(), "wb");
    std::vector<char> buffer(f ? 128 << 10 : 0);
    if (f)
        setvbuf(f, buffer.data(), _IOFBF, buffer.size());
    // the bytes are read either way, so the connection stays in step
    uint64_t left = size;
    uint32_t crc = 0;
    bool wrote = f != nullptr;
    while (left)
    {
        if (c.in.empty() && !fill(c))
        {
            if (f)
                fclose(f), remove(part.c_str());
            return err("connection closed during the upload");
        }
        const size_t n = size_t(std::min<uint64_t>(left, c.in.size()));
        crc = crc32_update(crc, reinterpret_cast<const uint8_t*>(c.in.data()), n);
        if (f && wrote && fwrite(c.in.data(), 1, n, f) != n)
            wrote = false;
        c.in.erase(0, n);
        left -= n;
    }
    if (path.empty())
        return err(why);
    if (!f)
        return err("cannot write " + part);
    if (fclose(f) != 0)
        wrote = false;
    if (!wrote)
    {
        remove(part.c_str());
        return err("write failed: " + part + " (SD card full?)");
    }
    // (the Switch's rename replaces an existing file: os/switch/rename_switch.c)
    if (rename(part.c_str(), path.c_str()) != 0)
        return err("cannot rename " + part + " to " + path);
    char t[64];
    snprintf(t, sizeof t, "crc32 %08x size %llu", crc, (unsigned long long)size);
    say("[debug] put %s: %llu bytes, crc32 %08x", path.c_str(), (unsigned long long)size, crc);
    return ok(t);
}

Reply do_ls(const Args& a)
{
    std::string why;
    const std::string path = resolve(a.empty() ? "" : a[0], false, why);
    if (path.empty())
        return err(why);
    DIR* d = opendir(path.c_str());
    if (!d)
        return err("cannot open the folder " + path);
    std::vector<std::string> lines;
    while (dirent* e = readdir(d))
    {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        struct stat st;
        const std::string full = path + "/" + e->d_name;
        char l[400];
        const bool known = stat(full.c_str(), &st) == 0;
        if (known && S_ISDIR(st.st_mode))
            snprintf(l, sizeof l, "d %12s %s", "-", e->d_name);
        else
            snprintf(l, sizeof l, "f %12llu %s", known ? (unsigned long long)st.st_size : 0ull, e->d_name);
        lines.push_back(l);
    }
    closedir(d);
    std::sort(lines.begin(), lines.end(), [](const std::string& x, const std::string& y) {
        return x.compare(15, std::string::npos, y, 15, std::string::npos) < 0;
    });
    std::string out;
    for (auto& l : lines)
        out += l + "\n";
    return ok(out);
}

// "log [all]": the connection becomes a stream of the log's text (all: from the oldest text kept)
void do_log(Conn& c, bool all)
{
    if (!g_cfg.readLog)
    {
        const char e[] = "no log";
        send_header(c.fd, false, sizeof e - 1), send_all(c.fd, e, sizeof e - 1);
        return;
    }
    uint64_t pos = 0;
    std::string chunk;
    g_cfg.readLog(pos, chunk); // pos: the end
    if (!all)
        chunk.clear();
    if (!send_header(c.fd, true, 0))
        return;
    while (g_running.load())
    {
        if (chunk.empty())
        {
            const uint64_t skipped = g_cfg.readLog(pos, chunk);
            if (skipped)
            {
                char note[112];
                snprintf(note, sizeof note, "[debug] %llu bytes of log skipped (the stream fell behind)\n",
                         (unsigned long long)skipped);
                chunk.insert(0, note);
            }
        }
        if (!chunk.empty() && !send_all(c.fd, chunk.data(), chunk.size()))
            return;
        chunk.clear();
        // the client closed the stream? (anything it sends is ignored)
        pollfd p{ c.fd, POLLIN, 0 };
        if (poll(&p, 1, 100) > 0)
        {
            char b[256];
            if (recv(c.fd, b, sizeof b, 0) <= 0)
                return;
        }
    }
}

Reply builtin(Conn& c, const std::string& name, const Args& a, bool& handled)
{
    handled = true;
    if (name == "ping")
        return ok("pong");
    if (name == "help")
    {
        std::string out =
            "ping\nhelp\n"
            "log [all]              the log as it is written (all: the text kept first)\n"
            "logtext                the log text kept in memory (the running session's file cannot be read)\n"
            "get <path>             a file (relative to the app folder, or /absolute)\n"
            "put <path> <size>      then size bytes; written to <path>.part, then renamed\n"
            "ls [path]\nrm <path>\nmkdir <path>\n";
        std::lock_guard<std::mutex> lk(g_cmdMu);
        for (auto& [n, cmd] : g_cmds)
            out += cmd.usage + "\n";
        return ok(out);
    }
    if (name == "logtext")
    {
        if (!g_cfg.readLog)
            return err("no log");
        uint64_t pos = 0;
        std::string text;
        const uint64_t skipped = g_cfg.readLog(pos, text);
        if (skipped)
        {
            char note[112];
            snprintf(note, sizeof note, "[debug] (the first %llu bytes of the log are not kept)\n", (unsigned long long)skipped);
            text.insert(0, note);
        }
        return ok(std::move(text));
    }
    if (name == "get")
    {
        if (a.size() != 1)
            return err("usage: get <path>");
        std::string why;
        Reply r;
        r.file = resolve(a[0], false, why);
        return r.file.empty() ? err(why) : r;
    }
    if (name == "put")
        return do_put(c, a);
    if (name == "ls")
        return do_ls(a);
    if (name == "rm" || name == "mkdir")
    {
        if (a.size() != 1)
            return err("usage: " + name + " <path>");
        std::string why;
        const std::string p = resolve(a[0], true, why);
        if (p.empty())
            return err(why);
        if (p == g_cfg.root)
            return err("not the app folder itself");
        struct stat st;
        if (name == "rm")
            return remove(p.c_str()) == 0 || rmdir(p.c_str()) == 0 ? ok() : err("cannot remove " + p);
        return mkdir(p.c_str(), 0777) == 0 || (stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) ? ok()
                                                                                                 : err("cannot create " + p);
    }
    handled = false;
    return {};
}

void close_slot(Slot& slot)
{
    std::lock_guard<std::mutex> lk(g_sockMu);
    if (slot.fd >= 0)
    {
        close(slot.fd);
        slot.fd = -1;
    }
}

void connection(void* arg)
{
    Slot& slot = *static_cast<Slot*>(arg);
    Conn c;
    c.fd = slot.fd;
    std::string line;
    while (g_running.load() && read_line(c, line))
    {
        Args a = split(line);
        if (a.empty())
            continue;
        const std::string name = a[0];
        a.erase(a.begin());
        if (name == "log")
        {
            do_log(c, !a.empty() && a[0] == "all");
            break;
        }
        bool handled;
        Reply r = builtin(c, name, a, handled);
        if (!handled)
        {
            Handler h;
            {
                std::lock_guard<std::mutex> lk(g_cmdMu);
                auto it = g_cmds.find(name);
                if (it != g_cmds.end())
                    h = it->second.handler;
            }
            r = h ? h(a) : err("unknown command " + name + " (help lists them)");
        }
        const bool sent = send_reply(c, r);
        if (r.after)
            r.after();
        if (!sent)
            break;
    }
    close_slot(slot);
    slot.state = 2;
}

#ifdef __SWITCH__
void join_ended()
{
    for (Slot& slot : g_slots)
        if (slot.state.load() == 2)
        {
            threadWaitForExit(&slot.thread);
            threadClose(&slot.thread); // frees its stack
            slot.state = 0;
        }
}
#endif

// a socket listening on port (any address), or -1
int open_listen(int port)
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(g_cfg.loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);
    addr.sin_port = htons(uint16_t(port));
    if (bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || listen(fd, 4) != 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

bool start_connection(Slot& slot)
{
#ifdef __SWITCH__
    // 128 KiB of stack from the heap while the connection is open (threadClose frees it).
    if (R_FAILED(threadCreate(&slot.thread, connection, &slot, nullptr, 0x20000, g_cfg.threadPriority, -2)))
        return false;
    if (R_FAILED(threadStart(&slot.thread)))
    {
        threadClose(&slot.thread);
        return false;
    }
    return true;
#else
    try
    {
        std::thread(connection, &slot).detach();
    }
    catch (...)
    {
        return false;
    }
    return true;
#endif
}

void listener(void*)
{
    int failures = 0; // accept() failures in a row
    while (g_running.load())
    {
        sockaddr_in from{};
        socklen_t len = sizeof from;
        const int fd = accept(g_listen, reinterpret_cast<sockaddr*>(&from), &len);
#ifdef __SWITCH__
        join_ended();
#else
        for (Slot& slot : g_slots)
            if (slot.state.load() == 2)
                slot.state = 0;
#endif
        if (fd < 0)
        {
            if (!g_running.load())
                break;
            // The Switch's sleep mode closes every socket, the listening one too: accept() then fails at once from
            // the wake-up on. After a second of failures the listening socket is opened again.
            if (++failures >= 10)
            {
                if (g_listen >= 0)
                    close(g_listen);
                g_listen = open_listen(g_cfg.port);
                say(g_listen >= 0 ? "[debug] listening again on port %d (the old socket stopped accepting, errno %d)"
                                  : "[debug] cannot listen on port %d again yet (errno %d); retrying",
                    g_cfg.port, errno);
                failures = 0;
            }
            wait_ms(100);
            continue;
        }
        failures = 0;
        Slot* free = nullptr;
        for (Slot& slot : g_slots)
            if (slot.state.load() == 0)
            {
                free = &slot;
                break;
            }
        if (!free)
        {
            static const char busy[] = "too many connections open (4 at most)";
            send_header(fd, false, sizeof busy - 1) && send_all(fd, busy, sizeof busy - 1);
            close(fd);
            continue;
        }
#ifdef SO_NOSIGPIPE
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
        const timeval idle{ kIdleSeconds, 0 }, sendTimeout{ kSendSeconds, 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &idle, sizeof idle);
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, sizeof sendTimeout);
        {
            std::lock_guard<std::mutex> lk(g_sockMu);
            free->fd = fd;
        }
        free->state = 1;
        if (!start_connection(*free))
        {
            say("[debug] no thread for the connection from %s", inet_ntoa(from.sin_addr));
            close_slot(*free);
            free->state = 0;
            continue;
        }
        say("[debug] connection from %s", inet_ntoa(from.sin_addr));
    }
}

#ifdef __SWITCH__
::Thread g_listenThread;
#endif
}

bool start(const Config& config)
{
    if (g_running.load())
        return true;
    g_cfg = config;
    g_listen = open_listen(config.port);
    if (g_listen < 0)
    {
        say("[debug] server off: cannot listen on port %d (errno %d)", config.port, errno);
        return false;
    }
    g_running = true;
#ifdef __SWITCH__
    if (R_FAILED(threadCreate(&g_listenThread, listener, nullptr, nullptr, 0x8000, config.threadPriority, -2)) ||
        R_FAILED(threadStart(&g_listenThread)))
    {
        say("[debug] server off: no thread for the listener");
        g_running = false;
        close(g_listen);
        g_listen = -1;
        return false;
    }
#else
    std::thread(listener, nullptr).detach();
#endif
    return true;
}

bool running() { return g_running.load(std::memory_order_relaxed); }

void add_command(const char* name, const char* usage, Handler handler)
{
    std::lock_guard<std::mutex> lk(g_cmdMu);
    g_cmds[name] = { usage, std::move(handler) };
}

void stop()
{
    if (!g_running.exchange(false))
        return;
    {
        std::lock_guard<std::mutex> lk(g_sockMu);
        for (Slot& slot : g_slots)
            if (slot.fd >= 0)
            {
                shutdown(slot.fd, SHUT_RDWR);
                close(slot.fd);
                slot.fd = -1;
            }
    }
    if (g_listen >= 0)
    {
        shutdown(g_listen, SHUT_RDWR);
        close(g_listen);
        g_listen = -1;
    }
}

uint32_t crc32_update(uint32_t crc, const uint8_t* p, size_t n)
{
    static uint32_t table[256];
    static const bool init = [] {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        return true;
    }();
    (void)init;
    crc = ~crc;
    for (size_t i = 0; i < n; i++)
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}
}
