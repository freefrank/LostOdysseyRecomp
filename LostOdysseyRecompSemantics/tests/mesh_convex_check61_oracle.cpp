#include "crt_full_context_oracle_fixture.h"
#include "lo_semantics/mesh_convex_check61.h"
namespace convex_oracle {
using Registers = mesh_convex_check61::Registers;
constexpr GuestAddress Tri = 0x30000, Points = 0x31000, Point = 0x32000, Owner = 0x33000,
                       Mesh = 0x34000, Polygon = 0x35000, Bytes = 0x36000;
constexpr std::array<test::Region, 3> Regions{
    {{0, 0x120000}, {0x82000000, 0x10000}, {0x82210000, 0x10000}}};
GuestMemory *memory = nullptr;
struct Native final : float_triplet_transfer::NativeServices {
    void SetHostFpControl(std::uint32_t v) override { PPCFPSCRRegister{}.setcsr(v); }
};
void Check(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Tri, 0);
        m.WriteU32(Tri + 4, mode == 1 ? 0 : 1);
        m.WriteU32(Tri + 8, mode == 2 ? 1 : mode == 3 ? 0 : 2);
        constexpr float points[]{0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, -1};
        for (unsigned i = 0; i < 12; ++i)
            m.WriteU32(Points + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        m.WriteU32(Point + 8, std::bit_cast<std::uint32_t>(mode == 6 ? -1.f : 1.f));
        m.WriteU32(Owner + 4, Mesh);
        m.WriteU32(Mesh + 16, mode == 10 ? 0 : Points);
        m.WriteU32(Mesh + 8, Tri);
        m.WriteU32(Mesh + 40, Polygon);
        m.WriteU32(Mesh + 36, 1);
        m.WriteU32(Mesh + 12, 4);
        m.WriteU16(Polygon, 3);
        m.WriteU32(Polygon + 4, Bytes);
        for (unsigned i = 0; i < 3; ++i)
            m.WriteU8(Bytes + i, i);
        m.WriteU32(Polygon + 20, std::bit_cast<std::uint32_t>(1.f));
        if (mode == 9)
            m.WriteU32(Points + 44, std::bit_cast<std::uint32_t>(1.f));
        m.WriteU32(0x82007784, std::bit_cast<std::uint32_t>(1.f));
        if (mode >= 11 && mode != 13)
            m.WriteU32(Points + 44, std::bit_cast<std::uint32_t>(1.f));
        if (mode == 14)
            m.WriteU32(Points + 56, std::bit_cast<std::uint32_t>(2.f));
        m.WriteU32(0x82000b58, std::bit_cast<std::uint32_t>(.01f));
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = 0x3ff0000000000000ull + i;
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = mode >= 8 ? Owner : Tri;
    s.r[4] = mode == 7 ? 0 : Points;
    s.r[5] = Point;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    if (mode >= 11) {
        s.r[3] = mode == 14 ? 5 : 4;
        s.r[4] = Points;
        s.r[5] = 1;
        s.r[6] = Tri;
        s.r[7] = (mode == 12 || mode == 14) ? 1 : 0;
    }
    auto entry = mode >= 11  ? 0x82bb86c8u
                 : mode == 0 ? 0x82bd8fa8u
                 : mode < 5  ? 0x82bd91e0u
                 : mode < 8  ? 0x82bd90a0u
                             : 0x82bb8e88u;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    auto om = before.Memory();
    memory = &om;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    switch (entry) {
    case 0x82bb86c8u:
        __imp__sub_82BB86C8(c, before.Bytes());
        break;
    case 0x82bd8fa8u:
        __imp__sub_82BD8FA8(c, before.Bytes());
        break;
    case 0x82bd91e0u:
        __imp__sub_82BD91E0(c, before.Bytes());
        break;
    case 0x82bd90a0u:
        __imp__sub_82BD90A0(c, before.Bytes());
        break;
    default:
        __imp__sub_82BB8E88(c, before.Bytes());
    }
    auto host = PPCFPSCRRegister{}.getcsr();
    auto m = after.Memory();
    Native native;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_convex_check61::Apply(entry, m, native, s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "convex%u Full%d RAM%d CSR%d\n", mode, a == b,
                     before.EqualCommitted(after), host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("convex primitive mismatch");
    }
    if (mode >= 11) {
        auto flipped = mode == 12 || mode == 14;
        if (s.r[3] != (mode == 13 ? 1u : 0u) || m.ReadU32(Tri + 4) != (flipped ? 2u : 1u) ||
            m.ReadU32(Tri + 8) != (flipped ? 1u : 2u))
            throw std::runtime_error("centroid orientation");
        return;
    }
    if (mode == 0) {
        if (m.ReadU32(Tri + 4) != 2 || m.ReadU32(Tri + 8) != 1)
            throw std::runtime_error("triangle flip");
    } else {
        auto expected = (mode < 4 || mode == 5 || mode == 8) ? 1u : 0u;
        if (s.r[3] != expected)
            throw std::runtime_error("triangle/halfspace result");
    }
}

void Collapse(unsigned mode) {
    struct Restore {
        std::uint32_t csr = PPCFPSCRRegister{}.getcsr();
        ~Restore() { PPCFPSCRRegister{}.setcsr(csr); }
    } restore;
    test::GuestWindow before(Regions), after(Regions);
    // First face is collinear; each mode selects a different shortest edge.
    float points[]{0, 0, 0, 1, 0, 0, 3, 0, 0, 0, 1, 0, 0, 0, 1};
    if (mode == 1)
        points[6] = .25f;
    if (mode == 2)
        points[3] = 2.75f;
    if (mode == 4)
        points[7] = 1.f;
    unsigned count = mode == 5 ? 0 : mode == 6 ? 1 : 6;
    auto seed = [&](test::GuestWindow &w) {
        w.Fill(0);
        auto m = w.Memory();
        m.WriteU32(Owner, count);
        m.WriteU32(0x82215748, std::bit_cast<std::uint32_t>(.001f));
        for (unsigned i = 0; i < 15; ++i)
            m.WriteU32(Points + 4 * i, std::bit_cast<std::uint32_t>(points[i]));
        constexpr unsigned faces[]{0, 1, 2, 0, 3, 4, 0, 3, 4, 0, 3, 4, 0, 3, 4, 0, 3, 4};
        for (unsigned i = 0; i < 18; ++i)
            m.WriteU32(Tri + 4 * i, faces[i]);
    };
    seed(before);
    seed(after);
    Registers s{};
    for (unsigned i = 0; i < 32; ++i) {
        s.r[i] = 0x1122334400000000ull + i;
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(i + 1));
    }
    s.r[1] = 0x8877665500080000ull;
    s.r[3] = Owner;
    s.r[4] = Tri;
    s.r[5] = Points;
    s.r[6] = mode == 3 ? 0 : 1;
    s.lr = 0x9988776681234567ull;
    s.xer_so = 1;
    s.cached_fp_control = 0x9fc0;
    auto om = before.Memory(), m = after.Memory();
    memory = &om;
    PPCContext c{};
    crt_full_oracle::ToPpc(c, s);
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    __imp__sub_82BB88A8(c, before.Bytes());
    auto host = PPCFPSCRRegister{}.getcsr();
    Native native;
    PPCFPSCRRegister{}.setcsr(s.cached_fp_control);
    (void)mesh_convex_check61::Apply(0x82bb88a8u, m, native, s);
    auto a = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(c)),
         b = crt_full_oracle::Snapshot(s);
    if (a != b || !before.EqualCommitted(after) || host != PPCFPSCRRegister{}.getcsr()) {
        std::fprintf(stderr, "collapse%u Full%d RAM%d host%d\n", mode, a == b,
                     before.EqualCommitted(after), host == PPCFPSCRRegister{}.getcsr());
        for (unsigned i = 0; i < a.size(); ++i)
            if (a[i] != b[i])
                std::fprintf(stderr, "field%u %llx/%llx\n", i, (unsigned long long)a[i],
                             (unsigned long long)b[i]);
        throw std::runtime_error("collapse original mismatch");
    }
    if (s.r[3] != (mode < 3 || mode == 4 ? 1u : 0u) || m.ReadU32(Owner) != (mode < 3    ? 5
                                                                            : mode == 6 ? 0
                                                                                        : count))
        throw std::runtime_error("collapse count/result");
    if (mode < 3)
        for (unsigned face = 0; face < 5; ++face) {
            if (m.ReadU32(Tri + 12 * face + 4) != 3 || m.ReadU32(Tri + 12 * face + 8) != 4)
                throw std::runtime_error("retained face indices");
        }
    memory = nullptr;
}
} // namespace convex_oracle
void ConvexSave(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    for (unsigned i = first; i < 32; ++i)
        WriteU64(*convex_oracle::memory, Address(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
    convex_oracle::memory->WriteU32(Address(s.r[1] - 8), Address(s.r[12]));
}
void ConvexRestore(unsigned first, PPCContext &c, std::uint8_t *) {
    auto s = crt_full_oracle::FromPpc(c);
    for (unsigned i = first; i < 32; ++i)
        s.r[i] = ReadU64(*convex_oracle::memory, Address(s.r[1] - 16 - 8 * (31 - i)));
    s.r[12] = convex_oracle::memory->ReadU32(Address(s.r[1] - 8));
    s.lr = s.r[12];
    crt_full_oracle::ToPpc(c, s);
}
int main() {
    try {
        for (unsigned i = 0; i < 15; ++i)
            convex_oracle::Check(i);
        for (unsigned i = 0; i < 7; ++i)
            convex_oracle::Collapse(i);
        std::puts("PASS mesh-convex-check61 22 original local-chain / indexed triangle and "
                  "halfspace cases");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
