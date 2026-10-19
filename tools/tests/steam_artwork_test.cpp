#include <os/steam_artwork.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

// Only disc acquisition is replaced. Composition and the PNG encoder invoked
// by production use the same miniz API and real CPU image data in this fixture.
namespace modding::asset_export
{
bool failPngEncoding = false;
TextureImage ReadTexture(const std::filesystem::path &, std::string_view, std::string_view)
{ throw std::runtime_error("disc not present in fixture"); }
std::vector<uint8_t> EncodeTexturePng(const TextureImage &image)
{
    if (failPngEncoding) throw std::runtime_error("injected PNG encoder failure");
    size_t size = 0;
    void *data = tdefl_write_image_to_png_file_in_memory_ex(image.rgba.data(), int(image.width), int(image.height), 4, &size, 1, MZ_FALSE);
    if (!data) throw std::runtime_error("fixture PNG encoder failed");
    std::unique_ptr<void, decltype(&mz_free)> owner(data, &mz_free);
    const auto *begin = static_cast<const uint8_t *>(data);
    return {begin, begin + size};
}
}
namespace
{
using steam_artwork::Image;
unsigned checks = 0;
void Check(bool value, const char *message)
{
    ++checks;
    if (!value) throw std::runtime_error(message);
}
Image Solid(uint32_t width, uint32_t height, std::array<uint8_t, 4> color)
{
    Image image{width, height, std::vector<uint8_t>(size_t(width) * height * 4)};
    for (size_t i = 0; i < image.rgba.size(); i += 4) std::copy(color.begin(), color.end(), image.rgba.begin() + i);
    return image;
}
uint32_t Big32(const std::vector<uint8_t> &bytes, size_t offset)
{
    Check(bytes.size() >= offset + 4, "PNG header is truncated");
    return uint32_t(bytes[offset]) << 24 | uint32_t(bytes[offset + 1]) << 16 | uint32_t(bytes[offset + 2]) << 8 | bytes[offset + 3];
}
std::pair<uint32_t, uint32_t> PngSize(const std::vector<uint8_t> &bytes)
{
    Check(bytes.size() >= 33 && bytes[0] == 137 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G', "not an encoded PNG");
    return {Big32(bytes, 16), Big32(bytes, 20)};
}
void Resampling()
{
    // Transparent red must not bleed into a blue opaque pixel after resizing.
    Image source{2, 1, {255, 0, 0, 0, 0, 0, 255, 255}};
    const auto resized = steam_artwork::Resize(source, 33, 4);
    for (size_t i = 0; i < resized.rgba.size(); i += 4)
        if (resized.rgba[i + 3]) Check(resized.rgba[i] == 0 && resized.rgba[i + 2] == 255, "resampling leaked transparent RGB or lost straight alpha");
    bool rejected = false;
    try { steam_artwork::Resize(source, 0, 10); } catch (const std::exception &) { rejected = true; }
    Check(rejected, "invalid artwork dimensions accepted");
}
void ArtworkGeneration(const std::filesystem::path &directory)
{
    constexpr std::array<const char *, 5> names{"portrait.png", "header.png", "hero.png", "logo.png", "icon.png"};
    for (const auto *name : names) std::ofstream(directory / name, std::ios::binary) << "bundled-" << name;
    const auto fallback = steam_artwork::Generate({}, directory);
    Check(fallback.source.find("bundled original-design fallback") != std::string::npos &&
          fallback.source.find("no game data selected") != std::string::npos, "missing game data fallback is not identified");
    for (size_t i = 0; i < names.size(); ++i)
        Check(std::string(fallback.pngs[i].begin(), fallback.pngs[i].end()) == std::string("bundled-") + names[i], "fallback changed a bundled file");

    const auto background = Solid(2048, 1024, {25, 65, 105, 255});
    auto atlas = Solid(512, 128, {0, 0, 0, 0});
    for (uint32_t y = 10; y < 32; ++y)
        for (uint32_t x = 20; x < 492; ++x)
        {
            const auto p = (size_t(y) * atlas.width + x) * 4;
            atlas.rgba[p] = atlas.rgba[p + 1] = atlas.rgba[p + 2] = 255;
            atlas.rgba[p + 3] = (x == 20 || x == 491 || y == 10 || y == 31) ? 96 : 255;
        }
    // The unrelated bottom atlas band must be omitted from the generated logo.
    std::fill(atlas.rgba.begin() + size_t(40) * 512 * 4, atlas.rgba.begin() + size_t(48) * 512 * 4, 128);
    std::fill(atlas.rgba.begin() + size_t(80) * 512 * 4, atlas.rgba.end(), 255);
    unsigned reads = 0;
    const auto original = steam_artwork::Generate("fixture-disc", directory,
        [&](const std::filesystem::path &root, std::string_view package, std::string_view object) -> Image {
            Check(root == "fixture-disc", "wrong game root passed to extraction"); ++reads;
            if (package == "bin/xenon/sys/img/titleparts.xxx" && object == "LO_TITLE") return atlas;
            if (package == "bin/xenon/event/1000/dr_17d.xxx" && object == "17_11") return background;
            throw std::runtime_error("unexpected texture identity");
        });
    Check(reads == 2 && original.source.find("player's game files") != std::string::npos && original.source.find("LO_TITLE") != std::string::npos,
          "original texture identities or source log missing");
    Check(PngSize(original.pngs[0]) == std::pair(600u, 900u), "portrait size differs");
    Check(PngSize(original.pngs[1]) == std::pair(920u, 430u), "header size differs");
    Check(PngSize(original.pngs[2]) == std::pair(3840u, 1240u), "hero size differs");
    const auto logoSize = PngSize(original.pngs[3]);
    Check(logoSize.first <= 1280 && logoSize.second <= 720 && logoSize.second < 80, "logo atlas crop or maximum size differs");
    Check(original.pngs[4] == fallback.pngs[4], "project icon should be retained");

    reads = 0;
    const auto alternate = steam_artwork::Generate("fixture-disc", directory,
        [&](const auto &, std::string_view package, std::string_view object) -> Image {
            ++reads;
            if (package == "bin/xenon/sys/img/titleparts.xxx" && object == "LO_TITLE") return atlas;
            if (package == "bin/xenon/event/1000/dr_07d.xxx" && object == "07_12") return background;
            throw std::runtime_error("primary illustration absent");
        });
    Check(reads == 3 && alternate.source.find("dr_07d.xxx:07_12") != std::string::npos &&
          alternate.source.find("player's game files") != std::string::npos, "ordered original illustration fallback failed");

    const auto failure = steam_artwork::Generate("unreadable-disc", directory,
        [](const auto &, auto, auto) -> Image { throw std::runtime_error("fixture unreadable package"); });
    Check(failure.pngs == fallback.pngs && failure.source.find("fixture unreadable package") != std::string::npos,
          "unreadable original package did not select the intact fallback set");
    const auto invisibleLogo = steam_artwork::Generate("fixture-disc", directory,
        [&](const auto &, auto, std::string_view object) -> Image {
            return object == "LO_TITLE" ? Solid(512, 128, {255, 255, 255, 0}) : background;
        });
    Check(invisibleLogo.pngs == fallback.pngs && invisibleLogo.source.find("no visible pixels") != std::string::npos,
          "invalid source wordmark did not select the fallback set");

    modding::asset_export::failPngEncoding = true;
    bool encodingRejected = false;
    try
    {
        steam_artwork::Generate("fixture-disc", directory,
            [&](const auto &, auto, std::string_view object) -> Image { return object == "LO_TITLE" ? atlas : background; });
    }
    catch (const std::exception &error) { encodingRejected = std::string(error.what()) == "injected PNG encoder failure"; }
    modding::asset_export::failPngEncoding = false;
    Check(encodingRejected, "PNG encoding failure was hidden by a successful fallback");

    std::filesystem::remove(directory / "hero.png");
    bool rejected = false;
    try { steam_artwork::Generate({}, directory); } catch (const std::exception &) { rejected = true; }
    Check(rejected, "missing fallback art silently accepted");
}
}
int main()
{
    const auto directory = std::filesystem::temp_directory_path() / ("lo-steam-artwork-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try
    {
        std::filesystem::create_directories(directory);
        Resampling(); ArtworkGeneration(directory);
        std::filesystem::remove_all(directory);
        std::cout << "Steam artwork: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &failure)
    {
        std::filesystem::remove_all(directory);
        std::cerr << "Steam artwork fixture: " << failure.what() << '\n';
        return 1;
    }
}
