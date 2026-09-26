#include <settings/menu_render.h>
#include <settings/menu_assets.h>
#include <hid/controller_glyphs.h>
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
    snapshot.row = 3;
    snapshot.rows = {
        {L"图形后端", L"Vulkan", true, {L"Direct3D 12", L"Vulkan"}, 1},
        {L"显示模式", L"无边框全屏", true, {L"窗口", L"无边框全屏", L"独占全屏"}, 1},
        {L"宽屏", L"开", true, {L"开", L"关"}, 0},
        {L"输出分辨率", L"3440 × 1440", true,
         {L"1720 × 720", L"2560 × 1080", L"3440 × 1440", L"3840 × 1600", L"5120 × 2160"}, 2},
        {L"内部分辨率", L"自动（跟随输出）", true,
         {L"自动（跟随输出）", L"720p", L"1080p", L"1440p", L"2160p"}, 0},
        {L"抗锯齿", L"TAA（实验性）", true, {L"关", L"FXAA", L"SMAA", L"TAA（实验性）"}, 3},
        {L"缩放质量", L"高", true, {L"标准", L"高"}, 1},
        {L"帧率", L"60 FPS（实验性）", true,
         {L"30 FPS", L"60 FPS（实验性）", L"120 FPS（实验性）"}, 1},
        {L"亮度校准", L"打开", true, {L"打开"}, 0},
        {L"保存图形设置", L"保存", true, {L"保存"}, 0}};
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
        if (argc > 1)
        {
            std::filesystem::create_directories(argv[1]);
            std::ofstream f(std::filesystem::path(argv[1]) / (std::to_string(w) + "x" + std::to_string(h) + ".ppm"), std::ios::binary);
            f << "P6\n" << w << " " << h << "\n255\n";
            for (auto p : pixels) { const char rgb[] = {char(p), char(p >> 8), char(p >> 16)}; f.write(rgb, 3); }
        }
        if (w == 1280 && h == 720)
        {
            const auto luminance=[](uint32_t p){ return int(p&255)+int((p>>8)&255)+int((p>>16)&255); };
            const int selectedY = 150 + snapshot.row * 43;
            Require(luminance(pixels[(selectedY + 18) * 1280 + 250]) > 500, "selected label bevel missing");
            Require(luminance(pixels[(selectedY + 18) * 1280 + 700]) > 500, "selected option bevel missing");
            Require(luminance(pixels[(selectedY + 18) * 1280 + 440]) < 500, "unselected option was highlighted");
            Require(pixels[640*1280+10] != 0xff000000u, "brushed-metal footer missing");
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
    // The original-language font and Unifont need not contain these glyphs:
    // the PS confirmation options, footer and all four symbols use line art.
    const auto regularPixels = pixels;
    snapshot.playStationPrompts = true;
    snapshot.help = L"A: confirm  B: back  X: select  Y: new folder";
    Require(settings::RasterizeMenu(snapshot,1280,720,pixels), "PlayStation menu rasterization failed");
    Require(pixels != regularPixels, "PlayStation hints did not change pixels");
    auto changed = [&](int x, int y, int w, int h) {
        size_t count = 0;
        for (int py = y; py < y + h; ++py)
            for (int px = x; px < x + w; ++px)
                count += pixels[size_t(py) * 1280 + px] != regularPixels[size_t(py) * 1280 + px];
        return count;
    };
    Require(changed(420, 410, 26, 24) > 8, "PlayStation confirmation button missing");
    // Inspect the produced PPM for the four distinct fallback shapes.
    if (argc > 1) {
        std::ofstream f(std::filesystem::path(argv[1])/"gameplay-playstation.ppm",std::ios::binary);
        f<<"P6\n1280 720\n255\n";
        for(auto p:pixels) { const char rgb[]={char(p),char(p>>8),char(p>>16)};f.write(rgb,3); }
    }
    snapshot.playStationPrompts = false;
    snapshot.help = L"Set the speed at which text is displayed.";
    pixels = regularPixels;
    if (argc>1) {
        std::ofstream f(std::filesystem::path(argv[1])/"gameplay-reference.ppm",std::ios::binary);
        f<<"P6\n1280 720\n255\n";
        for(auto p:pixels) { const char rgb[]={char(p),char(p>>8),char(p>>16)};f.write(rgb,3); }
    }
    // The live Settings help is translated before render. Only PS style
    // changes complete shoulder labels; Xbox text and nearby words survive.
    auto shoulderSnapshot = [&](const wchar_t* help, bool ps) {
        snapshot.help = help;
        snapshot.playStationPrompts = ps;
        Require(settings::RasterizeMenu(snapshot,1280,720,pixels), "shoulder prompt rasterization failed");
        return pixels;
    };
    auto saveShoulder = [&](const char* name) {
        if (argc <= 1) return;
        std::ofstream f(std::filesystem::path(argv[1])/name,std::ios::binary);
        Require(bool(f), "could not open shoulder screenshot");
        f<<"P6\n1280 720\n255\n";
        for(auto p:pixels) { const char rgb[]={char(p),char(p>>8),char(p>>16)};f.write(rgb,3); }
        Require(bool(f), "could not write shoulder screenshot");
    };
    const auto shoulderReference = shoulderSnapshot(L"LB / RB: category   LT + RT: edit   VOLT: unchanged",false);
    saveShoulder("shoulders-reference.ppm");
    const auto shoulderPlayStation = shoulderSnapshot(L"LB / RB: category   LT + RT: edit   VOLT: unchanged",true);
    saveShoulder("shoulders-playstation.ppm");
    auto changedFooter = [&](const std::vector<uint32_t>& before, const std::vector<uint32_t>& after) {
        size_t count=0;
        for(int y=645;y<700;++y)
            for(int x=130;x<1195;++x)
                count+=before[size_t(y)*1280+x]!=after[size_t(y)*1280+x];
        return count;
    };
    Require(changedFooter(shoulderReference,shoulderPlayStation)>30, "PS shoulder labels did not update Settings footer");
    Require(shoulderSnapshot(L"LB / RB: category   LT + RT: edit   VOLT: unchanged",false)==shoulderReference,
            "non-PS Settings footer did not restore");
    snapshot.language=4;
    const auto chineseShoulderReference=shoulderSnapshot(L"LB / RB：分类  LT+RT：类别",false);
    const auto chineseShoulderPlayStation=shoulderSnapshot(L"LB / RB：分类  LT+RT：类别",true);
    Require(changedFooter(chineseShoulderReference,chineseShoulderPlayStation)>20,
            "Chinese PS shoulder labels did not update Settings footer");
    snapshot.language=0;
    snapshot.playStationPrompts=false;
    snapshot.help=L"Set the speed at which text is displayed.";
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

    snapshot.tab = 3;
    snapshot.row = 0;
    snapshot.dialogTitle.clear();
    snapshot.dialogMessage.clear();
    snapshot.dialogChoices.clear();
    snapshot.rows = {
        {L"設定界面語言", L"한국어", true, {L"English", L"繁體中文", L"日本語", L"한국어", L"简体中文"}, 3},
        {L"遊戲語言", L"한국어", true, {L"English", L"日本語", L"한국어", L"繁體中文", L"简体中文"}, 2},
        {L"自動更新", L"開", true, {L"開", L"關"}, 0},
        {L"儲存設定", L"儲存", true, {L"儲存"}, 0}};
    snapshot.help = L"設定界面語言立即生效。";
    Require(settings::RasterizeMenu(snapshot, 1280, 720, pixels), "language tab rasterization failed");
    if (argc > 1) {
        std::ofstream f(std::filesystem::path(argv[1]) / "language-reference.ppm", std::ios::binary);
        f << "P6\n1280 720\n255\n";
        for (auto p : pixels) { const char rgb[] = {char(p), char(p >> 8), char(p >> 16)}; f.write(rgb, 3); }
    }
    std::puts("Language reference (with Korean 한국어) rendered at 1280x720");

    // Synthetic overflowing menu (>11 visible rows) to verify scroll clipping, hidden rows and overflow indicators.
    {
        // Reference snapshot: exactly 11 rows (the visible window), representing Option 3 through Option 13.
        settings::MenuSnapshot refSnapshot;
        refSnapshot.tab = 2;
        refSnapshot.language = 0;
        for (int i = 3; i < 14; ++i)
            refSnapshot.rows.push_back({L"Option " + std::to_wstring(i), L"Val", true, {L"Val"}, 0});
        refSnapshot.row = 10; // Corresponds to Option 13
        refSnapshot.scroll = 0;
        refSnapshot.help = L"Overflow test";
        std::vector<uint32_t> refPixels;
        Require(settings::RasterizeMenu(refSnapshot, 1280, 720, refPixels), "ref raster failed");

        // Overflow snapshot: 16 rows total, with Option 2 marked hidden.
        // Visible sequence: Option 0, 1 (above scroll=2), Option 3..13 (visible, 11 rows), Option 14, 15 (below).
        settings::MenuSnapshot overflowSnapshot;
        overflowSnapshot.tab = 2;
        overflowSnapshot.language = 0;
        for (int i = 0; i < 16; ++i)
        {
            settings::MenuRow row{L"Option " + std::to_wstring(i), L"Val", true, {L"Val"}, 0};
            if (i == 2) row.hidden = true;
            overflowSnapshot.rows.push_back(row);
        }
        overflowSnapshot.row = 13; // Option 13
        overflowSnapshot.scroll = 2; // skips visible 0, 1 (and hidden 2) -> displays Option 3..13
        overflowSnapshot.help = L"Overflow test";
        Require(settings::RasterizeMenu(overflowSnapshot, 1280, 720, pixels), "overflow rasterization failed");

        // Verify that slot 0 (y=150..191) renders Option 3 identically between ref and scrolled overflow.
        // Label area: x in [82, 350], y in [155, 185]
        bool slot0Matches = true;
        for (int y = 155; y < 185; ++y)
            for (int x = 82; x < 350; ++x)
                if (pixels[y * 1280 + x] != refPixels[y * 1280 + x])
                    slot0Matches = false;
        Require(slot0Matches, "scrolled slot 0 must render Option 3 identical to reference");

        // Verify overflow indicators:
        // Top indicator at (1026-40..1026, 150-24..150-4) has rendered glyph pixels because scroll > 0
        int topGlyphPixels = 0;
        for (int y = 150 - 24; y < 150 - 4; ++y)
            for (int x = 386 + 640 - 40; x < 386 + 640; ++x)
                if (pixels[y * 1280 + x] != refPixels[y * 1280 + x])
                    ++topGlyphPixels;
        Require(topGlyphPixels > 10, "top overflow indicator glyph pixels missing");

        // Bottom indicator at (1026-40..1026, 622..642) has rendered glyph pixels because rows remain below viewport
        int bottomGlyphPixels = 0;
        for (int y = 622; y < 642; ++y)
            for (int x = 386 + 640 - 40; x < 386 + 640; ++x)
                if (pixels[y * 1280 + x] != refPixels[y * 1280 + x])
                    ++bottomGlyphPixels;
        Require(bottomGlyphPixels > 10, "bottom overflow indicator glyph pixels missing");

        std::puts("Synthetic overflow (>11 rows) pixel comparison and overflow indicators passed");
    }
    return 0;
}
