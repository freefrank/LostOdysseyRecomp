"""Compile production packet parsing and interrupt loops with fake guest memory/functions.

This checks malformed command handling and callback publication without claiming
to run a game or exercise a graphics backend.
"""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def between(text, begin, end):
    if text.count(begin) != 1 or text.count(end) != 1:
        raise RuntimeError("Production extraction boundary changed")
    return text[text.index(begin):text.index(end)]


PRELUDE = r'''
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#define private public
#include "gpu/command_processor.h"
#undef private
#define LOG_ERROR(...) ((void)0)
#define LOG_INFO(...) ((void)0)
#define LOG_WARNING(...) ((void)0)
#define LOG_VERBOSE(...) ((void)0)
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static uint32_t ByteSwap(uint32_t n) { return __builtin_bswap32(n); }
static uint16_t ByteSwap(uint16_t n) { return __builtin_bswap16(n); }
template<class T> using be = T;
struct Register { uint64_t u64 = 0; };
struct FakePpc { Register r3, r4; };
static std::atomic<uint32_t> mismatches{0}, vblankCalls{0}, interruptCalls{0};
static void Callback1(FakePpc& ctx, uint8_t*) {
    if (ctx.r4.u64 != 15) ++mismatches;
    if (ctx.r3.u64) ++interruptCalls; else ++vblankCalls;
}
static void Callback2(FakePpc& ctx, uint8_t*) {
    if (ctx.r4.u64 != 25) ++mismatches;
    if (ctx.r3.u64) ++interruptCalls; else ++vblankCalls;
}
struct GuestThreadContext {
    FakePpc ppcContext;
    explicit GuestThreadContext(uint32_t) {}
    void SetCpuNumber(uint32_t) {}
};
namespace os { void SetCurrentThreadName(const char*) {} }
void DumpGuestThreadStates() {}
static std::array<uint32_t, 0x10000> memory{};
static std::atomic<uint32_t> translations{0};
struct FakeMemory {
    uint8_t* base = reinterpret_cast<uint8_t*>(memory.data());
    uint8_t* Translate(uint32_t address) {
        ++translations;
        const uint32_t physical = address & 0x1fffffff;
        Check(physical < sizeof(memory), "parser accessed an unchecked physical extent");
        return base + physical;
    }
    auto FindFunction(uint32_t id) {
        Check(id == 1 || id == 2, "unexpected callback address");
        return id == 1 ? &Callback1 : &Callback2;
    }
} g_memory;
namespace gpu {
static std::atomic<uint32_t> g_swapCount{0}, g_completedSwaps{0}, g_lastOpcode{0}, g_traceBudget{1};
static std::atomic<const char*> g_workerStage{"test"};
static constexpr bool g_capturePacketHistory = true;
struct PacketRecord { uint32_t header, offset, d0, d1, d2; bool ring; };
static PacketRecord g_history[64]{};
static uint32_t g_historyPos = 0;
static void DumpHistory(const char*) {}
namespace frame_rate { constexpr uint32_t kGuestRefreshHz = 60; }
struct FramePacer {
    template<class Time> auto Schedule(Time now, uint32_t) { return now; }
};
static constexpr bool g_gpuStats = false;
static uint32_t GpuSwap(uint32_t value, uint32_t endian) { return endian == 2 ? ByteSwap(value) : value; }
static struct { uint32_t constantWrites = 0; } g_frame;
static std::vector<std::pair<uint32_t, uint32_t>> writes;
static std::vector<uint32_t> shaderWords;
static void CaptureShader(uint32_t, const uint32_t* words, uint32_t count, bool) {
    // A wrapped inline shader uses an owned temporary image instead of guest
    // storage; its bounded lifetime is checked by ASan during the copy below.
    shaderWords.clear();
    for (uint32_t i = 0; i < count; ++i) shaderWords.push_back(ByteSwap(words[i]));
}
void CommandProcessor::WriteRegister(uint32_t index, uint32_t value) { writes.emplace_back(index, value); }
uint32_t CommandProcessor::ReadRegister(uint32_t) { return 0; }
void CommandProcessor::WriteRegisterFast(uint32_t index, uint32_t value) { WriteRegister(index, value); }
bool CommandProcessor::WritePlainRun(uint32_t, const uint32_t*, uint32_t) { return false; }
'''

TAIL = r'''
}
static constexpr uint32_t Type2 = 0x80000000;
static uint32_t Type3(uint32_t opcode, uint32_t count = 2) {
    return 0xc0000000 | ((count - 1) << 16) | (opcode << 8);
}
static void Words(uint32_t address, std::initializer_list<uint32_t> words) {
    for (uint32_t value : words) { memory.at(address / 4) = ByteSwap(value); address += 4; }
}
static bool RunIb(gpu::CommandProcessor& cp, uint32_t address, uint32_t count) {
    cp.m_indirectGuard.Reset();
    return cp.ExecuteIndirectBuffer(address, count);
}
int main() try {
    using Guard = gpu::IndirectBufferGuard;
    gpu::CommandProcessor cp;
    cp.m_running = true;
    // Empty and ordinary lists, followed by sequential reuse of the same child.
    Check(RunIb(cp, 0, 0), "empty IB rejected");
    Words(0x100, {Type2});
    Check(RunIb(cp, 0x100, 1), "ordinary IB rejected");
    Words(0x200, {Type3(0x3f), 0x100, 1, Type3(0x37), 0x100, 1});
    Check(RunIb(cp, 0x200, 6), "sequential child reuse rejected");
    // Self-cycles, two-list cycles, and physical aliases must terminate.
    Words(0x300, {Type3(0x3f), 0x300, 3});
    Check(!RunIb(cp, 0x300, 3), "self-cycle accepted");
    Words(0x300, {Type3(0x3f), 0x400, 3});
    Words(0x400, {Type3(0x3f), 0xa0000300, 3});
    Check(!RunIb(cp, 0x300, 3), "two-list/alias cycle accepted");
    Check(RunIb(cp, 0x100, 1), "failed path was not unwound");
    // Maximum accepted nesting, then one level over the limit.
    for (uint32_t i = 0; i < Guard::MaxDepth - 1; ++i)
        Words(0x1000 + i * 16, {Type3(0x3f), 0x1000 + (i + 1) * 16, 3});
    Words(0x1000 + (Guard::MaxDepth - 1) * 16, {Type2, Type2, Type2});
    Check(RunIb(cp, 0x1000, 3), "maximum permitted nesting rejected");
    Words(0x1000 + (Guard::MaxDepth - 1) * 16,
        {Type3(0x3f), 0x1000 + Guard::MaxDepth * 16, 1});
    Words(0x1000 + Guard::MaxDepth * 16, {Type2});
    Check(!RunIb(cp, 0x1000, 3), "excess nesting accepted");
    // Reject extents before translation, including overflowing dword lengths.
    for (const auto [address, count] : std::array<std::pair<uint32_t, uint32_t>, 3>{{
        {0x1ffffffc, 2}, {0x101, 1}, {0x100, 0xffffffff}}}) {
        const auto before = translations.load();
        Check(!RunIb(cp, address, count), "invalid extent accepted");
        Check(before == translations, "invalid extent translated before validation");
    }
    Check(Guard::ValidRange(0x1ffffffc, 1), "last physical dword rejected");
    // Truncated Type0/Type1/IB and an IB with only one operand are errors.
    for (uint32_t header : {0x00010005u, 0x40000000u, Type3(0x3f)}) {
        Words(0x500, {header, 0x100});
        Check(!RunIb(cp, 0x500, 2), "truncated packet accepted");
    }
    Words(0x500, {Type3(0x3f, 1), 0x100});
    Check(!RunIb(cp, 0x500, 2), "one-operand IB accepted");
    // Check every fixed minimum even when the encoded packet itself fits.
    for (const auto [opcode, minimum] : std::array<std::pair<uint32_t, uint32_t>, 15>{{
        {0x3f, 2}, {0x37, 2}, {0x3e, 2}, {0x5a, 2}, {0x50, 2}, {0x51, 2},
        {0x27, 2}, {0x2b, 2}, {0x22, 2}, {0x21, 3}, {0x58, 3}, {0x2f, 3},
        {0x64, 4}, {0x3c, 5}, {0x45, 6}}}) {
        Check(gpu::Type3MinimumOperands(opcode) == minimum, "opcode minimum changed unexpectedly");
        for (uint32_t count = 1; count < minimum; ++count) {
            Words(0x500, {Type3(opcode, count), 0, 0, 0, 0, 0});
            Check(!RunIb(cp, 0x500, count + 1), "undersized opcode payload accepted");
        }
    }
    // Variable inline shader sizes must fit their own packet, regardless of
    // words remaining in the surrounding command list.
    Words(0x500, {Type3(0x2b, 3), 0, 2, 0x11223344, 0x55667788});
    gpu::shaderWords.clear();
    Check(!RunIb(cp, 0x500, 5) && gpu::shaderWords.empty(), "inline shader crossed its payload");
    Words(0x500, {Type3(0x27, 2), 0x1ffffffc, 2});
    const auto beforeShader = translations.load();
    Check(!RunIb(cp, 0x500, 3), "external shader crossed physical boundary");
    Check(translations == beforeShader + 1, "invalid external shader was translated");
    // An ALU load's length describes external memory, not inline operands.
    Words(0xa00, {1, 2, 3, 4});
    Words(0x500, {Type3(0x2f, 4), 0xa00, 0, 4, 0x00010005});
    gpu::writes.clear();
    Check(RunIb(cp, 0x500, 5) && gpu::writes.size() == 4,
        "valid external ALU length/padding treated as inline operands");
    Words(0x500, {Type3(0x2f, 3), 0x1ffffffc, 0, 2});
    Check(!RunIb(cp, 0x500, 4), "external ALU source crossed physical boundary");
    Words(0x500, {Type3(0x3d, 3), 0x1ffffffc, 1, 2});
    Check(!RunIb(cp, 0x500, 4), "MEM_WRITE crossed physical boundary");
    Words(0x500, {Type3(0x5a, 2), 0, 0x1ffffffc});
    Check(!RunIb(cp, 0x500, 3), "EVENT_WRITE_EXT crossed physical boundary");
    // Indexed draws require both DMA operands; auto-index draws still fit the
    // short legitimate variants used by the command stream.
    Words(0x500, {Type3(0x36, 1), 0});
    Check(!RunIb(cp, 0x500, 2), "indexed draw without DMA operands accepted");
    Words(0x500, {Type3(0x22, 2), 0, 0});
    Check(!RunIb(cp, 0x500, 3), "indexed draw with missing DMA operands accepted");
    Words(0x500, {Type3(0x36, 1), 2u << 6});
    Check(RunIb(cp, 0x500, 2), "short auto-index draw rejected");
    Words(0x500, {Type3(0x36, 3), 0, 0xa00, 4});
    Check(RunIb(cp, 0x500, 4), "valid indexed draw rejected");
    // Reserved/trailing payload words remain inside their declared packet,
    // even if they resemble a register-write header plus value.
    Words(0x500, {Type3(0x60, 3), 5, 0x00000007, 99, 0x00000008, 111});
    gpu::writes.clear();
    Check(RunIb(cp, 0x500, 6) && uint32_t(cp.m_binMask) == 5 &&
        gpu::writes == std::vector<std::pair<uint32_t, uint32_t>>{{8, 111}},
        "SET_BIN_MASK_LO trailing operands executed as packets");
    Words(0x500, {Type3(0x21, 5), 5, 0xffffffff, 42, 0x00000007, 99, 0x00000008, 111});
    gpu::writes.clear();
    Check(RunIb(cp, 0x500, 8) &&
        gpu::writes == std::vector<std::pair<uint32_t, uint32_t>>{{5, 42}, {8, 111}},
        "REG_RMW trailing operands executed as packets");
    // A malformed nested packet must stop its parent before following writes.
    Words(0x500, {0x00010005u, 42});
    Words(0x600, {Type3(0x3f), 0x500, 2, 0x00000005, 99});
    gpu::writes.clear();
    Check(!RunIb(cp, 0x600, 5) && gpu::writes.empty(), "nested error did not propagate");
    cp.m_primaryBufferPhysical = 0x600;
    cp.m_primaryBufferSize = 32;
    cp.ExecutePrimaryBuffer(0, 5);
    Check(gpu::writes.empty(), "primary parser continued after malformed child");
    // Primary ring wrap remains valid, and tracing peeks never wrap linear IBs.
    cp.m_primaryBufferPhysical = 0x800;
    cp.m_primaryBufferSize = 16;
    Words(0x80c, {Type3(0x3f)});
    Words(0x800, {0x100, 1});
    Check(cp.ExecutePrimaryBuffer(3, 2) == 2, "wrapped ring did not finish");
    cp.m_primaryBufferPhysical = 0x800;
    cp.m_primaryBufferSize = 32;
    Words(0x81c, {Type3(0x60, 3)});
    Words(0x800, {6, 0x00000007, 99, 0x00000008, 112});
    gpu::writes.clear();
    Check(cp.ExecutePrimaryBuffer(7, 5) == 5 && uint32_t(cp.m_binMask) == 6 &&
        gpu::writes == std::vector<std::pair<uint32_t, uint32_t>>{{8, 112}},
        "ring trailing operand skip crossed packet boundary");
    cp.m_primaryBufferPhysical = 0x900;
    cp.m_primaryBufferSize = 32;
    Words(0x910, {Type3(0x2b, 4), 0, 2, 0x11223344});
    Words(0x900, {0x55667788});
    Check(cp.ExecutePrimaryBuffer(4, 1) == 1 &&
        gpu::shaderWords == std::vector<uint32_t>{0x11223344, 0x55667788},
        "wrapped inline shader was not captured safely");
    cp.InitializeRingBuffer(0x800, 31);
    Check(cp.m_primaryBufferSize == 0, "invalid ring size shift accepted");
    cp.InitializeRingBuffer(0x1ffff000, 12);
    Check(cp.m_primaryBufferSize == 0, "out-of-range ring accepted");
    cp.m_indirectGuard.Reset(2);
    Words(0x700, {Type2, Type2, Type2});
    Check(!cp.ExecuteIndirectBuffer(0x700, 3), "packet budget ignored");
    Check(RunIb(cp, 0x100, 1), "budget reset failed");

    // Compile and run both production callback dispatch loops while replacing
    // callback/userData registrations, including unregister and re-register.
    cp.SetInterruptCallback(1, 15);
    std::thread vsync([&] { cp.VsyncMain(); });
    std::thread interrupts([&] { cp.InterruptMain(); });
    cp.DispatchInterrupt(1, 2);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((!vblankCalls || !interruptCalls) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    for (uint32_t i = 0; i < 100000; ++i) {
        const uint32_t callback = i % 3;
        cp.SetInterruptCallback(callback, callback ? callback * 10 + 5 : 0);
        if (!(i % 32)) cp.DispatchInterrupt(1, i % 6);
        const auto snapshot = cp.GetInterruptCallbackSnapshot();
        if (snapshot.callback && snapshot.userData != snapshot.callback * 10 + 5) ++mismatches;
    }
    cp.m_running = false;
    cp.m_interruptCv.notify_all();
    vsync.join(); interrupts.join();
    Check(vblankCalls && interruptCalls, "both callback paths were not exercised");
    Check(!mismatches, "callback was paired with another registration's userdata");
    Check(cp.m_interruptsCompleted > 0, "interrupt completion was not published");
    std::cout << "PASS production GPU parser: cycles, depth, physical extents, truncated/opcode payloads, variable shader/ALU/draw operands, error propagation, ring/shader wrap, packet budget; concurrent callback snapshots/dispatch\n";
} catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='clang++')
    parser.add_argument('--out', type=Path, required=True)
    sanitizers = parser.add_mutually_exclusive_group()
    sanitizers.add_argument('--sanitize', action='store_true')
    sanitizers.add_argument('--tsan', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    mock = out / 'stubs/gpu'
    mock.mkdir(parents=True, exist_ok=True)
    (mock / 'frame_plan.h').write_text('#pragma once\nnamespace gpu::frame_plan::wire { struct PlanStage {}; struct CatalogStage {}; }\n')
    source = (ROOT / 'LostOdysseyRecomp/gpu/command_processor.cpp').read_text()
    opcodes = between(source, '        enum Type3Opcode : uint32_t', '        constexpr uint32_t kSwapSignature')
    minimums = between(source, '        uint32_t Type3MinimumOperands(', '        uint32_t GpuSwap(')
    reader = between(source, '    inline uint32_t CommandProcessor::Reader::ReadAndSwap()', '    bool CommandProcessor::Init()')
    ring = between(source, '    void CommandProcessor::InitializeRingBuffer(', '    void CommandProcessor::EnableReadPointerWriteBack(')
    registration = between(source, '    void CommandProcessor::SetInterruptCallback(', '    void CommandProcessor::UpdateWritePointer(')
    execution = between(source, '    void CommandProcessor::VsyncMain()', '    bool CommandProcessor::ExecutePacketType3(')
    type3 = between(source, '    bool CommandProcessor::ExecutePacketType3(', '        case PM4_ME_INIT:')
    handlers = source[source.index('    bool CommandProcessor::ExecutePacketType3('):]
    ib = between(handlers, '        case PM4_INDIRECT_BUFFER:\n', '        case PM4_WAIT_REG_MEM:\n')
    constants = between(handlers, '        case PM4_SET_CONSTANT:\n', '        case PM4_SET_CONSTANT2:\n')
    memory_write = between(handlers, '        case PM4_MEM_WRITE:\n', '        case PM4_COND_WRITE:\n')
    rmw = between(handlers, '        case PM4_REG_RMW:\n', '        case PM4_REG_TO_MEM:\n')
    bin_mask = between(handlers, '        case PM4_SET_BIN_MASK_LO:', '        case PM4_SET_BIN_MASK_HI:')
    extents = between(handlers, '        case PM4_EVENT_WRITE_EXT:\n', '        case PM4_EVENT_WRITE_ZPD:\n')
    shaders = between(handlers, '        case PM4_IM_LOAD:\n', '        case PM4_INVALIDATE_STATE:\n')
    draw = between(handlers, '        case PM4_DRAW_INDX:\n', '            {\n                renderer::DrawInfo di;')
    cpp = out / 'gpu_safety.cpp'
    cpp.write_text(PRELUDE + opcodes + minimums + reader + ring + registration + execution + type3 + ib + rmw + bin_mask + memory_write + extents + constants + shaders +
                   draw + '            return true;\n        }\n' +
                   '        default: reader.Advance(count); return true;\n        }\n    }\n' + TAIL)
    flags = ['-std=c++20', '-pthread', '-I' + str(out / 'stubs'), '-I' + str(ROOT / 'LostOdysseyRecomp')]
    if args.sanitize or args.tsan:
        flags += ['-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=' + ('thread' if args.tsan else 'address,undefined')]
    else:
        flags += ['-O2']
    build = subprocess.run([args.cxx, *flags, str(cpp), '-o', str(out / 'gpu_safety')], capture_output=True, text=True, timeout=120)
    (out / 'build.log').write_text(build.stdout + build.stderr)
    if build.returncode:
        print(build.stderr)
        return build.returncode
    run = subprocess.run([str(out / 'gpu_safety')], capture_output=True, text=True, timeout=30)
    (out / 'run.log').write_text(run.stdout + run.stderr)
    print(run.stdout + run.stderr, end='')
    return run.returncode


if __name__ == '__main__':
    raise SystemExit(main())
