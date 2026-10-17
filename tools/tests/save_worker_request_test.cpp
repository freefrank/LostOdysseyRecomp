// Exercise the production guest hooks without a game image or kernel runtime.
#include <stdafx.h>
#include <cstdlib>
#include "../../LostOdysseyRecomp/patches/save_worker.cpp"

namespace
{
void Check(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

struct RequestFixture
{
    std::mutex criticalSection;
    std::binary_semaphore published{0};
    std::binary_semaphore attempted{0};
    std::atomic<bool> requestHoldsLock{false};
    std::atomic<uint32_t> status{0};
    bool completedBeforeBusy = false;
    unsigned entry;
    uint32_t manager;
    alignas(64) uint8_t memory[64]{};
    PPCContext input{};
    PPCContext result{};

    RequestFixture(unsigned entryIndex) : entry(entryIndex), manager(0x400000 + entryIndex * 0x10000)
    {
        // Known bytes also cover registers the request does not use. No padding
        // is indeterminate when comparing the unmodified caller context below.
        std::memset(&input, 0x5A, sizeof(input));
        input.r3.u64 = manager;
        std::memset(&result, 0xA5, sizeof(result));
        result.r3.u64 = 1; // Original request's return value must survive unlock.
    }

    void Complete()
    {
        published.acquire();
        // Force the worker's attempt between the original request-flag store
        // and its final busy store, matching the vulnerable PPC instruction order.
        // Import instrumentation chooses a deterministic handshake; try_lock
        // could spuriously fail even for the unprotected baseline.
        if (!requestHoldsLock.load())
        {
            std::lock_guard lock(criticalSection);
            completedBeforeBusy = status.load() != 2;
            status.store(3);
            attempted.release();
        }
        else
        {
            attempted.release();
            std::lock_guard lock(criticalSection);
            completedBeforeBusy = status.load() != 2;
            status.store(3);
        }
    }
};

RequestFixture* fixture = nullptr;

void OriginalRequest(unsigned entry, PPCContext& ctx, uint8_t* base)
{
    Check(entry == fixture->entry, "hook dispatches to the matching original request");
    Check(base == fixture->memory, "original request receives the guest memory base");
    Check(std::memcmp(&ctx, &fixture->input, sizeof(ctx)) == 0,
        "locking preserves all incoming PPC registers and arguments");
    fixture->published.release();
    fixture->attempted.acquire();
    fixture->status.store(2);
    std::memcpy(&ctx, &fixture->result, sizeof(ctx));
}

void CheckRequest(unsigned entry, PPCFunc* request, bool protectedRequest)
{
    RequestFixture run(entry);
    fixture = &run;
    PPCContext ctx;
    std::memcpy(&ctx, &run.input, sizeof(ctx));
    std::thread worker([&] { run.Complete(); });
    request(ctx, run.memory);
    worker.join();
    Check(std::memcmp(&ctx, &run.result, sizeof(ctx)) == 0,
        "unlocking preserves the original request's PPC registers and return value");
    if (protectedRequest)
    {
        Check(!run.completedBeforeBusy, "worker cannot complete a partially published request");
        Check(run.status.load() == 3, "completed status survives the request's final busy store");
    }
    else
    {
        Check(run.completedBeforeBusy && run.status.load() == 2,
            "unprotected instruction order loses an early worker completion");
    }
    fixture = nullptr;
}
}

PPC_FUNC(__imp__RtlEnterCriticalSection)
{
    Check(base == fixture->memory, "lock import receives the guest memory base");
    Check(ctx.r3.u32 == fixture->manager + 2908, "lock uses this save manager's worker critical section");
    fixture->criticalSection.lock();
    fixture->requestHoldsLock.store(true);
    std::memset(&ctx, 0xCC, sizeof(ctx));
}

PPC_FUNC(__imp__RtlLeaveCriticalSection)
{
    Check(base == fixture->memory, "unlock import receives the guest memory base");
    Check(ctx.r3.u32 == fixture->manager + 2908, "unlock uses the same manager after original r3 changes");
    fixture->requestHoldsLock.store(false);
    fixture->criticalSection.unlock();
    std::memset(&ctx, 0xDD, sizeof(ctx));
}

extern "C" PPC_FUNC(__imp__sub_82DCBFD8) { OriginalRequest(0, ctx, base); }
extern "C" PPC_FUNC(__imp__sub_82DCC0E8) { OriginalRequest(1, ctx, base); }
extern "C" PPC_FUNC(__imp__sub_82DCC190) { OriginalRequest(2, ctx, base); }
extern "C" PPC_FUNC(__imp__sub_82DCC3C8) { OriginalRequest(3, ctx, base); }

int main()
{
    PPCFunc* const original[] = {
        __imp__sub_82DCBFD8, __imp__sub_82DCC0E8, __imp__sub_82DCC190, __imp__sub_82DCC3C8};
    PPCFunc* const hooks[] = {sub_82DCBFD8, sub_82DCC0E8, sub_82DCC190, sub_82DCC3C8};
    for (unsigned entry = 0; entry < std::size(hooks); ++entry)
    {
        CheckRequest(entry, original[entry], false);
        CheckRequest(entry, hooks[entry], true);
    }
    std::puts("PASS: 4 unprotected requests lose completion; 4 production hooks retain completion and PPC context");
}
