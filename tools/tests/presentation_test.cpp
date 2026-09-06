#include <gpu/presentation.h>
#include <plume_render_interface.h>
#include <stdafx.h>
namespace plume
{
std::unique_ptr<RenderInterface> CreateD3D12Interface();
}
int main()
{
    using namespace plume;
    auto api = CreateD3D12Interface();
    auto device = api->createDevice();
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    auto commands = queue->createCommandList();
    auto fence = device->createCommandFence();
    gpu::Presentation presentation;
    if (!presentation.Init(device.get()))
        return 1;
    auto source = device->createTexture(RenderTextureDesc::Texture2D(32, 16, 1, RenderFormat::R8G8B8A8_UNORM));
    auto upload = device->createBuffer(RenderBufferDesc::UploadBuffer(256 * 16));
    auto *data = static_cast<uint32_t *>(upload->map());
    for (unsigned y = 0; y < 16; y++)
        for (unsigned x = 0; x < 32; x++)
            data[y * 64 + x] = x > y + 8 ? 0xffffffff : 0xff000000;
    upload->unmap();
    auto submit = [&] {
        commands->end();
        const RenderCommandList *lists[] = {commands.get()};
        queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
        queue->waitForCommandFence(fence.get());
    };
    commands->begin();
    commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(source.get(), RenderTextureLayout::COPY_DEST));
    commands->copyTextureRegion(
        RenderTextureCopyLocation::Subresource(source.get()),
        RenderTextureCopyLocation::PlacedFootprint(upload.get(), RenderFormat::R8G8B8A8_UNORM, 32, 16, 1, 64));
    submit();
    auto render = [&](unsigned w, unsigned h, bool aa) {
        auto target = device->createTexture(
            RenderTextureDesc::Texture2D(w, h, 1, RenderFormat::R8G8B8A8_UNORM, RenderTextureFlag::RENDER_TARGET));
        auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(256 * h));
        commands->begin();
        presentation.Draw(commands.get(), source.get(), target.get(), 32, 16, w, h, aa);
        commands->barriers(RenderBarrierStage::COPY,
                           RenderTextureBarrier(target.get(), RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(
            RenderTextureCopyLocation::PlacedFootprint(readback.get(), RenderFormat::R8G8B8A8_UNORM, w, h, 1, 64),
            RenderTextureCopyLocation::Subresource(target.get()));
        submit();
        auto *mapped = static_cast<uint32_t *>(readback->map());
        std::vector<uint32_t> result(w * h);
        for (unsigned y = 0; y < h; y++)
            memcpy(result.data() + w * y, mapped + 64 * y, w * 4);
        readback->unmap();
        return result;
    };
    bool pass = true;
    const auto identity = render(32, 16, false);
    for (unsigned y = 0; y < 16; y++)
        for (unsigned x = 0; x < 32; x++)
            pass &= identity[y * 32 + x] == (x > y + 8 ? 0xffffffffu : 0xff000000u);
    printf("Native-resolution identity: %s\n", pass ? "PASS" : "FAIL");
    const auto scaled = render(64, 64, false);
    bool bars = true;
    for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 64; x++)
            if (y < 16 || y >= 48)
                bars &= scaled[y * 64 + x] == 0xff000000;
    pass &= bars;
    printf("Aspect ratio / letterboxing: %s\n", bars ? "PASS" : "FAIL");
    const auto filtered = render(32, 16, true);
    unsigned changed = 0, intermediate = 0;
    for (size_t i = 0; i < filtered.size(); i++)
    {
        changed += filtered[i] != identity[i];
        unsigned c = filtered[i] & 255;
        intermediate += c > 0 && c < 255;
    }
    pass &=
        changed > 0 && intermediate > 0 && filtered.front() == identity.front() && filtered.back() == identity.back();
    printf("FXAA diagonal: %u changed pixels, %u intermediate pixels; flat regions preserved\n", changed, intermediate);
    return pass ? 0 : 1;
}
