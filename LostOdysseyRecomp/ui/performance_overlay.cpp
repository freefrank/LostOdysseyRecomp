#include <stdafx.h>
#include "performance_overlay.h"
#include "text_raster.h"
#include <gpu/gpu_busy.h>
#include <kernel/io/file_system.h>
#include <settings/config.h>
#include <settings/menu_assets.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <unordered_map>
#ifndef _WIN32
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace ui::performance_overlay {
namespace {
using Clock = std::chrono::steady_clock;
namespace menu_assets = settings::menu_assets;

constexpr auto kRefresh = std::chrono::milliseconds(500);
// A longer gap between game frames is a host menu or a pause, not a frame.
constexpr auto kPause = std::chrono::seconds(1);
constexpr size_t kGraphFrames = 90;

// MangoHud's Steam Deck colours (0xBBGGRR).
constexpr uint32_t Rgb(uint32_t r, uint32_t g, uint32_t b) { return r | g << 8 | b << 16; }
constexpr uint32_t kWhite = Rgb(255, 255, 255);
constexpr uint32_t kUnit = Rgb(215, 215, 215);
constexpr uint32_t kFpsValue = Rgb(255, 214, 64);
constexpr uint32_t kLoadLabel = Rgb(46, 151, 203);     // CPU, GPU
constexpr uint32_t kBackendLabel = Rgb(235, 91, 91);   // the graphics API
constexpr uint32_t kUpscalerLabel = Rgb(173, 100, 193);
constexpr uint32_t kFgLabel = Rgb(194, 102, 147);
constexpr uint32_t kGraph = Rgb(120, 225, 90);
constexpr uint32_t kBackground = 0x99000000u; // black at 60%
constexpr uint32_t kDivider = 0xC0707070u;

// Process CPU time, every thread, user plus kernel, in nanoseconds.
uint64_t ProcessCpuNs() {
#ifdef _WIN32
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return 0;
    const auto ns = [](FILETIME t) { return (uint64_t(t.dwHighDateTime) << 32 | t.dwLowDateTime) * 100; };
    return ns(kernel) + ns(user);
#else
    // getrusage covers the whole process on Linux, Android and macOS alike
    // (the same totals as /proc/self/stat and task_info, without parsing).
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0;
    const auto ns = [](timeval t) { return uint64_t(t.tv_sec) * 1000000000ull + uint64_t(t.tv_usec) * 1000ull; };
    return ns(usage.ru_utime) + ns(usage.ru_stime);
#endif
}

// Like Task Manager: a share of all logical processors.
uint32_t LogicalProcessors() {
#ifdef _WIN32
    static const uint32_t count = std::max<uint32_t>(1, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
#else
    static const uint32_t count = uint32_t(std::max<long>(1, sysconf(_SC_NPROCESSORS_CONF)));
#endif
    return count;
}

struct Window {
    Clock::time_point start{};
    uint32_t frames = 0;
    uint64_t cpu = 0, gpu = 0, presented = 0;
    bool frameGeneration = false;
};

struct State {
    uint32_t mode = Off, outputWidth = 0, outputHeight = 0;
    Clock::time_point lastFrame{};
    Window window;
    bool baseline = true; // the next Refresh only starts a window
    std::array<float, kGraphFrames> intervals{};
    size_t intervalCount = 0, intervalNext = 0;
    Image image;
    std::unordered_map<uint64_t, text_raster::Raster> glyphs;
    std::shared_ptr<const menu_assets::Assets> assets;
} g;

// Straight-alpha source over a straight-alpha destination.
void Over(uint32_t& destination, uint32_t source) {
    const uint32_t sa = source >> 24;
    if (!sa) return;
    const uint32_t da = destination >> 24;
    if (sa == 255 || !da) { destination = source; return; }
    const uint32_t outA = sa * 255 + da * (255 - sa); // alpha times 255
    uint32_t result = (outA / 255) << 24;
    for (int k = 0; k < 3; ++k) {
        const uint32_t sc = (source >> (8 * k)) & 255, dc = (destination >> (8 * k)) & 255;
        result |= ((sc * sa * 255 + dc * da * (255 - sa)) / outA) << (8 * k);
    }
    destination = result;
}

class Canvas {
public:
    Canvas(Image& image, double scale) : image_(image), scale_(scale) {}
    double Scale() const { return scale_; }
    void Fill(int x, int y, int w, int h, uint32_t color) {
        const int x0 = std::max(0, x), y0 = std::max(0, y);
        const int x1 = std::min(int(image_.width), x + w), y1 = std::min(int(image_.height), y + h);
        for (int py = y0; py < y1; ++py)
            for (int px = x0; px < x1; ++px) Over(image_.pixels[size_t(py) * image_.width + px], color);
    }
    // Cached per character, size and colour: refreshes only blend glyphs.
    const text_raster::Raster& Glyph(char32_t c, double height, uint32_t face, bool fallback = false) {
        const uint64_t key = uint64_t(fallback) << 63 | uint64_t(c) << 40 | uint64_t(std::lround(height * 4)) << 24 | face;
        auto [entry, inserted] = g.glyphs.try_emplace(key);
        if (inserted) {
            std::string utf8;
            if (c < 0x80) utf8 += char(c);
            else if (c < 0x800) { utf8 += char(0xC0 | (c >> 6)); utf8 += char(0x80 | (c & 0x3F)); }
            else { utf8 += char(0xE0 | (c >> 12)); utf8 += char(0x80 | ((c >> 6) & 0x3F)); utf8 += char(0x80 | (c & 0x3F)); }
            entry->second = text_raster::DrawText(g.assets.get(), utf8, height, face, fallback);
        }
        return entry->second;
    }
    // Raster rows of the cap height (H's ink) for text of this height.
    std::pair<int, int> CapRows(double height) {
        const auto& glyph = Glyph(U'H', height, kWhite);
        int top = -1, bottom = -1;
        for (uint32_t y = 0; y < glyph.height; ++y)
            for (uint32_t x = 0; x < glyph.width; ++x)
                if ((glyph.pixels[size_t(y) * glyph.width + x] >> 24) >= 128) {
                    if (top < 0) top = int(y);
                    bottom = int(y);
                    break;
                }
        if (top < 0) return {1, int(std::lround(height))};
        return {top, bottom};
    }
    // Draws text with its raster top at y; returns the width.
    double Text(double x, int y, const std::u32string& text, double height, uint32_t face) {
        // Like the settings menu: a run the body font cannot cover (the × of a
        // resolution) is drawn whole in the fallback font.
        const bool fallback = g.assets && !std::all_of(text.begin(), text.end(), [](char32_t c) {
            return c == U' ' || g.assets->body.glyphs.contains(uint32_t(c));
        });
        double pen = x;
        for (const char32_t c : text) {
            const auto& glyph = Glyph(c, height, face, fallback);
            const int left = int(std::lround(pen)) - 1;
            for (uint32_t gy = 0; gy < glyph.height; ++gy) {
                const int py = y + int(gy);
                if (py < 0 || py >= int(image_.height)) continue;
                for (uint32_t gx = 0; gx < glyph.width; ++gx) {
                    const int px = left + int(gx);
                    if (px < 0 || px >= int(image_.width)) continue;
                    Over(image_.pixels[size_t(py) * image_.width + px], glyph.pixels[size_t(gy) * glyph.width + gx]);
                }
            }
            pen += glyph.advance;
        }
        return pen - x;
    }
    double Measure(const std::u32string& text, double height) {
        double width = 0;
        for (const char32_t c : text) width += Glyph(c, height, kWhite).advance;
        return width;
    }
    // A polyline through the values, oldest left, clamped to [0, top].
    void Graph(int x, int y, int w, int h, const float* values, size_t count, float top) {
        if (count < 2 || top <= 0) return;
        const int thickness = std::max(1, int(std::lround(scale_)));
        const uint32_t color = 0xFF000000u | kGraph;
        const auto rowOf = [&](float value) {
            return y + h - thickness - int(std::lround(std::clamp(value / top, 0.0f, 1.0f) * float(h - thickness)));
        };
        // Each column joins the previous one, so steep spikes stay connected.
        int previousX = x, previousY = rowOf(values[0]);
        Fill(previousX, previousY, thickness, thickness, color);
        for (size_t i = 1; i < count; ++i) {
            const int px = x + int(std::lround(double(i) * (w - 1) / double(count - 1)));
            const int py = rowOf(values[i]);
            int lastY = previousY;
            for (int cx = previousX + 1; cx <= px; ++cx) {
                const int cy = int(std::lround(previousY + (py - previousY) * double(cx - previousX) / double(px - previousX)));
                Fill(cx, std::min(lastY, cy), thickness, std::abs(cy - lastY) + thickness, color);
                lastY = cy;
            }
            previousX = px;
            previousY = py;
        }
    }

private:
    Image& image_;
    double scale_;
};

std::u32string Ascii(const std::string& text) { return std::u32string(text.begin(), text.end()); }
// "1280x720": the body fonts lack ×, and a resolution in the fallback font
// would not match the other numbers.
std::u32string Size(uint32_t width, uint32_t height) {
    return Ascii(std::to_string(width) + "x" + std::to_string(height));
}

struct Values {
    double fps = 0, frameMs = 0;
    int cpu = -1, gpu = -1;  // percent; -1 unknown
    double fgFps = -1;       // -1 while frame generation is off
};

// One row of fields separated by dividers. Labels sit on the values' baseline;
// units are smaller and share the values' cap top, like MangoHud's.
class Row {
public:
    Row(Canvas& canvas, int height) : canvas_(canvas), height_(height) {
        const double s = canvas.Scale();
        valueSize_ = 21 * s;
        smallValueSize_ = 18 * s;
        labelSize_ = 16 * s;
        unitSize_ = 12 * s;
        const auto [capTop, baseline] = canvas.CapRows(valueSize_);
        valueTop_ = int(std::lround(height * 0.5 - (capTop + baseline + 1) * 0.5));
        baseline_ = valueTop_ + baseline;
        capTop_ = valueTop_ + capTop;
        x_ = 10 * s;
    }
    double X() const { return x_; }
    void Label(const char* text, uint32_t color) {
        x_ += canvas_.Text(x_, baseline_ - canvas_.CapRows(labelSize_).second, Ascii(text), labelSize_, color);
        x_ += 7 * canvas_.Scale();
    }
    void Value(const std::u32string& text, uint32_t color = kWhite, bool reduced = false) {
        const double size = reduced ? smallValueSize_ : valueSize_;
        x_ += canvas_.Text(x_, baseline_ - canvas_.CapRows(size).second, text, size, color);
    }
    void Unit(const char* text) {
        x_ += 2 * canvas_.Scale();
        x_ += canvas_.Text(x_, capTop_ - canvas_.CapRows(unitSize_).first, Ascii(text), unitSize_, kUnit);
        x_ += 6 * canvas_.Scale();
    }
    void Divider() {
        const double s = canvas_.Scale();
        x_ += 4 * s;
        const int inset = int(std::lround(6 * s));
        canvas_.Fill(int(std::lround(x_)), inset, std::max(1, int(std::lround(s))), height_ - 2 * inset, kDivider);
        x_ += 12 * s;
    }
    void Graph(const float* values, size_t count, float top) {
        const double s = canvas_.Scale();
        const int w = int(std::lround(72 * s)), h = int(std::lround(height_ - 10 * s));
        canvas_.Graph(int(std::lround(x_)), int(std::lround(5 * s)), w, h, values, count, top);
        x_ += w + 8 * s;
    }

private:
    Canvas& canvas_;
    int height_;
    double valueSize_, smallValueSize_, labelSize_, unitSize_, x_;
    int valueTop_ = 0, baseline_ = 0, capTop_ = 0;
};

std::u32string Number(double value, int decimals) {
    char text[32];
    std::snprintf(text, sizeof(text), "%.*f", decimals, value);
    return Ascii(text);
}
std::u32string Percent(int value) { return value < 0 ? U"--" : Ascii(std::to_string(value)); }

void Draw(const Values& values, const Sources& sources) {
    // 1.0 at 720p, 1.5 at 1080p, 3.0 at 4K.
    const double s = std::max(0.75, g.outputHeight / 720.0);
    const int height = int(std::lround(30 * s));
    // The settings menu's font, which is loaded when the overlay is turned on there.
    auto assets = menu_assets::Cached(FileSystem::GetGameRoot(), settings::GetConfig().uiLanguage);
    if (assets != g.assets) { g.assets = std::move(assets); g.glyphs.clear(); }
    auto& image = g.image;
    image.x = image.y = 0;
    image.height = std::min<uint32_t>(height, g.outputHeight);
    if (g.mode == Detailed) {
        // A strip across the top, like the Deck's horizontal layout.
        image.width = g.outputWidth;
        image.pixels.assign(size_t(image.width) * image.height, kBackground);
        Canvas canvas(image, s);
        Row row(canvas, int(image.height));
        row.Label(sources.upscaler, kUpscalerLabel);
        row.Value(Size(sources.renderWidth, sources.renderHeight), kWhite, true);
        if (sources.outputWidth) {
            row.Value(U" → ", kUnit, true);
            row.Value(Size(sources.outputWidth, sources.outputHeight), kWhite, true);
        }
        row.Divider();
        row.Label(sources.backend, kBackendLabel);
        row.Value(Number(values.fps, 0), kFpsValue);
        row.Unit("FPS");
        std::array<float, kGraphFrames> ordered{};
        const size_t count = g.intervalCount;
        float sum = 0;
        for (size_t i = 0; i < count; ++i) {
            ordered[i] = g.intervals[(g.intervalNext + kGraphFrames - count + i) % kGraphFrames];
            sum += ordered[i];
        }
        row.Graph(ordered.data(), count, count ? 2.0f * sum / float(count) : 0.0f);
        row.Value(Number(values.frameMs, 1));
        row.Unit("ms");
        if (values.fgFps >= 0) {
            row.Divider();
            row.Label("FG", kFgLabel);
            row.Value(Number(values.fgFps, 0), kFpsValue);
            row.Unit("FPS");
        }
        row.Divider();
        row.Label("CPU", kLoadLabel);
        row.Value(Percent(values.cpu));
        row.Unit("%");
        row.Divider();
        row.Label("GPU", kLoadLabel);
        row.Value(Percent(values.gpu));
        row.Unit("%");
    } else {
        // The frame rate alone in a block at the top-left corner.
        const auto fps = Number(values.fps, 0), ms = Number(values.frameMs, 1);
        image.width = g.outputWidth;
        image.pixels.assign(size_t(image.width) * image.height, 0);
        Canvas canvas(image, s);
        Row row(canvas, int(image.height));
        row.Value(fps, kFpsValue);
        row.Unit("FPS");
        row.Value(ms);
        row.Unit("ms");
        const uint32_t used = std::min<uint32_t>(image.width, uint32_t(std::lround(row.X() + 4 * s)));
        // Background behind the text only, then crop to it.
        std::vector<uint32_t> block(size_t(used) * image.height, kBackground);
        for (uint32_t y = 0; y < image.height; ++y)
            for (uint32_t x = 0; x < used; ++x) Over(block[size_t(y) * used + x], image.pixels[size_t(y) * image.width + x]);
        image.pixels = std::move(block);
        image.width = used;
        // Clear of the screen edge, as the Deck's corner counter.
        image.x = image.y = int(std::lround(6 * s));
    }
    ++image.revision;
}
}

bool Frame(uint32_t mode, Clock::time_point now, uint32_t outputWidth, uint32_t outputHeight) {
    if (mode != g.mode || outputWidth != g.outputWidth || outputHeight != g.outputHeight) {
        gpu::gpu_busy::Request(mode == Detailed);
        if (mode != g.mode) {
            g.image.pixels.clear();
            g.image.width = g.image.height = 0;
            ++g.image.revision;
            g.intervalCount = 0;
        }
        if (outputHeight != g.outputHeight) g.glyphs.clear(); // sizes follow the output height
        g.mode = mode;
        g.outputWidth = outputWidth;
        g.outputHeight = outputHeight;
        g.lastFrame = {};
    }
    const bool restart = g.lastFrame == Clock::time_point{} || now - g.lastFrame > kPause;
    if (!restart) {
        ++g.window.frames;
        if (mode == Detailed) {
            g.intervals[g.intervalNext] = std::chrono::duration<float, std::milli>(now - g.lastFrame).count();
            g.intervalNext = (g.intervalNext + 1) % kGraphFrames;
            g.intervalCount = std::min(g.intervalCount + 1, kGraphFrames);
        }
    }
    g.lastFrame = now;
    if (restart) g.baseline = true;
    return g.baseline || (g.window.frames && now - g.window.start >= kRefresh);
}

void Refresh(const Sources& sources) {
    const auto now = g.lastFrame;
    const bool detailed = g.mode == Detailed;
    const uint64_t cpu = detailed ? ProcessCpuNs() : 0;
    if (!g.baseline) {
        const double seconds = std::chrono::duration<double>(now - g.window.start).count();
        Values values;
        values.fps = g.window.frames / seconds;
        values.frameMs = 1000.0 * seconds / g.window.frames;
        if (detailed && cpu > g.window.cpu)
            values.cpu = int(std::lround(std::min(100.0, (cpu - g.window.cpu) / (seconds * 1e9 * LogicalProcessors()) * 100.0)));
        // No completed timestamp in the window: unsupported or not started yet.
        if (detailed && sources.gpuBusy > g.window.gpu)
            values.gpu = int(std::lround(std::min(100.0, (sources.gpuBusy - g.window.gpu) / (seconds * 1e9) * 100.0)));
        // No presents counted in the window (the count query failed): no FG value.
        if (detailed && sources.frameGeneration && g.window.frameGeneration && sources.presentedFrames > g.window.presented)
            values.fgFps = (sources.presentedFrames - g.window.presented) / seconds;
        Draw(values, sources);
    }
    g.baseline = false;
    g.window = {now, 0, cpu, sources.gpuBusy, sources.presentedFrames, sources.frameGeneration};
}

const Image& Current() { return g.image; }

void Stop() {
    if (g.mode == Off) return;
    gpu::gpu_busy::Request(false);
    g = {};
}
}
