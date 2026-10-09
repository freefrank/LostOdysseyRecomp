#include "lo_semantics/object_sort_dispatch61.h"
#include "lo_semantics/object_sort_support61.h"
#include "lo_semantics/reader_buffer_growth61.h"
#include "lo_semantics/crt_close_next61.h"
#include "lo_semantics/crt_close_upper61.h"
#include "lo_semantics/crt_close_buffer_callers_context.h"
#include "lo_semantics/crt_reader_cleanup_callers_context.h"
#include "lo_semantics/crt_reader_units61.h"
#include "lo_semantics/crt_reader_follow61.h"
#include "lo_semantics/recovery_abi.h"
#include <bit>

namespace lo::semantic::gpu::object_sort_dispatch61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;
void Compare(Registers& s, std::uint64_t left, std::uint64_t right)
{
    const auto a = Address(left), b = Address(right);
    s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b), s.xer_so};
}
void Lower(GuestAddress entry, GuestAddress next, GuestMemory& memory, Dependencies d, Registers& s)
{
    s.lr = next;
    auto& guest = d.engine.sort.guest;
    switch (entry) {
    case 0x82bd0938u: (void)crt_close_next61::Apply(entry, memory, guest, s); break;
    case 0x82bd0cd0u: (void)crt_close_upper61::Apply(entry, memory, d.engine.sort, s); break;
    case 0x82bd0df0u: (void)crt_reader_cleanup_callers_context::Apply(entry, memory, guest, s); break;
    case 0x82bd1050u: (void)crt_reader_units61::Apply(entry, memory, guest, s); break;
    case 0x82bd1200u:
        (void)crt_close_buffer_callers_context::Apply(entry, memory,
            {guest, {d.engine.sort.accepted, d.output}}, s); break;
    case 0x82bd2870u:
        (void)reader_buffer_growth61::Apply(entry, memory, {guest, d.engine.fp}, s); break;
    case 0x82bd2c78u: (void)crt_reader_follow61::Apply(entry, memory, guest, s); break;
    case 0x82bd2df0u: (void)crt_reader_bucket_sort61::Apply(entry, memory, d.engine.sort, s); break;
    default: (void)object_sort_support61::Apply(entry, memory, {guest, d.engine.fp}, s); break;
    }
}
void Flush(GuestAddress next, GuestMemory& memory, Dependencies d, Registers& s)
{
    if (!s.cr6.eq) return;
    s.r[3] = s.r[31];
    memory.WriteU8(Address(s.r[31] + 24u), std::uint8_t(s.r[22]));
    Lower(0x82bd0938u, next, memory, d, s);
}
void EncodeGroup(GuestAddress next, GuestMemory& memory, Dependencies d, Registers& s)
{
    s.r[4] = s.r[31];
    s.r[5] = memory.ReadU32(Address(s.r[24] + 88u));
    s.r[3] = s.r[1] + 80u;
    s.lr = next;
    (void)object_sort_engine61::Apply(0x82bae200u, memory, d.engine, s);
}
// Emit the ordinary one-bit group tag. Byte/count store order differs between
// the true and false tags and is retained for ordinary aliased guest memory.
void Tag(bool consecutive, GuestAddress next, GuestMemory& memory, Dependencies d, Registers& s)
{
    auto& r = s.r;
    r[10] = memory.ReadU8(Address(r[31] + 25u));
    r[11] = memory.ReadU8(Address(r[31] + 24u));
    r[10] = std::rotl(Address(r[10]), 1);
    ++r[11];
    if (consecutive) {
        r[10] &= 0xffu; r[11] &= 0xffu; r[4] = r[10] | 1u;
        Compare(s, r[11], 8u);
        memory.WriteU8(Address(r[31] + 24u), std::uint8_t(r[11]));
        memory.WriteU8(Address(r[31] + 25u), std::uint8_t(r[4]));
    } else {
        r[4] = r[10] & 0xffu; r[11] &= 0xffu;
        Compare(s, r[11], 8u);
        memory.WriteU8(Address(r[31] + 25u), std::uint8_t(r[4]));
        memory.WriteU8(Address(r[31] + 24u), std::uint8_t(r[11]));
    }
    Flush(next, memory, d, s);
}
// 32-bit explicit value, or the all-ones terminator. r30 remains live across
// byte flush calls, just like the value register and writer identity.
void Word(bool terminator, GuestAddress next, GuestMemory& memory, Dependencies d, Registers& s)
{
    auto& r = s.r;
    r[30] = r[21];
    do {
        if (terminator) {
            r[10] = memory.ReadU8(Address(r[31] + 25u));
            r[9] = std::countl_zero(Address(r[30]));
            r[11] = memory.ReadU8(Address(r[31] + 24u));
        } else {
            r[9] = r[30] & r[29];
            r[10] = memory.ReadU8(Address(r[31] + 25u));
            r[11] = memory.ReadU8(Address(r[31] + 24u));
            r[9] = std::countl_zero(Address(r[9]));
        }
        r[10] = std::rotl(Address(r[10]), 1);
        r[9] = (Address(r[9]) >> 5u) & 1u;
        r[10] &= 0xffu;
        if (terminator) {
            r[9] ^= 1u; ++r[11]; r[4] = r[10] | r[9]; r[11] &= 0xffu;
        } else {
            ++r[11]; r[9] ^= 1u; r[11] &= 0xffu; r[4] = r[9] | r[10];
        }
        Compare(s, r[11], 8u);
        if (terminator) {
            memory.WriteU8(Address(r[31] + 25u), std::uint8_t(r[4]));
            memory.WriteU8(Address(r[31] + 24u), std::uint8_t(r[11]));
        } else {
            memory.WriteU8(Address(r[31] + 24u), std::uint8_t(r[11]));
            memory.WriteU8(Address(r[31] + 25u), std::uint8_t(r[4]));
        }
        Flush(next, memory, d, s);
        r[30] = Address(r[30]) >> 1u;
        Compare(s, r[30], 0u);
    } while (!s.cr6.eq);
}
void Dispatch(GuestMemory& memory, Dependencies d, Registers& s)
{
    auto& r = s.r;
    r[12] = s.lr; s.lr = 0x82bafec8u;
    for (unsigned i = 18u; i <= 31u; ++i)
        WriteU64(memory, Address(r[1] - 8u * (33u - i)), r[i]);
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    const auto caller_sp = r[1]; r[1] -= 288u;
    memory.WriteU32(Address(r[1]), Address(caller_sp));
    r[11] = 0xffffffff832e0000ull; r[9] = ~std::uint64_t{0};
    r[10] = r[11] - 15288u;
    r[11] = 0xffffffff83210000ull; r[22] = 0u; r[11] += 24972u;
    r[24] = r[3]; r[18] = r[4]; r[31] = r[5];
    memory.WriteU32(Address(r[11] - 8u), Address(r[9]));
    memory.WriteU32(Address(r[11] - 4u), Address(r[9]));
    memory.WriteU32(Address(r[11]), Address(r[9]));
    r[9] = r[22]; r[11] = 32u; s.ctr = r[11];
    do { memory.WriteU32(Address(r[10]), Address(r[9])); r[10] += 4u; } while (--s.ctr != 0u);
    r[19] = 0xffffffff832e0000ull;
    r[10] = memory.ReadU32(Address(r[24] + 104u)); r[5] = 1u;
    r[4] = (std::uint64_t(Address(r[10])) << 2u) & 0xfffffffcu;
    r[3] = memory.ReadU32(Address(r[19] - 2744u));
    r[11] = memory.ReadU32(Address(r[3])); r[11] = memory.ReadU32(Address(r[11] + 8u));
    s.ctr = r[11]; s.lr = 0x82baff38u;
    d.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, memory, s);
    r[11] = memory.ReadU32(Address(r[24] + 104u)); r[20] = r[3]; r[10] = r[22];
    Compare(s, r[11], 0u);
    if (s.cr6.gt) {
        r[11] = r[22];
        do {
            r[9] = memory.ReadU32(Address(r[24] + 108u)); ++r[10];
            r[9] = memory.ReadU32(Address(r[11] + r[9])); r[9] &= 0x3fffffffu;
            memory.WriteU32(Address(r[11] + r[20]), Address(r[9])); r[11] += 4u;
            r[9] = memory.ReadU32(Address(r[24] + 104u)); Compare(s, r[10], r[9]);
        } while (s.cr6.lt);
    }
    r[3] = r[1] + 96u; Lower(0x82bd2c50u, 0x82baff7cu, memory, d, s);
    r[6] = 1u; r[4] = r[20]; r[5] = memory.ReadU32(Address(r[24] + 104u)); r[3] = r[1] + 96u;
    Lower(0x82bd2df0u, 0x82baff90u, memory, d, s);
    r[11] = r[3]; r[3] = r[1] + 80u; r[30] = memory.ReadU32(Address(r[11] + 4u));
    Lower(0x82bd2a08u, 0x82baffa0u, memory, d, s);
    r[5] = 0u; r[4] = 4096u; r[3] = r[1] + 128u;
    Lower(0x82bd0cd0u, 0x82baffb0u, memory, d, s);
    Compare(s, r[31], 0u); if (s.cr6.eq) r[31] = r[1] + 128u;
    r[4] = 80u; r[29] = memory.ReadU32(Address(r[24] + 88u)); r[3] = r[31];
    Lower(0x82bd0938u, 0x82baffccu, memory, d, s);
    r[4] = 77u; Lower(0x82bd0938u, 0x82baffd4u, memory, d, s);
    r[4] = 65u; Lower(0x82bd0938u, 0x82baffdcu, memory, d, s);
    r[4] = 80u; Lower(0x82bd0938u, 0x82baffe4u, memory, d, s);
    r[4] = 4u; Lower(0x82bd1050u, 0x82baffecu, memory, d, s);
    r[4] = r[29]; Lower(0x82bd1050u, 0x82bafff4u, memory, d, s);
    r[10] = 0x3fff0000u; r[11] = memory.ReadU32(Address(r[24] + 104u));
    r[28] = r[22]; r[25] = r[10] | 0xffffu; r[23] = r[22]; r[29] = r[25]; r[21] = 0xffffffff80000000ull;
    Compare(s, r[11], 0u);
    bool consecutive_final = false;
    if (s.cr6.gt) {
        r[11] = memory.ReadU32(Address(r[1] + 84u)); r[26] = r[30];
        do {
            r[10] = memory.ReadU32(Address(r[26])); r[10] = (std::uint64_t(Address(r[10])) << 2u) & 0xfffffffcu;
            r[27] = memory.ReadU32(Address(r[10] + r[20])); Compare(s, r[27], r[25]);
            if (!s.cr6.eq) {
                Compare(s, r[27], r[29]);
                if (!s.cr6.eq) {
                    Compare(s, r[29], r[25]);
                    if (!s.cr6.eq) {
                        r[11] = memory.ReadU8(Address(r[31] + 24u)); Compare(s, r[29], r[28]);
                        r[10] = memory.ReadU8(Address(r[31] + 25u)); ++r[11];
                        r[10] = std::rotl(Address(r[10]), 1); r[11] &= 0xffu;
                        memory.WriteU8(Address(r[31] + 24u), std::uint8_t(r[11]));
                        if (s.cr6.eq) {
                            r[10] &= 0xffu; Compare(s, r[11], 8u); r[4] = r[10] | 1u;
                            memory.WriteU8(Address(r[31] + 25u), std::uint8_t(r[4]));
                            Flush(0x82bb0084u, memory, d, s);
                        } else {
                            r[4] = r[10] & 0xffu; Compare(s, r[11], 8u);
                            memory.WriteU8(Address(r[31] + 25u), std::uint8_t(r[4]));
                            Flush(0x82bb00a4u, memory, d, s);
                            Word(false, 0x82bb00f0u, memory, d, s); r[28] = r[29];
                        }
                        EncodeGroup(0x82bb0110u, memory, d, s);
                        r[11] = memory.ReadU32(Address(r[1] + 84u)); ++r[28];
                    }
                    Compare(s, r[11], 0u);
                    if (!s.cr6.eq) { r[11] = r[22]; memory.WriteU32(Address(r[1] + 84u), Address(r[11])); }
                    r[29] = r[27];
                }
                r[10] = memory.ReadU32(Address(r[1] + 80u));
                r[30] = memory.ReadU32(Address(r[26])); Compare(s, r[11], r[10]);
                if (s.cr6.eq) {
                    r[4] = 1u; r[3] = r[1] + 80u; Lower(0x82bd2870u, 0x82bb0148u, memory, d, s);
                    r[11] = memory.ReadU32(Address(r[1] + 84u));
                }
                r[10] = memory.ReadU32(Address(r[1] + 88u)); r[11] = (std::uint64_t(Address(r[11])) << 2u) & 0xfffffffcu;
                memory.WriteU32(Address(r[11] + r[10]), Address(r[30]));
                r[11] = memory.ReadU32(Address(r[1] + 84u)); ++r[11]; memory.WriteU32(Address(r[1] + 84u), Address(r[11]));
            }
            r[10] = memory.ReadU32(Address(r[24] + 104u)); ++r[23]; r[26] += 4u; Compare(s, r[23], r[10]);
        } while (s.cr6.lt);
        Compare(s, r[29], r[28]); consecutive_final = s.cr6.eq;
    }
    if (consecutive_final) Tag(true, 0x82bb01b8u, memory, d, s);
    else { Tag(false, 0x82bb01f0u, memory, d, s); Word(false, 0x82bb023cu, memory, d, s); }
    EncodeGroup(0x82bb0258u, memory, d, s);
    Tag(false, 0x82bb028cu, memory, d, s); Word(true, 0x82bb02d4u, memory, d, s);
    r[11] = memory.ReadU32(Address(r[24] + 104u)); r[29] = r[22]; Compare(s, r[11], 0u);
    if (s.cr6.gt) {
        r[30] = r[22];
        do {
            r[10] = memory.ReadU32(Address(r[24] + 108u)); r[9] = memory.ReadU8(Address(r[31] + 25u));
            r[11] = memory.ReadU8(Address(r[31] + 24u)); r[9] = std::rotl(Address(r[9]), 1); ++r[11];
            r[10] = memory.ReadU32(Address(r[30] + r[10])); r[11] &= 0xffu; r[10] = ~r[10];
            Compare(s, r[11], 8u); r[10] = (Address(r[10]) >> 31u) & 1u; r[10] |= r[9];
            memory.WriteU8(Address(r[31] + 24u), std::uint8_t(r[11])); r[4] = r[10] & 0xffu;
            memory.WriteU8(Address(r[31] + 25u), std::uint8_t(r[4])); Flush(0x82bb033cu, memory, d, s);
            r[11] = memory.ReadU32(Address(r[24] + 104u)); ++r[29]; r[30] += 4u; Compare(s, r[29], r[11]);
        } while (s.cr6.lt);
    }
    Compare(s, r[18], 0u);
    if (!s.cr6.eq) { r[5] = 0u; r[4] = r[18]; r[3] = r[31]; Lower(0x82bd1200u, 0x82bb0368u, memory, d, s); }
    Compare(s, r[20], 0u);
    if (!s.cr6.eq) {
        r[3] = memory.ReadU32(Address(r[19] - 2744u)); r[4] = r[20];
        r[11] = memory.ReadU32(Address(r[3])); r[11] = memory.ReadU32(Address(r[11] + 20u));
        s.ctr = r[11]; s.lr = 0x82bb0388u;
        d.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, memory, s);
    }
    r[3] = r[1] + 128u; Lower(0x82bd0df0u, 0x82bb0390u, memory, d, s);
    r[3] = r[1] + 80u; Lower(0x82bd2c08u, 0x82bb0398u, memory, d, s);
    r[3] = r[1] + 96u; Lower(0x82bd2c78u, 0x82bb03a0u, memory, d, s);
    r[3] = 1u; r[1] += 288u;
    for (unsigned i = 18u; i <= 31u; ++i) r[i] = ReadU64(memory, Address(r[1] - 8u * (33u - i)));
    r[12] = memory.ReadU32(Address(r[1] - 8u)); s.lr = r[12];
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, Dependencies deps, Registers& state)
{
    if (entry != 0x82bafec0u) return false;
    Dispatch(memory, deps, state); return true;
}
}
