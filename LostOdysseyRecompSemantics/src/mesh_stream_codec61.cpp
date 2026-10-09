#include "lo_semantics/mesh_stream_codec61.h"
#include "lo_semantics/mesh_stream_write61.h"
#include "lo_semantics/mesh_polygon_collect61.h"
#include "lo_semantics/recovery_abi.h"
#include <array>
#include <bit>
namespace lo::semantic::gpu::mesh_stream_codec61 {
namespace {
using recovery_abi::Address;
struct Codec {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    unsigned Word(unsigned p) { return m.ReadU32(p); }
    void Enter() {
        auto old = s.r[1];
        m.WriteU32(Address(old - 8), Address(s.lr));
        for (unsigned i = 14; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(old - 16 - 8 * (31 - i)), s.r[i]);
        s.r[1] -= 512;
        m.WriteU32(Address(s.r[1]), Address(old));
    }
    void Leave() {
        s.r[1] += 512;
        for (unsigned i = 14; i < 32; ++i)
            s.r[i] = recovery_abi::ReadU64(m, Address(s.r[1] - 16 - 8 * (31 - i)));
        s.lr = m.ReadU32(Address(s.r[1] - 8));
    }
    void Virtual(unsigned stream, unsigned slot, unsigned ret) {
        s.r[3] = stream;
        s.ctr = Word(Word(stream) + slot);
        s.lr = ret;
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    unsigned Scalar(unsigned stream, bool swap, bool half, unsigned ret) {
        Virtual(stream, half ? 8 : 12, ret);
        auto word = Address(s.r[3]);
        if (swap)
            word = half ? __builtin_bswap16(std::uint16_t(word)) : __builtin_bswap32(word);
        return word;
    }
    void FourBytes(const std::array<unsigned, 4> &out, unsigned stream) {
        constexpr unsigned ret[]{0x82bad7f4u, 0x82bad810u, 0x82bad82cu, 0x82bad848u};
        for (unsigned i = 0; i < 4; ++i) {
            Virtual(stream, 4, ret[i]);
            m.WriteU8(out[i], Address(s.r[3]));
        }
    }
    bool Header(const std::array<unsigned, 4> &magic, unsigned versionOut, unsigned endianOut,
                unsigned stream) {
        auto sp = Address(s.r[1]);
        FourBytes({sp + 80, sp + 81, sp + 82, sp + 83}, stream);
        if (m.ReadU8(sp + 80) != 'N' || m.ReadU8(sp + 81) != 'X' || m.ReadU8(sp + 82) != 'S')
            return false;
        m.WriteU8(endianOut, m.ReadU8(sp + 83) & 1);
        FourBytes({sp + 80, sp + 81, sp + 82, sp + 83}, stream);
        for (unsigned i = 0; i < 4; ++i)
            if (m.ReadU8(sp + 80 + i) != std::uint8_t(magic[i]))
                return false;
        auto version = Scalar(stream, m.ReadU8(endianOut) != 0, false, 0x82bad8e0u);
        m.WriteU32(versionOut, version);
        return true;
    }

    bool IceHeader(const std::array<unsigned, 4> &magic, unsigned versionOut, unsigned endianOut,
                   unsigned stream) {
        constexpr unsigned first[]{0x82bd81ecu, 0x82bd8204u, 0x82bd821cu, 0x82bd8234u},
            second[]{0x82bd8274u, 0x82bd828cu, 0x82bd82a4u, 0x82bd82bcu};
        std::array<unsigned, 4> bytes;
        for (unsigned i = 0; i < 4; ++i) {
            Virtual(stream, 4, first[i]);
            bytes[i] = Address(s.r[3]) & 255u;
        }
        if (bytes[0] != 'I' || bytes[1] != 'C' || bytes[2] != 'E')
            return false;
        m.WriteU8(endianOut, bytes[3] & 1u);
        for (unsigned i = 0; i < 4; ++i) {
            Virtual(stream, 4, second[i]);
            bytes[i] = Address(s.r[3]) & 255u;
        }
        for (unsigned i = 0; i < 4; ++i)
            if (bytes[i] != (magic[i] & 255u))
                return false;
        auto version = Scalar(stream, m.ReadU8(endianOut) != 0, false, 0x82bd8314u);
        m.WriteU32(versionOut, version);
        return true;
    }
    void ReadHalves(unsigned out, unsigned count, bool swap, unsigned stream) {
        s.r[4] = out;
        s.r[5] = count * 2u;
        Virtual(stream, 24, 0x82bd7f0cu);
        if (swap)
            for (unsigned i = 0; i < count; ++i)
                m.WriteU16(out + 2 * i, __builtin_bswap16(m.ReadU16(out + 2 * i)));
        s.r[3] = 1;
    }
    void Adaptive(unsigned maximum, unsigned count, unsigned out, unsigned stream, bool swap,
                  bool half) {
        unsigned width =
            (half ? (maximum & 65535u) : maximum) <= 255 ? 1 : ((half || maximum <= 65535) ? 2 : 4);
        if (width == 4) {
            ReadWords(out, count, swap, stream, 0x82bd7f8cu);
            return;
        }
        auto old = s.r[1];
        s.r[12] = std::uint32_t(0 - width * count) & 0xfffffff0u;
        mesh_polygon_collect61::ProbeStack(m, s);
        auto back = Word(Address(s.r[1]));
        s.r[1] += s.r[12];
        m.WriteU32(Address(s.r[1]), back);
        auto temporary = Address(s.r[1] + 80);
        if (width == 1) {
            s.r[4] = temporary;
            s.r[5] = count;
            Virtual(stream, 24, half ? 0x82bd84c0u : 0x82bd87a0u);
        } else
            ReadHalves(temporary, count, swap, stream);
        for (unsigned i = 0; i < count; ++i) {
            unsigned value = width == 1 ? m.ReadU8(temporary + i) : m.ReadU16(temporary + 2 * i);
            if (half)
                m.WriteU16(out + 2 * i, value);
            else
                m.WriteU32(out + 4 * i, value);
        }
        s.r[1] = old;
    }
    void ReadWords(unsigned out, unsigned count, bool swap, unsigned stream,
                   unsigned ret = 0x82badb14u) {
        s.r[4] = out;
        s.r[5] = count * 4u;
        Virtual(stream, 24, ret);
        if (swap)
            for (unsigned i = 0; i < count; ++i)
                m.WriteU32(out + 4 * i, __builtin_bswap32(Word(out + 4 * i)));
        s.r[3] = 1;
    }
};
} // namespace
bool Apply(GuestAddress entry, GuestMemory &m, Dependencies d, Registers &s) {
    unsigned slot = 0;
    switch (entry) {
    case 0x82b9d4c8u:
        slot = 4;
        break;
    case 0x824b9f18u:
        slot = 8;
        break;
    case 0x824b9f30u:
        slot = 12;
        break;
    case 0x82b9c788u:
        slot = 16;
        break;
    case 0x82b9c7a0u:
        slot = 20;
        break;
    case 0x82b9d4e0u:
        slot = 24;
        break;
    default:
        break;
    }
    if (slot) {
        s.r[3] = m.ReadU32(Address(s.r[3]) + 4);
        s.r[11] = m.ReadU32(m.ReadU32(Address(s.r[3])) + slot);
        s.ctr = s.r[11];
        d.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        return true;
    }
    if (entry == 0x82bada70u || entry == 0x82badcd0u || entry == 0x82badd60u)
        return mesh_stream_write61::Apply(entry, m, d, s);
    switch (entry) {
    case 0x82bd81b0u:
    case 0x82bd7ed8u:
    case 0x82bd7f58u:
    case 0x82bd8468u:
    case 0x82bd8748u:
    case 0x82bad7c0u:
    case 0x82bad858u:
    case 0x82bad8b8u:
    case 0x82bad928u:
    case 0x82bad9a0u:
    case 0x82bada00u:
    case 0x82badae0u:
    case 0x82badc58u:
    case 0x82bade98u:
        break;
    default:
        return false;
    }
    auto a = s.r;
    Codec c{m, d, s};
    c.Enter();
    switch (entry) {
    case 0x82bd81b0u:
        s.r[3] = c.IceHeader({Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6])},
                             Address(a[7]), Address(a[8]), Address(a[9]))
                     ? 1
                     : 0;
        break;
    case 0x82bd7ed8u:
        c.ReadHalves(Address(a[3]), Address(a[4]), (a[5] & 255) != 0, Address(a[6]));
        break;
    case 0x82bd7f58u:
        c.ReadWords(Address(a[3]), Address(a[4]), (a[5] & 255) != 0, Address(a[6]), 0x82bd7f8cu);
        break;
    case 0x82bd8468u:
    case 0x82bd8748u:
        c.Adaptive(Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6]), (a[7] & 255) != 0,
                   entry == 0x82bd8468u);
        break;
    case 0x82bad7c0u:
        c.FourBytes({Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6])}, Address(a[7]));
        break;
    case 0x82bad858u:
        s.r[3] = c.Scalar(Address(a[4]), (a[3] & 255) != 0, true, 0x82bad880u);
        break;
    case 0x82bad8b8u:
        s.r[3] = c.Scalar(Address(a[4]), (a[3] & 255) != 0, false, 0x82bad8e0u);
        break;
    case 0x82bad928u: {
        auto bits = c.Scalar(Address(a[4]), (a[3] & 255) != 0, false, 0x82bad950u);
        s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(std::bit_cast<float>(bits)));
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.fp.SetHostFpControl(s.cached_fp_control);
        }
        break;
    }
    case 0x82bad9a0u:
        s.r[4] = (a[4] & 255) ? __builtin_bswap16(std::uint16_t(a[3])) : a[3];
        c.Virtual(Address(a[5]), 32, 0x82bad9ecu);
        break;
    case 0x82bada00u:
        s.r[4] = (a[4] & 255) ? __builtin_bswap32(Address(a[3])) : a[3];
        c.Virtual(Address(a[5]), 36, 0x82bada5cu);
        break;
    case 0x82badae0u:
        c.ReadWords(Address(a[3]), Address(a[4]), (a[5] & 255) != 0, Address(a[6]));
        break;
    case 0x82badc58u:
        for (unsigned i = 0; i < Address(a[4]); ++i) {
            auto value = m.ReadU16(Address(a[3]) + 2 * i);
            s.r[4] = (a[5] & 255) ? __builtin_bswap16(value) : value;
            c.Virtual(Address(a[6]), 32, 0x82badcbcu);
        }
        break;
    case 0x82bade98u:
        s.r[3] = c.Header({Address(a[3]), Address(a[4]), Address(a[5]), Address(a[6])},
                          Address(a[7]), Address(a[8]), Address(a[9]))
                     ? 1
                     : 0;
        break;
    }
    c.Leave();
    return true;
}
} // namespace lo::semantic::gpu::mesh_stream_codec61
