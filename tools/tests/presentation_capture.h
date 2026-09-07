#pragma once
#include <fstream>

// Optional real-frame replay. All variants consume the exact same packed RGBA8
// input, avoiding camera/animation differences between separate game launches.
inline int ReplayPresentationCapture(plume::RenderDevice* device, const char* input,
    unsigned width, unsigned height, const char* output)
{
    using namespace plume;
    if (!width || !height || width > 7680 || height > 4320 ||
        uint64_t(width) * height > 7680ull * 4320) return 2;
    std::ifstream file(input, std::ios::binary | std::ios::ate);
    const auto bytes = uint64_t(width) * height * 4;
    if (!file || file.tellg() != std::streamoff(bytes)) return 2;
    std::vector<uint32_t> pixels(width * height);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(pixels.data()), bytes)) return 2;
    const std::filesystem::path directory(output);
    if (std::filesystem::exists(directory)) return 2; // Preserve previous evidence.
    std::filesystem::create_directories(directory);
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    auto commands = queue->createCommandList();
    auto fence = device->createCommandFence();
    auto submit = [&] {
        commands->end();
        const RenderCommandList* lists[]{commands.get()};
        queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
        queue->waitForCommandFence(fence.get());
    };
    gpu::Presentation presentation;
    if (!presentation.Init(device)) return 1;
    auto source = device->createTexture(RenderTextureDesc::Texture2D(width, height, 1, RenderFormat::R8G8B8A8_UNORM));
    const unsigned pitch = (width + 63) & ~63u;
    auto upload = device->createBuffer(RenderBufferDesc::UploadBuffer(uint64_t(pitch) * height * 4));
    auto mapped = static_cast<uint32_t*>(upload->map());
    for (unsigned y = 0; y < height; ++y) memcpy(mapped + y*pitch, pixels.data() + y*width, width*4);
    upload->unmap();
    commands->begin();
    commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(source.get(), RenderTextureLayout::COPY_DEST));
    commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(source.get()),
        RenderTextureCopyLocation::PlacedFootprint(upload.get(), RenderFormat::R8G8B8A8_UNORM, width, height, 1, pitch));
    submit();
    for (unsigned aa = 0; aa < 3; ++aa)
    for (unsigned quality = 0; quality < 2; ++quality)
    {
        constexpr unsigned outWidth = 1920, outHeight = 1080, outPitch = 1920;
        auto target = device->createTexture(RenderTextureDesc::Texture2D(outWidth, outHeight, 1, RenderFormat::R8G8B8A8_UNORM, RenderTextureFlag::RENDER_TARGET));
        auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(uint64_t(outPitch)*outHeight*4));
        commands->begin();
        presentation.Draw(commands.get(), source.get(), target.get(), width, height, outWidth, outHeight,
            gpu::PresentationOptions{static_cast<gpu::Antialiasing>(aa), static_cast<gpu::ScalingFilter>(quality)});
        commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(target.get(), RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(), RenderFormat::R8G8B8A8_UNORM, outWidth, outHeight, 1, outPitch),
            RenderTextureCopyLocation::Subresource(target.get()));
        submit();
        auto result = static_cast<const uint32_t*>(readback->map());
        std::ofstream image(directory / ("aa"+std::to_string(aa)+"-quality"+std::to_string(quality)+".ppm"), std::ios::binary);
        image << "P6\n" << outWidth << ' ' << outHeight << "\n255\n";
        for (unsigned y = 0; y < outHeight; ++y)
        for (unsigned x = 0; x < outWidth; ++x)
        {
            const uint32_t pixel = result[y*outPitch+x];
            const char rgb[]{char(pixel), char(pixel>>8), char(pixel>>16)};
            image.write(rgb, 3);
        }
        image.close();
        readback->unmap();
        if (!image) return 1;
    }
    printf("Real-frame replay: six 1920x1080 variants written; visual comparison required\n");
    return 0;
}
