#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/crt_reader_upper_follow61.h"
#include "lo_semantics/grid_transform_support61.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/owned_tree_reorder_support61.h"
#include "lo_semantics/tree_mesh_lifetime61.h"
#include <limits>
namespace cook_storage_oracle {
using Registers = mesh_cook_storage61::Registers;
constexpr GuestAddress Owner = 0x30000, Table = 0x31000, Arrays = 0x40000, Other = 0x41000,
                       OtherTable = 0x42000, Temporary = 0x43000, Free = 0x2000, Delete = 0x2004;
constexpr std::array<test::Region, 3> Regions{
    {{0, 0x120000}, {0x82000000, 0x1000}, {0x83216000, 0xca000}}};
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
Native native;
struct Guest final : manager_release_context61::GuestServices {
    std::vector<std::array<std::uint64_t, 73>> events;
    std::vector<GuestAddress> freed;
    void CallDirect(GuestAddress, GuestMemory &, Registers &) override {
        throw std::runtime_error("unexpected cook storage direct");
    }
    void CallIndirect(GuestAddress e, GuestMemory &, Registers &s) override {
        std::array<std::uint64_t, 73> event{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), event.begin());
        event.back() = e;
        events.push_back(event);
        if (e == Free)
            freed.push_back(Address(s.r[4]));
        else if (e == Delete) {
            if (s.r[3] != Other || s.r[4] != 1)
                throw std::runtime_error("cook virtual delete shape");
            freed.push_back(Other);
        } else
            throw std::runtime_error("cook storage callback");
        s.r[3] = 0;
        s.r[8] ^= 0x1234u;
        s.cr7.eq ^= 1u;
    }
};
Guest *guest = nullptr;
GuestMemory *memory = nullptr;
void Lower(GuestAddress e, GuestMemory &m, Guest &g, Registers &s) {
    switch (e) {
    case 0x82bd0798u:
        (void)crt_close_recursive_buffer_context::Apply(e, m, g, s);
        break;
    case 0x82bd1aa8u:
        (void)crt_reader_upper_follow61::Apply(e, m, g, s);
        break;
    case 0x82f2b308u:
        (void)grid_transform_support61::Apply(e, m, native, s);
        break;
    case 0x82bc8588u:
    case 0x82bc85f0u:
        (void)mesh_auxiliary_storage61::Apply(e, m, {g, native}, s);
        break;
    case 0x82bd20f0u:
        (void)owned_tree_reorder_support61::Apply(e, m, {g, native}, s);
        break;
    case 0x82bd2268u:
        (void)tree_mesh_lifetime61::Apply(e, m, {g, native}, s);
        break;
    case 0x822d3068u:
        break;
    default:
        throw std::runtime_error("cook lower");
    }
}
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(mode < 2 ? 0xa5 : 0);
        auto m = w.Memory();
        m.WriteU32(0x832df554, 0);
        m.WriteU32(0x83216624, Table);
        m.WriteU32(0x832df548, 0x83216624);
        m.WriteU32(Table + 12, Free | 1);
        m.WriteU32(Table + 20, Free | 3);
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x82000e0c, std::bit_cast<std::uint32_t>(std::numeric_limits<float>::max()));
        m.WriteU32(0x82000d64, std::bit_cast<std::uint32_t>(-std::numeric_limits<float>::max()));
        m.WriteU32(0x82000e40, std::bit_cast<std::uint32_t>(0.01f));
        if (mode == 2) {
            m.WriteU32(Owner + 80, Arrays);
            for (unsigned off : {20u, 16u, 4u, 12u})
                m.WriteU32(Arrays + off, 0x50000 + off * 256u);
            m.WriteU32(Owner + 8 + 32, 0x60000);
            m.WriteU32(Owner + 8 + 24, 0x61004);
            m.WriteU32(Owner + 288, Other);
            m.WriteU32(Other, OtherTable);
            m.WriteU32(OtherTable, Delete | 1);
            m.WriteU32(Owner + 156 + 72, 0x62000);
            m.WriteU32(Owner + 156 + 88, 0x63000);
        }
        if (mode == 4) {
            m.WriteU32(Temporary + 8, 0x70000);
            m.WriteU32(Temporary + 20, 0x71000);
        }
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = Owner;
    s.r[4] = Temporary;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    Guest expected, actual;
    auto om = before.Memory();
    guest = &expected;
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (mode == 0)
        __imp__sub_82BC58A0(c, before.Bytes());
    else if (mode == 1)
        __imp__sub_82B9E4B0(c, before.Bytes());
    else if (mode < 4)
        __imp__sub_82B9E518(c, before.Bytes());
    else
        __imp__sub_82BA01C0(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    guest = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    auto entry = mode == 0   ? 0x82bc58a0u
                 : mode == 1 ? 0x82b9e4b0u
                 : mode < 4  ? 0x82b9e518u
                             : 0x82ba01c0u;
    if (!mesh_cook_storage61::Apply(entry, m, {actual, native}, s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events ||
        host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("mesh cook storage Full72/RAM/host/callback mismatch");
    if (mode < 2) {
        if (m.ReadU32(Owner) != 0x820d5d30u || m.ReadU32(Owner + 8) != 0x820d6c34u ||
            m.ReadU32(Owner + 156) != 0x820d6970u || m.ReadU32(Owner + 156 + 80) != Owner + 160 ||
            m.ReadU32(Owner + 288) || m.ReadU32(Owner + 344) || s.r[3] != Owner)
            throw std::runtime_error("mesh constructor shape");
    } else if (mode < 4) {
        std::vector<GuestAddress> wanted;
        if (mode == 2) {
            for (unsigned off : {20u, 16u, 4u, 12u})
                wanted.push_back(0x50000 + off * 256u);
            for (auto ptr : {Arrays, 0x60000u, 0x61000u, Other, 0x63000u, 0x62000u})
                wanted.push_back(ptr);
        }
        if (actual.freed != wanted || m.ReadU32(Owner) != 0x820d67a0u || m.ReadU32(Owner + 80) ||
            m.ReadU32(Owner + 288))
            throw std::runtime_error("mesh cleanup order");
    } else if (actual.freed != std::vector<GuestAddress>{0x70000, 0x71000} ||
               m.ReadU32(Temporary + 8) || m.ReadU32(Temporary + 20) || s.r[3])
        throw std::runtime_error("temporary mesh cleanup");
}
} // namespace cook_storage_oracle
void CookStorageIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cook_storage_oracle::guest->CallIndirect(e, *cook_storage_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void CookStorageLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    cook_storage_oracle::Lower(e, *cook_storage_oracle::memory, *cook_storage_oracle::guest, s);
    crt_full_oracle::ToPpc(c, s);
}
void CookStorageSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cook_storage_oracle::memory;
    for (unsigned i = 29; i < 32; ++i)
        WriteU64(m, Address(s.r[1] - 16u - 8u * (31u - i)), s.r[i]);
    m.WriteU32(Address(s.r[1] - 8u), Address(s.r[12]));
}
void CookStorageRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *cook_storage_oracle::memory;
    for (unsigned i = 29; i < 32; ++i)
        s.r[i] = ReadU64(m, Address(s.r[1] - 16u - 8u * (31u - i)));
    s.r[12] = m.ReadU32(Address(s.r[1] - 8u));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 5; ++mode)
            cook_storage_oracle::Check(mode);
        std::puts("PASS mesh-cook-storage61 5 original-upper/shared-component cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
