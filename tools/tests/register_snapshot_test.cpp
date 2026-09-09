#include <gpu/register_snapshot.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
struct BigEndianWord {
    std::array<uint8_t, 4> bytes{};
    mutable unsigned reads = 0;
    explicit operator uint32_t() const {
        ++reads;
        return uint32_t(bytes[0]) << 24 | uint32_t(bytes[1]) << 16 |
            uint32_t(bytes[2]) << 8 | bytes[3];
    }
    void Set(uint32_t value) {
        bytes = {uint8_t(value >> 24), uint8_t(value >> 16), uint8_t(value >> 8), uint8_t(value)};
    }
};
void Check(bool condition) { if (!condition) { std::fputs("register snapshot mismatch\n", stderr); std::exit(1); } }
}
int main() {
    std::vector<uint32_t> registers(0x5003);
    std::vector<BigEndianWord> mmio(registers.size());
    for (size_t i = 0; i < registers.size(); ++i) {
        registers[i] = i % 3 ? uint32_t(i * 2654435761u) : 0;
        mmio[i].Set(uint32_t(i * 2246822519u) ^ 0x80ff0100u);
    }
    unsigned checked = 0;
    // Full VS/PS banks, mixed zero/nonzero values, both ends and an invalid start.
    for (uint32_t first : {0u, 0x4000u, 0x4400u, 0x4fffu, 0x5003u, 0xffffffffu}) {
        for (size_t count : {size_t(0), size_t(1), size_t(1024), size_t(2048)}) {
            std::vector<uint32_t> output(count + 2, 0xdeadbeef);
            for (auto& word : mmio) word.reads = 0;
            gpu::CopyRegisterSnapshot(std::span<const uint32_t>(registers), first,
                std::span<uint32_t>(output).subspan(1, count), first < mmio.size() ? mmio.data() + first : nullptr);
            Check(output.front() == 0xdeadbeef && output.back() == 0xdeadbeef);
            for (size_t i = 0; i < count; ++i) {
                const uint64_t index = uint64_t(first) + i;
                uint32_t expected = 0;
                if (index < registers.size()) {
                    Check(mmio[index].reads == (registers[index] == 0 ? 1u : 0u));
                    expected = registers[index] ? registers[index] : uint32_t(mmio[index]);
                }
                Check(output[i + 1] == expected); ++checked;
            }
        }
    }
    // Direct MMIO stores must be observed afresh where the register bank is zero.
    std::array<uint32_t, 3> output{};
    for (uint32_t value : {0u, 0xffffffffu, 0x80000000u, 0x12345678u}) {
        registers[0] = 0; mmio[0].Set(value);
        registers[1] = value; mmio[1].Set(~value);
        gpu::CopyRegisterSnapshot(std::span<const uint32_t>(registers), 0, std::span<uint32_t>(output), mmio.data());
        Check(output[0] == value && output[1] == (value ? value : ~value)); ++checked;
    }
    std::printf("register snapshot: %u ordered values and MMIO/bounds checks passed\n", checked);
}
