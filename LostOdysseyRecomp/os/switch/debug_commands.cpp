// The game's debug server commands and hooks (os/debug_server.h): info, input, press/hold/release/stick, shot and
// restart, on top of the core's files and log (debug_server_core.h). Client: tools/switch/lo_debug.py; docs/SWITCH.md,
// "Debug server". The command set follows SwitchWakerHD's runtime/src/platform/debug_switch.cpp.
#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include <os/debug_server.h>
#include <os/log_file.h>
#include <os/netlog.h>
#include <os/switch_platform.h>
#include <debug/perf_stats.h>
#include <lo_build_revision.h>
#include <version.h>
#include "debug_server_core.h"
#include "ppm_png.h"
#include <switch.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <thread>

extern std::atomic<uint32_t> g_presentedSwaps; // gpu/command_processor.cpp: frames presented

#ifdef LO_SWITCH_PGO_GEN
namespace os::pgo { void RegisterCommands(); } // os/switch/pgo_switch.cpp: training builds only
#endif

namespace os::debug_server
{
namespace
{
constexpr int kDefaultPort = 6543;
constexpr int kServerPriority = 0x2B; // above the game's threads (0x2C), as the network log
std::atomic<bool> g_running{false};
int g_port = kDefaultPort;
std::string g_root;          // the app folder, "sdmc:/switch/LostOdysseyRecomp"
uint64_t g_startTick = 0;    // when the server started: the session, so a client sees a restart

void Say(const char* line) { std::fprintf(stderr, "%s\n", line); }

// ---------------------------------------------------------------- injected controller state
// XInput button masks as the game reads them (hid::GetState), the names as tools/scenario.py's and the Switch's.
const struct
{
    const char* name;
    uint16_t mask;
} kButtons[] = {
    { "UP", 0x1 }, { "DOWN", 0x2 }, { "LEFT", 0x4 }, { "RIGHT", 0x8 }, { "START", 0x10 }, { "PLUS", 0x10 }, { "+", 0x10 },
    { "BACK", 0x20 }, { "MINUS", 0x20 }, { "-", 0x20 }, { "LS", 0x40 }, { "RS", 0x80 }, { "LB", 0x100 }, { "L", 0x100 },
    { "RB", 0x200 }, { "R", 0x200 }, { "A", 0x1000 }, { "B", 0x2000 }, { "X", 0x4000 }, { "Y", 0x8000 },
};
// The triggers are axes: held at 255.
constexpr int kLeftTrigger = 16, kRightTrigger = 17;

std::mutex g_inputMutex;
std::atomic<bool> g_inputHeld{false};  // something below may be held: hid's poll takes the lock only then
uint64_t g_until[18] = {};             // per button bit, then LT and RT: 0 up, else the tick it ends (~0: released by hand)
struct Stick
{
    int16_t x = 0, y = 0;
    uint64_t until = 0;
};
Stick g_sticks[2];

std::mutex g_testInputMutex;
std::atomic<bool> g_testInputPending{false};
std::string g_testInput;

uint64_t Now() { return armGetSystemTick(); }
uint64_t After(int ms) { return Now() + armNsToTicks(uint64_t(ms) * 1000000); }

// "A", "A+B", "zl", "+" -> masks (bits 16 and 17: the triggers); false on an unknown name
bool ParseButtons(std::string s, uint32_t& bits)
{
    for (auto& ch : s)
        ch = char(std::toupper(static_cast<unsigned char>(ch)));
    const auto one = [&](const std::string& name) {
        if (name == "LT" || name == "ZL")
            return bits |= 1u << kLeftTrigger, true;
        if (name == "RT" || name == "ZR")
            return bits |= 1u << kRightTrigger, true;
        for (const auto& b : kButtons)
            if (name == b.name)
                return bits |= b.mask, true;
        return false;
    };
    if (s == "+" || s == "-")
        return one(s);
    for (size_t start = 0;;)
    {
        const size_t end = s.find('+', start);
        if (!one(s.substr(start, end == std::string::npos ? std::string::npos : end - start)))
            return false;
        if (end == std::string::npos)
            return true;
        start = end + 1;
    }
}

// the buttons of args, the last one may be a duration in ms
bool ParsePress(const debugsrv::Args& a, uint32_t& bits, int& ms)
{
    bits = 0;
    for (size_t i = 0; i < a.size(); i++)
    {
        char* end;
        const long v = std::strtol(a[i].c_str(), &end, 10);
        if (i + 1 == a.size() && i > 0 && *end == 0 && !a[i].empty())
        {
            ms = int(std::clamp(v, 1l, 60000l));
            continue;
        }
        if (!ParseButtons(a[i], bits))
            return false;
    }
    return bits != 0;
}

constexpr char kButtonNames[] = "A B X Y LB RB LT RT START BACK UP DOWN LEFT RIGHT LS RS (also L R ZL ZR PLUS MINUS)";

debugsrv::Reply Press(const debugsrv::Args& a, bool hold)
{
    uint32_t bits = 0;
    int ms = 120;
    if (!ParsePress(a, bits, ms))
        return debugsrv::err(std::string("unknown button: ") + kButtonNames);
    {
        const uint64_t until = hold ? ~uint64_t(0) : After(ms);
        std::lock_guard lock(g_inputMutex);
        for (int i = 0; i < 18; i++)
            if (bits & (1u << i))
                g_until[i] = until;
        g_inputHeld = true;
    }
    // A press replies once released and a few polls have seen it up, so presses in a row stay separate.
    if (!hold)
        std::this_thread::sleep_for(std::chrono::milliseconds(ms + 50));
    return debugsrv::ok();
}

debugsrv::Reply Release(const debugsrv::Args& a)
{
    uint32_t bits = 0;
    int unused = 0;
    if (!a.empty() && !ParsePress(a, bits, unused))
        return debugsrv::err(std::string("unknown button: ") + kButtonNames);
    std::lock_guard lock(g_inputMutex);
    for (int i = 0; i < 18; i++)
        if (a.empty() || (bits & (1u << i)))
            g_until[i] = 0;
    if (a.empty())
        g_sticks[0].until = g_sticks[1].until = 0;
    return debugsrv::ok();
}

debugsrv::Reply StickCommand(const debugsrv::Args& a)
{
    if (a.size() < 3)
        return debugsrv::err("usage: stick <L|R> <x> <y> [ms]   (-1..1, y up; 0 0 lets go)");
    const int which = std::toupper(static_cast<unsigned char>(a[0][0])) == 'R' ? 1 : 0;
    const float x = std::clamp(std::strtof(a[1].c_str(), nullptr), -1.0f, 1.0f);
    const float y = std::clamp(std::strtof(a[2].c_str(), nullptr), -1.0f, 1.0f);
    const int ms = a.size() > 3 ? std::clamp(std::atoi(a[3].c_str()), 0, 600000) : 0;
    {
        std::lock_guard lock(g_inputMutex);
        Stick& s = g_sticks[which];
        s.x = int16_t(x * 32767.0f);
        s.y = int16_t(y * 32767.0f);
        s.until = x == 0 && y == 0 && !ms ? 0 : ms > 0 ? After(ms) : ~uint64_t(0);
        g_inputHeld = true;
    }
    if (ms > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(ms + 50));
    return debugsrv::ok();
}

// "input <serial> <mask> <x> <y> <polls> [lt rt [rx ry]]": one LO_TEST_INPUT_FILE pulse (tools/scenario.py --switch)
debugsrv::Reply Input(const debugsrv::Args& a)
{
    if (a.size() != 5 && a.size() != 7 && a.size() != 9)
        return debugsrv::err("usage: input <serial> <hex mask> <x> <y> <polls> [lt rt [rx ry]] (LO_TEST_INPUT_FILE's line)");
    std::string line;
    for (const auto& field : a)
    {
        char* end;
        std::strtol(field.c_str(), &end, &field == &a[1] ? 16 : 10);
        if (field.empty() || *end)
            return debugsrv::err("not a number: " + field);
        line += (line.empty() ? "" : " ") + field;
    }
    {
        std::lock_guard lock(g_testInputMutex);
        g_testInput = line;
        g_testInputPending = true;
    }
    return debugsrv::ok();
}

// ---------------------------------------------------------------- screenshots
std::mutex g_shotCommand;                   // one "shot" at a time
std::mutex g_shotPathMutex;
std::string g_shotPath;                     // written before g_shotRequested
std::atomic<uint32_t> g_shotRequested{0}, g_shotTaken{0}, g_shotDone{0};
std::atomic<bool> g_shotSaved{false};
std::atomic<uint32_t> g_shotSwap{0};

// "shot [ppm]": the next presented frame (video::SaveScreenshot, the path LO_SCREENSHOT_REQUEST uses), as a PNG, or
// as the PPM it was saved as (tools/regression.py compares PPMs)
debugsrv::Reply Shot(const debugsrv::Args& a)
{
    if (!g_presentedSwaps.load(std::memory_order_relaxed))
        return debugsrv::err("no frame presented yet");
    const bool ppm = !a.empty() && a[0] == "ppm";
    std::lock_guard command(g_shotCommand);
    const uint32_t serial = g_shotRequested.load() + 1;
    std::string path = g_root + "/debug-shot-" + std::to_string(serial) + ".ppm";
    {
        std::lock_guard lock(g_shotPathMutex);
        // A shot that timed out may have been saved since.
        if (!g_shotPath.empty())
            std::remove(g_shotPath.c_str());
        g_shotPath = path;
    }
    g_shotRequested.store(serial, std::memory_order_release);
    for (int i = 0; i < 1000 && g_shotDone.load(std::memory_order_acquire) != serial; i++) // 20 s
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (g_shotDone.load(std::memory_order_acquire) != serial)
        return debugsrv::err("no frame within 20 s (is the game presenting frames?)");
    if (!g_shotSaved.load())
    {
        std::remove(path.c_str());
        return debugsrv::err("the screenshot failed at frame " + std::to_string(g_shotSwap.load()) + " (see the log)");
    }
    if (ppm)
    {
        debugsrv::Reply r;
        r.file = path;
        r.removeFile = true;
        return r;
    }
    // Below the game's threads while compressing (a few hundred ms of CPU), so the shot does not cause a hitch.
    os::switch_platform::SetCurrentThreadPriority(0x3B);
    std::string png, why;
    bool converted = false;
    try
    {
        converted = PpmToPng(path, png, why);
    }
    catch (const std::bad_alloc&)
    {
        why = "out of memory for the PNG";
    }
    os::switch_platform::SetCurrentThreadPriority(kServerPriority);
    std::remove(path.c_str());
    return converted ? debugsrv::ok(std::move(png)) : debugsrv::err(why);
}

// ---------------------------------------------------------------- info, restart
uint64_t ProgramId()
{
    u64 id = 0;
    svcGetInfo(&id, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0);
    return id;
}

// appletRestartProgram starts this program again. From a HOME-menu forwarder that is the forwarder, which loads the
// NRO from the SD card again. Through title takeover the program is the game that was taken over (an ID that starts
// with 01): restarting it starts that game, not the homebrew menu. LO_DEBUG_RESTART=1 says "restart works here".
bool RestartWorks()
{
    const char* setting = std::getenv("LO_DEBUG_RESTART");
    if (setting && *setting)
        return std::strcmp(setting, "0") != 0;
    return ProgramId() >> 56 != 0x01;
}

std::string AddressText()
{
    in_addr address{};
    address.s_addr = uint32_t(gethostid()); // libnx: the console's address (nifm), network order
    return inet_ntoa(address);
}

std::string PowerText()
{
    static const bool psm = R_SUCCEEDED(psmInitialize());
    u32 percent = 0;
    PsmChargerType charger = PsmChargerType_Unconnected;
    if (!psm || R_FAILED(psmGetBatteryChargePercentage(&percent)) || R_FAILED(psmGetChargerType(&charger)))
        return "unknown";
    return std::to_string(percent) + "%, " + (charger == PsmChargerType_Unconnected ? "on battery" : "charging");
}

debugsrv::Reply Info(const debugsrv::Args&)
{
    uint32_t cpuHz = 0, gpuHz = 0, memoryHz = 0;
    os::switch_platform::ReadClockRates(cpuHz, gpuHz, memoryHz);
    const uint64_t program = ProgramId();
    const double uptime = double(armTicksToNs(Now() - g_startTick)) / 1e9;
    char text[1024];
    std::snprintf(text, sizeof(text),
        "build %s (%s)\n"
        "session %016llx (up %.0f s)\n"
        "frame %u\n"
        "map %s\n"
        "mode %s\n"
        "clocks cpu %u MHz, gpu %u MHz, memory %u MHz\n"
        "memory %s\n"
        "battery %s\n"
        "screen %s\n"
        "program %016llx (%s)\n"
        "address %s:%d\n",
        lo_version::Source, LO_BUILD_REVISION, static_cast<unsigned long long>(g_startTick), uptime,
        g_presentedSwaps.load(std::memory_order_relaxed), perf_stats::MapName().c_str(),
        os::switch_platform::IsDocked() ? "docked" : "handheld", cpuHz / 1000000, gpuHz / 1000000, memoryHz / 1000000,
        os::switch_platform::MemorySummary().c_str(), PowerText().c_str(),
        os::switch_platform::ScreenSummary().c_str(), static_cast<unsigned long long>(program),
        RestartWorks() ? "restart works: HOME-menu forwarder" : "a game's ID: title takeover, restart is off",
        AddressText().c_str(), g_port);
    std::string out = text;
    const std::string perf = perf_stats::LastPerfLine();
    if (!perf.empty())
        out += "last " + perf + "\n";
    return debugsrv::ok(std::move(out));
}

debugsrv::Reply Restart(const debugsrv::Args& a)
{
    const bool force = !a.empty() && a[0] == "force";
    if (!force && !RestartWorks())
    {
        char program[24];
        std::snprintf(program, sizeof(program), "%016llx", static_cast<unsigned long long>(ProgramId()));
        return debugsrv::err(std::string("restart is off: this looks like title takeover (program ") + program +
            "), where it would start the game that was taken over. From a "
            "HOME-menu forwarder: LO_DEBUG_RESTART=1 in env.txt, or \"restart force\". Otherwise start the game again "
            "by hand.");
    }
    debugsrv::Reply r = debugsrv::ok("restarting: the program starts again and loads the NRO from the SD card");
    r.after = [] {
        std::fprintf(stderr, "[debug] restart: appletRestartProgram\n");
        os::logger::DrainForExit();
        static const char argument[] = "lo debug restart";
        const Result rc = appletRestartProgram(argument, sizeof(argument));
        // (on success the system ends this process, asking the game to quit first)
        std::fprintf(stderr, "[debug] restart failed: appletRestartProgram rc %#x\n", rc);
    };
    return r;
}

// On by LO_DEBUG_SERVER (env.txt, "1" or a port; "0" turns it off whatever the settings say), else by
// config/settings.ini's debug_server=1 (System > Debug > Debug server). source: which one decided.
bool Enabled(const std::string& root, int& port, const char*& source)
{
    port = kDefaultPort;
    if (const char* setting = std::getenv("LO_DEBUG_SERVER"); setting && *setting)
    {
        source = "env.txt";
        if (const int number = std::atoi(setting); number > 1 && number < 65536)
            port = number;
        return std::strcmp(setting, "0") != 0;
    }
    source = "settings";
    bool on = false;
    // os::user_paths::SettingsPath() on the Switch (that header needs the runtime's precompiled header).
    if (FILE* file = std::fopen((root + "/config/settings.ini").c_str(), "r"))
    {
        char line[256];
        while (std::fgets(line, sizeof(line), file))
            if (std::strncmp(line, "debug_server=", 13) == 0)
                on = std::atoi(line + 13) == 1;
        std::fclose(file);
    }
    return on;
}
}

void StartNetwork(const char* dataRoot)
{
    g_root = dataRoot;
    while (g_root.size() > 1 && g_root.back() == '/')
        g_root.pop_back();
    int port = kDefaultPort;
    const char* source = "";
    const bool wanted = Enabled(g_root, port, source);
    const bool sockets = os::netlog::Start(std::getenv("LO_NETLOG"), wanted);
    if (!wanted || !sockets)
        return;
    g_port = port;
    g_startTick = Now();
    debugsrv::add_command("info", "info                   build, session, frame, map, mode, clocks, memory, address", Info);
    debugsrv::add_command("screen", "screen [watch|busy on|off]  why the screen stays on; holds a test reason (as a movie, as a load)",
        [](const debugsrv::Args& a) {
            using namespace os::switch_platform;
            if (a.size() == 2 && (a[0] == "watch" || a[0] == "busy") && (a[1] == "on" || a[1] == "off"))
            {
                const uint32_t bit = a[0] == "watch" ? AwakeTestWatching : AwakeTestBusy;
                SetScreenAwake(bit, a[1] == "on" ? bit : 0);
            }
            else if (!a.empty())
                return debugsrv::err("usage: screen [watch|busy on|off]");
            return debugsrv::ok(ScreenSummary() + "\n");
        });
    debugsrv::add_command("input", "input <serial> <mask> <x> <y> <polls> [lt rt [rx ry]]  one LO_TEST_INPUT_FILE pulse", Input);
    debugsrv::add_command("press", "press <buttons> [ms]   A B X Y LB RB LT RT START BACK UP DOWN LEFT RIGHT LS RS, A+B "
        "together; 120 ms; replies once released",
        [](const debugsrv::Args& a) { return Press(a, false); });
    debugsrv::add_command("hold", "hold <buttons>         held until release",
        [](const debugsrv::Args& a) { return Press(a, true); });
    debugsrv::add_command("release", "release [buttons]      (no buttons: everything, sticks too)", Release);
    debugsrv::add_command("stick", "stick <L|R> <x> <y> [ms]  -1..1, y up; without ms until \"stick L 0 0\"", StickCommand);
    debugsrv::add_command("shot", "shot [ppm]             the next frame as a PNG (ppm: as saved)", Shot);
    debugsrv::add_command("restart", "restart [force]        start the program again (a HOME-menu forwarder)", Restart);
#ifdef LO_SWITCH_PGO_GEN
    os::pgo::RegisterCommands();
#endif
    debugsrv::Config config;
    config.port = port;
    config.root = g_root;
    config.absolutePrefix = "sdmc:";
    config.log = Say;
    config.readLog = [](uint64_t& position, std::string& text) { return os::netlog::Read(position, text); };
    config.threadPriority = kServerPriority;
    if (!debugsrv::start(config))
        return;
    g_running = true;
    std::fprintf(stderr, "[debug] server listening on %s:%d, turned on by %s (tools/switch/lo_debug.py; local network "
        "only, no password)\n", AddressText().c_str(), port, source);
}

bool Running() { return g_running.load(std::memory_order_relaxed); }

bool ClientRecently() { return Running() && debugsrv::client_within(300); }

bool TakeTestInput(char* line, size_t size)
{
    if (!g_testInputPending.load(std::memory_order_relaxed))
        return false;
    std::lock_guard lock(g_testInputMutex);
    if (!g_testInputPending.exchange(false))
        return false;
    std::snprintf(line, size, "%s", g_testInput.c_str());
    return true;
}

bool HeldInput(Held& held)
{
    if (!g_inputHeld.load(std::memory_order_relaxed))
        return false;
    held = {};
    const uint64_t now = Now();
    bool any = false;
    std::lock_guard lock(g_inputMutex);
    for (int i = 0; i < 18; i++)
    {
        if (g_until[i] && now >= g_until[i])
            g_until[i] = 0;
        if (!g_until[i])
            continue;
        any = true;
        if (i == kLeftTrigger)
            held.leftTrigger = 255;
        else if (i == kRightTrigger)
            held.rightTrigger = 255;
        else
            held.buttons |= uint16_t(1u << i);
    }
    for (int s = 0; s < 2; s++)
    {
        Stick& stick = g_sticks[s];
        if (stick.until && now >= stick.until)
            stick.until = 0;
        if (stick.until)
        {
            any = true;
            held.stick[s] = true;
            held.x[s] = stick.x;
            held.y[s] = stick.y;
        }
    }
    if (!any)
        g_inputHeld = false;
    return any;
}

bool TakeShotRequest(const char*& path)
{
    if (!g_running.load(std::memory_order_relaxed))
        return false;
    const uint32_t requested = g_shotRequested.load(std::memory_order_acquire);
    if (requested == g_shotTaken.load(std::memory_order_relaxed))
        return false;
    g_shotTaken.store(requested, std::memory_order_relaxed);
    // Copied: a "shot" that timed out lets the next one replace g_shotPath. Only the command processor calls this.
    static char copy[512];
    {
        std::lock_guard lock(g_shotPathMutex);
        std::snprintf(copy, sizeof(copy), "%s", g_shotPath.c_str());
    }
    path = copy;
    return true;
}

void ShotDone(uint32_t swap, bool saved)
{
    g_shotSwap = swap;
    g_shotSaved = saved;
    g_shotDone.store(g_shotTaken.load(std::memory_order_relaxed), std::memory_order_release);
}
}
#endif
