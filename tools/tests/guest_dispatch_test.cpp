// Exercise the actual generated dispatch table and the indirect-only script
// handlers from issue #5. Synthetic guest objects; no game assets or GPU run.
#include <stdafx.h>
#include <kernel/memory.h>
#include <cstdio>

int main()
{
    if (!g_memory.base) return 1;
    auto* base = g_memory.base;
    unsigned checks = 0;
    auto require = [&](bool value, const char* name) {
        ++checks;
        if (!value) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
    };
    require(g_memory.FindFunction(0x82AFA388) == sub_82AFA388, "first leaf has its own generated dispatch entry");
    require(g_memory.FindFunction(0x82AFD150) == sub_82AFD150, "second leaf has its own generated dispatch entry");
    require(g_memory.FindFunction(0x82AFA388) != g_memory.FindFunction(0x82AFA2E8), "first leaf is separate from preceding function");
    require(g_memory.FindFunction(0x82AFD150) != g_memory.FindFunction(0x82AFD038), "second leaf is separate from preceding function");
    // The Android log's 0x82F81040 is a local switch case, not a function.
    // Run the actual regenerated comparison helper through all seven classes.
    require(g_memory.FindFunction(0x82F80F48) == sub_82F80F48, "comparison helper remains registered");
    PPC_STORE_U32(0x82000E50, std::bit_cast<uint32_t>(0.0f));
    PPC_STORE_U32(0x82007784, std::bit_cast<uint32_t>(1.0f));
    PPC_STORE_U32(0x82000D80, std::bit_cast<uint32_t>(1.0f));
    constexpr uint32_t labels[]{0x82F8100C,0x82F81014,0x82F8101C,0x82F81024,0x82F8102C,0x82F81034,0x82F81040};
    for (uint32_t i=0; i<7; ++i) PPC_STORE_U32(0x82F80FF0+4*i,labels[i]);
    struct CompareCase { double a,b; uint32_t mask; };
    const CompareCase cases[]{{0,0,0x28},{1,0,0x18},{0,1,0x30},{.25,0,8},
                              {.6,.6,0x10},{0,.5,0x20},{.2,.3,0},{1,.5,~0u}};
    for (const auto& c:cases)
        for (uint32_t mask=0; mask<64; ++mask)
        {
            PPCContext ctx{};
            ctx.r3.u64=mask; ctx.f1.f64=c.a; ctx.f2.f64=c.b; ctx.lr=0x82F86788;
            g_memory.FindFunction(0x82F80F48)(ctx,base);
            const uint64_t expected=c.mask==~0u ? 0 : uint64_t(!(mask&c.mask));
            require(ctx.r3.u64==expected,"comparison helper preserves every local case and its shared Boolean return");
        }
    constexpr uint32_t object=0x2000, flags=0x3000, script=0x4000, stream=0x5000, actor=0x6000;
    for (unsigned alternate=0; alternate<2; ++alternate) {
        for (unsigned command=0; command<5; ++command) {
            const uint32_t initialFlags=0x00600055u | (alternate ? 0x04000000u : 0u);
            PPC_STORE_U32(object+44,flags); PPC_STORE_U32(object+24,script);
            PPC_STORE_U32(flags+28,initialFlags);
            PPC_STORE_U32(script+36,stream); PPC_STORE_U32(script+52,0x20);
            PPC_STORE_U8(stream+0x21,alternate ? 4 : command);
            PPC_STORE_U8(stream+0x22,alternate ? command : 4);
            PPCContext ctx{}; ctx.r3.u32=object; ctx.ctr.u32=0x82AFA388;
            g_memory.FindFunction(ctx.ctr.u32)(ctx,base);
            const uint32_t expected=command==0 ? initialFlags|0x00200000u : command==1 ? initialFlags|0x00400000u :
                command==2 ? initialFlags&~0x00200000u : command==3 ? initialFlags&~0x00400000u : initialFlags;
            require(PPC_LOAD_U32(flags+28)==expected,"first leaf updates the selected flag without disturbing unrelated bits");
            require(PPC_LOAD_U32(script+52)==0x22,"first leaf advances script instead of stalling at missing function");
        }
        for (unsigned command : {0u,1u,15u,16u}) {
            for (unsigned actorPresent=0; actorPresent<2; ++actorPresent) {
                PPC_STORE_U32(flags+28,alternate ? 0x04000000u : 0u);
                PPC_STORE_U32(script+52,0x20); PPC_STORE_U32(script+4,actorPresent ? actor : 0);
                PPC_STORE_U32(actor+0x12A3C,0x00000123u);
                PPC_STORE_U8(stream+0x21,alternate ? 16 : command);
                PPC_STORE_U8(stream+0x22,alternate ? command : 16);
                PPCContext ctx{}; ctx.r3.u32=object; ctx.ctr.u32=0x82AFD150;
                g_memory.FindFunction(ctx.ctr.u32)(ctx,base);
                require(PPC_LOAD_U32(actor+0x12A3C)==((command==0 && actorPresent) ? 0x80000123u : 0x123u),
                    "second leaf handles active, empty and unrelated opcodes");
                require(PPC_LOAD_U32(script+52)==0x22,"second leaf advances script on all branches");
            }
        }
    }
    std::printf("PASS: %u actual generated dispatch and script handler checks\n",checks);
    return 0;
}
