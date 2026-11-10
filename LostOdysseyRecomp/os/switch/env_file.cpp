// env.txt: the console has no process environment, so <data root>/env.txt supplies the LO_* switches the PC build
// reads from its own (one KEY=VALUE per line, # comments).
//
// It is applied twice. ApplyEarly runs before every C++ static initializer: libnx calls the constructors
// (__libc_init_array) after __appInit has mounted the SD card, and priority 101 puts this one ahead of every
// default-priority initializer in the program. Switches read there (command_processor.cpp's LO_GPU_STATS and
// LO_CP_FUSED_PLAIN_RUN, plume-deko3d's LO_DEKO3D_DESCRIPTOR_CACHE, logger.h's LO_VERBOSE...) used to keep their
// defaults whatever env.txt said, because the file was only read in switch_platform::Initialize, from main.
// Initialize applies it again (os::switch_platform::ApplyEnvFile), as before, and logs each line.
//
// getenv is wrapped (-Wl,--wrap=getenv, LostOdysseyRecomp/CMakeLists.txt) to name every switch read before that
// second pass: those env.txt could not set (read before ApplyEarly) and those only the early pass let it set.
// After Initialize the wrapper is one relaxed load and a tail call.
#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" char* __real_getenv(const char* name);

namespace
{
enum Phase : int
{
    BeforeEnvFile, // nothing applied yet: env.txt cannot set what is read now
    EarlyApplied,  // ApplyEarly ran; still in static initialization or main's start
    Logged,        // Initialize applied and logged it: the wrapper only forwards
};
std::atomic<int> g_phase{BeforeEnvFile};
// The number of settings ApplyEarly applied, -1 when it could not open env.txt.
int g_earlySettings = -1;

// The names getenv was asked for in the first two phases, without repeats, for Initialize's log.
struct NameList
{
    char names[32][48];
    int count;
    int dropped;
};
NameList g_read[2];
std::atomic_flag g_readLock = ATOMIC_FLAG_INIT;

__attribute__((cold, noinline)) void Remember(int phase, const char* name)
{
    while (g_readLock.test_and_set(std::memory_order_acquire)) {}
    NameList& list = g_read[phase];
    bool seen = false;
    for (int i = 0; i < list.count && !seen; ++i)
        seen = std::strncmp(list.names[i], name, sizeof(list.names[i]) - 1) == 0;
    if (!seen)
    {
        if (list.count < int(sizeof(list.names) / sizeof(list.names[0])))
            std::snprintf(list.names[list.count++], sizeof(list.names[0]), "%s", name);
        else
            ++list.dropped;
    }
    g_readLock.clear(std::memory_order_release);
}

// Sets every KEY=VALUE line of `path`; `log`: one "env.txt: KEY=VALUE" line on stderr for each.
// Returns the number of settings, or -1 when the file can't be opened.
int Apply(const char* path, bool log)
{
    FILE* env = std::fopen(path, "r");
    if (!env)
        return -1;
    int settings = 0;
    char line[512];
    while (std::fgets(line, sizeof(line), env))
    {
        char* text = line;
        while (*text == ' ' || *text == '\t') ++text;
        size_t length = std::strlen(text);
        while (length && (text[length - 1] == '\n' || text[length - 1] == '\r' || text[length - 1] == ' '))
            text[--length] = 0;
        char* equals = std::strchr(text, '=');
        if (!*text || *text == '#' || !equals || equals == text)
            continue;
        *equals = 0;
        const char* value = equals + 1;
        // A value the early pass already set is left alone: a pointer getenv returned to a static initializer
        // (LO_SHADER_DUMP_DIR) stays valid. A different one (something between the passes set the variable)
        // is replaced, so env.txt still wins over it, as when it was read only once.
        const char* current = __real_getenv(text);
        if (!current || std::strcmp(current, value) != 0)
            setenv(text, value, 1);
        if (log)
            std::fprintf(stderr, "env.txt: %s=%s\n", text, value);
        ++settings;
    }
    std::fclose(env);
    return settings;
}

std::string Join(const NameList& list)
{
    std::string text;
    for (int i = 0; i < list.count; ++i)
        (text += ' ') += list.names[i];
    if (list.dropped)
        text += " (and " + std::to_string(list.dropped) + " more)";
    return text;
}

__attribute__((constructor(101))) void ApplyEarly()
{
    constexpr const char* root = LO_SWITCH_DATA_ROOT;
    char path[512];
    std::snprintf(path, sizeof(path), "%s%s/env.txt", std::strncmp(root, "sdmc:", 5) == 0 ? "" : "sdmc:", root);
    g_earlySettings = Apply(path, false);
    g_phase.store(EarlyApplied, std::memory_order_relaxed);
}
} // namespace

extern "C" char* __wrap_getenv(const char* name)
{
    if (const int phase = g_phase.load(std::memory_order_relaxed); phase != Logged && name) [[unlikely]]
        Remember(phase, name);
    return __real_getenv(name);
}

namespace os::switch_platform
{
// switch_runtime.cpp's Initialize, once stderr goes to stderr.log.
void ApplyEnvFile(const char* path)
{
    const int settings = Apply(path, true);
    if (g_read[BeforeEnvFile].count)
        std::fprintf(stderr, "env.txt: read before env.txt was applied, so env.txt can't set them:%s\n",
            Join(g_read[BeforeEnvFile]).c_str());
    if (settings >= 0 && g_earlySettings < 0)
        std::fprintf(stderr, "env.txt: not readable before static initialization; what was read there keeps its "
            "default:%s\n", Join(g_read[EarlyApplied]).c_str());
    else if (g_earlySettings >= 0)
        std::fprintf(stderr, "env.txt: %d settings applied before static initialization; read there:%s\n",
            g_earlySettings, Join(g_read[EarlyApplied]).c_str());
    g_phase.store(Logged, std::memory_order_relaxed);
}
} // namespace os::switch_platform
#endif
