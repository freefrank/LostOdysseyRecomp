#pragma once

#include <cstdint>
#include <string_view>

union SDL_Event;
namespace host_ui { struct Rasterizer; }

namespace updater::game_prompt
{
void ShowChecking(uint32_t uiLanguage);
void HideChecking();
bool Confirm(std::string_view version, std::string_view changelog, uint32_t uiLanguage);
bool ConfirmBeforeImport(std::string_view version, std::string_view changelog, uint32_t uiLanguage);
bool Visible();
bool HandleEvent(const SDL_Event &event, uint32_t windowId, int windowWidth, int windowHeight);
void Render(host_ui::Rasterizer &rasterizer);
}
