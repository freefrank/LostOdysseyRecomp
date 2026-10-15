// LoFontPatch: language-pack glyphs added to a font package without the game.
//   LoFontPatch --disc <disc folder> --member <archive path> --out <folder>
//               [--add <font>=<file.lofont>]... [--self-test]
// Reads the package from the disc's archives, checks its fonts
// (modding/font_patch.h Check), adds the glyphs (--self-test: three test
// glyphs on every font), checks the result again and reads every changed font
// back with the settings menu's own reader (settings/menu_assets.h). Writes the
// rebuilt package, each changed font's pages as RGBA (<font>.page<n>.rgba,
// width and height in <font>.json with the glyph table) to --out. Exit code 1
// on any failure.
#include <gpu/shader/cpx_decode.h>
#include <modding/font_patch.h>
#include <modding/fpi_index.h>
#include <settings/menu_assets.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
std::vector<uint8_t> ReadFile(const fs::path &file, uint64_t offset = 0, uint64_t size = ~0ull)
{
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open " + file.string());
    input.seekg(0, std::ios::end);
    const uint64_t length = uint64_t(input.tellg());
    if (size == ~0ull) size = length - offset;
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    input.seekg(std::streamoff(offset));
    if (!input.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(bytes.size()))) throw std::runtime_error("short read of " + file.string());
    return bytes;
}

void WriteFile(const fs::path &file, const void *data, size_t size)
{
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    if (!output || !output.write(static_cast<const char *>(data), std::streamsize(size))) throw std::runtime_error("cannot write " + file.string());
}

std::string Lower(std::string value)
{
    for (auto &c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}

std::vector<uint8_t> ReadMember(const fs::path &disc, const std::string &member)
{
    std::map<std::string, fs::path> names;
    for (const auto &item : fs::directory_iterator(disc)) names.emplace(Lower(item.path().filename().string()), item.path());
    const auto index = names.at("lo.fpi");
    auto header = ReadFile(index, 0, 64);
    const auto parsed = modding::fpi::Read(ReadFile(index, 0, modding::fpi::IndexSize(header)));
    const auto *file = modding::fpi::Find(parsed, member);
    if (!file) throw std::runtime_error(member + " is not in " + index.string());
    auto bytes = ReadFile(names.at(parsed.archives.at(file->archive)), file->offset, file->size);
    if (bytes.size() >= 3 && bytes[0] == 'c' && bytes[1] == 'p' && bytes[2] == 'x' && !xenos::resources::cpx::Decode(bytes, bytes))
        throw std::runtime_error("invalid CPX data in " + member);
    return bytes;
}

// Three outlined test glyphs (Arabic alef, beh, teh) for --self-test.
modding::font_patch::Glyphs TestGlyphs()
{
    modding::font_patch::Glyphs glyphs;
    glyphs.width = 64; glyphs.height = 24;
    glyphs.rgba.assign(size_t(glyphs.width) * glyphs.height * 4, 0);
    for (uint32_t i = 0; i < 3; ++i)
    {
        const uint32_t x0 = 1 + i * 20, w = 18, h = 22;
        glyphs.glyphs.push_back({0x0627 + 1 + i, x0, 1, w, h});
        for (uint32_t y = 1; y < 1 + h; ++y)
            for (uint32_t x = x0; x < x0 + w; ++x)
            {
                const bool edge = x == x0 || y == 1 || x == x0 + w - 1 || y == h;
                auto *p = &glyphs.rgba[(size_t(y) * glyphs.width + x) * 4];
                p[0] = p[1] = p[2] = edge ? 0 : 255; p[3] = 255;
            }
    }
    return glyphs;
}
}

int main(int argc, char **argv)
{
    try
    {
        fs::path disc, out;
        std::string member;
        std::vector<std::pair<std::string, fs::path>> files;
        bool selfTest = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            auto next = [&] { if (i + 1 >= argc) throw std::runtime_error(arg + " needs a value"); return std::string(argv[++i]); };
            if (arg == "--disc") disc = next();
            else if (arg == "--member") member = next();
            else if (arg == "--out") out = next();
            else if (arg == "--self-test") selfTest = true;
            else if (arg == "--add")
            {
                const auto value = next();
                const auto eq = value.find('=');
                if (eq == std::string::npos) throw std::runtime_error("--add takes <font>=<file>");
                files.emplace_back(value.substr(0, eq), value.substr(eq + 1));
            }
            else throw std::runtime_error("unknown argument " + arg);
        }
        if (disc.empty() || member.empty() || out.empty()) throw std::runtime_error("usage: LoFontPatch --disc <folder> --member <path> --out <folder> [--add <font>=<file>]... [--self-test]");
        fs::create_directories(out);
        const auto original = ReadMember(disc, member);
        std::printf("original %s (%zu bytes)\n", member.c_str(), original.size());
        const auto fonts = modding::font_patch::Check(original);
        for (const auto &line : fonts) std::printf("  %s\n", line.c_str());

        std::vector<modding::font_patch::Addition> additions;
        for (const auto &[font, file] : files)
            additions.push_back({font, modding::font_patch::Parse(ReadFile(file)), file.string()});
        if (selfTest)
            for (const auto &line : fonts) additions.push_back({line.substr(0, line.find(':')), TestGlyphs(), "self test"});
        if (additions.empty()) return 0;

        const auto result = modding::font_patch::RebuildPackage(original, additions);
        bool failed = result.patched != additions.size();
        for (const auto &note : result.notes) std::printf("patch: %s\n", note.c_str());
        if (result.package.empty()) throw std::runtime_error("nothing was patched");
        std::printf("rebuilt (%zu bytes)\n", result.package.size());
        for (const auto &line : modding::font_patch::Check(result.package)) std::printf("  %s\n", line.c_str());
        const auto stem = member.substr(member.rfind('/') + 1);
        WriteFile(out / stem, result.package.data(), result.package.size());

        // The settings menu's reader is independent of font_patch: every new glyph must be there.
        // It reads DXT5 pages only (the menu's fonts); fonts on A8R8G8B8 pages are listed with their glyphs instead.
        const auto infos = modding::font_patch::ReadFonts(result.package);
        for (const auto &addition : additions)
        {
            const auto info = std::find_if(infos.begin(), infos.end(), [&](const auto &f) { return f.name == addition.font; });
            if (info != infos.end() && std::any_of(info->pages.begin(), info->pages.end(), [](const auto &p) { return p.format != 7; }))
            {
                size_t found = 0;
                for (const auto &g : addition.glyphs.glyphs)
                    found += std::any_of(info->glyphs.begin(), info->glyphs.end(), [&](const auto &h) {
                        return h.codepoint == g.codepoint && h.width == g.width && h.height == g.height; });
                std::printf("glyph table: %s has %zu/%zu new glyphs (pages not DXT5: the menu reader does not read them)\n",
                            addition.font.c_str(), found, addition.glyphs.glyphs.size());
                failed |= found != addition.glyphs.glyphs.size();
                continue;
            }
            const auto font = settings::menu_assets::DecodeFont(result.package, addition.font);
            size_t found = 0;
            for (const auto &g : addition.glyphs.glyphs)
            {
                const auto it = font.glyphs.find(g.codepoint);
                if (it != font.glyphs.end() && it->second.width == g.width && it->second.height == g.height) ++found;
            }
            std::printf("menu reader: %s has %zu/%zu new glyphs, %zu pages, height %u\n", addition.font.c_str(), found,
                        addition.glyphs.glyphs.size(), font.pages.size(), font.height);
            failed |= found != addition.glyphs.glyphs.size();
            std::string json = "{\"kerning\": " + std::to_string(font.kerning) + ", \"pages\": [";
            for (size_t p = 0; p < font.pages.size(); ++p)
            {
                const auto &page = font.pages[p];
                // BGRA words -> RGBA bytes.
                std::vector<uint8_t> rgba(page.pixels.size() * 4);
                for (size_t i = 0; i < page.pixels.size(); ++i)
                {
                    const auto v = page.pixels[i];
                    rgba[i * 4] = uint8_t(v >> 16); rgba[i * 4 + 1] = uint8_t(v >> 8); rgba[i * 4 + 2] = uint8_t(v); rgba[i * 4 + 3] = uint8_t(v >> 24);
                }
                WriteFile(out / (addition.font + ".page" + std::to_string(p) + ".rgba"), rgba.data(), rgba.size());
                json += std::string(p ? ", " : "") + "[" + std::to_string(page.width) + ", " + std::to_string(page.height) + "]";
            }
            json += "], \"glyphs\": {";
            bool first = true;
            for (const auto &[cp, g] : font.glyphs)
            {
                json += std::string(first ? "" : ", ") + "\"" + std::to_string(cp) + "\": [" + std::to_string(g.x) + ", " + std::to_string(g.y) + ", " +
                        std::to_string(g.width) + ", " + std::to_string(g.height) + ", " + std::to_string(g.page) + "]";
                first = false;
            }
            json += "}}\n";
            WriteFile(out / (addition.font + ".json"), json.data(), json.size());
        }
        std::printf(failed ? "FAILED\n" : "OK\n");
        return failed ? 1 : 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "error: %s\n", error.what());
        return 1;
    }
}
