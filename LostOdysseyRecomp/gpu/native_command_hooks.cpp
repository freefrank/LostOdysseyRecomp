#include <stdafx.h>
#include "gpu/native_command_stream.h"
#include "kernel/memory.h"
#include "os/logger.h"
#include <array>
#include <cstdlib>
#include <cstring>
#include <string_view>

extern "C" PPC_FUNC(__imp__sub_823C1BD8);

namespace gpu::native_command
{
const Mode mode = [] {
    const char* value = std::getenv("LO_NATIVE_COMMANDS");
    if (!value) return Mode::Off;
    const std::string_view text(value);
    if (text == "1" || text == "all") return Mode::All;
    if (text == "registers") return Mode::Registers;
    return Mode::Off;
}();
const char* ModeName()
{
    switch (mode) {
    case Mode::All: return "all";
    case Mode::Registers: return "registers";
    default: return "off";
    }
}

namespace
{
uint32_t Load(const uint8_t* base, uint32_t address)
{
    uint32_t word;
    std::memcpy(&word, base + address, sizeof(word));
    return GuestWord(word);
}
void Store(uint8_t* base, uint32_t address, uint32_t value)
{
    const auto word = GuestWord(value);
    std::memcpy(base + address, &word, sizeof(word));
}
bool Range(uint32_t address, uint32_t bytes)
{
    return address >= 0x10000 && uint64_t(address) + bytes <= 0x100000000ull;
}
bool LiveDevice(const uint8_t* base, uint32_t device)
{
    // Only the current title D3D device and its ordinary live command stream.
    // Preserve the SDK's special/recorded submission paths unchanged.
    return base && Range(device, 0x33A4) && Load(base, 0x83302A38) == device &&
        !(base[device + 0x2ABC] & 0x81) && Load(base, device + 0x33A0) == 0;
}
}
}

// SDK dirty-register writer. Fast path replaces its PM4 header construction,
// per-word PPC stores and CP type-0 decode. The source is copied by value into
// the same owned IB before the usual device cursor is published.
PPC_FUNC(sub_823C1BD8)
{
    using namespace gpu::native_command;
    const uint32_t device = ctx.r3.u32;
    const uint32_t first = ctx.r5.u32;
    const uint64_t mask = ctx.r4.u64;
    if (mode != Mode::Off && RegisterRange(first, mask) && LiveDevice(base, device))
    {
        const auto cursor = Load(base, device + 0x30);
        const auto limit = Load(base, device + 0x34);
        const auto words = RegisterWords(mask);
        const auto source = ctx.r6.u32;
        // Also avoid a rollover that the original sparse PM4 run would need.
        const auto required = std::max(words, ValueCount(mask) + RunCount(mask));
        if (CanAppend(cursor, limit, required) && Range(source, RegisterSpan(mask) * 4))
        {
            std::array<uint32_t, kMaxRegisterWords> encoded;
            EncodeRegisters(first, mask, [&](uint32_t index) {
                return Load(base, source + index * 4);
            }, encoded);
            std::memcpy(base + cursor + 4, encoded.data(), words * 4);
            const auto end = cursor + words * 4;
            Store(base, device + 0x30, end);
            // Preserve the writer's live outputs; nonvolatile registers, stack
            // and LR are untouched. The no-rollover SDK path leaves r3=device.
            ctx.r4.u64 = end;
            ctx.r5.u64 = first + RegisterSpan(mask);
            return;
        }
    }
    __imp__sub_823C1BD8(ctx, base);
}

// At 0x823CD44C in sub_823CD050, after state/geometry allocation and the
// special-submission branch, before PM4 DRAW_INDX emission. True skips to
// 0x823CD52C, keeping the original resource copies and deferred cursor publish.
// Scope: six-index list / four-index fan UP draws. Host draw counts are
// post-expansion; matching geometry alone does not identify a material.
bool NativeIndexedQuad(PPCRegister& device, PPCRegister& cursor,
    PPCRegister& initiator, PPCRegister& dmaBase, PPCRegister& dmaSize,
    PPCRegister& end)
{
    using namespace gpu::native_command;
    auto* base = g_memory.base;
    static thread_local unsigned traces = 0;
    if (mode == Mode::All && traces < 4) {
        ++traces;
        LOG_INFO("native UP producer: initiator={:#x} primitive={} count={} source={} live={}",
            initiator.u32, initiator.u32 & 63, initiator.u32 >> 16,
            (initiator.u32 >> 6) & 3, LiveDevice(base, device.u32));
    }
    if (mode != Mode::All || !IndexedQuad(initiator.u32) ||
        !LiveDevice(base, device.u32) ||
        !CanAppend(cursor.u32, Load(base, device.u32 + 0x34), kDrawWords)) return false;
    std::array<uint32_t, kDrawWords> encoded;
    EncodeIndexedQuad(initiator.u32, dmaBase.u32, dmaSize.u32, encoded);
    const uint32_t writtenEnd = cursor.u32 + kDrawWords * 4;
    std::memcpy(base + cursor.u32 + 4, encoded.data(), encoded.size() * 4);
    cursor.u64 = writtenEnd - 4;
    end.u64 = writtenEnd;
    return true;
}

// Ordinary DrawPrimitive producer, after state flush and its special-stream
// branch. The original downstream publication and count bookkeeping remain.
bool NativeAutoFan(PPCRegister& device, PPCRegister& cursor,
    PPCRegister& primitive, PPCRegister& count, PPCRegister& end)
{
    using namespace gpu::native_command;
    auto* base = g_memory.base;
    if (mode != Mode::All || (primitive.u32 & 63) != 5 || count.u32 != 4 ||
        !LiveDevice(base, device.u32) ||
        !CanAppend(cursor.u32, Load(base, device.u32 + 0x34), 3)) return false;
    const uint32_t encoded[kAutoFanWords] = {GuestWord(kAutoFan), 0x00040085u};
    const auto writtenEnd = cursor.u32 + sizeof(encoded);
    std::memcpy(base + cursor.u32 + 4, encoded, sizeof(encoded));
    cursor.u64 = writtenEnd - 4;
    end.u64 = writtenEnd;
    return true;
}
