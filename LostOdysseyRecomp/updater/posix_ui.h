#pragma once

#include <string>
#include <string_view>
#include <cstdint>

namespace updater
{
// SDL-based update confirmation for Linux.
// Displays version, changelog, and asks user to Install (Enter / Button A) or Later (Esc / Button B).
// Safe with SDL_VIDEODRIVER=dummy (will not crash or hang).
bool ConfirmUpdateSdl(std::string_view version, std::string_view changelog, uint32_t uiLanguage);

// SDL-based notice for external update (e.g. Flatpak).
// Informs user an update is available via flatpak update.
void ShowExternalUpdateNoticeSdl(std::string_view version, uint32_t uiLanguage);
} // namespace updater
