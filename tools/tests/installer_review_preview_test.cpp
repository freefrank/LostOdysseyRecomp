#include <install/installer_ui.h>
#include <settings/config.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

namespace {
uint32_t language = 0;
}

settings::Config settings::GetConfig()
{
    Config config;
    config.uiLanguage = language;
    return config;
}

std::filesystem::path install::DefaultGameDirectory(const std::filesystem::path& executableDirectory)
{
    return executableDirectory / "unused-fixture-root";
}

install::ContentScan install::ScanContent(const std::vector<std::filesystem::path>&, const Cancelled&)
{
    throw std::runtime_error("review preview must not scan files");
}

install::InstallResult install::ReimportContent(const ContentScan&, const std::filesystem::path&,
                                               const Progress&, const Cancelled&, const Commit&)
{
    throw std::runtime_error("review preview must not import content");
}

bool install::WriteGamePath(const std::filesystem::path&, const std::filesystem::path&, std::string&)
{
    throw std::runtime_error("review preview must not write game-path.txt");
}

int main()
{
    SDL_setenv_unsafe("SDL_VIDEODRIVER", "dummy", 1);
    const auto output = std::filesystem::current_path() / "out" / "import-menu-preview";
    std::filesystem::create_directories(output);
    // A four-disc rejection like #251: longer than one 1280 px line, so the
    // review page has to wrap it.
    static const char* const kScanError =
        "No supported Lost Odyssey discs or DLC found. Details: Lost Odyssey (Disc 1).iso (4294967295 bytes): "
        "the image is incomplete: its 'xenon_snd.fpd' ends 1330356225 bytes past the end of the file (FAT32 storage cuts copies at 4 GB; "
        "an interrupted copy also leaves a short file); Lost Odyssey (Disc 2).iso (4294967295 bytes): the image is incomplete: its "
        "'xenon_field.fpd' ends 902344704 bytes past the end of the file (FAT32 storage cuts copies at 4 GB; an interrupted copy also leaves a short file)";
    for (auto pair : {std::pair{0u, "review-en.bmp"}, std::pair{1u, "review-tw.bmp"},
                      std::pair{2u, "review-ja.bmp"}, std::pair{3u, "review-ko.bmp"},
                      std::pair{4u, "review-zh.bmp"}, std::pair{0u, "review-scrolled.bmp"},
                      std::pair{0u, "review-error.bmp"}})
    {
        std::fprintf(stderr, "render review language %u\n", pair.first);
        language = pair.first;
        const auto bmp = output / pair.second;
        SDL_setenv_unsafe("LO_IMPORTER_PREVIEW_BMP", bmp.string().c_str(), 1);
        SDL_setenv_unsafe("LO_IMPORTER_PREVIEW_SCROLL", pair.second == std::string_view("review-scrolled.bmp") ? "1" : "", 1);
        SDL_setenv_unsafe("LO_IMPORTER_PREVIEW_ERROR", pair.second == std::string_view("review-error.bmp") ? kScanError : "", 1);
        const auto result = install::ShowInstallerUI(output, output, output / "unused-fixture-root");
        if (!result.cancelled || result.success || !std::filesystem::is_regular_file(bmp) ||
            std::filesystem::file_size(bmp) < 1280 * 720 * 3)
            return 1;
    }
    SDL_setenv_unsafe("LO_IMPORTER_PREVIEW_BMP", "", 1);
    return 0;
}
