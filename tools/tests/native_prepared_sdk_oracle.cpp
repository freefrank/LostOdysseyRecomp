#include <stdafx.h>
#include "gpu/native_mesh.h"
#include <iostream>
#include <random>

extern "C" PPC_FUNC(__imp__sub_823C6860);
extern "C" PPC_FUNC(__imp__sub_827B56B0);
// sub_823C6860/sub_827B56B0 use the canonical shared-header declarations.
extern uint32_t oracle_preparation_model, oracle_preparation_calls;
namespace gpu::native_frontend {
void ExecuteOriginalWithPreparedTail(PPCContext&, uint8_t*, bool);
bool SubmitPrepared(uint32_t, uint32_t, bool, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t);
}
using namespace gpu::native_frontend;
constexpr uint32_t kDevice = 0x100000, kStream = 0x200000, kResource = 0x105000;
static void Check(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
static uint32_t Get(const uint8_t* base, uint32_t address) {
    uint32_t value; std::memcpy(&value, base + address, 4);
    return gpu::native_command::GuestWord(value);
}
static void Put(uint8_t* base, uint32_t address, uint32_t value) {
    value = gpu::native_command::GuestWord(value); std::memcpy(base + address, &value, 4);
}
static void Put64(uint8_t* base, uint32_t address, uint64_t value) {
    Put(base, address, uint32_t(value >> 32)); Put(base, address + 4, uint32_t(value));
}
struct Result {
    std::vector<uint32_t> registers = std::vector<uint32_t>(0x5003, 0xdeadbeef);
    std::vector<uint32_t> events;
    uint32_t draws = 0, preparedCommands = 0;
    void Write(uint32_t reg, uint32_t value) {
        Check(reg < registers.size(), "register bound");
        registers[reg] = value;
        if (reg == 0x2007 || (reg >= 0xa2f && reg <= 0xa31) ||
            (reg >= 0x2388 && reg < 0x23a0) || (reg >= 0x4900 && reg < 0x4928) ||
            reg == 0x2185 || reg == 0x2186) events.insert(events.end(), {reg, value});
        if (reg == 0xa31) registers[reg] |= 0x80000000u;
    }
    void Wait(uint32_t info, uint32_t reg, uint32_t ref, uint32_t mask, uint32_t interval) {
        Check(info == 3 && reg == 0xa31 && ref == 0 && mask == 0x80000000u && interval == 8,
            "unexpected wait outside bounded contract");
        events.insert(events.end(), {0xffffffff, info, reg, ref, mask, interval});
        registers[reg] &= ~0x80000000u;
    }
    void Draw(const MeshDraw& draw, bool predicate) {
        if (!predicate) return;
        registers[0x21fc] = draw.initiator;
        if (draw.Indexed()) {
            registers[0x21fa] = draw.dmaBase; registers[0x21fb] = draw.dmaSize;
        }
        ++draws;
    }
};
static Result Execute(const uint8_t* base, uint32_t end, bool predicate) {
    Result result;
    for (uint32_t cursor = kStream + 4; cursor <= end;) {
        const auto header = Get(base, cursor); cursor += 4;
        if (header == kPreparedMesh || header == kMesh) {
            const auto* body = reinterpret_cast<const uint32_t*>(base + cursor);
            Check(cursor + 4 <= end + 4 && body[0] >= kHeaderWords &&
                uint64_t(cursor) + (body[0] - 1) * 4ull <= uint64_t(end) + 4, "native record bound");
            DecodedMesh mesh;
            Check(Decode(std::span(body, body[0] - 1), mesh), "native payload");
            Check(mesh.deltas[0].mask == 0 && mesh.deltas[1].mask == 0,
                "prepared tail must not replay prefix ALU banks");
            ApplyDeltas(mesh, [&](uint32_t reg, uint32_t value) { result.Write(reg, value); },
                [&](uint32_t control) {
                    result.Write(0x2007, control); result.Write(0xa31, 0x10000);
                    result.Write(0xa2f, 0); result.Write(0xa30, 4096);
                    result.Wait(3, 0xa31, 0, 0x80000000u, 8);
                });
            result.Draw(mesh.draw, predicate);
            result.preparedCommands += header == kPreparedMesh;
            cursor += (body[0] - 1) * 4;
            continue;
        }
        const auto type = header >> 30;
        if (type == 2) continue;
        const auto count = ((header >> 16) & 0x3fff) + 1;
        Check(uint64_t(cursor) + count * 4ull <= uint64_t(end) + 4, "PM4 record bound");
        if (type == 0) {
            const auto first = header & 0x7fff;
            for (uint32_t i = 0; i < count; ++i)
                result.Write(first + ((header & 0x8000) ? 0 : i), Get(base, cursor + i * 4));
        } else if (type == 3) {
            const auto opcode = (header >> 8) & 0x7f;
            if (opcode == 0x3c) {
                Check(count == 5 && !(header & 1), "unpredicated stream wait");
                result.Wait(Get(base, cursor), Get(base, cursor + 4), Get(base, cursor + 8),
                    Get(base, cursor + 12), Get(base, cursor + 16));
            } else {
                Check(opcode == 0x22 && (count == 2 || count == 4), "unexpected SDK operation");
                MeshDraw draw{Get(base, cursor + 4), 0, 0, 0};
                if (draw.Indexed()) { draw.dmaBase = Get(base, cursor + 8); draw.dmaSize = Get(base, cursor + 12); }
                result.Draw(draw, predicate || !(header & 1));
            }
        } else Check(false, "unexpected PM4 type");
        cursor += count * 4;
    }
    return result;
}
static void CheckNonvolatile(const PPCContext& a, const PPCContext& b) {
    Check(a.r1.u64 == b.r1.u64 && a.lr == b.lr, "epilogue SP/LR");
#define CHECK_REG(n) Check(a.r##n.u64 == b.r##n.u64, "nonvolatile r" #n)
    CHECK_REG(14); CHECK_REG(15); CHECK_REG(16); CHECK_REG(17); CHECK_REG(18); CHECK_REG(19);
    CHECK_REG(20); CHECK_REG(21); CHECK_REG(22); CHECK_REG(23); CHECK_REG(24); CHECK_REG(25);
    CHECK_REG(26); CHECK_REG(27); CHECK_REG(28); CHECK_REG(29); CHECK_REG(30); CHECK_REG(31);
#undef CHECK_REG
}
int main() {
    Check(meshEnabled && preparedTailEnabled, "enable mesh and prepared-tail modes");
    auto* base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x100000000ull, MEM_RESERVE, PAGE_READWRITE));
    Check(base != nullptr, "reserve synthetic guest memory");
    Check(VirtualAlloc(base, 0x400000, MEM_COMMIT, PAGE_READWRITE) != nullptr, "commit synthetic state");
    Check(VirtualAlloc(base + 0x83302000, 0x1000, MEM_COMMIT, PAGE_READWRITE) != nullptr, "commit identity");
    Put(base, 0x83302a38, kDevice);
    std::mt19937_64 random(0x69145764);
    uint32_t accepted = 0, retained = 0;
    for (uint32_t trial = 0; trial < 96; ++trial) {
        std::memset(base + kDevice, 0, 0x6000);
        for (auto layout : kGroups)
            for (uint32_t i = 0; i < layout.fields * layout.wordsPerField; ++i)
                Put(base, kDevice + layout.guestOffset + i * 4, uint32_t(random()));
        oracle_preparation_model = trial / 32;
        const bool indexed = (trial & 1) != 0;
        DirtyState dirty{random(), random(), (random() & ~(15ull << 17)) | 5u, random(), random(), 0};
        if (oracle_preparation_model) { dirty.main |= 1ull << 17; dirty.derived = 4; }
        Put64(base, kDevice, dirty.vertex); Put64(base, kDevice + 8, dirty.pixel);
        Put64(base, kDevice + 16, dirty.main); Put64(base, kDevice + 24, dirty.fetchRaster);
        Put64(base, kDevice + 32, dirty.misc); Put64(base, kDevice + 40, dirty.derived);
        Put(base, kDevice + 48, kStream); Put(base, kDevice + 52, 0x3ffff0); Put(base, kDevice + 56, 0x3ff000);
        Put(base, kDevice + 12428, kResource);
        Put(base, kResource, ((trial & 2) ? 0x80000000u : 0) | ((trial & 3) << 29));
        Put(base, kResource + 24, 0xa0080040u + (trial % 3) * 2);
        PPCContext input{}; input.r1.u64 = 0x80000; input.r3.u64 = kDevice;
        input.r4.u64 = trial % 6 + 1; input.r5.u64 = indexed ? 123 : 37;
        input.r6.u64 = indexed ? 3 : trial + 6; input.r7.u64 = trial + 6; input.lr = 0x12345678;
        input.r14.u64 = 0x123456789abcdef0; input.r31.u64 = 0xfedcba9876543210;
        std::array<uint8_t, 0x6000> original, reference;
        std::memcpy(original.data(), base + kDevice, original.size());
        auto context = input; oracle_preparation_calls = 0;
        if (indexed) __imp__sub_823C6860(context, base); else __imp__sub_827B56B0(context, base);
        CheckNonvolatile(context, input);
        const auto calls = oracle_preparation_calls;
        const auto end = Get(base, kDevice + 48);
        const auto pass = Execute(base, end, true), skip = Execute(base, end, false);
        Check(pass.preparedCommands == 0, "unscoped direct implementation must stay legacy");
        std::memcpy(reference.data(), base + kDevice, reference.size());
        std::memcpy(base + kDevice, original.data(), original.size());
        context = input; oracle_preparation_calls = 0;
        if (!oracle_preparation_model) ExecuteOriginalWithPreparedTail(context, base, indexed);
        else if (indexed) sub_823C6860(context, base); else sub_827B56B0(context, base);
        CheckNonvolatile(context, input);
        Check(oracle_preparation_calls == calls, "preparation repeated or skipped");
        const auto nativeEnd = Get(base, kDevice + 48);
        const auto nativePass = Execute(base, nativeEnd, true), nativeSkip = Execute(base, nativeEnd, false);
        Check(nativePass.registers == pass.registers && nativeSkip.registers == skip.registers, "state equivalence");
        Check(nativePass.events == pass.events && nativeSkip.events == skip.events, "prefix/wait/bool ordering");
        Check(nativePass.draws == pass.draws && nativeSkip.draws == skip.draws, "predicate equivalence");
        const uint32_t expected = oracle_preparation_model == 2 ? 0u : 1u;
        Check(nativePass.preparedCommands == expected, "tail acceptance or uncleared-shader rejection");
        accepted += expected; retained += !expected;
        // Cursor length changes; all other guest device bytes, including newly
        // dirtied future ALU flags, must match the original continuation.
        Put(base, kDevice + 48, end);
        Check(std::memcmp(reference.data(), base + kDevice, reference.size()) == 0, "guest writeback differs");
        Check(!SubmitPrepared(0x80000 - (indexed ? 240 : 208), kDevice, indexed, 4, 0, 6, 1, 0, 0),
            "tail authorization survived original-call return");
    }
    VirtualFree(base, 0, MEM_RELEASE);
    std::cout << "PREPARED_ORACLE_PASS cases=96 accepted=" << accepted << " retained=" << retained
              << " canonical_prefix_tail=true preparation_helpers=opaque_models predicates=pass,skip"
              << " future_alu_flags=preserved epilogue=verified game_or_gpu_validation=false\n";
}
