#pragma once

#include <os/logger.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#if defined(_M_X64) || defined(__x86_64__)
#include <intrin.h>
#endif
#else
#include <time.h>
#endif

// LO_RECORD_TIMING=1: splits the renderer's per-draw "record" timer (from the
// scissor setup to the end of the draw) into phases. A frame whose record time
// reaches LO_RECORD_TIMING_MS (default 8) logs the phase totals, first-use
// pipeline counts, motion-vector replay work and its slowest draws
// (LO_RECORD_TIMING_TOP, default 8). Every 600th frame logs the same summary as
// a baseline. Off: one branch per phase mark, no clock reads.
namespace gpu::record_timing
{
    inline bool Enabled()
    {
        static const bool enabled = [] {
            const char* value = std::getenv("LO_RECORD_TIMING");
            return value && *value && std::strcmp(value, "0") != 0;
        }();
        return enabled;
    }

    inline double ThresholdMs()
    {
        static const double ms = [] {
            const char* value = std::getenv("LO_RECORD_TIMING_MS");
            const double parsed = value && *value ? std::atof(value) : 8.0;
            return parsed > 0 ? parsed : 8.0;
        }();
        return ms;
    }

    inline uint32_t TopCount()
    {
        static const uint32_t count = [] {
            const char* value = std::getenv("LO_RECORD_TIMING_TOP");
            const long parsed = value && *value ? std::strtol(value, nullptr, 10) : 8;
            return uint32_t(parsed < 1 ? 1 : parsed > 32 ? 32 : parsed);
        }();
        return count;
    }

    enum class Phase : uint8_t {
        Scissor, Promote, Framebuffer, Trace, Pipeline, Sets, Occlusion, Draw,
        Hdr, FgUi, FsrAlpha, SceneDlss, MvTrack, MvPrepare, MvDraw, Tail, Count
    };
    inline constexpr size_t kPhases = size_t(Phase::Count);

    inline const char* PhaseName(Phase phase)
    {
        static constexpr const char* names[kPhases] = {
            "scissor", "promote", "framebuffer", "trace", "pipeline", "sets", "occlusion", "draw",
            "hdr", "fg_ui", "fsr_alpha", "scene_dlss", "mv_track", "mv_prepare", "mv_draw", "tail" };
        return size_t(phase) < kPhases ? names[size_t(phase)] : "?";
    }

    // Where the bound game-state pipeline came from (renderer PipelineSlot).
    enum class Origin : uint8_t { Unknown, Prebuilt, ScenePrefetch, Sibling, DrawJob, Served, Created, Count };
    inline constexpr size_t kOrigins = size_t(Origin::Count);

    inline const char* OriginName(Origin origin)
    {
        static constexpr const char* names[kOrigins] = {
            "unknown", "prebuilt", "scene_prefetch", "sibling", "draw_job", "served", "created" };
        return size_t(origin) < kOrigins ? names[size_t(origin)] : "?";
    }

    // Thread CPU time and wall time in one unit, for the on-CPU share of a
    // scope: a low share means the render thread waited or was preempted.
    struct CpuStamp { uint64_t thread = 0, wall = 0; };

    inline CpuStamp ReadCpu()
    {
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
        // Both in TSC ticks on current Windows.
        ULONG64 cycles = 0;
        QueryThreadCycleTime(GetCurrentThread(), &cycles);
        return { uint64_t(cycles), uint64_t(__rdtsc()) };
#elif !defined(_WIN32)
        timespec thread{}, wall{};
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &thread);
        clock_gettime(CLOCK_MONOTONIC, &wall);
        return { uint64_t(thread.tv_sec) * 1000000000ull + uint64_t(thread.tv_nsec),
            uint64_t(wall.tv_sec) * 1000000000ull + uint64_t(wall.tv_nsec) };
#else
        return {};
#endif
    }

    // Work counters outside the record scope's own code. The renderer supplies
    // a reader; per-draw values are the difference across the draw.
    struct Counters {
        uint32_t mvTranslated = 0, mvShaders = 0, mvPipelines = 0, mvPending = 0, mvScene = 0;
        double mvTranslateMs = 0, mvShaderMs = 0, mvPipelineMs = 0, mvSceneMs = 0;
        uint32_t framebuffers = 0, otherPipelines = 0;
        double otherPipelineMs = 0;
    };
    using CounterReader = void (*)(const void* context, Counters& out);

    struct DrawSample {
        double ms = 0, onCpu = -1;
        std::array<float, kPhases> phaseMs{};
        uint64_t vs = 0, ps = 0;
        uint32_t draw = 0, indexCount = 0, primitive = 0;
        uint32_t rtBase = 0, rtFormat = 0, width = 0, height = 0;
        bool depth = false, pipelineKnown = false, firstUse = false, linked = false;
        Origin origin = Origin::Unknown;
        Counters work{};
    };

    class Recorder
    {
        using Clock = std::chrono::steady_clock;
        static double Ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

        CounterReader reader = nullptr;
        const void* readerContext = nullptr;
        DrawSample current{};
        Counters before{};
        CpuStamp cpuBefore{};
        Phase phase = Phase::Scissor;
        Clock::time_point started{}, marked{}, lastFrameEnd{};
        bool active = false;

        // Frame totals.
        std::array<double, kPhases> phaseMs{};
        double recordMs = 0, firstUseMs = 0;
        uint64_t threadTicks = 0, wallTicks = 0;
        uint32_t draws = 0, slowDraws = 0;
        std::array<uint32_t, kOrigins> firstUse{};
        uint32_t firstUseLinked = 0;
        Counters work{};
        std::array<DrawSample, 32> top{};
        uint32_t topCount = 0;

        static void Add(Counters& total, const Counters& a, const Counters& b)
        {
            total.mvTranslated += a.mvTranslated - b.mvTranslated;
            total.mvShaders += a.mvShaders - b.mvShaders;
            total.mvPipelines += a.mvPipelines - b.mvPipelines;
            total.mvPending += a.mvPending - b.mvPending;
            total.mvScene += a.mvScene - b.mvScene;
            total.mvTranslateMs += a.mvTranslateMs - b.mvTranslateMs;
            total.mvShaderMs += a.mvShaderMs - b.mvShaderMs;
            total.mvPipelineMs += a.mvPipelineMs - b.mvPipelineMs;
            total.mvSceneMs += a.mvSceneMs - b.mvSceneMs;
            total.framebuffers += a.framebuffers - b.framebuffers;
            total.otherPipelines += a.otherPipelines - b.otherPipelines;
            total.otherPipelineMs += a.otherPipelineMs - b.otherPipelineMs;
        }
        void ReadCounters(Counters& out) const { if (reader) reader(readerContext, out); else out = {}; }

        void Keep(const DrawSample& sample)
        {
            const uint32_t limit = TopCount();
            if (topCount < limit) { top[topCount++] = sample; return; }
            uint32_t smallest = 0;
            for (uint32_t i = 1; i < topCount; ++i)
                if (top[i].ms < top[smallest].ms) smallest = i;
            if (sample.ms > top[smallest].ms) top[smallest] = sample;
        }

        static std::string Phases(const std::array<double, kPhases>& values)
        {
            std::string out;
            for (size_t i = 0; i < kPhases; ++i)
                out += fmt::format("{}{}={:.2f}", i ? " " : "", PhaseName(Phase(i)), values[i]);
            return out;
        }
        static std::string WorkText(const Counters& w)
        {
            return fmt::format("mv_translated={} ({:.2f} ms) mv_shaders={} ({:.2f} ms) mv_pipelines={} ({:.2f} ms) mv_pending={} mv_scene={} ({:.2f} ms) framebuffers={} other_pipelines={} ({:.2f} ms)",
                w.mvTranslated, w.mvTranslateMs, w.mvShaders, w.mvShaderMs, w.mvPipelines, w.mvPipelineMs,
                w.mvPending, w.mvScene, w.mvSceneMs, w.framebuffers, w.otherPipelines, w.otherPipelineMs);
        }

    public:
        void SetCounterReader(CounterReader read, const void* context) { reader = read; readerContext = context; }

        void Begin(uint32_t draw)
        {
            current = {};
            current.draw = draw;
            ReadCounters(before);
            cpuBefore = ReadCpu();
            phase = Phase::Scissor;
            started = marked = Clock::now();
            active = true;
        }
        void Enter(Phase next)
        {
            if (!active) return;
            const auto now = Clock::now();
            current.phaseMs[size_t(phase)] += float(Ms(now - marked));
            marked = now;
            phase = next;
        }
        DrawSample& Current() { return current; }
        void End()
        {
            if (!active) return;
            const auto now = Clock::now();
            current.phaseMs[size_t(phase)] += float(Ms(now - marked));
            current.ms = Ms(now - started);
            const CpuStamp cpuAfter = ReadCpu();
            const uint64_t thread = cpuAfter.thread - cpuBefore.thread, wall = cpuAfter.wall - cpuBefore.wall;
            if (wall) current.onCpu = double(thread) / double(wall);
            threadTicks += thread; wallTicks += wall;
            Counters after{};
            ReadCounters(after);
            Add(current.work, after, before);
            Add(work, after, before);
            active = false;

            ++draws;
            recordMs += current.ms;
            for (size_t i = 0; i < kPhases; ++i) phaseMs[i] += current.phaseMs[i];
            if (current.ms >= 1.0) ++slowDraws;
            if (current.firstUse) {
                ++firstUse[size_t(current.origin)];
                if (current.linked) ++firstUseLinked;
                firstUseMs += current.ms;
            }
            Keep(current);
        }

        // Frame end on the render thread. Logs at the threshold or every 600th frame.
        void EndFrame(uint64_t frame)
        {
            const auto now = Clock::now();
            const double frameMs = lastFrameEnd.time_since_epoch().count() ? Ms(now - lastFrameEnd) : 0.0;
            lastFrameEnd = now;
            const bool slow = recordMs >= ThresholdMs();
            if ((slow || frame % 600 == 0) && draws) {
                uint32_t firstUseTotal = 0;
                std::string origins;
                for (size_t i = 0; i < kOrigins; ++i) {
                    firstUseTotal += firstUse[i];
                    if (firstUse[i]) origins += fmt::format(" {}={}", OriginName(Origin(i)), firstUse[i]);
                }
                LOG_INFO("record timing frame={} {} record_ms={:.2f} frame_ms={:.1f} draws={} draws_over_1ms={} on_cpu={} | {} | first_use={}{} linked={} first_use_ms={:.2f} | {}",
                    frame, slow ? "slow" : "periodic", recordMs, frameMs, draws, slowDraws,
                    wallTicks ? fmt::format("{:.2f}", double(threadTicks) / double(wallTicks)) : std::string("n/a"),
                    Phases(phaseMs), firstUseTotal, origins, firstUseLinked, firstUseMs, WorkText(work));
                if (slow) {
                    std::sort(top.begin(), top.begin() + topCount,
                        [](const DrawSample& a, const DrawSample& b) { return a.ms > b.ms; });
                    for (uint32_t i = 0; i < topCount; ++i) {
                        const auto& s = top[i];
                        // Two largest phases of this draw.
                        size_t first = 0, second = 1;
                        if (s.phaseMs[second] > s.phaseMs[first]) std::swap(first, second);
                        for (size_t p = 2; p < kPhases; ++p) {
                            if (s.phaseMs[p] > s.phaseMs[first]) { second = first; first = p; }
                            else if (s.phaseMs[p] > s.phaseMs[second]) second = p;
                        }
                        LOG_INFO("record timing frame={} slow_draw={} draw={} ms={:.3f} on_cpu={} top={}:{:.3f} next={}:{:.3f} vs={:016x} ps={:016x} pipeline={} first_use={} linked={} rt={:#x}/{} {}x{} depth={} prim={} indices={} | {}",
                            frame, i + 1, s.draw, s.ms, s.onCpu < 0 ? std::string("n/a") : fmt::format("{:.2f}", s.onCpu),
                            PhaseName(Phase(first)), s.phaseMs[first], PhaseName(Phase(second)), s.phaseMs[second],
                            s.vs, s.ps, s.pipelineKnown ? OriginName(s.origin) : "not_game_state", s.firstUse, s.linked,
                            s.rtBase, s.rtFormat, s.width, s.height, s.depth, s.primitive, s.indexCount, WorkText(s.work));
                    }
                }
            }
            phaseMs = {};
            recordMs = firstUseMs = 0;
            threadTicks = wallTicks = 0;
            draws = slowDraws = 0;
            firstUse = {};
            firstUseLinked = 0;
            work = {};
            topCount = 0;
        }
    };

    // RAII scope for one draw's record section; early returns close it.
    class Probe
    {
        Recorder* recorder;
    public:
        Probe(Recorder& owner, bool enabled, uint32_t draw) : recorder(enabled ? &owner : nullptr)
        {
            if (recorder) recorder->Begin(draw);
        }
        ~Probe() { if (recorder) recorder->End(); }
        Probe(const Probe&) = delete;
        Probe& operator=(const Probe&) = delete;
        void Enter(Phase phase) { if (recorder) recorder->Enter(phase); }
        DrawSample* Sample() { return recorder ? &recorder->Current() : nullptr; }
    };
}
