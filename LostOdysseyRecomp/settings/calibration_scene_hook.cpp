#include <stdafx.h>
#include <gpu/video.h>

extern "C" PPC_FUNC(__imp__sub_82A18080);

// Field menu activation: r4 selects the menu (1 = camp), r5 opens/closes it.
// Entry precedes the transition effects (8299F400) and the SystemMenu opener
// (8285FF58 -> 8287F6C0). Waiting for its draw body or Settings is too late:
// the opening frame still draws the world but already blurs its frontbuffer.
PPC_FUNC(sub_82A18080)
{
    constexpr uint32_t SystemMenu = 0x8326DAA8;
    if (ctx.r4.u32 == 1) {
        if (ctx.r5.u32 != 1)
            gpu::video::SetCalibrationGameMenuOpen(false);
        else if (PPC_LOAD_U32(SystemMenu) == 1)
            gpu::video::SetCalibrationGameMenuOpen(true);
    }
    __imp__sub_82A18080(ctx, base);
}
