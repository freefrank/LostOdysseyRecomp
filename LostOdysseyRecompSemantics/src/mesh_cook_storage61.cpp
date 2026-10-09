#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/crt_reader_upper_follow61.h"
#include "lo_semantics/grid_transform_support61.h"
#include "lo_semantics/owned_tree_reorder_support61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/tree_mesh_lifetime61.h"
#include <bit>
namespace lo::semantic::gpu::mesh_cook_storage61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
struct Mesh {
    GuestMemory &m;
    Dependencies deps;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Store(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Compare(std::uint64_t v) {
        auto x = Address(v);
        s.cr6 = {0u, std::uint8_t(x != 0u), std::uint8_t(x == 0u), s.xer_so};
    }
    void Enter(unsigned first, unsigned frame, GuestAddress continuation = 0) {
        auto &r = s.r;
        r[12] = s.lr;
        if (continuation)
            s.lr = continuation;
        Store(r[1] - 8u, r[12]);
        for (unsigned i = first; i < 32; ++i)
            WriteU64(m, Address(r[1] - 16u - 8u * (31u - i)), r[i]);
        const auto stack = r[1];
        r[1] -= frame;
        Store(r[1], stack);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        for (unsigned i = first; i < 32; ++i)
            r[i] = ReadU64(m, Address(r[1] - 16u - 8u * (31u - i)));
        r[12] = Word(r[1] - 8u);
        s.lr = r[12];
    }
    void Allocator(GuestAddress continuation) {
        s.lr = continuation;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m, deps.guest, s);
    }
    void Call(GuestAddress continuation) {
        s.ctr = s.r[11];
        s.lr = continuation;
        deps.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Load(unsigned f, std::uint64_t p) {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            deps.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[f] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Save(unsigned f, std::uint64_t p) {
        Store(p, std::bit_cast<std::uint32_t>(
                     static_cast<float>(std::bit_cast<double>(s.fpr_bits[f]))));
    }
    void Arrays() {
        Enter(30, 112);
        auto &r = s.r;
        r[31] = r[3];
        r[30] = 0;
        constexpr unsigned offsets[]{20, 16, 4, 12};
        constexpr GuestAddress allocators[]{0x82bbcdacu, 0x82bbcdd4u, 0x82bbcdfcu, 0x82bbce24u};
        for (unsigned i = 0; i < 4; ++i) {
            r[11] = Word(r[31] + offsets[i]);
            Compare(r[11]);
            if (!s.cr6.eq) {
                Allocator(allocators[i]);
                r[11] = Word(r[3]);
                r[4] = Word(r[31] + offsets[i]);
                r[11] = Word(r[11] + 12u);
                Call(allocators[i] + 20u);
                Store(r[31] + offsets[i], r[30]);
            }
        }
        Leave(30, 112);
    }
    void Release() {
        Enter(29, 112, 0x82bc51f0u);
        auto &r = s.r;
        r[31] = r[3];
        r[29] = 0;
        r[30] = Word(r[31] + 80u);
        Compare(r[30]);
        if (!s.cr6.eq) {
            r[3] = r[30];
            s.lr = 0x82bc5210u;
            Arrays();
            Allocator(0x82bc5214u);
            r[11] = Word(r[3]);
            r[4] = r[30];
            r[11] = Word(r[11] + 12u);
            Call(0x82bc5228u);
            Store(r[31] + 80u, r[29]);
        }
        r[3] = r[31] + 8u;
        Store(r[31] + 4u, r[29]);
        s.lr = 0x82bc5238u;
        (void)owned_tree_reorder_support61::Apply(0x82bd20f0u, m, deps, s);
        r[3] = Word(r[31] + 288u);
        Compare(r[3]);
        if (!s.cr6.eq) {
            r[11] = Word(r[3]);
            r[4] = 1;
            r[11] = Word(r[11]);
            Call(0x82bc5258u);
            Store(r[31] + 288u, r[29]);
        }
        Store(r[31] + 48u, r[29]);
        Store(r[31] + 44u, r[29]);
        Leave(29, 112);
    }
    void Construct() {
        Enter(30, 112);
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        r[31] = r[3];
        r[11] += 23856u;
        r[3] = r[31] + 8u;
        Store(r[31], r[11]);
        s.lr = 0x82bc58ccu;
        (void)crt_reader_upper_follow61::Apply(0x82bd1aa8u, m, deps.guest, s);
        r[30] = 0;
        r[3] = r[31] + 84u;
        Store(r[31] + 80u, r[30]);
        s.lr = 0x82bc58dcu;
        (void)grid_transform_support61::Apply(0x82f2b308u, m, deps.fp, s);
        r[10] = 0xffffffff82000000ull;
        Store(r[31] + 108u, r[30]);
        r[11] = r[31] + 112u;
        r[3] = r[31] + 156u;
        Load(0, r[10] + 3596u);
        r[10] = 0xffffffff82000000ull;
        for (unsigned off : {0u, 4u, 8u})
            Save(0, r[11] + off);
        Load(13, r[10] + 3428u);
        for (unsigned off : {12u, 16u, 20u})
            Save(13, r[11] + off);
        s.lr = 0x82bc5914u;
        (void)mesh_auxiliary_storage61::Apply(0x82bc8588u, m, deps, s);
        r[11] = 0xffffffff82000000ull;
        Store(r[31] + 288u, r[30]);
        r[3] = r[31];
        Load(0, r[11] + 3648u);
        Save(0, r[31] + 292u);
        for (unsigned off : {344u, 4u, 44u, 48u})
            Store(r[31] + off, r[30]);
        Leave(30, 112);
    }
    void DerivedConstruct() {
        Enter(31, 96);
        auto &r = s.r;
        r[31] = r[3];
        s.lr = 0x82b9e4c8u;
        Construct();
        r[11] = 0xffffffff820d0000ull;
        r[3] = r[31];
        r[11] += 23856u;
        Store(r[31], r[11]);
        Leave(31, 96);
    }
    void Destroy() {
        Enter(31, 96);
        auto &r = s.r;
        r[11] = 0xffffffff820d0000ull;
        r[31] = r[3];
        r[11] += 23856u;
        Store(r[31], r[11]);
        s.lr = 0x82bc5974u;
        Release();
        r[3] = r[31] + 156u;
        s.lr = 0x82bc597cu;
        (void)mesh_auxiliary_storage61::Apply(0x82bc85f0u, m, deps, s);
        r[3] = r[31] + 84u;
        s.lr = 0x82bc5984u; /*822D3068 is an already-mapped empty leaf.*/
        r[3] = r[31] + 8u;
        s.lr = 0x82bc598cu;
        (void)tree_mesh_lifetime61::Apply(0x82bd2268u, m, deps, s);
        r[11] = 0xffffffff820d0000ull;
        r[11] += 26528u;
        Store(r[31], r[11]);
        Leave(31, 96);
    }
    void DerivedDestroy() {
        s.r[11] = 0xffffffff820d0000ull;
        s.r[11] += 23856u;
        Store(s.r[3], s.r[11]);
        Destroy();
    }
    void Temporary() {
        Enter(29, 112, 0x82ba01c8u);
        auto &r = s.r;
        r[31] = r[4];
        r[29] = 0;
        r[30] = 0xffffffff832e0000ull;
        constexpr unsigned offsets[]{8, 20};
        constexpr GuestAddress returns[]{0x82ba01f8u, 0x82ba021cu};
        for (unsigned i = 0; i < 2; ++i) {
            r[4] = Word(r[31] + offsets[i]);
            Compare(r[4]);
            if (!s.cr6.eq) {
                r[3] = Word(r[30] - 2744u);
                r[11] = Word(r[3]);
                r[11] = Word(r[11] + 20u);
                Call(returns[i]);
                Store(r[31] + offsets[i], r[29]);
            }
        }
        r[3] = 0;
        Leave(29, 112);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies deps, Registers &s) {
    Mesh x{m, deps, s};
    switch (e) {
    case 0x82bbcd80u:
    case 0x82bbd4e0u:
        x.Arrays();
        return true;
    case 0x82bc51e8u:
        x.Release();
        return true;
    case 0x82bc58a0u:
        x.Construct();
        return true;
    case 0x82bc5950u:
        x.Destroy();
        return true;
    case 0x82b9e4b0u:
        x.DerivedConstruct();
        return true;
    case 0x82b9e518u:
        x.DerivedDestroy();
        return true;
    case 0x82ba01c0u:
        x.Temporary();
        return true;
    default:
        return false;
    }
}
} // namespace lo::semantic::gpu::mesh_cook_storage61
