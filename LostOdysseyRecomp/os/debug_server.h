#pragma once

#include <cstddef>
#include <cstdint>

// Debug server (Switch): a TCP port (6543) for a PC on the local network, with
// tools/switch/lo_debug.py: build and frame info, the log as it is written,
// screenshots, controller input, the app folder's files and deploying a new
// NRO (docs/SWITCH.md, "Debug server"). Off unless env.txt has
// LO_DEBUG_SERVER=1 or settings.ini debug_server=1, read at start; never on in
// a fresh install. There is no password: local network only.
//
// The server and its commands are os/switch/debug_server_core.cpp and
// os/switch/debug_commands.cpp. These are the runtime's hooks; while the
// server is off each costs one relaxed atomic load. Elsewhere they do nothing.
namespace os::debug_server
{
// A controller state injected by "press", "hold" and "stick" (XInput masks and ranges).
struct Held
{
    uint16_t buttons = 0;
    uint8_t leftTrigger = 0, rightTrigger = 0;
    bool stick[2] = {}; // left, right: overridden now
    int16_t x[2] = {}, y[2] = {};
};

#if defined(__SWITCH__)
// os::switch_platform::Initialize, after env.txt: sets up the sockets once for
// the network log (os/netlog.h) and the server, and starts the server when it
// is turned on.
void StartNetwork(const char* dataRoot);
bool Running();
// hid::GetState: a line sent with "input", in LO_TEST_INPUT_FILE's format
// ("serial hexButtonMask leftX leftY polls [LT RT [rightX rightY]]"), once.
bool TakeTestInput(char* line, size_t size);
// hid::GetState: what "press", "hold" and "stick" hold now; false when nothing.
bool HeldInput(Held& held);
// The GPU command processor after a present: true once when "shot" waits for
// a screenshot, with the file to save it to (PPM); then ShotDone.
bool TakeShotRequest(const char*& path);
void ShotDone(uint32_t swap, bool saved);
#else
inline bool Running() { return false; }
inline bool TakeTestInput(char*, size_t) { return false; }
inline bool HeldInput(Held&) { return false; }
inline bool TakeShotRequest(const char*&) { return false; }
inline void ShotDone(uint32_t, bool) {}
#endif
}
