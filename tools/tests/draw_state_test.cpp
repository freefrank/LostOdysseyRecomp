#include "gpu/legacy_draw_state.h"
#include <array>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace gpu::renderer;
static void Require(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
int main()
{
    std::vector<uint32_t> registers(0x5003);
    std::vector<uint8_t> mmio(registers.size() * 4);
    for (uint32_t i = 0; i < registers.size(); ++i) registers[i] = 0x80000000u + i;
    auto storeBE = [&](size_t index, uint32_t value) {
        for (size_t byte = 0; byte < 4; ++byte) mmio[index * 4 + byte] = uint8_t(value >> (24 - 8 * byte));
    };
    registers[0x43FC] = 0; storeBE(0x43FC, 0x3f800000);
    registers[0x47FF] = 0; storeBE(0x47FF, 0x40000000);
    registers[0x210F] = std::bit_cast<uint32_t>(640.0f);
    registers[0x2112] = std::bit_cast<uint32_t>(360.0f);
    DrawInfo info{4, 312, true, 0x1abc002, 156, 2, false};
    std::array<uint32_t, 3> vs{1, 2, 3}, ps{4, 5, 6};
    auto state = CaptureLegacyDrawState(info, DrawWords::Legacy(registers, mmio.data()),
        {vs, 17, 27}, {ps, 37, 47});
    Require(state.draw.indexBase == 0x1abc002 && state.draw.indexCount == 312 && state.draw.indexEndian == 2,
        "index subrange and endian are explicit and unchanged");
    Require(state.vertexShader.words.data() == vs.data() && state.pixelShader.byteHash == 47,
        "shader identity and microcode byte order preserved");
    Require(state.targets.colorInfo == std::array<uint32_t, 4>{0x80002001,0x80002003,0x80002004,0x80002005},
        "non-contiguous color target mapping");
    Require(state.targets.depthInfo == 0x80002002 && state.targets.surfaceInfo == 0x80002000, "target mapping");
    Require(state.pipeline.depthControl == 0x80002200 && state.pipeline.modeControl == 0x80002208 &&
        state.pipeline.stencilRefMaskBack == 0x8000210C && state.pipeline.clipControl == 0x80002204,
        "pipeline and backface stencil mapping");
    Require(state.viewport.scaleOffset[0] == 640.0f && state.viewport.scaleOffset[3] == 360.0f &&
        state.viewport.vertexControl == 0x80002302 && state.viewport.scissorTL == 0x80002081, "viewport mapping");
    Require(state.resolve.destinationInfo == 0x8000231B && state.resolve.depthClear == 0x8000200B &&
        state.resolve.colorClear == 0x8000200C, "resolve and clear flags are explicit");
    std::array<uint32_t,1024> vertex{}, pixel{};
    state.vertexConstants.Copy(vertex); state.pixelConstants.Copy(pixel);
    Require(vertex[1020] == 0x3f800000 && pixel[1023] == 0x40000000, "high ALU bank zero MMIO fallback");
    Require(state.AluConstantFloat(1020) == 1.0f && state.AluConstantFloat(2047) == 2.0f,
        "ALU bank split keeps diagnostic and shader values consistent");
    Require(state.fetchConstants.Read(191) == 0x800048BF && state.boolConstants.Read(7) == 0x80004907 &&
        state.loopConstants.Read(31) == 0x80004927, "fetch, bool and loop bank limits");
    Require(!state.vertexConstantRevision.Tracked() && state.legacyTraceRegisters.Size() == registers.size(),
        "uncovered legacy writes never claim revision-only reuse");
    std::array<uint32_t,3> native{1,0,3}, copy{};
    DrawWords(native).Copy(copy);
    Require(copy == native && !DrawWords(native).HasLegacyFallback(), "native zero is authoritative, not missing state");
    copy[0] = 99;
    Require(native[0] == 1, "jitter/work copies do not mutate reusable base values");
    std::array<uint32_t,5> extended{9,9,9,9,9};
    DrawWords(native).Subspan(1,100).Copy(extended);
    Require(extended == std::array<uint32_t,5>{0,3,0,0,0}, "borrowed bank bounds and partial snapshots");
    Require(DrawWords(native).Subspan(100,2).Read(0) == 0, "out-of-range view is empty");
    // Differential bulk-copy coverage against scalar Read, including an
    // unaligned BE mirror, partial banks, high constants and post-capture writes.
    for (size_t size : {size_t(0), size_t(1), size_t(15), size_t(16), size_t(17), size_t(1024)}) {
        std::vector<uint32_t> source(size);
        std::vector<uint8_t> bytes(size * 4 + 1);
        for (size_t pattern = 0; pattern < 4; ++pattern) {
            for (size_t i = 0; i < size; ++i) {
                source[i] = pattern == 0 || (i % (pattern + 1)) == 0 ? 0u : uint32_t(0x80000000u + i);
                uint32_t bits = uint32_t(0x7fc00000u + i); // retain NaN payload bits
                for (size_t b = 0; b < 4; ++b) bytes[1 + i * 4 + b] = uint8_t(bits >> (24 - b * 8));
            }
            const auto view = DrawWords::Legacy(source, bytes.data() + 1);
            for (size_t length : {size_t(0), size / 2, size, size + 3}) {
                std::vector<uint32_t> result(length, 0xdeadbeef);
                view.Copy(result);
                for (size_t i = 0; i < length; ++i)
                    Require(result[i] == view.Read(i), "bulk copy must equal scalar view including padded tail");
            }
            if (size) {
                source[size - 1] = 0;
                bytes.back() ^= 3; // unobserved guest-MMIO mutation must remain visible
                std::vector<uint32_t> result(size);
                view.Copy(result);
                Require(result.back() == view.Read(size - 1), "no revision-only reuse hides a direct MMIO write");
                result.back() = 123;
                Require(source.back() == 0, "working copy never mutates the source");
            }
        }
    }
    std::cout << "draw-state adapter contracts passed\n";
}
