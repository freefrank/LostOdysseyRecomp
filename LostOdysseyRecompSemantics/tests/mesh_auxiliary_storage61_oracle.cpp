#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_auxiliary_storage61.h"
#include "lo_semantics/object_sort_support61.h"
namespace mesh_aux_oracle {
using Registers = mesh_auxiliary_storage61::Registers;
constexpr GuestAddress Owner = 0x30000, Table = 0x31000, Descriptor = 0x60000, Free = 0x2000;
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
        throw std::runtime_error("unexpected aux direct");
    }
    void CallIndirect(GuestAddress e, GuestMemory &, Registers &s) override {
        std::array<std::uint64_t, 73> event{};
        auto snap = crt_full_oracle::Snapshot(s);
        std::copy(snap.begin(), snap.end(), event.begin());
        event.back() = e;
        events.push_back(event);
        if (e != Free)
            throw std::runtime_error("unexpected aux free");
        freed.push_back(Address(s.r[4]));
        s.r[3] = 0;
        s.r[8] ^= 0x1234u;
        s.cr7.eq ^= 1u;
    }
};
Guest *guest = nullptr;
GuestMemory *memory = nullptr;
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    const auto seed = [&](test::GuestWindow &w) {
        w.Fill(mode ? 0 : 0xa5);
        auto m = w.Memory();
        m.WriteU32(0x82000e50, 0);
        m.WriteU32(0x832df554, 0);
        m.WriteU32(0x83216624, Table);
        m.WriteU32(Table + 12, Free | 1);
        if (mode == 1) {
            for (unsigned off : {64u, 68u, 60u, 56u, 48u, 44u, 40u, 8u, 20u, 16u, 100u, 104u})
                m.WriteU32(Owner + off, 0x40000u + off * 256u);
            m.WriteU32(Owner + 76, Descriptor);
        }
        if (mode == 2) {
            m.WriteU32(Owner + 72, 0x70000);
            m.WriteU32(Owner + 88, 0x71000);
            m.WriteU32(Owner + 76, Descriptor);
            m.WriteU32(Owner + 64, 0x70008);
            m.WriteU32(Owner + 100, 0x71008);
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
    if (!mode)
        __imp__sub_82BC8588(c, before.Bytes());
    else
        __imp__sub_82BC85F0(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    guest = nullptr;
    memory = nullptr;
    auto m = after.Memory();
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    if (!mesh_auxiliary_storage61::Apply(mode ? 0x82bc85f0u : 0x82bc8588u, m, {actual, native},
                                         s) ||
        crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)) != crt_full_oracle::Snapshot(s) ||
        !before.EqualCommitted(after) || expected.events != actual.events ||
        host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("mesh auxiliary Full72/RAM/host/callback mismatch");
    if (mode == 0) {
        if (m.ReadU32(Owner) != 0x820d6970u || m.ReadU32(Owner + 80) != Owner + 4u ||
            m.ReadU32(Owner + 84) != 0xa5a5a5a5u)
            throw std::runtime_error("aux constructor layout");
        for (unsigned off = 88; off < 132; off += 4)
            if (m.ReadU32(Owner + off))
                throw std::runtime_error("aux zeros");
    } else {
        std::vector<GuestAddress> wanted;
        if (mode == 1) {
            for (unsigned off : {104u, 100u, 64u, 68u, 60u, 56u, 48u, 44u, 40u, 8u, 20u, 16u})
                wanted.push_back(0x40000u + off * 256u);
            wanted.push_back(Descriptor);
        }
        if (mode == 2)
            wanted = {0x71000, 0x70000, Descriptor};
        if (actual.freed != wanted || m.ReadU32(Owner) != 0x820d694cu)
            throw std::runtime_error("aux release ordering/base");
        if (mode == 2 && (m.ReadU32(Owner + 64) != 0x70008 || m.ReadU32(Owner + 100) != 0x71008))
            throw std::runtime_error("aggregate interior pointers should remain untouched");
    }
}
} // namespace mesh_aux_oracle
void MeshAuxIndirect(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    mesh_aux_oracle::guest->CallIndirect(e, *mesh_aux_oracle::memory, s);
    crt_full_oracle::ToPpc(c, s);
}
void MeshAuxLower(std::uint32_t e, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    if (e == 0x82bd0798u)
        (void)crt_close_recursive_buffer_context::Apply(e, *mesh_aux_oracle::memory,
                                                        *mesh_aux_oracle::guest, s);
    else
        (void)object_sort_support61::Apply(e, *mesh_aux_oracle::memory,
                                           {*mesh_aux_oracle::guest, mesh_aux_oracle::native}, s);
    crt_full_oracle::ToPpc(c, s);
}
void MeshAuxSave(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mesh_aux_oracle::memory;
    for (unsigned i = 29; i < 32; ++i)
        WriteU64(m, Address(s.r[1] - 16u - 8u * (31u - i)), s.r[i]);
    m.WriteU32(Address(s.r[1] - 8u), Address(s.r[12]));
}
void MeshAuxRestore(PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    auto &m = *mesh_aux_oracle::memory;
    for (unsigned i = 29; i < 32; ++i)
        s.r[i] = ReadU64(m, Address(s.r[1] - 16u - 8u * (31u - i)));
    s.r[12] = m.ReadU32(Address(s.r[1] - 8u));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned mode = 0; mode < 4; ++mode)
            mesh_aux_oracle::Check(mode);
        std::puts("PASS mesh-auxiliary-storage61 4 composed-original cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
