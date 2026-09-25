#pragma once
#include <cstdint>
#include <vector>
namespace settings
{
// Game tab actions follow the seven adjustable retail settings.
inline constexpr int GameRestoreRow = 7;
inline constexpr int GameMainMenuRow = 8;
// Logical ids for the graphics tab. MenuSnapshot::row stores these as int.
// Count is the tab length, not the on-screen viewport.
enum class GraphicsRow : int
{
    Backend = 0,
    DisplayMode = 1,
    Widescreen = 2,
    OutputResolution = 3,
    AntiAliasing = 4,
    DlssQuality = 5,
    FsrSharpness = 6,
    ScalingQuality = 7,
    FrameRate = 8,
    Brightness = 9,
    Save = 10,
    Count = 11,
};
// Called by input polling before returning the guest-facing controller state.
bool FilterInput(uint16_t &buttons, int16_t leftX, int16_t leftY);
// Snapshot rendered on the presentation thread, never accessing guest memory.
bool DrawMenu(std::vector<uint32_t> &pixels, uint64_t &revision, uint32_t width = 1280, uint32_t height = 720);
void PointerClick(float x, float y, bool reverse);
bool IsOpen();
} // namespace settings
