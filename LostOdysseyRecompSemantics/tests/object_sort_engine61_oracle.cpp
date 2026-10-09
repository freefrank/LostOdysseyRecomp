// Only the original 82BAE200 body is pinned. Its seven call boundaries use
// independently accepted complete lowers through the same mutable services.
// This compares upper logic, not a second validation of those lower bodies.
#include "object_sort_engine61_oracle_fixture.h"

namespace sort_engine61_oracle {
struct Case {
    const char* name;
    std::vector<Point> points;
    Point previous;
    unsigned dimension = 32u;
    unsigned pending = 0u;
};
Environment* original_environment = nullptr;

struct ExpectedBits {
    std::vector<std::uint8_t> bytes;
    unsigned pending;
    std::uint8_t accumulator;
    explicit ExpectedBits(unsigned count) : pending(count), accumulator(count ? 5u : 0u) {}
    void Write(std::uint32_t value, unsigned width) {
        for (unsigned remaining = width; remaining != 0u; --remaining) {
            accumulator = static_cast<std::uint8_t>((accumulator << 1u) | ((value >> (remaining - 1u)) & 1u));
            if (++pending == 8u) { bytes.push_back(accumulator); pending = 0u; }
        }
    }
};
std::uint32_t Morton(Point point, unsigned width) {
    std::uint32_t key = 0u;
    for (unsigned bit = 0u; bit < width; ++bit) {
        key |= ((point.z >> bit) & 1u) << (3u * bit);
        key |= ((point.y >> bit) & 1u) << (3u * bit + 1u);
        key |= ((point.x >> bit) & 1u) << (3u * bit + 2u);
    }
    return key;
}
ExpectedBits Expected(const Case& test_case, Point& last) {
    const unsigned width = test_case.dimension <= 32u ? 5u : 6u;
    auto points = test_case.points;
    std::stable_sort(points.begin(), points.end(), [width](Point left, Point right) {
        return Morton(left, width) < Morton(right, width);
    });
    ExpectedBits expected(test_case.pending);
    expected.Write(static_cast<std::uint32_t>(points.size()), 32u);
    last = test_case.previous;
    // This wire-code table was independently recovered by the BAF600 decoder.
    constexpr std::array<std::array<int, 3>, 26> deltas{{
        {-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1},
        {-1,-1,0},{1,1,0},{-1,1,0},{1,-1,0},{0,-1,-1},{0,1,1},
        {0,-1,1},{0,1,-1},{-1,0,-1},{1,0,1},{-1,0,1},{1,0,-1},
        {-1,-1,-1},{1,1,1},{-1,-1,1},{1,1,-1},{1,-1,-1},{-1,1,1},
        {1,-1,1},{-1,1,-1}}};
    for (auto point : points) {
        const std::array difference{int(point.x) - int(last.x), int(point.y) - int(last.y), int(point.z) - int(last.z)};
        unsigned code = 0u;
        while (code < deltas.size() && difference != deltas[code]) ++code;
        if (code == 26u) {
            if (difference[1] == 0 && difference[2] == 0) code = 26u;
            else if (difference[0] == 0 && difference[2] == 0) code = 27u;
            else if (difference[0] == 0 && difference[1] == 0) code = 28u;
            else if (difference[2] == 0) code = 29u;
            else if (difference[1] == 0) code = 30u;
            else code = 31u;
        }
        expected.Write(code, 5u);
        if (code == 26u || code >= 29u) expected.Write(point.x, width);
        if (code == 27u || code == 29u || code == 31u) expected.Write(point.y, width);
        if (code == 28u || code == 30u || code == 31u) expected.Write(point.z, width);
        last = point;
    }
    return expected;
}

void Check(const Case& test_case) {
    RestoreHost restore;
    GuestWindow original(EngineRegions), recovered(EngineRegions);
    Seed(original, test_case.points, test_case.previous, test_case.dimension, test_case.pending);
    Seed(recovered, test_case.points, test_case.previous, test_case.dimension, test_case.pending);
    Environment expected(original), actual(recovered);
    const auto initial = Initial(test_case.dimension);
    auto state = initial;
    PPCContext context{}; crt_full_oracle::ToPpc(context, initial);
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    original_environment = &expected;
    __imp__sub_82BAE200(context, original.Bytes());
    original_environment = nullptr;
    const auto expected_host = PPCFPSCRRegister{}.getcsr();
    PPCFPSCRRegister{}.setcsr(initial.cached_fp_control);
    if (!object_sort_engine61::Apply(0x82bae200u, actual.stream.memory, actual.Deps(), state))
        throw std::runtime_error("missing engine entry");
    const auto expected_state = crt_full_oracle::Snapshot(crt_full_oracle::FromPpc(context));
    const auto actual_state = crt_full_oracle::Snapshot(state);
    for (unsigned i = 0u; i < expected_state.size(); ++i)
        if (expected_state[i] != actual_state[i]) {
            std::fprintf(stderr, "%s: Full72 index %u expected %016llx actual %016llx\n", test_case.name, i,
                static_cast<unsigned long long>(expected_state[i]), static_cast<unsigned long long>(actual_state[i]));
            throw std::runtime_error("engine register mismatch");
        }
    if (expected.guest.events != actual.guest.events) {
        const auto count = std::min(expected.guest.events.size(), actual.guest.events.size());
        for (unsigned event = 0u; event < count; ++event)
            for (unsigned field = 0u; field < 73u; ++field)
                if (expected.guest.events[event][field] != actual.guest.events[event][field]) {
                    std::fprintf(stderr, "%s: callback %u field %u expected %016llx actual %016llx\n", test_case.name, event, field,
                        static_cast<unsigned long long>(expected.guest.events[event][field]),
                        static_cast<unsigned long long>(actual.guest.events[event][field]));
                    throw std::runtime_error("engine callback mismatch");
                }
        throw std::runtime_error("engine callback count mismatch");
    }
    if (!original.EqualCommitted(recovered))
        throw std::runtime_error("engine RAM mismatch");
    if (expected_host != PPCFPSCRRegister{}.getcsr())
        throw std::runtime_error("engine FP-control mismatch");
    if (state.r[1] != initial.r[1] || state.lr != 0x81234567u)
        throw std::runtime_error("engine high-SP/low-LR restore mismatch");
    for (unsigned i = 19u; i < 32u; ++i)
        if (state.r[i] != initial.r[i]) throw std::runtime_error("engine saved-register mismatch");

    Point last{};
    const auto bits = Expected(test_case, last);
    const auto memory = actual.stream.memory;
    if (memory.ReadU32(Node + 4u) != bits.bytes.size() ||
        memory.ReadU8(Writer + 24u) != bits.pending ||
        memory.ReadU8(Writer + 25u) != bits.accumulator ||
        memory.ReadU32(0x83216184u) != last.x ||
        memory.ReadU32(0x83216188u) != last.y ||
        memory.ReadU32(0x8321618cu) != last.z)
        throw std::runtime_error("engine independent bit count/accumulator/last-coordinate mismatch");
    for (unsigned i = 0u; i < bits.bytes.size(); ++i)
        if (memory.ReadU8(Data + i) != bits.bytes[i])
            throw std::runtime_error("engine independent encoded-byte mismatch");
}
}

void OriginalObjectSortEngine61Lower(std::uint32_t entry, PPCContext& context, std::uint8_t*) {
    auto state = crt_full_oracle::FromPpc(context);
    sort_engine61_oracle::EngineLower(entry, *sort_engine61_oracle::original_environment, state);
    crt_full_oracle::ToPpc(context, state);
}

void OriginalObjectSortEngine61Save(unsigned first, PPCContext& context, std::uint8_t*) {
    const auto state = crt_full_oracle::FromPpc(context);
    auto& memory = sort_engine61_oracle::original_environment->stream.memory;
    for (unsigned i = first; i < 32u; ++i)
        recovery_abi::WriteU64(memory, Address(state.r[1] - 16u - 8u * (31u - i)), state.r[i]);
    memory.WriteU32(Address(state.r[1] - 8u), Address(state.r[12]));
}

void OriginalObjectSortEngine61Restore(unsigned first, PPCContext& context, std::uint8_t*) {
    auto state = crt_full_oracle::FromPpc(context);
    auto& memory = sort_engine61_oracle::original_environment->stream.memory;
    for (unsigned i = first; i < 32u; ++i)
        state.r[i] = recovery_abi::ReadU64(memory, Address(state.r[1] - 16u - 8u * (31u - i)));
    state.r[12] = memory.ReadU32(Address(state.r[1] - 8u));
    state.lr = state.r[12];
    crt_full_oracle::ToPpc(context, state);
}

int main() {
    using namespace sort_engine61_oracle;
    try {
        const std::array cases{
            sort_engine61_oracle::Case{"empty", {}, {4u,4u,4u}},
            sort_engine61_oracle::Case{"neighbor-zero-code", {{3u,4u,4u}}, {4u,4u,4u}},
            sort_engine61_oracle::Case{"neighbor-low-bit", {{5u,4u,4u}}, {4u,4u,4u}},
            sort_engine61_oracle::Case{"neighbor-shifted-mask", {{4u,3u,4u}}, {4u,4u,4u}},
            sort_engine61_oracle::Case{"neighbor-power-two", {{4u,4u,3u}}, {4u,4u,4u}},
            sort_engine61_oracle::Case{"neighbor-general-mask", {{4u,4u,5u}}, {4u,4u,4u}},
            sort_engine61_oracle::Case{"mixed-morton-absolute-pending", {{12u,4u,7u}, {3u,3u,3u},
                {4u,3u,3u}, {4u,4u,3u}, {4u,4u,4u}, {16u,16u,16u}, {4u,8u,4u}},
                {0u,0u,0u}, 64u, 3u}};
        for (const auto& test_case : cases) Check(test_case);
        std::puts("PASS object-sort-engine61 7 original-upper/shared-accepted-lower cases");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what()); return 1;
    }
}
