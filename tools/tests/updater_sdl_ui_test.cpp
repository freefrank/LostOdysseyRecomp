#define SDL_MAIN_HANDLED
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef _WIN32
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "updater/update.h"
#include "updater/posix_ui.h"
#include "updater/external_update_notice.h"
#include "install/installer_colors.h"
#include "install/installer_font.h"

#include <cassert>
#include <iostream>
#include <string>
#include <cstdlib>

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    SDL_SetMainReady();
    // Test 1: SDL_VIDEODRIVER=dummy ConfirmUpdateSdl should return false and not crash or hang
#ifdef _WIN32
    _putenv("SDL_VIDEODRIVER=dummy");
#else
    setenv("SDL_VIDEODRIVER", "dummy", 1);
#endif
    bool confirmed = updater::ConfirmUpdateSdl("v0.5.14", "Test changelog line 1\nTest changelog line 2", 0);
    assert(!confirmed);

    // Test 2: SDL_VIDEODRIVER=dummy ShowExternalUpdateNoticeSdl should safely return and not crash or hang
    for (uint32_t language = 0; language < 5; ++language)
    {
        updater::ShowExternalUpdateNoticeSdl("v0.5.14", language);
        const auto& text = updater::FlatpakUpdateNoticeText(language);
        // Rendered instructions and the URL must fit in full, including CJK.
        for (const auto line : {text.download, text.install, text.systemScope,
                                updater::FlatpakReleaseUrl, updater::FlatpakBundleInstallCommand})
        {
            if (install::ui::MeasureTextWidth(line, 0.9f) > 700 - 48)
            {
                std::cerr << "FAIL: clipped Flatpak guidance for UI language " << language << '\n';
                return 1;
            }
        }
        if (install::ui::MeasureTextWidth(text.close) > 120)
        {
            std::cerr << "FAIL: clipped Flatpak confirmation label\n";
            return 1;
        }
    }

    // Test 3: StatusName for ExternalUpdateAvailable
    assert(std::string(updater::StatusName(updater::StartupStatus::ExternalUpdateAvailable)) == "external-update-available");

    // Bitmap glyphs need tracking beyond their packed cell width, including at fractional scales.
    const int asciiWidth = install::ui::MeasureTextWidth("AB", 1.0f);
    const int scaledWidth = install::ui::MeasureTextWidth("AB", 0.85f);
    const int mixedWidth = install::ui::MeasureTextWidth("中A", 1.0f);
    if (asciiWidth <= 16 || scaledWidth < 16 || mixedWidth <= 24)
    {
        std::cerr << "FAIL: cramped installer glyph advances: ascii=" << asciiWidth
                  << " scaled=" << scaledWidth << " mixed=" << mixedWidth << '\n';
        return 1;
    }

    std::cout << "PASS: updater_sdl_ui_test\n";
    return 0;
}
