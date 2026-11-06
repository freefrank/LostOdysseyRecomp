// The debug server on a computer, for testing the protocol, tools/switch/lo_debug.py and tools/scenario.py --switch
// without a console. The server core and the PNG writer are the Switch build's own files
// (LostOdysseyRecomp/os/switch/debug_server_core.cpp, ppm_png.cpp); the game's commands (debug_commands.cpp, which
// needs libnx and the game) are stand-ins here with the same names, arguments and replies, and the log is a made-up
// game's: a line per frame with the timing lines tools/scenario.py reads, draws rising once the scene "loads". With
// LO_BENCH in the folder's env.txt (read at start, as the game does) it also logs a made-up "bench result", "bench env"
// and "bench done" for tools/switch/bench.py --debug-server, and quits after them with LO_BENCH_EXIT=1.
//
// Build and run from the repository root (macOS or Linux):
//   M=tools/XenosRecomp/thirdparty/smol-v/testing/external/miniz; S=LostOdysseyRecomp/os/switch
//   cc -O1 -c $M/miniz.c -o out/miniz.o
//   c++ -std=c++20 -O1 -I$S -I$M tools/switch/debug_server_host_test.cpp $S/debug_server_core.cpp $S/ppm_png.cpp \
//       out/miniz.o -o out/debug_server_host_test
//   out/debug_server_host_test out/fake-sd 6543 &      # an empty folder plays the app folder; 127.0.0.1 only
//   python3 tools/switch/lo_debug.py --host 127.0.0.1 info
// Options after the port: frames per second of the fake game (default 120), the frame its scene loads at (300), and
// "takeover" to refuse restart without force as a title-takeover console does.
#include "debug_server_core.h"
#include "ppm_png.h"

#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace
{
std::string g_root;
int g_port = 6543;
char** g_argv = nullptr;
uint64_t g_session = 0;
std::atomic<uint32_t> g_frame{0};
std::atomic<uint32_t> g_inputs{0};
bool g_takeover = false;
std::map<std::string, std::string> g_env; // env.txt's KEY=VALUE lines

// The network log's history (os/switch/netlog.cpp): the last 1-2 MiB, Read as it does.
std::mutex g_logMutex;
std::string g_history;
uint64_t g_end = 0;

void Log(const std::string& line)
{
    std::lock_guard lock(g_logMutex);
    g_history += line + "\n";
    g_end += line.size() + 1;
    if (g_history.size() > 2u << 20)
        g_history.erase(0, g_history.size() - (1u << 20));
}

uint64_t Read(uint64_t& position, std::string& text)
{
    std::lock_guard lock(g_logMutex);
    const uint64_t first = g_end - g_history.size();
    const uint64_t dropped = position < first ? first - position : 0;
    const uint64_t from = std::max(position, first);
    text.append(g_history, size_t(std::min(from, g_end) - first), std::string::npos);
    position = g_end;
    return dropped;
}

double Seconds()
{
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

void ReadEnv()
{
    std::ifstream f(g_root + "/env.txt");
    for (std::string line; std::getline(f, line);)
    {
        const size_t eq = line.find('=');
        if (line.empty() || line[0] == '#' || eq == std::string::npos)
            continue;
        std::string value = line.substr(eq + 1);
        while (!value.empty() && (value.back() == '\r' || value.back() == ' '))
            value.pop_back();
        g_env[line.substr(0, eq)] = value;
    }
}

uint32_t EnvNumber(const char* key, uint32_t fallback)
{
    const auto it = g_env.find(key);
    return it == g_env.end() ? fallback : uint32_t(std::strtoul(it->second.c_str(), nullptr, 10));
}

// LO_BENCH's lines (debug/perf_stats.cpp FinishBench), with made-up figures.
void BenchResult(const std::string& name, uint32_t frames, uint32_t startSwap, uint32_t endSwap, double seconds)
{
    char line[1024];
    std::snprintf(line, sizeof(line), "[%9.3f t0001] [note]  bench result: name=%s status=ok frames=%u seconds=%.2f "
        "start_swap=%u end_swap=%u fps=%.2f ms_mean=33.40 ms_p50=33.30 ms_p90=33.40 ms_p95=34.10 ms_p99=40.00 "
        "ms_max=41.20 ms_stdev=1.10 low1_fps=25.00 late_pct=2.0 target_fps=30 hitches=0 loading=0 pace_ms=10.%02u "
        "present_ms=0.40 cp_idle_ms=5.00 gpu_ms=%.2f gpu_pct=40.0 draws=%u gpu_draws=880 pipeline_binds=120.0 "
        "scene=host-test threads=main:%.0f,render:55 mode=handheld cpu_mhz=1020 gpu_mhz=460 mem_mhz=1331 map=-",
        Seconds(), name.c_str(), frames, seconds, startSwap, endSwap, frames / seconds, unsigned(g_session % 100),
        12.0 + double(g_session % 7) / 10.0, 900 + endSwap % 17, 80.0 + double(g_session % 9));
    Log(line);
    std::string env;
    for (const auto& [key, value] : g_env)
        if (key.rfind("LO_", 0) == 0)
            env += " " + key + "=" + value;
    std::snprintf(line, sizeof(line), "[%9.3f t0001] [note]  bench env:%s", Seconds(), env.c_str());
    Log(line);
    std::snprintf(line, sizeof(line), "[%9.3f t0001] [note]  bench done: name=%s status=ok", Seconds(), name.c_str());
    Log(line);
}

void Game(int fps, uint32_t sceneFrame)
{
    const std::string bench = g_env.count("LO_BENCH") ? g_env["LO_BENCH"] : "";
    const uint32_t startDraws = EnvNumber("LO_BENCH_START_DRAWS", 1), settle = EnvNumber("LO_BENCH_SETTLE", 150),
                   measured = EnvNumber("LO_BENCH_FRAMES", 600);
    uint32_t startSwap = 0;
    double startSeconds = 0;
    for (uint32_t frame = 1;; frame++)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(1000000 / fps));
        g_frame = frame;
        const uint32_t draws = frame < sceneFrame ? 40 : 900 + frame % 17;
        if (!bench.empty())
        {
            if (!startSwap && draws >= startDraws)
                startSwap = frame + settle, Log("[note]  bench: " + bench + " reached its scene at swap " + std::to_string(frame));
            if (startSwap && frame == startSwap)
                startSeconds = Seconds();
            if (startSwap && frame == startSwap + measured)
            {
                BenchResult(bench, measured, startSwap, frame, Seconds() - startSeconds);
                if (EnvNumber("LO_BENCH_EXIT", 0))
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // the log reaches the stream
                    debugsrv::stop();
                    std::_Exit(0);
                }
            }
        }
        char line[256];
        std::snprintf(line, sizeof(line), "[%9.3f t0001] [info]  render timing frame=%u draws=%u gpu_queue_batches_elapsed_ms=%.2f",
            Seconds(), frame, draws, 12.0 + frame % 5);
        Log(line);
        std::snprintf(line, sizeof(line), "[%9.3f t0001] [info]  present timing completed=%u target=30 frame_ms=%.1f", Seconds(),
            frame, frame % 50 ? 33.3 : 40.0);
        Log(line);
    }
}

// A 320x180 gradient with the frame number in its brightness, as video::SaveScreenshot would save it.
bool SavePpm(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f)
        return false;
    const unsigned w = 320, h = 180, frame = g_frame.load();
    std::fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (unsigned y = 0; y < h; y++)
        for (unsigned x = 0; x < w; x++)
        {
            const unsigned char rgb[3] = { (unsigned char)(x * 255 / w), (unsigned char)(y * 255 / h), (unsigned char)(frame & 0xff) };
            std::fwrite(rgb, 1, 3, f);
        }
    return std::fclose(f) == 0;
}

debugsrv::Reply Info(const debugsrv::Args&)
{
    char text[512];
    std::snprintf(text, sizeof(text),
        "build host-test (debug_server_host_test)\nsession %016llx (up %.0f s)\nframe %u\nmap -\nmode handheld\n"
        "clocks cpu 1020 MHz, gpu 460 MHz, memory 1331 MHz\nmemory (host test)\nbattery unknown\n"
        "program 0500000000000000 (restart works: HOME-menu forwarder)\naddress 127.0.0.1:%d\n",
        (unsigned long long)g_session, Seconds(), g_frame.load(), g_port);
    return debugsrv::ok(text);
}

debugsrv::Reply Shot(const debugsrv::Args& a)
{
    const std::string path = g_root + "/debug-shot-test.ppm";
    if (!SavePpm(path))
        return debugsrv::err("cannot save " + path);
    Log("[debug] screenshot at frame " + std::to_string(g_frame.load()));
    if (!a.empty() && a[0] == "ppm")
    {
        debugsrv::Reply r;
        r.file = path;
        r.removeFile = true;
        return r;
    }
    std::string png, why;
    const bool converted = os::debug_server::PpmToPng(path, png, why);
    std::remove(path.c_str());
    return converted ? debugsrv::ok(std::move(png)) : debugsrv::err(why);
}

debugsrv::Reply Record(const char* name, const debugsrv::Args& a)
{
    std::string line = std::string("[debug] ") + name;
    for (const auto& arg : a)
        line += " " + arg;
    Log(line);
    g_inputs++;
    return debugsrv::ok();
}
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <folder> [port] [fps] [scene frame]\n", argv[0]);
        return 2;
    }
    g_argv = argv;
    g_root = argv[1];
    g_port = argc > 2 ? std::atoi(argv[2]) : 6543;
    const int fps = argc > 3 ? std::atoi(argv[3]) : 120;
    const uint32_t sceneFrame = argc > 4 ? uint32_t(std::atoi(argv[4])) : 300;
    g_takeover = argc > 5 && std::strcmp(argv[5], "takeover") == 0;
    ReadEnv();
    g_session = uint64_t(std::chrono::steady_clock::now().time_since_epoch().count());
    debugsrv::add_command("info", "info                   (host test) build, session, frame", Info);
    debugsrv::add_command("shot", "shot [ppm]             (host test) a gradient", Shot);
    for (const char* name : { "input", "press", "hold", "release", "stick" })
        debugsrv::add_command(name, (std::string(name) + "                  (host test) logged").c_str(),
            [name](const debugsrv::Args& a) { return Record(name, a); });
    debugsrv::add_command("restart", "restart [force]        (host test) the program starts again", [](const debugsrv::Args& a) {
        if (g_takeover && (a.empty() || a[0] != "force"))
            return debugsrv::err("restart is off: this looks like title takeover (program 0100000000001000), where it "
                "would start the game that was taken over. From a HOME-menu forwarder: LO_DEBUG_RESTART=1 in env.txt, or "
                "\"restart force\". Otherwise start the game again by hand.");
        debugsrv::Reply r = debugsrv::ok("restarting: the program starts again");
        r.after = [] {
            debugsrv::stop();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            execv(g_argv[0], g_argv);
            std::perror("execv");
            std::_Exit(1);
        };
        return r;
    });
    debugsrv::Config config;
    config.port = g_port;
    config.root = g_root;
    config.absolutePrefix = "";
    config.loopbackOnly = true; // this computer only
    config.log = [](const char* line) { std::fprintf(stderr, "%s\n", line); Log(line); };
    config.readLog = Read;
    Log("[debug] host test session starting");
    Log("[   0.000 t0001] [note]  build: source=host-test revision=host-test pointer_bits=64");
    for (int attempt = 0; !debugsrv::start(config); attempt++)
    {
        if (attempt == 50)
            return 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // the previous image's socket may linger
    }
    std::fprintf(stderr, "[debug] host test server on port %d, folder %s\n", g_port, g_root.c_str());
    Game(fps, sceneFrame);
}
