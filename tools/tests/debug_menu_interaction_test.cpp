#include <debug/menu_overlay.h>
#include <debug/teleport.h>
#include <debug/battle_menu.h>
#include <debug/map_info.h>
#include <debug/save_anywhere.h>
#include <host_ui/host_ui.h>
#include <host_ui/rasterizer.h>
#include <debug/translations.h>
#include <debug/cheat_overlay.h>
#include <settings/config.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "REQUIRE FAILED: %s\n", message);
        std::exit(1);
    }
}

struct MockServices
{
    uint32_t selectedLanguage = 1;
    bool saveLanguageSuccess = true;
    bool keepAudioPitch = true;
    bool saveFastForwardSuccess = true;
    int fastForwardSaveCalls = 0;
    uint32_t menuAnimationRate = 1;
    bool saveMenuAnimationSuccess = true;
    int menuAnimationSaveCalls = 0;
    bool captureRequested = false;
    bool saveAnywhere = false;
    int partySwitchRequests = 0;
    settings::EncounterRate encounterRate = settings::EncounterRate::Normal;
    int encounterRateSaveCalls = 0;
    bool battleExpDouble = false;
    bool everyStep = false;
    bool allowVictory = true;
    bool victoryRequested = false;
    bool victoryCancelled = false;
    bool allowSoftReset = true;
    int softResetRequests = 0;
    const wchar_t* softResetRefusal = nullptr;

    bool allowTeleport = true;
    bool allowSavePos = true;
    bool allowRestorePos = true;
    bool allowPoiTeleport = true;

    int teleportCalls = 0;
    int savePosCalls = 0;
    int restorePosCalls = 0;
    int poiTeleportCalls = 0;

    debug_menu::Position lastTeleport{};
    debug_menu::TeleportSnapshot snapshot;
} g_mock;
} // namespace

namespace hid { bool UsesPlayStationPrompts() { return false; } }

namespace settings
{
Config GetConfig()
{
    Config c;
    c.debugLanguage = g_mock.selectedLanguage;
    c.fastForwardKeepAudioPitch = g_mock.keepAudioPitch;
    c.menuAnimationRate = g_mock.menuAnimationRate;
    return c;
}
bool SaveDebugLanguage(uint32_t language)
{
    if (!g_mock.saveLanguageSuccess) return false;
    g_mock.selectedLanguage = language;
    return true;
}
bool SaveFastForward(bool, uint32_t, uint32_t, bool keepAudioPitch)
{
    ++g_mock.fastForwardSaveCalls;
    if (!g_mock.saveFastForwardSuccess) return false;
    g_mock.keepAudioPitch = keepAudioPitch;
    return true;
}
bool SaveMenuAnimationRate(uint32_t rate)
{
    ++g_mock.menuAnimationSaveCalls;
    if (!g_mock.saveMenuAnimationSuccess) return false;
    g_mock.menuAnimationRate = rate;
    return true;
}
}

namespace gpu::renderer
{
void RequestDebugCapture() { g_mock.captureRequested = true; }
std::wstring DebugCaptureStatus() { return L"Ready"; }
}

namespace debug_menu
{
MapInfo GetMapInfo()
{
    return {true, 42, L"Test Wasteland", L"wasteland_01"};
}
bool SaveAnywhereEnabled() { return g_mock.saveAnywhere; }
void SetSaveAnywhereEnabled(bool enabled) { g_mock.saveAnywhere = enabled; }
void RequestPartySwitch() { ++g_mock.partySwitchRequests; }
settings::EncounterRate GetEncounterRate() { return g_mock.encounterRate; }
void SetEncounterRate(settings::EncounterRate rate)
{
    g_mock.encounterRate = rate;
    g_mock.everyStep = false;
    ++g_mock.encounterRateSaveCalls;
}
bool BattleExpDoubleEnabled() { return g_mock.battleExpDouble; }
void SetBattleExpDoubleEnabled(bool enabled) { g_mock.battleExpDouble = enabled; }
bool EncounterEveryStepEnabled() { return g_mock.everyStep; }
void SetEncounterEveryStepEnabled(bool enabled) { g_mock.everyStep = enabled; }

bool RequestVictory()
{
    if (!g_mock.allowVictory) return false;
    g_mock.victoryRequested = true;
    return true;
}
void CancelVictory()
{
    g_mock.victoryCancelled = true;
}
const wchar_t* Status()
{
    return g_mock.victoryRequested ? L"Waiting for safe phase" : L"Ready";
}

TeleportSnapshot GetTeleportSnapshot()
{
    return g_mock.snapshot;
}

bool RequestTeleport(Position position)
{
    if (!g_mock.allowTeleport) return false;
    g_mock.lastTeleport = position;
    ++g_mock.teleportCalls;
    return true;
}
bool RequestTeleportOffset(Position) { return true; }

bool RequestSavePosition()
{
    if (!g_mock.allowSavePos) return false;
    ++g_mock.savePosCalls;
    return true;
}
bool RequestRestorePosition()
{
    if (!g_mock.allowRestorePos) return false;
    ++g_mock.restorePosCalls;
    return true;
}
bool RequestPoiTeleport(uint64_t)
{
    if (!g_mock.allowPoiTeleport) return false;
    ++g_mock.poiTeleportCalls;
    return true;
}
const wchar_t* RequestMapJump(const char*) { return nullptr; }

namespace soft_reset
{
bool Request() { ++g_mock.softResetRequests; return g_mock.allowSoftReset; }
const wchar_t* Status() { return L"Please wait for loading to finish"; }
const wchar_t* TakeRefusal()
{
    const wchar_t* refusal = g_mock.softResetRefusal;
    g_mock.softResetRefusal = nullptr;
    return refusal;
}
}
}

int main()
{
    // Setup initial mock environment
    g_mock.snapshot.available = true;
    g_mock.snapshot.current = {100.0f, 200.0f, 300.0f};
    g_mock.snapshot.bookmarkAvailable = true;
    g_mock.snapshot.bookmark = {50.0f, 60.0f, 70.0f};
    g_mock.snapshot.poiRevision = 1;
    g_mock.snapshot.pois = {
        {1, L"Save Point 1", {100.0f, 200.0f, 300.0f}},
        {2, L"Chest 1", {150.0f, 250.0f, 350.0f}}
    };

    Require(!debug_menu::IsOverlayVisible(), "Overlay should be initially hidden");
    Require(!host_ui::IsGamePaused(), "Game should not be initially paused");

    // 1. Toggle Overlay & Pause State
    debug_menu::ToggleOverlay();
    Require(debug_menu::IsOverlayVisible(), "ToggleOverlay did not make overlay visible");
    Require(host_ui::IsGamePaused(), "ToggleOverlay did not pause the game");

    // 2. Tab 0 (Overview) Row 0: Language Toggle & failure handling
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.selectedLanguage == 0, "Language toggle did not change to English");

    g_mock.saveLanguageSuccess = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    g_mock.saveLanguageSuccess = true;

    // Row 1: Render Capture
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.captureRequested, "Capture was not requested");

    // Row 2: Save Anywhere
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.saveAnywhere, "Save anywhere was not enabled");

    // Row 3: Force RB party switch
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.partySwitchRequests == 1, "Party switch was not requested");

    // Row 4: Return to title requires an explicit Yes; nested inputs stay modal.
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.softResetRequests == 0 && debug_menu::IsOverlayVisible(),
        "Return to title must default to No and keep confirmation modal");
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Cancel);
    Require(g_mock.softResetRequests == 0 && debug_menu::IsOverlayVisible() && host_ui::IsGamePaused(),
        "Cancelling Return to title must retain the menu without submitting a reset");
    g_mock.allowSoftReset = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.softResetRequests == 1 && debug_menu::IsOverlayVisible() && host_ui::IsGamePaused(),
        "Refused Return to title must retain the menu and pause");
    g_mock.allowSoftReset = true;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.softResetRequests == 2 && !debug_menu::IsOverlayVisible() && !host_ui::IsGamePaused(),
        "Accepted Return to title must close the menu and resume guest execution");
    debug_menu::PollResetNotice();
    Require(!debug_menu::IsOverlayVisible() && !host_ui::IsGamePaused(),
        "An empty reset notice must not reopen the menu");
    g_mock.softResetRefusal = L"Please wait for saving to finish";
    debug_menu::PollResetNotice();
    Require(debug_menu::IsOverlayVisible() && host_ui::IsGamePaused() && !g_mock.softResetRefusal,
        "A guest-thread refusal must be consumed and reopen the menu with the game paused");
    debug_menu::PollResetNotice();
    Require(debug_menu::IsOverlayVisible() && host_ui::IsGamePaused() && g_mock.softResetRequests == 2,
        "Polling a consumed refusal must retain the menu without another reset request");
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    debug_menu::HandleInput(debug_menu::InputAction::NextTab); // Cheats
    g_mock.softResetRefusal = L"Please wait for loading to finish";
    debug_menu::PollResetNotice();
    Require(debug_menu::IsOverlayVisible() && host_ui::IsGamePaused() && !g_mock.softResetRefusal,
        "A late refusal on Cheats must retain the paused menu and consume the notice");
    Require(!g_mock.everyStep && g_mock.encounterRate == settings::EncounterRate::Normal && g_mock.saveAnywhere,
        "Return to title changed the player's cheat options");

    // Late refusal returns to Overview row 4, including from an already-open Cheats page.
    // Row 5: Win Battle (Success and Rejection)
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.victoryRequested, "Victory request not registered");

    g_mock.allowVictory = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);

    // Row 6: Cancel Victory, the last Overview row
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.victoryCancelled, "Cancel victory was not called");
    g_mock.victoryCancelled = false;
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.victoryCancelled, "Down moved past the last Overview row");

    // 3. Tab Switching: NextTab -> Teleport Tab
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    debug_menu::UpdateOverlaySnapshot();

    // 4. Tab 1 (Teleport) Row 0: Save Position
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.savePosCalls == 1, "Save position was not called");

    g_mock.allowSavePos = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.savePosCalls == 1, "Save position should be rejected without increment");

    // Row 1: Restore Position
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.restorePosCalls == 1, "Restore position was not called");

    g_mock.allowRestorePos = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.restorePosCalls == 1, "Restore position should be rejected without increment");

    // Row 2: Fill Current Coordinates
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);

    // Row 3: Coordinate Axis selection & adjustment
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Left);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Switch to Y axis
    debug_menu::HandleInput(debug_menu::InputAction::Right);

    // Row 4: Teleport to XYZ
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.teleportCalls == 1, "Teleport XYZ was not called");
    Require(g_mock.lastTeleport.x == 100.0f && g_mock.lastTeleport.y == 300.0f &&
                g_mock.lastTeleport.z == 300.0f,
            "Axis adjustment did not submit the expected XYZ coordinates");

    // Dirty Z, then refill without closing/reopening the overlay. Opening also
    // initializes XYZ, so testing only the initial fill misses a stale Z value.
    debug_menu::HandleInput(debug_menu::InputAction::Up); // Row 3, Y selected
    debug_menu::HandleInput(debug_menu::InputAction::Confirm); // Switch to Z
    debug_menu::HandleInput(debug_menu::InputAction::Right); // Z = 400
    debug_menu::HandleInput(debug_menu::InputAction::Up); // Row 2: Fill
    g_mock.snapshot.current = {-125.0f, 450.0f, 875.0f};
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Down); // Row 4: Teleport
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.teleportCalls == 2, "Teleport after refilling coordinates was not called");
    Require(g_mock.lastTeleport.x == -125.0f && g_mock.lastTeleport.y == 450.0f &&
                g_mock.lastTeleport.z == 875.0f,
            "Fill Coordinates did not replace every edited axis with the current position");

    // An unavailable snapshot must preserve the user's target, not partially
    // replace it with invalid coordinates.
    debug_menu::HandleInput(debug_menu::InputAction::Up);
    debug_menu::HandleInput(debug_menu::InputAction::Up); // Row 2: Fill
    g_mock.snapshot.available = false;
    g_mock.snapshot.current = {1.0f, 2.0f, 3.0f};
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.teleportCalls == 3, "Teleport after an unavailable fill was not called");
    Require(g_mock.lastTeleport.x == -125.0f && g_mock.lastTeleport.y == 450.0f &&
                g_mock.lastTeleport.z == 875.0f,
            "An unavailable Fill Coordinates request changed the target");
    g_mock.snapshot.available = true;

    g_mock.allowTeleport = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.teleportCalls == 3, "Teleport XYZ should be rejected without increment");

    // Row 5: Step Size Switch
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Right);

    // Row 6: POI Selection Cycling
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    debug_menu::HandleInput(debug_menu::InputAction::Left);

    // Row 7: Teleport to POI
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.poiTeleportCalls == 1, "POI teleport was not called");

    g_mock.allowPoiTeleport = false;
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.poiTeleportCalls == 1, "POI teleport should be rejected without increment");

    // 5. Render overlay into PixelBuffer with Rasterizer
    host_ui::PixelBuffer frame;
    Require(frame.Resize(), "Logical overlay allocation failed");
    host_ui::Rasterizer rasterizer(frame);
    debug_menu::RenderOverlay(rasterizer);

    bool hasNonZeroPixel = false;
    for (uint32_t px : frame.pixels)
    {
        if (px != 0) { hasNonZeroPixel = true; break; }
    }
    Require(hasNonZeroPixel, "RenderOverlay produced completely blank frame");

    // Verify accurate text measurement (ASCII 9px, CJK 17px)
    Require(rasterizer.MeasureWString(L"A") == 9, "MeasureWString ASCII glyph width should be 9px");
    Require(rasterizer.MeasureWString(L"中") == 17, "MeasureWString CJK glyph width should be 17px");
    Require(rasterizer.MeasureWString(L"中文测试") == 17 * 4, "MeasureWString 4 CJK glyphs width should be 68px");

    // Verify translations bidirectional clean mapping
    Require(std::wstring(debug_menu::translations::Text(L"当前战斗判胜", false)) == L"Win Current Battle", "Win battle translation to English");
    Require(std::wstring(debug_menu::translations::Text(L"Win Current Battle", true)) == L"当前战斗判胜", "Win battle translation to Chinese");
    Require(std::wstring(debug_menu::translations::Poi(L"存档点 1", false)) == L"Save point 1", "POI translation to English");
    Require(std::wstring(debug_menu::translations::Poi(L"Save Point 1", true)) == L"存档点 1", "POI translation to Chinese");

    for (const auto& notice : {
        std::pair{L"Return to title unavailable", L"当前无法返回标题"},
        std::pair{L"Not available during battle", L"战斗期间不可用"},
        std::pair{L"Please wait for loading to finish", L"请等待读取完成"},
        std::pair{L"Please wait for saving to finish", L"请等待保存完成"},
        std::pair{L"Close the save/load screen first", L"先关闭存档/读档画面"},
        std::pair{L"Already at the title screen", L"当前已在标题画面"},
        std::pair{L"Returning to title", L"正在返回标题"}})
    {
        Require(std::wstring(debug_menu::translations::Text(notice.first, true)) == notice.second,
            "Return to title notice lacks its Chinese translation");
        for (bool zh : {false, true})
            Require(rasterizer.MeasureWString(debug_menu::translations::Text(notice.first, zh)) <= 700,
                "Return to title notice exceeds the Overview text width");
    }

    // Live quick-tool options must fit their production text columns.
    for (bool zh : {false, true})
    {
        for (const auto* key : {L"Return to title", L"Unsaved progress will be lost"})
            Require(rasterizer.MeasureWString(debug_menu::translations::Text(key, zh)) <= 520,
                "Return to title confirmation text exceeds the dialog width");
        Require(rasterizer.MeasureWString(debug_menu::translations::Text(
            L"Return to title after confirmation; unavailable in battle.", zh)) <= 700,
            "Return to title help exceeds the Overview text width");
        for (const auto* label : {debug_menu::cheat_overlay::FastForwardAudioLabel[zh],
                                 debug_menu::cheat_overlay::EncounterRateLabel[zh],
                                 debug_menu::cheat_overlay::BattleExpLabel[zh],
                                 debug_menu::cheat_overlay::MenuAnimationLabel[zh]})
            Require(rasterizer.MeasureWString(label) <= 170,
                "Quick-tool label exceeds the Cheats label column");
        for (const auto& rate : debug_menu::cheat_overlay::EncounterRateValues)
            Require(rasterizer.MeasureWString(rate[zh]) <= 350,
                "Encounter rate value exceeds the Cheats value column");
        for (const auto& mode : debug_menu::cheat_overlay::FastForwardAudioValues)
            Require(rasterizer.MeasureWString(mode[zh]) <= 350,
                "Fast-forward audio value exceeds the Cheats value column");
        for (const auto* value : {L"ON", L"OFF"})
            Require(rasterizer.MeasureWString(value) <= 350,
                "Battle EXP value exceeds the Cheats value column");
        const std::wstring help = debug_menu::cheat_overlay::MenuAnimationHelp[zh];
        Require(help.find(L'\n') == std::wstring::npos && rasterizer.MeasureWString(help) <= 700,
            "Menu animations help must fit one complete line");
        for (const auto rate : settings::MenuAnimationRates)
            Require(rasterizer.MeasureWString(debug_menu::cheat_overlay::MenuAnimationValue(rate, zh)) <= 350,
                "Menu animations value exceeds the Cheats value column");
    }
    const debug_menu::cheat_overlay::Model quickTools;
    Require(quickTools.Rows() == 12 &&
            (int(quickTools.Rows()) - 1) * debug_menu::cheat_overlay::QuickToolRowHeight + 23 <= 296,
        "Quick-tool rows overlap the help area");

    // Cheats -> Quick tools: the independent live options need no memory-edit
    // confirmation and remain usable before a playable field has loaded.
    Require(!debug_menu::cheats::session.Get().enabled, "Memory edits unexpectedly enabled");
    debug_menu::HandleInput(debug_menu::InputAction::NextTab);
    for(int i=0;i<4;++i) debug_menu::HandleInput(debug_menu::InputAction::Down);
    Require(debug_menu::fast_forward::KeepAudioPitch(), "Fast-forward audio should initially keep pitch");
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(!debug_menu::fast_forward::KeepAudioPitch() && !g_mock.keepAudioPitch,
        "Confirm should select and persist Speed up");
    g_mock.saveFastForwardSuccess = false;
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    Require(!debug_menu::fast_forward::KeepAudioPitch() && !g_mock.keepAudioPitch,
        "Failed audio-mode save must preserve the live and persisted choice");
    g_mock.saveFastForwardSuccess = true;
    debug_menu::HandleInput(debug_menu::InputAction::Left);
    Require(debug_menu::fast_forward::KeepAudioPitch() && g_mock.keepAudioPitch && g_mock.fastForwardSaveCalls == 3,
        "Left should select Keep pitch and persist once per change");
    Require(!debug_menu::fast_forward::GetStatus().enabled,
        "Changing audio mode must not enable fast-forward");
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.encounterRate==settings::EncounterRate::Half, "Encounter rate did not select Half");
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    Require(g_mock.encounterRate==settings::EncounterRate::None, "Encounter rate did not select None");
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.everyStep && g_mock.encounterRate==settings::EncounterRate::None && g_mock.encounterRateSaveCalls==2,
        "Every step must enable the session flag and preserve the saved None choice");
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(!g_mock.everyStep && g_mock.encounterRate==settings::EncounterRate::Normal,
        "Encounter rate did not wrap from Every step to Normal and disable Every step");
    debug_menu::HandleInput(debug_menu::InputAction::Left);
    Require(g_mock.everyStep && g_mock.encounterRate==settings::EncounterRate::Normal && g_mock.encounterRateSaveCalls==3,
        "Backward wrap to Every step must preserve the saved Normal choice");
    debug_menu::HandleInput(debug_menu::InputAction::Left);
    Require(!g_mock.everyStep && g_mock.encounterRate==settings::EncounterRate::None,
        "Left from Every step must select None and disable Every step");
    debug_menu::HandleInput(debug_menu::InputAction::Down);
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.battleExpDouble && g_mock.encounterRate==settings::EncounterRate::None,
        "EXP toggle changed encounter rate or failed to enable");
    debug_menu::HandleInput(debug_menu::InputAction::Left);
    Require(!g_mock.battleExpDouble && g_mock.encounterRate==settings::EncounterRate::None,
        "EXP toggle changed encounter rate or failed to disable");
    Require(!debug_menu::cheats::session.Get().enabled, "Quick options enabled memory edits");
    debug_menu::RenderOverlay(rasterizer);

    // Cheats / Quick tools / Menu animations: forward/backward cycling persists each choice.
    debug_menu::HandleInput(debug_menu::InputAction::Down); // Row 7: Menu animations
    Require(g_mock.menuAnimationRate == 1, "Menu animation speed should initially be Normal");
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.menuAnimationRate == 2, "Confirm should select and save Fast");
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    Require(g_mock.menuAnimationRate == 4, "Right should select and save Faster");
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    Require(g_mock.menuAnimationRate == 1, "Right should wrap back to Normal");
    debug_menu::HandleInput(debug_menu::InputAction::Left);
    Require(g_mock.menuAnimationRate == 4 && g_mock.menuAnimationSaveCalls == 4,
        "Left should wrap to Faster and persist once per change");
    debug_menu::HandleInput(debug_menu::InputAction::Cancel);
    debug_menu::ToggleOverlay();
    debug_menu::HandleInput(debug_menu::InputAction::Confirm);
    Require(g_mock.menuAnimationRate == 1, "Reopening the debug menu must restore its persisted animation speed");
    g_mock.saveMenuAnimationSuccess = false;
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    Require(g_mock.menuAnimationRate == 1, "Failed animation save must preserve the persisted choice");
    g_mock.saveMenuAnimationSuccess = true;
    debug_menu::HandleInput(debug_menu::InputAction::Right);
    Require(g_mock.menuAnimationRate == 2, "Failed animation save must restore the row's previous choice");
    Require(g_mock.encounterRate == settings::EncounterRate::None && !g_mock.battleExpDouble &&
            !debug_menu::cheats::session.Get().enabled,
        "Menu animation changes affected another quick-tool option or enabled memory edits");
    debug_menu::RenderOverlay(rasterizer);

    // Both debug-menu languages must show the complete two-line help at the
    // production 700px content width, without an ellipsis or discarded text.
    for(bool zh : {false,true}) for(unsigned row : {4u,5u,6u,7u}) {
        std::wstring remaining=debug_menu::cheat_overlay::QuickToolHelp(row,zh);
        Require(!remaining.empty(), "Quick-tool help is missing a language");
        for(int line=0;!remaining.empty() && line<2;++line) {
            size_t n=remaining.size();
            while(n>1 && rasterizer.MeasureWString(remaining.substr(0,n))>700) --n;
            if(n<remaining.size()) {
                const auto space=remaining.rfind(L' ',n);
                if(space!=std::wstring::npos && space>n/2)n=space;
            }
            Require(rasterizer.MeasureWString(remaining.substr(0,n))<=700,"Help line exceeds content width");
            remaining.erase(0,n);
            while(!remaining.empty() && remaining.front()==L' ')remaining.erase(0,1);
        }
        Require(remaining.empty(), "Quick-tool help exceeds the two-line limit");
    }

    // 6. Dismiss overlay with Cancel (B / Esc)
    debug_menu::HandleInput(debug_menu::InputAction::Cancel);
    Require(!debug_menu::IsOverlayVisible(), "Cancel input did not hide overlay");
    Require(!host_ui::IsGamePaused(), "Cancel input did not unpause game");

    std::puts("PASS debug menu interaction: modal overlay, pause sync, navigation, error reporting, and rasterizer rendering");
    return 0;
}
