#include <plume_render_interface.h>
#include <cstdio>
#include <cstdint>

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

namespace plume {
std::unique_ptr<RenderInterface> CreateMetalInterface();
}

static uint32_t ExpectedPixel(uint32_t x, uint32_t y)
{
    // Different channels and rows expose missing copies and incorrect row strides.
    return 0xff000000u | ((x * 13u & 255u) << 16) | ((y * 29u & 255u) << 8) |
        ((x + y * 7u) & 255u);
}

static bool CheckPixels(plume::RenderBuffer* readback, uint32_t width, uint32_t height,
    uint32_t rowPixels, const char* direction)
{
    const auto* pixels = static_cast<const uint32_t*>(readback->map());
    if (!pixels) return false;
    bool pass = true;
    for (uint32_t y = 0; y < height && pass; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint32_t expected = ExpectedPixel(x, y);
            if (pixels[y * rowPixels + x] != expected) {
                std::fprintf(stderr, "%s pixel (%u,%u): actual=%08x expected=%08x\n",
                    direction, x, y, pixels[y * rowPixels + x], expected);
                pass = false;
                break;
            }
        }
    }
    readback->unmap();
    std::printf("%s: %s\n", direction, pass ? "PASS" : "FAIL");
    return pass;
}

static int TestCopies(NSWindow* window, CAMetalLayer* layer)
{
    using namespace plume;
    auto api = CreateMetalInterface();
    auto device = api ? api->createDevice() : nullptr;
    auto queue = device ? device->createCommandQueue(RenderCommandListType::DIRECT) : nullptr;
    if (!queue) return 1;
    std::printf("Metal GPU: %s\n", device->getDescription().name.c_str());

    RenderWindow renderWindow{(__bridge void*)window, (__bridge void*)layer};
    constexpr auto format = RenderFormat::B8G8R8A8_UNORM;
    auto swap = queue->createSwapChain(RenderSwapChainDesc(renderWindow, format, 3));
    auto acquired = device->createCommandSemaphore();
    uint32_t index = 0;
    if (!swap || swap->isEmpty() || !acquired || !swap->acquireTexture(acquired.get(), &index)) {
        std::fprintf(stderr, "Could not acquire a real CAMetalLayer drawable\n");
        return 1;
    }
    auto* drawable = swap->getTexture(index);
    const uint32_t width = swap->getWidth(), height = swap->getHeight();
    const uint32_t rowPixels = (width + 63u) & ~63u;
    const uint64_t bytes = uint64_t(rowPixels) * height * sizeof(uint32_t);
    auto source = device->createTexture(RenderTextureDesc::Texture2D(width, height, 1, format));
    auto destination = device->createTexture(RenderTextureDesc::Texture2D(width, height, 1, format));
    auto upload = device->createBuffer(RenderBufferDesc::UploadBuffer(bytes));
    auto drawableReadback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(bytes));
    auto textureReadback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(bytes));
    auto commands = queue->createCommandList();
    auto fence = device->createCommandFence();
    if (!drawable || !source || !destination || !upload || !drawableReadback ||
        !textureReadback || !commands || !fence) return 1;
    auto* data = static_cast<uint32_t*>(upload->map());
    if (!data) return 1;
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < rowPixels; ++x)
            data[y * rowPixels + x] = x < width ? ExpectedPixel(x, y) : 0;
    upload->unmap();

    commands->begin();
    commands->barriers(RenderBarrierStage::COPY,
        RenderTextureBarrier(source.get(), RenderTextureLayout::COPY_DEST));
    commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(source.get()),
        RenderTextureCopyLocation::PlacedFootprint(upload.get(), format, width, height, 1, rowPixels));
    commands->barriers(RenderBarrierStage::COPY,
        RenderTextureBarrier(source.get(), RenderTextureLayout::COPY_SOURCE));
    commands->barriers(RenderBarrierStage::COPY,
        RenderTextureBarrier(drawable, RenderTextureLayout::COPY_DEST));
    // #651: a swapchain RenderTexture is a drawable, with a different native
    // representation from an ordinary device-created texture. Exercise both ends.
    commands->copyTexture(drawable, source.get());
    commands->barriers(RenderBarrierStage::COPY,
        RenderTextureBarrier(drawable, RenderTextureLayout::COPY_SOURCE));
    commands->copyTextureRegion(
        RenderTextureCopyLocation::PlacedFootprint(drawableReadback.get(), format, width, height, 1, rowPixels),
        RenderTextureCopyLocation::Subresource(drawable));
    commands->barriers(RenderBarrierStage::COPY,
        RenderTextureBarrier(destination.get(), RenderTextureLayout::COPY_DEST));
    commands->copyTexture(destination.get(), drawable);
    commands->barriers(RenderBarrierStage::COPY,
        RenderTextureBarrier(destination.get(), RenderTextureLayout::COPY_SOURCE));
    commands->copyTextureRegion(
        RenderTextureCopyLocation::PlacedFootprint(textureReadback.get(), format, width, height, 1, rowPixels),
        RenderTextureCopyLocation::Subresource(destination.get()));
    commands->end();
    const RenderCommandList* lists[] = {commands.get()};
    RenderCommandSemaphore* waits[] = {acquired.get()};
    queue->executeCommandLists(lists, 1, waits, 1, nullptr, 0, fence.get());
    queue->waitForCommandFence(fence.get());
    const bool toDrawable = CheckPixels(drawableReadback.get(), width, height, rowPixels, "texture -> drawable");
    const bool fromDrawable = CheckPixels(textureReadback.get(), width, height, rowPixels, "drawable -> texture");
    return toDrawable && fromDrawable ? 0 : 1;
}

int main()
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        // The window supplies Cocoa drawable dimensions but is never ordered on
        // screen or made key. No game data, shaders or MetalFX support are needed.
        NSWindow* window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 32, 16)
            styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
        [window setReleasedWhenClosed:NO];
        CAMetalLayer* layer = [CAMetalLayer layer];
        window.contentView.wantsLayer = YES;
        window.contentView.layer = layer;
        const int result = TestCopies(window, layer);
        [window close];
        return result;
    }
}
