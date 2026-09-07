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
