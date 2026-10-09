#include "lo_semantics/object_grid_transform61.h"
#include "lo_semantics/recovery_abi.h"

#include <array>
#include <bit>

namespace lo::semantic::gpu::object_grid_transform61 {
namespace {
using recovery_abi::Address;
using recovery_abi::ReadU64;
using recovery_abi::WriteU64;

void Compare(Registers& s, std::uint64_t lhs, std::uint64_t rhs)
{
    const auto a = Address(lhs), b = Address(rhs);
    s.cr6 = {std::uint8_t(a < b), std::uint8_t(a > b), std::uint8_t(a == b), s.xer_so};
}
bool Missing(Registers& s, std::uint64_t value)
{
    const auto signed_value = std::bit_cast<std::int32_t>(Address(value));
    s.cr6 = {std::uint8_t(signed_value < -1), std::uint8_t(signed_value > -1),
        std::uint8_t(signed_value == -1), s.xer_so};
    return s.cr6.eq;
}
std::uint64_t Product(std::uint64_t a, std::uint64_t b)
{
    return std::uint64_t(std::int64_t(std::bit_cast<std::int32_t>(Address(a))) *
        std::int64_t(std::bit_cast<std::int32_t>(Address(b))));
}
double Single(double value) { return static_cast<float>(value); }
void SetFloat(Registers& s, unsigned index, double value)
{
    s.fpr_bits[index] = std::bit_cast<std::uint64_t>(value);
}
double LoadFloat(GuestMemory& memory, std::uint64_t address)
{
    return std::bit_cast<float>(memory.ReadU32(Address(address)));
}
void Lower(GuestAddress entry, GuestAddress continuation, GuestMemory& memory,
    NativeServices& native, Registers& state)
{
    state.lr = continuation;
    (void)grid_transform_support61::Apply(entry, memory, native, state);
}

// Corner metadata describes coordinates and observable stack layout, rather
// than instructions. The two x-only variants exchange x/y FPR scratch roles.
struct Corner {
    unsigned index_register, dx, dy, dz;
    std::array<unsigned, 3> spill;
    unsigned position;
    GuestAddress continuation;
};
constexpr std::array<Corner, 8> Corners{{
    {9u,0u,0u,0u,{464u,344u,360u},168u,0x82bb098cu},
    {29u,0u,0u,1u,{376u,392u,408u},136u,0x82bb0a60u},
    {25u,0u,1u,0u,{424u,440u,456u},88u,0x82bb0b34u},
    {26u,0u,1u,1u,{472u,304u,432u},104u,0x82bb0c0cu},
    {22u,1u,0u,0u,{400u,320u,336u},120u,0x82bb0ce0u},
    {23u,1u,0u,1u,{352u,288u,416u},152u,0x82bb0db8u},
    {19u,1u,1u,0u,{368u,448u,384u},184u,0x82bb0e90u},
    {20u,1u,1u,1u,{296u,312u,328u},200u,0x82bb0f6cu}}};

// An unmarked test retains the scratch used by the original's short-circuit
// corner chain. Missing corners do not read the grid buffer.
bool Unmarked(unsigned index, GuestMemory& memory, Registers& s)
{
    auto& r = s.r;
    if (Missing(s, r[index])) return false;
    r[11] = memory.ReadU32(Address(r[31] + 108u));
    r[10] = Address(r[index]) << 2u;
    r[10] = memory.ReadU32(Address(r[11] + r[10]));
    r[10] = ~r[10];
    r[10] = Address(r[10]) >> 31u;
    Compare(s, r[10], 0u);
    return !s.cr6.eq;
}

void Position(const Corner& corner, GuestMemory& memory, NativeServices& native, Registers& s)
{
    auto& r = s.r;
    const std::array<std::uint32_t, 3> coordinate{
        Address(r[28] + corner.dx), Address(r[27] + corner.dy), Address(r[24] + corner.dz)};
    const bool swapped_xy = corner.dx == 1u && corner.dy == 0u;
    const bool swapped_integer_xy = corner.dx == 0u && corner.dy == 1u;
    r[11] = coordinate[swapped_integer_xy ? 1u : 0u];
    r[10] = coordinate[swapped_integer_xy ? 0u : 1u];
    r[9] = coordinate[2];
    // Integer-to-double and then double-to-single are separate stages; their
    // intermediate integer bits reside at each original guest spill slot.
    std::array<double, 3> values{};
    const std::array<unsigned, 3> order = swapped_xy ?
        std::array<unsigned, 3>{1u,0u,2u} : std::array<unsigned, 3>{0u,1u,2u};
    for (const auto axis : order) {
        WriteU64(memory, Address(r[1] + corner.spill[axis]), coordinate[axis]);
        if (s.cached_fp_control & 0x8040u) {
            s.cached_fp_control &= ~0x8040u;
            native.SetHostFpControl(s.cached_fp_control);
        }
        const auto integer = std::bit_cast<std::int64_t>(ReadU64(memory, Address(r[1] + corner.spill[axis])));
        values[axis] = Single(static_cast<double>(integer));
    }
    SetFloat(s, 5u, values[swapped_xy ? 1u : 0u]);
    SetFloat(s, 4u, values[swapped_xy ? 0u : 1u]);
    SetFloat(s, 3u, values[2]);
    std::array<double, 3> scale{}, bias{}, origin{}, intermediate{}, position{};
    for (unsigned axis = 0u; axis < 3u; ++axis) {
        scale[axis] = LoadFloat(memory, r[31] + 76u + 4u * axis);
        bias[axis] = LoadFloat(memory, r[31] + 40u + 4u * axis);
        origin[axis] = LoadFloat(memory, r[31] + 28u + 4u * axis);
        // The generated body uses a double product/subtraction narrowed to
        // single, followed by a separate single addition; do not fuse the
        // addition of origin into the multiply/subtract stage.
        const double product = scale[axis] * values[axis];
        intermediate[axis] = Single(product - bias[axis]);
        position[axis] = Single(intermediate[axis] + origin[axis]);
    }
    SetFloat(s, 6u, origin[2]);
    SetFloat(s, 7u, bias[2]);
    SetFloat(s, 8u, scale[2]);
    SetFloat(s, 9u, origin[1]);
    SetFloat(s, 10u, origin[0]);
    SetFloat(s, 11u, swapped_xy ? intermediate[2] : bias[1]);
    SetFloat(s, 0u, position[swapped_xy ? 1u : 0u]);
    SetFloat(s, 13u, position[swapped_xy ? 0u : 1u]);
    SetFloat(s, 12u, position[2]);
    const auto store = [&](unsigned axis) {
        memory.WriteU32(Address(r[1] + corner.position + axis * 4u),
            std::bit_cast<std::uint32_t>(static_cast<float>(position[axis])));
    };
    for (const auto axis : order) store(axis);
    r[7] = r[1] + 80u;
    r[6] = 0u;
    r[5] = r[21];
    r[4] = r[1] + corner.position;
    r[3] = r[1] + 224u;
    Lower(0x82bd78e8u, corner.continuation, memory, native, s);
    r[11] = memory.ReadU32(Address(r[31] + 108u));
    r[10] = memory.ReadU32(Address(r[1] + 280u));
    r[10] |= 0x80000000u;
    memory.WriteU32(Address(r[11] + r[30]), Address(r[10]));
}

void VisitMarked(const Corner& corner, bool first, GuestMemory& memory,
    NativeServices& native, Registers& s)
{
    auto& r = s.r;
    if (Missing(s, r[corner.index_register])) return;
    if (!first) r[11] = memory.ReadU32(Address(r[31] + 108u));
    r[30] = Address(r[corner.index_register]) << 2u;
    r[10] = memory.ReadU32(Address(r[11] + r[30]));
    r[10] = ~r[10];
    r[10] = Address(r[10]) >> 31u;
    Compare(s, r[10], 0u);
    if (!s.cr6.eq) return;
    Position(corner, memory, native, s);
}

void Transform(GuestMemory& memory, NativeServices& native, Registers& s)
{
    auto& r = s.r;
    r[12] = s.lr;
    s.lr = 0x82bb06e0u;
    for (unsigned i = 17u; i < 32u; ++i)
        WriteU64(memory, Address(r[1] - 16u - (31u - i) * 8u), r[i]);
    memory.WriteU32(Address(r[1] - 8u), Address(r[12]));
    const auto caller_sp = r[1];
    r[1] -= 608u;
    memory.WriteU32(Address(r[1]), Address(caller_sp));
    r[31] = r[3]; r[21] = r[4];
    r[18] = ~std::uint64_t{0};
    memory.WriteU32(Address(r[1] + 80u), Address(r[18]));
    r[3] = r[1] + 224u;
    Lower(0x82bd78c0u, 0x82bb06fcu, memory, native, s);
    r[7] = memory.ReadU32(Address(r[31] + 88u));
    r[17] = 0u; r[24] = 0u;
    Compare(s, r[7], 0u);
    if (!s.cr6.eq) {
        do {
            r[27] = 0u; Compare(s, r[7], 0u);
            if (!s.cr6.eq) {
                do {
                    r[28] = 0u; Compare(s, r[7], 0u);
                    if (!s.cr6.eq) {
                        do {
                            r[11] = memory.ReadU32(Address(r[31] + 88u));
                            r[10] = memory.ReadU32(Address(r[31] + 92u));
                            r[8] = Product(r[11], r[27]);
                            r[9] = Product(r[10], r[24]);
                            r[9] += r[8];
                            r[8] = r[11] - 1u;
                            r[9] += r[28];
                            Compare(s, r[28], r[8]);
                            r[29] = r[10] + r[9];
                            r[25] = r[11] + r[9];
                            r[26] = r[11] + r[29];
                            r[22] = r[9] + 1u; r[19] = r[25] + 1u;
                            r[23] = r[29] + 1u; r[20] = r[26] + 1u;
                            if (s.cr6.eq) r[20] = r[23] = r[19] = r[22] = r[18];
                            r[11] = r[7] - 1u; Compare(s, r[27], r[11]);
                            if (s.cr6.eq) r[20] = r[26] = r[19] = r[25] = r[18];
                            Compare(s, r[24], r[11]);
                            if (s.cr6.eq) r[20] = r[26] = r[23] = r[29] = r[18];
                            const bool any_unmarked = Unmarked(9u, memory, s) || Unmarked(22u, memory, s) ||
                                Unmarked(25u, memory, s) || Unmarked(29u, memory, s) ||
                                Unmarked(23u, memory, s) || Unmarked(26u, memory, s) ||
                                Unmarked(19u, memory, s) || Unmarked(20u, memory, s);
                            if (any_unmarked) {
                                ++r[17];
                                for (unsigned i = 0u; i < Corners.size(); ++i)
                                    VisitMarked(Corners[i], i == 0u, memory, native, s);
                            }
                            ++r[28]; r[7] = memory.ReadU32(Address(r[31] + 88u));
                            Compare(s, r[28], r[7]);
                        } while (s.cr6.lt);
                    }
                    ++r[27]; r[7] = memory.ReadU32(Address(r[31] + 88u));
                    Compare(s, r[27], r[7]);
                } while (s.cr6.lt);
            }
            ++r[24]; r[7] = memory.ReadU32(Address(r[31] + 88u));
            Compare(s, r[24], r[7]);
        } while (s.cr6.lt);
    }
    r[3] = r[1] + 224u;
    Lower(0x82bd78d8u, 0x82bb0fb4u, memory, native, s);
    r[3] = r[17];
    r[1] += 608u;
    for (unsigned i = 17u; i < 32u; ++i)
        r[i] = ReadU64(memory, Address(r[1] - 16u - (31u - i) * 8u));
    r[12] = memory.ReadU32(Address(r[1] - 8u)); s.lr = r[12];
}
}
bool Apply(GuestAddress entry, GuestMemory& memory, NativeServices& native, Registers& state)
{
    if (entry != 0x82bb06d8u) return false;
    Transform(memory, native, state);
    return true;
}
}
