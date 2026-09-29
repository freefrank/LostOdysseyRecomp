#include "gpu/native_command_stream.h"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace gpu::native_command;

static void CheckMask(uint32_t first, uint64_t mask)
{
    std::array<uint32_t, 64> source;
    for (uint32_t i = 0; i < 64; ++i) source[i] = 0x81234567u + i * 0x1020304u;
    std::array<uint32_t, kMaxRegisterWords> native;
    native.fill(0xDEADBEEF);
    assert(EncodeRegisters(first, mask, [&](uint32_t i) { return source[i]; }, native));
    assert(GuestWord(native[0]) == kRegisters);
    assert(native[1] == first && native[2] == uint32_t(mask >> 32) && native[3] == uint32_t(mask));
    for (size_t i = RegisterWords(mask); i < native.size(); ++i) assert(native[i] == 0xDEADBEEF);

    // Independent reference: form and decode the SDK type-0 packets one bit at
    // a time; no production VisitRuns/ApplyRegisters in the old-path oracle.
    std::vector<uint32_t> pm4;
    uint32_t i = 0, runs = 0;
    while (i < 64)
    {
        if (!(mask & (uint64_t(1) << (63 - i)))) { ++i; continue; }
        const uint32_t begin = i;
        while (i < 64 && (mask & (uint64_t(1) << (63 - i)))) ++i;
        pm4.push_back(GuestWord(((i - begin - 1) << 16) | (first + begin)));
        for (uint32_t n = begin; n < i; ++n) pm4.push_back(GuestWord(source[n]));
        ++runs;
    }
    assert(runs == RunCount(mask));
    assert(pm4.size() == ValueCount(mask) + runs);
    std::vector<uint32_t> reference(kRegisterCount, 0xF00DFACE), refMirror(kRegisterCount, 0xEDACABBA);
    for (size_t at = 0; at < pm4.size();)
    {
        const auto header = GuestWord(pm4[at++]);
        const uint32_t count = ((header >> 16) & 0x3FFF) + 1;
        const auto reg = header & 0x7FFF;
        for (uint32_t n = 0; n < count; ++n)
        {
            reference[reg + n] = GuestWord(pm4[at]);
            refMirror[reg + n] = pm4[at++];
        }
    }
    // Mutating the caller's source after encoding must not affect the record.
    source.fill(0);
    std::vector<uint32_t> actual(kRegisterCount, 0xF00DFACE), mirror(kRegisterCount, 0xEDACABBA);
    const auto values = std::span(native.data() + kRegisterHeaderWords, ValueCount(mask));
    assert(ApplyRegisters(first, mask, values, actual, mirror));
    assert(actual == reference && mirror == refMirror);
    assert(ApplyRegisters(first, mask, values, actual, mirror)); // IB replay.
    assert(actual == reference && mirror == refMirror);
}

int main()
{
    uint32_t masks = 0;
    for (uint32_t bit = 0; bit < 64; ++bit)
    {
        CheckMask(0x4000, uint64_t(1) << bit); ++masks;
        CheckMask(0x4800, ~uint64_t(0) << bit); ++masks;
    }
    for (const auto mask : {0xAAAAAAAAAAAAAAAAull, 0x5555555555555555ull,
        0x8000000000000001ull, 0xF00F00F00F00F00Full, 0xFFFFFFFFFFFFFFFFull})
    {
        CheckMask(0x2000, mask); ++masks;
    }
    CheckMask(kRegisterCount - 1, 0x8000000000000000ull); ++masks;
    assert(!RegisterRange(0x4000, 0));
    assert(!RegisterRange(0x0578, ~uint64_t(0))); // scratch writeback is not bypassed.
    assert(!RegisterRange(0x0A31, 0x8000000000000000ull)); // coherence side effects.
    assert(!RegisterRange(0x7F20, 0x8000000000000000ull)); // frame plan transaction.
    assert(!RegisterRange(kRegisterCount - 1, 0x4000000000000000ull));
    assert(!RegisterRange(0xFFFFFFF0, ~uint64_t(0)));
    std::array<uint32_t, kMaxRegisterWords> invalid; invalid.fill(42);
    assert(!EncodeRegisters(0x0578, ~uint64_t(0), [](uint32_t) { assert(false); return 0u; }, invalid));
    for (const auto v : invalid) assert(v == 42);
    assert(!EncodeRegisters(0x4000, ~uint64_t(0), [](uint32_t) { assert(false); return 0u; }, std::span(invalid).first(67)));
    std::vector<uint32_t> regs(kRegisterCount, 7), mirror(kRegisterCount, 9);
    assert(!ApplyRegisters(0x4000, ~uint64_t(0), std::span(invalid).first(63), regs, mirror));
    for (auto v : regs) assert(v == 7);
    for (auto v : mirror) assert(v == 9);

    assert(CanAppend(0x10000, 0x10040, 4));
    assert(!CanAppend(0x10000, 0x10010, 4));
    assert(!CanAppend(0x10001, 0x10040, 4));
    assert(!CanAppend(0xFFFFFFF0, 0xFFFFFFFC, 4));
    assert(!CanAppend(0x10000, 0x10040, 0));
    std::array<uint32_t, kDrawWords + 1> draw; draw.fill(99);
    for (uint32_t index32 : {0u, 0x800u})
    {
        assert(EncodeIndexedQuad(0x60005 | index32, 0xABCDEF02, 0x80000006, draw));
        assert(GuestWord(draw[0]) == kIndexedQuad && draw[1] == (0x60005 | index32));
        assert(draw[2] == 0xABCDEF02 && draw[3] == 0x80000006 && draw[4] == 99);
    }
    assert(!EncodeIndexedQuad(0x60085, 0, 0, draw)); // auto-index source excluded.
    assert(!EncodeIndexedQuad(0x60008, 0, 0, draw)); // rectangle primitive excluded.
    assert(!EncodeIndexedQuad(0x90005, 0, 0, draw));
    assert(!EncodeIndexedQuad(0x60005, 0, 0, std::span(draw).first(3)));
    std::puts("native command stream: reference PM4 state/mirror, source snapshot, replay, bounds and indexed-quad checks passed");
    std::printf("mask cases: %u\n", masks);
}
