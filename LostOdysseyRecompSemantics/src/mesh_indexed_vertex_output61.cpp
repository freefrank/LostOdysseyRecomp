#include "lo_semantics/mesh_indexed_vertex_output61.h"
#include "lo_semantics/crt_reader_chain61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/mesh_geometry_math61.h"
#include "lo_semantics/mesh_indexed_channels61.h"
#include "lo_semantics/mesh_indexed_compact61.h"
#include "lo_semantics/mesh_indexed_normals61.h"
#include "lo_semantics/mesh_indexed_remap61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>
#include <cmath>
namespace lo::semantic::gpu::mesh_indexed_vertex_output61 {
namespace {
using recovery_abi::Address;
struct Output {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    static std::uint32_t Shift(std::uint64_t v, unsigned n) { return Address(v) << n; }
    void Compare(std::uint64_t a, std::uint64_t b = 0) {
        auto x = Address(a), y = Address(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    void FCompare(double a, double b) {
        s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b),
                 std::uint8_t(std::isnan(a) || std::isnan(b))};
    }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    double F(unsigned i) { return std::bit_cast<double>(s.fpr_bits[i]); }
    void Single(unsigned i, double v) {
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(float(v)));
    }
    void Load(unsigned i, std::uint64_t p) {
        Gradual();
        s.fpr_bits[i] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(Word(p))));
    }
    void Store(unsigned i, std::uint64_t p) { Word(p, std::bit_cast<std::uint32_t>(float(F(i)))); }
    void Lower(GuestAddress e, GuestAddress c) {
        s.lr = c;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.sort.guest, s);
            break;
        case 0x82bd2c50u:
            (void)object_sort_support61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82bd2c78u:
            (void)crt_reader_follow61::Apply(e, m, d.sort.guest, s);
            break;
        case 0x82bd2df0u:
            (void)crt_reader_bucket_sort61::Apply(e, m, d.sort, s);
            break;
        case 0x82bc0108u:
            Run();
            break;
        case 0x82bbdf60u:
        case 0x82bbf628u:
        case 0x82bbe310u:
        case 0x82bbf590u:
            (void)mesh_indexed_workspace61::Apply(e, m, d, s);
            break;
        case 0x82bc0ba8u:
            Pipeline();
            break;
        case 0x82bc0930u:
            Groups();
            break;
        case 0x82b7bc40u:
            crt_reader_chain61::ApplySupport_B7BC40(m, d.sort.accepted, s);
            break;
        case 0x82bc0058u:
            (void)mesh_indexed_compact61::Apply(e, m, d, s);
            break;
        case 0x82bbed20u:
            (void)mesh_indexed_normals61::Apply(e, m, d, s);
            break;
        case 0x82bbe948u:
        case 0x82bbebe0u:
        case 0x82bbf208u:
            (void)mesh_indexed_channels61::Apply(e, m, d, s);
            break;
        case 0x82bd2870u:
            (void)reader_buffer_growth61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82bd2a08u:
        case 0x82bd2c08u:
            (void)object_sort_support61::Apply(e, m, {d.sort.guest, d.fp}, s);
            break;
        case 0x82bb3c00u:
            (void)mesh_indexed_channels61::Apply(e, m, d, s);
            break;
        case 0x82bbfea8u:
            (void)mesh_indexed_remap61::Apply(e, m, d, s);
            break;
        case 0x822da388u:
            (void)mesh_geometry_math61::Apply(e, m, d.fp, s);
            break;
        }
    }
    void Call(GuestAddress c) {
        s.ctr = s.r[11];
        s.lr = c;
        d.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    void FreeArray(unsigned reg, GuestAddress allocator, GuestAddress call) {
        Lower(0x82bd0798u, allocator);
        s.r[11] = Word(s.r[3]);
        s.r[4] = s.r[reg];
        s.r[11] = Word(s.r[11] + 12);
        Call(call);
    }
    // Stable sorting by smoothing mask, then face label, preserves input
    // ordering within each equal-key batch. Emission's return is ignored.
    void Groups() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bc0938u;
        for (unsigned i = 24; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= 176;
        Word(r[1], old);
        r[31] = r[3];
        Lower(0x82bd0798u, 0x82bc0944u);
        r[11] = Word(r[3]);
        r[5] = 1;
        r[10] = Word(r[31] + 224);
        r[4] = Shift(r[10], 2);
        r[11] = Word(r[11]);
        Call(0x82bc0960u);
        r[26] = r[3];
        Lower(0x82bd0798u, 0x82bc0968u);
        r[11] = Word(r[31] + 224);
        r[5] = 1;
        r[4] = Shift(r[11], 2);
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(0x82bc0984u);
        r[28] = r[3];
        Lower(0x82bd0798u, 0x82bc098cu);
        r[11] = Word(r[31] + 224);
        r[5] = 1;
        r[4] = Shift(r[11], 2);
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(0x82bc09a8u);
        r[29] = r[3];
        Compare(r[26]);
        bool failed = s.cr6.eq;
        if (!failed) {
            Compare(r[28]);
            failed = s.cr6.eq;
        }
        if (!failed) {
            Compare(r[29]);
            failed = s.cr6.eq;
        }
        if (failed) {
            Compare(r[29]);
            if (!s.cr6.eq)
                FreeArray(29, 0x82bc0b48u, 0x82bc0b5cu);
            Compare(r[28]);
            if (!s.cr6.eq)
                FreeArray(28, 0x82bc0b68u, 0x82bc0b7cu);
            Compare(r[26]);
            if (!s.cr6.eq)
                FreeArray(26, 0x82bc0b88u, 0x82bc0b9cu);
            r[3] = 0;
        } else {
            r[11] = Word(r[31] + 224);
            r[9] = 0;
            Compare(r[11]);
            if (s.cr6.gt) {
                r[11] = 0;
                r[10] = r[29];
                r[7] = r[28] - r[29];
                do {
                    r[8] = Word(r[31] + 248);
                    ++r[9];
                    r[8] += r[11];
                    r[8] = Word(r[8] + 24);
                    Word(r[7] + r[10], r[8]);
                    r[8] = Word(r[31] + 248);
                    r[8] += r[11];
                    r[11] += 48;
                    r[8] = Word(r[8] + 28);
                    Word(r[10], r[8]);
                    r[10] += 4;
                    r[8] = Word(r[31] + 224);
                    Compare(r[9], r[8]);
                } while (s.cr6.lt);
            }
            r[3] = r[1] + 80;
            Lower(0x82bd2c50u, 0x82bc0a20u);
            r[30] = Word(r[31] + 224);
            r[6] = 1;
            r[4] = r[29];
            r[3] = r[1] + 80;
            r[5] = r[30];
            Lower(0x82bd2df0u, 0x82bc0a38u);
            r[4] = r[28];
            r[5] = r[30];
            r[6] = 1;
            Lower(0x82bd2df0u, 0x82bc0a48u);
            r[10] = Word(r[3] + 4);
            r[11] = Word(r[31] + 224);
            r[5] = 0;
            r[24] = 0;
            Compare(r[11]);
            // Original requires a nonempty input and reads the first rank here.
            r[11] = Shift(Word(r[10]), 2);
            r[6] = Word(r[11] + r[28]);
            r[7] = Word(r[11] + r[29]);
            if (s.cr6.gt) {
                r[25] = r[10];
                do {
                    r[27] = Word(r[25]);
                    r[30] = Shift(r[27], 2);
                    r[11] = Word(r[30] + r[28]);
                    Compare(r[11], r[6]);
                    if (s.cr6.eq) {
                        r[11] = Word(r[30] + r[29]);
                        Compare(r[11], r[7]);
                    }
                    if (s.cr6.eq) {
                        r[11] = Shift(r[5], 2);
                        ++r[5];
                        Word(r[11] + r[26], r[27]);
                    } else {
                        r[4] = r[26];
                        r[3] = r[31];
                        Lower(0x82bc0108u, 0x82bc0ab0u);
                        r[6] = Word(r[30] + r[28]);
                        r[7] = Word(r[30] + r[29]);
                        r[5] = 1;
                        Word(r[26], r[27]);
                    }
                    r[11] = Word(r[31] + 224);
                    ++r[24];
                    r[25] += 4;
                    Compare(r[24], r[11]);
                } while (s.cr6.lt);
            }
            r[4] = r[26];
            r[3] = r[31];
            Lower(0x82bc0108u, 0x82bc0ae0u);
            FreeArray(29, 0x82bc0ae4u, 0x82bc0af8u);
            FreeArray(28, 0x82bc0afcu, 0x82bc0b10u);
            FreeArray(26, 0x82bc0b14u, 0x82bc0b28u);
            r[3] = r[1] + 80;
            Lower(0x82bd2c78u, 0x82bc0b30u);
            r[3] = 1;
        }
        r[1] += 176;
        for (unsigned i = 24; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void SignedCompare(std::uint64_t a, std::uint64_t b) {
        auto x = std::int32_t(a), y = std::int32_t(b);
        s.cr6 = {std::uint8_t(x < y), std::uint8_t(x > y), std::uint8_t(x == y), s.xer_so};
    }
    // Per-label summary: label, face count, vertex count, smoothing-batch count.
    void LabelSummary(bool final) {
        auto &r = s.r;
        r[30] = r[31] + 192;
        Capacity(30);
        constexpr GuestAddress middle[]{0x82bc0d74u, 0x82bc0da8u, 0x82bc0ddcu, 0x82bc0e10u};
        constexpr GuestAddress last[]{0x82bc0e90u, 0x82bc0ec4u, 0x82bc0ef8u, 0x82bc0f2cu};
        constexpr unsigned values[]{22, 28, 27};
        for (unsigned i = 0; i < 3; ++i) {
            Grow(30, final ? last[i] : middle[i]);
            Push(30, values[i]);
            r[11] = Word(r[30] + 4);
            r[10] = Word(r[30]);
            ++r[11];
            Compare(r[11], r[10]);
            Word(r[30] + 4, r[11]);
        }
        Grow(30, final ? last[3] : middle[3]);
        if (final)
            Push(30, 25);
        else {
            r[11] = Word(r[30] + 4);
            r[10] = r[25];
            r[9] = Word(r[30] + 8);
            r[25] = 0;
            r[11] = Shift(r[11], 2);
            r[28] = 0;
            r[27] = 0;
            Word(r[11] + r[9], r[10]);
        }
        Increment(30);
    }
    bool PipelineBody() {
        auto &r = s.r;
        r[31] = r[3];
        r[29] = r[4];
        r[11] = Word(r[31] + 224);
        Compare(r[11]);
        if (s.cr6.eq)
            return false;
        Lower(0x82bd0798u, 0x82bc0bd8u);
        r[11] = Word(r[31] + 224);
        r[5] = 0;
        r[4] = Shift(r[11], 2);
        r[11] = Word(r[3]);
        r[11] = Word(r[11]);
        Call(0x82bc0bf4u);
        Compare(r[3]);
        Word(r[31] + 256, r[3]);
        if (s.cr6.eq)
            return false;
        r[11] = Word(r[31] + 224);
        r[4] = 255;
        r[5] = Shift(r[11], 2);
        Lower(0x82b7bc40u, 0x82bc0c10u);
        r[11] = 0;
        r[3] = r[31];
        Word(r[31] + 260, r[11]);
        constexpr GuestAddress stages[]{0x82bc0058u, 0x82bbe948u, 0x82bbebe0u,
                                        0x82bbed20u, 0x82bbf208u, 0x82bc0930u};
        constexpr GuestAddress returns[]{0x82bc0c20u, 0x82bc0c34u, 0x82bc0c48u,
                                         0x82bc0c5cu, 0x82bc0c70u, 0x82bc0c84u};
        for (unsigned i = 0; i < 6; ++i) {
            r[3] = r[31];
            Lower(stages[i], returns[i]);
            r[11] = r[3] & 255;
            Compare(r[11]);
            if (s.cr6.eq)
                return false;
        }
        r[11] = Word(r[31] + 8);
        r[10] = std::uint64_t(std::int64_t(-859045888));
        r[18] = Word(r[31] + 20);
        r[22] = ~std::uint64_t(0);
        r[17] = Word(r[31] + 164);
        r[9] = r[10] | 52429;
        r[28] = 0;
        r[27] = 0;
        Word(r[29] + 12, r[11]);
        r[25] = 0;
        r[11] = Word(r[31] + 24);
        r[21] = 0;
        r[20] = 0;
        Word(r[29] + 16, r[11]);
        r[10] = Word(r[31] + 152);
        Word(r[29] + 20, r[10]);
        r[10] = Word(r[31] + 184);
        Word(r[29] + 24, r[10]);
        constexpr unsigned source[]{40, 88, 56, 104}, dest[]{48, 60, 52, 64};
        for (unsigned i = 0; i < 4; ++i) {
            r[8] = Word(r[31] + source[i]);
            Word(r[29] + dest[i], r[8]);
        }
        r[8] = m.ReadU8(Address(r[31] + 281));
        m.WriteU8(Address(r[29] + 84), std::uint8_t(r[8]));
        constexpr unsigned source2[]{72, 120, 136, 168}, dest2[]{56, 68, 72, 80};
        for (unsigned i = 0; i < 4; ++i) {
            r[8] = Word(r[31] + source2[i]);
            Word(r[29] + dest2[i], r[8]);
        }
        r[8] = Word(r[31] + 180);
        r[9] = (std::uint64_t(Address(r[8])) * Address(r[9])) >> 32;
        r[9] = Address(r[9]) >> 2;
        Compare(r[9]);
        if (!s.cr6.eq) {
            r[23] = r[11];
            r[26] = r[10] + 12;
            r[24] = r[9];
            do {
                r[19] = Word(r[26] - 12);
                SignedCompare(r[19], r[22]);
                if (!s.cr6.eq) {
                    SignedCompare(r[22], ~std::uint64_t(0));
                    if (!s.cr6.eq)
                        LabelSummary(false);
                    r[22] = r[19];
                }
                r[11] = Word(r[23]);
                --r[24];
                r[10] = Word(r[26]);
                ++r[25];
                r[23] += 4;
                r[26] += 20;
                r[28] += r[11];
                r[27] += r[10];
                r[21] += r[11];
                r[20] += r[10];
                Compare(r[24]);
            } while (!s.cr6.eq);
        }
        LabelSummary(true);
        r[11] = Word(r[31] + 196);
        r[30] = Word(r[29] + 80);
        r[11] = Address(r[11]) >> 2;
        Compare(r[30]);
        Word(r[29] + 88, r[11]);
        r[11] = Word(r[31] + 200);
        Word(r[29], r[21]);
        Word(r[29] + 92, r[11]);
        r[11] = Word(r[31] + 208);
        Word(r[29] + 8, r[18]);
        Word(r[29] + 44, r[20]);
        Word(r[29] + 4, r[11]);
        r[11] = Word(r[31] + 212);
        Word(r[29] + 32, r[11]);
        r[11] = Word(r[31] + 216);
        Word(r[29] + 36, r[11]);
        r[11] = Word(r[31] + 220);
        Word(r[29] + 76, r[17]);
        Word(r[29] + 40, r[11]);
        if (!s.cr6.eq) {
            Lower(0x82bd0798u, 0x82bc0f9cu);
            r[11] = Word(r[31] + 224);
            r[5] = 0;
            r[4] = Shift(r[11], 2);
            r[11] = Word(r[3]);
            r[11] = Word(r[11]);
            Call(0x82bc0fb8u);
            r[28] = r[3];
            Compare(r[28]);
            if (s.cr6.eq)
                return false;
            r[10] = Word(r[31] + 260);
            r[11] = 0;
            Compare(r[10]);
            if (s.cr6.gt) {
                r[10] = 0;
                do {
                    r[8] = Word(r[31] + 256);
                    r[9] = r[11];
                    ++r[11];
                    r[8] = Word(r[8] + r[10]);
                    r[10] += 4;
                    r[8] = Shift(r[8], 2);
                    Word(r[8] + r[28], r[9]);
                    r[9] = Word(r[31] + 260);
                    Compare(r[11], r[9]);
                } while (s.cr6.lt);
            }
            r[11] = Word(r[31] + 276);
            r[10] = 0;
            Compare(r[11]);
            if (s.cr6.gt) {
                do {
                    r[11] = Word(r[30]);
                    r[30] += 4;
                    Compare(r[11]);
                    if (!s.cr6.eq) {
                        do {
                            r[9] = Word(r[30]);
                            --r[11];
                            r[9] = Shift(r[9], 2);
                            Compare(r[11]);
                            r[9] = Word(r[9] + r[28]);
                            Word(r[30], r[9]);
                            r[30] += 4;
                        } while (!s.cr6.eq);
                    }
                    r[11] = Word(r[31] + 276);
                    ++r[10];
                    Compare(r[10], r[11]);
                } while (s.cr6.lt);
            }
            FreeArray(28, 0x82bc1054u, 0x82bc1068u);
        }
        r[11] = Word(r[31] + 224);
        r[5] = 1;
        r[6] = 0;
        Compare(r[11]);
        if (s.cr6.gt) {
            r[10] = Word(r[31] + 256);
            r[11] = 0;
            do {
                r[9] = Word(r[10] + r[11]);
                r[8] = Word(r[31] + 248);
                r[7] = Shift(r[9], 1);
                r[9] += r[7];
                r[9] = Shift(r[9], 4);
                r[9] += r[8];
                r[9] = Word(r[9] + 44);
                Word(r[10] + r[11], r[9]);
                r[10] = Word(r[31] + 256);
                r[9] = Word(r[10] + r[11]);
                Compare(r[9], r[6]);
                if (!s.cr6.eq)
                    r[5] = 0;
                r[9] = Word(r[31] + 224);
                ++r[6];
                r[11] += 4;
                Compare(r[6], r[9]);
            } while (s.cr6.lt);
        }
        r[11] = r[5] & 255;
        Compare(r[11]);
        r[11] = 0;
        if (s.cr6.eq)
            r[11] = Word(r[31] + 256);
        r[3] = 1;
        Word(r[29] + 28, r[11]);
        return true;
    }
    void Pipeline() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bc0bb0u;
        for (unsigned i = 17; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= 208;
        Word(r[1], old);
        if (!PipelineBody())
            r[3] = 0;
        r[1] += 208;
        for (unsigned i = 17; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    bool IndexedInputBody() {
        auto &r = s.r;
        r[31] = 0;
        r[11] = r[1] + 80;
        r[27] = r[3];
        r[26] = r[4];
        r[28] = 1;
        r[24] = r[5];
        recovery_abi::WriteU64(m, Address(r[11]), r[31]);
        r[3] = r[1] + 272;
        recovery_abi::WriteU64(m, Address(r[11] + 8), r[31]);
        r[23] = r[6];
        for (unsigned off : {16, 24, 32})
            recovery_abi::WriteU64(m, Address(r[11] + off), r[31]);
        r[11] = Word(r[27]);
        Word(r[1] + 88, r[31]);
        Word(r[1] + 92, r[31]);
        Word(r[1] + 96, r[24]);
        Word(r[1] + 100, r[31]);
        Word(r[1] + 80, r[11]);
        r[11] = Word(r[26]);
        Word(r[1] + 104, r[31]);
        m.WriteU8(Address(r[1] + 108), std::uint8_t(r[28]));
        m.WriteU8(Address(r[1] + 109), std::uint8_t(r[31]));
        m.WriteU8(Address(r[1] + 110), std::uint8_t(r[31]));
        Word(r[1] + 84, r[11]);
        for (unsigned off : {111, 112, 117, 119})
            m.WriteU8(Address(r[1] + off), std::uint8_t(r[31]));
        for (unsigned off : {113, 114, 115, 116, 118})
            m.WriteU8(Address(r[1] + off), std::uint8_t(r[28]));
        Lower(0x82bbdf60u, 0x82bb9898u);
        r[4] = r[1] + 80;
        r[3] = r[1] + 272;
        Lower(0x82bbf628u, 0x82bb98a4u);
        r[11] = r[3] & 255;
        Compare(r[11]);
        if (s.cr6.eq)
            return false;
        r[11] = Word(r[26]);
        r[29] = r[31];
        Compare(r[11]);
        if (s.cr6.gt) {
            r[30] = r[23] + 8;
            r[25] = ~std::uint64_t(0);
            do {
                r[11] = Word(r[30] - 8);
                r[4] = r[1] + 144;
                r[3] = r[1] + 272;
                Word(r[1] + 144, r[29]);
                Word(r[1] + 148, r[25]);
                Word(r[1] + 152, r[28]);
                Word(r[1] + 160, r[31]);
                Word(r[1] + 120, r[11]);
                r[11] = Word(r[30] - 4);
                Word(r[1] + 164, r[31]);
                m.WriteU8(Address(r[1] + 168), std::uint8_t(r[31]));
                Word(r[1] + 124, r[11]);
                r[11] = Word(r[30]);
                Word(r[1] + 128, r[11]);
                r[11] = r[1] + 120;
                Word(r[1] + 156, r[11]);
                Lower(0x82bbe310u, 0x82bb9920u);
                r[11] = Word(r[26]);
                ++r[29];
                r[30] += 12;
                Compare(r[29], r[11]);
            } while (s.cr6.lt);
        }
        r[11] = r[1] + 176;
        r[5] = 56;
        r[4] = 0;
        r[3] = r[1] + 208;
        for (unsigned off : {0, 8, 16, 24})
            recovery_abi::WriteU64(m, Address(r[11] + off), r[31]);
        Lower(0x82b7bc40u, 0x82bb9958u);
        r[4] = r[1] + 176;
        r[3] = r[1] + 272;
        Word(r[1] + 264, r[31]);
        Word(r[1] + 268, r[31]);
        Lower(0x82bc0ba8u, 0x82bb996cu);
        r[11] = r[3] & 255;
        Compare(r[11]);
        if (s.cr6.eq)
            return false;
        r[11] = Word(r[1] + 220);
        r[8] = r[31];
        r[10] = Word(r[1] + 176);
        Word(r[27], r[11]);
        Word(r[26], r[10]);
        r[11] = Word(r[27]);
        Compare(r[11]);
        if (s.cr6.gt) {
            r[9] = Word(r[1] + 224);
            r[11] = r[24] + 8;
            r[6] = Word(r[1] + 236);
            do {
                r[10] = Word(r[9]);
                ++r[8];
                r[9] += 4;
                r[7] = Shift(r[10], 1);
                r[10] += r[7];
                r[10] = Shift(r[10], 2);
                r[10] += r[6];
                Load(0, r[10]);
                Store(0, r[11] - 8);
                Load(0, r[10] + 4);
                Store(0, r[11] - 4);
                Load(0, r[10] + 8);
                Store(0, r[11]);
                r[10] = Word(r[27]);
                r[11] += 12;
                Compare(r[8], r[10]);
            } while (s.cr6.lt);
        }
        r[11] = Word(r[26]);
        Compare(r[11]);
        if (s.cr6.gt) {
            r[10] = Word(r[1] + 188);
            r[11] = r[23] + 4;
            r[9] = r[10] - r[23];
            do {
                r[8] = Word(r[10]);
                ++r[31];
                Word(r[11] - 4, r[8]);
                r[8] = Word(r[9] + r[11]);
                Word(r[11], r[8]);
                r[8] = Word(r[10] + 8);
                r[10] += 12;
                Word(r[11] + 4, r[8]);
                r[11] += 12;
                r[8] = Word(r[26]);
                Compare(r[31], r[8]);
            } while (s.cr6.lt);
        }
        return true;
    }
    // Adapts mutable count pointers and packed xyz/u32 triangle arrays to the
    // temporary indexed workspace; copies only retained geometry back before
    // destroying all borrowed views and owned temporary buffers.
    void IndexedInput() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bb9808u;
        for (unsigned i = 23; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        auto old = r[1];
        r[1] -= 656;
        Word(r[1], old);
        bool ok = IndexedInputBody();
        r[3] = r[1] + 272;
        Lower(0x82bbf590u, ok ? 0x82bb9a38u : 0x82bb98b8u);
        r[3] = ok ? 1 : 0;
        r[1] += 656;
        for (unsigned i = 23; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
    void Capacity(unsigned desc) {
        auto &r = s.r;
        r[11] = Word(r[desc]);
        r[10] = Word(r[desc] + 4);
        Compare(r[10], r[11]);
    }
    void Grow(unsigned desc, GuestAddress c) {
        auto &r = s.r;
        if (s.cr6.eq) {
            r[4] = 1;
            r[3] = r[desc];
            Lower(0x82bd2870u, c);
        }
    }
    void Push(unsigned desc, unsigned value) {
        auto &r = s.r;
        r[11] = Word(r[desc] + 4);
        r[10] = Word(r[desc] + 8);
        r[11] = Shift(r[11], 2);
        Word(r[11] + r[10], r[value]);
    }
    void Increment(unsigned desc) {
        auto &r = s.r;
        r[11] = Word(r[desc] + 4);
        ++r[11];
        Word(r[desc] + 4, r[11]);
    }
    void BufferedFloat(bool earlyShift) {
        auto &r = s.r;
        r[11] = Word(r[30] + 4);
        r[10] = Word(r[1] + 80);
        if (earlyShift)
            r[11] = Shift(r[11], 2);
        r[9] = Word(r[30] + 8);
        if (!earlyShift)
            r[11] = Shift(r[11], 2);
        Word(r[11] + r[9], r[10]);
    }
    void FirstAttribute() {
        auto &r = s.r;
        Compare(r[10]); // The caller has already loaded the pointer, then tuple IDs.
        if (s.cr6.eq)
            return;
        r[11] = m.ReadU8(Address(r[31] + 286));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[30] = r[31] + 48;
            Capacity(30);
            Grow(30, 0x82bc0258u);
            Push(30, 29);
            Increment(30);
            return;
        }
        r[11] = Shift(r[29], 1);
        r[30] = r[31] + 96;
        r[11] += r[29];
        r[29] = Shift(r[11], 2);
        r[11] = Word(r[30]);
        Load(0, r[29] + r[10]);
        r[10] = Word(r[30] + 4);
        Store(0, r[1] + 80);
        Compare(r[10], r[11]);
        Grow(30, 0x82bc02a0u);
        BufferedFloat(true);
        r[11] = Word(r[30] + 4);
        r[10] = Word(r[30]);
        ++r[11];
        Compare(r[11], r[10]);
        Word(r[30] + 4, r[11]);
        r[11] = Word(r[31] + 240);
        r[11] += r[29];
        Load(0, r[11] + 4);
        Store(0, r[1] + 80);
        Grow(30, 0x82bc02e8u);
        BufferedFloat(false);
        Increment(30);
        r[11] = m.ReadU8(Address(r[31] + 281));
        Compare(r[11]);
        if (s.cr6.eq)
            return;
        r[11] = Word(r[31] + 240);
        r[10] = Word(r[30] + 4);
        r[11] += r[29];
        Load(0, r[11] + 8);
        r[11] = Word(r[30]);
        Store(0, r[1] + 80);
        Compare(r[10], r[11]);
        Grow(30, 0x82bc0340u);
        BufferedFloat(false);
        Increment(30);
    }
    void SecondAttribute() {
        auto &r = s.r;
        r[10] = Word(r[31] + 244);
        Compare(r[10]);
        if (s.cr6.eq)
            return;
        r[11] = m.ReadU8(Address(r[31] + 287));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[30] = r[31] + 64;
            Capacity(30);
            Grow(30, 0x82bc0398u);
            Push(30, 26);
            Increment(30);
        } else {
            r[11] = Shift(r[26], 1);
            r[3] = r[31] + 112;
            r[11] += r[26];
            r[11] = Shift(r[11], 2);
            r[4] = r[11] + r[10];
            Lower(0x82bb3c00u, 0x82bc03d0u);
        }
    }
    void WeightedNormal() {
        auto &r = s.r;
        r[10] = Word(r[11] + 12);
        r[7] = r[20];
        r[9] = Word(r[11] + 16);
        r[6] = r[20];
        r[3] = Shift(r[10], 1);
        r[8] = Word(r[11] + 20);
        r[11] = Word(r[31] + 252);
        r[4] = Shift(r[9], 1);
        r[10] += r[3];
        r[5] = Shift(r[8], 1);
        r[10] = Shift(r[10], 2);
        r[9] += r[4];
        r[8] += r[5];
        r[9] = Shift(r[9], 2);
        r[8] = Shift(r[8], 2);
        r[10] = Word(r[10] + r[11]);
        Compare(r[28], r[10]);
        Word(r[1] + 104, r[10]);
        r[10] = Word(r[9] + r[11]);
        r[11] = Word(r[8] + r[11]);
        Word(r[1] + 108, r[10]);
        Word(r[1] + 112, r[11]);
        if (s.cr6.eq) {
            r[7] = 2;
            r[6] = 1;
        } else {
            Compare(r[28], r[10]);
            if (s.cr6.eq) {
                r[7] = 2;
                r[6] = r[20];
            } else {
                Compare(r[28], r[11]);
                if (s.cr6.eq) {
                    r[7] = r[20];
                    r[6] = 1;
                }
            }
        }
        r[11] = Shift(r[7], 2);
        r[10] = Word(r[31] + 236);
        r[9] = r[1] + 104;
        r[8] = Shift(r[6], 2);
        r[7] = r[1] + 104;
        r[9] = Word(r[11] + r[9]);
        r[11] = Shift(r[28], 1);
        r[8] = Word(r[8] + r[7]);
        r[6] = Shift(r[9], 1);
        r[11] += r[28];
        r[7] = Shift(r[8], 1);
        r[9] += r[6];
        r[11] = Shift(r[11], 2);
        r[8] += r[7];
        r[9] = Shift(r[9], 2);
        r[11] += r[10];
        r[9] += r[10];
        r[8] = Shift(r[8], 2);
        r[10] += r[8];
        Load(7, r[11] + 4);
        Load(13, r[9] + 4);
        Load(8, r[11]);
        Single(13, F(13) - F(7));
        Load(0, r[9]);
        Single(0, F(0) - F(8));
        Load(7, r[11]);
        Load(11, r[10]);
        Load(8, r[11] + 8);
        Single(11, F(11) - F(7));
        Load(12, r[9] + 8);
        Load(7, r[11] + 8);
        Single(12, F(12) - F(8));
        Load(9, r[10] + 8);
        Load(8, r[11] + 4);
        Single(9, F(9) - F(7));
        Load(10, r[10] + 4);
        Single(10, F(10) - F(8));
        Single(7, F(13) * F(11));
        Single(8, F(9) * F(0));
        Single(5, F(10) * F(13));
        Single(6, F(10) * F(12));
        Single(10, F(10) * F(0) - F(7));
        Single(8, F(12) * F(11) - F(8));
        Single(12, F(9) * F(12) + F(5));
        Single(13, F(9) * F(13) - F(6));
        Single(2, F(11) * F(0) + F(12));
        Single(0, F(8) * F(8));
        Single(0, F(10) * F(10) + F(0));
        Single(0, F(13) * F(13) + F(0));
        Single(1, std::sqrt(F(0)));
        Lower(0x822da388u, 0x82bc05fcu);
        r[11] = Word(r[31] + 248);
        Single(0, F(1));
        r[11] += r[30];
        Load(13, r[11] + 32);
        Load(12, r[11] + 36);
        Load(11, r[11] + 40);
        Single(13, F(13) * F(0));
        Single(12, F(12) * F(0));
        Single(0, F(11) * F(0));
        Single(31, F(13) + F(31));
        Single(30, F(12) + F(30));
    }
    void SmoothNormal() {
        auto &r = s.r;
        r[11] = m.ReadU8(Address(r[31] + 282));
        Compare(r[11]);
        if (s.cr6.eq)
            return;
        r[11] = m.ReadU8(Address(r[31] + 284));
        s.fpr_bits[31] = s.fpr_bits[28];
        s.fpr_bits[30] = s.fpr_bits[28];
        Store(31, r[1] + 88);
        s.fpr_bits[29] = s.fpr_bits[28];
        Store(30, r[1] + 92);
        Store(29, r[1] + 96);
        r[24] = r[20];
        r[23] = r[20];
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[30] = r[31] + 160;
            r[23] = Word(r[31] + 164);
            Capacity(30);
            Grow(30, 0x82bc042cu);
            Push(30, 20);
            Increment(30);
        }
        r[11] = Word(r[31] + 264);
        r[25] = Shift(r[28], 2);
        r[26] = r[20];
        r[11] = Word(r[25] + r[11]);
        Compare(r[11]);
        if (s.cr6.gt) {
            do {
                r[10] = Word(r[31] + 268);
                r[9] = Word(r[31] + 272);
                r[11] = Word(r[31] + 248);
                r[10] = Word(r[25] + r[10]);
                r[10] += r[26];
                r[10] = Shift(r[10], 2);
                r[29] = Word(r[10] + r[9]);
                r[10] = Shift(r[29], 1);
                r[10] += r[29];
                r[30] = Shift(r[10], 4);
                r[11] += r[30];
                r[10] = Word(r[11] + 28);
                r[10] &= r[22];
                Compare(r[10]);
                if (!s.cr6.eq) {
                    r[10] = m.ReadU8(Address(r[31] + 290));
                    Compare(r[10]);
                    if (!s.cr6.eq)
                        WeightedNormal();
                    else {
                        Load(0, r[11] + 32);
                        Load(13, r[11] + 36);
                        Single(31, F(0) + F(31));
                        Load(0, r[11] + 40);
                        Single(30, F(13) + F(30));
                    }
                    r[11] = m.ReadU8(Address(r[31] + 284));
                    Single(29, F(0) + F(29));
                    ++r[24];
                    Compare(r[11]);
                    if (!s.cr6.eq) {
                        r[30] = r[31] + 160;
                        Capacity(30);
                        Grow(30, 0x82bc0674u);
                        Push(30, 29);
                        Increment(30);
                    }
                }
                r[11] = Word(r[31] + 264);
                ++r[26];
                r[11] = Word(r[25] + r[11]);
                Compare(r[26], r[11]);
            } while (s.cr6.lt);
            Store(29, r[1] + 96);
            Store(30, r[1] + 92);
            Store(31, r[1] + 88);
        }
        r[11] = m.ReadU8(Address(r[31] + 284));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[11] = Word(r[31] + 168);
            r[10] = Shift(r[23], 2);
            Word(r[10] + r[11], r[24]);
            r[11] = Word(r[31] + 276);
            ++r[11];
            Word(r[31] + 276, r[11]);
        }
        Single(0, F(30) * F(30));
        Single(0, F(29) * F(29) + F(0));
        Single(0, F(31) * F(31) + F(0));
        FCompare(F(0), F(28));
        if (!s.cr6.eq) {
            Single(0, std::sqrt(F(0)));
            Single(0, F(27) / F(0));
            Single(13, F(0) * F(31));
            Store(13, r[1] + 88);
            Single(13, F(30) * F(0));
            Store(13, r[1] + 92);
            Single(0, F(29) * F(0));
            Store(0, r[1] + 96);
        }
        r[4] = r[1] + 88;
        r[3] = r[31] + 128;
        Lower(0x82bb3c00u, 0x82bc0714u);
    }
    void Position() {
        auto &r = s.r;
        r[10] = Word(r[31] + 236);
        Compare(r[10]);
        if (s.cr6.eq)
            return;
        r[11] = m.ReadU8(Address(r[31] + 285));
        Compare(r[11]);
        if (!s.cr6.eq) {
            r[30] = r[31] + 32;
            Capacity(30);
            Grow(30, 0x82bc074cu);
            Push(30, 28);
            Increment(30);
        } else {
            r[11] = Shift(r[28], 1);
            r[3] = r[31] + 80;
            r[11] += r[28];
            r[11] = Shift(r[11], 2);
            r[4] = r[11] + r[10];
            Lower(0x82bb3c00u, 0x82bc0784u);
        }
    }
    void EmitFaces() {
        auto &r = s.r;
        r[30] = r[31] + 16;
        Capacity(30);
        Grow(30, 0x82bc07b0u);
        r[11] = Word(r[30] + 4);
        Compare(r[17]);
        r[10] = Word(r[30] + 8);
        r[11] = Shift(r[11], 2);
        Word(r[11] + r[10], r[17]);
        Increment(30);
        if (s.cr6.eq)
            return;
        r[26] = r[17];
        do {
            r[11] = Word(r[31] + 4);
            r[9] = Word(r[31]);
            r[28] = Word(r[27]);
            Compare(r[11], r[9]);
            r[10] = Word(r[31] + 248);
            r[11] = Shift(r[28], 1);
            r[11] += r[28];
            r[30] = Shift(r[11], 4);
            r[29] = Word(r[30] + r[10]);
            Grow(31, 0x82bc080cu);
            Push(31, 29);
            r[11] = Word(r[31] + 4);
            r[10] = Word(r[31] + 248);
            ++r[11];
            r[9] = Word(r[31]);
            r[10] += r[30];
            Compare(r[11], r[9]);
            Word(r[31] + 4, r[11]);
            r[29] = Word(r[10] + 4);
            Grow(31, 0x82bc084cu);
            Push(31, 29);
            r[11] = Word(r[31] + 4);
            r[10] = Word(r[31] + 248);
            ++r[11];
            r[9] = Word(r[31]);
            r[10] += r[30];
            Compare(r[11], r[9]);
            Word(r[31] + 4, r[11]);
            r[30] = Word(r[10] + 8);
            Grow(31, 0x82bc088cu);
            Push(31, 30);
            r[10] = Word(r[31] + 4);
            r[11] = Word(r[31] + 256);
            ++r[10];
            Compare(r[11]);
            Word(r[31] + 4, r[10]);
            if (!s.cr6.eq) {
                r[10] = Word(r[31] + 260);
                r[10] = Shift(r[10], 2);
                Word(r[10] + r[11], r[28]);
                r[11] = Word(r[31] + 260);
                ++r[11];
                Word(r[31] + 260, r[11]);
            }
            --r[26];
            r[27] += 4;
            Compare(r[26]);
        } while (!s.cr6.eq);
    }
    void Body() {
        auto &r = s.r;
        r[31] = r[3];
        r[27] = r[4];
        r[17] = r[5];
        r[30] = r[6];
        r[29] = r[7];
        r[11] = Word(r[31] + 248);
        Compare(r[11]);
        if (s.cr6.eq) {
            r[3] = 0;
            return;
        }
        r[18] = r[31] + 176;
        Capacity(18);
        Grow(18, 0x82bc0170u);
        Push(18, 30);
        r[11] = Word(r[18] + 4);
        r[10] = Word(r[18]);
        ++r[11];
        Compare(r[11], r[10]);
        Word(r[18] + 4, r[11]);
        Grow(18, 0x82bc01a4u);
        r[11] = Word(r[18] + 4);
        r[3] = r[1] + 128;
        r[10] = Word(r[18] + 8);
        r[11] = Shift(r[11], 2);
        Word(r[11] + r[10], r[29]);
        Increment(18);
        Lower(0x82bd2a08u, 0x82bc01c8u);
        r[6] = r[1] + 128;
        r[5] = r[17];
        r[4] = r[27];
        r[3] = r[31];
        Lower(0x82bbfea8u, 0x82bc01dcu);
        r[21] = Word(r[1] + 136);
        r[20] = 0;
        Compare(r[3]);
        if (!s.cr6.eq) {
            r[10] = 0xffffffff82000000ull;
            r[11] = 0xffffffff82000000ull;
            r[19] = r[3];
            Load(27, r[10] + 30596);
            Load(28, r[11] + 3664);
            do {
                r[11] = r[21] + 4;
                r[10] = Word(r[31] + 240);
                r[28] = Word(r[21]);
                Compare(r[10]);
                r[29] = Word(r[11]);
                r[11] += 4;
                r[26] = Word(r[11]);
                r[11] += 4;
                r[21] = r[11] + 4;
                r[22] = Word(r[11]);
                FirstAttribute();
                SecondAttribute();
                SmoothNormal();
                Position();
                --r[19];
                Compare(r[19]);
            } while (!s.cr6.eq);
        }
        EmitFaces();
        Capacity(18);
        Grow(18, 0x82bc08f8u);
        r[11] = Word(r[18] + 4);
        r[3] = r[1] + 128;
        r[10] = Word(r[18] + 8);
        r[11] = Shift(r[11], 2);
        Word(r[10] + r[11], r[20]);
        Increment(18);
        Lower(0x82bd2c08u, 0x82bc091cu);
        r[3] = r[17];
    }
    void Run() {
        auto &r = s.r;
        r[12] = s.lr;
        s.lr = 0x82bc0110u;
        for (unsigned i = 17; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[1] - 16 - 8 * (31 - i)), r[i]);
        Word(r[1] - 8, r[12]);
        r[12] = r[1] - 128;
        s.lr = 0x82bc0118u;
        Gradual();
        for (unsigned i = 27; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(r[12] - 8 * (32 - i)), s.fpr_bits[i]);
        auto old = r[1];
        r[1] -= 320;
        Word(r[1], old);
        Body();
        r[1] += 320;
        r[12] = r[1] - 128;
        Gradual();
        for (unsigned i = 27; i < 32; ++i)
            s.fpr_bits[i] = recovery_abi::ReadU64(m, Address(r[12] - 8 * (32 - i)));
        for (unsigned i = 17; i < 32; ++i)
            r[i] = recovery_abi::ReadU64(m, Address(r[1] - 16 - 8 * (31 - i)));
        r[12] = Word(r[1] - 8);
        s.lr = r[12];
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    if (e == 0x82bb9800u)
        Output{m, d, s}.IndexedInput();
    else if (e == 0x82bc0108u)
        Output{m, d, s}.Run();
    else if (e == 0x82bc0930u)
        Output{m, d, s}.Groups();
    else if (e == 0x82bc0ba8u)
        Output{m, d, s}.Pipeline();
    else
        return false;
    return true;
}
} // namespace lo::semantic::gpu::mesh_indexed_vertex_output61
