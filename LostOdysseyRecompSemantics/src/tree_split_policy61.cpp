#include "lo_semantics/tree_split_policy61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::tree_split_policy61 {
bool Apply(GuestAddress entry, GuestMemory& memory,
           float_triplet_transfer::NativeServices& native, Registers& state) {
    using recovery_abi::Address;
    auto& r = state.r;
    if (entry == 0x82bb3b88u) {
        const auto threshold = memory.ReadU32(Address(r[3] + 4u));
        const bool withinLimit = threshold >= Address(r[5]);
        state.xer_ca = withinLimit;
        r[11] = withinLimit ? 0u : ~std::uint64_t(0);
        r[3] = withinLimit ? 0u : 1u;
        return true;
    }
    if (entry != 0x82bb3b60u) return false;
    r[11] = ((r[7] + 3u) << 2u) & 0xfffffffcu;
    r[10] = (r[7] << 2u) & 0xfffffffcu;
    if (state.cached_fp_control & 0x8040u) {
        state.cached_fp_control &= ~0x8040u;
        native.SetHostFpControl(state.cached_fp_control);
    }
    const auto load = [&](unsigned f, std::uint64_t address) {
        state.fpr_bits[f] = std::bit_cast<std::uint64_t>(static_cast<double>(
            std::bit_cast<float>(memory.ReadU32(Address(address)))));
    };
    const auto value = [&](unsigned f) { return std::bit_cast<double>(state.fpr_bits[f]); };
    const auto single = [&](unsigned f, double number) {
        state.fpr_bits[f] = std::bit_cast<std::uint64_t>(static_cast<double>(static_cast<float>(number)));
    };
    load(0, r[10] + r[6]);
    load(13, r[11] + r[6]);
    r[11] = 0xffffffff82020000ull;
    single(13, value(13) + value(0));
    load(0, 0x8201f9f0u);
    single(1, value(13) * value(0));
    return true;
}
}
