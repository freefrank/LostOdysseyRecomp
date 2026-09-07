#include <settings/menu_render.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <cstdlib>
static void Require(bool ok, const char *message) { if (!ok) { std::fprintf(stderr, "%s\n", message); std::exit(1); } }
int main(int argc, char **argv)
{
    settings::MenuSnapshot snapshot;
    snapshot.tab = 2;
    snapshot.language = 4;
    snapshot.rows = {{L"抗锯齿 / Antialiasing", L"SMAA"}, {L"输出分辨率", L"3840 x 2160"},
                     {L"帧率", L"60 FPS"}, {L"动态状态", L"等待验证", false}};
    snapshot.help = L"方向键选择 · Enter 确认 / Native-resolution text";
    std::vector<uint32_t> pixels;
    Require(!settings::RasterizeMenu(snapshot, 0, 720, pixels), "zero dimension accepted");
    Require(!settings::RasterizeMenu(snapshot, 16385, 1, pixels), "oversized dimension accepted");
    for (const auto size : {std::pair{1280u,720u}, {1920u,1080u}, {3840u,2160u}, {1920u,1200u}, {720u,1280u}})
    {
        const auto [w,h] = size;
        Require(settings::RasterizeMenu(snapshot,w,h,pixels), "rasterization failed");
        Require(pixels.size() == size_t(w)*h, "wrong output dimensions");
        Require(std::all_of(pixels.begin(),pixels.end(),[](uint32_t p){return (p>>24)==255;}), "nonopaque pixel");
        if (uint64_t(w)*720 != uint64_t(h)*1280)
            Require(pixels.front()==0xff000000 && pixels.back()==0xff000000, "letterbox not black");
        const auto textPixels=std::count(pixels.begin(),pixels.end(),0xffd5e0e5u);
        Require(textPixels > 100, "missing glyph pixels");
        if (argc>1) {
            std::filesystem::create_directories(argv[1]);
            std::ofstream f(std::filesystem::path(argv[1])/(std::to_string(w)+"x"+std::to_string(h)+".ppm"),std::ios::binary);
            f<<"P6\n"<<w<<" "<<h<<"\n255\n";
            for(auto p:pixels) { const char rgb[]={char(p),char(p>>8),char(p>>16)};f.write(rgb,3); }
        }
        std::printf("native menu %ux%u: opaque, text, aspect fit passed\n",w,h);
    }
    return 0;
}
