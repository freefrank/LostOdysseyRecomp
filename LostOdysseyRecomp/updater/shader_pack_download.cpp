#include "shader_pack_download.h"
#include "shader_pack_index.h"
#include "http.h"

#include "../gpu/shader/portable_shader_pack_location.h"

#include <SDL3/SDL.h>
#include <hid/face_buttons.h>
#include <host_ui/rasterizer.h>
#if defined(__ANDROID__)
#include <hid/android_touch.h>
#endif
#include <host_ui/widgets.h>
#include <os/stale_files.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace updater::shader_pack
{
namespace
{
namespace pack = xenos::portable_pack;

std::string PathUtf8(const std::filesystem::path &path)
{
    const auto value = path.u8string();
    return std::string(reinterpret_cast<const char *>(value.data()), value.size());
}

unsigned long ProcessId()
{
#ifdef _WIN32
    return static_cast<unsigned long>(_getpid());
#else
    return static_cast<unsigned long>(getpid());
#endif
}

std::optional<pack::Flavor> ExpectedFlavor(gpu::backend::Backend configured)
{
#if defined(_WIN32)
    // The renderer tries the requested backend first; D3D11 runs as D3D12.
    const auto requested = gpu::backend::Requested(configured, std::getenv("LO_GRAPHICS_API"));
    if (!requested) return std::nullopt;
    return *requested == gpu::backend::Backend::Vulkan || *requested == gpu::backend::Backend::Metal
        ? pack::Flavor::Vulkan : pack::Flavor::D3D12;
#else
    // Linux, Android and macOS (Metal reads the Vulkan SPIR-V).
    (void)configured;
    return pack::Flavor::Vulkan;
#endif
}

std::string Mebibytes(uint64_t bytes)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.1f", double(bytes) / (1024.0 * 1024.0));
    return text;
}

enum class Phase { Offer, Downloading, Verifying, Failed, Finished };

struct Session
{
    std::mutex mutex;
    Phase phase = Phase::Offer;
    int selected = 0;
    bool chinese = false;
    bool accepted = false;
    bool skipped = false;
    bool cancelled = false;
    std::string failure;
    std::string label;
    uint64_t total = 0;
    std::atomic<uint64_t> completed{0};
    std::atomic<bool> cancel{false};
};

struct Job
{
    pack::Flavor flavor = pack::Flavor::Vulkan;
    pack::Digest contract{};
    IndexEntry entry;
    std::string url;
    std::filesystem::path directory;
};

struct InstallResult
{
    bool cancelled = false;
    std::string failure; // empty on success
};

InstallResult Install(const Job &job, Session &session)
{
    std::error_code error;
    std::filesystem::create_directories(job.directory, error);
    if (error) return {false, "could not create " + PathUtf8(job.directory) + ": " + error.message()};
    const auto space = std::filesystem::space(job.directory, error);
    if (!error && space.available < job.entry.size + (64ull << 20))
        return {false, "not enough free disk space (" + Mebibytes(job.entry.size + (64ull << 20)) + " MiB needed)"};
    const auto target = job.directory / pack::FileNameOf(job.flavor);
    auto temp = target;
    temp += ".download-" + std::to_string(ProcessId());
    struct Cleanup
    {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ignored; if (!path.empty()) std::filesystem::remove(path, ignored); }
    } cleanup{temp};

    std::string failure;
    bool cancelled = false, oversize = false;
    const bool downloaded = DownloadFile(job.url, temp, job.entry.size, [&](uint64_t completed, uint64_t) {
        session.completed = completed;
        oversize = completed > job.entry.size;
        return !oversize && !session.cancel.load();
    }, failure, cancelled);
    if (oversize) return {false, "the server sent more than the " + std::to_string(job.entry.size) + " bytes the index lists"};
    if (!downloaded) return {cancelled, cancelled ? std::string("cancelled") : failure};
    {
        std::lock_guard lock(session.mutex);
        session.phase = Phase::Verifying;
    }
    const auto size = std::filesystem::file_size(temp, error);
    if (error || size != job.entry.size)
        return {false, "downloaded " + std::to_string(error ? 0 : size) + " bytes, the index lists " + std::to_string(job.entry.size)};
    {
        std::ifstream input(temp, std::ios::binary);
        xenos::resources::Sha256Incremental hash;
        std::vector<char> buffer(1u << 20);
        while (input)
        {
            input.read(buffer.data(), std::streamsize(buffer.size()));
            if (const auto count = input.gcount(); count > 0)
                hash.Update({reinterpret_cast<const uint8_t *>(buffer.data()), size_t(count)});
        }
        if (!input.eof()) return {false, "could not read the downloaded pack"};
        if (xenos::resources::Sha256Hex(hash.Finalize()) != job.entry.sha256)
            return {false, "the downloaded pack does not match the published SHA-256"};
    }
    try
    {
        pack::Reader reader(temp, job.contract, pack::FormatOf(job.flavor));
        reader.VerifyAll();
    }
    catch (const std::exception &exception)
    {
        return {false, std::string("the downloaded pack was rejected: ") + exception.what()};
    }
    std::filesystem::rename(temp, target, error);
    if (error) return {false, "could not install " + PathUtf8(target) + ": " + error.message()};
    cleanup.path.clear();
    return {};
}

// A window like the update prompt's. The renderer has not started yet, so SDL
// is initialized here and shut down again before video::Init.
class PromptWindow
{
public:
    bool Open()
    {
        const char *driver = std::getenv("SDL_VIDEODRIVER");
        if (driver && std::string_view(driver) == "dummy") return false;
        constexpr uint32_t subsystems = SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS;
        if (!SDL_InitSubSystem(subsystems)) return false;
        initialized_ = subsystems;
        if (const char *current = SDL_GetCurrentVideoDriver(); current && std::string_view(current) == "dummy")
            return false;
        window_ = SDL_CreateWindow("Lost Odyssey Recomp", 1280, 720,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!window_) return false;
        SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (!renderer_) renderer_ = SDL_CreateRenderer(window_, "software");
        if (renderer_) SDL_SetRenderVSync(renderer_, 1);
        // SDL2 sampled textures with nearest filtering by default; SDL3 smooths them.
        if (renderer_) SDL_SetDefaultTextureScaleMode(renderer_, SDL_SCALEMODE_NEAREST);
        texture_ = renderer_ ? SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 1280, 720)
                             : nullptr;
        if (!texture_) return false;
        SDL_SetRenderLogicalPresentation(renderer_, 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        int count = 0;
        if (SDL_JoystickID *ids = SDL_GetGamepads(&count))
        {
            for (int i = 0; i < count; ++i)
                if (auto *controller = SDL_OpenGamepad(ids[i])) controllers_.push_back(controller);
            SDL_free(ids);
        }
        pixels_.Resize(1280, 720);
        return true;
    }
    ~PromptWindow()
    {
        for (auto *controller : controllers_) SDL_CloseGamepad(controller);
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        if (initialized_) SDL_QuitSubSystem(initialized_);
        if (initialized_ && !SDL_WasInit(0)) SDL_Quit();
    }
    uint32_t Id() const { return SDL_GetWindowID(window_); }
    void Size(int &width, int &height) const { SDL_GetWindowSize(window_, &width, &height); }
    host_ui::PixelBuffer &Pixels() { return pixels_; }
    void Present()
    {
        SDL_UpdateTexture(texture_, nullptr, pixels_.pixels.data(), 1280 * sizeof(uint32_t));
        SDL_RenderClear(renderer_);
        SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }

private:
    uint32_t initialized_ = 0;
    SDL_Window *window_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
    std::vector<SDL_Gamepad *> controllers_;
    host_ui::PixelBuffer pixels_;
};

enum class Action { None, Activate, Primary, Secondary, Left, Right, Close };

Action ReadAction(const SDL_Event &event, uint32_t windowId, int width, int height)
{
    if (event.type == SDL_EVENT_QUIT ||
        (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == windowId))
        return Action::Close;
    // This is the process's only window, so a key event without a focused window counts too.
    if (event.type == SDL_EVENT_KEY_DOWN && (event.key.windowID == windowId || !event.key.windowID) && !event.key.repeat)
    {
        switch (event.key.key)
        {
        case SDLK_LEFT: return Action::Left;
        case SDLK_RIGHT: return Action::Right;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE: return Action::Activate;
        case SDLK_ESCAPE: return Action::Secondary;
        default: return Action::None;
        }
    }
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
    {
        switch (hid::face_buttons::FromEvent(event.gbutton))
        {
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return Action::Left;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return Action::Right;
        case SDL_GAMEPAD_BUTTON_SOUTH: return Action::Activate;
        case SDL_GAMEPAD_BUTTON_EAST: return Action::Secondary;
        default: return Action::None;
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.windowID == windowId)
    {
        const double scale = std::min(width / 1280.0, height / 720.0);
        if (scale <= 0) return Action::None;
        const int x = int((event.button.x - (width - 1280 * scale) / 2) / scale);
        const int y = int((event.button.y - (height - 720 * scale) / 2) / scale);
        if (y >= 618 && y < 664)
        {
            if (x >= 866 && x < 1006) return Action::Primary;
            if (x >= 1022 && x < 1162) return Action::Secondary;
        }
    }
    return Action::None;
}

#if defined(__ANDROID__)
// The on-screen controller covers the window and feeds hid, not SDL events:
// its newly pressed A, B and D-pad left/right act like a controller's.
Action TouchAction(uint16_t &previous)
{
    const uint16_t buttons = hid::android_touch::Snapshot().buttons;
    const uint16_t pressed = buttons & ~previous;
    previous = buttons;
    if (pressed & 0x1000) return Action::Activate; // A
    if (pressed & 0x2000) return Action::Secondary; // B
    if (pressed & 0x0004) return Action::Left;
    if (pressed & 0x0008) return Action::Right;
    return Action::None;
}
#endif

void Apply(Session &session, Action action)
{
    std::lock_guard lock(session.mutex);
    switch (session.phase)
    {
    case Phase::Offer:
        if (action == Action::Left) session.selected = 0;
        else if (action == Action::Right) session.selected = 1;
        else if (action == Action::Primary || (action == Action::Activate && session.selected == 0))
        {
            session.accepted = true;
            session.phase = Phase::Downloading;
        }
        else if (action == Action::Secondary || action == Action::Activate)
        {
            session.skipped = true;
            session.phase = Phase::Finished;
        }
        else if (action == Action::Close) session.phase = Phase::Finished;
        break;
    case Phase::Downloading:
        if (action == Action::Secondary || action == Action::Close) session.cancel = true;
        break;
    case Phase::Failed:
        if (action != Action::None && action != Action::Left && action != Action::Right)
            session.phase = Phase::Finished;
        break;
    default:
        break;
    }
}

int DrawWrapped(host_ui::Rasterizer &r, int x, int y, int width, std::string_view text, uint32_t color)
{
    std::string line;
    for (size_t offset = 0; offset < text.size();)
    {
        const size_t start = offset;
        if (!host_ui::font::DecodeUtf8(text, offset)) break;
        std::string glyph(text.substr(start, offset - start));
        if (!line.empty() && r.MeasureString(line + glyph) > width)
        {
            r.DrawString(x, y, line, color);
            y += 22;
            line.clear();
            if (glyph == " ") continue;
        }
        line += glyph;
    }
    if (!line.empty())
    {
        r.DrawString(x, y, line, color);
        y += 22;
    }
    return y;
}

void Render(Session &session, host_ui::Rasterizer &r)
{
    std::lock_guard lock(session.mutex);
    const bool zh = session.chinese;
    const auto white = host_ui::MakeColor(255, 230, 235, 240);
    const auto muted = host_ui::MakeColor(255, 160, 173, 187);
    r.FillRect(0, 0, 1280, 720, host_ui::MakeColor(255, 15, 20, 29));
    host_ui::DrawPanel(r, 80, 50, 1120, 620);
    host_ui::DrawHeader(r, 80, 50, 1120, 62, zh ? L"着色器包" : L"Shader bundle");
    const auto progressBar = [&](uint64_t completed) {
        r.DrawRect(112, 220, 1056, 28, host_ui::MakeColor(255, 65, 75, 88));
        const double fraction = session.total ? std::min(1.0, double(completed) / double(session.total)) : 0.0;
        r.FillRect(114, 222, int(1052 * fraction), 24, host_ui::MakeColor(255, 196, 160, 82));
        const auto amount = Mebibytes(completed) + " / " + Mebibytes(session.total) + " MiB   " +
                            std::to_string(int(fraction * 100)) + "%";
        r.DrawString(112, 262, amount, muted);
    };
    switch (session.phase)
    {
    case Phase::Offer:
    {
        r.DrawString(112, 140, zh ? "没有找到 " + session.label + " 渲染器的预编译着色器。"
                                  : "No precompiled shaders were found for the " + session.label + " renderer.", white, 1.2f);
        r.DrawString(112, 184, zh ? "现在下载着色器包吗？（" + Mebibytes(session.total) + " MiB）"
                                  : "Download the shader bundle now? (" + Mebibytes(session.total) + " MiB)", white, 1.2f);
        int y = DrawWrapped(r, 112, 250, 1056, zh ? "不下载的话，游戏会先在本机编译全部着色器，可能需要几分钟。"
            : "Without it, the game first compiles all of its shaders on this device, which can take several minutes.", muted);
        DrawWrapped(r, 112, y + 8, 1056, zh ? "选择跳过后，着色器更新之前不会再询问。"
            : "If you skip, you will not be asked again until the shaders change.", muted);
        host_ui::DrawButton(r, 866, 618, 140, 46, zh ? L"下载 (A)" : L"Download (A)", session.selected == 0);
        host_ui::DrawButton(r, 1022, 618, 140, 46, zh ? L"跳过 (B)" : L"Skip (B)", session.selected == 1);
        break;
    }
    case Phase::Downloading:
        r.DrawString(112, 140, zh ? "正在下载 " + session.label + " 着色器包……"
                                  : "Downloading the " + session.label + " shader bundle...", white, 1.2f);
        progressBar(session.completed.load());
        host_ui::DrawButton(r, 1022, 618, 140, 46, zh ? L"取消 (B)" : L"Cancel (B)", true);
        break;
    case Phase::Verifying:
        r.DrawString(112, 140, zh ? "正在校验着色器包……" : "Verifying the shader bundle...", white, 1.2f);
        progressBar(session.total);
        break;
    case Phase::Failed:
    {
        r.DrawString(112, 140, zh ? "着色器包没有装好。" : "The shader bundle could not be installed.", white, 1.2f);
        const int y = DrawWrapped(r, 112, 196, 1056, session.failure, muted);
        DrawWrapped(r, 112, y + 8, 1056, zh ? "游戏会改为在本机编译着色器。"
            : "The game will compile its shaders on this device instead.", muted);
        host_ui::DrawButton(r, 1022, 618, 140, 46, zh ? L"继续 (A)" : L"Continue (A)", true);
        break;
    }
    case Phase::Finished:
        break;
    }
}
} // namespace

std::string PrepareAtStartup(const StartupRequest &request)
{
    const char *mode = std::getenv("LO_SHADER_PACK_DOWNLOAD");
    const std::string_view choice = mode ? mode : "";
    if (choice == "0") return "check disabled by LO_SHADER_PACK_DOWNLOAD=0";
    if (std::getenv("LO_HEADLESS")) return "check skipped: headless";
#if LO_PLATFORM_SWITCH
    // No network download or SDL window on the console: the Vulkan pack is
    // copied to the SD card (docs/SWITCH.md) and the renderer finds it there.
    (void)request;
    return "check skipped: Switch (copy portable_vk.lospv to the shaders folder)";
#endif
    if (pack::DistributionPacksDisabled() || pack::ConfiguredPackPath())
        return "check skipped: developer shader settings";
    const auto flavor = ExpectedFlavor(request.configuredBackend);
    if (!flavor) return "check skipped: invalid renderer request";
    // A download interrupted by a killed process leaves "<pack>.download-<pid>".
    for (const auto each : {pack::Flavor::Vulkan, pack::Flavor::D3D12})
        os::RemoveStaleSiblings(pack::InstallDirectory() / pack::FileNameOf(each), L".download-");
    const auto name = std::string(pack::FlavorName(*flavor));
    pack::Digest contract{};
    try
    {
        contract = pack::FlavorContract(request.unboundXex, *flavor);
    }
    catch (const std::exception &exception)
    {
        return std::string("check failed: ") + exception.what();
    }
    const auto contractHex = xenos::resources::Sha256Hex(contract);
    const auto shortContract = contractHex.substr(0, 16);
    for (const auto &path : pack::CandidatePaths(*flavor))
    {
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error)) continue;
        try
        {
            pack::Reader reader(path, contract, pack::FormatOf(*flavor));
            return name + " pack present: " + PathUtf8(path);
        }
        catch (const std::exception &)
        {
            // A pack for another contract: offer the current one.
        }
    }

    const bool automatic = choice == "1";
    if (!automatic && std::getenv("LO_BACKGROUND")) return name + " pack missing; not offered in a background run";
    const auto directory = pack::InstallDirectory();
    const auto declineRecord = directory / "declined-downloads.txt";
    if (!automatic && Declined(declineRecord, contractHex))
        return name + " pack missing; the download was skipped earlier for contract " + shortContract;

    const auto indexUrl = IndexUrl(std::getenv("LO_SHADER_PACK_INDEX_URL"));
    std::string body, error;
    if (!ReadResponse(indexUrl, MaxIndexBytes, body, error))
        return name + " pack missing; index unavailable: " + error;
    const auto entries = ParseIndex(body, error);
    if (!entries) return name + " pack missing; " + error;
    const auto *entry = Select(*entries, name, contractHex);
    if (!entry) return name + " pack missing; none published for contract " + shortContract;

    Job job;
    job.flavor = *flavor;
    job.contract = contract;
    job.entry = *entry;
    job.url = AssetUrl(indexUrl, entry->file);
    job.directory = directory;
    Session session;
    session.chinese = request.uiLanguage == 4;
    session.label = std::string(pack::FlavorLabel(*flavor));
    session.total = entry->size;
    InstallResult result;
    std::thread worker;
    const auto start = [&] {
        worker = std::thread([&] {
            auto outcome = Install(job, session);
            std::lock_guard lock(session.mutex);
            session.cancelled = outcome.cancelled;
            session.failure = outcome.failure;
            session.phase = outcome.failure.empty() || outcome.cancelled || automatic ? Phase::Finished : Phase::Failed;
            result = std::move(outcome);
        });
    };

    PromptWindow window;
    const bool visible = !std::getenv("LO_BACKGROUND") && window.Open();
    if (!visible && !automatic) return name + " pack missing; no window to offer the download";
    if (automatic)
    {
        session.accepted = true;
        session.phase = Phase::Downloading;
        start();
    }
    if (visible)
    {
        host_ui::Rasterizer rasterizer(window.Pixels());
        bool started = automatic;
#if defined(__ANDROID__)
        // Ignore a button still held from before the window opened.
        uint16_t touchButtons = hid::android_touch::Snapshot().buttons;
#endif
        while (true)
        {
            SDL_Event event;
            if (SDL_WaitEventTimeout(&event, 16))
            {
                int width = 0, height = 0;
                window.Size(width, height);
                do Apply(session, ReadAction(event, window.Id(), width, height));
                while (SDL_PollEvent(&event));
            }
#if defined(__ANDROID__)
            Apply(session, TouchAction(touchButtons));
#endif
            bool begin = false;
            {
                std::lock_guard lock(session.mutex);
                if (session.phase == Phase::Finished) break;
                begin = session.accepted && !started;
            }
            if (begin)
            {
                started = true;
                start();
            }
            Render(session, rasterizer);
            window.Present();
        }
    }
    if (worker.joinable()) worker.join();

    if (session.skipped)
    {
        std::string recordError;
        if (!RecordDecline(declineRecord, contractHex, recordError))
            return name + " pack missing; skipped by the player (" + recordError + ")";
        return name + " pack missing; skipped by the player for contract " + shortContract;
    }
    if (!session.accepted) return name + " pack missing; the offer was closed";
    if (result.cancelled) return name + " pack missing; download cancelled";
    if (!result.failure.empty()) return name + " pack missing; download failed: " + result.failure;
    return name + " pack installed: " + PathUtf8(directory / pack::FileNameOf(*flavor)) + " (" +
           std::to_string(entry->size) + " bytes, contract " + shortContract + ")";
}
}
