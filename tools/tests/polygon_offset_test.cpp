// Headless D3D12 test: coplanar surfaces must separate with the guest bias.
#include <cstdio>
#include <gpu/polygon_offset.h>
#include <array>
#include <gpu/shader/dxc_compiler.h>
#include <plume_render_interface.h>
#include <stdexcept>
namespace plume {
std::unique_ptr<RenderInterface> CreateD3D12Interface();
}
int main() {
  try {
    const auto tiny = gpu::GetPolygonOffset(1u << 11, true, false, 0, -1e-10f, 0, 0);
    const auto f24 = gpu::GetPolygonOffset(1u << 11, true, true, 0, 1e-10f, 0, 0);
    if (tiny.constant != -1 || f24.constant != 8)
      throw std::runtime_error("small nonzero bias was lost");
    using namespace plume;
    auto api = CreateD3D12Interface();
    auto device = api->createDevice();
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    auto commands = queue->createCommandList();
    auto fence = device->createCommandFence();
    auto layout = device->createPipelineLayout(RenderPipelineLayoutDesc{});
    auto shader = [&](const char *source, const char *profile) {
      auto c = xenos::CompileHlsl(source, "main", profile);
      if (!c.ok)
        throw std::runtime_error(c.errors);
      return device->createShader(c.dxil.data(), c.dxil.size(), "main",
                                  RenderShaderFormat::DXIL);
    };
    auto vs = shader("float4 main(uint id:SV_VertexID):SV_Position { float2 "
                     "uv=float2((id<<1)&2,id&2); return "
                     "float4(uv*float2(2,-2)+float2(-1,1),0.5+uv.x*0.125,1); }",
                     "vs_6_0");
    auto red =
        shader("float4 main():SV_Target{return float4(1,0,0,1);}", "ps_6_0");
    auto color = device->createTexture(
        RenderTextureDesc::Texture2D(8, 2, 1, RenderFormat::R8G8B8A8_UNORM,
                                     RenderTextureFlag::RENDER_TARGET));
    auto depth = device->createTexture(
        RenderTextureDesc::Texture2D(8, 2, 1, RenderFormat::D32_FLOAT_S8_UINT,
                                     RenderTextureFlag::DEPTH_TARGET));
    auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(512));
    const RenderTexture *attachments[] = {color.get()};
    auto fb = device->createFramebuffer(
        RenderFramebufferDesc(attachments, 1, depth.get()));
    RenderGraphicsPipelineDesc desc;
    desc.pipelineLayout = layout.get();
    desc.vertexShader = vs.get();
    desc.pixelShader = red.get();
    desc.renderTargetFormat[0] = RenderFormat::R8G8B8A8_UNORM;
    desc.renderTargetCount = 1;
    desc.renderTargetBlend[0] = RenderBlendDesc::Copy();
    desc.renderTargetBlend[0].renderTargetWriteMask = 0;
    desc.depthTargetFormat = RenderFormat::D32_FLOAT_S8_UINT;
    desc.depthEnabled = true;
    desc.depthWriteEnabled = true;
    desc.depthFunction = RenderComparisonFunction::ALWAYS;
    desc.cullMode = RenderCullMode::NONE;
    auto write = device->createGraphicsPipeline(desc);
    desc.renderTargetBlend[0].renderTargetWriteMask = 15;
    desc.depthWriteEnabled = false;
    desc.depthFunction = RenderComparisonFunction::LESS;
    const float offset = 0.0001f;
    const std::array<gpu::PolygonOffset, 8> biases = {
      gpu::GetPolygonOffset(0, true, false, 0, -offset, 0, 0),
      gpu::GetPolygonOffset(1u<<11, true, false, 0, offset, 0, 0),
      gpu::GetPolygonOffset(1u<<11, true, false, 0, -offset, 0, 0),
      gpu::GetPolygonOffset((1u<<11)|(1u<<12)|1, true, false, 0, offset, 0, -offset),
      gpu::GetPolygonOffset(1u<<13, false, false, 0, -offset, 0, 0),
      gpu::GetPolygonOffset(1u<<11, false, false, 0, -offset, 0, 0),
      gpu::GetPolygonOffset(1u<<11, true, false, 16, 0, 0, 0),
      gpu::GetPolygonOffset(1u<<11, true, false, -16, 0, 0, 0)
    };
    std::array<std::unique_ptr<RenderPipeline>, 16> testPipelines;
    for (size_t i = 0; i < testPipelines.size(); ++i) {
      desc.depthFunction = i < 8 ? RenderComparisonFunction::LESS : RenderComparisonFunction::GREATER;
      desc.depthBias = biases[i % 8].constant;
      desc.slopeScaledDepthBias = biases[i % 8].slope;
      testPipelines[i] = device->createGraphicsPipeline(desc);
    }
    commands->begin();
    commands->barriers(
        RenderBarrierStage::GRAPHICS,
        RenderTextureBarrier(color.get(), RenderTextureLayout::COLOR_WRITE));
    commands->barriers(
        RenderBarrierStage::GRAPHICS,
        RenderTextureBarrier(depth.get(), RenderTextureLayout::DEPTH_WRITE));
    commands->setFramebuffer(fb.get());
    commands->clearColor(0, RenderColor(0, 0, 1, 1));
    commands->clearDepthStencil(true, true, 1, 0);
    RenderViewport vp(0, 0, 8, 2);
    commands->setViewports(&vp, 1);
    commands->setGraphicsPipelineLayout(layout.get());
    RenderRect full{0, 0, 8, 2};
    commands->setScissors(&full, 1);
    commands->setPipeline(write.get());
    commands->drawInstanced(3, 1, 0, 0);
    for (int i = 0; i < 16; ++i) {
      const int x = i % 8, y = i / 8;
      RenderRect column{x, y, x + 1, y + 1};
      commands->setScissors(&column, 1);
      commands->setPipeline(testPipelines[i].get());
      commands->drawInstanced(3, 1, 0, 0);
    }
    commands->barriers(
        RenderBarrierStage::COPY,
        RenderTextureBarrier(color.get(), RenderTextureLayout::COPY_SOURCE));
    commands->copyTextureRegion(
        RenderTextureCopyLocation::PlacedFootprint(
            readback.get(), RenderFormat::R8G8B8A8_UNORM, 8, 2, 1, 64, 0),
        RenderTextureCopyLocation::Subresource(color.get(), 0));
    commands->end();
    const RenderCommandList *lists[] = {commands.get()};
    queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
    queue->waitForCommandFence(fence.get());
    auto pixels = static_cast<const unsigned char *>(readback->map());
    bool pass = true;
    const bool redExpected[] = {false, false, true, true, true, false, false, true,
                               false, true, false, false, false, false, true, false};
    for (int i = 0; i < 16; ++i) {
      const int x = i % 8, y = i / 8;
      const auto pixel = pixels + y * 256 + x * 4;
      pass &= pixel[0] == (redExpected[i] ? 255 : 0) && pixel[1] == 0 &&
              pixel[2] == (redExpected[i] ? 0 : 255);
      std::printf("pixel %d,%d: %u %u %u\n", x, y, pixel[0], pixel[1], pixel[2]);
    }
    readback->unmap();
    // Map12: two lighting/base vertex transforms differ by about 7e-8 at
    // reversed depth 0.012. The integer approximation is too small there.
    const auto layerBias = gpu::GetPolygonOffset(1u << 11, true, true, 0, 1e-6f, 0, 0);
    const std::string prefix = "float4 main(uint id:SV_VertexID):SV_Position { float2 uv=float2((id<<1)&2,id&2); return float4(uv*float2(2,-2)+float2(-1,1),";
    auto layerVs = shader((prefix + "0.012-0.00000007,1); }").c_str(), "vs_6_0");
    auto offsetVs = shader((prefix + "0.012-0.00000007+" + std::to_string(layerBias.absolute) + ",1); }").c_str(), "vs_6_0");
    desc.vertexShader = layerVs.get();
    desc.depthFunction = RenderComparisonFunction::GREATER_EQUAL;
    desc.depthBias = layerBias.constant;
    desc.slopeScaledDepthBias = 0;
    auto legacyLayer = device->createGraphicsPipeline(desc);
    desc.vertexShader = offsetVs.get();
    desc.depthBias = 0;
    auto absoluteLayer = device->createGraphicsPipeline(desc);
    commands->begin();
    commands->barriers(RenderBarrierStage::GRAPHICS,
        RenderTextureBarrier(color.get(), RenderTextureLayout::COLOR_WRITE));
    commands->setFramebuffer(fb.get());
    commands->clearColor(0, RenderColor(0, 0, 1, 1));
    commands->clearDepthStencil(true, false, 0.012f, 0);
    RenderRect occluder{2, 0, 3, 1};
    commands->clearDepthStencil(true, false, 0.02f, 0, &occluder, 1);
    commands->setViewports(&vp, 1);
    commands->setGraphicsPipelineLayout(layout.get());
    for (int x = 0; x < 3; ++x) {
      RenderRect column{x, 0, x + 1, 1};
      commands->setScissors(&column, 1);
      commands->setPipeline(x == 0 ? legacyLayer.get() : absoluteLayer.get());
      commands->drawInstanced(3, 1, 0, 0);
    }
    commands->barriers(RenderBarrierStage::COPY,
        RenderTextureBarrier(color.get(), RenderTextureLayout::COPY_SOURCE));
    commands->copyTextureRegion(
        RenderTextureCopyLocation::PlacedFootprint(readback.get(), RenderFormat::R8G8B8A8_UNORM, 8, 2, 1, 64, 0),
        RenderTextureCopyLocation::Subresource(color.get(), 0));
    commands->end();
    queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
    queue->waitForCommandFence(fence.get());
    pixels = static_cast<const unsigned char *>(readback->map());
    for (int x = 0; x < 3; ++x) {
      const bool redLayer = x == 1;
      pass &= pixels[x * 4] == (redLayer ? 255 : 0) && pixels[x * 4 + 2] == (redLayer ? 0 : 255);
      std::printf("shallow layer %d: %u %u %u\n", x, pixels[x*4], pixels[x*4+1], pixels[x*4+2]);
    }
    readback->unmap();
    std::puts(pass ? "PASS: GPU polygon bias sign, culling, PARA and slope"
                   : "FAIL: GPU polygon bias mismatch");
    return pass ? 0 : 1;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
