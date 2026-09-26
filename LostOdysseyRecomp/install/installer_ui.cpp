#include "installer_ui.h"
#include "installer_colors.h"
#include "installer_navigation.h"
#include "installer_font.h"
#include "file_browser.h"
#include "import_game.h"
#include "../hid/controller_prompts.h"
#include "../settings/config.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>
#include <deque>

#include <SDL.h>

namespace install
{
using namespace install::ui;
namespace
{
// Draw a compact, clean graphic folder icon
void DrawFolderIcon(SDL_Renderer* renderer, int x, int y, bool selected)
{
    Color bodyCol = selected ? Color{ 180, 135, 45, 255 } : COLOR_FOLDER_ICON;
    Color darkCol = selected ? Color{ 60, 45, 15, 255 } : Color{ 40, 42, 45, 255 };
    // Back tab
    FillRect(renderer, x, y, 6, 3, bodyCol);
    // Main folder body
    FillRect(renderer, x, y + 3, 14, 10, bodyCol);
    DrawRect(renderer, x, y + 3, 14, 10, darkCol);
}

// Draw a compact, clean graphic file icon
void DrawFileIcon(SDL_Renderer* renderer, int x, int y, bool selected)
{
    Color bodyCol = selected ? Color{ 100, 105, 110, 255 } : COLOR_FILE_ICON;
    Color lineCol = selected ? COLOR_SEL_INK : COLOR_STEEL_PANEL;
    FillRect(renderer, x + 2, y + 1, 10, 13, bodyCol);
    DrawRect(renderer, x + 2, y + 1, 10, 13, lineCol);
    // Dog-ear corner fold
    FillRect(renderer, x + 8, y + 1, 4, 4, selected ? COLOR_SEL_SURFACE : COLOR_STEEL_PANEL);
}

// Controller button prompt chip (e.g. [A], [B], [X])
void DrawButtonPrompt(SDL_Renderer* renderer, int x, int y, std::string_view btn, std::string_view label, Color btnColor, bool playStation)
{
    int btnW = playStation ? 24 : ui::MeasureTextWidth(btn, 1.0f) + 8;
    int btnH = 20;
    FillRect(renderer, x, y, btnW, btnH, btnColor);
    DrawRect(renderer, x, y, btnW, btnH, COLOR_BORDER_DARK);
    if (playStation && btn.size() == 1)
    {
        using hid::prompts::Face;
        const auto face = btn == "A" ? Face::A : btn == "B" ? Face::B : btn == "X" ? Face::X : Face::Y;
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        hid::prompts::DrawFace(face, x + 4, y + 2, 16, [&](int ax, int ay, int bx, int by) {
            SDL_RenderDrawLine(renderer, ax, ay, bx, by);
        });
    }
    else ui::DrawString(renderer, x + 4, y + 2, btn, 20, 20, 20, 255, 1.0f);
    ui::DrawString(renderer, x + btnW + 6, y + 2, label, COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
}

SDL_Texture* CreateDpadIcon(SDL_Renderer* renderer)
{
    // One transparent bitmap, uploaded once and reused by both browser screens.
    std::array<Uint32, 32 * 32> pixels{};
    for (int y = 0; y < 32; ++y)
    {
        for (int x = 0; x < 32; ++x)
        {
            const bool vertical = x >= 11 && x <= 21;
            const bool horizontal = y >= 11 && y <= 21;
            if (!vertical && !horizontal) continue;
            if ((y == 0 || y == 31) && (x == 11 || x == 21)) continue;
            if ((x == 0 || x == 31) && (y == 11 || y == 21)) continue;
            Uint32 color = x + y < 31 ? 0x41494AFF : 0x191F20FF;
            const bool arrow =
                (y >= 3 && y <= 7 && std::abs(x - 16) <= y - 3) ||
                (y >= 25 && y <= 29 && std::abs(x - 16) <= 29 - y) ||
                (x >= 3 && x <= 7 && std::abs(y - 16) <= x - 3) ||
                (x >= 25 && x <= 29 && std::abs(y - 16) <= 29 - x);
            if (arrow) color = 0xE4E9E8FF;
            else if ((x - 16) * (x - 16) + (y - 16) * (y - 16) <= 6) color = 0x41494AFF;
            pixels[y * 32 + x] = color;
        }
    }
    auto* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 32, 32);
    if (texture)
    {
        SDL_UpdateTexture(texture, nullptr, pixels.data(), 32 * sizeof(Uint32));
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    return texture;
}

void DrawNavigationPrompt(SDL_Renderer* renderer, SDL_Texture* icon, int x, int y, std::string_view label)
{
    const SDL_Rect target{ x, y - 6, 32, 32 };
    SDL_RenderCopy(renderer, icon, nullptr, &target);
    ui::DrawString(renderer, x + 38, y + 2, label, COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
}

std::string FormatBytes(uint64_t bytes)
{
    double gib = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    if (gib >= 1.0)
    {
        ss << gib << " GiB";
    }
    else
    {
        ss << mib << " MiB";
    }
    return ss.str();
}

std::string EllipsizePath(const std::string& path, size_t maxLen)
{
    if (path.length() <= maxLen) return path;
    if (maxLen <= 5) return path.substr(0, maxLen);
    return "..." + path.substr(path.length() - (maxLen - 3));
}

enum class ScreenState
{
    BrowseSource,
    ReviewDiscs,
    BrowseDest,
    Importing,
    Complete,
    Error
};

struct UIState : InstallerSessionState
{
    ScreenState screen = ScreenState::BrowseSource;

    // Source browsing
    std::filesystem::path currentSourceBrowse;
    std::vector<ui::DirectoryItem> sourceItems;
    int sourceSelectedIndex = 0;
    int sourceScrollOffset = 0;
    std::vector<ui::DirectoryItem> roots;
    int rootSelectedIndex = 0;
    bool focusOnRoots = false;

    // Scan result
    std::filesystem::path selectedSource;
    std::atomic<bool> isScanning{ false };
    int reviewSelectedIndex = 0;
    int reviewScrollOffset = 0;

    // Destination browsing
    std::filesystem::path selectedDest;
    std::filesystem::path currentDestBrowse;
    std::vector<ui::DirectoryItem> destItems;
    int destSelectedIndex = 0;
    int destScrollOffset = 0;
    bool destFocusOnRoots = false;
    int destRootSelectedIndex = 0;
    bool destNaming = false;
    bool destNameReplaceOnType = false;
    bool destStatusError = false;
    std::string destNewName;
    std::string destStatus;

    // Import progress
    std::atomic<bool> isImporting{ false };
    std::atomic<bool> cancelRequested{ false };
    std::atomic<uint64_t> progressDone{ 0 };
    std::atomic<uint64_t> progressTotal{ 1 };
    std::mutex progressMutex;
    std::string progressLabel;
    std::string importError;
    std::vector<int> installedDiscs;
    InstallResult installResult;
    struct WorkerEvent
    {
        enum class Type { ScanFinished, ScanFailed, ImportFinished, ImportFailed } type;
        ContentScan scan;
        InstallResult install;
        std::string error;
        bool cancelled = false;
    };
    std::mutex workerMutex;
    std::deque<WorkerEvent> workerEvents;

    // UI flags
    bool quit = false;
};

} // namespace

InstallerResult ShowInstallerUI(const std::filesystem::path& executableDirectory,
                                const std::filesystem::path& initialSource,
                                const std::filesystem::path& initialDest)
{
    InstallerResult result;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0)
    {
        result.error = std::string("SDL_Init failed: ") + SDL_GetError();
        return result;
    }

    std::vector<SDL_GameController*> controllers;
    hid::prompts::ActiveController promptController;
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
    {
        if (SDL_IsGameController(i))
        {
            if (auto* pad = SDL_GameControllerOpen(i))
            {
                controllers.push_back(pad);
                promptController.Connected(SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)), SDL_GameControllerGetType(pad));
            }
        }
    }

    constexpr int LOGICAL_WIN_WIDTH = 1280;
    constexpr int LOGICAL_WIN_HEIGHT = 720;

    int requestedWidth = LOGICAL_WIN_WIDTH;
    int requestedHeight = LOGICAL_WIN_HEIGHT;

    SDL_Rect displayBounds{};
    if (SDL_GetDisplayUsableBounds(0, &displayBounds) == 0 && displayBounds.w > 0 && displayBounds.h > 0)
    {
        int targetW = static_cast<int>(std::round(displayBounds.w * 0.8f));
        int targetH = static_cast<int>(std::round(displayBounds.h * 0.8f));

        requestedWidth = std::min(displayBounds.w, std::max(LOGICAL_WIN_WIDTH, targetW));
        requestedHeight = std::min(displayBounds.h, std::max(LOGICAL_WIN_HEIGHT, targetH));
    }

    float uiScale = 1.0f;
    float ddpi = 0.0f, hdpi = 0.0f, vdpi = 0.0f;
    if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) == 0 && hdpi > 0.0f)
    {
        // Standard baseline display DPI is 96.0f
        uiScale = hdpi / 96.0f;
    }
    uiScale = std::clamp(uiScale, 1.0f, 2.0f);

    int windowWidth = static_cast<int>(std::round(requestedWidth * uiScale));
    int windowHeight = static_cast<int>(std::round(requestedHeight * uiScale));

    if (displayBounds.w > 0 && displayBounds.h > 0)
    {
        windowWidth = std::min(windowWidth, displayBounds.w);
        windowHeight = std::min(windowHeight, displayBounds.h);
    }
#ifdef LO_INSTALLER_UI_TESTING
    if (SDL_getenv("LO_IMPORTER_PREVIEW_BMP"))
    {
        windowWidth = LOGICAL_WIN_WIDTH;
        windowHeight = LOGICAL_WIN_HEIGHT;
    }
#endif
    windowWidth = std::max(1, windowWidth);
    windowHeight = std::max(1, windowHeight);

    SDL_Window* window = SDL_CreateWindow(
        "Lost Odyssey Recomp - Game Content Installer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window)
    {
        result.error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        for (auto* pad : controllers)
        {
            SDL_GameControllerClose(pad);
        }
        SDL_Quit();
        return result;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer)
    {
        result.error = std::string("SDL_CreateRenderer failed: ") + SDL_GetError();
        for (auto* pad : controllers)
        {
            SDL_GameControllerClose(pad);
        }
        SDL_DestroyWindow(window);
        SDL_Quit();
        return result;
    }

    if (SDL_RenderSetLogicalSize(renderer, LOGICAL_WIN_WIDTH, LOGICAL_WIN_HEIGHT) != 0)
    {
        result.error = std::string("SDL_RenderSetLogicalSize failed: ") + SDL_GetError();
        for (auto* pad : controllers)
        {
            SDL_GameControllerClose(pad);
        }
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return result;
    }
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);

    SDL_Texture* dpadIcon = CreateDpadIcon(renderer);
    UIState state;
    const uint32_t uiLanguage = settings::GetConfig().uiLanguage;
    state.roots = ui::GetSystemRoots();
#ifdef LO_INSTALLER_UI_TESTING
    if (SDL_getenv("LO_IMPORTER_PREVIEW_BMP"))
    {
        // Test-only review fixture: no source scan, content write or import.
        ContentScan preview;
        for (int number = 1; number <= 4; ++number)
        {
            DiscInfo disc;
            disc.disc = number;
            disc.discs = 4;
            disc.edition = "Asia";
            disc.files = 218;
            disc.bytes = 244000000;
            preview.discs.push_back(std::move(disc));
        }
        for (int i = 0; i < 9; ++i)
        {
            DlcPackageInfo dlc;
            dlc.displayName = "Additional content " + std::to_string(i + 1);
            dlc.contentId = "DLC-" + std::to_string(i + 1);
            dlc.files = 12;
            dlc.bytes = 120000;
            preview.packages.push_back(std::move(dlc));
        }
        state.FinishScan(std::move(preview));
        state.ToggleSelection(1);
        state.screen = ScreenState::ReviewDiscs;
        state.reviewSelectedIndex = 1;
        if (const char* scroll = SDL_getenv("LO_IMPORTER_PREVIEW_SCROLL"); scroll && *scroll)
        {
            state.reviewScrollOffset = 2;
            state.reviewSelectedIndex = 12;
        }
    }
#endif

    std::filesystem::path startSource = initialSource;
    if (startSource.empty() || !std::filesystem::exists(startSource))
    {
        startSource = std::filesystem::current_path();
    }
    state.currentSourceBrowse = startSource;
    state.sourceItems = ui::ListDirectory(state.currentSourceBrowse);
    // If the list has at least two items, select index 1 (the first actual item below '..')
    if (state.sourceItems.size() > 1)
    {
        state.sourceSelectedIndex = 1;
    }

    std::filesystem::path defaultDest = initialDest;
    if (defaultDest.empty())
    {
        defaultDest = DefaultGameDirectory(executableDirectory);
    }
    state.selectedDest = defaultDest;
    state.currentDestBrowse = defaultDest.parent_path();
    if (!std::filesystem::exists(state.currentDestBrowse))
    {
        state.currentDestBrowse = std::filesystem::current_path();
    }
    state.destItems = ui::ListDirectory(state.currentDestBrowse);

    std::thread workerThread;

    auto startScan = [&](const std::filesystem::path& srcPath) {
        if (state.isScanning.load()) return;
        state.isScanning = true;
        state.cancelRequested = false;
        state.selectedSource = srcPath;
        state.BeginScan();

        if (workerThread.joinable()) workerThread.join();

        workerThread = std::thread([&state, srcPath]() {
            UIState::WorkerEvent event;
            try
            {
                event.type = UIState::WorkerEvent::Type::ScanFinished;
                event.scan = ScanContent(srcPath, [&state] { return state.cancelRequested.load(); });
            }
            catch (const Error& err)
            {
                event.type = UIState::WorkerEvent::Type::ScanFailed;
                event.error = err.what();
            }
            catch (const std::exception& e)
            {
                event.type = UIState::WorkerEvent::Type::ScanFailed;
                event.error = e.what();
            }
            { std::lock_guard<std::mutex> lock(state.workerMutex); state.workerEvents.push_back(std::move(event)); }
        });
    };

    auto startImport = [&]() {
        if (state.isScanning.load() || state.isImporting.load() || !state.BeginImport()) return;
        state.isImporting = true;
        state.cancelRequested = false;
        state.progressDone = 0;
        state.progressTotal = 1;
        state.importError.clear();
        state.screen = ScreenState::Importing;

        if (workerThread.joinable()) workerThread.join();

        const ContentScan selection = state.SelectedContent();
        const auto destination = state.selectedDest;
        workerThread = std::thread([&state, executableDirectory, selection, destination]() {
            UIState::WorkerEvent event;
            try
            {
                auto progressCb = [&](uint64_t done, uint64_t total, std::string_view label) {
                    state.progressDone = done;
                    state.progressTotal = std::max<uint64_t>(1, total);
                    std::lock_guard<std::mutex> lock(state.progressMutex);
                    state.progressLabel = std::string(label);
                };
                auto cancelCb = [&]() -> bool {
                    return state.cancelRequested.load();
                };

                event.install = ReimportContent(selection, destination, progressCb, cancelCb,
                    [&](const InstallResult& committed) {
                        if (state.cancelRequested.load())
                            throw Error("Import cancelled; original files were kept", true);
                        CommitImportedGamePath(committed, [&](const std::filesystem::path& root, std::string& error) {
                            return WriteGamePath(executableDirectory, root, error);
                        });
                    });
                if (!event.install.error.empty() || event.install.cancelled)
                    event.type = UIState::WorkerEvent::Type::ImportFailed;
                else
                    event.type = UIState::WorkerEvent::Type::ImportFinished;
            }
            catch (const Error& err)
            {
                if (err.cancelled())
                {
                    event.cancelled = true;
                    event.type = UIState::WorkerEvent::Type::ImportFailed;
                }
                else
                {
                    event.type = UIState::WorkerEvent::Type::ImportFailed;
                    event.error = err.what();
                }
            }
            catch (const std::exception& ex)
            {
                event.type = UIState::WorkerEvent::Type::ImportFailed;
                event.error = ex.what();
            }
            { std::lock_guard<std::mutex> lock(state.workerMutex); state.workerEvents.push_back(std::move(event)); }
        });
    };

    auto consumeWorkerEvents = [&]() {
        std::deque<UIState::WorkerEvent> events;
        { std::lock_guard<std::mutex> lock(state.workerMutex); events.swap(state.workerEvents); }
        for (auto& event : events)
        {
            if (event.type == UIState::WorkerEvent::Type::ScanFinished)
            {
                state.FinishScan(std::move(event.scan));
                state.reviewSelectedIndex = ReviewActionStart(state.scanResult);
                state.reviewScrollOffset = 0;
                state.isScanning = false;
            }
            else if (event.type == UIState::WorkerEvent::Type::ScanFailed)
            {
                state.FailScan(std::move(event.error));
                state.isScanning = false;
            }
            else
            {
                state.isImporting = false;
                state.installResult = std::move(event.install);
                state.installedDiscs = state.installResult.discs;
                state.importError = event.error.empty() ? state.installResult.error : event.error;
                switch (state.FinishImport(state.installResult, event.cancelled,
                                           event.type != UIState::WorkerEvent::Type::ImportFinished || !state.importError.empty()))
                {
                case InstallerSessionState::ImportOutcome::Complete: state.screen = ScreenState::Complete; break;
                case InstallerSessionState::ImportOutcome::Cancelled: state.screen = ScreenState::ReviewDiscs; break;
                case InstallerSessionState::ImportOutcome::Failed: state.screen = ScreenState::Error; break;
                }
            }
        }
    };

    auto refreshSourceList = [&]() {
        state.sourceItems = ui::ListDirectory(state.currentSourceBrowse);
        state.sourceSelectedIndex = 0;
        state.sourceScrollOffset = 0;
    };

    auto refreshDestList = [&]() {
        state.destItems = ui::ListDirectory(state.currentDestBrowse);
        state.destSelectedIndex = 0;
        state.destScrollOffset = 0;
    };

    auto cancelNewFolder = [&]() {
        state.destNaming = false;
        SDL_StopTextInput();
    };

    auto beginNewFolder = [&]() {
        if (state.screen != ScreenState::BrowseDest || state.isImporting.load()) return;
        state.destNewName = "New Folder";
        for (unsigned number = 2; std::filesystem::exists(state.currentDestBrowse / state.destNewName); ++number)
            state.destNewName = "New Folder " + std::to_string(number);
        state.destNameReplaceOnType = true;
        state.destNaming = true;
        state.destStatus.clear();
        SDL_StartTextInput();
    };

    auto confirmNewFolder = [&]() {
        const auto created = ui::CreateFolder(state.currentDestBrowse, state.destNewName);
        if (!created)
        {
            state.destStatus = created.error;
            state.destStatusError = true;
            return;
        }
        cancelNewFolder();
        state.currentDestBrowse = created.path;
        state.selectedDest = created.path;
        state.destFocusOnRoots = false;
        refreshDestList();
        state.destStatus = "Folder created and selected. Select this destination to continue.";
        state.destStatusError = false;
    };

    constexpr int VISIBLE_ITEMS = 14;
    constexpr int REVIEW_VISIBLE_ITEMS = 11;

    auto handleNavUp = [&]() {
        if (state.isScanning.load() || state.isImporting.load()) return;
        if (state.screen == ScreenState::BrowseSource)
        {
            if (state.focusOnRoots)
            {
                if (state.rootSelectedIndex > 0) state.rootSelectedIndex--;
            }
            else
            {
                if (state.sourceSelectedIndex > 0)
                {
                    state.sourceSelectedIndex--;
                    if (state.sourceSelectedIndex < state.sourceScrollOffset)
                        state.sourceScrollOffset = state.sourceSelectedIndex;
                }
            }
        }
        else if (state.screen == ScreenState::BrowseDest)
        {
            if (state.destFocusOnRoots)
            {
                if (state.destRootSelectedIndex > 0) state.destRootSelectedIndex--;
            }
            else
            {
                if (state.destSelectedIndex > 0)
                {
                    state.destSelectedIndex--;
                    if (state.destSelectedIndex < state.destScrollOffset)
                        state.destScrollOffset = state.destSelectedIndex;
                }
            }
        }
        else if (state.screen == ScreenState::ReviewDiscs)
        {
            state.reviewSelectedIndex = std::max(0, state.reviewSelectedIndex - 1);
            if (state.reviewSelectedIndex < state.reviewScrollOffset)
                state.reviewScrollOffset = state.reviewSelectedIndex;
            if (state.reviewSelectedIndex < ReviewActionStart(state.scanResult) &&
                state.reviewSelectedIndex >= state.reviewScrollOffset + REVIEW_VISIBLE_ITEMS)
                state.reviewScrollOffset = state.reviewSelectedIndex - REVIEW_VISIBLE_ITEMS + 1;
        }
    };

    auto handleNavDown = [&]() {
        if (state.isScanning.load() || state.isImporting.load()) return;
        if (state.screen == ScreenState::BrowseSource)
        {
            if (state.focusOnRoots)
            {
                if (state.rootSelectedIndex + 1 < static_cast<int>(state.roots.size())) state.rootSelectedIndex++;
            }
            else
            {
                if (state.sourceSelectedIndex + 1 < static_cast<int>(state.sourceItems.size()))
                {
                    state.sourceSelectedIndex++;
                    if (state.sourceSelectedIndex >= state.sourceScrollOffset + VISIBLE_ITEMS)
                        state.sourceScrollOffset = state.sourceSelectedIndex - VISIBLE_ITEMS + 1;
                }
            }
        }
        else if (state.screen == ScreenState::BrowseDest)
        {
            if (state.destFocusOnRoots)
            {
                if (state.destRootSelectedIndex + 1 < static_cast<int>(state.roots.size())) state.destRootSelectedIndex++;
            }
            else
            {
                if (state.destSelectedIndex + 1 < static_cast<int>(state.destItems.size()))
                {
                    state.destSelectedIndex++;
                    if (state.destSelectedIndex >= state.destScrollOffset + 13)
                        state.destScrollOffset = state.destSelectedIndex - 13 + 1;
                }
            }
        }
        else if (state.screen == ScreenState::ReviewDiscs)
        {
            state.reviewSelectedIndex = std::min(ReviewActionStart(state.scanResult) + 2, state.reviewSelectedIndex + 1);
            if (state.reviewSelectedIndex < ReviewActionStart(state.scanResult) &&
                state.reviewSelectedIndex >= state.reviewScrollOffset + REVIEW_VISIBLE_ITEMS)
                state.reviewScrollOffset = state.reviewSelectedIndex - REVIEW_VISIBLE_ITEMS + 1;
        }
    };

    auto handleNavLeft = [&]() {
        if (state.isScanning.load() || state.isImporting.load()) return;
        if (state.screen == ScreenState::BrowseSource)
        {
            state.focusOnRoots = true;
        }
        else if (state.screen == ScreenState::BrowseDest)
        {
            state.destFocusOnRoots = true;
        }
        else if (state.screen == ScreenState::ReviewDiscs)
        {
            if (state.reviewSelectedIndex >= ReviewActionStart(state.scanResult))
                state.reviewSelectedIndex = std::max(ReviewActionStart(state.scanResult), state.reviewSelectedIndex - 1);
        }
    };

    auto handleNavRight = [&]() {
        if (state.isScanning.load() || state.isImporting.load()) return;
        if (state.screen == ScreenState::BrowseSource)
        {
            state.focusOnRoots = false;
        }
        else if (state.screen == ScreenState::BrowseDest)
        {
            state.destFocusOnRoots = false;
        }
        else if (state.screen == ScreenState::ReviewDiscs)
        {
            if (state.reviewSelectedIndex >= ReviewActionStart(state.scanResult))
                state.reviewSelectedIndex = std::min(ReviewActionStart(state.scanResult) + 2, state.reviewSelectedIndex + 1);
        }
    };

    auto handleAction = [&]() {
        if (state.isScanning.load() || state.isImporting.load()) return;
        if (state.screen == ScreenState::BrowseSource)
        {
            if (state.focusOnRoots)
            {
                if (state.rootSelectedIndex >= 0 && state.rootSelectedIndex < static_cast<int>(state.roots.size()))
                {
                    state.currentSourceBrowse = state.roots[state.rootSelectedIndex].path;
                    refreshSourceList();
                    state.focusOnRoots = false;
                }
            }
            else
            {
                if (state.sourceSelectedIndex >= 0 && state.sourceSelectedIndex < static_cast<int>(state.sourceItems.size()))
                {
                    const auto& item = state.sourceItems[state.sourceSelectedIndex];
                    if (item.isDirectory)
                    {
                        std::error_code ec;
                        auto can = std::filesystem::canonical(item.path, ec);
                        state.currentSourceBrowse = ec ? item.path : can;
                        refreshSourceList();
                    }
                }
            }
        }
        else if (state.screen == ScreenState::BrowseDest)
        {
            if (state.destFocusOnRoots)
            {
                if (state.destRootSelectedIndex >= 0 && state.destRootSelectedIndex < static_cast<int>(state.roots.size()))
                {
                    state.currentDestBrowse = state.roots[state.destRootSelectedIndex].path;
                    refreshDestList();
                    state.destFocusOnRoots = false;
                }
            }
            else
            {
                if (state.destSelectedIndex >= 0 && state.destSelectedIndex < static_cast<int>(state.destItems.size()))
                {
                    const auto& item = state.destItems[state.destSelectedIndex];
                    if (item.isDirectory)
                    {
                        std::error_code ec;
                        auto can = std::filesystem::canonical(item.path, ec);
                        state.currentDestBrowse = ec ? item.path : can;
                        refreshDestList();
                    }
                }
            }
        }
        else if (state.screen == ScreenState::ReviewDiscs)
        {
            int actionStart = ReviewActionStart(state.scanResult);
            if (state.reviewSelectedIndex < actionStart)
            {
                state.ToggleSelection(state.reviewSelectedIndex);
            }
            else if (state.reviewSelectedIndex == actionStart)
            {
                startImport();
            }
            else if (state.reviewSelectedIndex == actionStart + 1)
            {
                state.screen = ScreenState::BrowseDest;
            }
            else if (state.reviewSelectedIndex == actionStart + 2)
            {
                state.screen = ScreenState::BrowseSource;
            }
        }
        else if (state.screen == ScreenState::Complete)
        {
            state.quit = true;
        }
        else if (state.screen == ScreenState::Error)
        {
            state.screen = ScreenState::ReviewDiscs;
        }
    };

    auto handleSelectCurrent = [&]() {
        if (state.isScanning.load() || state.isImporting.load()) return;
        if (state.screen == ScreenState::BrowseSource)
        {
            startScan(state.currentSourceBrowse);
            state.screen = ScreenState::ReviewDiscs;
            state.reviewSelectedIndex = ReviewActionStart(state.scanResult);
        }
        else if (state.screen == ScreenState::BrowseDest)
        {
            state.selectedDest = state.currentDestBrowse;
            state.screen = ScreenState::ReviewDiscs;
        }
    };

    auto handleCancel = [&]() {
        if (state.isScanning.load())
        {
            state.cancelRequested = true;
            state.screen = ScreenState::BrowseSource;
            return;
        }
        if (state.screen == ScreenState::BrowseSource)
        {
            state.quit = true;
            state.userCancelled = true;
        }
        else if (state.screen == ScreenState::ReviewDiscs)
        {
            state.reviewScrollOffset = 0;
            state.screen = ScreenState::BrowseSource;
        }
        else if (state.screen == ScreenState::BrowseDest)
        {
            state.screen = ScreenState::ReviewDiscs;
        }
        else if (state.screen == ScreenState::Importing)
        {
            state.cancelRequested = true;
        }
        else if (state.screen == ScreenState::Complete || state.screen == ScreenState::Error)
        {
            state.quit = true;
        }
    };

    ui::StickNavigation stickNavigation;
    while (!state.quit)
    {
        consumeWorkerEvents();
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                state.quit = true;
                state.userCancelled = true;
                if (state.isImporting.load() || state.isScanning.load())
                {
                    state.cancelRequested = true;
                }
                break;

            case SDL_KEYDOWN:
                promptController.Keyboard();
                if (state.destNaming)
                {
                    switch (event.key.keysym.sym)
                    {
                    case SDLK_RETURN: case SDLK_KP_ENTER: confirmNewFolder(); break;
                    case SDLK_ESCAPE: cancelNewFolder(); break;
                    case SDLK_BACKSPACE:
                        if (state.destNameReplaceOnType) state.destNewName.clear();
                        else if (!state.destNewName.empty())
                        {
                            auto end = state.destNewName.size() - 1;
                            while (end > 0 && (static_cast<unsigned char>(state.destNewName[end]) & 0xc0) == 0x80) --end;
                            state.destNewName.erase(end);
                        }
                        state.destNameReplaceOnType = false;
                        state.destStatus.clear();
                        break;
                    default: break;
                    }
                    break;
                }
                switch (event.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    handleCancel();
                    break;
                case SDLK_UP:
                case SDLK_w:
                    handleNavUp();
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    handleNavDown();
                    break;
                case SDLK_LEFT:
                case SDLK_a:
                    handleNavLeft();
                    break;
                case SDLK_RIGHT:
                case SDLK_d:
                    handleNavRight();
                    break;
                case SDLK_RETURN:
                case SDLK_SPACE:
                    handleAction();
                    break;
                case SDLK_TAB:
                case SDLK_f:
                    handleSelectCurrent();
                    break;
                case SDLK_F2:
                    beginNewFolder();
                    break;
                case SDLK_BACKSPACE:
                    if (state.screen == ScreenState::BrowseSource && !state.focusOnRoots)
                    {
                        state.currentSourceBrowse = state.currentSourceBrowse.parent_path();
                        refreshSourceList();
                    }
                    else if (state.screen == ScreenState::BrowseDest && !state.destFocusOnRoots)
                    {
                        state.currentDestBrowse = state.currentDestBrowse.parent_path();
                        refreshDestList();
                    }
                    else
                    {
                        handleCancel();
                    }
                    break;
                default:
                    break;
                }
                break;

            case SDL_TEXTINPUT:
                if (state.destNaming && event.text.text[0])
                {
                    if (state.destNameReplaceOnType) state.destNewName.clear();
                    state.destNameReplaceOnType = false;
                    if (state.destNewName.size() + std::strlen(event.text.text) <= 240)
                        state.destNewName += event.text.text;
                    state.destStatus.clear();
                }
                break;

            case SDL_MOUSEWHEEL:
                if (state.screen == ScreenState::ReviewDiscs && !state.isScanning.load())
                    state.reviewScrollOffset = std::clamp(state.reviewScrollOffset - event.wheel.y,
                        0, std::max(0, ReviewActionStart(state.scanResult) - REVIEW_VISIBLE_ITEMS));
                break;

            case SDL_CONTROLLERDEVICEADDED:
            {
                const auto id = SDL_JoystickGetDeviceInstanceID(event.cdevice.which);
                const bool opened = std::any_of(controllers.begin(), controllers.end(), [id](auto* pad) {
                    return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == id;
                });
                if (!opened && SDL_IsGameController(event.cdevice.which))
                    if (auto* pad = SDL_GameControllerOpen(event.cdevice.which)) {
                        controllers.push_back(pad);
                        promptController.Connected(id, SDL_GameControllerGetType(pad));
                    }
                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED:
                std::erase_if(controllers, [&](auto* pad) {
                    if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) != event.cdevice.which) return false;
                    promptController.Disconnected(event.cdevice.which);
                    SDL_GameControllerClose(pad);
                    return true;
                });
                break;

            case SDL_CONTROLLERBUTTONDOWN:
                if (state.destNaming)
                {
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
                        event.cbutton.button == SDL_CONTROLLER_BUTTON_Y) confirmNewFolder();
                    else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) cancelNewFolder();
                    break;
                }
                switch (event.cbutton.button)
                {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    handleNavUp();
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    handleNavDown();
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    handleNavLeft();
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    handleNavRight();
                    break;
                case SDL_CONTROLLER_BUTTON_A:
                    handleAction();
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                    handleCancel();
                    break;
                case SDL_CONTROLLER_BUTTON_X:
                    handleSelectCurrent();
                    break;
                case SDL_CONTROLLER_BUTTON_Y:
                    if (state.screen == ScreenState::BrowseDest) beginNewFolder();
                    else handleSelectCurrent();
                    break;
                default:
                    break;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    // SDL already converts button.x/y to logical coordinates after
                    // SDL_RenderSetLogicalSize; do not convert a second time.
                    int mx = event.button.x;
                    int my = event.button.y;
                    int winW = LOGICAL_WIN_WIDTH;
                    int winH = LOGICAL_WIN_HEIGHT;
                    int curBodyY = 80;
                    int curFooterY = winH - 68;
                    int curBodyH = curFooterY - curBodyY - 10;

                    if (state.screen == ScreenState::BrowseSource || state.screen == ScreenState::BrowseDest)
                    {
                        const int mainX = 194;
                        const int newFolderY = curBodyY + curBodyH - 44;
                        if (state.screen == ScreenState::BrowseDest &&
                            mx >= mainX + 14 && mx <= mainX + 230 &&
                            my >= newFolderY && my <= newFolderY + 30)
                        {
                            if (state.destNaming) confirmNewFolder();
                            else beginNewFolder();
                            break;
                        }
                        if (state.destNaming && mx >= mainX + 244 && mx <= mainX + 390 &&
                            my >= newFolderY && my <= newFolderY + 30)
                        {
                            cancelNewFolder();
                            break;
                        }
                        if (state.destNaming) break;
                        bool isSource = (state.screen == ScreenState::BrowseSource);
                        auto& items = isSource ? state.sourceItems : state.destItems;
                        auto& selIdx = isSource ? state.sourceSelectedIndex : state.destSelectedIndex;
                        auto& scrollOff = isSource ? state.sourceScrollOffset : state.destScrollOffset;
                        auto& onRoots = isSource ? state.focusOnRoots : state.destFocusOnRoots;
                        auto& rootIdx = isSource ? state.rootSelectedIndex : state.destRootSelectedIndex;

                        int rootsW = 160;
                        int rootListY = curBodyY + 44;
                        int rootItemH = 28;

                        // Click in roots panel
                        if (mx >= 20 && mx <= 20 + rootsW && my >= rootListY)
                        {
                            int clickedRoot = (my - rootListY) / rootItemH;
                            if (clickedRoot >= 0 && clickedRoot < static_cast<int>(state.roots.size()))
                            {
                                onRoots = true;
                                rootIdx = clickedRoot;
                                handleAction();
                            }
                        }
                        else
                        {
                            // Click in main list
                            int mainX = 20 + rootsW + 14;
                            int mainW = winW - mainX - 20;
                            int listY = curBodyY + 76;
                            int rowH = 28;
                            int maxVisible = (curBodyH - (isSource ? 90 : 180)) / rowH;

                            // Check if path bar was clicked to select folder
                            if (mx >= mainX + 14 && mx <= mainX + mainW - 14 && my >= curBodyY + 36 && my <= curBodyY + 64)
                            {
                                handleSelectCurrent();
                            }
                            else if (mx >= mainX + 14 && mx <= mainX + mainW - 14 && my >= listY)
                            {
                                int clickedVisible = (my - listY) / rowH;
                                if (clickedVisible >= 0 && clickedVisible < maxVisible)
                                {
                                    int clickedIndex = clickedVisible + scrollOff;
                                    if (clickedIndex >= 0 && clickedIndex < static_cast<int>(items.size()))
                                    {
                                        onRoots = false;
                                        selIdx = clickedIndex;
                                        handleAction();
                                    }
                                }
                            }
                        }
                    }
                    else if (state.screen == ScreenState::ReviewDiscs)
                    {
                        constexpr int tableY = 80 + 110;
                        constexpr int firstRowY = tableY + 32;
                        if (mx >= 40 && mx <= winW - 40 && my >= firstRowY &&
                            my < firstRowY + REVIEW_VISIBLE_ITEMS * 28)
                        {
                            const int item = state.reviewScrollOffset + (my - firstRowY) / 28;
                            if (item < ReviewActionStart(state.scanResult))
                            {
                                state.reviewSelectedIndex = item;
                                handleAction();
                            }
                            break;
                        }
                        int btnY = curBodyY + curBodyH - 65;
                        int btnW = 230;
                        int btnH = 38;
                        int actionStart = ReviewActionStart(state.scanResult);

                        if (my >= btnY && my <= btnY + btnH)
                        {
                            if (mx >= 40 && mx <= 40 + btnW)
                            {
                                state.reviewSelectedIndex = actionStart;
                                handleAction();
                            }
                            else if (mx >= 40 + btnW + 20 && mx <= 40 + btnW + 20 + btnW)
                            {
                                state.reviewSelectedIndex = actionStart + 1;
                                handleAction();
                            }
                            else if (mx >= 40 + (btnW + 20) * 2 && mx <= 40 + (btnW + 20) * 2 + btnW)
                            {
                                state.reviewSelectedIndex = actionStart + 2;
                                handleAction();
                            }
                        }
                    }
                    else if (state.screen == ScreenState::Complete || state.screen == ScreenState::Error)
                    {
                        handleAction();
                    }
                }
                break;

            default:
                break;
            }
        }

        int stickX = 0;
        int stickY = 0;
        if ((SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) &&
            !state.isScanning.load() && !state.isImporting.load() && !state.destNaming)
        {
            for (auto* controller : controllers)
            {
                if (!SDL_GameControllerGetAttached(controller)) continue;
                const int x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
                const int y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
                if (std::max(std::abs(x), std::abs(y)) > std::max(std::abs(stickX), std::abs(stickY)))
                {
                    stickX = x;
                    stickY = y;
                }
            }
        }
        switch (stickNavigation.Update(stickX, stickY, SDL_GetTicks64()))
        {
        case ui::Direction::Left: handleNavLeft(); break;
        case ui::Direction::Right: handleNavRight(); break;
        case ui::Direction::Up: handleNavUp(); break;
        case ui::Direction::Down: handleNavDown(); break;
        case ui::Direction::None: break;
        }

        SDL_GameControllerUpdate();
        for (auto* pad : controllers)
        {
            if (!SDL_GameControllerGetAttached(pad)) continue;
            uint32_t buttons = 0;
            for (int button = SDL_CONTROLLER_BUTTON_A; button < SDL_CONTROLLER_BUTTON_MAX; ++button)
                if (SDL_GameControllerGetButton(pad, SDL_GameControllerButton(button))) buttons |= 1u << button;
            const auto moved = [&](SDL_GameControllerAxis axis) {
                return std::abs(int(SDL_GameControllerGetAxis(pad, axis))) > 12000;
            };
            const bool stick = moved(SDL_CONTROLLER_AXIS_LEFTX) || moved(SDL_CONTROLLER_AXIS_LEFTY) ||
                               moved(SDL_CONTROLLER_AXIS_RIGHTX) || moved(SDL_CONTROLLER_AXIS_RIGHTY) ||
                               SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000 ||
                               SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000;
            promptController.Observe(SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)), buttons, stick);
        }
        const bool playStation = promptController.PlayStation();
        auto buttonPrompt = [&](int x, int y, std::string_view btn, std::string_view label, Color color) {
            DrawButtonPrompt(renderer, x, y, btn, label, color, playStation);
        };

        // Render pass
        int w = LOGICAL_WIN_WIDTH;
        int h = LOGICAL_WIN_HEIGHT;
        SetDrawColor(renderer, COLOR_STEEL);
        SDL_RenderClear(renderer);

            // Header: Lost Odyssey Steel Bar
            DrawBevelPanel(renderer, 0, 0, w, 70, COLOR_STEEL_PANEL);
            ui::DrawString(renderer, 28, 16, "LOST ODYSSEY RECOMP", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 1.25f);
            ui::DrawString(renderer, 28, 42, "Content Importer - Disc & Extracted Folder Setup", COLOR_GOLD_MUTED.r, COLOR_GOLD_MUTED.g, COLOR_GOLD_MUTED.b, 255, 0.9f);

        // Footer / Controller & Keyboard Prompts Bar
        int footerH = 68;
        int footerY = h - footerH;
        DrawBevelPanel(renderer, 0, footerY, w, footerH, COLOR_STEEL_PANEL);

        // Row 1: Controller button chips matching Lost Odyssey legend
        int chipX = 24;
        int chipY = footerY + 10;

        if (state.screen == ScreenState::BrowseSource || state.screen == ScreenState::BrowseDest)
        {
            buttonPrompt(chipX, chipY, "A", "Open", COLOR_GREEN);
            chipX += 90;
            buttonPrompt(chipX, chipY, "X", "Select", COLOR_CYAN);
            chipX += 100;
            if (state.screen == ScreenState::BrowseDest)
            {
                buttonPrompt(chipX, chipY, "Y", "New folder", COLOR_ACCENT_GOLD);
                chipX += 145;
            }
            buttonPrompt(chipX, chipY, "B", "Back", COLOR_RED);
            chipX += 90;
            DrawNavigationPrompt(renderer, dpadIcon, chipX, chipY, "Move");
        }
        else if (state.screen == ScreenState::ReviewDiscs)
        {
            buttonPrompt(chipX, chipY, "A", "Toggle / open", COLOR_GREEN);
            chipX += 100;
            buttonPrompt(chipX, chipY, "B", "Back", COLOR_RED);
            chipX += 90;
            DrawNavigationPrompt(renderer, dpadIcon, chipX, chipY, "Move");
        }
        else if (state.screen == ScreenState::Importing)
        {
            buttonPrompt(chipX, chipY, "B", "Cancel", COLOR_RED);
        }
        else if (state.screen == ScreenState::Complete)
        {
            buttonPrompt(chipX, chipY, "A", "Finish", COLOR_GREEN);
        }
        else if (state.screen == ScreenState::Error)
        {
            buttonPrompt(chipX, chipY, "A", "Retry", COLOR_GREEN);
            chipX += 100;
            buttonPrompt(chipX, chipY, "B", "Exit", COLOR_RED);
        }

        // Row 2: Keyboard shortcuts hint line
        int kbX = 24;
        int kbY = footerY + 40;
        ui::DrawString(renderer, kbX, kbY, "KEYBOARD:", COLOR_GOLD_MUTED.r, COLOR_GOLD_MUTED.g, COLOR_GOLD_MUTED.b, 255, 0.85f);
        int kbTextX = kbX + ui::MeasureTextWidth("KEYBOARD: ", 0.85f);

        if (state.screen == ScreenState::BrowseSource || state.screen == ScreenState::BrowseDest)
        {
            ui::DrawString(renderer, kbTextX, kbY,
                           state.screen == ScreenState::BrowseDest
                               ? (state.destNaming ? "[Type] Rename  [Enter] Create  [Esc] Cancel"
                                                   : "[Enter] Open  [F] Select  [F2] New folder  [Esc] Back")
                               : "[Enter] Open  [F] Select  [Bksp] Up  [Esc] Back  [Arrows] Move",
                           COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.85f);
        }
        else if (state.screen == ScreenState::ReviewDiscs)
        {
            ui::DrawString(renderer, kbTextX, kbY,
                            "[Enter] Toggle / open  [Esc] Back  [Arrows] Move",
                           COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.85f);
        }
        else if (state.screen == ScreenState::Importing)
        {
            ui::DrawString(renderer, kbTextX, kbY,
                           "[Esc] Cancel",
                           COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.85f);
        }
        else if (state.screen == ScreenState::Complete)
        {
            ui::DrawString(renderer, kbTextX, kbY,
                           "[Enter] Finish  [Esc] Exit",
                           COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.85f);
        }
        else if (state.screen == ScreenState::Error)
        {
            ui::DrawString(renderer, kbTextX, kbY,
                           "[Enter] Retry  [Esc] Exit",
                           COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.85f);
        }

        // Body Content
        int bodyY = 80;
        int bodyH = footerY - bodyY - 10;

        if (state.screen == ScreenState::BrowseSource || state.screen == ScreenState::BrowseDest)
        {
            bool isSource = (state.screen == ScreenState::BrowseSource);
            const auto& currentPath = isSource ? state.currentSourceBrowse : state.currentDestBrowse;
            const auto& items = isSource ? state.sourceItems : state.destItems;
            int selIdx = isSource ? state.sourceSelectedIndex : state.destSelectedIndex;
            int scrollOff = isSource ? state.sourceScrollOffset : state.destScrollOffset;
            bool onRoots = isSource ? state.focusOnRoots : state.destFocusOnRoots;
            int rootIdx = isSource ? state.rootSelectedIndex : state.destRootSelectedIndex;

            // Left panel: Drives / Roots
            int rootsW = 160;
            DrawBevelPanel(renderer, 20, bodyY, rootsW, bodyH, COLOR_STEEL_PANEL);
            ui::DrawString(renderer, 32, bodyY + 14, "SYSTEM DRIVES", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 0.9f);
            SetDrawColor(renderer, COLOR_BORDER_LINE);
            SDL_RenderDrawLine(renderer, 24, bodyY + 34, 20 + rootsW - 4, bodyY + 34);

            int rootItemY = bodyY + 44;
            int rootItemH = 28;
            for (size_t i = 0; i < state.roots.size(); ++i)
            {
                bool isSel = (onRoots && static_cast<int>(i) == rootIdx);
                int rowY = rootItemY + static_cast<int>(i) * rootItemH;

                if (isSel)
                {
                    DrawSelectionBar(renderer, 24, rowY, rootsW - 8, rootItemH - 2);
                    // Perfectly centered text vertically inside the selection bar
                    int textY = rowY + (rootItemH - 2 - 16) / 2;
                    ui::DrawString(renderer, 36, textY, state.roots[i].name, COLOR_SEL_INK.r, COLOR_SEL_INK.g, COLOR_SEL_INK.b, 255, 1.0f);
                }
                else
                {
                    int textY = rowY + (rootItemH - 2 - 16) / 2;
                    ui::DrawString(renderer, 36, textY, state.roots[i].name, COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
                }
            }

            // Right panel: Directory Browser
            int mainX = 20 + rootsW + 14;
            int mainW = w - mainX - 20;
            DrawBevelPanel(renderer, mainX, bodyY, mainW, bodyH, COLOR_STEEL_PANEL);

            std::string headerTitle = isSource ? "SOURCE SELECTION (Extracted Folder / Discs)" : "DESTINATION DIRECTORY SELECTION";
            ui::DrawString(renderer, mainX + 16, bodyY + 14, headerTitle, COLOR_CYAN.r, COLOR_CYAN.g, COLOR_CYAN.b, 255, 0.95f);

                // Path bar well
                std::u8string u8p = currentPath.u8string();
                std::string pathStr(reinterpret_cast<const char*>(u8p.data()), u8p.size());
                FillRect(renderer, mainX + 14, bodyY + 36, mainW - 28, 28, COLOR_RAIL);
                DrawRect(renderer, mainX + 14, bodyY + 36, mainW - 28, 28, COLOR_BORDER_LINE);

                // Truncate path cleanly if needed using precise UTF-8 glyph measurement
                int availPathW = mainW - 48;
                std::string displayPath = ui::TruncateTextWidth(pathStr, availPathW, 0.95f, "...");
                ui::DrawString(renderer, mainX + 22, bodyY + 42, displayPath, COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 0.95f);

                // Item list
                int listY = bodyY + 76;
                int rowH = 28;
                int maxVisible = (bodyH - (isSource ? 90 : 180)) / rowH;

                for (int i = 0; i < maxVisible && (i + scrollOff) < static_cast<int>(items.size()); ++i)
                {
                    int itemIdx = i + scrollOff;
                    const auto& item = items[itemIdx];
                    int itemY = listY + i * rowH;

                    bool isSelected = (!onRoots && itemIdx == selIdx);
                    int textY = itemY + (rowH - 2 - 16) / 2;

                    // Available width for item text (leaving margin for icon and scroll space)
                    int availItemW = mainW - 75;
                    std::string displayName = ui::TruncateTextWidth(item.name, availItemW, 1.0f, "...");

                    if (isSelected)
                    {
                        DrawSelectionBar(renderer, mainX + 14, itemY, mainW - 28, rowH - 2);

                        if (item.isDirectory)
                        {
                            DrawFolderIcon(renderer, mainX + 24, textY + 1, true);
                        }
                        else
                        {
                            DrawFileIcon(renderer, mainX + 24, textY + 1, true);
                        }
                        ui::DrawString(renderer, mainX + 46, textY, displayName, COLOR_SEL_INK.r, COLOR_SEL_INK.g, COLOR_SEL_INK.b, 255, 1.0f);
                    }
                    else
                    {
                        if (item.isDirectory)
                        {
                            DrawFolderIcon(renderer, mainX + 24, textY + 1, false);
                            ui::DrawString(renderer, mainX + 46, textY, displayName, COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
                        }
                        else
                        {
                            DrawFileIcon(renderer, mainX + 24, textY + 1, false);
                            ui::DrawString(renderer, mainX + 46, textY, displayName, COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 1.0f);
                        }
                    }
                }

            if (items.empty())
            {
                ui::DrawString(renderer, mainX + 24, listY + 12, "(Directory is empty)", COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 1.0f);
            }
            if (!isSource)
            {
                const int actionY = bodyY + bodyH - 44;
                DrawBevelPanel(renderer, mainX + 14, actionY, 216, 30, COLOR_RAIL);
                ui::DrawString(renderer, mainX + 24, actionY + 7,
                               state.destNaming ? "CREATE FOLDER" : playStation ? "NEW FOLDER  [F2]" : "NEW FOLDER  [F2 / Y]",
                               COLOR_CYAN.r, COLOR_CYAN.g, COLOR_CYAN.b, 255, 0.9f);
                if (state.destNaming)
                {
                    DrawBevelPanel(renderer, mainX + 244, actionY, 146, 30, COLOR_RAIL);
                    ui::DrawString(renderer, mainX + 254, actionY + 7, playStation ? "CANCEL" : "CANCEL  [B]",
                                   COLOR_RED.r, COLOR_RED.g, COLOR_RED.b, 255, 0.9f);
                }
                if (state.destNaming)
                {
                    const auto name = "Name: " + state.destNewName +
                        (state.destNameReplaceOnType ? " (type to rename)" : "");
                    ui::DrawString(renderer, mainX + 16, actionY - 55,
                                   ui::TruncateTextWidth(name, mainW - 32, 0.9f),
                                   COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 0.9f);
                }
                if (!state.destStatus.empty())
                {
                    const auto color = state.destStatusError ? COLOR_RED : COLOR_INK;
                    ui::DrawString(renderer, mainX + 16, actionY - 28,
                                   ui::TruncateTextWidth(state.destStatus, mainW - 32, 0.9f),
                                   color.r, color.g, color.b, 255, 0.9f);
                }
            }
        }
        else if (state.screen == ScreenState::ReviewDiscs)
        {
            // Review Discs Screen
            int cardW = w - 40;
            DrawBevelPanel(renderer, 20, bodyY, cardW, bodyH, COLOR_STEEL_PANEL);

            ui::DrawString(renderer, 40, bodyY + 20, "SCANNED CONTENT REVIEW", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 1.25f);

            const auto sourceUtf8 = state.selectedSource.u8string();
            std::string sourceInfo = ui::TruncateTextWidth("Source: " + std::string(sourceUtf8.begin(), sourceUtf8.end()), cardW - 40, 0.95f);
            ui::DrawString(renderer, 40, bodyY + 54, sourceInfo, COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.95f);

            const auto destUtf8 = state.selectedDest.u8string();
            std::string destInfo = ui::TruncateTextWidth("Destination: " + std::string(destUtf8.begin(), destUtf8.end()), cardW - 40, 0.95f);
            ui::DrawString(renderer, 40, bodyY + 76, destInfo, COLOR_CYAN.r, COLOR_CYAN.g, COLOR_CYAN.b, 255, 0.95f);

            if (state.isScanning.load())
            {
                ui::DrawString(renderer, 40, bodyY + 120, "Scanning files and calculating hashes... Please wait.", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 1.0f);
            }
            else if (!state.scanError.empty())
            {
                ui::DrawString(renderer, 40, bodyY + 120, "Scan Error: " + state.scanError, COLOR_RED.r, COLOR_RED.g, COLOR_RED.b, 255, 1.0f);
            }
            else
            {
                int tableY = bodyY + 110;
                FillRect(renderer, 40, tableY, cardW - 80, 26, COLOR_RAIL);
                DrawRect(renderer, 40, tableY, cardW - 80, 26, COLOR_BORDER_LINE);
                ui::DrawString(renderer, 50, tableY + 5, "DISC / ITEM", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 0.9f);
                ui::DrawString(renderer, 220, tableY + 5, "EDITION", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 0.9f);
                ui::DrawString(renderer, 400, tableY + 5, "FILES", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 0.9f);
                ui::DrawString(renderer, 500, tableY + 5, "SIZE", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 0.9f);
                ui::DrawString(renderer, 650, tableY + 5, "STATUS / HASH", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 0.9f);

                const int firstRowY = tableY + 32;
                for (int visibleRow = 0; visibleRow < REVIEW_VISIBLE_ITEMS; ++visibleRow)
                {
                    const int item = state.reviewScrollOffset + visibleRow;
                    if (item >= ReviewActionStart(state.scanResult)) break;
                    const int rowY = firstRowY + visibleRow * 28;
                    const bool checked = state.selected[item];
                    const bool focused = state.reviewSelectedIndex == item;
                    if (focused) DrawSelectionBar(renderer, 44, rowY - 4, cardW - 48, 26);
                    const auto primary = focused ? COLOR_SEL_INK : checked ? COLOR_INK : COLOR_MUTED;
                    const auto secondary = focused ? COLOR_SEL_INK : checked ? COLOR_CYAN : COLOR_MUTED;
                    const auto status = focused ? COLOR_SEL_INK : checked ? COLOR_GREEN : COLOR_MUTED;
                    ui::DrawString(renderer, 50, rowY, checked ? "[x]" : "[ ]", primary.r, primary.g, primary.b, 255, 1.0f);
                    if (item < int(state.scanResult.discs.size()))
                    {
                        const auto& d = state.scanResult.discs[item];
                        ui::DrawString(renderer, 86, rowY, "Disc " + std::to_string(d.disc) + " of " + std::to_string(d.discs), primary.r, primary.g, primary.b, 255, 1.0f);
                        ui::DrawString(renderer, 220, rowY, d.edition, secondary.r, secondary.g, secondary.b, 255, 1.0f);
                        ui::DrawString(renderer, 400, rowY, std::to_string(d.files), primary.r, primary.g, primary.b, 255, 1.0f);
                        ui::DrawString(renderer, 500, rowY, FormatBytes(d.bytes), primary.r, primary.g, primary.b, 255, 1.0f);
                        ui::DrawString(renderer, 650, rowY, d.identity.empty() ? "Verified" : d.identity, status.r, status.g, status.b, 255, 1.0f);
                    }
                    else
                    {
                        const auto& package = state.scanResult.packages[item - int(state.scanResult.discs.size())];
                        ui::DrawString(renderer, 86, rowY, "DLC", primary.r, primary.g, primary.b, 255, 1.0f);
                        ui::DrawString(renderer, 220, rowY, ui::TruncateTextWidth(package.displayName, 168, 1.0f), secondary.r, secondary.g, secondary.b, 255, 1.0f);
                        ui::DrawString(renderer, 400, rowY, std::to_string(package.files), primary.r, primary.g, primary.b, 255, 1.0f);
                        ui::DrawString(renderer, 500, rowY, FormatBytes(package.bytes), primary.r, primary.g, primary.b, 255, 1.0f);
                        ui::DrawString(renderer, 650, rowY,
                                       ui::TruncateTextWidth(package.contentId, cardW - 670, 1.0f),
                                       status.r, status.g, status.b, 255, 1.0f);
                    }
                }

                if (state.scanResult.discs.empty() && state.scanResult.packages.empty())
                {
                    ui::DrawString(renderer, 50, firstRowY, "No valid Lost Odyssey discs or XEX files found in this folder.", COLOR_RED.r, COLOR_RED.g, COLOR_RED.b, 255, 1.0f);
                    ui::DrawString(renderer, 50, firstRowY + 22, "Make sure the folder contains disc1..disc4 or default.xex.", COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 1.0f);
                }
                if (state.reviewScrollOffset > 0)
                    ui::DrawString(renderer, 1225, tableY + 5, "^", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 0.9f);
                if (ReviewActionStart(state.scanResult) > state.reviewScrollOffset + REVIEW_VISIBLE_ITEMS)
                    ui::DrawString(renderer, 1225, firstRowY + (REVIEW_VISIBLE_ITEMS - 1) * 28, "v", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 0.9f);

                // Action Buttons
                int btnY = bodyY + bodyH - 65;
                const auto hint = ReviewReplaceHint(uiLanguage);
                ui::DrawString(renderer, 40, btnY - 33,
                               ui::TruncateTextWidth(hint, cardW - 80, 0.88f),
                               COLOR_GOLD_MUTED.r, COLOR_GOLD_MUTED.g, COLOR_GOLD_MUTED.b, 255, 0.88f);
                int btnW = 230;
                int btnH = 38;
                int actionStart = ReviewActionStart(state.scanResult);

                // Button 1: Start Import
                bool b1Sel = (state.reviewSelectedIndex == actionStart);
                if (b1Sel && state.CanImport())
                {
                    DrawSelectionBar(renderer, 40, btnY, btnW, btnH);
                    ui::DrawString(renderer, 75, btnY + 10, "START IMPORT", COLOR_SEL_INK.r, COLOR_SEL_INK.g, COLOR_SEL_INK.b, 255, 1.1f);
                }
                else
                {
                    DrawBevelPanel(renderer, 40, btnY, btnW, btnH, COLOR_RAIL);
                    const auto ink = state.CanImport() ? COLOR_INK : COLOR_MUTED;
                    ui::DrawString(renderer, 75, btnY + 10, "START IMPORT", ink.r, ink.g, ink.b, 255, 1.1f);
                }

                // Button 2: Change Destination
                bool b2Sel = (state.reviewSelectedIndex == actionStart + 1);
                if (b2Sel)
                {
                    DrawSelectionBar(renderer, 40 + btnW + 20, btnY, btnW, btnH);
                    ui::DrawString(renderer, 40 + btnW + 45, btnY + 10, "CHANGE DEST", COLOR_SEL_INK.r, COLOR_SEL_INK.g, COLOR_SEL_INK.b, 255, 1.1f);
                }
                else
                {
                    DrawBevelPanel(renderer, 40 + btnW + 20, btnY, btnW, btnH, COLOR_RAIL);
                    ui::DrawString(renderer, 40 + btnW + 45, btnY + 10, "CHANGE DEST", COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.1f);
                }

                // Button 3: Rescan / Change Source
                bool b3Sel = (state.reviewSelectedIndex == actionStart + 2);
                if (b3Sel)
                {
                    DrawSelectionBar(renderer, 40 + (btnW + 20) * 2, btnY, btnW, btnH);
                    ui::DrawString(renderer, 40 + (btnW + 20) * 2 + 40, btnY + 10, "CHANGE SOURCE", COLOR_SEL_INK.r, COLOR_SEL_INK.g, COLOR_SEL_INK.b, 255, 1.1f);
                }
                else
                {
                    DrawBevelPanel(renderer, 40 + (btnW + 20) * 2, btnY, btnW, btnH, COLOR_RAIL);
                    ui::DrawString(renderer, 40 + (btnW + 20) * 2 + 40, btnY + 10, "CHANGE SOURCE", COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.1f);
                }
            }
        }
        else if (state.screen == ScreenState::Importing)
        {
            int cardW = w - 40;
            DrawBevelPanel(renderer, 20, bodyY, cardW, bodyH, COLOR_STEEL_PANEL);

            ui::DrawString(renderer, 40, bodyY + 30, "IMPORTING GAME CONTENT...", COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 1.25f);

            uint64_t done = state.progressDone.load();
            uint64_t total = state.progressTotal.load();
            double pct = total > 0 ? std::clamp(static_cast<double>(done) / total, 0.0, 1.0) : 0.0;

            std::string label;
            {
                std::lock_guard<std::mutex> lock(state.progressMutex);
                label = state.progressLabel;
            }

            ui::DrawString(renderer, 40, bodyY + 75, label, COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.95f);

            int barY = bodyY + 105;
            int barW = cardW - 80;
            int barH = 30;
            FillRect(renderer, 40, barY, barW, barH, COLOR_RAIL);
            DrawRect(renderer, 40, barY, barW, barH, COLOR_BORDER_LINE);

            int fillW = static_cast<int>(barW * pct);
            if (fillW > 0)
            {
                FillRect(renderer, 42, barY + 2, fillW - 4, barH - 4, COLOR_CYAN);
            }

            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1) << (pct * 100.0) << "% (" << FormatBytes(done) << " / " << FormatBytes(total) << ")";
            std::string pctStr = ss.str();
            ui::DrawString(renderer, 40, barY + 40, pctStr, COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);

            ui::DrawString(renderer, 40, bodyY + 195, "Files are copied safely to staging and verified before final publish.", COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.95f);
            ui::DrawString(renderer, 40, bodyY + 220, "Original source disc files remain untouched.", COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.95f);
        }
        else if (state.screen == ScreenState::Complete)
        {
            int cardW = w - 40;
            DrawBevelPanel(renderer, 20, bodyY, cardW, bodyH, COLOR_STEEL_PANEL);

            ui::DrawString(renderer, 40, bodyY + 40, "IMPORT COMPLETED SUCCESSFULLY!", COLOR_GREEN.r, COLOR_GREEN.g, COLOR_GREEN.b, 255, 1.25f);
            ui::DrawString(renderer, 40, bodyY + 80, "Game files were installed to:", COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
            ui::DrawString(renderer, 40, bodyY + 105,
                           ui::TruncateTextWidth(state.installResult.destination, cardW - 80, 1.0f),
                           COLOR_CYAN.r, COLOR_CYAN.g, COLOR_CYAN.b, 255, 1.0f);

            std::string summary;
            if (!state.installedDiscs.empty())
            {
                summary = "Discs imported: ";
                for (int discNum : state.installedDiscs)
                    summary += "Disc " + std::to_string(discNum) + " ";
                if (!state.installResult.dlcImported.empty())
                    summary += "  /  DLC imported: " + std::to_string(state.installResult.dlcImported.size());
            }
            else
                summary = "DLC imported: " + std::to_string(state.installResult.dlcImported.size()) +
                          "  /  Already current: " + std::to_string(state.installResult.dlcUnchanged.size());
            ui::DrawString(renderer, 40, bodyY + 140, summary, COLOR_ACCENT_GOLD.r, COLOR_ACCENT_GOLD.g, COLOR_ACCENT_GOLD.b, 255, 1.0f);
            const std::string pathStatus = !state.installResult.warning.empty()
                ? state.installResult.warning
                : ShouldPersistGamePath(state.installResult)
                    ? "Game path updated after importing the selected discs."
                    : "Game path unchanged; selected DLC was imported.";
            ui::DrawString(renderer, 40, bodyY + 170,
                           ui::TruncateTextWidth(pathStatus, cardW - 80, 0.95f),
                           COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.95f);
            if (playStation)
            {
                ui::DrawString(renderer, 40, bodyY + 222, "Press", COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
                buttonPrompt(94, bodyY + 219, "A", "or [Enter] to close importer.", COLOR_GREEN);
            }
            else ui::DrawString(renderer, 40, bodyY + 220, "Press [A] or [Enter] to close importer.", COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
        }
        else if (state.screen == ScreenState::Error)
        {
            int cardW = w - 40;
            DrawBevelPanel(renderer, 20, bodyY, cardW, bodyH, COLOR_STEEL_PANEL);

            ui::DrawString(renderer, 40, bodyY + 40, "IMPORT ENCOUNTERED AN ERROR", COLOR_RED.r, COLOR_RED.g, COLOR_RED.b, 255, 1.25f);
            ui::DrawString(renderer, 40, bodyY + 90, "Details:", COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
            ui::DrawString(renderer, 40, bodyY + 115, state.importError, COLOR_RED.r, COLOR_RED.g, COLOR_RED.b, 255, 1.0f);

            ui::DrawString(renderer, 40, bodyY + 160, "Check the details above. Original source files were kept safe.", COLOR_MUTED.r, COLOR_MUTED.g, COLOR_MUTED.b, 255, 0.95f);
            if (playStation)
            {
                ui::DrawString(renderer, 40, bodyY + 202, "Press", COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
                buttonPrompt(94, bodyY + 199, "B", "or [Enter] to return and retry.", COLOR_RED);
            }
            else ui::DrawString(renderer, 40, bodyY + 200, "Press [B] or [Enter] to return and retry.", COLOR_INK.r, COLOR_INK.g, COLOR_INK.b, 255, 1.0f);
        }

#ifdef LO_INSTALLER_UI_TESTING
        if (const char* preview = SDL_getenv("LO_IMPORTER_PREVIEW_BMP"))
        {
            if (auto* surface = SDL_CreateRGBSurfaceWithFormat(0, LOGICAL_WIN_WIDTH, LOGICAL_WIN_HEIGHT,
                                                                32, SDL_PIXELFORMAT_ARGB8888))
            {
                if (SDL_RenderReadPixels(renderer, nullptr, surface->format->format,
                                         surface->pixels, surface->pitch) == 0)
                    SDL_SaveBMP(surface, preview);
                SDL_FreeSurface(surface);
            }
            state.quit = state.userCancelled = true;
        }
#endif
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // A close request can race the worker's last event. A committed import
    // remains successful even when the window was closed a frame earlier.
    JoinWorkerAndConsume(workerThread, state.cancelRequested, consumeWorkerEvents);

    for (auto* pad : controllers)
    {
        SDL_GameControllerClose(pad);
    }

    SDL_DestroyTexture(dpadIcon);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    result.success = state.installSuccess;
    result.cancelled = state.userCancelled && !state.installSuccess;
    result.destination = state.installSuccess ? std::filesystem::path(state.installResult.destination) : state.selectedDest;
    result.error = state.importError;

    return result;
}

} // namespace install
