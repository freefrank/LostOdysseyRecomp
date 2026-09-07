// Synthetic numerical sampling contract, using production translated helpers.
#include <gpu/shader/xenos_translator.h>
#include <gpu/shader/xenos_shader_code.h>
#include <gpu/shader/dxc_compiler.h>
#include <plume_render_interface.h>
#include <plume_render_interface_builders.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
namespace plume { std::unique_ptr<RenderInterface> CreateD3D12Interface(); }
namespace {
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::string ExtractFunction(const std::string& source, const char* signature) {
    const auto start = source.find(signature);
    Require(start != std::string::npos, "production helper missing");
    size_t end = source.find('{', start); int depth = 0;
    do { if (source[end] == '{') ++depth; if (source[end] == '}') --depth; ++end; }
    while (depth && end < source.size());
    Require(!depth, "unterminated production helper");
    return source.substr(start, end - start) + "\n";
}
std::string Source() {
    using namespace xenos;
    std::array<uint32_t, 6> code{};
    ControlFlowExecInstruction cf{};
    cf.address = 1; cf.count = 1; cf.sequence = 1; cf.opcode = ControlFlowOpcode::ExecEnd;
    std::memcpy(code.data(), &cf, 6);
    TextureFetchInstruction fetch{};
    fetch.opcode = FetchOpcode::TextureFetch; fetch.dimension = TextureDimension::Texture2D;
    fetch.constIndex = 31; fetch.dstSwizzle = 0x688; fetch.srcSwizzle = 4;
    std::memcpy(code.data() + 3, &fetch, 12);
    const auto translated = TranslateShader(code.data(), uint32_t(code.size()), true);
    Require(translated.errors.empty(), "fixture translation failed");
    std::string source = R"(
Texture2D<float4> tex : register(t0);
SamplerState linearClamp : register(s0);
cbuffer Dimensions : register(b0) { uint4 xeTextureSize[8]; };
#define XE_SAMPLE(t,s,uv) t.Sample(s,uv)
)";
    for (const char* name : {"float2 XeTextureDimensions(", "float4 XeTex2D(",
             "float4 XeTex2DLevelZero(", "float2 XeWeights2D("})
        source += ExtractFunction(translated.hlsl, name);
    source += R"(
float4 vertex(uint id : SV_VertexID) : SV_Position {
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2,-2) + float2(-1,1),0,1);
}
float4 pixel(float4 position : SV_Position) : SV_Target {
    uint test = uint(position.x);
    bool denormalized = (test == 2 || test == 3 || test == 5);
    float2 uv = denormalized ? float2(1.75,3.75) : float2(.21875,.46875);
    float2 offset = float2(1.5,-.5);
    if (test >= 4) return float4(XeWeights2D(tex,uv,offset,31u,denormalized),0,1);
    if (test == 1 || test == 3) return XeTex2DLevelZero(tex,linearClamp,uv,offset,31u,denormalized);
    return XeTex2D(tex,linearClamp,uv,offset,31u,denormalized);
})";
    return source;
}
}
int main() {
    try {
        using namespace plume;
        const auto source = Source();
        const auto vertex = xenos::CompileHlsl(source, "vertex", "vs_6_0");
        const auto pixel = xenos::CompileHlsl(source, "pixel", "ps_6_0");
        if (!vertex.ok || !pixel.ok) throw std::runtime_error(vertex.errors + pixel.errors);
        auto api = CreateD3D12Interface(); auto device = api->createDevice();
        Require(bool(device), "D3D12 device unavailable");
        // Invalid dimensions are rejected before a large allocation is attempted.
        // This exercises the native factory failure contract without OOM stress.
        auto rejected = device->createTexture(RenderTextureDesc::Texture2D(16385, 1, 1, RenderFormat::R8G8B8A8_UNORM));
        Require(!rejected, "failed D3D12 texture allocation returned a non-null wrapper");
        auto recovery = device->createTexture(RenderTextureDesc::Texture2D(1, 1, 1, RenderFormat::R8G8B8A8_UNORM));
        Require(bool(recovery), "normal texture allocation failed after rejected dimensions");
        recovery.reset();
        std::puts("PASS: invalid-width allocation returns null; normal allocation still succeeds (expected CreateResource error above)");
        auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
        auto commands = queue->createCommandList(); auto fence = device->createCommandFence();
        auto vs = device->createShader(vertex.dxil.data(), vertex.dxil.size(), "vertex", RenderShaderFormat::DXIL);
        auto ps = device->createShader(pixel.dxil.data(), pixel.dxil.size(), "pixel", RenderShaderFormat::DXIL);
        RenderDescriptorSetBuilder set; set.begin(); set.addTexture(0); set.addSampler(0); set.end();
        RenderPipelineLayoutBuilder layoutBuilder; layoutBuilder.begin(false, false);
        layoutBuilder.addPushConstant(0, 0, 128, RenderShaderStageFlag::PIXEL);
        layoutBuilder.addDescriptorSet(set); layoutBuilder.end();
        auto layout = layoutBuilder.create(device.get());
        RenderSamplerDesc samplerDesc;
        samplerDesc.addressU = samplerDesc.addressV = samplerDesc.addressW = RenderTextureAddressMode::CLAMP;
        auto sampler = device->createSampler(samplerDesc);
        constexpr auto format = RenderFormat::R32G32B32A32_FLOAT;
        RenderGraphicsPipelineDesc desc; desc.pipelineLayout = layout.get();
        desc.vertexShader = vs.get(); desc.pixelShader = ps.get(); desc.renderTargetCount = 1;
        desc.renderTargetFormat[0] = format; desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
        desc.cullMode = RenderCullMode::NONE;
        auto pipeline = device->createGraphicsPipeline(desc); Require(bool(pipeline), "sampling pipeline creation failed");
        auto submit = [&] { commands->end(); const RenderCommandList* lists[]{commands.get()};
            queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get()); queue->waitForCommandFence(fence.get()); };
        unsigned checks = 0; double worstError = 0;
        for (uint32_t physical : {8u, 12u, 24u}) {
            auto texture = device->createTexture(RenderTextureDesc::Texture2D(physical, physical, 1, format));
            const uint32_t rowBytes = (physical * 16 + 255) & ~255u;
            auto upload = device->createBuffer(RenderBufferDesc::UploadBuffer(rowBytes * physical));
            auto* data = static_cast<float*>(upload->map());
            // A linear field has a known analytic bilinear sample at any interior
            // UV. Physical gradients intentionally differ at each resolution.
            for (unsigned y = 0; y < physical; ++y) for (unsigned x = 0; x < physical; ++x) {
                float* p = data + y * (rowBytes / 4) + x * 4;
                p[0] = (x + .5f) / physical; p[1] = (y + .5f) / physical; p[2] = 0; p[3] = 1;
            }
            upload->unmap();
            commands->begin(); commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(texture.get(), RenderTextureLayout::COPY_DEST));
            commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(texture.get()),
                RenderTextureCopyLocation::PlacedFootprint(upload.get(), format, physical, physical, 1, rowBytes / 16));
            submit();
            for (bool resolved : {false, true}) {
                auto target = device->createTexture(RenderTextureDesc::Texture2D(6, 1, 1, format, RenderTextureFlag::RENDER_TARGET));
                auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(256));
                const RenderTexture* attachments[]{target.get()};
                auto framebuffer = device->createFramebuffer(RenderFramebufferDesc(attachments, 1));
                auto descriptors = set.create(device.get());
                descriptors->setTexture(0, texture.get(), RenderTextureLayout::SHADER_READ); descriptors->setSampler(1, sampler.get());
                std::array<uint32_t, 32> dimensions{}; if (resolved) dimensions[31] = 8 | (8 << 16);
                commands->begin(); commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(texture.get(), RenderTextureLayout::SHADER_READ));
                commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(target.get(), RenderTextureLayout::COLOR_WRITE));
                commands->setFramebuffer(framebuffer.get()); RenderViewport viewport(0, 0, 6, 1); RenderRect scissor(0, 0, 6, 1);
                commands->setViewports(&viewport, 1); commands->setScissors(&scissor, 1);
                commands->setGraphicsPipelineLayout(layout.get()); commands->setPipeline(pipeline.get());
                commands->setGraphicsPushConstants(0, dimensions.data()); commands->setGraphicsDescriptorSet(descriptors.get(), 0);
                commands->drawInstanced(3, 1, 0, 0);
                commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(target.get(), RenderTextureLayout::COPY_SOURCE));
                commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(), format, 6, 1, 1, 16), RenderTextureCopyLocation::Subresource(target.get()));
                submit();
                const auto* result = static_cast<const float*>(readback->map()); const double logical = resolved ? 8 : physical;
                for (unsigned test = 0; test < 6; ++test) {
                    const bool denormalized = test == 2 || test == 3 || test == 5;
                    const double x = denormalized ? 1.75 : .21875 * logical, y = denormalized ? 3.75 : .46875 * logical;
                    const auto fraction = [](double value) { return value - std::floor(value); };
                    const double expected[]{test >= 4 ? fraction(x + 1) : (x + 1.5) / logical,
                        test >= 4 ? fraction(y - 1) : (y - .5) / logical, 0, 1};
                    for (unsigned c = 0; c < 4; ++c) {
                        const double error = std::abs(result[test * 4 + c] - expected[c]); worstError = std::max(error, worstError);
                        if (!std::isfinite(result[test * 4 + c]) || error > .0001) {
                            std::fprintf(stderr, "physical=%u resolved=%u test=%u channel=%u actual=%.8f expected=%.8f\n", physical, resolved, test, c, result[test * 4 + c], expected[c]);
                            throw std::runtime_error("guest texel sampling contract mismatch");
                        }
                    }
                    ++checks;
                }
                readback->unmap();
            }
        }
        std::printf("PASS: %u production-helper GPU samples at 1x/1.5x/3x; max error %.8f; synthetic contract only\n", checks, worstError);
        return 0;
    } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
