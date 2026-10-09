#include "lo_semantics/mesh_indexed_cook61.h"
#include "lo_semantics/crt_copy_full_context.h"
#include "lo_semantics/integer_leaf.h"
#include "lo_semantics/mesh_bounds_select61.h"
#include "lo_semantics/mesh_convex_check61.h"
#include "lo_semantics/mesh_cook_support61.h"
#include "lo_semantics/mesh_indexed_vertex_output61.h"
#include "lo_semantics/mesh_polygon_plane61.h"
#include "lo_semantics/mesh_vertex_dedup61.h"
#include "lo_semantics/recovery_abi.h"
#include "lo_semantics/mesh_cook_storage61.h"
#include "lo_semantics/mesh_hull_preprocess61.h"
#include "lo_semantics/mesh_cook_stream61.h"
#include <bit>
#include <initializer_list>
namespace lo::semantic::gpu::mesh_indexed_cook61 {
namespace {
using recovery_abi::Address;
struct Cook {
    GuestMemory &m;
    Dependencies d;
    Registers &s;
    std::uint32_t Word(std::uint64_t p) { return m.ReadU32(Address(p)); }
    void Word(std::uint64_t p, std::uint64_t v) { m.WriteU32(Address(p), Address(v)); }
    void Gradual() {
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            d.edge.engine.fp.SetHostFpControl(s.cached_fp_control);
        }
    }
    float Float(std::uint64_t p) {
        Gradual();
        return std::bit_cast<float>(Word(p));
    }
    void Float(std::uint64_t p, float v) { Word(p, std::bit_cast<std::uint32_t>(v)); }
    void Enter(unsigned first, unsigned frame) {
        s.r[12] = s.lr;
        Word(s.r[1] - 8, s.lr);
        for (unsigned i = first; i < 32; ++i)
            recovery_abi::WriteU64(m, Address(s.r[1] - 16 - 8 * (31 - i)), s.r[i]);
        auto old = s.r[1];
        s.r[1] -= frame;
        Word(s.r[1], old);
    }
    void Leave(unsigned first, unsigned frame) {
        s.r[1] += frame;
        for (unsigned i = first; i < 32; ++i)
            s.r[i] = recovery_abi::ReadU64(m, Address(s.r[1] - 16 - 8 * (31 - i)));
        s.r[12] = Word(s.r[1] - 8);
        s.lr = s.r[12];
    }
    void Call(GuestAddress e, GuestAddress ret, std::initializer_list<std::uint64_t> args) {
        unsigned i = 3;
        for (auto a : args)
            s.r[i++] = a;
        s.lr = ret;
        switch (e) {
        case 0x82bd0798u:
            (void)crt_close_recursive_buffer_context::Apply(e, m, d.edge.engine.sort.guest, s);
            break;
        case 0x82b7a0b0u:
            (void)crt_copy_full_context::Apply(e, m, s);
            break;
        case 0x82bb9800u:
            (void)mesh_indexed_vertex_output61::Apply(e, m, d.edge.engine, s);
            break;
        case 0x82bb8580u:
            (void)mesh_vertex_dedup61::Apply(e, m, d.edge.engine, s);
            break;
        case 0x82bb86c8u:
        case 0x82bb8e88u:
            (void)mesh_convex_check61::Apply(e, m, d.edge.engine.fp, s);
            break;
        case 0x82bc65f8u:
            (void)mesh_geometry_math61::Apply(e, m, d.edge.engine.fp, s);
            break;
        case 0x82bd9390u:
        case 0x82bc3880u:
            (void)mesh_polygon_plane61::Apply(e, m, d.edge.engine.fp, s);
            break;
        case 0x82bb9aa8u:
            (void)mesh_polygon_build61::Apply(e, m, d, s);
            break;
        case 0x82bb3008u:
        case 0x82bb32d8u:
            (void)mesh_cache_lifetime61::Apply(e, m, d.lifetime, s);
            break;
        case 0x82b9c298u:
            (void)diagnostic_format_routes61::Apply(e, m, d.edge.diagnostics, s);
            break;
        case 0x82bbb0a8u:
            Build();
            break;
        case 0x82bb3060u:
            Adapter();
            break;
        case 0x82b9e4b0u:
        case 0x82b9e518u:
        case 0x82ba01c0u:
            (void)mesh_cook_storage61::Apply(e, m, d.lifetime, s);
            break;
        case 0x82b9f6f0u:
            (void)mesh_cook_stream61::Apply(e, m, d, s);
            break;
        case 0x82b9f198u:
            ValidateBuild();
            break;
        case 0x82ba5cf8u:
            (void)mesh_hull_preprocess61::Apply(e, m, d, s);
            break;
        case 0x82b9e8a0u:
            Strided();
            break;
        case 0x82b9e3f8u:
        case 0x82b9e7b0u:
            (void)mesh_cook_hull61::Apply(e, m, d, s);
            break;
        case 0x82b9e6a8u:
            (void)mesh_cook_tree61::Apply(e, m, {d.lifetime, d.edge.diagnostics}, s);
            break;
        case 0x82b9eb58u:
            (void)mesh_cook_support61::Apply(e, m, {d.lifetime, d.edge.diagnostics}, s);
            break;
        case 0x82b9ea90u:
            (void)mesh_bounds_select61::Apply(e, m, {d.edge.engine.sort.guest, d.edge.engine.fp},
                                              s);
            break;
        }
    }
    bool Success() { return (s.r[3] & 255) != 0; }
    std::uint32_t Allocate(std::uint32_t bytes, unsigned tag, GuestAddress get, GuestAddress ret) {
        Call(0x82bd0798u, get, {});
        s.r[4] = bytes;
        s.r[5] = tag;
        s.r[11] = Word(Word(s.r[3]));
        s.ctr = s.r[11];
        s.lr = ret;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        return Address(s.r[3]);
    }
    void Free(std::uint32_t p, GuestAddress get, GuestAddress ret) {
        Call(0x82bd0798u, get, {});
        s.r[4] = p;
        s.r[11] = Word(Word(s.r[3]) + 12);
        s.ctr = s.r[11];
        s.lr = ret;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    bool BuildBody(std::uint32_t adapter, std::uint32_t input, std::uint32_t polygonCount,
                   std::uint32_t polygons, std::uint32_t flags) {
        const auto mesh = Word(adapter + 4), sp = Address(s.r[1]);
        Word(mesh + 12, Word(input));
        Word(mesh + 4, Word(input + 8));
        auto vertices = Allocate(12 * Word(mesh + 12), 47, 0x82bbb0e8u, 0x82bbb110u);
        Word(mesh + 16, vertices);
        if (!vertices)
            return false;
        Call(0x82b7a0b0u, 0x82bbb158u, {vertices, Word(input + 4), 12 * Word(mesh + 12)});
        if (Word(input + 16)) {
            auto p = Allocate(12 * Word(mesh + 4), 0, 0x82bbb168u, 0x82bbb190u);
            Word(mesh + 8, p);
            if (!p)
                return false;
        }
        if (Word(input + 12)) {
            auto p = Allocate(12 * Word(mesh + 4), 0, 0x82bbb1bcu, 0x82bbb1e4u);
            Word(mesh + 8, p);
            if (!p)
                return false;
            for (unsigned i = 0; i < Word(mesh + 4); ++i)
                for (unsigned j = 0; j < 3; ++j)
                    Word(p + 12 * i + 4 * j, Word(Word(input + 12) + 12 * i + 4 * j));
        }
        if (polygonCount && polygons) {
            Word(mesh + 36, 0);
            if (auto p = Word(mesh + 44)) {
                Free(p, 0x82bbb2a0u, 0x82bbb2b8u);
                Word(mesh + 44, 0);
            }
            if (auto p = Word(mesh + 40)) {
                Free(p, 0x82bbb2d4u, 0x82bbb2ecu);
                Word(mesh + 40, 0);
            }
            const auto count = Word(polygonCount);
            Word(mesh + 36, count);
            auto records = Allocate(36 * count, 6, 0x82bbb304u, 0x82bbb32cu);
            Word(mesh + 40, records);
            if (!records)
                return false;
            Call(0x82bc65f8u, 0x82bbb34cu, {mesh, sp + 88});
            std::uint32_t indexCount = 0, cursor = polygons;
            for (unsigned i = 0; i < count; ++i) {
                auto n = Word(cursor);
                indexCount += n;
                cursor += 4 * (n + 1);
            }
            auto indices = Allocate(indexCount, 49, 0x82bbb384u, 0x82bbb39cu);
            Word(mesh + 44, indices);
            if (!indices)
                return false;
            cursor = polygons;
            auto out = indices;
            for (unsigned i = 0; i < count; ++i) {
                const auto n = Word(cursor), record = records + 36 * i;
                Word(record + 4, out);
                m.WriteU16(record, std::uint16_t(n));
                // Keep the source copy size and byte-slice advancement; do not
                // silently reinterpret the caller's externally supplied layout.
                Call(0x82b7a0b0u, 0x82bbb408u, {out, cursor + 4, 4 * n});
                Call(0x82bd9390u, 0x82bbb428u, {record + 12, n, out, vertices});
                float dot = float(Float(record + 20) * Float(sp + 96));
                dot = float(double(Float(record + 16)) * Float(sp + 92) + dot);
                dot = float(double(Float(record + 12)) * Float(sp + 88) + dot);
                dot = float(dot + Float(record + 24));
                if (dot > Float(0x82000e50u)) {
                    Call(0x82bc3880u, 0x82bbb474u, {n, out});
                    for (unsigned j = 0; j < 4; ++j)
                        Word(record + 12 + 4 * j, Word(record + 12 + 4 * j) ^ 0x80000000u);
                }
                cursor += 4 * (n + 1);
                out += n;
            }
            for (unsigned i = 0; i < count; ++i) {
                const auto record = records + 36 * i;
                Float(record + 28, Float(0x82000e0cu));
                Float(record + 32, Float(0x82000d64u));
                for (unsigned j = 0; j < Word(mesh + 12); ++j) {
                    auto point = vertices + 12 * j;
                    float dot = float(Float(point + 4) * Float(record + 16));
                    dot = float(double(Float(point + 8)) * Float(record + 20) + dot);
                    dot = float(double(Float(point)) * Float(record + 12) + dot);
                    if (dot < Float(record + 28))
                        Float(record + 28, dot);
                    if (dot > Float(record + 32))
                        Float(record + 32, dot);
                }
                if (flags && m.ReadU8(flags + i))
                    m.WriteU16(record + 2, m.ReadU16(record + 2) | 1u);
            }
        } else {
            const auto triangles = Word(mesh + 8);
            Call(0x82bb9800u, 0x82bbb628u, {mesh + 12, mesh + 4, vertices, triangles});
            if (Success()) {
                Call(0x82bb8580u, 0x82bbb644u, {mesh + 4, triangles, 1});
                Call(0x82bb86c8u, 0x82bbb65cu,
                     {Word(mesh + 12), vertices, Word(mesh + 4), triangles, 1});
                Call(0x82bb86c8u, 0x82bbb674u,
                     {Word(mesh + 12), vertices, Word(mesh + 4), triangles, 1});
                Call(0x82bb8580u, 0x82bbb684u, {mesh + 4, triangles, 1});
            }
            Word(sp + 80, Word(mesh + 4));
            Call(0x82bb8580u, 0x82bbb6a8u, {sp + 80, triangles, 0});
            if (!Success())
                return false;
            Call(0x82bb86c8u, 0x82bbb6d4u,
                 {Word(mesh + 12), Word(mesh + 16), Word(sp + 80), triangles, 0});
            if (!Success())
                return false;
            Call(0x82bb9aa8u, 0x82bbb6f8u, {adapter});
            if (!Success())
                return false;
        }
        Call(0x82bc65f8u, 0x82bbb710u, {mesh, mesh + 24});
        Call(0x82bb8e88u, 0x82bbb718u, {adapter});
        return Success();
    }
    void Build() {
        const auto a = Address(s.r[3]), input = Address(s.r[4]), count = Address(s.r[5]),
                   polys = Address(s.r[6]), flags = Address(s.r[7]);
        auto old = s.r[1];
        Enter(22, 208);
        Gradual();
        recovery_abi::WriteU64(m, Address(old - 96), s.fpr_bits[31]);
        bool ok = BuildBody(a, input, count, polys, flags);
        Gradual();
        s.fpr_bits[31] = recovery_abi::ReadU64(m, Address(old - 96));
        s.r[3] = ok ? 1 : 0;
        Leave(22, 208);
    }
    void Adapter() {
        auto a = Address(s.r[3]);
        Enter(31, 96);
        Call(0x82bbb0a8u, 0x82bb3078u, {});
        if (Success()) {
            // The source loads the same metadata word six times.
            for (unsigned off = 108; off <= 128; off += 4) {
                auto owner = Word(a + 12);
                Word(owner + off, Word(Word(Word(owner + 80) + 4)));
            }
            s.r[3] = 1;
        } else
            s.r[3] = 0;
        Leave(31, 96);
    }
    std::uint32_t StackArray(std::uint32_t bytes, GuestAddress ret) {
        s.r[12] = std::uint32_t(0 - bytes) & 0xfffffff0u;
        s.lr = ret;
        mesh_polygon_collect61::ProbeStack(m, s);
        auto back = Word(s.r[1]);
        s.r[1] += s.r[12];
        Word(s.r[1], back);
        return Address(s.r[1] + 80);
    }
    bool ValidDescriptor(std::uint32_t input) {
        const auto count = Word(input), flags = Word(input + 24);
        if (count < 3 || (count > 65535 && (flags & 2)) || !Word(input + 16) ||
            Word(input + 8) < 12)
            return false;
        if (Word(input + 20))
            return Word(input + 4) >= 2 && Word(input + 12) >= ((flags & 2) ? 6u : 12u);
        return (flags & 4) != 0;
    }
    std::uint32_t ContextAllocate(std::uint32_t bytes, unsigned tag, GuestAddress ret) {
        s.r[3] = Word(0x832df548u);
        s.r[4] = bytes;
        s.r[5] = tag;
        s.ctr = Word(Word(s.r[3]) + 8);
        s.lr = ret;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
        return Address(s.r[3]);
    }
    void ContextFree(std::uint32_t pointer, GuestAddress ret) {
        s.r[3] = Word(0x832df548u);
        s.r[4] = pointer;
        s.ctr = Word(Word(s.r[3]) + 20);
        s.lr = ret;
        d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
    }
    bool MainBody(std::uint32_t input, std::uint32_t stream) {
        constexpr std::uint32_t settings = 0x832dc180u;
        if (!Word(settings + 660) || !ValidDescriptor(input))
            return false;
        const auto sp = Address(s.r[1]), descriptor = sp + 128, temporary = sp + 96,
                   preprocess = sp + 160;
        for (unsigned i = 0; i < 7; ++i)
            Word(descriptor + 4 * i, Word(input + 4 * i));
        const auto flags = Word(descriptor + 24);
        bool inflate = (flags & 4) && (flags & 8), alternate = (flags & 4) && (flags & 16);
        if (alternate)
            Call(0x82b9c298u, 0x82b9c914u,
                 {1, 0xffffffff820d5880ull, 335, 0, 0xffffffff820d5910ull});
        auto owner = ContextAllocate(348, 14, 0x82b9c934u);
        if (!owner)
            return false;
        Call(0x82b9e4b0u, 0x82b9c940u, {owner});
        owner = Address(s.r[3]);
        if (!owner)
            return false;
        for (unsigned off : {160, 164, 168, 172, 100, 104, 108, 112, 116})
            Word(sp + off, 0);
        Float(sp + 176, Float(0x82000d6cu));
        Float(sp + 180, Float(0x82000d7cu));
        Word(sp + 184, 4096);
        m.WriteU8(temporary, 1);
        if (!alternate && (flags & 4)) {
            Word(preprocess, 5);
            Word(preprocess + 4, Word(descriptor));
            Word(preprocess + 8, Word(descriptor + 16));
            Word(preprocess + 12, Word(descriptor + 8));
            Float(preprocess + 20, Float(inflate ? settings + 4 : 0x82000e50u));
            Call(0x82ba5cf8u, 0x82b9ca20u, {sp + 80, preprocess, temporary});
            if (Address(s.r[3]) == 0) {
                Word(descriptor + 16, Word(temporary + 8));
                Word(descriptor, Word(temporary + 4));
                Word(descriptor + 4, Word(temporary + 12));
                Word(descriptor + 8, 12);
                Word(descriptor + 12, 12);
                Word(descriptor + 24, Word(descriptor + 24) & ~4u);
                Word(descriptor + 20, Word(temporary + 20));
            }
        }
        Call(0x82b9f198u, 0x82b9ca6cu, {owner, descriptor});
        bool ok = Success();
        if (ok)
            Call(0x82b9f6f0u, 0x82b9cac0u, {owner, stream, Word(descriptor + 24) & 32u});
        Call(0x82b9e518u, ok ? 0x82b9cac8u : 0x82b9ca80u, {owner});
        ContextFree(owner, ok ? 0x82b9cae0u : 0x82b9ca98u);
        Call(0x82ba01c0u, ok ? 0x82b9caecu : 0x82b9caa4u, {sp + 80, temporary});
        return ok;
    }
    void Main() {
        auto input = Address(s.r[3]), stream = Address(s.r[4]);
        Enter(25, 256);
        s.r[3] = MainBody(input, stream) ? 1 : 0;
        Leave(25, 256);
    }
    bool ValidateBuildBody(std::uint32_t owner, std::uint32_t input) {
        const auto count = Word(input), flags = Word(input + 24);
        bool valid = count >= 3 && !(count > 65535 && (flags & 2)) && Word(input + 16) &&
                     Word(input + 8) >= 12;
        if (Word(input + 20))
            valid = valid && Word(input + 4) >= 2 && Word(input + 12) >= ((flags & 2) ? 6u : 12u);
        else
            valid = valid && (flags & 4);
        if (!valid) {
            Call(0x82b9c298u, 0x82b9f248u,
                 {1, 0xffffffff820d5cd8ull, 79, 0, 0xffffffff820d5ee8ull});
            return false;
        }
        auto descriptor = Address(s.r[1] + 80);
        for (unsigned i = 0; i < 7; ++i)
            Word(descriptor + 4 * i, Word(input + 4 * i));
        if (!Word(descriptor + 20)) {
            Word(descriptor + 12, 12);
            s.r[3] = Word(0x832df548u);
            s.r[4] = 12 * count;
            s.r[5] = 254;
            s.ctr = Word(Word(s.r[3]) + 8);
            s.lr = 0x82b9f2e4u;
            d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
            auto packed = Address(s.r[3]);
            for (unsigned i = 0; i < count; ++i)
                Call(0x82b7a0b0u, 0x82b9f314u,
                     {packed + 12 * i, Word(descriptor + 16) + i * Word(descriptor + 8), 12});
            // B9CB60 is an already mapped configuration pointer leaf.
            lo::semantic::integer_leaf::Registers leaf{};
            (void)lo::semantic::integer_leaf::Apply(0x82b9cb60u, leaf);
            s.fpr_bits[1] = std::bit_cast<std::uint64_t>(double(Float(Address(leaf.r3) + 4)));
            Call(0x82b9e3f8u, 0x82b9f340u, {owner + 156, count, packed});
            // Preserve the original failure path's ownership, including its
            // early return before freeing the packed temporary.
            if (!Success())
                return false;
            Word(descriptor, Word(owner + 168));
            Word(descriptor + 4, Word(owner + 160));
            Word(descriptor + 8, 12);
            Word(descriptor + 12, 6);
            Word(descriptor + 16, Word(owner + 172));
            Word(descriptor + 20, Word(owner + 164));
            Word(descriptor + 24, (flags & ~2u) | 2u);
            if (packed) {
                s.r[3] = Word(0x832df548u);
                s.r[4] = packed;
                s.ctr = Word(Word(s.r[3]) + 20);
                s.lr = 0x82b9f3a4u;
                d.edge.engine.sort.guest.CallIndirect(Address(s.ctr) & ~3u, m, s);
            }
        } else {
            if (flags & 4)
                Call(0x82b9e7b0u, 0x82b9f3ccu, {owner, descriptor});
            else
                Call(0x82b9e8a0u, 0x82b9f3d4u, {owner, descriptor});
            if (!Success())
                return false;
        }
        Call(0x82b9e6a8u, 0x82b9f3e8u, {owner}); // Source intentionally ignores this result.
        Call(0x82b9ea90u, 0x82b9f3f0u, {owner});
        Call(0x82b9eb58u, 0x82b9f3f8u, {owner});
        if (!Success())
            return false;
        // Already mapped 82614AE8 is the constant-one leaf.
        s.r[3] = 1;
        return true;
    }
    void ValidateBuild() {
        auto owner = Address(s.r[3]), input = Address(s.r[4]);
        Enter(22, 208);
        s.r[3] = ValidateBuildBody(owner, input) ? 1 : 0;
        Leave(22, 208);
    }
    void Strided() {
        auto owner = Address(s.r[3]), input = Address(s.r[4]);
        Enter(23, 224);
        auto frame = s.r[1];
        Word(owner + 108, Word(owner + 108) & ~1u);
        auto vertexCount = Word(input), triangleCount = Word(input + 4);
        auto vertices = StackArray(12 * vertexCount, 0x82b9e8e0u);
        for (unsigned i = 0; i < vertexCount; ++i)
            Call(0x82b7a0b0u, 0x82b9e914u,
                 {vertices + 12 * i, Word(input + 16) + i * Word(input + 8), 12});
        auto indices = StackArray(12 * triangleCount, 0x82b9e940u);
        for (unsigned i = 0; i < triangleCount; ++i) {
            auto source = Word(input + 20) + i * Word(input + 12);
            if (Word(input + 24) & 2u)
                for (unsigned j = 0; j < 3; ++j)
                    Word(indices + 12 * i + 4 * j, m.ReadU16(source + 2 * j));
            else
                Call(0x82b7a0b0u, 0x82b9e9d0u, {indices + 12 * i, source, 12});
        }
        Word(frame + 80, vertexCount);
        Word(frame + 84, vertices);
        Word(frame + 88, triangleCount);
        Word(frame + 92, indices);
        Word(frame + 96, 0);
        Call(0x82bb3008u, 0x82b9ea0cu, {frame + 112, owner + 156});
        if (Word(input + 24) & 32u)
            m.WriteU16(frame + 120, 1);
        Call(0x82bb3060u, 0x82b9ea3cu, {frame + 112, frame + 80, 0, 0, 0});
        bool ok = Success();
        if (!ok)
            Call(0x82b9c298u, 0x82b9ea68u,
                 {4, 0xffffffff820d5cd8ull, 494, 0, 0xffffffff820d5df4ull});
        Call(0x82bb32d8u, ok ? 0x82b9ea84u : 0x82b9ea70u, {frame + 112});
        s.r[3] = ok ? 1 : 0;
        s.r[1] = frame;
        Leave(23, 224);
    }
};
} // namespace
bool Apply(GuestAddress e, GuestMemory &m, Dependencies d, Registers &s) {
    Cook c{m, d, s};
    switch (e) {
    case 0x82b9c7d8u:
        c.Main();
        break;
    case 0x82b9f198u:
        c.ValidateBuild();
        break;
    case 0x82bbb0a8u:
        c.Build();
        break;
    case 0x82bb3060u:
        c.Adapter();
        break;
    case 0x82b9e8a0u:
        c.Strided();
        break;
    default:
        return false;
    }
    return true;
}
} // namespace lo::semantic::gpu::mesh_indexed_cook61
