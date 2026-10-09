#include <settings/menu_render.h>
#include <settings/menu_assets.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>

namespace {
using Clock = std::chrono::steady_clock;
unsigned checks = 0;
void Check(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
std::shared_ptr<const settings::menu_assets::Assets> Atlas(unsigned seed) {
    auto a = std::make_shared<settings::menu_assets::Assets>();
    a->menu.width = 512; a->menu.height = 1024;
    a->menu.pixels.resize(512 * 1024);
    for (size_t i = 0; i < a->menu.pixels.size(); ++i) {
        const uint32_t c = 65 + (i * 17 + i / 512 * 11 + seed) % 70;
        a->menu.pixels[i] = 0xff000000 | c | (c << 8) | (c << 16);
    }
    return a;
}
settings::MenuSnapshot Snapshot() {
    settings::MenuSnapshot s;
    s.tab = 2; s.language = 0;
    s.rows = {
        {L"Anti-aliasing / Upscaling", L"FSR 3.1", true, {L"Off",L"FXAA",L"SMAA",L"TAA",L"DLSS",L"FSR 3.1"},5},
        {L"Quality", L"Quality", true, {L"Quality",L"Balanced",L"Performance",L"Native AA"},0},
        {L"FSR sharpness", L"25%", true, {},0,25},
        {L"Save graphics settings", L"Save", true, {L"Save"},0}};
    s.help = L"D-pad: navigate / adjust    A: confirm    B: back";
    s.assets = Atlas(0);
    return s;
}
std::vector<uint32_t> Draw(const settings::MenuSnapshot& s, uint32_t w, uint32_t h) {
    std::vector<uint32_t> pixels;
    if (!settings::RasterizeMenu(s,w,h,pixels)) std::abort();
    return pixels;
}
}
int main() {
    auto s = Snapshot();
    for (auto [w,h] : {std::pair{1280u,720u}, {1920u,1080u}, {3840u,2160u}, {720u,1280u}}) {
        const auto coldStart = Clock::now();
        auto cold = Draw(s,w,h);
        const double coldMs = std::chrono::duration<double,std::milli>(Clock::now()-coldStart).count();
        std::vector<double> times;
        for (int focus = 0; focus < 4; ++focus) {
            s.row = focus;
            const auto start = Clock::now();
            auto warm = Draw(s,w,h);
            times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
            // A new immutable asset owner forces a cold redraw. Same atlas
            // bytes must produce exactly the same image as the warmed cache.
            s.assets = std::make_shared<settings::menu_assets::Assets>(*s.assets);
            Check(warm == Draw(s,w,h), "cached/cold pixels match after focus change");
        }
        std::sort(times.begin(),times.end());
        std::printf("%ux%u synthetic atlas: cold=%.3f ms, warm median=%.3f ms\n",w,h,coldMs,(times[1]+times[2])/2);
    }
    s.language = 4; s.rows[0].name = L"抗锯齿 / 超分";
    s.rows[0].value = L"DLSS"; s.rows[0].selectedChoice = 4;
    s.rows[2].hidden = true;
    s.dialogTitle = L"确认"; s.dialogMessage = L"保存图形设置？";
    s.dialogChoices = {L"保存",L"取消"}; s.dialogSelection = 1;
    auto warm = Draw(s,1280,720);
    s.assets = std::make_shared<settings::menu_assets::Assets>(*s.assets);
    Check(warm == Draw(s,1280,720), "language, values, hidden rows and dialog stay dynamic");
    s.assets = Atlas(19);
    auto replaced = Draw(s,1280,720);
    Check(warm != replaced, "asset replacement invalidates backdrop");
    s.assets.reset();
    auto fallback = Draw(s,1280,720);
    Check(fallback != replaced && fallback == Draw(s,1280,720), "missing assets use cached fallback backdrop");
    auto worker = std::async(std::launch::async,[s] { return Draw(s,1280,720); });
    Check(worker.get() == fallback, "separate presentation threads do not share mutable cache");
    std::vector<uint32_t> empty;
    Check(!settings::RasterizeMenu(s,0,720,empty), "invalid size still rejected");
    // Audio-tab speaker test: the layout draws under the rows with a path that
    // follows the phase; the marker layer changes pixels only inside the bounds
    // it reports, wider when it frames a speaker.
    auto audio = Snapshot();
    audio.tab = 1; audio.row = 0; audio.rows.resize(2);
    const auto plain = Draw(audio,1920,1080);
    audio.speakerLayout = true;
    const auto layout = Draw(audio,1920,1080);
    Check(plain != layout, "speaker layout draws under the rows");
    audio.speakerPhase = 180;
    Check(Draw(audio,1920,1080) != layout, "the decoded path follows the phase");
    for (float position : {1.0f, 2.5f, 4.0f}) {
        auto marked = layout;
        const auto r = settings::DrawSpeakerMarker(marked,1920,1080,position,90.0f);
        bool inside = true, changed = false;
        for (size_t y = 0; y < 1080; ++y)
            for (size_t x = 0; x < 1920; ++x)
                if (marked[y*1920+x] != layout[y*1920+x]) {
                    changed = true;
                    inside = inside && x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1;
                }
        Check(changed && inside, "speaker marker stays inside its bounds");
        Check((r.x1 - r.x0 > 80) == (position != 2.5f), "the marker frames a speaker only while resting on it");
    }
    std::printf("PASS: %u native-resolution backdrop cache checks\n",checks);
}
