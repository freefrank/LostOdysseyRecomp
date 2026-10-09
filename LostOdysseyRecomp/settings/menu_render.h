#pragma once
#include "menu.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace settings
{
namespace menu_assets { struct Assets; }
// Visible list slots: rows start at y=150 with height 43 and clip at y=640,
// so (640 - 150) / 43 == 11 rows fit without scrolling.
inline constexpr int kMenuVisibleRows = 11;
struct MenuRow
{
    std::wstring name, value;
    bool enabled = true;
    // Ordered exactly like the left/right input cycle. The renderer uses the
    // list to recreate the original game's horizontal option cells.
    std::vector<std::wstring> choices;
    int selectedChoice = 0;
    // Non-negative values render the original Min/Max volume slider instead
    // of option cells.
    int sliderPercent = -1;
    // Confirmation-button choices use the original colored A/B key legend.
    bool controllerButtons = false;
    // Hidden rows keep their logical index (input dispatch stays stable) but
    // are skipped by navigation, drawing and mouse hit-testing.
    bool hidden = false;
    // Show only the selected choice between arrows, as rows with more than
    // five choices do; for long names such as GPUs and displays.
    bool singleValue = false;
    bool operator==(const MenuRow &) const = default;
};
struct MenuSnapshot
{
    int tab = 0, row = 0;
    // First visible slot when rows overflow the list area. Publish keeps the
    // focused row inside [scroll, scroll + kMenuVisibleRows).
    int scroll = 0;
    uint32_t language = 0;
    bool playStationPrompts = false;
    std::vector<MenuRow> rows;
    std::wstring help;
    // Graphics-tab DLSS status. Empty on other tabs, so those pages keep one help line.
    std::wstring notice;
    std::wstring dialogTitle, dialogMessage;
    std::vector<std::wstring> dialogChoices;
    int dialogSelection = 0;
    uint64_t revision = 0;
    HdrCalibration calibration;
    BrightnessCalibration brightness;
    NeuralRenderingTuning neuralRendering;
    std::shared_ptr<const menu_assets::Assets> assets;
    // Only the panels, without title, tabs, rows or help: what the content
    // fades in over when the menu opens.
    bool backdropOnly = false;
};
// Render glyphs at output resolution, fitting the existing 1280x720 logical layout.
bool RasterizeMenu(const MenuSnapshot &snapshot, uint32_t width, uint32_t height,
                   std::vector<uint32_t> &pixels);

// Presentation-only menu motion (#151): the shown image eases from what was on
// screen to the newest raster. Menu logic and input never wait for it; a newer
// raster restarts the ease from whatever is on screen at that moment.
inline constexpr auto kMenuOpenFade = std::chrono::milliseconds(220);
inline constexpr auto kMenuChangeFade = std::chrono::milliseconds(150);
class MenuTransition
{
  public:
    using Clock = std::chrono::steady_clock;
    // shown: the opaque image on screen now; target: the new raster of the same
    // size, swapped in (its buffer comes back as scratch). Returns false, with
    // target untouched, when the sizes do not match. Only rows that differ are
    // blended afterwards.
    bool Start(const std::vector<uint32_t> &shown, std::vector<uint32_t> &target, uint32_t width,
               Clock::time_point now, Clock::duration duration);
    // Writes the frame for `now` into pixels, which must still hold this
    // transition's previous frame. Returns true while easing; the last frame is
    // exactly the target. A clock that moved backwards settles at once.
    bool Advance(std::vector<uint32_t> &pixels, Clock::time_point now);
    bool Running() const { return running; }
    void Stop() { running = false; }
    void Reset();

  private:
    std::vector<uint32_t> from, to;
    std::vector<std::pair<size_t, size_t>> spans; // pixel ranges of the rows that differ
    Clock::time_point start{};
    Clock::duration duration{};
    bool running = false;
};
}
