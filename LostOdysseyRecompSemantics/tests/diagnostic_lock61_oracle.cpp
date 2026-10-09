// Pin both lock originals. The CAS hook only injects an optional competing
// atomic store before a real atomic compare/exchange; it never fakes success.
#ifdef __sync_bool_compare_and_swap
#undef __sync_bool_compare_and_swap
#endif
#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/diagnostic_lock61.h"
#include <algorithm>
#include <atomic>
#include <mutex>

namespace diagnostic_lock61_oracle {
using Full = diagnostic_lock61::Registers;
using Machine = diagnostic_lock61::MachineState;
constexpr GuestAddress Handle = 0x30000u, Target = 0x31000u;
constexpr GuestAddress Tls = 0x40000u, Thread = 0x41000u;
constexpr std::array<test::Region, 1> LockRegions{{{0u,0x120000u}}};
constexpr std::uint32_t Swap(std::uint32_t value) {
    return (value << 24u) | ((value << 8u) & 0x00ff0000u) |
        ((value >> 8u) & 0x0000ff00u) | (value >> 24u);
}
using Event = std::array<std::uint64_t, 79>;
struct Synchronization final : diagnostic_lock61::SynchronizationServices {
    test::GuestWindow& window;
    Full& live;
    Machine& machine;
    std::recursive_mutex critical_section;
    bool held = false;
    bool inject;
    unsigned attempts = 0u, failures = 0u;
    std::vector<Event> events;
    Synchronization(test::GuestWindow& memory, Full& registers, Machine& extra, bool competing)
        : window(memory), live(registers), machine(extra), inject(competing) {}
    ~Synchronization() { if (held) critical_section.unlock(); }
    std::uint32_t& Raw(GuestAddress address) {
        if (address != Target + 28u) throw std::runtime_error("unexpected lock flag address");
        return *reinterpret_cast<std::uint32_t*>(window.Bytes() + address);
    }
    Event Trace(unsigned operation, GuestAddress address, std::uint32_t expected,
        std::uint32_t desired, const Full& state, const Machine& extra) {
        Event event{}; const auto snapshot = crt_full_oracle::Snapshot(state);
        std::copy(snapshot.begin(), snapshot.end(), event.begin());
        event[72] = operation; event[73] = address; event[74] = expected;
        event[75] = desired; event[76] = extra.msr; event[77] = extra.reserved_bits;
        return event;
    }
    std::uint32_t LoadReservedWord(GuestAddress address, GuestMemory&) override {
        return Swap(std::atomic_ref<std::uint32_t>(Raw(address)).load(std::memory_order_seq_cst));
    }
    bool RealCas(GuestAddress address, std::uint32_t expected, std::uint32_t desired,
        const Full& state, const Machine& extra) {
        auto event = Trace(2u, address, expected, desired, state, extra);
        auto atomic = std::atomic_ref<std::uint32_t>(Raw(address));
        if (inject && attempts == 0u)
            atomic.store(Swap(2u), std::memory_order_seq_cst);
        auto expected_raw = Swap(expected);
        const bool success = atomic.compare_exchange_strong(expected_raw, Swap(desired),
            std::memory_order_seq_cst, std::memory_order_seq_cst);
        ++attempts; if (!success) ++failures;
        event[78] = success; events.push_back(event);
        return success;
    }
    bool CompareExchangeWord(GuestAddress address, std::uint32_t expected,
        std::uint32_t desired, GuestMemory&) override {
        return RealCas(address, expected, desired, live, machine);
    }
    void EnterCriticalSection(GuestMemory&, Full& state, Machine& extra) override {
        if (state.r[3] != Target || held) throw std::runtime_error("unexpected native critical enter");
        events.push_back(Trace(1u, Target, 0u, 0u, state, extra));
        critical_section.lock(); held = true;
        state.r[6] ^= 0x123456789abcdef0ull;
        state.fpr_bits[7] ^= 0x100u; state.cr1.gt ^= 1u;
        extra.msr ^= 0x10000000u;
        extra.reserved_bits ^= 0x0100000000000000ull;
        state.r[3] = 0xabcdef1234567890ull;
    }
    void LeaveCriticalSection(GuestMemory&, Full& state, Machine& extra) override {
        if (state.r[3] != Target || !held) throw std::runtime_error("unexpected native critical leave");
        events.push_back(Trace(3u, Target, 0u, 0u, state, extra));
        critical_section.unlock(); held = false;
        state.r[6] ^= 0xfedcba9876543210ull;
        state.fpr_bits[7] ^= 0x180u; state.cr7.eq ^= 1u;
        extra.msr ^= 0x01000000u;
        extra.reserved_bits ^= 0x0200000000000000ull;
        state.r[3] = 0x123456789abcdef0ull;
    }
};
Synchronization* original_sync = nullptr;
Machine FromMachine(const PPCContext& context) { return {context.msr, context.reserved.u64}; }
void ToMachine(PPCContext& context, Machine machine) {
    context.msr = machine.msr; context.reserved.u64 = machine.reserved_bits;
}
void Check(bool competing) {
    test::GuestWindow original(LockRegions), recovered(LockRegions);
    const auto seed = [](test::GuestWindow& window) {
        window.Fill(0xa5u); auto memory = window.Memory();
        memory.WriteU32(Handle, Target); memory.WriteU32(Target + 28u, 0u);
        memory.WriteU32(Tls + 256u, Thread); memory.WriteU32(Thread + 332u, 0x76543210u);
    };
    seed(original); seed(recovered);
    auto memory = recovered.Memory();
    Full initial{};
    for (unsigned i = 0u; i < 32u; ++i) {
        initial.r[i] = 0x1122334400000000ull + i;
        initial.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    initial.r[1] = 0x8877665500080000ull;
    initial.r[3] = 0xaabbccdd00000000ull | Handle;
    initial.r[13] = 0x7766554400000000ull | Tls;
    initial.lr = 0x9988776681234567ull; initial.xer_so = 1u;
    auto expected_state = initial, state = initial;
    Machine expected_machine{0x020a8020u,0xcafebabe11223344ull}, machine = expected_machine;
    Synchronization expected(original, expected_state, expected_machine, competing);
    Synchronization actual(recovered, state, machine, competing);
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial); ToMachine(context, expected_machine);
    for (const auto entry : {0x822b29a0u,0x822b3438u}) {
        context.r3.u64 = initial.r[3]; state.r[3] = initial.r[3];
        original_sync = &expected;
        if (entry == 0x822b29a0u) __imp__sub_822B29A0(context, original.Bytes());
        else __imp__sub_822B3438(context, original.Bytes());
        original_sync = nullptr;
        if (!diagnostic_lock61::Apply(entry, memory, actual, state, machine))
            throw std::runtime_error("missing diagnostic lock entry");
        expected_state = crt_full_oracle::FromPpc(context); expected_machine = FromMachine(context);
        if (crt_full_oracle::Snapshot(expected_state) != crt_full_oracle::Snapshot(state) ||
            expected_machine.msr != machine.msr || expected_machine.reserved_bits != machine.reserved_bits ||
            !original.EqualCommitted(recovered) || expected.events != actual.events)
            throw std::runtime_error("diagnostic lock Full72/MSR/reservation/RAM/native-CAS trace mismatch");
    }
    if (actual.held || actual.failures != (competing ? 1u : 0u) ||
        actual.attempts != (competing ? 3u : 2u) || state.r[3] != 1u ||
        state.r[1] != initial.r[1] || state.lr != 0x81234567u ||
        memory.ReadU32(Target + 28u) != (competing ? 2u : 0u) ||
        memory.ReadU32(Target + 32u) != 0x76543210u ||
        Address(machine.reserved_bits >> 32u) != 0xc9febabeu)
        throw std::runtime_error("diagnostic lock retry/native mutex/TLS/reserved-high outcome mismatch");
}
}

bool OriginalDiagnosticLock61Cas(std::uint32_t* pointer, std::uint32_t expected_raw,
    std::uint32_t desired_raw, PPCContext& context) {
    auto& sync = *diagnostic_lock61_oracle::original_sync;
    const auto address = static_cast<GuestAddress>(reinterpret_cast<std::uint8_t*>(pointer) - sync.window.Bytes());
    return sync.RealCas(address, diagnostic_lock61_oracle::Swap(expected_raw),
        diagnostic_lock61_oracle::Swap(desired_raw), crt_full_oracle::FromPpc(context),
        diagnostic_lock61_oracle::FromMachine(context));
}
void OriginalDiagnosticLock61Enter(PPCContext& context, std::uint8_t*) {
    auto& sync = *diagnostic_lock61_oracle::original_sync;
    auto state = crt_full_oracle::FromPpc(context);
    auto machine = diagnostic_lock61_oracle::FromMachine(context);
    auto memory = sync.window.Memory(); sync.EnterCriticalSection(memory, state, machine);
    crt_full_oracle::ToPpc(context, state); diagnostic_lock61_oracle::ToMachine(context, machine);
}
void OriginalDiagnosticLock61Leave(PPCContext& context, std::uint8_t*) {
    auto& sync = *diagnostic_lock61_oracle::original_sync;
    auto state = crt_full_oracle::FromPpc(context);
    auto machine = diagnostic_lock61_oracle::FromMachine(context);
    auto memory = sync.window.Memory(); sync.LeaveCriticalSection(memory, state, machine);
    crt_full_oracle::ToPpc(context, state); diagnostic_lock61_oracle::ToMachine(context, machine);
}
void OriginalDiagnosticLock61ThreadIdentity(PPCContext& context, std::uint8_t*) {
    auto memory = diagnostic_lock61_oracle::original_sync->window.Memory();
    context.r11.u64 = memory.ReadU32(Address(context.r13.u64 + 256u));
    context.r3.u64 = memory.ReadU32(Address(context.r11.u64 + 332u));
}
int main() {
    try {
        diagnostic_lock61_oracle::Check(false);
        diagnostic_lock61_oracle::Check(true);
        std::puts("PASS diagnostic-lock61 2 original acquire/release scenarios with real CAS and recursive mutex");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
