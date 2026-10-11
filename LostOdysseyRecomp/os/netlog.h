#pragma once

#include <cstddef>

// Live copy of the runtime log and stderr for a PC on the LAN. The console's
// log files cannot be read while the game holds them open, so with
// LO_NETLOG=1 (or a port) in env.txt the Switch listens on TCP and streams
// every line to whoever connects, starting with the newest megabyte
// (tools/switch/netlog.py). Elsewhere these do nothing.
namespace os::netlog
{
#if defined(__SWITCH__)
// Called once LO_NETLOG is known; drops what was buffered when it is off.
void Start(const char* setting);
void Write(const char* data, size_t size);
#else
inline void Start(const char*) {}
inline void Write(const char*, size_t) {}
#endif
}
