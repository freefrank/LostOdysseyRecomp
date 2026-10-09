#pragma once
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/object_sort_engine61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/crt_close_next61.h"
#include "lo_semantics/crt_reader_follow61.h"

#include <algorithm>
#include <bit>

namespace sort_engine61_oracle {
using Full = object_sort_engine61::Registers;
constexpr GuestAddress Owner = 0x30000u, Writer = 0x31000u, Node = 0x32000u;
constexpr GuestAddress Data = 0x33000u, Vtable = 0x34000u, Indices = 0x35000u;
constexpr GuestAddress AllocateTarget = 0x2a00u, FreeTarget = 0x2a04u;
constexpr std::array<test::Region, 5> EngineRegions{{{0u, 0x120000u},
    {0x82000000u, 0x1000u}, {0x821ba000u, 0x1000u},
    {0x83216000u, 0x1000u}, {0x832df000u, 0x1000u}}};

// The bucket-sort dependency type includes CRT services unreachable from its
// selected allocation/memset path. Borrow explicit throwing services instead
// of copying the unrelated open/close original-body harnesses.
struct Unselected final : crt_stream_open_routes_context::GuestServices,
    crt_stream_position_routes::NativeServices,
    crt_async_status_transfer::NativeServices,
    crt_utf8_conversion_routes::NativeServices,
    heap_allocation_context::BoundaryServices,
    heap_free_context::BoundaryServices,
    crt_free_context::LowerCalls,
    crt_stream_close_error::GuestServices,
    crt_float_environment::NativeServices,
    crt_formatting_support::NativeServices,
    crt_formatter::DynamicServices,
    crt_stream_bulk_close_routes::NativeServices,
    crt_stream_close_shared_lower::NativeServices
{
    using Open = crt_stream_open_routes_context::Registers;
    [[noreturn]] static void Fail() { throw std::runtime_error("unexpected engine CRT boundary"); }
    void KeTlsGetValue(GuestMemory&, Open&) override { Fail(); }
    void KeTlsSetValue(GuestMemory&, Open&) override { Fail(); }
    void AllocateCrtRecord(GuestMemory&, Open&) override { Fail(); }
    void EnterCriticalSection(GuestMemory&, Open&) override { Fail(); }
    void LeaveCriticalSection(GuestMemory&, Open&) override { Fail(); }
    void InitAnsiString(GuestMemory&, Open&) override { Fail(); }
    void CallOpenFile(GuestAddress, GuestMemory&, Open&) override { Fail(); }
    void NtQueryInformationFile(GuestMemory&, Full&) override { Fail(); }
    void NtSetInformationFile(GuestMemory&, Full&) override { Fail(); }
    void CallIndirect(GuestAddress, GuestMemory&, Full&) override { Fail(); }
    void NtWaitForSingleObjectEx(GuestMemory&, Full&) override { Fail(); }
    void NtStatusToDosError(GuestMemory&, Full&) override { Fail(); }
    void RtlMultiByteToUnicodeN(GuestMemory&, crt_utf8_conversion_routes::Registers&) override { Fail(); }
    void RtlNtStatusToDosError(GuestMemory&, crt_utf8_conversion_routes::Registers&) override { Fail(); }
    void CallDirect(GuestAddress, GuestMemory&, heap_allocation_context::Registers&) override { Fail(); }
    void CallNative(GuestAddress, GuestMemory&, heap_allocation_context::Registers&) override { Fail(); }
    void Call(GuestAddress, GuestMemory&, crt_free_context::Registers&) override { Fail(); }
    void CallIndirect(GuestMemory&, GuestAddress, crt_stream_close_error::Registers&) override { Fail(); }
    void CallDebugMonitor(GuestAddress, GuestMemory&, crt_float_environment::Registers&) override { Fail(); }
    void CallExceptionHandler(GuestAddress, GuestMemory&, crt_float_environment::Registers&) override { Fail(); }
    void BugCheck(GuestMemory&, crt_float_environment::Registers&) override { Fail(); }
    void InitializeUnicodeString(GuestMemory&, crt_formatting_support::Registers&) override { Fail(); }
    void UnicodeStringToAnsiString(GuestMemory&, crt_formatting_support::Registers&) override { Fail(); }
    void FreeAnsiString(GuestMemory&, crt_formatting_support::Registers&) override { Fail(); }
    void InitAnsiString(GuestMemory&, GuestAddress, GuestAddress) override { Fail(); }
    std::uint64_t WriteAnsi(GuestAddress, std::uint16_t) override { Fail(); }
    void CallGuestFormatter(GuestAddress, GuestMemory&, crt_formatter::Registers&) override { Fail(); }
    void EnterCriticalSection(GuestMemory&, Full&) override { Fail(); }
    void RtlUnwind(GuestMemory&, Full&) override { Fail(); }
};

struct Guest final : crt_close_recursive_buffer_context::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    unsigned allocations = 0u;
    void CallIndirect(GuestAddress target, GuestMemory&, Full& state) override {
        std::array<std::uint64_t, 73> event{};
        const auto snapshot = crt_full_oracle::Snapshot(state);
        std::copy(snapshot.begin(), snapshot.end(), event.begin());
        event.back() = target; events.push_back(event);
        state.r[10] ^= 0x123456789abcdef0ull;
        state.fpr_bits[7] ^= 0x100u;
        state.cr1.gt ^= 1u; state.cr7.eq ^= 1u; state.xer_ca ^= 1u;
        if (target == AllocateTarget)
            state.r[3] = 0x9988776600090000ull + 0x1000u * allocations++;
        else if (target == FreeTarget) state.r[3] = 0x1234567800000000ull;
        else throw std::runtime_error("unexpected engine allocator target");
    }
};
struct Fp final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t value) override { PPCFPSCRRegister{}.setcsr(value); }
};
struct RestoreHost {
    std::uint32_t control = PPCFPSCRRegister{}.getcsr();
    ~RestoreHost() { PPCFPSCRRegister{}.setcsr(control); }
};
struct Environment {
    Services stream;
    Unselected unused;
    Guest guest;
    Fp fp;
    explicit Environment(GuestWindow& window) : stream(window, Mode::LockedWrite) {}
    object_sort_engine61::Dependencies Deps() {
        family::Dependencies stream_deps{stream, stream, stream, stream,
            {stream, stream, stream, stream, stream, stream, stream, stream}, stream, stream};
        crt_stream_open_pipeline::Dependencies open{stream_deps, unused,
            {stream_deps, unused, unused, unused},
            {stream_deps, unused, unused, unused, unused}, unused, unused};
        crt_wide_stream_output::Dependencies output{stream_deps, unused};
        crt_float_formatting::Dependencies floating{{stream, stream}, unused};
        crt_formatter::Dependencies formatter{output, floating, unused, stream, unused};
        crt_format_stream::Dependencies format{formatter};
        crt_stream_close_error::Dependencies close{stream_deps, unused};
        crt_stream_close_pipeline::Dependencies pipeline{close, format, unused};
        crt_stream_bulk_close_routes::Dependencies bulk{pipeline, unused};
        crt_stream_close_shared_lower::Dependencies accepted{bulk, {open, stream}, unused};
        return {{guest, accepted}, fp};
    }
};

struct Point { std::uint32_t x, y, z; };
inline void Seed(GuestWindow& window, const std::vector<Point>& points,
    Point previous, unsigned dimension, unsigned pending = 0u)
{
    window.Fill(0xa5u);
    auto memory = window.Memory();
    memory.WriteU32(Owner + 4u, static_cast<std::uint32_t>(points.size()));
    memory.WriteU32(Owner + 8u, Indices);
    memory.WriteU32(Writer, Node); memory.WriteU32(Writer + 16u, Data);
    memory.WriteU8(Writer + 24u, static_cast<std::uint8_t>(pending));
    memory.WriteU8(Writer + 25u, pending ? 5u : 0u);
    memory.WriteU32(Node, Data); memory.WriteU32(Node + 4u, 0u);
    memory.WriteU32(Node + 8u, 1024u); memory.WriteU32(Node + 12u, 0u);
    memory.WriteU32(0x832df554u, 0u); memory.WriteU32(0x83216624u, Vtable);
    memory.WriteU32(Vtable, AllocateTarget | 1u); memory.WriteU32(Vtable + 12u, FreeTarget | 3u);
    memory.WriteU32(0x82000e50u, std::bit_cast<std::uint32_t>(1.0f));
    memory.WriteU32(0x821baa74u, std::bit_cast<std::uint32_t>(2.0f));
    memory.WriteU32(0x83216184u, previous.x);
    memory.WriteU32(0x83216188u, previous.y);
    memory.WriteU32(0x8321618cu, previous.z);
    for (unsigned i = 0u; i < points.size(); ++i) {
        const auto point = points[i];
        memory.WriteU32(Indices + i * 4u, point.x + dimension * (point.y + dimension * point.z));
    }
}
inline Full Initial(unsigned dimension) {
    Full state{};
    for (unsigned i = 0u; i < 32u; ++i) {
        state.r[i] = 0x1122334400000000ull + i;
        state.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    state.r[1] = 0x8877665500080000ull;
    state.r[3] = 0xaabbccdd00000000ull | Owner;
    state.r[4] = 0xccdd001100000000ull | Writer;
    state.r[5] = dimension;
    state.lr = 0x9988776681234567ull;
    state.cached_fp_control = 0x9fc0u; state.xer_so = 1u;
    return state;
}
inline void EngineLower(GuestAddress entry, Environment& env, Full& state) {
    auto& memory = env.stream.memory;
    const auto deps = env.Deps();
    if (entry == 0x82bd0938u) {
        (void)crt_close_next61::Apply(entry, memory, env.guest, state); return;
    }
    if (entry == 0x82bd2870u) {
        (void)reader_buffer_growth61::Apply(entry, memory, {env.guest, env.fp}, state); return;
    }
    if (entry == 0x82bd2c78u) {
        (void)crt_reader_follow61::Apply(entry, memory, env.guest, state); return;
    }
    if (entry == 0x82bd2df0u) {
        (void)crt_reader_bucket_sort61::Apply(entry, memory, deps.sort, state); return;
    }
    if (!object_sort_support61::Apply(entry, memory, {env.guest, env.fp}, state))
        throw std::runtime_error("unexpected engine lower entry");
}
}
