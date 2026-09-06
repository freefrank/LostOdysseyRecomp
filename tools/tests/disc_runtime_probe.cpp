// Linked only into LoDiscRuntimeTest. Runs the original request/update routines
// on the guest thread; never writes saves or synthesizes a completed transition.
#include <stdafx.h>
#include <os/logger.h>
extern "C" PPC_FUNC(__imp__sub_82320428);
PPC_FUNC(sub_82821E90);
PPC_FUNC(sub_82320428)
{
    const uint32_t manager = ctx.r3.u32;
    __imp__sub_82320428(ctx, base);
    static unsigned last = 0, tick = 0, pending = 0;
    if (pending && PPC_LOAD_U16(manager+8) == pending && !PPC_LOAD_U32(manager+4) && !PPC_LOAD_U32(manager+24))
    {
        LOG_INFO("disc runtime probe: original manager completed disc {}",pending);
        pending = 0;
    }
    if (++tick % 30 || pending) return;
    const char* path = getenv("LO_TEST_DISC_REQUEST");
    if (!path) return;
    unsigned serial = 0, disc = 0;
    std::ifstream input(path);
    if (!(input >> serial >> disc) || serial == last || disc < 1 || disc > 4) return;
    last = serial;
    auto saved = ctx;
    ctx.r3.u32 = manager; ctx.r4.u32 = disc;
    sub_82821E90(ctx,base);
    ctx = saved;
    pending = disc;
    LOG_INFO("disc runtime probe: original request disc {}",disc);
}
