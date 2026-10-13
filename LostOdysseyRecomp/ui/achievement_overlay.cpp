#include <stdafx.h>
#include "achievement_overlay.h"
#include "achievement_toast.h"
#include <achievements/achievements.h>
#include <apu/audio.h>
#include <host_ui/unifont.h>
#include <kernel/io/file_system.h>
#include <os/logger.h>
#include <os/user_paths.h>
#include <settings/config.h>
#include <settings/menu_assets.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>

namespace ui::achievement_overlay {
namespace {
namespace menu_assets = settings::menu_assets;

struct Raster {
    std::vector<uint32_t> pixels; // straight-alpha RGBA8
    uint32_t width = 0, height = 0;
};

// The menu font of the UI language that shares the game language's script.
uint32_t UiLanguageFor(uint32_t game) {
    switch (game) {
    case 2: return 2; // Japanese
    case 7: return 3; // Korean
    case 8: return 1; // Traditional Chinese
    case 9: return 4; // Simplified Chinese
    default: return 0;
    }
}

std::u32string Codepoints(const std::string& text) {
    std::u32string out;
    for (size_t at = 0; at < text.size();)
        if (const uint32_t cp = host_ui::font::DecodeUtf8(text, at)) out += char32_t(cp);
    return out;
}

Raster DecodeIcon(const std::vector<uint8_t>& png) {
    Raster out;
    if (png.empty()) return out;
    SDL_IOStream* io = SDL_IOFromConstMem(png.data(), png.size());
    SDL_Surface* decoded = io ? SDL_LoadPNG_IO(io, true) : nullptr;
    if (!decoded) return out;
    SDL_Surface* rgba = SDL_ConvertSurface(decoded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(decoded);
    if (!rgba) return out;
    out.width = uint32_t(rgba->w);
    out.height = uint32_t(rgba->h);
    out.pixels.resize(size_t(out.width) * out.height);
    for (uint32_t y = 0; y < out.height; ++y)
        memcpy(&out.pixels[size_t(y) * out.width], static_cast<const uint8_t*>(rgba->pixels) + size_t(y) * rgba->pitch, out.width * 4);
    SDL_DestroySurface(rgba);
    return out;
}

// One line of white text with the game font's dark edge, `height` output
// pixels tall. Characters the game fonts lack come from Unifont.
Raster DrawText(const menu_assets::Assets* assets, const std::string& utf8, double height) {
    const auto text = Codepoints(utf8);
    const auto covers = [&](const menu_assets::Font& font) {
        return font.height && std::all_of(text.begin(), text.end(), [&](char32_t c) { return c == U' ' || font.glyphs.contains(uint32_t(c)); });
    };
    const menu_assets::Font* font = nullptr;
    if (assets) font = covers(assets->body) ? &assets->body : covers(assets->fallback) ? &assets->fallback : nullptr;
    Raster out;
    if (font) {
        const double ratio = height / font->height;
        double advance = 0;
        for (const char32_t c : text) advance += c == U' ' ? font->height * 0.3 : int(font->glyphs.at(uint32_t(c)).width) + font->kerning;
        out.width = uint32_t(std::ceil(advance * ratio)) + 2;
        out.height = uint32_t(std::ceil(font->height * ratio)) + 2;
        std::vector<double> color(size_t(out.width) * out.height * 3), alpha(size_t(out.width) * out.height);
        double left = 1;
        for (const char32_t c : text) {
            if (c == U' ') { left += font->height * 0.3 * ratio; continue; }
            const auto& g = font->glyphs.at(uint32_t(c));
            const auto& page = font->pages[g.page];
            const double w = g.width * ratio, h = g.height * ratio;
            const int x0 = std::max(0, int(left)), x1 = std::min(int(out.width), int(std::ceil(left + w)));
            const int y1 = std::min(int(out.height), int(std::ceil(1 + h)));
            for (int py = 1; py < y1; ++py)
                for (int px = x0; px < x1; ++px) {
                    const double u = std::clamp((px + .5 - left) * g.width / w - .5, 0.0, double(g.width - 1));
                    const double v = std::clamp((py + .5 - 1) * g.height / h - .5, 0.0, double(g.height - 1));
                    const int ux = int(u), vy = int(v), ux1 = std::min<int>(ux + 1, g.width - 1), vy1 = std::min<int>(vy + 1, g.height - 1);
                    const double fx = u - ux, fy = v - vy;
                    const uint32_t s[] = {page.pixels[size_t(g.y + vy) * page.width + g.x + ux], page.pixels[size_t(g.y + vy) * page.width + g.x + ux1],
                                          page.pixels[size_t(g.y + vy1) * page.width + g.x + ux], page.pixels[size_t(g.y + vy1) * page.width + g.x + ux1]};
                    const double wt[] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
                    const size_t at = size_t(py) * out.width + px;
                    for (int i = 0; i < 4; ++i) {
                        const double a = ((s[i] >> 24) & 255) * wt[i];
                        // The page's grey level blends the dark edge into the white face.
                        const double face = ((s[i] >> 16) & 255) / 255.0;
                        const double value = 16 + (255 - 16) * face;
                        alpha[at] += a;
                        for (int k = 0; k < 3; ++k) color[at * 3 + k] += value * a;
                    }
                }
            left += (int(g.width) + font->kerning) * ratio;
        }
        out.pixels.resize(alpha.size());
        for (size_t i = 0; i < alpha.size(); ++i) {
            const double a = std::min(alpha[i], 255.0);
            if (a <= 0) continue;
            uint32_t pixel = uint32_t(a) << 24;
            for (int k = 0; k < 3; ++k) pixel |= uint32_t(std::clamp(color[i * 3 + k] / alpha[i], 0.0, 255.0)) << (8 * k);
            out.pixels[i] = pixel;
        }
        return out;
    }
    // Unifont: 16 px bitmaps scaled to the height, with a one-pixel dark shadow.
    const double scale = height / 16.0;
    double advance = 0;
    for (const char32_t c : text) advance += (host_ui::font::GetGlyph(uint32_t(c)).width + 1) * scale;
    out.width = uint32_t(std::ceil(advance)) + 2;
    out.height = uint32_t(std::ceil(height)) + 2;
    out.pixels.assign(size_t(out.width) * out.height, 0);
    for (int pass = 0; pass < 2; ++pass) {
        double left = 0;
        const int shift = pass == 0 ? 1 : 0;
        const uint32_t ink = pass == 0 ? 0xFF101010u : 0xFFFFFFFFu;
        for (const char32_t c : text) {
            const auto g = host_ui::font::GetGlyph(uint32_t(c));
            for (uint32_t py = 0; py + shift < out.height; ++py)
                for (int px = int(left); px < int(left + g.width * scale) && px + shift < int(out.width); ++px) {
                    const int gx = std::min(g.width - 1, int((px - left) / scale)), gy = std::min(15, int(py / scale));
                    if (!g.bitmap) continue;
                    const uint16_t row = g.width == 8 ? uint16_t(g.bitmap[gy]) << 8 : uint16_t(g.bitmap[gy * 2] << 8 | g.bitmap[gy * 2 + 1]);
                    if (row & (0x8000 >> gx)) out.pixels[size_t(py + shift) * out.width + px + shift] = ink;
                }
            left += (g.width + 1) * scale;
        }
    }
    return out;
}

// achievement.wav next to settings.ini replaces the built-in chime.
std::shared_ptr<const std::vector<float>> Chime() {
    static const auto chime = [] {
        std::vector<float> samples;
        const auto path = os::user_paths::SettingsPath().parent_path() / "achievement.wav";
        SDL_AudioSpec spec{};
        Uint8* data = nullptr;
        Uint32 length = 0;
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec) && SDL_LoadWAV(path.string().c_str(), &spec, &data, &length)) {
            const SDL_AudioSpec mono{SDL_AUDIO_F32, 1, 48000};
            Uint8* converted = nullptr;
            int convertedLength = 0;
            if (SDL_ConvertAudioSamples(&spec, data, int(length), &mono, &converted, &convertedLength)) {
                samples.assign(reinterpret_cast<const float*>(converted), reinterpret_cast<const float*>(converted) + convertedLength / 4);
                SDL_free(converted);
                LOG_INFO("achievements: chime from {} ({:.2f} s)", path.string(), samples.size() / 48000.0);
            }
            SDL_free(data);
        }
        if (samples.empty()) samples = ui::achievement_toast::SynthesizeChime(48000);
        return std::make_shared<const std::vector<float>>(std::move(samples));
    }();
    return chime;
}

struct Toast {
    uint16_t id = 0;
    std::optional<achievements::Info> info;
    Raster icon, title, detail;
    float scale = 0;
    ui::achievement_toast::Content content;
    double clock = 0;
    std::chrono::steady_clock::time_point last;
};
std::optional<Toast> gToast;
Image gImage;
uint64_t gRevision = 0;

void Prepare(Toast& toast, float scale) {
    toast.scale = scale;
    const auto assets = menu_assets::Cached(FileSystem::GetGameRoot(), UiLanguageFor(settings::GameLanguage()));
    toast.title = DrawText(assets.get(), achievements::UnlockedHeading(), 21.0 * scale);
    toast.detail = DrawText(assets.get(), fmt::format("{}G - {}", toast.info->score, toast.info->name), 16.0 * scale);
    auto& c = toast.content;
    c.revision = ++gRevision;
    c.icon = toast.icon.pixels.empty() ? nullptr : toast.icon.pixels.data();
    c.iconWidth = toast.icon.width;
    c.iconHeight = toast.icon.height;
    c.title = toast.title.pixels.data();
    c.titleWidth = toast.title.width;
    c.titleHeight = toast.title.height;
    c.detail = toast.detail.pixels.data();
    c.detailWidth = toast.detail.width;
    c.detailHeight = toast.detail.height;
}
}

const Image* Update(uint32_t outputWidth, uint32_t outputHeight) {
    if (!outputWidth || !outputHeight) return nullptr;
    const auto now = std::chrono::steady_clock::now();
    if (!gToast) {
        const auto id = achievements::NextAnnouncement();
        if (!id) return nullptr;
        const auto config = settings::GetConfig();
        if (!config.achievementNotifications) return nullptr;
        Toast toast;
        toast.id = *id;
        toast.info = achievements::Find(*id);
        if (!toast.info) return nullptr;
        toast.icon = DecodeIcon(toast.info->iconPng);
        toast.last = now;
        LOG_INFO("achievements: toast for {} ({}x{} icon)", toast.id, toast.icon.width, toast.icon.height);
        gToast = std::move(toast);
        if (config.achievementSound) apu::PlayUiSound(Chime());
    } else {
        // Frames that were not presented (menus, loads) do not count.
        gToast->clock += std::min(std::chrono::duration<double>(now - gToast->last).count(), 0.05);
        gToast->last = now;
    }
    const float scale = std::max(0.5f, std::min(outputWidth / 1280.0f, outputHeight / 720.0f));
    if (scale != gToast->scale) Prepare(*gToast, scale);
    if (gToast->clock >= ui::achievement_toast::Duration()) {
        gToast.reset();
        return nullptr;
    }
    auto frame = ui::achievement_toast::Render(gToast->content, gToast->clock, scale);
    if (!frame.visible || !frame.width || !frame.height) return nullptr;
    gImage.pixels = std::move(frame.pixels);
    gImage.width = frame.width;
    gImage.height = frame.height;
    // Bottom centre, inside the title-safe area like the console's own toast.
    const int anchorX = int(outputWidth / 2), anchorY = int(outputHeight * 0.88);
    gImage.x = std::clamp(anchorX + frame.offsetX, 0, std::max(0, int(outputWidth) - int(frame.width)));
    gImage.y = std::clamp(anchorY + frame.offsetY, 0, std::max(0, int(outputHeight) - int(frame.height)));
    if (gImage.width > outputWidth || gImage.height > outputHeight) return nullptr;
    return &gImage;
}
}
