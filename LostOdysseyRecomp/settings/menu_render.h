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
    // Audio tab speaker test (Rear angle row): the 5.1 layout is drawn under
    // the rows with the surrounds at speakerRear and the path a matrix decoder
    // gives the test sound; the marker that follows it is a layer (DrawSpeakerMarker).
    bool speakerLayout = false;
    int speakerRear = 110;
    std::shared_ptr<const menu_assets::Assets> assets;
    // Only the panels, without title, tabs, rows or help: what the content
    // fades in over when the menu opens.
    bool backdropOnly = false;
    // Only help, a key then its label, at the title menu's lower right as
    // straight alpha: the Settings legend (title_entry.cpp). The raster is
    // just TitleHintBounds of the output.
    bool titleHint = false;
};
// The list cursor arrow, top-left corner in the 1280x720 layout.
struct MenuArrow
{
    bool visible = false;
    int x = 0, y = 0;
};
// Render glyphs at output resolution, fitting the existing 1280x720 logical layout.
// With arrow set, the list arrow is left out and reported there, for presentation
// to move it; under a prompt it stays in the raster, dimmed with the page.
bool RasterizeMenu(const MenuSnapshot &snapshot, uint32_t width, uint32_t height,
                   std::vector<uint32_t> &pixels, MenuArrow *arrow = nullptr);
struct MenuRect
{
    size_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
// The list arrow at a layout position, and the output pixels it can cover.
void DrawMenuArrow(std::vector<uint32_t> &pixels, uint32_t width, uint32_t height, int x, int y);
// The output pixels of the title menu's Settings legend: with titleHint,
// RasterizeMenu returns only this rectangle, row by row.
MenuRect TitleHintBounds(uint32_t width, uint32_t height);
MenuRect MenuArrowBounds(uint32_t width, uint32_t height, int x, int y);
// The speaker test marker where a matrix decoder puts the test sound at
// `position` along its circle (apu::SpeakerPan) with the surrounds encoded for
// `rearAngle`, framing the speaker it rests on, and the output pixels it covers.
MenuRect DrawSpeakerMarker(std::vector<uint32_t> &pixels, uint32_t width, uint32_t height, float position, float rearAngle);

// Presentation-only menu motion (#151), timed like the retail Configuration and
// camp menus (recorded frame by frame on psvita, 2026-10-09). Menu logic and
// input never wait for it; a newer raster restarts from whatever is on screen.
namespace menu_motion
{
using namespace std::chrono_literals;
// Cursor, value and help changes: one blended frame at 60 FPS.
inline constexpr std::chrono::milliseconds ChangeFade = 33ms;
// A tab switch, like the camp menu changing pages.
inline constexpr std::chrono::milliseconds PageFade = 170ms;
// A prompt and its dimmed page come in, and go, together.
inline constexpr std::chrono::milliseconds DialogFade = 100ms;
// Closing: the content goes before the game's own exit animation.
inline constexpr std::chrono::milliseconds CloseFade = 100ms;
// Opening: tabs, values and help fade in; the list rows follow top to bottom.
inline constexpr std::chrono::milliseconds OpenPanelFade = 190ms;
inline constexpr std::chrono::milliseconds OpenRowStagger = 27ms;
// The arrow follows its row like UE3 FInterpTo at speed 10. It appears above
// the first row shortly after the open starts, and sways sideways while idle.
inline constexpr double ArrowSpeed = 10.0;
inline constexpr std::chrono::milliseconds ArrowOpenDelay = 83ms;
inline constexpr int ArrowOpenDrop = 62;
inline constexpr int ArrowSway = 5;
inline constexpr std::chrono::milliseconds ArrowSwayPeriod = 870ms;
// A prompt darkens the page to 195/256.
inline constexpr uint32_t DialogDim = 195;
// One list row's opacity t seconds after it starts fading in.
double RowFade(double t);
} // namespace menu_motion

// Output rows of the visible list slots and where the list column ends, for the
// opening cascade.
struct MenuCascade
{
    size_t split = 0;
    std::vector<std::pair<size_t, size_t>> bands;
};
MenuCascade MenuOpenCascade(uint32_t width, uint32_t height);

class MenuTransition
{
  public:
    using Clock = std::chrono::steady_clock;
    // shown: the opaque image on screen now; target: the new raster of the same
    // size, swapped in (its buffer comes back as scratch). Returns false, with
    // target untouched, when the sizes do not match. Only rows that differ are
    // blended afterwards, linearly over `duration`.
    bool Start(const std::vector<uint32_t> &shown, std::vector<uint32_t> &target, uint32_t width,
               Clock::time_point now, Clock::duration duration);
    // The opening: list rows fade in one after another, the rest together.
    bool StartCascade(const std::vector<uint32_t> &shown, std::vector<uint32_t> &target, uint32_t width,
                      Clock::time_point now, MenuCascade cascade);
    // Shows target at once; it stays the clean image Erase restores from.
    void Cut(std::vector<uint32_t> &pixels, std::vector<uint32_t> &target, uint32_t width);
    // Writes the frame for `now` into pixels, which must still hold this
    // transition's previous frame. Returns true while easing; the last frame is
    // exactly the target. A clock that moved backwards settles at once.
    bool Advance(std::vector<uint32_t> &pixels, Clock::time_point now);
    // Restores the clean image under rect where Advance did not just rewrite it,
    // removing a layer drawn on top (the arrow).
    void Erase(std::vector<uint32_t> &pixels, const MenuRect &rect) const;
    bool Running() const { return running; }
    Clock::duration Duration() const { return duration; }
    void Reset();

  private:
    double Weight(size_t row, bool list, double seconds) const;
    std::vector<uint32_t> from, to;
    std::vector<std::pair<size_t, size_t>> spans; // pixel ranges of the rows that differ
    MenuCascade cascade;                          // empty bands: one linear fade
    size_t width = 0;
    Clock::time_point start{};
    Clock::duration duration{};
    bool running = false;
};
}
