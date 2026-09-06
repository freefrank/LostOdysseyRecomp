// Driver regression: large rectangle lists must preserve cleared and untouched pixels.
#include <gpu/depth_clear_layout.h>
#include <plume_render_interface.h>
#include <cstdio>
#include <vector>
namespace plume { std::unique_ptr<RenderInterface> CreateD3D12Interface(); }
int main() {
    using namespace plume;
    constexpr uint32_t width = 1280, height = 736;
    auto api = CreateD3D12Interface();
    auto device = api->createDevice();
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    for (const uint32_t count : {0u, 1u, 16u, 17u, 720u, 721u}) {
        auto depth = device->createTexture(RenderTextureDesc::Texture2D(
            width, height, 1, RenderFormat::D32_FLOAT, RenderTextureFlag::DEPTH_TARGET));
        auto fb = device->createFramebuffer(RenderFramebufferDesc(nullptr, 0, depth.get()));
        auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(width * height * 4));
        auto commands = queue->createCommandList();
        auto fence = device->createCommandFence();
        std::vector<RenderRect> rects;
        if (count == 720) {
            for (const auto& r : gpu::renderer::MapDepthClear(640, 2, {0, 0, 640, 360}, width, height, 0))
                rects.push_back({r.left, r.top, r.right, r.bottom});
            if (rects.size() != count) return 2;
        } else {
            for (uint32_t i = 0; i < count; ++i) {
                const int x = int((i % 320) * 4), y = int((i / 320) * 4);
                rects.push_back({x, y, x + 2, y + 2});
            }
        }
        std::vector<float> expected(width * height, count ? 1.0f : 0.25f);
        for (const auto& r : rects)
            for (int y = r.top; y < r.bottom; ++y)
                for (int x = r.left; x < r.right; ++x) expected[y * width + x] = 0.25f;
        commands->begin();
        commands->barriers(RenderBarrierStage::GRAPHICS, RenderTextureBarrier(depth.get(), RenderTextureLayout::DEPTH_WRITE));
        commands->setFramebuffer(fb.get());
        commands->clearDepthStencil(true, false, 1, 0);
        std::printf("clear %u rectangles\n", count); std::fflush(stdout);
        commands->clearDepthStencil(true, false, 0.25f, 0, rects.data(), count);
        commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(depth.get(), RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(
            readback.get(), RenderFormat::R32_FLOAT, width, height, 1, width, 0),
            RenderTextureCopyLocation::Subresource(depth.get(), 0));
        commands->end();
        const RenderCommandList* lists[] = {commands.get()};
        queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
        queue->waitForCommandFence(fence.get());
        const auto values = static_cast<const float*>(readback->map());
        size_t errors = 0;
        for (size_t i = 0; i < expected.size(); ++i) errors += values[i] != expected[i];
        readback->unmap();
        std::printf("%u rectangles: %zu mismatches across %zu depth pixels\n", count, errors, expected.size());
        if (errors) return 1;
    }
    std::puts("PASS: full clear, batch boundaries, sparse isolation and 720 tile rectangles");
}
