// Headless raster test: stencil write with a nonzero reference, then masked
// lighting.
#include <cstdio>
#include <gpu/depth_format.h>
#include <gpu/shader/dxc_compiler.h>
#include <plume_render_interface.h>
#include <stdexcept>
namespace plume {
std::unique_ptr<RenderInterface> CreateD3D12Interface();
}
int main() {
  try {
    static_assert(gpu::PackDepth24Unorm(0.0f) == 0);
    static_assert(gpu::PackDepth24Unorm(0.5f) == 0x800000);
    static_assert(gpu::PackDepth24Unorm(1.0f) == 0xFFFFFF);
    static_assert(gpu::PackDepth24Unorm(0x1.fffffep-1f) == 0xFFFFFE);
    static_assert(((gpu::PackDepth24Unorm(1.0f) << 8) | 0xFF) == 0xFFFFFFFF);
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
      return device->createShader(c.bytecode.data(), c.bytecode.size(), "main",
                                  RenderShaderFormat::DXIL);
    };
    auto vs = shader("float4 main(uint id:SV_VertexID):SV_Position { float2 "
                     "uv=float2((id<<1)&2,id&2); return "
                     "float4(uv*float2(2,-2)+float2(-1,1),0,1); }",
                     "vs_6_0");
    auto red =
        shader("float4 main():SV_Target{return float4(1,0,0,1);}", "ps_6_0");
    auto blue =
        shader("float4 main():SV_Target{return float4(0,0,1,1);}", "ps_6_0");
    auto color = device->createTexture(
        RenderTextureDesc::Texture2D(8, 1, 1, RenderFormat::R8G8B8A8_UNORM,
                                     RenderTextureFlag::RENDER_TARGET));
    auto depth = device->createTexture(
        RenderTextureDesc::Texture2D(8, 1, 1, RenderFormat::D32_FLOAT_S8_UINT,
                                     RenderTextureFlag::DEPTH_TARGET));
    auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(256));
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
    desc.depthFunction = RenderComparisonFunction::ALWAYS;
    desc.stencilEnabled = true;
    desc.stencilReference = 3;
    desc.stencilReadMask = 255;
    desc.stencilWriteMask = 255;
    desc.stencilFrontFace.compareFunction = RenderComparisonFunction::ALWAYS;
    desc.stencilFrontFace.passOp = RenderStencilOp::REPLACE;
    desc.stencilBackFace = desc.stencilFrontFace;
    auto write = device->createGraphicsPipeline(desc);
    desc.renderTargetBlend[0].renderTargetWriteMask = 15;
    desc.stencilWriteMask = 0;
    desc.stencilFrontFace.compareFunction = RenderComparisonFunction::EQUAL;
    desc.stencilFrontFace.passOp = RenderStencilOp::KEEP;
    desc.stencilBackFace = desc.stencilFrontFace;
    auto equal = device->createGraphicsPipeline(desc);
    desc.pixelShader = blue.get();
    desc.stencilFrontFace.compareFunction = RenderComparisonFunction::NOT_EQUAL;
    desc.stencilBackFace = desc.stencilFrontFace;
    auto notEqual = device->createGraphicsPipeline(desc);
    commands->begin();
    commands->barriers(
        RenderBarrierStage::GRAPHICS,
        RenderTextureBarrier(color.get(), RenderTextureLayout::COLOR_WRITE));
    commands->barriers(
        RenderBarrierStage::GRAPHICS,
        RenderTextureBarrier(depth.get(), RenderTextureLayout::DEPTH_WRITE));
    commands->setFramebuffer(fb.get());
    commands->clearColor(0, RenderColor(0, 0, 0, 1));
    commands->clearDepthStencil(true, true, 1, 0);
    RenderViewport vp(0, 0, 8, 1);
    commands->setViewports(&vp, 1);
    commands->setGraphicsPipelineLayout(layout.get());
    RenderRect left{0, 0, 4, 1}, full{0, 0, 8, 1};
    commands->setScissors(&left, 1);
    commands->setPipeline(write.get());
    commands->drawInstanced(3, 1, 0, 0);
    commands->setScissors(&full, 1);
    commands->setPipeline(equal.get());
    commands->drawInstanced(3, 1, 0, 0);
    commands->setPipeline(notEqual.get());
    commands->drawInstanced(3, 1, 0, 0);
    commands->barriers(
        RenderBarrierStage::COPY,
        RenderTextureBarrier(color.get(), RenderTextureLayout::COPY_SOURCE));
    commands->copyTextureRegion(
        RenderTextureCopyLocation::PlacedFootprint(
            readback.get(), RenderFormat::R8G8B8A8_UNORM, 8, 1, 1, 64, 0),
        RenderTextureCopyLocation::Subresource(color.get(), 0));
    commands->end();
    const RenderCommandList *lists[] = {commands.get()};
    queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
    queue->waitForCommandFence(fence.get());
    auto pixels = static_cast<const unsigned char *>(readback->map());
    bool pass = true;
    for (int x = 0; x < 8; ++x) {
      pass &= pixels[x * 4] == (x < 4 ? 255 : 0) && pixels[x * 4 + 1] == 0 &&
              pixels[x * 4 + 2] == (x < 4 ? 0 : 255);
      std::printf("pixel %d: %u %u %u\n", x, pixels[x * 4], pixels[x * 4 + 1],
                  pixels[x * 4 + 2]);
    }
    readback->unmap();
    // A stale shadow PS exports depth=1, while the volume's geometric depth
    // is 0. With GREATER_EQUAL against 0.5 these take opposite stencil paths.
    // Depth-only rendering must therefore bind no guest pixel shader.
    auto exportedDepth = shader(
        "void main(out float4 c:SV_Target,out float z:SV_Depth) "
        "{ c=0; z=1; }", "ps_6_0");
    for (bool stalePixelShader : {true, false}) {
      RenderGraphicsPipelineDesc volumeDesc = desc;
      volumeDesc.pixelShader = stalePixelShader ? exportedDepth.get() : nullptr;
      volumeDesc.renderTargetBlend[0].renderTargetWriteMask = 0;
      volumeDesc.depthEnabled = true;
      volumeDesc.depthWriteEnabled = false;
      volumeDesc.depthFunction = RenderComparisonFunction::GREATER_EQUAL;
      volumeDesc.stencilWriteMask = 255;
      volumeDesc.stencilFrontFace.compareFunction = RenderComparisonFunction::ALWAYS;
      volumeDesc.stencilFrontFace.passOp = RenderStencilOp::KEEP;
      volumeDesc.stencilFrontFace.depthFailOp = RenderStencilOp::REPLACE;
      volumeDesc.stencilBackFace = volumeDesc.stencilFrontFace;
      auto volume = device->createGraphicsPipeline(volumeDesc);
      commands->begin();
      commands->barriers(RenderBarrierStage::GRAPHICS,
          RenderTextureBarrier(color.get(), RenderTextureLayout::COLOR_WRITE));
      commands->setFramebuffer(fb.get());
      commands->clearColor(0, RenderColor(0, 0, 0, 1));
      commands->clearDepthStencil(true, true, 0.5f, 0);
      commands->setViewports(&vp, 1);
      commands->setScissors(&full, 1);
      commands->setGraphicsPipelineLayout(layout.get());
      commands->setPipeline(volume.get());
      commands->drawInstanced(3, 1, 0, 0);
      commands->setPipeline(equal.get());
      commands->drawInstanced(3, 1, 0, 0);
      commands->setPipeline(notEqual.get());
      commands->drawInstanced(3, 1, 0, 0);
      commands->barriers(RenderBarrierStage::COPY,
          RenderTextureBarrier(color.get(), RenderTextureLayout::COPY_SOURCE));
      commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(
          readback.get(), RenderFormat::R8G8B8A8_UNORM, 8, 1, 1, 64, 0),
          RenderTextureCopyLocation::Subresource(color.get(), 0));
      commands->end();
      queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
      queue->waitForCommandFence(fence.get());
      pixels = static_cast<const unsigned char *>(readback->map());
      bool casePass = true;
      for (int x = 0; x < 8; ++x) {
        casePass &= pixels[x * 4] == (stalePixelShader ? 0 : 255) &&
                    pixels[x * 4 + 1] == 0 &&
                    pixels[x * 4 + 2] == (stalePixelShader ? 255 : 0);
      }
      readback->unmap();
      pass &= casePass;
      std::printf("%s: %s stencil uses %s depth\n", casePass ? "PASS" : "FAIL",
          stalePixelShader ? "stale PS" : "depth-only",
          stalePixelShader ? "exported" : "geometric");
    }
    std::puts(pass ? "PASS: nonzero stencil reference masks lighting"
                   : "FAIL: stencil mask mismatch");
    return pass ? 0 : 1;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "%s\n", e.what());
    return 1;
  }
}
