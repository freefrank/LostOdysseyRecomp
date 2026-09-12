#include <stdafx.h>
#include <cpu/poll_wait.h>
#include <os/logger.h>

extern "C" PPC_FUNC(__imp__sub_823B62A0);
extern "C" PPC_FUNC(__imp__sub_827B6278);

namespace
{
    thread_local uint32_t waitingDevice = 0;
    thread_local uint32_t waitReports = 0;

    bool Enabled()
    {
        static const bool enabled = getenv("LO_GPU_WAIT_TRACE") != nullptr;
        return enabled;
    }
}

// Keep the original wait and failure behavior. The host-side copy lets a
// later poll distinguish a bad argument from corruption of the guest stack.
// This file already owns both PPC_FUNC symbols; poll_wait.cpp must not add
// another. Pause only while the outer wait is active and the inner poll
// returned 1 (timestamp still pending).
PPC_FUNC(sub_823B62A0)
{
    poll_wait::RunScoped(poll_wait::Kind::GpuPoll, [&] {
        if (!Enabled())
        {
            __imp__sub_823B62A0(ctx, base);
            return;
        }
        const uint32_t previous = waitingDevice;
        waitingDevice = ctx.r3.u32;
        if (waitReports++ < 8)
            LOG_INFO("gpu wait enter: device={:#x} timestamp={:#x} reason={} sp={:#x} caller={:#x} pcr={:#x}",
                waitingDevice, ctx.r4.u32, ctx.r5.u32, ctx.r1.u32, uint32_t(ctx.lr), ctx.r13.u32);
        __imp__sub_823B62A0(ctx, base);
        waitingDevice = previous;
    });
}

PPC_FUNC(sub_827B6278)
{
    if (Enabled() && waitingDevice)
    {
        const uint32_t state = ctx.r3.u32;
        const uint32_t device = PPC_LOAD_U32(state);
        if (device != waitingDevice)
        {
            LOG_ERROR("gpu wait state changed: expected={:#x} actual={:#x} state={:#x} sp={:#x} caller={:#x} pcr={:#x} words={:08x},{:08x},{:08x},{:08x},{:08x},{:08x}",
                waitingDevice, device, state, ctx.r1.u32, uint32_t(ctx.lr), ctx.r13.u32,
                device, PPC_LOAD_U32(state + 4), PPC_LOAD_U32(state + 8),
                PPC_LOAD_U32(state + 12), PPC_LOAD_U32(state + 16), PPC_LOAD_U32(state + 20));
        }
    }
    __imp__sub_827B6278(ctx, base);
    poll_wait::GpuPollResult(ctx.r3.s32);
}
