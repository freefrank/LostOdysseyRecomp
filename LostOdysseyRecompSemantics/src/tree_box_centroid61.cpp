#include "lo_semantics/tree_box_centroid61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace lo::semantic::gpu::tree_box_centroid61 {
namespace {
using recovery_abi::Address;
constexpr std::uint64_t HalfConstantBase = 0xffffffff82020000ull;
constexpr std::uint32_t HalfConstantAddress = 0x8201f9f0u;

struct Centroid {
    GuestMemory& memory;
    float_triplet_transfer::NativeServices& native;
    Registers& state;

    double Value(unsigned index) const {
        return std::bit_cast<double>(state.fpr_bits[index]);
    }
    void Single(unsigned index, double value) {
        state.fpr_bits[index] = std::bit_cast<std::uint64_t>(
            static_cast<double>(static_cast<float>(value)));
    }
    void Load(unsigned index, std::uint64_t address) {
        const auto value = std::bit_cast<float>(memory.ReadU32(Address(address)));
        state.fpr_bits[index] = std::bit_cast<std::uint64_t>(static_cast<double>(value));
    }
    void Store(unsigned index, std::uint64_t address) {
        memory.WriteU32(Address(address), std::bit_cast<std::uint32_t>(
            static_cast<float>(Value(index))));
    }
    void EnableGradualUnderflow() {
        if (state.cached_fp_control & 0x8040u) {
            state.cached_fp_control &= ~0x8040u;
            native.SetHostFpControl(state.cached_fp_control);
        }
    }
    static std::uint64_t Twice(std::uint64_t value) {
        return (value << 1u) & 0xfffffffeu;
    }
    static std::uint64_t WordOffset(std::uint64_t value) {
        return (value << 2u) & 0xfffffffcu;
    }
    void Axis() {
        auto& r = state.r;
        r[10] = r[5] + 3u;
        r[11] = memory.ReadU32(Address(r[3] + 72u));
        r[9] = WordOffset(r[5]);
        r[8] = WordOffset(r[10]);
        r[10] = Twice(r[4]);
        r[10] += r[4];
        r[10] = (r[10] << 3u) & 0xfffffff8u;
        r[11] += r[10];
        EnableGradualUnderflow();
        Load(0, r[8] + r[11]);
        Load(13, r[9] + r[11]);
        r[11] = HalfConstantBase;
        Single(13, Value(0) + Value(13));
        Load(0, HalfConstantAddress);
        Single(1, Value(13) * Value(0));
    }
    void Triplet() {
        auto& r = state.r;
        r[10] = Twice(r[4]);
        r[11] = memory.ReadU32(Address(r[3] + 72u));
        r[10] += r[4];
        r[10] = (r[10] << 3u) & 0xfffffff8u;
        r[11] += r[10];
        EnableGradualUnderflow();
        Load(0, r[11]);
        Load(13, r[11] + 12u);
        Load(12, r[11] + 4u);
        Single(13, Value(0) + Value(13));
        Load(10, r[11] + 16u);
        Load(0, r[11] + 20u);
        Single(12, Value(10) + Value(12));
        Load(11, r[11] + 8u);
        r[11] = HalfConstantBase;
        Single(11, Value(0) + Value(11));
        Load(0, HalfConstantAddress);
        Single(13, Value(13) * Value(0));
        Store(13, r[5]);
        Single(12, Value(12) * Value(0));
        Store(12, r[5] + 4u);
        Single(0, Value(11) * Value(0));
        Store(0, r[5] + 8u);
    }
};
}
bool Apply(GuestAddress entry, GuestMemory& memory,
           float_triplet_transfer::NativeServices& native, Registers& state) {
    Centroid operation{memory, native, state};
    switch (entry) {
    case 0x82bd8848u: operation.Axis(); return true;
    case 0x82bd8888u: operation.Triplet(); return true;
    default: return false;
    }
}
}
