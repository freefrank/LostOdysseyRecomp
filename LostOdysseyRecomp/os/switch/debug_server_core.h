// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
// distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
//
// Adapted from SwitchWakerHD's runtime/src/platform/debug_server.h (github.com/centollOS/SwitchWakerHD, commit
// cfd44c0a26182361820ac59859c1d94243ad8ea0, MPL-2.0) for LostOdysseyRecomp: libnx threads joined by the listener,
// the log text read from the network log's history instead of a copy of its own, writes kept inside the app folder,
// and the controller commands moved to the game's side (debug_commands.cpp, XInput buttons).
//
// Debug server core: a TCP port for a PC on the local network. It knows nothing of the game: the files of the app
// folder (get, put, ls, rm, mkdir), the log as it is written (log, logtext) and the commands the game adds. There is
// no password; it only runs when the player turned it on (os/debug_server.h).
//
// Protocol: one command per line ("name arg arg ...", "double quotes" around an argument with spaces). Every reply is
// "ok <n>\n" or "err <n>\n" followed by n bytes (text, or a file's contents). "put <path> <size>" is followed by the
// file's size bytes. "log" replies "ok 0" and the connection becomes a stream of the log's text until the client
// closes it. At most 4 connections are open at once, each on a thread of its own.
//
// Builds on the Switch (libnx sockets, initialised by the caller) and on POSIX hosts, so the protocol and the client
// can be tested on a computer (tools/switch/debug_server_host_test.cpp).
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace debugsrv
{
using Args = std::vector<std::string>; // the command's arguments, its name not included

struct Reply
{
    bool ok = true;
    std::string text;            // the reply's bytes, unless file is set
    std::string file;            // send this file's contents instead
    bool removeFile = false;     // remove file once sent (a screenshot)
    std::function<void()> after; // run once the reply is sent (restart)
};
inline Reply ok(std::string text = {}) { return { true, std::move(text), {}, false, {} }; }
inline Reply err(std::string text) { return { false, std::move(text), {}, false, {} }; }

using Handler = std::function<Reply(const Args&)>;

struct Config
{
    int port = 6543;
    bool loopbackOnly = false;                    // listen on 127.0.0.1 alone (the host test), not every address
    // get / put / ls / rm / mkdir: relative paths are inside it, and put / rm / mkdir only write inside it.
    // get and ls also read absolute paths ("/atmosphere/...", "sdmc:/...") for crash reports.
    std::string root;
    std::string absolutePrefix;                   // put before an absolute "/..." path ("sdmc:" on the Switch)
    std::function<void(const char*)> log;         // a line for the game's log (no newline)
    // The log's text from position on (position: bytes logged since the start); sets position to the end and
    // returns how many bytes before the text are no longer kept (0 when none were dropped).
    std::function<uint64_t(uint64_t& position, std::string& text)> readLog;
    int threadPriority = 0x2B;                    // Switch: Horizon priority of the server's threads
};

// Starts the listening thread; false when the port cannot be opened. The Switch: sockets must be initialised.
bool start(const Config& config);
bool running();
// Commands the game adds; any thread, before or after start. Handlers run on the connection's thread.
void add_command(const char* name, const char* usage, Handler handler);
// Closes the sockets (before the program restarts).
void stop();

// CRC-32 (zlib's), for uploads: the client compares it with its own.
uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t size);
}
