#define SDL_MAIN_HANDLED
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef _WIN32
#include <x86intrin.h>
#endif
#include <SDL.h>
#include "updater/update.h"
#include "updater/posix_ui.h"
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
    updater::ShowExternalUpdateNoticeSdl("v0.5.14", 0);

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
