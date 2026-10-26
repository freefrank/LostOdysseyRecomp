#include "ppc_config.h"
#include "ppc_context.h"
#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

// Keep the optional published-binary inspection's context offsets grounded.
static_assert(offsetof(PPCContext, r3) == 0 && offsetof(PPCContext, r10) == 0x50);
static_assert(offsetof(PPCContext, r11) == 0x58 && offsetof(PPCContext, r24) == 0xC0);
static_assert(offsetof(PPCContext, lr) == 0x100 && offsetof(PPCContext, ctr) == 0x108);
static_assert(offsetof(PPCContext, f1) == 0x150 && offsetof(PPCContext, f2) == 0x158);

extern PPCFuncMapping PPCFuncMappings[];
static unsigned missing = 0, indirect = 0, checks = 0, reads = 0;
static constexpr std::array<uint32_t, 7> labels{
    0x82F8100C, 0x82F81014, 0x82F8101C, 0x82F81024, 0x82F8102C, 0x82F81034, 0x82F81040};
static void Missing(PPCContext& ctx, uint8_t*) { ++missing; ctx.r3.u64 = 0; }
static PPCFunc* Lookup(uint32_t address)
{
    ++indirect;
    for (auto* it = PPCFuncMappings; it->guest; ++it)
        if (it->guest == address) return it->host;
    return Missing;
}
static uint32_t Load(uint32_t address)
{
    if (address < 0x82F80FF0 || address >= 0x82F8100C || (address & 3)) std::abort();
    ++reads;
    return labels[(address - 0x82F80FF0) / 4];
}
#undef PPC_LOOKUP_FUNC
#define PPC_LOOKUP_FUNC(base, address) Lookup(uint32_t(address))
#undef PPC_LOAD_U32
#define PPC_LOAD_U32(address) Load(uint32_t(address))
#include "ppc_recomp.0.cpp"
#include "ppc_func_mapping.cpp"

static void Check(bool value, const char* reason)
{
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}
int main(int argc, char** argv)
{
    int only = -1;
    if (argc == 3 && std::string_view(argv[1]) == "--case") only = std::atoi(argv[2]);
    else if (argc != 1) return 2;
    if (only >= 7) return 2;
    alignas(32) uint8_t memory[32]{};
    constexpr uint32_t masks[]{0x28, 0x18, 0x30, 8, 0x10, 0x20};
    for (uint32_t trial = 0; trial < 2000; ++trial)
        for (uint32_t index = 0; index < 7; ++index)
        {
            if (only >= 0 && index != uint32_t(only)) continue;
            PPCContext ctx{};
            const uint32_t flags = trial % 64;
            ctx.r3.u64 = flags | ((trial / 64) % 2 ? 0x1234567800000000ull : 0);
            ctx.r10.u64 = 0xBAD0;
            ctx.r11.u64 = index;
            ctx.lr = 0x82F86788;
            sub_82F80F48(ctx, memory);
            const uint64_t expected = index == 6 || !(flags & masks[index]);
            if (ctx.r3.u64 != expected)
                std::fprintf(stderr, "case=%u flags=%#x actual=%llu expected=%llu missing=%u\n",
                    index, flags, (unsigned long long)ctx.r3.u64, (unsigned long long)expected, missing);
            Check(ctx.r3.u64 == expected, "comparison switch lost a case or its shared return");
            Check(ctx.r10.u64 == expected, "comparison switch changed return bookkeeping");
        }
    Check(!missing && !indirect, "a local switch label was dispatched as a guest function");
    Check(reads == (only < 0 ? 14000u : 2000u), "fixture did not execute the production table load");
    std::printf("PASS: %u production-generated switch checks (reduced PPC fixture)\n", checks);
}
