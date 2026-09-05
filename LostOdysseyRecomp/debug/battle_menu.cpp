#include <stdafx.h>
#include <os/logger.h>
#include "battle_menu.h"

extern std::atomic<uint32_t> g_presentedSwaps;
extern "C" PPC_FUNC(__imp__sub_8238A640);
extern "C" PPC_FUNC(__imp__sub_82AAA7C8);
extern "C" PPC_FUNC(__imp__sub_82AC6D88);

namespace
{
    enum class State { Unavailable, Ready, Pending, Applied, Cancelled };
    std::atomic<State> state{State::Unavailable};
    std::atomic<uint64_t> lastBattleTick{0};
    uint64_t Now()
    {
        return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
}

void debug_menu::RequestVictory()
{
    if (Now() - lastBattleTick.load() > 1000) return;
    State expected = State::Ready;
    if (state.compare_exchange_strong(expected, State::Pending))
        LOG_INFO("debug menu: victory requested for current battle");
}

void debug_menu::CancelVictory()
{
    State expected = State::Pending;
    state.compare_exchange_strong(expected, State::Cancelled);
}

const wchar_t* debug_menu::Status()
{
    if (Now() - lastBattleTick.load() > 1000) return L"当前没有可跳过的战斗 / No active battle";
    switch (state.load())
    {
    case State::Ready: return L"战斗中：可请求判胜 / Battle active";
    case State::Pending: return L"等待战斗空闲或回合边界… / Waiting for safe phase";
    case State::Applied: return L"已进入胜利收尾 / Victory requested in game";
    case State::Cancelled: return L"请求已取消 / Cancelled";
    default: return L"当前没有可跳过的战斗 / No active battle";
    }
}

// Battle core tick, called on the guest game thread. Keep UI requests atomic;
// never mutate guest state from the video/window thread.
PPC_FUNC(sub_8238A640)
{
    const uint32_t core = ctx.r3.u32;
    const uint32_t phase = PPC_LOAD_U32(core + 0x38);
    static uint32_t previousPhase = ~0u;
    static uint64_t previousTick = 0;
    const uint64_t now = Now();
    if (core == 0x832ca0e8 && PPC_LOAD_U32(core + 0x15cc) == 2 && phase >= 1 && phase <= 10)
    {
        if (previousTick == 0 || now - previousTick > 1000 || previousPhase == 0 ||
            (phase < previousPhase && previousPhase >= 12))
            state = State::Ready;
        if (state == State::Cancelled) state = State::Ready;
        lastBattleTick = now;
        // Deterministic one-shot input for local integration tests, disabled by default.
        static const uint32_t testAt = getenv("LO_DEBUG_WIN_AT") ? strtoul(getenv("LO_DEBUG_WIN_AT"), nullptr, 10) : 0;
        static bool testSent = false;
        if (testAt && !testSent && g_presentedSwaps >= testAt)
        {
            testSent = true;
            debug_menu::RequestVictory();
        }
        // Command setup/selection has no executing attack. Otherwise wait until
        // the normal end-of-action/end-of-turn boundary rather than cutting a cinematic.
        if (state == State::Pending && (phase == 2 || phase == 3 || phase == 6 || phase == 10))
        {
            PPCContext saved = ctx;
            const uint32_t resultManager = PPC_LOAD_U32(0x83291dc0);
            if (!resultManager) { state = State::Unavailable; __imp__sub_8238A640(ctx, base); return; }
            ctx.r4.u64 = 11;
            ctx.r5.u64 = 1;
            __imp__sub_82AAA7C8(ctx, base);
            // The ordinary victory branch enters phase 11 and calls this result
            // initializer. Phase 12 is defeat; forcing a phase alone is incomplete.
            ctx.r3.u64 = resultManager;
            __imp__sub_82AC6D88(ctx, base);
            ctx = saved;
            state = State::Applied;
            LOG_INFO("debug menu: entered native victory phase from {} at swap {}", phase, g_presentedSwaps.load());
        }
    }
    else if (state == State::Pending)
        state = State::Unavailable;
    if (phase != previousPhase)
    {
        LOG_INFO("battle debug: core {:#x} phase {} -> {} scene {}", core, previousPhase, phase, PPC_LOAD_U32(core + 0x15cc));
        previousPhase = phase;
    }
    previousTick = now;
    __imp__sub_8238A640(ctx, base);
}
