// Cutscene replacement. A mod's MP4 for the playing video is decoded on the
// host (movie/movie_player.h) and drawn over the guest's own movie quad. The
// guest keeps playing its WMV, so audio, subtitles, skip and end are its own.
//
// The guest frame update sleeps the render thread until the next 29.97 fps
// frame is due, which holds the game at 30 fps during a cutscene. While a
// replacement plays, the update is skipped until that frame is due, so the
// replacement can be drawn at the game's frame rate. LO_MOVIE_FULL_RATE=0
// keeps the guest pacing. LO_TRACE_MOVIE=1 logs the player calls.
#include <stdafx.h>

#include "gpu/movie_replace.h"
#include "gpu/frame_plan.h"
#include "cpu/ppc_context.h"
#include "kernel/memory.h"
#include <movie/movie_player.h>
#include <os/logger.h>
#if defined(__ANDROID__)
#include <diagnostics/android_tester.h>
#endif

#include <bit>
#include <cstdlib>
#include <string>

extern "C" PPC_FUNC(__imp__sub_82815248);
extern "C" PPC_FUNC(__imp__sub_823F0038);
extern "C" PPC_FUNC(__imp__sub_82813888);
extern "C" PPC_FUNC(__imp__sub_82815010);
extern "C" PPC_FUNC(__imp__sub_82814570);
extern "C" PPC_FUNC(__imp__sub_82815640);

namespace
{
bool Flag(const char* name, bool fallback)
{
    const char* v = std::getenv(name);
    return v && *v ? *v != '0' : fallback;
}

bool Trace()
{
    static const bool enabled = Flag("LO_TRACE_MOVIE", false);
    return enabled;
}

bool FullRate()
{
    static const bool enabled = Flag("LO_MOVIE_FULL_RATE", true);
    return enabled;
}

bool Valid(uint32_t address, uint32_t size)
{
    return g_memory.base && address >= 0x10000 && uint64_t(address) + size <= 0x100000000ull;
}

uint32_t Word(uint32_t address)
{
    return Valid(address, 4) ? __builtin_bswap32(*reinterpret_cast<const uint32_t*>(g_memory.base + address)) : 0;
}

uint64_t Dword(uint32_t address)
{
    return uint64_t(Word(address)) << 32 | Word(address + 4);
}

// UTF-16BE text, ASCII only (others become '?').
std::string Wide(uint32_t address, uint32_t count)
{
    std::string text;
    for (uint32_t i = 0; i < count && Valid(address + i * 2, 2); ++i)
    {
        const uint16_t c = uint16_t(g_memory.base[address + i * 2] << 8 | g_memory.base[address + i * 2 + 1]);
        if (!c) break;
        text += c < 0x80 ? char(c) : '?';
    }
    return text;
}

// "..\bin\xenon\mov\mv_00v.wmv" -> "mv_00v", the movie key mods use.
std::string Stem(const std::string& path)
{
    const size_t slash = path.find_last_of("\\/");
    std::string name = path.substr(slash == std::string::npos ? 0 : slash + 1);
    if (const size_t dot = name.rfind('.'); dot != std::string::npos) name.resize(dot);
    for (char& c : name)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return name;
}

thread_local uint32_t proxyMovie = 0;
}

namespace gpu::movie_replace
{
void ProxyBegin(uint32_t guestMovie)
{
    proxyMovie = guestMovie;
}

void ProxyEnd()
{
    proxyMovie = 0;
}

void EmitDraw(uint32_t device, uint32_t surfaceInfo, uint32_t colorInfo, float x, float y, float width, float height)
{
    if (!proxyMovie) return;
    const uint32_t session = movie::ReplacementSession(proxyMovie);
    if (!session) return;
    const int64_t us = movie::ReplacementTimeUs(session, movie::NowMs());
    const uint32_t values[WordCount] = {Magic, surfaceInfo, colorInfo,
        std::bit_cast<uint32_t>(x), std::bit_cast<uint32_t>(y), std::bit_cast<uint32_t>(width), std::bit_cast<uint32_t>(height),
        session, uint32_t(uint64_t(us)), uint32_t(uint64_t(us) >> 32), Magic};
    if (Trace())
    {
        static uint32_t draws = 0;
        if (++draws <= 600 || draws % 60 == 0)
            LOG_INFO("movie trace: draw#{} session={} t={:.1f}ms rect=({:.1f},{:.1f} {:.1f}x{:.1f})", draws, session, us / 1000.0, x, y, width, height);
    }
    gpu::frame_plan::EmitPrivatePacket(device, RegisterBase, values);
}
}

// Movie start, game thread: r3 = movie object, r4 = UTF-16 path of the WMV.
PPC_FUNC(sub_82815248)
{
    const uint32_t movie = ctx.r3.u32;
    const std::string path = Wide(ctx.r4.u32, 128);
    if (Trace())
        LOG_INFO("movie trace: start M={:08x} '{}' lr={:08x}", movie, path, uint32_t(ctx.lr));
    __imp__sub_82815248(ctx, base);
#if defined(__ANDROID__)
    ::diagnostics::android_tester::g_recorder.SetMovieActive(true);
#endif
    movie::StartReplacement(movie, Stem(path));
}

// Frame update, render thread: r3 = movie object; returns true when a new
// frame went to the guest textures. player (+20) +288 is that frame's time in ms.
PPC_FUNC(sub_823F0038)
{
    const uint32_t movie = ctx.r3.u32;
    const uint32_t session = movie::ReplacementSession(movie);
    const double entered = movie::NowMs();
    // Also the proxy's repeat call after a late frame: the guest then fetches
    // the next one and would sleep until it is due.
    if (session && FullRate() && !movie::GuestUpdateDue(session, entered))
    {
        ctx.r3.u64 = 0; // as when the guest has no new frame yet
        return;
    }
#if defined(__ANDROID__)
    auto& tester = ::diagnostics::android_tester::g_recorder;
    const bool measureMovie = tester.recording.load(std::memory_order_relaxed);
    const uint64_t movieStart = measureMovie ? ::diagnostics::android_tester::NowNs() : 0;
#endif
    __imp__sub_823F0038(ctx, base);
    const bool shown = (ctx.r3.u32 & 0xFF) != 0;
    const uint64_t pts = Dword(Word(movie + 20) + 288);
#if defined(__ANDROID__)
    if (measureMovie) tester.SetMovieUpdate(shown, pts, ::diagnostics::android_tester::NowNs() - movieStart);
#endif
    if (session && shown)
        movie::GuestFrameShown(session, double(pts), movie::NowMs());
    if (Trace())
    {
        static uint32_t calls = 0;
        if (++calls <= 900 || calls % 30 == 0)
            LOG_INFO("movie trace: update#{} ret={} took={:.2f}ms pts={} session={}", calls, int(shown), movie::NowMs() - entered, pts, session);
    }
}

PPC_FUNC(sub_82813888)
{
    if (Trace())
        LOG_INFO("movie trace: 82813888 r3={:08x} r4={:08x} lr={:08x}", ctx.r3.u32, ctx.r4.u32, uint32_t(ctx.lr));
    __imp__sub_82813888(ctx, base);
}

PPC_FUNC(sub_82815010)
{
    if (Trace())
        LOG_INFO("movie trace: 82815010 r3={:08x} r4={:08x} lr={:08x}", ctx.r3.u32, ctx.r4.u32, uint32_t(ctx.lr));
    __imp__sub_82815010(ctx, base);
}

// Release command, game thread (skip and natural end both come here).
PPC_FUNC(sub_82814570)
{
    if (Trace())
        LOG_INFO("movie trace: release M={:08x} lr={:08x}", ctx.r3.u32, uint32_t(ctx.lr));
    __imp__sub_82814570(ctx, base);
}

// Render-thread close after the movie's last draw: r3 = movie object.
PPC_FUNC(sub_82815640)
{
    const uint32_t movie = ctx.r3.u32;
    if (Trace())
        LOG_INFO("movie trace: close M={:08x} r4={:08x} lr={:08x}", movie, ctx.r4.u32, uint32_t(ctx.lr));
    __imp__sub_82815640(ctx, base);
#if defined(__ANDROID__)
    ::diagnostics::android_tester::g_recorder.SetMovieActive(false);
#endif
    movie::StopReplacement(movie);
}
