#include "lo_semantics/mesh_vertex_dedup61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/global_assignments.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
namespace lo::semantic::gpu::mesh_vertex_dedup61 {
void Initialize(GuestMemory &m, Registers &s) {
    using R = GlobalAssignmentRegister;
    GlobalAssignmentRegisters g{};
    g.r3 = s.r[3];
    g.r4 = s.r[4];
    g.r5 = s.r[5];
    const ConstantRegisterValue zero[] = {{R::R11, 0}};
    const AssignmentFieldWrite writes[] = {{AssignmentAddressKind::ObjectField, R::R3, 0, 4,
                                            AssignmentWidth::Word,
                                            AssignmentValueKind::InputRegister, R::R4, 0, zero},
                                           {AssignmentAddressKind::ObjectField,
                                            R::R3,
                                            0,
                                            0,
                                            AssignmentWidth::Word,
                                            AssignmentValueKind::InputRegister,
                                            R::R5,
                                            0,
                                            {}},
                                           {AssignmentAddressKind::ObjectField,
                                            R::R3,
                                            0,
                                            8,
                                            AssignmentWidth::Word,
                                            AssignmentValueKind::InputRegister,
                                            R::R11,
                                            0,
                                            {}},
                                           {AssignmentAddressKind::ObjectField,
                                            R::R3,
                                            0,
                                            12,
                                            AssignmentWidth::Word,
                                            AssignmentValueKind::InputRegister,
                                            R::R11,
                                            0,
                                            {}},
                                           {AssignmentAddressKind::ObjectField,
                                            R::R3,
                                            0,
                                            16,
                                            AssignmentWidth::Word,
                                            AssignmentValueKind::InputRegister,
                                            R::R11,
                                            0,
                                            {}}};
    WriteFieldAssignments(m, g, writes, {});
    s.r[11] = g.r11;
}
namespace {
using recovery_abi::Address;
struct Dedup {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Integer(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void Load(std::uint64_t p) {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
        s.fpr_bits[0] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Store(std::uint64_t p) {
        Word(p, std::bit_cast<std::uint32_t>(float(std::bit_cast<double>(s.fpr_bits[0]))));
    }
    void Enter(unsigned first, unsigned frame, GuestAddress continuation = 0) {
        auto &r = s.r;
        r[12] = s.lr;
        if (continuation)
            s.lr = continuation;
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= frame;
        Word(r[1], old);
    }
    void Leave(unsigned first, unsigned frame) {
        auto &r = s.r;
        r[1] += frame;
        for (unsigned i = first; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Allocator(GuestAddress continuation) {
        s.lr = continuation;
        (void)crt_close_recursive_buffer_context::Apply(0x82bd0798u, m, d.sort.guest, s);
    }
    void Call(unsigned target, GuestAddress continuation) {
        s.ctr = s.r[target];
        s.lr = continuation;
        d.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void Free(unsigned offset, GuestAddress allocator, GuestAddress invoke) {
        auto &r = s.r;
        Allocator(allocator);
        r[11] = Word(r[3]);
        r[4] = Word(r[31] + offset);
        r[11] = Word(r[11] + 12);
        Call(11, invoke);
    }
    void Cleanup() {
        Enter(30, 112);
        auto &r = s.r;
        r[31] = r[3];
        r[30] = 0;
        r[11] = Word(r[31] + 16);
        Integer(r[11]);
        if (!s.cr6.eq) {
            Free(16, 0x82bc2d74u, 0x82bc2d88u);
            Word(r[31] + 16, r[30]);
        }
        r[11] = Word(r[31] + 12);
        Integer(r[11]);
        if (!s.cr6.eq) {
            Free(12, 0x82bc2d9cu, 0x82bc2db0u);
            Word(r[31] + 12, r[30]);
        }
        r[3] = r[31];
        Leave(30, 112);
    }
    void Gather(unsigned axis) {
        auto &r = s.r;
        r[11] = Word(r[31]);
        r[9] = 0;
        Integer(r[11]);
        if (!s.cr6.gt)
            return;
        r[11] = 0;
        r[10] = r[30];
        do {
            r[8] = Word(r[31] + 4);
            ++r[9];
            if (axis == 0) {
                Load(r[11] + r[8]);
                r[11] += 12;
            } else {
                r[8] += r[11];
                r[11] += 12;
                Load(r[8] + 4 * axis);
            }
            Store(r[10]);
            r[8] = Word(r[31]);
            r[10] += 4;
            Integer(r[9], r[8]);
        } while (s.cr6.lt);
    }
    void Sort(unsigned axis) {
        auto &r = s.r;
        r[6] = 1;
        if (axis == 0) {
            r[4] = r[30];
            r[5] = Word(r[31]);
        } else {
            r[5] = Word(r[31]);
            r[4] = r[30];
        }
        r[3] = r[1] + 96;
        s.lr = axis == 0 ? 0x82bc2ea4u : axis == 1 ? 0x82bc2ef8u : 0x82bc2f4cu;
        (void)crt_reader_bucket_sort61::Apply(0x82bd2df0u, m, d.sort, s);
    }
    void BuildBody() {
        auto &r = s.r;
        r[31] = r[3];
        r[28] = r[4];
        s.lr = 0x82bc2de8u;
        Cleanup();
        Allocator(0x82bc2decu);
        r[11] = Word(r[31]);
        r[5] = 43;
        r[4] = Address(r[11]) << 2;
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(11, 0x82bc2e08u);
        Integer(r[3]);
        Word(r[31] + 16, r[3]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        Allocator(0x82bc2e24u);
        r[11] = Word(r[31]);
        r[5] = 1;
        r[4] = Address(r[11]) << 2;
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(11, 0x82bc2e40u);
        r[30] = r[3];
        Integer(r[30]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        Gather(0);
        r[3] = r[1] + 96;
        s.lr = 0x82bc2e90u;
        (void)object_sort_support61::Apply(0x82bd2c50u, m, {d.sort.guest, d.fp}, s);
        Sort(0);
        Gather(1);
        Sort(1);
        Gather(2);
        Sort(2);
        r[29] = Word(r[3] + 4);
        Allocator(0x82bc2f54u);
        r[11] = Word(r[3]);
        r[4] = r[30];
        r[11] = Word(r[11] + 12);
        Call(11, 0x82bc2f68u);
        r[11] = ~std::uint64_t(0);
        r[30] = r[1] + 80;
        for (unsigned offset : {80u, 84u, 88u})
            Word(r[1] + offset, r[11]);
        r[11] = 0;
        Word(r[31] + 8, r[11]);
        Allocator(0x82bc2f88u);
        r[11] = Word(r[31]);
        r[5] = 44;
        r[9] = Word(r[3]);
        r[10] = Address(r[11]) << 1;
        r[11] += r[10];
        r[10] = Word(r[9]);
        r[4] = Address(r[11]) << 2;
        Call(10, 0x82bc2facu);
        r[9] = Word(r[31]);
        Word(r[31] + 12, r[3]);
        Integer(r[9]);
        if (!s.cr6.eq) {
            do {
                r[8] = Word(r[29]);
                --r[9];
                r[11] = Word(r[31] + 4);
                r[29] += 4;
                r[10] = Address(r[8]) << 1;
                r[6] = Word(r[30]);
                r[10] += r[8];
                r[7] = Address(r[10]) << 2;
                r[11] += r[7];
                r[10] = Word(r[11]);
                Integer(r[10], r[6]);
                if (s.cr6.eq) {
                    r[10] = Word(r[11] + 4);
                    r[6] = Word(r[30] + 4);
                    Integer(r[10], r[6]);
                    if (s.cr6.eq) {
                        r[10] = Word(r[11] + 8);
                        r[6] = Word(r[30] + 8);
                        Integer(r[10], r[6]);
                    }
                }
                if (!s.cr6.eq) {
                    r[10] = Word(r[31] + 8);
                    r[6] = Word(r[31] + 12);
                    r[5] = Address(r[10]) << 1;
                    r[4] = r[10] + 1;
                    r[10] += r[5];
                    r[10] = Address(r[10]) << 2;
                    r[10] += r[6];
                    Word(r[31] + 8, r[4]);
                    for (unsigned axis = 0; axis < 3; ++axis) {
                        Load(r[11] + 4 * axis);
                        Store(r[10] + 4 * axis);
                    }
                }
                r[11] = Word(r[31] + 8);
                r[10] = Address(r[8]) << 2;
                r[8] = Word(r[31] + 16);
                Integer(r[9]);
                r[6] = r[11] - 1;
                r[11] = Word(r[31] + 4);
                r[30] = r[7] + r[11];
                Word(r[10] + r[8], r[6]);
            } while (!s.cr6.eq);
        }
        Integer(r[28]);
        if (!s.cr6.eq) {
            r[11] = Word(r[31] + 16);
            Word(r[28] + 8, r[11]);
            r[11] = Word(r[31] + 8);
            Word(r[28] + 4, r[11]);
            r[11] = Word(r[31] + 12);
            Word(r[28], r[11]);
        }
        r[3] = r[1] + 96;
        s.lr = 0x82bc3090u;
        (void)crt_reader_follow61::Apply(0x82bd2c78u, m, d.sort.guest, s);
        r[3] = 1;
    }
    void Build() {
        Enter(28, 160, 0x82bc2dd8u);
        BuildBody();
        Leave(28, 160);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Dedup x{m, d, s};
    switch (e) {
    case 0x82bc2d48u:
    case 0x82bc38e0u:
        x.Cleanup();
        break;
    case 0x82bc2dd0u:
        x.Build();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_vertex_dedup61
