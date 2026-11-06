#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Live copy of the runtime log and stderr for a PC on the LAN. The console's
// log files cannot be read while the game holds them open, so with
// LO_NETLOG=1 (or a port) in env.txt the Switch listens on TCP and streams
// every line to whoever connects, starting with the newest megabyte
// (tools/switch/netlog.py). Elsewhere these do nothing.
namespace os::netlog
{
#if defined(__SWITCH__)
// Called once LO_NETLOG and the debug server setting are known: one socket
// service setup for both (os/debug_server.h). Drops what was buffered when
// neither runs. True when the sockets are ready for the debug server.
bool Start(const char* setting, bool debugServer);
void Write(const char* data, size_t size);
// The log's text from position on (position: bytes logged since the start,
// set to the end); returns how many bytes after position are no longer kept.
// For the debug server's log stream: the history holds the last 1-2 MiB.
uint64_t Read(uint64_t& position, std::string& text);
#else
inline bool Start(const char*, bool) { return false; }
inline void Write(const char*, size_t) {}
#endif
}
