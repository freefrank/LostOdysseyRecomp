#include <stdafx.h>
#include <os/logger.h>

extern "C" PPC_FUNC(__imp__sub_82BE1C00);
extern "C" PPC_FUNC(__imp__sub_828212E0);

PPC_FUNC(sub_82BE1C00)
{
    LOG_ERROR("guest disc failure caller={:#x}", uint32_t(ctx.lr));
    __imp__sub_82BE1C00(ctx, base);
}

// Record the resource lookup failure before its asynchronous fatal UI thread
// loses the original caller and resource name. This does not bypass the error.
PPC_FUNC(sub_828212E0)
{
    if (uint32_t(ctx.lr) == 0x8237d308)
    {
        const uint32_t data = PPC_LOAD_U32(ctx.r31.u32 + 4);
        const uint32_t count = PPC_LOAD_U32(ctx.r31.u32 + 8);
        std::string name;
        if (data && count < 1024)
            for (uint32_t i = 0; i < count; ++i)
            {
                const uint16_t c = PPC_LOAD_U16(data + i * 2);
                if (!c) break;
                name += c < 128 ? char(c) : '?';
            }
        LOG_ERROR("fatal resource lookup: '{}' object={:#x}", name, ctx.r31.u32);
    }
    __imp__sub_828212E0(ctx, base);
}
