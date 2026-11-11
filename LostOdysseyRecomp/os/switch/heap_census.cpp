// Heap census (LO_SWITCH_HEAP_CENSUS=ON builds only): who holds newlib's heap.
//
// The CMake option links newlib's allocator entry points through the wrappers below (-Wl,--wrap=_malloc_r and the
// others); a default build has neither the wrappers nor this file's code. With the option on, LO_HEAP_CENSUS=1 in
// env.txt (or a sampling period in bytes, LO_HEAP_CENSUS=<bytes>) starts it once env.txt is read; until then, and
// without it, each call costs one relaxed load and a branch.
//
// Two tables, both since the census started:
// - by chunk size (powers of two): every allocation and free, exact. Bytes are dlmalloc chunk sizes, what
//   mallinfo().uordblks adds up, so a class's net bytes are its share of uordblks' growth.
// - by call site: one allocation in about every <period> bytes allocated on a thread (period 32 KiB by default;
//   anything at least that big always), with its call stack (_Unwind_Backtrace, up to 8 frames above the wrapper).
//   A site's live estimate counts each sampled allocation still held as max(size, period) bytes.
// Every LO_HEAP_CENSUS_SECONDS (60 by default) the log gets a "heap census:" summary (NOTICE) and
// state/heap-census/<seconds>.txt the full tables; tools/switch/heap_census.py diffs two of them and names the
// frames from the ELF.
//
// Nested calls (memalign, calloc and realloc call malloc and free inside newlib; the unwinder can allocate) go
// straight to newlib: a thread-local depth guard. Pointers allocated before the start are freed into the size table
// too, so its counts can go below zero: compare two reports, not one against nothing.
#include <os/platform.h>
#if LO_PLATFORM_SWITCH && defined(LO_SWITCH_HEAP_CENSUS)
#include <os/detach_thread.h>
#include <os/heap_census.h>
#include <os/logger.h>
#include <os/thread_name.h>

#include <switch.h>

#include <malloc.h>
#include <reent.h>
#include <unwind.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <thread>

extern "C"
{
void* __real__malloc_r(struct _reent*, size_t);
void __real__free_r(struct _reent*, void*);
void* __real__calloc_r(struct _reent*, size_t, size_t);
void* __real__realloc_r(struct _reent*, void*, size_t);
void* __real__memalign_r(struct _reent*, size_t, size_t);
// The executable's first instruction: frames are logged as offsets into the ELF (cpu_profiler_switch.cpp).
extern char _start;
}

namespace
{
constexpr int kClasses = 48;          // chunk size classes: [2^i, 2^(i+1))
constexpr int kFrames = 8;
constexpr int kSkipFrames = 3;        // CaptureStack, OnAlloc, the wrapper
constexpr uint32_t kSites = 1u << 13; // call stacks
constexpr uint32_t kLive = 1u << 18;  // sampled allocations still held (16 B each: 4 MiB)
constexpr uint32_t kFilter = 1u << 22; // counting filter over the sampled pointers (4 MiB)

enum Op { OpMalloc, OpFree, OpCalloc, OpRealloc, OpMemalign, OpCount };

std::atomic<bool> g_on{false};
int64_t g_period = 32768;
uintptr_t g_base = 0;

std::atomic<int64_t> g_classCount[kClasses];
std::atomic<int64_t> g_classBytes[kClasses];
std::atomic<uint64_t> g_ops[OpCount];
std::atomic<uint64_t> g_samples{0}, g_droppedSites{0}, g_droppedLive{0};

struct Site
{
    uint64_t hash;       // 0: free slot
    uint32_t frames[kFrames];
    uint32_t depth;
    uint32_t lastSize;
    int64_t liveCount;
    int64_t liveBytes;   // estimate: max(size, period) per sampled allocation held
    int64_t realBytes;   // the sampled allocations' own chunk bytes
    uint64_t sampled;    // ever
};

struct Live
{
    uintptr_t ptr;       // 0: empty
    uint32_t site;
    uint32_t weight;     // KiB-rounded would lose small ones: bytes, capped at 4 GiB
    uint32_t chunk;
    uint32_t pad;
};

Mutex g_lock;
Site g_site[kSites];
Live g_live[kLive];
uint32_t g_liveUsed = 0;
std::atomic<uint8_t> g_filter[kFilter];

thread_local int t_depth = 0;
thread_local int64_t t_countdown = 0;
thread_local uint64_t t_random = 0;

inline size_t ChunkBytes(const void* p)
{
    // newlib's dlmalloc: the chunk's size word sits just below the pointer; the low bits are flags.
    return reinterpret_cast<const size_t*>(p)[-1] & ~size_t(7);
}

inline int ClassOf(size_t bytes)
{
    const int c = bytes ? 63 - __builtin_clzll(bytes) : 0;
    return c < kClasses ? c : kClasses - 1;
}

inline uint64_t Mix(uint64_t x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;
    return x;
}

inline uint32_t FilterSlot(uintptr_t p) { return uint32_t(Mix(p) >> 20) & (kFilter - 1); }
inline uint32_t LiveSlot(uintptr_t p) { return uint32_t(Mix(p ^ 0x9e3779b97f4a7c15ull)) & (kLive - 1); }

// Between 1/2 and 3/2 of the period, so that a pattern of allocations repeating with the period is not always
// sampled at the same point.
int64_t NextPeriod()
{
    if (t_random == 0)
        t_random = Mix(reinterpret_cast<uintptr_t>(&t_random) ^ armGetSystemTick()) | 1;
    t_random ^= t_random << 13;
    t_random ^= t_random >> 7;
    t_random ^= t_random << 17;
    return g_period / 2 + int64_t(t_random % uint64_t(g_period));
}

struct Backtrace
{
    uint32_t frames[kFrames + kSkipFrames];
    int depth;
};

_Unwind_Reason_Code CollectFrame(struct _Unwind_Context* context, void* arg)
{
    Backtrace& bt = *static_cast<Backtrace*>(arg);
    if (bt.depth >= kFrames + kSkipFrames)
        return _URC_END_OF_STACK;
    const uintptr_t ip = _Unwind_GetIP(context);
    if (ip == 0)
        return _URC_END_OF_STACK;
    bt.frames[bt.depth++] = uint32_t(ip - g_base);
    return _URC_NO_REASON;
}

__attribute__((noinline)) void CaptureStack(Backtrace& bt)
{
    bt.depth = 0;
    _Unwind_Backtrace(CollectFrame, &bt);
}

// With g_lock held.
uint32_t FindSite(const uint32_t* frames, uint32_t depth, uint64_t hash)
{
    for (uint32_t i = uint32_t(hash) & (kSites - 1), n = 0; n < kSites / 2; ++n, i = (i + 1) & (kSites - 1))
    {
        Site& s = g_site[i];
        if (s.hash == 0)
        {
            s.hash = hash;
            s.depth = depth;
            std::memcpy(s.frames, frames, depth * sizeof(uint32_t));
            return i;
        }
        if (s.hash == hash && s.depth == depth && std::memcmp(s.frames, frames, depth * sizeof(uint32_t)) == 0)
            return i;
    }
    return UINT32_MAX;
}

// With g_lock held; linear probing, deleted with a backward shift so that no tombstones build up.
bool InsertLive(uintptr_t p, uint32_t site, uint32_t weight, uint32_t chunk)
{
    if (g_liveUsed >= kLive / 2)
        return false;
    uint32_t i = LiveSlot(p);
    while (g_live[i].ptr != 0)
        i = (i + 1) & (kLive - 1);
    g_live[i] = Live{ p, site, weight, chunk, 0 };
    ++g_liveUsed;
    return true;
}

bool RemoveLive(uintptr_t p, Live& out)
{
    uint32_t i = LiveSlot(p);
    while (g_live[i].ptr != p)
    {
        if (g_live[i].ptr == 0)
            return false;
        i = (i + 1) & (kLive - 1);
    }
    out = g_live[i];
    // Backward-shift deletion.
    uint32_t hole = i;
    for (uint32_t j = (i + 1) & (kLive - 1); g_live[j].ptr != 0; j = (j + 1) & (kLive - 1))
    {
        const uint32_t home = LiveSlot(g_live[j].ptr);
        // j's entry may move into the hole when its home is not in (hole, j].
        const bool between = hole <= j ? (home > hole && home <= j) : (home > hole || home <= j);
        if (!between)
        {
            g_live[hole] = g_live[j];
            hole = j;
        }
    }
    g_live[hole].ptr = 0;
    --g_liveUsed;
    return true;
}

__attribute__((noinline)) void OnAlloc(void* p, size_t requested)
{
    const size_t chunk = ChunkBytes(p);
    const int c = ClassOf(chunk);
    g_classCount[c].fetch_add(1, std::memory_order_relaxed);
    g_classBytes[c].fetch_add(int64_t(chunk), std::memory_order_relaxed);
    if (t_countdown == 0)
        t_countdown = NextPeriod(); // the thread's first allocation
    t_countdown -= int64_t(requested);
    if (t_countdown > 0)
        return;
    t_countdown = NextPeriod();
    Backtrace bt;
    CaptureStack(bt);
    const uint32_t depth = bt.depth > kSkipFrames ? uint32_t(bt.depth - kSkipFrames) : 0;
    const uint32_t* frames = bt.frames + kSkipFrames;
    uint64_t hash = 0x6a09e667f3bcc909ull;
    for (uint32_t i = 0; i < depth; ++i)
        hash = Mix(hash ^ frames[i]);
    hash |= 1;
    const int64_t weight = std::max<int64_t>(int64_t(requested), g_period);
    const uintptr_t key = reinterpret_cast<uintptr_t>(p);
    g_samples.fetch_add(1, std::memory_order_relaxed);
    mutexLock(&g_lock);
    const uint32_t site = FindSite(frames, depth, hash);
    if (site == UINT32_MAX)
        g_droppedSites.fetch_add(1, std::memory_order_relaxed);
    else if (!InsertLive(key, site, uint32_t(std::min<int64_t>(weight, UINT32_MAX)), uint32_t(std::min<size_t>(chunk, UINT32_MAX))))
        g_droppedLive.fetch_add(1, std::memory_order_relaxed);
    else
    {
        Site& s = g_site[site];
        s.liveCount += 1;
        s.liveBytes += weight;
        s.realBytes += int64_t(chunk);
        s.sampled += 1;
        s.lastSize = uint32_t(std::min<size_t>(requested, UINT32_MAX));
        g_filter[FilterSlot(key)].fetch_add(1, std::memory_order_relaxed);
    }
    mutexUnlock(&g_lock);
}

void OnFree(void* p)
{
    const size_t chunk = ChunkBytes(p);
    const int c = ClassOf(chunk);
    g_classCount[c].fetch_sub(1, std::memory_order_relaxed);
    g_classBytes[c].fetch_sub(int64_t(chunk), std::memory_order_relaxed);
    const uintptr_t key = reinterpret_cast<uintptr_t>(p);
    std::atomic<uint8_t>& f = g_filter[FilterSlot(key)];
    if (f.load(std::memory_order_relaxed) == 0)
        return;
    mutexLock(&g_lock);
    Live live;
    if (RemoveLive(key, live))
    {
        Site& s = g_site[live.site];
        s.liveCount -= 1;
        s.liveBytes -= live.weight;
        s.realBytes -= live.chunk;
        f.fetch_sub(1, std::memory_order_relaxed);
    }
    mutexUnlock(&g_lock);
}

struct Snapshot
{
    double seconds;
    size_t uordblks, arena;
    uint64_t ops[OpCount];
    uint64_t samples, droppedSites, droppedLive, liveUsed;
    int64_t classCount[kClasses], classBytes[kClasses];
};

Site g_copy[kSites]; // the report thread's copy of g_site

void Report(const std::filesystem::path& folder, std::chrono::steady_clock::time_point start)
{
    Snapshot snap{};
    snap.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    // This thread's own allocations (the strings and the file below) are counted like any other.
    const struct mallinfo mi = mallinfo();
    snap.uordblks = size_t(mi.uordblks);
    snap.arena = size_t(mi.arena);
    for (int i = 0; i < OpCount; ++i)
        snap.ops[i] = g_ops[i].load(std::memory_order_relaxed);
    for (int i = 0; i < kClasses; ++i)
    {
        snap.classCount[i] = g_classCount[i].load(std::memory_order_relaxed);
        snap.classBytes[i] = g_classBytes[i].load(std::memory_order_relaxed);
    }
    snap.samples = g_samples.load(std::memory_order_relaxed);
    snap.droppedSites = g_droppedSites.load(std::memory_order_relaxed);
    snap.droppedLive = g_droppedLive.load(std::memory_order_relaxed);
    mutexLock(&g_lock);
    std::memcpy(g_copy, g_site, sizeof(g_site));
    snap.liveUsed = g_liveUsed;
    mutexUnlock(&g_lock);

    int64_t classTotal = 0;
    for (int i = 0; i < kClasses; ++i)
        classTotal += snap.classBytes[i];
    uint32_t sites = 0;
    int64_t liveEstimate = 0;
    for (const Site& s : g_copy)
        if (s.hash != 0)
        {
            ++sites;
            liveEstimate += s.liveBytes;
        }

    char name[32];
    std::snprintf(name, sizeof(name), "%06u.txt", unsigned(snap.seconds + 0.5));
    const std::filesystem::path path = folder / name;
    if (FILE* out = std::fopen(path.c_str(), "w"))
    {
        std::fprintf(out, "heap-census 1\nseconds %.1f\nperiod %" PRId64 "\nuordblks %zu\narena %zu\n", snap.seconds,
            g_period, snap.uordblks, snap.arena);
        std::fprintf(out, "ops malloc %" PRIu64 " free %" PRIu64 " calloc %" PRIu64 " realloc %" PRIu64 " memalign %" PRIu64 "\n",
            snap.ops[OpMalloc], snap.ops[OpFree], snap.ops[OpCalloc], snap.ops[OpRealloc], snap.ops[OpMemalign]);
        std::fprintf(out, "samples %" PRIu64 " live %" PRIu64 " dropped_sites %" PRIu64 " dropped_live %" PRIu64 "\n",
            snap.samples, snap.liveUsed, snap.droppedSites, snap.droppedLive);
        for (int i = 0; i < kClasses; ++i)
            if (snap.classCount[i] != 0 || snap.classBytes[i] != 0)
                std::fprintf(out, "class %d %" PRId64 " %" PRId64 "\n", i, snap.classCount[i], snap.classBytes[i]);
        // site <live count> <live estimate> <live chunk bytes> <sampled ever> <last size> <frames...>
        for (const Site& s : g_copy)
        {
            if (s.hash == 0)
                continue;
            std::fprintf(out, "site %" PRId64 " %" PRId64 " %" PRId64 " %" PRIu64 " %u", s.liveCount, s.liveBytes,
                s.realBytes, s.sampled, s.lastSize);
            for (uint32_t i = 0; i < s.depth; ++i)
                std::fprintf(out, " %x", s.frames[i]);
            std::fputc('\n', out);
        }
        std::fclose(out);
    }

    // The log: the totals and the five sites holding the most.
    LOG_NOTICE("heap census: {:.0f} s, uordblks {} arena {}, size table net {} bytes, ops malloc {} free {} calloc {} "
               "realloc {} memalign {}, samples {} (live {}, {} sites, estimate {} bytes, dropped {}/{}) -> {}",
        snap.seconds, snap.uordblks, snap.arena, classTotal, snap.ops[OpMalloc], snap.ops[OpFree], snap.ops[OpCalloc],
        snap.ops[OpRealloc], snap.ops[OpMemalign], snap.samples, snap.liveUsed, sites, liveEstimate, snap.droppedSites,
        snap.droppedLive, path.string());
    uint32_t order[5]{};
    int count = 0;
    for (uint32_t i = 0; i < kSites; ++i)
    {
        if (g_copy[i].hash == 0)
            continue;
        if (count < 5)
            order[count++] = i;
        else if (g_copy[i].liveBytes > g_copy[order[4]].liveBytes)
            order[4] = i;
        else
            continue;
        for (int at = count - 1; at > 0 && g_copy[order[at]].liveBytes > g_copy[order[at - 1]].liveBytes; --at)
            std::swap(order[at], order[at - 1]);
    }
    for (int k = 0; k < count; ++k)
    {
        const Site& s = g_copy[order[k]];
        std::string frames;
        for (uint32_t i = 0; i < s.depth && i < 5; ++i)
            frames += fmt::format(" {:x}", s.frames[i]);
        LOG_NOTICE("heap census: site {} held {} (estimate {} bytes, {} chunk bytes), last size {}, frames{}", k + 1,
            s.liveCount, s.liveBytes, s.realBytes, s.lastSize, frames);
    }
}
} // namespace

extern "C" void* __wrap__malloc_r(struct _reent* r, size_t n)
{
    if (!g_on.load(std::memory_order_relaxed) || t_depth != 0)
        return __real__malloc_r(r, n);
    ++t_depth;
    g_ops[OpMalloc].fetch_add(1, std::memory_order_relaxed);
    void* p = __real__malloc_r(r, n);
    if (p)
        OnAlloc(p, n);
    --t_depth;
    return p;
}

extern "C" void __wrap__free_r(struct _reent* r, void* p)
{
    if (!g_on.load(std::memory_order_relaxed) || t_depth != 0 || p == nullptr)
        return __real__free_r(r, p);
    ++t_depth;
    g_ops[OpFree].fetch_add(1, std::memory_order_relaxed);
    // Out of the tables before newlib can hand the address to another thread.
    OnFree(p);
    __real__free_r(r, p);
    --t_depth;
}

extern "C" void* __wrap__calloc_r(struct _reent* r, size_t n, size_t size)
{
    if (!g_on.load(std::memory_order_relaxed) || t_depth != 0)
        return __real__calloc_r(r, n, size);
    ++t_depth;
    g_ops[OpCalloc].fetch_add(1, std::memory_order_relaxed);
    void* p = __real__calloc_r(r, n, size);
    if (p)
        OnAlloc(p, n * size);
    --t_depth;
    return p;
}

extern "C" void* __wrap__memalign_r(struct _reent* r, size_t alignment, size_t n)
{
    if (!g_on.load(std::memory_order_relaxed) || t_depth != 0)
        return __real__memalign_r(r, alignment, n);
    ++t_depth;
    g_ops[OpMemalign].fetch_add(1, std::memory_order_relaxed);
    void* p = __real__memalign_r(r, alignment, n);
    if (p)
        OnAlloc(p, n);
    --t_depth;
    return p;
}

extern "C" void* __wrap__realloc_r(struct _reent* r, void* old, size_t n)
{
    if (!g_on.load(std::memory_order_relaxed) || t_depth != 0)
        return __real__realloc_r(r, old, n);
    ++t_depth;
    g_ops[OpRealloc].fetch_add(1, std::memory_order_relaxed);
    // A realloc is a free of the old block and an allocation of the new one, even when it stays in place: the new
    // block is attributed to the realloc's caller.
    if (old)
        OnFree(old);
    void* p = __real__realloc_r(r, old, n);
    if (p)
        OnAlloc(p, n);
    else if (old && n != 0)
        OnAlloc(old, ChunkBytes(old)); // failed: the old block is still held
    --t_depth;
    return p;
}

namespace os::heap_census
{
void Start()
{
    const char* value = getenv("LO_HEAP_CENSUS");
    if (value == nullptr || *value == 0 || std::strcmp(value, "0") == 0)
    {
        LOG_NOTICE("heap census: built in, off (LO_HEAP_CENSUS=1 or a sampling period in bytes starts it)");
        return;
    }
    const long long period = std::atoll(value);
    g_period = period > 1 ? period : 32768;
    int seconds = 60;
    if (const char* s = getenv("LO_HEAP_CENSUS_SECONDS"))
        seconds = std::max(5, std::atoi(s));
    g_base = reinterpret_cast<uintptr_t>(&_start);
    mutexInit(&g_lock);
    const std::filesystem::path folder = std::filesystem::path(LO_SWITCH_DATA_ROOT) / "state" / "heap-census";
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    // The unwinder sorts the executable's frame table, allocating, at its first use: done here, under the guard,
    // so it never happens inside a census lock.
    {
        ++t_depth;
        Backtrace bt;
        CaptureStack(bt);
        --t_depth;
        LOG_NOTICE("heap census: on, one sample per {} bytes allocated, report every {} s to {} (unwinder depth here {})",
            g_period, seconds, folder.string(), bt.depth);
    }
    g_on.store(true, std::memory_order_release);
    // os::DetachThread: libnx has no pthread_detach, std::thread::detach() throws there.
    os::DetachThread(std::thread([folder, seconds] {
        os::SetCurrentThreadName("Heap Census");
        const auto start = std::chrono::steady_clock::now();
        for (;;)
        {
            std::this_thread::sleep_for(std::chrono::seconds(seconds));
            Report(folder, start);
        }
    }));
}
} // namespace os::heap_census
#endif
