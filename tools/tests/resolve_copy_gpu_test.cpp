// GPU regression for padded FP16 resolves: initialize the placed RT before
// partial copies, and preserve pixels outside subsequent copy rectangles.
#include <plume_render_interface.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace plume { std::unique_ptr<RenderInterface> CreateD3D12Interface(); }

int main(int argc, char** argv) {
    using namespace plume;
    const bool skipInit = argc == 2 && std::strcmp(argv[1], "--uninitialized") == 0;
    auto api = CreateD3D12Interface();
    if (!api) return 2;
    auto device = api->createDevice();
    if (!device) return 2;
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    if (!queue) return 2;
    constexpr auto format = RenderFormat::R16G16B16A16_FLOAT;
    constexpr uint32_t srcWidth = 480, srcHeight = 480;
    constexpr uint32_t dstWidth = 448, dstHeight = 242, copyWidth = 432;
    constexpr uint32_t rowPitch = dstWidth * 8; // Already 256-byte aligned.
    constexpr std::array<uint16_t, 4> first = {0x3400, 0x3800, 0x3A00, 0x3C00};
    constexpr std::array<uint16_t, 4> second = {0x3C00, 0x3A00, 0x3800, 0x3400};
    for (unsigned allocation = 0; allocation < 3; ++allocation) {
        auto src = device->createTexture(RenderTextureDesc::Texture2D(srcWidth, srcHeight, 1, format, RenderTextureFlag::RENDER_TARGET));
        auto dst = device->createTexture(RenderTextureDesc::Texture2D(dstWidth, dstHeight, 1, format, RenderTextureFlag::RENDER_TARGET));
        if (!src || !dst) return 2;
        const RenderTexture* srcTarget[] = {src.get()};
        const RenderTexture* dstTarget[] = {dst.get()};
        auto srcFb = device->createFramebuffer(RenderFramebufferDesc(srcTarget, 1));
        auto dstFb = device->createFramebuffer(RenderFramebufferDesc(dstTarget, 1));
        auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(rowPitch * dstHeight));
        auto commands = queue->createCommandList();
        auto fence = device->createCommandFence();
        if (!srcFb || !dstFb || !readback || !commands || !fence) return 2;
        for (unsigned pass = 0; pass < 2; ++pass) {
            commands->begin();
            commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(src.get(), RenderTextureLayout::COLOR_WRITE));
            commands->setFramebuffer(srcFb.get());
            commands->clearColor(0, pass ? RenderColor(1, .75f, .5f, .25f) : RenderColor(.25f, .5f, .75f, 1));
            // This is deliberately once per allocation, not once per resolve.
            if (pass == 0 && !skipInit) {
                commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(dst.get(), RenderTextureLayout::COLOR_WRITE));
                commands->setFramebuffer(dstFb.get());
                commands->clearColor(0, RenderColor(0, 0, 0, 0));
            }
            commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(src.get(), RenderTextureLayout::COPY_SOURCE));
            commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(dst.get(), RenderTextureLayout::COPY_DEST));
            const RenderBox box = pass ? RenderBox{16, 8, 80, 40, 0, 1} : RenderBox{0, 0, copyWidth, dstHeight, 0, 1};
            commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(dst.get()),
                RenderTextureCopyLocation::Subresource(src.get()), box.left, box.top, 0, &box);
            commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(dst.get(), RenderTextureLayout::SHADER_READ));
            commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(dst.get(), RenderTextureLayout::COPY_SOURCE));
            commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(), format, dstWidth, dstHeight, 1, rowPitch / 8, 0),
                RenderTextureCopyLocation::Subresource(dst.get()));
            commands->end();
            const RenderCommandList* lists[] = {commands.get()};
            queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
            queue->waitForCommandFence(fence.get());
            const auto pixels = static_cast<const uint16_t*>(readback->map());
            if (!pixels) return 2;
            size_t mismatch = 0, nonzero = 0;
            for (uint32_t y = 0; y < dstHeight; ++y) {
                for (uint32_t x = 0; x < dstWidth; ++x) {
                    bool wrong = false;
                    for (unsigned c = 0; c < 4; ++c) {
                        const auto actual = pixels[(y * dstWidth + x) * 4 + c];
                        const auto expected = x >= copyWidth ? 0 :
                            pass && x >= 16 && x < 80 && y >= 8 && y < 40 ? second[c] : first[c];
                        wrong |= actual != expected;
                        nonzero += actual != 0;
                    }
                    mismatch += wrong;
                }
            }
            readback->unmap();
            std::printf("allocation=%u pass=%u initialized=%d mismatch_pixels=%zu nonzero_components=%zu\n",
                allocation, pass, !skipInit, mismatch, nonzero);
            if (mismatch) return 1;
        }
    }
    std::puts("PASS: padded FP16 resolve, partial update preservation, reallocation");
    return 0;
}
