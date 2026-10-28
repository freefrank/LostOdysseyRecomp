#include "steam_artwork.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace steam_artwork
{
namespace
{
namespace fs = std::filesystem;
constexpr std::array<const char *, 5> Names{"portrait.png", "header.png", "hero.png", "logo.png", "icon.png"};
void Validate(const Image &image)
{
    if (!image.width || !image.height || image.width > 8192 || image.height > 8192 ||
        uint64_t(image.width) * image.height * 4 != image.rgba.size())
        throw std::runtime_error("invalid Steam artwork source image");
}
uint8_t Byte(double value) { return uint8_t(std::clamp(std::lround(value), 0l, 255l)); }
// Catmull-Rom bicubic reconstruction. Scaled support provides antialiasing
// when reducing a texture, and the separable pass keeps large hero art cheap.
double Cubic(double x)
{
    x = std::abs(x);
    if (x < 1) return (1.5 * x - 2.5) * x * x + 1;
    return x < 2 ? ((-0.5 * x + 2.5) * x - 4) * x + 2 : 0;
}
using Pixel = std::array<float, 4>;
struct Tap { uint32_t index; float weight; };
std::vector<std::vector<Tap>> Weights(uint32_t source, uint32_t destination)
{
    std::vector<std::vector<Tap>> result(destination);
    const double scale = std::max(1., double(source) / destination);
    for (uint32_t x = 0; x < destination; ++x)
    {
        const double center = (x + .5) * source / destination - .5;
        double total = 0;
        for (int i = int(std::ceil(center - 2 * scale)); i <= int(std::floor(center + 2 * scale)); ++i)
        {
            const auto weight = Cubic((i - center) / scale);
            if (weight == 0) continue;
            result[x].push_back({uint32_t(std::clamp(i, 0, int(source) - 1)), float(weight)});
            total += weight;
        }
        for (auto &tap : result[x]) tap.weight = float(tap.weight / total);
    }
    return result;
}
Image Crop(const Image &image, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    Validate(image);
    if (!width || !height || x >= image.width || y >= image.height || width > image.width - x || height > image.height - y)
        throw std::runtime_error("invalid Steam artwork crop");
    Image result{width, height, std::vector<uint8_t>(size_t(width) * height * 4)};
    for (uint32_t row = 0; row < height; ++row)
        std::copy_n(image.rgba.begin() + (size_t(y + row) * image.width + x) * 4, size_t(width) * 4,
                    result.rgba.begin() + size_t(row) * width * 4);
    return result;
}
Image Trim(const Image &image)
{
    Validate(image);
    uint32_t left = image.width, top = image.height, right = 0, bottom = 0;
    for (uint32_t y = 0; y < image.height; ++y)
        for (uint32_t x = 0; x < image.width; ++x)
            if (image.rgba[(size_t(y) * image.width + x) * 4 + 3] > 8)
            { left = std::min(left, x); top = std::min(top, y); right = std::max(right, x + 1); bottom = std::max(bottom, y + 1); }
    if (left >= right || top >= bottom) throw std::runtime_error("the game logo has no visible pixels");
    return Crop(image, left, top, right - left, bottom - top);
}
Image Fit(const Image &image, uint32_t width, uint32_t height)
{
    const double scale = std::min(double(width) / image.width, double(height) / image.height);
    return Resize(image, std::max(1u, uint32_t(std::lround(image.width * scale))),
                  std::max(1u, uint32_t(std::lround(image.height * scale))));
}
Image Cover(const Image &image, uint32_t width, uint32_t height)
{
    const double ratio = double(width) / height;
    uint32_t cropWidth = image.width, cropHeight = image.height;
    if (double(image.width) / image.height > ratio) cropWidth = std::max(1u, uint32_t(image.height * ratio));
    else cropHeight = std::max(1u, uint32_t(image.width / ratio));
    return Resize(Crop(image, (image.width - cropWidth) / 2, (image.height - cropHeight) / 2, cropWidth, cropHeight), width, height);
}
void Overlay(Image &destination, const Image &source, uint32_t x, uint32_t y)
{
    if (x + source.width > destination.width || y + source.height > destination.height)
        throw std::runtime_error("Steam artwork overlay exceeds the canvas");
    for (uint32_t row = 0; row < source.height; ++row)
        for (uint32_t column = 0; column < source.width; ++column)
        {
            const auto *s = source.rgba.data() + (size_t(row) * source.width + column) * 4;
            auto *d = destination.rgba.data() + (size_t(y + row) * destination.width + x + column) * 4;
            const double alpha = s[3] / 255.;
            for (size_t c = 0; c < 3; ++c) d[c] = Byte(s[c] * alpha + d[c] * (1 - alpha));
            d[3] = 255;
        }
}
void Shade(Image &image, double amount)
{
    // A simple dark gradient supports the original title wordmark without
    // repainting the player's source artwork.
    for (uint32_t y = 0; y < image.height; ++y)
        for (uint32_t x = 0; x < image.width; ++x)
        {
            auto *p = image.rgba.data() + (size_t(y) * image.width + x) * 4;
            const double fade = 1 - amount * std::pow(double(y) / image.height, 2);
            for (size_t c = 0; c < 3; ++c) p[c] = Byte(p[c] * p[3] / 255. * fade);
            p[3] = 255;
        }
}
std::vector<uint8_t> Read(const fs::path &file)
{
    std::ifstream input(file, std::ios::binary | std::ios::ate);
    const auto length = input.tellg();
    if (!input || length <= 0 || uint64_t(length) > 64u * 1024 * 1024)
        throw std::runtime_error("cannot read bundled Steam artwork: " + file.filename().string());
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size())))
        throw std::runtime_error("short read of bundled Steam artwork: " + file.filename().string());
    return bytes;
}
struct Lookup { const char *package, *object; uint32_t x = 0, y = 0, width = 0, height = 0; };
// Stable original package + Texture2D identities, verified against both the
// Asian multilingual and USA/Europe Disc 1. No broad asset-name guessing.
constexpr Lookup Backgrounds[] = {
    // Dream illustrations are atlases: these verified rectangles exclude
    // neighbouring panels and contain only scenery, with no lettering.
    {"bin/xenon/event/1000/dr_17d.xxx", "17_11", 0, 512, 1024, 512},
    {"bin/xenon/event/1000/dr_07d.xxx", "07_12", 0, 0, 1024, 512},
    {"bin/xenon/event/1000/dr_24d.xxx", "24_12"},
};
constexpr Lookup Logos[] = {
    // The 32px band is the wordmark with its original shadow. Lower rows hold
    // a blurred reflection, trademark and grey fade used by title-screen UI.
    {"bin/xenon/sys/img/titleparts.xxx", "LO_TITLE", 0, 0, 512, 32},
};
template <size_t N>
Image Acquire(const fs::path &root, const Lookup (&lookups)[N], const TextureReader &read,
              std::string &identity)
{
    std::string reason;
    for (const auto &lookup : lookups)
    {
        try
        {
            auto image = read(root, lookup.package, lookup.object);
            Validate(image);
            identity = std::string(lookup.package) + ":" + lookup.object;
            if (lookup.width)
            {
                image = Crop(image, lookup.x, lookup.y, lookup.width, lookup.height);
                identity += " crop=" + std::to_string(lookup.x) + "," + std::to_string(lookup.y) + "," +
                            std::to_string(lookup.width) + "," + std::to_string(lookup.height);
            }
            return image;
        }
        catch (const std::exception &failure)
        {
            if (!reason.empty()) reason += "; ";
            reason += std::string(lookup.package) + ":" + lookup.object + " (" + failure.what() + ")";
        }
    }
    throw std::runtime_error(reason);
}
}

Image Resize(const Image &source, uint32_t width, uint32_t height)
{
    Validate(source);
    Image result{width, height, {}};
    if (!width || !height || width > 8192 || height > 8192 || uint64_t(width) * height > 16u * 1024 * 1024)
        throw std::runtime_error("invalid Steam artwork output size");
    const auto horizontal = Weights(source.width, width), vertical = Weights(source.height, height);
    std::vector<Pixel> temporary(size_t(width) * source.height);
    for (uint32_t y = 0; y < source.height; ++y)
        for (uint32_t x = 0; x < width; ++x)
            for (const auto &tap : horizontal[x])
            {
                const auto *p = source.rgba.data() + (size_t(y) * source.width + tap.index) * 4;
                auto &pixel = temporary[size_t(y) * width + x];
                const float alpha = p[3] / 255.f;
                for (size_t c = 0; c < 3; ++c) pixel[c] += p[c] * alpha * tap.weight;
                pixel[3] += p[3] * tap.weight;
            }
    result.rgba.resize(size_t(width) * height * 4);
    for (uint32_t y = 0; y < height; ++y)
        for (uint32_t x = 0; x < width; ++x)
        {
            Pixel pixel{};
            for (const auto &tap : vertical[y])
                for (size_t c = 0; c < 4; ++c) pixel[c] += temporary[size_t(tap.index) * width + x][c] * tap.weight;
            auto *p = result.rgba.data() + (size_t(y) * width + x) * 4;
            const double alpha = std::clamp(double(pixel[3]), 0., 255.);
            p[3] = Byte(alpha);
            for (size_t c = 0; c < 3; ++c) p[c] = p[3] ? Byte(pixel[c] * 255. / alpha) : 0;
        }
    return result;
}

std::array<Image, 4> Compose(const Image &background, const Image &wordmark)
{
    Validate(background);
    const auto trimmed = Trim(wordmark);
    std::array<Image, 4> result{Cover(background, 600, 900), Cover(background, 920, 430), Cover(background, 3840, 1240), Fit(trimmed, 1280, 720)};
    Shade(result[0], .7); Shade(result[1], .55); Shade(result[2], 0);
    const auto portraitLogo = Fit(trimmed, 510, 260), headerLogo = Fit(trimmed, 620, 250);
    Overlay(result[0], portraitLogo, (600 - portraitLogo.width) / 2, 900 - 100 - portraitLogo.height);
    Overlay(result[1], headerLogo, (920 - headerLogo.width) / 2, (430 - headerLogo.height) / 2);
    return result;
}

Artwork Generate(const fs::path &gameRoot, const fs::path &fallbackDirectory, const TextureReader &readTexture)
{
    Artwork result;
    Image background, logo;
    std::string backgroundSource, logoSource;
    try
    {
        if (gameRoot.empty()) throw std::runtime_error("no game data selected");
        const TextureReader reader = readTexture ? readTexture : modding::asset_export::ReadTexture;
        background = Acquire(gameRoot, Backgrounds, reader, backgroundSource);
        logo = Trim(Acquire(gameRoot, Logos, reader, logoSource));
    }
    catch (const std::exception &error)
    {
        for (size_t i = 0; i < result.pngs.size(); ++i) result.pngs[i] = Read(fallbackDirectory / Names[i]);
        result.source = "Steam artwork: bundled original-design fallback; extraction unavailable: " + std::string(error.what());
        return result;
    }
    // Internal composition/encoding errors must reach AddToSteam's error result,
    // rather than being misreported as unavailable original game artwork.
    const auto images = Compose(background, logo);
    for (size_t i = 0; i < images.size(); ++i) result.pngs[i] = modding::asset_export::EncodeTexturePng(images[i]);
    result.pngs[4] = Read(fallbackDirectory / Names[4]); // Original project icon remains host-visible in grid/.
    result.source = "Steam artwork: player's game files; background=" + backgroundSource + "; logo=" + logoSource;
    return result;
}
}
