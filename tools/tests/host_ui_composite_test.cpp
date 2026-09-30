#include <host_ui/rasterizer.h>

#include <array>
#include <cstdio>
#include <cstdlib>

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}
}

int main()
{
    Require(host_ui::PackRgba(0x11, 0x22, 0x33, 0x44) == 0x44332211u,
            "RGBA packing does not match R8G8B8A8_UNORM memory order");
    Require(host_ui::MakeColor(0x44, 0x11, 0x22, 0x33) == 0x44332211u,
            "alpha-first color helper does not match RGBA packing");
    Require(host_ui::ColorBlend(host_ui::PackRgba(0, 0, 0, 255),
                                host_ui::PackRgba(200, 100, 50, 128)) ==
                host_ui::PackRgba(100, 50, 25, 255),
            "RGBA channel blending is incorrect");

    // Verify straight-alpha source-over consistency across single and intermediate transparent layers (R10)
    const uint32_t opaqueBlack = host_ui::PackRgba(0, 0, 0, 255);
    const uint32_t halfWhite = host_ui::PackRgba(255, 255, 255, 128);
    const uint32_t transparentBlack = host_ui::PackRgba(0, 0, 0, 0);

    const uint32_t directBlend = host_ui::ColorBlend(opaqueBlack, halfWhite);
    const uint32_t intermediate = host_ui::ColorBlend(transparentBlack, halfWhite);
    const uint32_t twoPassBlend = host_ui::ColorBlend(opaqueBlack, intermediate);

    Require(directBlend == host_ui::PackRgba(128, 128, 128, 255),
            "Direct 50% white blend onto black produced unexpected color");
    Require(twoPassBlend == directBlend,
            "Intermediate transparent layer caused double-darkening in straight-alpha blending");

    host_ui::PixelBuffer overlay;
    Require(overlay.Resize(), "logical overlay allocation failed");
    overlay.Clear(host_ui::MakeColor(128, 220, 40, 20));

    constexpr std::array sizes{
        std::pair{800u, 600u},
        std::pair{1280u, 720u},
        std::pair{1920u, 1080u},
        std::pair{2560u, 1440u},
        std::pair{3840u, 2160u},
        std::pair{900u, 1600u},
    };
    for (const auto [width, height] : sizes)
    {
        const uint32_t background = host_ui::MakeColor(255, 10, 20, 30);
        std::vector<uint32_t> destination(size_t(width) * height, background);
        Require(host_ui::CompositeScaled(overlay, width, height, destination), "scaled composite failed");
        Require(destination.size() == size_t(width) * height, "composite changed output dimensions");
        Require(destination[size_t(height / 2) * width + width / 2] != background,
                "scaled overlay did not cover output center");
        Require((destination[size_t(height / 2) * width + width / 2] >> 24) == 255,
                "composite output was not opaque");
        if (uint64_t(width) * host_ui::kOverlayHeight != uint64_t(height) * host_ui::kOverlayWidth)
            Require(destination.front() == background && destination.back() == background,
                    "scaled overlay overwrote letterbox pixels");
    }

    std::vector<uint32_t> wrongSize(1);
    Require(!host_ui::CompositeScaled(overlay, 1280, 720, wrongSize),
            "composite accepted mismatched output dimensions");
    Require(!overlay.Resize(0, 720) && overlay.pixels.empty() && !overlay.width && !overlay.height,
            "pixel buffer accepted an invalid size");
    std::puts("host UI scaled composite dimensions, letterboxing, and bounds passed");
    return 0;
}
