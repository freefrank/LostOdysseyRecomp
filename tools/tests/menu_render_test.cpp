#include <settings/menu_render.h>
#include <settings/menu_assets.h>
#include <lzokay.hpp>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <cstdlib>
static void Require(bool ok, const char *message) { if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); } }
void LoMenuRenderTrace(const std::wstring &value, bool original)
{
    std::printf("font=%s text=", original ? "original" : "fallback");
    for (const auto c : value) std::printf("%04x ", unsigned(c));
    std::putchar('\n');
}
int main(int argc, char **argv)
{
    settings::MenuSnapshot snapshot;
    snapshot.tab = 2;
    snapshot.language = 4;
    snapshot.row = 4;
    snapshot.rows = {
        {L"图形后端", L"Vulkan", true, {L"Direct3D 12", L"Vulkan"}, 1},
        {L"显示模式", L"无边框全屏", true, {L"窗口", L"无边框全屏", L"独占全屏"}, 1},
        {L"输出分辨率", L"3840 × 2160", true,
         {L"1280 × 720", L"1600 × 900", L"1920 × 1080", L"2560 × 1440", L"3840 × 2160"}, 4},
        {L"内部分辨率", L"自动（跟随输出）", true,
         {L"自动（跟随输出）", L"1280 × 720", L"1920 × 1080", L"2560 × 1440", L"3840 × 2160"}, 0},
        {L"抗锯齿", L"SMAA", true, {L"关", L"FXAA", L"SMAA", L"TAA（实验性）"}, 2},
        {L"缩放质量", L"高", true, {L"标准", L"高"}, 1},
        {L"帧率", L"60 FPS（实验性）", true,
         {L"30 FPS", L"60 FPS（实验性）", L"120 FPS（实验性）"}, 1},
        {L"亮度校准", L"打开", true, {L"打开"}, 0},
        {L"应用显示设置", L"应用", true, {L"应用"}, 0}};
    snapshot.help = L"LB / RB：分类    方向键：选择 / 调整    A：确认    B：返回";
    if (argc > 2)
    {
        snapshot.assets = settings::menu_assets::Cached(argv[2], snapshot.language);
        Require(bool(snapshot.assets), "installed SCH menu assets did not load");
        if (argc > 3 && std::string(argv[3]) == "--fallback-only")
        {
            const auto &font = snapshot.assets->fallback;
            Require(font.height && font.kerning == 0, "Abc native metrics missing");
            for (const auto ch : std::wstring(L"0123456789 ×"))
                Require(font.glyphs.contains(uint32_t(ch)), "Abc does not cover resolution string");
            const auto &g = font.glyphs.at(0x00d7);
            Require(g.x == 483 && g.y == 101 && g.width == 15 && g.height == 20 && g.page == 0,
                    "Abc native multiplication-sign mapping mismatch");
            Require(!font.glyphs.contains(0x952f) && !font.glyphs.contains(0x5e27), "unexpected SCH missing-character mapping");
            std::vector<uint32_t> image;
            Require(settings::RasterizeMenu(snapshot, 1280, 720, image), "Abc fallback raster failed");
            std::filesystem::create_directories(argv[1]);
            std::ofstream output(std::filesystem::path(argv[1]) / "graphics-abc.ppm", std::ios::binary);
            output << "P6\n1280 720\n255\n";
            for (auto pixel : image) { const char rgb[] = {char(pixel), char(pixel >> 8), char(pixel >> 16)}; output.write(rgb, 3); }
            std::puts("SCH Graphics Abc whole-string fallback: native metrics and multiplication-sign mapping passed");
            return 0;
        }
        if (argc < 4 || std::string(argv[3]) != "--render-only")
        {
        Require(snapshot.assets == settings::menu_assets::Cached(argv[2], snapshot.language), "menu asset cache missed");
        Require(snapshot.assets->body.glyphs.contains(L'设') && snapshot.assets->body.glyphs.contains(L'置'), "original Chinese glyph mapping missing");
        Require(!settings::menu_assets::Load(std::filesystem::path(argv[1]) / "absent-install", 0), "missing installation accepted");
        std::vector<uint8_t> malformed(64, 0);
        bool rejected = false;
        try { settings::menu_assets::DecodeFont(malformed, "Maru23"); } catch (...) { rejected = true; }
        Require(rejected, "malformed package accepted");
        // Extended zero-run used to read beyond its input in upstream lzokay.
        const uint8_t truncated[] = {0, 0, 0, 0}; uint8_t output[32]{}; size_t actual = 0;
        Require(lzokay::decompress(truncated, sizeof(truncated), output, sizeof(output), actual) == lzokay::EResult::InputOverrun,
                "truncated LZO extended length accepted");
        const uint8_t lookbehind[] = {18, 0, 0x40, 1, 0x11, 0, 0};
        Require(lzokay::decompress(lookbehind, sizeof(lookbehind), output, sizeof(output), actual) == lzokay::EResult::LookbehindOverrun,
                "LZO lookbehind preceding output accepted");
        const uint8_t literals[] = {22, 1, 2, 3, 4, 5, 0x11, 0, 0};
        Require(lzokay::decompress(literals, sizeof(literals), output, 4, actual) == lzokay::EResult::OutputOverrun,
                "LZO output overrun accepted");
        std::filesystem::create_directories(argv[1]);
        const auto &page = snapshot.assets->body.pages.front();
        std::ofstream raw(std::filesystem::path(argv[1]) / "font-sch.bgra", std::ios::binary);
        raw.write(reinterpret_cast<const char *>(page.pixels.data()), std::streamsize(page.pixels.size() * 4));
        std::puts("installed SCH FPI/CPX/font/texture, cache, absent/malformed and LZO bounds passed");
        }
    }
    std::vector<uint32_t> pixels;
    Require(!settings::RasterizeMenu(snapshot, 0, 720, pixels), "zero dimension accepted");
    Require(!settings::RasterizeMenu(snapshot, 16385, 1, pixels), "oversized dimension accepted");
    for (const auto &row : snapshot.rows)
        if (!row.choices.empty())
            Require(row.selectedChoice >= 0 && row.selectedChoice < int(row.choices.size()) &&
                    row.value == row.choices[row.selectedChoice], "selected choice/value mismatch");
    // Native reference size plus one letterboxed scale/long-label case.
    for (const auto size : {std::pair{1280u,720u}, {1920u,1200u}})
    {
        const auto [w,h] = size;
        Require(settings::RasterizeMenu(snapshot,w,h,pixels), "rasterization failed");
        Require(pixels.size() == size_t(w)*h, "wrong output dimensions");
        Require(std::all_of(pixels.begin(),pixels.end(),[](uint32_t p){return (p>>24)==255;}), "nonopaque pixel");
        if (uint64_t(w)*720 != uint64_t(h)*1280)
            Require(pixels.front()==0xff000000 && pixels.back()==0xff000000, "letterbox not black");
        const auto brightPixels=std::count_if(pixels.begin(),pixels.end(),[](uint32_t p){
            return (p&255)>210 && ((p>>8)&255)>210 && ((p>>16)&255)>210;
        });
        Require(brightPixels > 100, "missing outlined light glyphs");
        if (argc > 1 && w == 1280 && h == 720)
        {
            std::filesystem::create_directories(argv[1]);
            std::ofstream f(std::filesystem::path(argv[1]) / "first-frame.ppm", std::ios::binary);
            f << "P6\n" << w << " " << h << "\n255\n";
            for (auto p : pixels) { const char rgb[] = {char(p), char(p >> 8), char(p >> 16)}; f.write(rgb, 3); }
        }
        if (w == 1280 && h == 720)
        {
            const auto luminance=[](uint32_t p){ return int(p&255)+int((p>>8)&255)+int((p>>16)&255); };
            Require(luminance(pixels[340*1280+250]) > 500, "selected label bevel missing");
            Require(luminance(pixels[340*1280+730]) > 500, "selected option bevel missing");
            Require(luminance(pixels[340*1280+575]) < 500, "unselected option was highlighted");
            Require(pixels[640*1280+10] != 0xff000000u, "brushed-metal footer missing");
        }
        if (argc>1) {
            std::filesystem::create_directories(argv[1]);
            std::ofstream f(std::filesystem::path(argv[1])/(std::to_string(w)+"x"+std::to_string(h)+".ppm"),std::ios::binary);
            f<<"P6\n"<<w<<" "<<h<<"\n255\n";
            for(auto p:pixels) { const char rgb[]={char(p),char(p>>8),char(p>>16)};f.write(rgb,3); }
        }
        std::printf("original-style menu %ux%u: opaque, choice mapping, aspect fit passed\n",w,h);
    }
    snapshot.tab = 0;
    snapshot.language = 0;
    if (argc > 2)
    {
        snapshot.assets = settings::menu_assets::Cached(argv[2], 0);
        Require(bool(snapshot.assets), "installed INT menu assets did not load");
        Require(snapshot.assets->body.glyphs.size() == 199 && snapshot.assets->body.kerning == -2, "INT native font metrics mismatch");
        const auto &a = snapshot.assets->body.glyphs.at(L'A');
        Require(a.x == 92 && a.y == 37 && a.width == 18 && a.height == 31, "INT CharRemap does not select A");
        for (const auto pair : {std::pair{"font-int.bgra", &snapshot.assets->body.pages.front()}, std::pair{"menu-int.bgra", &snapshot.assets->menu}})
        {
            std::ofstream raw(std::filesystem::path(argv[1]) / pair.first, std::ios::binary);
            raw.write(reinterpret_cast<const char *>(pair.second->pixels.data()), std::streamsize(pair.second->pixels.size() * 4));
        }
    }
    snapshot.row = 0;
    snapshot.rows = {
        {L"Text Speed", L"Normal", true, {L"Fast", L"Normal", L"Slow"}, 1},
        {L"Caption", L"On", true, {L"On", L"Off"}, 0},
        {L"Remember Battle Cursor", L"On", true, {L"On", L"Off"}, 0},
        {L"Automatic Back-row Input", L"Off", true, {L"On", L"Off"}, 1},
        {L"Invert Camera Vertically", L"Off", true, {L"On", L"Off"}, 1},
        {L"Invert Camera Horizontally", L"Off", true, {L"On", L"Off"}, 1},
        {L"Confirmation Button", L"A / B", true, {L"A / B", L"B / A"}, 0, -1, true},
        {L"Restore Game Defaults", L"Restore", true, {L"Restore"}, 0}};
    snapshot.help = L"Set the speed at which text is displayed.";
    Require(settings::RasterizeMenu(snapshot,1280,720,pixels), "gameplay reference rasterization failed");
    if (argc>1) {
        std::ofstream f(std::filesystem::path(argv[1])/"gameplay-reference.ppm",std::ios::binary);
        f<<"P6\n1280 720\n255\n";
        for(auto p:pixels) { const char rgb[]={char(p),char(p>>8),char(p>>16)};f.write(rgb,3); }
    }
    std::puts("English gameplay reference rendered at 1280x720");
    snapshot.tab = 1;
    snapshot.row = 1;
    snapshot.rows = {
        {L"Voice Language", L"English", true, {L"English", L"Japanese", L"Korean"}, 0},
        {L"Music", L"72%", true, {}, 0, 72},
        {L"Sound Effects", L"40%", true, {}, 0, 40}};
    snapshot.help = L"Use left and right to adjust the music volume.";
    Require(settings::RasterizeMenu(snapshot,1280,720,pixels), "audio reference rasterization failed");
    if (argc>1) {
        std::ofstream f(std::filesystem::path(argv[1])/"audio-reference.ppm",std::ios::binary);
        f<<"P6\n1280 720\n255\n";
        for(auto p:pixels) { const char rgb[]={char(p),char(p>>8),char(p>>16)};f.write(rgb,3); }
    }
    std::puts("English audio reference rendered at 1280x720");
    snapshot.dialogTitle = L"需要重新启动 / Restart required";
    snapshot.dialogMessage = L"保存这些设置并立即重新启动吗？";
    snapshot.dialogChoices = {L"立即重新启动", L"稍后", L"取消"};
    snapshot.dialogSelection = 1;
    Require(settings::RasterizeMenu(snapshot,1280,720,pixels), "restart dialog rasterization failed");
    Require(pixels[250*1280+300] != 0xff000000u, "restart dialog surface missing");
    if (argc>1) {
        std::filesystem::create_directories(argv[1]);
        std::ofstream f(std::filesystem::path(argv[1])/"restart-dialog.ppm",std::ios::binary);
        f<<"P6\n1280 720\n255\n";
        for(auto p:pixels) { const char rgb[]={char(p),char(p>>8),char(p>>16)};f.write(rgb,3); }
    }
    std::puts("restart dialog: restart now/later/cancel surface passed");
    return 0;
}
