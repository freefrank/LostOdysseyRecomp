// Native cutscene video. The game's WMV player (the Xbox Windows Media SDK,
// run as recompiled guest code) decodes 720p WMV3 at about real time on the
// Switch, and its GetNextFrame blocks the render thread until a frame is
// ready, so cutscenes stutter and the whole game stalls when it falls behind.
// With this on, the video is also decoded on a host thread with FFmpeg's VC-1
// decoder and copied into the player's Y/U/V textures, and the guest player
// is asked never to wait: GetNextFrame's "no wait" flag (descriptor +4 bit 0,
// read back through the player's vtable +252) makes it return "no new frame"
// instead of waiting for a sample or sleeping until a frame is due. The guest
// player is otherwise untouched: it still plays the audio, keeps the movie
// state and reports the end of the movie.
//
// Guest functions involved:
//   sub_82815060(M)        create the player for movie M (M+20 player, M+44 info)
//   sub_823F0FA8(pl, d)    next frame into descriptor d (render thread)
//   sub_82CE9248 / sub_82CE9FD8(pl)  pause / resume
//   sub_82CF8140(pl)       the player's video pump thread (player vtable +144,
//                          thread player+268): runs the guest WMV decoder
// On by default on the Switch, where the guest decoder freezes cutscenes
// (LO_NATIVE_MOVIES=0 turns it off); elsewhere LO_NATIVE_MOVIES=1 opts in.
#include <stdafx.h>
#include "movie_player.h"

#if LO_HAS_WMV3_DECODER

#include <os/logger.h>
#include <os/thread_name.h>
#include <kernel/io/file_system.h>
#include <host_ui/host_ui.h>
#include <modding/fpi_index.h>
#include "wmv_stream.h"
#if LO_PLATFORM_SWITCH
#include <os/switch_platform.h>
#endif
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>

extern "C" PPC_FUNC(__imp__sub_82815060);
extern "C" PPC_FUNC(__imp__sub_823F0FA8);
extern "C" PPC_FUNC(__imp__sub_82CE9248);
extern "C" PPC_FUNC(__imp__sub_82CE9FD8);
extern "C" PPC_FUNC(__imp__sub_82CF8140);

namespace
{
namespace fs = std::filesystem;

// GetNextFrame results the movie update (sub_823F0038) checks for.
constexpr uint32_t kEndOfMovie = 0x16660026;
constexpr uint32_t kNoNewFrame = 0x166600E8;

bool Enabled()
{
    static const bool enabled = [] {
        const char* value = std::getenv("LO_NATIVE_MOVIES");
        if (!value || !value[0])
            return bool(LO_PLATFORM_SWITCH);
        return std::strcmp(value, "1") == 0;
    }();
    return enabled;
}

std::string Lower(std::string text)
{
    for (auto& c : text) c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// One movie at a time: the game plays a single cutscene and starts each one
// through sub_82815060, which replaces the previous one here.
struct Movie
{
    std::unique_ptr<movie::WmvStream> stream;
    std::string name;
    uint32_t player = 0;      // guest player object (M+20)
    bool started = false;     // first frame requested
    uint64_t startNs = 0;     // game time of the first request
    uint64_t pausedNs = 0, pauseStartNs = 0;
    bool paused = false;
    uint32_t shown = 0, late = 0;
    uint32_t lastPts = 0;
    bool loggedEnd = false;
    bool failed = false;
#if LO_PLATFORM_SWITCH
    uint32_t guestVideoThread = 0;
    int guestVideoPriority = -1;
#endif
};

std::mutex g_mutex;
std::unique_ptr<Movie> g_movie;

// Archive and offset of a movie file on the current disc, from its LO.fpi.
struct Location
{
    std::string archive;
    uint64_t offset = 0;
    uint64_t size = 0;
};

bool Locate(const std::string& name, Location& location, std::string& error)
{
    const fs::path index = FileSystem::ResolvePath("\\Device\\Cdrom0\\LO.fpi");
    if (index.empty())
    {
        error = "no disc";
        return false;
    }
    // Parsed once per disc.
    static std::mutex cacheMutex;
    std::lock_guard cacheLock(cacheMutex);
    static std::map<std::string, modding::fpi::Index> cache;
    const std::string key = FileSystem::PathUtf8(index);
    auto it = cache.find(key);
    if (it == cache.end())
    {
        std::ifstream input(index, std::ios::binary);
        std::vector<uint8_t> bytes(64);
        if (!input || !input.read(reinterpret_cast<char*>(bytes.data()), 64))
        {
            error = "cannot read " + key;
            return false;
        }
        try
        {
            const auto used = modding::fpi::IndexSize(bytes);
            bytes.resize(size_t(used));
            if (!input.read(reinterpret_cast<char*>(bytes.data()) + 64, std::streamsize(used - 64)))
            {
                error = "short read of " + key;
                return false;
            }
            it = cache.emplace(key, modding::fpi::Read(bytes)).first;
        }
        catch (const std::exception& e)
        {
            error = std::string("bad index: ") + e.what();
            return false;
        }
    }
    const auto& parsed = it->second;
    // The game names movies by file name ("mv_00v.wmv"); the index holds
    // "bin/xenon/mov/mv_00v.wmv".
    const std::string suffix = "/" + name;
    const modding::fpi::File* found = nullptr;
    for (const auto& file : parsed.files)
    {
        if (file.path == name || (file.path.size() > suffix.size() &&
                                  file.path.compare(file.path.size() - suffix.size(), suffix.size(), suffix) == 0))
        {
            // Prefer the movie folder over test data with the same name.
            if (!found || file.path.find("/mov/") != std::string::npos) found = &file;
        }
    }
    if (!found)
    {
        error = name + " is not in the disc index";
        return false;
    }
    if (found->archive >= parsed.archives.size())
    {
        error = "bad archive number";
        return false;
    }
    // Archive names in the index are lower case; match the disc files case-insensitively.
    const std::string archiveName = parsed.archives[found->archive];
    fs::path archive;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(index.parent_path(), ec))
    {
        if (Lower(FileSystem::PathUtf8(entry.path().filename())) == archiveName)
        {
            archive = entry.path();
            break;
        }
    }
    if (archive.empty())
    {
        error = archiveName + " missing";
        return false;
    }
    location.archive = FileSystem::PathUtf8(archive);
    location.offset = found->offset;
    location.size = found->size;
    return true;
}

// The movie's name: an FString (UTF-16 big endian) at info+48.
std::string MovieName(uint8_t* base, uint32_t info)
{
    const uint32_t data = PPC_LOAD_U32(info + 48);
    const int32_t count = int32_t(PPC_LOAD_U32(info + 52));
    std::string name;
    if (!data || count <= 0 || count > 260) return name;
    for (int32_t i = 0; i < count; ++i)
    {
        const uint16_t c = PPC_LOAD_U16(data + uint32_t(i) * 2);
        if (!c) break;
        if (c >= 0x80) return {};
        name += char(c == '\\' ? '/' : std::tolower(c));
    }
    if (const auto slash = name.rfind('/'); slash != std::string::npos) name.erase(0, slash + 1);
    return name;
}

void DecoderThreadSetup(const char* name)
{
    os::SetCurrentThreadName(name);
#if LO_PLATFORM_SWITCH
    // Free to run on any application core (core 2 preferred); new threads
    // otherwise stay on the creating core.
    os::switch_platform::SetThreadIdealCore(0, 2);
#endif
}

uint64_t MovieTimeNs(Movie& movie, uint64_t now)
{
    uint64_t paused = movie.pausedNs;
    if (movie.paused) paused += now - movie.pauseStartNs;
    const uint64_t elapsed = now - movie.startNs;
    return elapsed > paused ? elapsed - paused : 0;
}

void LogEnd(Movie& movie, const char* why)
{
    if (movie.loggedEnd || !movie.stream) return;
    movie.loggedEnd = true;
    const uint32_t decoded = movie.stream->DecodedFrames();
    const double seconds = movie.stream->DecodeSeconds();
    LOG_NOTICE("native movie: {} {}: {} frames decoded ({:.1f} fps decode speed), {} shown, {} late{}{}", movie.name, why,
               decoded, seconds > 0 ? decoded / seconds : 0.0, movie.shown, movie.late,
               movie.stream->LastError().empty() ? "" : ", error: ", movie.stream->LastError());
}
}

// Movie start: open the video on the host as well.
PPC_FUNC(sub_82815060)
{
    const uint32_t m = ctx.r3.u32;
    std::unique_ptr<Movie> previous;
    {
        std::lock_guard lock(g_mutex);
        previous = std::move(g_movie);
    }
    if (previous)
    {
        LogEnd(*previous, "replaced");
        previous.reset(); // stops its decoder thread
    }
    if (!Enabled())
    {
        __imp__sub_82815060(ctx, base);
        return;
    }

    auto movie = std::make_unique<Movie>();
    const uint32_t info = PPC_LOAD_U32(m + 44);
    const int32_t videoTrack = info ? int32_t(PPC_LOAD_U32(info + 64)) : -1;
    movie->name = info ? MovieName(base, info) : std::string();
    std::string error;
    Location location;
    if (movie->name.empty())
        error = "no movie name";
    else if (videoTrack == -1)
        error = "movie has no video track";
    else if (Locate(movie->name, location, error))
    {
        movie->stream = std::make_unique<movie::WmvStream>();
        if (!movie->stream->Open(location.archive, location.offset, location.size, videoTrack, error))
            movie->stream.reset();
    }
    if (!movie->stream)
    {
        LOG_WARNING("native movie: {} uses the guest decoder ({})", movie->name.empty() ? "?" : movie->name, error);
        __imp__sub_82815060(ctx, base);
        return;
    }

    __imp__sub_82815060(ctx, base);

    const uint32_t player = PPC_LOAD_U32(m + 20);
    const uint32_t width = PPC_LOAD_U32(m + 120), height = PPC_LOAD_U32(m + 124);
    if (!player || width != movie->stream->Width() || height != movie->stream->Height())
    {
        LOG_WARNING("native movie: {} uses the guest decoder (player {:#x}, textures {}x{})", movie->name, player, width,
                    height);
        return;
    }
    movie->player = player;
    movie->stream->Start(DecoderThreadSetup);
    LOG_NOTICE("native movie: {} {}x{} video track {} at {}+{:#x}, player {:#x}", movie->name, movie->stream->Width(),
               movie->stream->Height(), videoTrack, location.archive, location.offset, player);
    std::lock_guard lock(g_mutex);
    g_movie = std::move(movie);
}

// Next frame (render thread): let the guest player run without waiting, then
// copy the host frame due now into the locked Y/U/V textures.
PPC_FUNC(sub_823F0FA8)
{
    const uint32_t player = ctx.r3.u32, desc = ctx.r4.u32;
    {
        std::lock_guard lock(g_mutex);
        if (g_movie && g_movie->player == player && !g_movie->failed && !g_movie->stream->LastError().empty())
        {
            g_movie->failed = true;
#if LO_PLATFORM_SWITCH
            if (g_movie->guestVideoThread && g_movie->guestVideoPriority >= 0)
                os::switch_platform::SetThreadPriority(g_movie->guestVideoThread, g_movie->guestVideoPriority);
#endif
            LogEnd(*g_movie, "failed");
        }
        if (!g_movie || !player || g_movie->player != player || g_movie->failed)
        {
            // Not ours: stock behaviour.
            ctx.r3.u64 = player;
            ctx.r4.u64 = desc;
            goto guest;
        }
    }
    {
        const uint32_t flags = PPC_LOAD_U32(desc + 4);
        PPC_STORE_U32(desc + 4, flags | 1); // never wait for this request
        __imp__sub_823F0FA8(ctx, base);
        PPC_STORE_U32(desc + 4, flags);
        const uint32_t result = ctx.r3.u32;
        PPC_STORE_U32(desc + 8, 0); // never "late": the caller would loop
        if (int32_t(result) < 0 || result == kEndOfMovie)
        {
            if (result == kEndOfMovie)
            {
                std::lock_guard lock(g_mutex);
                if (g_movie && g_movie->player == player) LogEnd(*g_movie, "finished");
            }
            return; // errors and the end of the movie come from the guest player
        }

        std::lock_guard lock(g_mutex);
        Movie* movie = g_movie.get();
        if (!movie || movie->player != player) return;
        const uint64_t now = host_ui::GetActiveGameTimeNs();
        if (!movie->started)
        {
            movie->started = true;
            movie->startNs = now;
        }
        const uint32_t timeMs = uint32_t(std::min<uint64_t>(MovieTimeNs(*movie, now) / 1000000, 0xFFFFFFFFu));
        bool fresh = false;
        const movie::VideoFrame* frame = movie->stream->FrameAt(timeMs, fresh);
        // A guest frame the player just wrote (result 0) is replaced by the
        // host's current frame; without a host frame nothing is shown new.
        const bool guestWrote = result != kNoNewFrame;
        if (!frame || (!fresh && !guestWrote))
        {
            ctx.r3.u64 = kNoNewFrame;
            return;
        }
        const uint32_t w = frame->width, h = frame->height;
        const uint32_t y = PPC_LOAD_U32(desc + 12), u = PPC_LOAD_U32(desc + 16), v = PPC_LOAD_U32(desc + 20);
        const uint32_t yPitch = PPC_LOAD_U32(desc + 36), uPitch = PPC_LOAD_U32(desc + 40), vPitch = PPC_LOAD_U32(desc + 44);
        if (!y || !u || !v || yPitch < w || uPitch < w / 2 || vPitch < w / 2)
        {
            ctx.r3.u64 = kNoNewFrame;
            return;
        }
        for (uint32_t row = 0; row < h; ++row)
            std::memcpy(base + y + size_t(row) * yPitch, frame->y.data() + size_t(row) * w, w);
        for (uint32_t row = 0; row < h / 2; ++row)
        {
            std::memcpy(base + u + size_t(row) * uPitch, frame->u.data() + size_t(row) * (w / 2), w / 2);
            std::memcpy(base + v + size_t(row) * vPitch, frame->v.data() + size_t(row) * (w / 2), w / 2);
        }
        if (fresh)
        {
            // A frame more than two frame times old: the decoder is behind.
            if (timeMs > frame->ptsMs + 67) ++movie->late;
            if (movie->shown++ == 0)
                LOG_NOTICE("native movie: {} first frame at {} ms", movie->name, timeMs);
            movie->lastPts = frame->ptsMs;
        }
        ctx.r3.u64 = 0;
        return;
    }
guest:
    __imp__sub_823F0FA8(ctx, base);
}

PPC_FUNC(sub_82CE9248)
{
    const uint32_t player = ctx.r3.u32;
    __imp__sub_82CE9248(ctx, base);
    std::lock_guard lock(g_mutex);
    if (g_movie && g_movie->player == player && g_movie->started && !g_movie->paused)
    {
        g_movie->paused = true;
        g_movie->pauseStartNs = host_ui::GetActiveGameTimeNs();
    }
}

PPC_FUNC(sub_82CE9FD8)
{
    const uint32_t player = ctx.r3.u32;
    __imp__sub_82CE9FD8(ctx, base);
    std::lock_guard lock(g_mutex);
    if (g_movie && g_movie->player == player && g_movie->paused)
    {
        g_movie->paused = false;
        g_movie->pausedNs += host_ui::GetActiveGameTimeNs() - g_movie->pauseStartNs;
    }
}

// The guest video pump keeps decoding (about one core) although its frames
// are replaced; at the lowest application priority it only gets CPU time the
// audio, the game and the host decoder leave over. Nothing waits for it. The
// audio pump (vtable +140) is a separate thread and keeps its priority.
// LO_MOVIE_GUEST_VIDEO_PRIORITY=0 keeps the game's priority (hex value: use it).
PPC_FUNC(sub_82CF8140)
{
    bool lower = false;
    {
        std::lock_guard lock(g_mutex);
        lower = g_movie && ctx.r3.u32 && g_movie->player == ctx.r3.u32;
    }
#if LO_PLATFORM_SWITCH
    static const int priority = [] {
        const char* value = std::getenv("LO_MOVIE_GUEST_VIDEO_PRIORITY");
        return value && *value ? int(std::strtol(value, nullptr, 16)) : 0x3F;
    }();
    if (lower && priority >= 0x2C && priority <= 0x3F)
    {
        std::lock_guard lock(g_mutex);
        if (g_movie && !g_movie->failed && g_movie->player == ctx.r3.u32)
        {
            const int originalPriority = os::switch_platform::CurrentThreadPriority();
            if (originalPriority >= 0)
            {
                g_movie->guestVideoThread = os::switch_platform::CurrentThreadHandle();
                g_movie->guestVideoPriority = originalPriority;
                os::switch_platform::SetCurrentThreadPriority(priority);
                LOG_NOTICE("native movie: guest video decoder thread priority {:#x}", priority);
            }
        }
    }
#else
    (void)lower;
#endif
    __imp__sub_82CF8140(ctx, base);
}

bool movie::NativeMoviesEnabled()
{
    return Enabled();
}

#else

bool movie::NativeMoviesEnabled()
{
    return false;
}

#endif
