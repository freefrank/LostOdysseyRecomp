// Issue #16: the freeze controller uses a raw material with no particle shaders.
// Keep the emitter's parameter-update material, but reject an unsupported raw
// material at the existing sprite-render compatibility gate. 822C9150 then uses
// its original engine-default material fallback; no particle is discarded.
#include <stdafx.h>
#include <os/logger.h>
#include "particle_material_compat.h"

extern "C" PPC_FUNC(__imp__sub_822C9498);

namespace
{
    uint32_t ResolveBaseMaterial(PPCContext& ctx, uint8_t* base, uint32_t material)
    {
        // Same GetMaterial virtual call as the first half of 822C9498, including
        // MaterialInstance parent resolution. Use a private register context so
        // the compatibility result and all caller-visible registers stay intact.
        ctx.r3.u64 = material;
        ctx.ctr.u64 = PPC_LOAD_U32(PPC_LOAD_U32(material) + 284);
        PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32 & ~3u);
        return ctx.r3.u32;
    }
}

PPC_FUNC(sub_822C9498)
{
    // This gate is shared by sprite, SubUV and other particle builders. Only
    // 822C9150 uses the ordinary FParticleVertexFactory checked by this policy.
    const bool ordinarySprite = uint32_t(ctx.lr) == 0x822C91CC;
    const uint32_t material = ctx.r3.u32;
    __imp__sub_822C9498(ctx, base);
    if (!ordinarySprite || !ctx.r3.u32)
        return;

    uint32_t root = material;
    if (PPC_LOAD_U32(material) != 0x8200FAB0)
    {
        PPCContext query = ctx;
        // The generated gate reserves a 96-byte frame around this call. Reserve
        // the same space, including the backchain, for a MaterialInstance query.
        // The real caller's registers, linkage area and stack pointer stay intact.
        query.r1.u64 -= 96;
        PPC_STORE_U32(query.r1.u32, ctx.r1.u32);
        root = ResolveBaseMaterial(query, base, material);
    }
    // Raw material's virtual+380 is the leaf constant-true function 82614AE8. The gate's last indirect call is that
    // slot of this same material; read the vtable rather than its CTR, which is a local of the generated gate.
    if (!root || PPC_LOAD_U32(PPC_LOAD_U32(root) + 380) != 0x82614AE8)
        return;
    const auto decision = particle_material_compat::CheckRawMaterial(root,
        [base](uint32_t address) { return PPC_LOAD_U32(address); },
        [base](uint32_t address) { return PPC_LOAD_U8(address); });
    if (decision == particle_material_compat::Decision::MissingParticleShaders)
    {
        ctx.r3.u64 = 0;
        static std::atomic<uint32_t> reports{0};
        if (reports.fetch_add(1, std::memory_order_relaxed) < 8)
            LOG_INFO("particle material fallback: raw material {:#x} has no particle shaders", root);
    }
}
