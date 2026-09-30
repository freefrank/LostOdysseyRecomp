// Bounded Windows03 draw 2091 geometry/clear/crop probe on actual Vulkan GPU.
// The production postprocess mask path creates the target, records the clear
// and draw, then production propagation resolves and qualifies its fetch.
#include <gpu/fsr_alpha_postprocess_gpu.h>
#include <plume_vulkan.h>

#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace plume { std::unique_ptr<RenderInterface> CreateVulkanInterface(); }
namespace {
using namespace plume;
constexpr uint32_t Width = 320, Height = 320, ResolveWidth = 288, ResolveHeight = 161;
constexpr uint32_t ValidWidth = 285, ValidHeight = 161, SourcePitch = 512, ResolvePitch = 512;
constexpr uint64_t Frame = 12000, Epoch = 3125, SourceAllocation = 12, WriteOrdinal = 205228;
// Captured Windows03 draw 2091 (VS 2f6bbed8149a7804, PS 7c260eacff1d681d):
// index order [0,1,3,0,3,2], NDC clip xy as exact binary32 bits below.
constexpr std::array<uint32_t, 4> XBits{3212797665u, 1065235618u, 3212797665u, 1065235618u};
constexpr std::array<uint32_t, 4> YBits{1065283889u, 1065283889u, 3212628882u, 3212628882u};
constexpr float CapturedViewportWidth = 285.22186f, CapturedViewportHeight = 161.33333f;

void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

std::string VertexSource() {
    std::string vs = "struct Out { float4 position : SV_Position;\n";
    for (int i = 0; i < 16; ++i) vs += "float4 i" + std::to_string(i) + " : TEXCOORD" + std::to_string(i) + ";\n";
    vs += "};\nOut main(uint id : SV_VertexID) {\n"
          "uint4 xs=uint4(3212797665u,1065235618u,3212797665u,1065235618u);\n"
          "uint4 ys=uint4(1065283889u,1065283889u,3212628882u,3212628882u);\n"
          "Out o; o.position=float4(asfloat(xs[id]),asfloat(ys[id]),0,1);\n";
    for (int i = 0; i < 16; ++i) vs += "o.i" + std::to_string(i) + "=0.5;\n";
    return vs + "return o; }\n";
}

// DXC compiles the same production nine-tap PS variant. A constant R8=255
// input makes its 9 point taps independent of the original varying UV math;
// this isolates the recorded quad raster coverage and clear boundary.
const char* PixelPrelude = R"hlsl(
Texture2D<float4> tex2D_0 : register(t0, space1);
void main(
)hlsl";

struct Probe {
    std::unique_ptr<RenderInterface> api = CreateVulkanInterface();
    std::unique_ptr<RenderDevice> device;
    std::unique_ptr<RenderCommandQueue> queue;
    std::unique_ptr<RenderCommandList> commands;
    std::unique_ptr<RenderCommandFence> fence;
    std::unique_ptr<RenderBuffer> upload, readbackSource, readbackResolve;
    std::unique_ptr<RenderTexture> input, returnedColor;
    std::unique_ptr<RenderPipelineLayout> layout;
    std::array<std::unique_ptr<RenderDescriptorSet>, 5> sets;
    std::unique_ptr<RenderShader> blitVertex;
    gpu::fsr_alpha::PostprocessGPU post;
    gpu::fsr_alpha::PropagationGPU propagation;
    std::vector<std::shared_ptr<gpu::fsr_alpha::MaskLease>> batchUses;
    Probe() : propagation(nullptr) {
        Check(bool(api), "Vulkan interface");
        device = api->createDevice(); Check(bool(device), "GPU device");
        queue = device->createCommandQueue(RenderCommandListType::DIRECT);
        Check(bool(queue), "direct GPU queue");
        commands = queue->createCommandList(); fence = device->createCommandFence();
        Check(commands && fence, "command list and fence");
        propagation = gpu::fsr_alpha::PropagationGPU(device.get());
    }
    void Run(const char* resultPath) {
#ifdef _WIN32
        // Preload the repository's SPIR-V-capable DXC: system SDK DXC builds
        // may support DXIL only, and dxc_compiler tries their bare DLL first.
        const auto dxcPath = (std::filesystem::path(LO_FIXTURE_ROOT) /
            "tools/XenosRecomp/thirdparty/dxc-bin/bin/x64/dxcompiler.dll").wstring();
        Check(LoadLibraryW(dxcPath.c_str()) != nullptr, "load bundled SPIR-V DXC");
#endif
        constexpr uint32_t InputSize = 16, InputPitch = 256;
        upload = device->createBuffer(RenderBufferDesc::UploadBuffer(8192));
        readbackSource = device->createBuffer(RenderBufferDesc::ReadbackBuffer(size_t(SourcePitch) * Height));
        readbackResolve = device->createBuffer(RenderBufferDesc::ReadbackBuffer(size_t(ResolvePitch) * ResolveHeight));
        input = device->createTexture(RenderTextureDesc::Texture2D(InputSize, InputSize, 1, RenderFormat::R8_UNORM));
        returnedColor = device->createTexture(RenderTextureDesc::Texture2D(ResolveWidth, ResolveHeight, 1,
            RenderFormat::R8G8B8A8_UNORM));
        Check(upload && readbackSource && readbackResolve && input && returnedColor, "GPU allocations");
        auto* bytes = static_cast<uint8_t*>(upload->map()); Check(bytes, "upload map");
        std::memset(bytes, 0, 8192);
        for (unsigned y = 0; y < InputSize; ++y)
            std::memset(bytes + y * InputPitch, 255, InputSize); // constant nonzero mask alpha
        constexpr unsigned IndexOffset = 4096, ConstantOffset = 4608;
        const uint32_t indices[6]{0, 1, 3, 0, 3, 2};
        std::memcpy(bytes + IndexOffset, indices, sizeof(indices));
        upload->unmap();

        RenderDescriptorSetBuilder builders[5];
        RenderPipelineLayoutBuilder layoutBuilder;
        layoutBuilder.begin(false, false);
        layoutBuilder.addPushConstant(0, 0, 24,
            RenderShaderStageFlag::VERTEX | RenderShaderStageFlag::PIXEL);
        for (int bank = 0; bank < 5; ++bank) {
            builders[bank].begin();
            if (bank == 1) builders[bank].addTexture(0);
            builders[bank].end(); layoutBuilder.addDescriptorSet(builders[bank]);
        }
        layoutBuilder.end();
        layout = layoutBuilder.create(device.get()); Check(bool(layout), "production-compatible 5-set layout");
        for (int bank = 0; bank < 5; ++bank) {
            sets[bank] = builders[bank].create(device.get());
            Check(bool(sets[bank]), "descriptor set");
        }
        sets[1]->setTexture(0, input.get(), RenderTextureLayout::SHADER_READ);
        const std::string vertex = VertexSource();
        const auto compiled = xenos::CompileCachedHlsl(vertex, "main", "vs_6_0",
            xenos::ShaderBinaryFormat::Spirv);
        Check(compiled.ok, compiled.errors.c_str());
        blitVertex = device->createShader(compiled.bytecode.data(), compiled.bytecode.size(),
            "main", RenderShaderFormat::SPIRV);
        Check(bool(blitVertex) && post.Init(device.get(), layout.get(), blitVertex.get()),
            "production postprocess initialization");
        xenos::TranslatedShader vs{}, ps{};
        vs.hlsl = vertex; ps.isPixelShader = true; ps.hlsl = PixelPrelude;
        gpu::pipeline_cache::Key key{};
        key.vs = 0x2f6bbed8149a7804ull; key.ps = 0x7c260eacff1d681dull;
        RenderGraphicsPipelineDesc graphics{};
        graphics.primitiveTopology = RenderPrimitiveTopology::TRIANGLE_LIST;
        graphics.cullMode = RenderCullMode::NONE;
        // Prepare is asynchronous like the real renderer; prepare before recording.
        RenderPipeline* pipeline = nullptr;
        for (unsigned i = 0; i < 600 && !pipeline; ++i) {
            pipeline = post.Prepare(key, 1u, graphics, vs, ps, nullptr, 0, nullptr, 0);
            Check(post.LastError().empty(), post.LastError().c_str());
            if (!pipeline) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        Check(pipeline, "production postprocess shader/pipeline ready");
        propagation.BeginFrame(Frame, Epoch);
        commands->begin();
        commands->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(input.get(), RenderTextureLayout::COPY_DEST));
        commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(input.get()),
            RenderTextureCopyLocation::PlacedFootprint(upload.get(), RenderFormat::R8_UNORM,
                InputSize, InputSize, 1, InputPitch));
        commands->barriers(RenderBarrierStage::GRAPHICS,
            RenderTextureBarrier(input.get(), RenderTextureLayout::SHADER_READ));
        RenderIndexBufferView indexView(RenderBufferReference(upload.get(), IndexOffset),
            sizeof(indices), RenderFormat::R32_UINT);
        commands->setIndexBuffer(&indexView);
        RenderDescriptorSet* const bindings[5]{sets[0].get(), sets[1].get(), sets[2].get(),
            sets[3].get(), sets[4].get()};
        const RenderBufferReference constants[3]{{upload.get(), ConstantOffset},
            {upload.get(), ConstantOffset + 256}, {upload.get(), ConstantOffset + 512}};
        const RenderViewport viewport(0, 0, CapturedViewportWidth, CapturedViewportHeight);
        const RenderRect scissor(0, 0, ValidWidth, ValidHeight);
        auto mask = post.Draw(commands.get(), pipeline, Width, Height, constants, bindings, 5,
            viewport, scissor, true, 6, 0, batchUses);
        Check(bool(mask), "production Draw clear and six-index quad");
        // The captured clear covers 320x320, while its declared source valid
        // region is [0,0,285,161]. The inset [1,1,284,160] is only a
        // conservative fully-covered rect, not exact pixel-center coverage.
        gpu::fsr_alpha::SourceMask stage{Frame, Epoch, SourceAllocation, 2091,
            Width, Height, {0, 0, ValidWidth, ValidHeight},
            gpu::fsr_alpha::SourceStage::Downsample, mask};
        propagation.PublishPostprocess(stage);
        auto resolved = propagation.RecordResolve(commands.get(), Frame, Epoch, SourceAllocation,
            Width, Height, 0x123400, 6, 19, WriteOrdinal, ResolveWidth, ResolveHeight,
            {0, 0, ResolveWidth, ResolveHeight}, "copy", batchUses);
        Check(resolved.copied && resolved.version.validRect == gpu::fsr_alpha::MaskRect{0, 0, 285, 161},
            "production resolve intersects recorded copy with valid rect");
        auto rejected = propagation.RecordFetchView(commands.get(), Frame, Epoch, 0x123400, 6,
            19, WriteOrdinal, returnedColor.get(), 286, 161, batchUses);
        Check(!rejected && rejected.reason == "uninitialized_fetch_crop" && !rejected.mask,
            "production fetch rejects x=285 padding outside valid rect");
        auto accepted = propagation.RecordFetchView(commands.get(), Frame, Epoch, 0x123400, 6,
            19, WriteOrdinal, returnedColor.get(), 285, 161, batchUses);
        Check(bool(accepted) && accepted.version.writeOrdinal == WriteOrdinal,
            "production fetch accepts maximum valid crop");
        commands->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(mask->texture.get(), RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readbackSource.get(),
            RenderFormat::R8_UNORM, Width, Height, 1, SourcePitch),
            RenderTextureCopyLocation::Subresource(mask->texture.get()));
        commands->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(resolved.version.mask->texture.get(), RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readbackResolve.get(),
            RenderFormat::R8_UNORM, ResolveWidth, ResolveHeight, 1, ResolvePitch),
            RenderTextureCopyLocation::Subresource(resolved.version.mask->texture.get()));
        commands->end();
        const RenderCommandList* lists[]{commands.get()};
        queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
        auto& vkFence = static_cast<VulkanCommandFence&>(*fence);
        auto& vkDevice = static_cast<VulkanDevice&>(*device);
        Check(vkWaitForFences(vkDevice.vk, 1, &vkFence.vk, VK_TRUE, 5'000'000'000ull) == VK_SUCCESS,
            "actual GPU completed readback fence");

        // Independent pixel-center computation from captured binary32 clip
        // coordinates and binary32 viewport, not the policy's written rect.
        const double left = (double(std::bit_cast<float>(XBits[0])) + 1.) * double(CapturedViewportWidth) / 2.;
        const double right = (double(std::bit_cast<float>(XBits[1])) + 1.) * double(CapturedViewportWidth) / 2.;
        const double top = (1. - double(std::bit_cast<float>(YBits[0]))) * double(CapturedViewportHeight) / 2.;
        const double bottom = (1. - double(std::bit_cast<float>(YBits[2]))) * double(CapturedViewportHeight) / 2.;
        Check(left > 0. && left < .5 && right > 283.5 && right < 284.5 &&
            top > 0. && top < .5 && bottom > 159.5 && bottom < 160.5,
            "captured geometry has unambiguous pixel-center thresholds");
        const auto expected = [&](uint32_t x, uint32_t y) -> uint8_t {
            const double cx = double(x) + .5, cy = double(y) + .5;
            return x < ValidWidth && y < ValidHeight &&
                cx > left && cx < right && cy > top && cy < bottom ? 255 : 0;
        };
        auto* source = static_cast<const uint8_t*>(readbackSource->map());
        auto* copy = static_cast<const uint8_t*>(readbackResolve->map());
        Check(source && copy, "map completed GPU readback");
        uint32_t sourceFailures = 0, copyFailures = 0, nonzero = 0;
        for (uint32_t y = 0; y < Height; ++y) for (uint32_t x = 0; x < Width; ++x) {
            sourceFailures += source[y * SourcePitch + x] != expected(x, y);
            nonzero += source[y * SourcePitch + x] != 0;
        }
        for (uint32_t y = 0; y < ResolveHeight; ++y)
            for (uint32_t x = 0; x < ResolveWidth; ++x)
                copyFailures += copy[y * ResolvePitch + x] != expected(x, y);
        // Include both sides of every audited boundary, the inner clear border,
        // the scissor exterior, and the source/resolve padding.
        constexpr std::array<uint32_t, 9> xs{0, 1, 2, 283, 284, 285, 287, 319, 150};
        constexpr std::array<uint32_t, 9> ys{0, 1, 2, 159, 160, 161, 200, 319, 80};
        for (auto y : ys) for (auto x : xs)
            Check(source[y * SourcePitch + x] == expected(x, y), "boundary sample mismatch");
        for (auto y : ys) if (y < ResolveHeight) for (auto x : xs) if (x < ResolveWidth)
            Check(copy[y * ResolvePitch + x] == expected(x, y), "resolved boundary sample mismatch");
        std::array<uint32_t, 9> row{}, column{};
        for (size_t i = 0; i < xs.size(); ++i) row[i] = source[80 * SourcePitch + xs[i]];
        for (size_t i = 0; i < ys.size(); ++i) column[i] = source[ys[i] * SourcePitch + 150];
        readbackSource->unmap(); readbackResolve->unmap();
        Check(sourceFailures == 0 && copyFailures == 0 && nonzero == 284 * 160,
            "byte-exact source and resolve coverage including cleared padding");
        std::ofstream out(resultPath, std::ios::trunc); Check(bool(out), "result open");
        out << "{\"status\":\"passed\",\"capture\":\"windows03 draw 2091\","
            << "\"gpu_fence_completed\":true,\"source_pixels\":" << Width * Height
            << ",\"resolve_pixels\":" << ResolveWidth * ResolveHeight
            << ",\"nonzero_pixels\":" << nonzero
            << ",\"source_mismatch_pixels\":" << sourceFailures
            << ",\"resolve_mismatch_pixels\":" << copyFailures
            << ",\"boundary_x\":[0,1,2,283,284,285,287,319,150],\"row_y80\":[";
        for (size_t i = 0; i < row.size(); ++i) out << (i ? "," : "") << row[i];
        out << "],\"boundary_y\":[0,1,2,159,160,161,200,319,80],\"column_x150\":[";
        for (size_t i = 0; i < column.size(); ++i) out << (i ? "," : "") << column[i];
        out << "],\"valid_rect\":[0,0,285,161],\"rejected_crop\":[286,161],"
            << "\"rejection\":\"uninitialized_fetch_crop\",\"accepted_crop\":[285,161],"
            << "\"quad_bounds_pixel\":[" << left << ',' << top << ',' << right << ',' << bottom
            << "]}\n";
        Check(bool(out), "result write");
        std::printf("fence=complete source=320x320 resolve=288x161 nonzero=%u source_mismatches=%u resolve_mismatches=%u crop286=rejected\n",
            nonzero, sourceFailures, copyFailures);
    }
};
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: LoFsrRasterEdgeGpuTest result.json\n"); return 2; }
    try { Probe{}.Run(argv[1]); return 0; }
    catch (const std::exception& e) { std::fprintf(stderr, "raster-edge GPU fixture failed: %s\n", e.what()); return 1; }
}
