#pragma once

#include <modding/asset_export.h>
#include <array>
#include <filesystem>
#include <functional>
#include <string>

namespace steam_artwork
{
using Image = modding::asset_export::TextureImage;
using TextureReader = std::function<Image(const std::filesystem::path &, std::string_view, std::string_view)>;
// Order matches Steam's portrait, header, hero, transparent logo, icon slots.
struct Artwork
{
    std::array<std::vector<uint8_t>, 5> pngs;
    std::string source;
};

// No extracted game image is cached beside the program or added to packages.
// Acquisition/decoding failures use the bundled original-design set as a unit.
Artwork Generate(const std::filesystem::path &gameRoot, const std::filesystem::path &fallbackDirectory,
                 const TextureReader &readTexture = {});

// CPU composition also exposed to the focused fixtures and local art previews.
// The resampler filters premultiplied colour, then returns straight RGBA.
Image Resize(const Image &source, uint32_t width, uint32_t height);
std::array<Image, 4> Compose(const Image &background, const Image &wordmark);
}
