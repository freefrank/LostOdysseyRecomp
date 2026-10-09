#include "lo_semantics/tree_box_bounds61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>

namespace lo::semantic::gpu::tree_box_bounds61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Bounds {
    GuestMemory& memory;
    float_triplet_transfer::NativeServices& native;
    Registers& state;
    double Value(unsigned f) const { return std::bit_cast<double>(state.fpr_bits[f]); }
    void Load(unsigned f, std::uint64_t address) {
        state.fpr_bits[f] = std::bit_cast<std::uint64_t>(static_cast<double>(
            std::bit_cast<float>(memory.ReadU32(Address(address)))));
    }
    void Store(unsigned f, std::uint64_t address) {
        memory.WriteU32(Address(address), std::bit_cast<std::uint32_t>(static_cast<float>(Value(f))));
    }
    void Gradual() {
        if (state.cached_fp_control & 0x8040u) {
            state.cached_fp_control &= ~0x8040u;
            native.SetHostFpControl(state.cached_fp_control);
        }
    }
    void IntegerCompare(std::uint64_t a, std::uint64_t b) {
        const auto x = Address(a), y = Address(b);
        state.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), state.xer_so};
    }
    void FloatCompare(unsigned a, unsigned b) {
        Gradual();
        const auto x = Value(a), y = Value(b);
        state.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y),
                     std::uint8_t(std::isnan(x) || std::isnan(y))};
    }
    void Merge() {
        const auto destination = state.r[3], source = state.r[4];
        Gradual(); Load(8, destination); Load(0, source); Load(13, source + 4u);
        FloatCompare(8, 0);
        Load(12, source + 8u); Load(6, destination + 4u); Load(7, destination + 8u);
        if (!state.cr6.lt) state.fpr_bits[8] = state.fpr_bits[0];
        FloatCompare(6, 13);
        if (!state.cr6.lt) state.fpr_bits[6] = state.fpr_bits[13];
        FloatCompare(7, 12);
        if (!state.cr6.lt) state.fpr_bits[7] = state.fpr_bits[12];
        Gradual(); Load(0, source + 12u); Load(13, destination + 12u); Load(10, source + 16u);
        FloatCompare(13, 0);
        Load(9, source + 20u); Load(11, destination + 16u); Load(12, destination + 20u);
        if (!state.cr6.gt) state.fpr_bits[13] = state.fpr_bits[0];
        FloatCompare(11, 10);
        if (!state.cr6.gt) state.fpr_bits[11] = state.fpr_bits[10];
        FloatCompare(12, 9);
        if (!state.cr6.gt) state.fpr_bits[12] = state.fpr_bits[9];
        Gradual();
        constexpr unsigned resultRegisters[]{8, 6, 7, 13, 11, 12};
        for (unsigned axis = 0; axis < 6; ++axis) Store(resultRegisters[axis], destination + 4u * axis);
    }
    void BoxOffset() {
        auto& r = state.r;
        r[9] = (r[11] << 1u) & 0xfffffffeu;
        r[11] += r[9];
        r[11] = (r[11] << 3u) & 0xfffffff8u;
    }
    void Gather() {
        auto& r = state.r;
        r[12] = state.lr; state.lr = 0x82bd8ee8u;
        for (unsigned i = 28; i < 32; ++i) WriteU64(memory, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
        const auto stack = r[1]; r[1] -= 128u; memory.WriteU32(Address(r[1]), Address(stack));
        r[28] = r[3]; r[31] = r[6];
        IntegerCompare(r[4], 0);
        bool valid = !state.cr6.eq;
        if (valid) { IntegerCompare(r[5], 0); valid = !state.cr6.eq; }
        if (valid) {
            r[11] = memory.ReadU32(Address(r[4]));
            IntegerCompare(r[5], 1);
            r[10] = memory.ReadU32(Address(r[28] + 72u));
            BoxOffset(); r[11] += r[10];
            Gradual();
            // The original copies coordinate-by-coordinate, not via a box
            // snapshot. Keep that ordering when output aliases source storage.
            for (unsigned axis = 0; axis < 6; ++axis) {
                Load(0, r[11] + 4u * axis); Store(0, r[31] + 4u * axis);
            }
            if (state.cr6.gt) {
                r[29] = r[4] + 4u; r[30] = r[5] - 1u;
                do {
                    r[11] = memory.ReadU32(Address(r[29])); r[3] = r[31];
                    r[10] = memory.ReadU32(Address(r[28] + 72u));
                    BoxOffset(); r[4] = r[11] + r[10];
                    state.lr = 0x82bd8f7cu; Merge();
                    --r[30]; r[29] += 4u; IntegerCompare(r[30], 0);
                } while (!state.cr6.eq);
            }
        }
        r[3] = valid ? 1u : 0u;
        r[1] += 128u;
        for (unsigned i = 28; i < 32; ++i) r[i] = ReadU64(memory, Address(r[1] - 16u - 8u * (31u - i)));
        r[12] = memory.ReadU32(Address(r[1] - 8u)); state.lr = r[12];
    }
};
}
bool Apply(GuestAddress entry, GuestMemory& memory,
           float_triplet_transfer::NativeServices& native, Registers& state) {
    Bounds operation{memory, native, state};
    switch (entry) {
    case 0x82bdde70u: operation.Merge(); return true;
    case 0x82bd8ee0u: operation.Gather(); return true;
    default: return false;
    }
}
}
