#include <modding/image_mod.h>
#include <modding/texture_mod.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace fs = std::filesystem;
using namespace modding;
namespace {
void Env(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value) setenv(name, value, 1); else unsetenv(name);
#endif
}
struct Environment {
    std::vector<std::pair<std::string, std::optional<std::string>>> saved;
    Environment() {
        for (const auto* name : {"LO_MODS", "LO_MODS_DIR", "LO_MODS_MODE"}) {
            const auto* value = std::getenv(name);
            saved.emplace_back(name, value ? std::optional<std::string>(value) : std::nullopt);
            Env(name, nullptr);
        }
    }
    ~Environment() { for (auto& [name, value] : saved) Env(name.c_str(), value ? value->c_str() : nullptr); }
};
struct Temp {
    fs::path path;
    Temp() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int i = 0; i != 100; ++i) {
            path = fs::temp_directory_path() / ("lo-mod-test-" + std::to_string(stamp) + "-" + std::to_string(i));
            if (fs::create_directory(path)) return;
        }
        throw std::runtime_error("cannot create test directory");
    }
    ~Temp() { Shutdown(); std::error_code ec; fs::remove_all(path, ec); }
};
void Write(const fs::path& path, std::string_view bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error("test file write failed");
}
std::string Lotex(const std::string& key, uint32_t w = 2, uint32_t h = 1) {
    std::string data("LOTEX1\r\n", 8);
    auto u32 = [&](uint32_t value) { for (int i = 0; i != 4; ++i) data.push_back(char(value >> (8 * i))); };
    u32(w); u32(h); u32(uint32_t(key.size())); u32(1); data += key;
    // R,G,B,A -> native 0xAARRGGBB, including transparent pixels.
    for (uint32_t i = 0; i != w * h; ++i) data.append("\x12\x34\x56\x78", 4);
    return data;
}
const std::string key = "bin/xenon/loc/int/menu/test.xxx#21:Icon_Page_0";
const AssetRequest request{{AssetKind::Image, key}, {}};
void Mod(const fs::path& root, const std::string& dir, const std::string& id, int priority, bool enabled = true) {
    Write(root / dir / "test.lotex", Lotex(key));
    Write(root / dir / "mod.ini", "api_version=1\nid=" + id + "\nimage:" + key +
        "=test.lotex\npriority=" + std::to_string(priority) + "\nenabled=" + (enabled ? "true\n" : "false\n"));
}
struct Provider : AssetProvider {
    fs::path path;
    int calls = 0;
    bool recursive = false, throws = false;
    std::optional<ResolvedAsset> Resolve(const AssetRequest& r) override {
        ++calls;
        if (throws) throw std::runtime_error("optional provider failed");
        if (recursive) return modding::Resolve(r);
        return ResolvedAsset{r.id, path, "provider", 0};
    }
    ~Provider() override { (void)Generation(); } // Must be destroyed outside the API mutex.
};
void Keys() {
    assert(MakeManifestKey("BIN\\XENON//LOC/./int/menu/test.xxx", 21, "Icon_Page_0") == key);
    for (const auto* path : {"", ".", "../bad", "a/../b", "/absolute", "C:\\bad", "a/", " a", "a#b", "a=b"})
        assert(MakeManifestKey(path, 0, "Object").empty());
    for (const auto* object : {"", " a", "a ", "a:b", "a/b", "a\\b", "a#b", "a=b", "a\nb"})
        assert(MakeManifestKey("a.xxx", 0, object).empty());
    assert(!MakeManifestKey("a.xxx", UINT32_MAX, "Object").empty());
    assert(OverlayRelativePath({AssetKind::Image, "a.xxx#4294967296:X"}).empty());
    assert(OverlayRelativePath({AssetKind(99), key}).empty());
    assert(OverlayRelativePath({AssetKind::Image, key}).parent_path() == fs::path("overlay/images"));
    assert(OverlayRelativePath({AssetKind::Image, "BIN/XENON/LOC/int/menu/test.xxx#00021:Icon_Page_0"}) ==
           OverlayRelativePath(request.id));
}
void Resolution(const fs::path& root) {
    Mod(root, "a", "first", 1);
    Mod(root, "b", "second", 1);
    Mod(root, "c", "disabled", 200, false);
    Initialize(root);
    assert(Mode() == ResolutionMode::Combined);
    assert((ModIds() == std::vector<std::string>{"first", "second"}));
    assert(Resolve(request)->modId == "second");
    auto decoded = ReadImageReplacement(request, 2, 1);
    assert(decoded && decoded->pixels == std::vector<uint32_t>({0x78123456, 0x78123456}));
    assert(!ReadImageReplacement(request, 1, 2));
    assert(!ReadImageReplacement({{AssetKind::Font, key}, {}}, 2, 1));
    const auto oldGeneration = Generation();
    Mod(root, "a", "first", 3);
    Reload();
    assert(Generation() > oldGeneration && Resolve(request)->modId == "first");

    auto provider = std::make_shared<Provider>();
    provider->path = root / "a/test.lotex";
    assert(RegisterProvider(AssetKind::Image, provider));
    assert(!RegisterProvider(AssetKind::Image, provider));
    assert(Resolve(request)->modId == "provider");
    provider->recursive = true;
    assert(Resolve(request)->modId == "first");
    provider->recursive = false; provider->throws = true;
    assert(Resolve(request)->modId == "first");
    provider->throws = false;

    const auto overlay = root / OverlayRelativePath(request.id);
    Write(overlay, Lotex(key));
    assert(Resolve(request)->modId == "@overlay");
    Env("LO_MODS_MODE", "overlay"); Reload();
    const auto calls = provider->calls;
    assert(Mode() == ResolutionMode::Overlay && Resolve(request)->modId == "@overlay");
    fs::remove(overlay);
    assert(!Resolve(request) && provider->calls == calls); // A disabled manager mod cannot reappear via a provider/manifest.
    Write(overlay, Lotex(key));
    Env("LO_MODS_MODE", "standalone"); Reload();
    assert(Resolve(request)->modId == "provider");
    UnregisterProvider(AssetKind::Image, provider.get());
    provider.reset();
    assert(Resolve(request)->modId == "first");
    Env("LO_MODS", "0"); Reload(); assert(!Resolve(request));
    Env("LO_MODS", nullptr); Env("LO_MODS_MODE", "invalid"); Reload();
    assert(!Resolve(request) && !Diagnostics().empty());
    Env("LO_MODS_MODE", nullptr); Reload();

    // Payload corruption must fall back to vanilla, not leak a losing mod.
    auto bad = Lotex(key); bad[0] = '?'; Write(overlay, bad);
    assert(!ReadImageReplacement(request, 2, 1));
    Write(overlay, Lotex("a.xxx#0:Wrong")); assert(!ReadImageReplacement(request, 2, 1));
    bad = Lotex(key); bad.pop_back(); Write(overlay, bad); assert(!ReadImageReplacement(request, 2, 1));
    bad = Lotex(key) + "x"; Write(overlay, bad); assert(!ReadImageReplacement(request, 2, 1));
    bad = Lotex(key); bad[20] = 2; Write(overlay, bad); assert(!ReadImageReplacement(request, 2, 1));
    Write(overlay, Lotex(key)); assert(ReadImageReplacement(request, 2, 1));
    fs::remove(overlay);
}
void Validation(const fs::path& root, const fs::path& outside) {
    Write(outside / "outside.lotex", Lotex(key));
    Write(root / "escape/mod.ini", "api_version=1\nid=escape\npriority=999\nimage:" + key + "=../../outside/outside.lotex\n");
    Write(root / "bad-version/mod.ini", "api_version=2\nid=bad-version\n");
    Write(root / "no-version/mod.ini", "id=no-version\n");
    Write(root / "duplicate/mod.ini", "api_version=1\nid=x\nid=y\n");
    Write(root / "huge/mod.ini", std::string(1024 * 1024 + 1, 'x'));
    Initialize(root);
    assert(Diagnostics().size() >= 5);
    assert(Resolve(request)->modId == "first");
    std::error_code ec;
    fs::create_directory_symlink(outside, root / "symlink", ec);
    if (!ec) {
        Write(outside / "mod.ini", "api_version=1\nid=symlink\npriority=10000\nimage:" + key + "=outside.lotex\n");
        // Symlinked mod folders (Vortex, Steam Deck deployments) are followed.
        Reload(); assert(Resolve(request)->modId == "symlink");
    }
    // Environment root changes must not permanently replace the default root.
    auto path = outside.generic_u8string(); const std::string utf8(path.begin(), path.end());
    Env("LO_MODS_DIR", utf8.c_str()); Reload(); assert(Root() == fs::absolute(outside));
    Env("LO_MODS_DIR", nullptr); Reload(); assert(Root() == fs::absolute(root));
    // Remove deliberately invalid fixtures before stressing immutable snapshots.
    for (const auto* dir : {"escape", "bad-version", "no-version", "duplicate", "huge", "symlink"})
        fs::remove_all(root / dir);
    Reload();
    auto reader = std::async(std::launch::async, [] {
        for (int i = 0; i != 100; ++i) assert(Resolve(request));
    });
    for (int i = 0; i != 20; ++i) Reload();
    reader.get();
}
std::string Lotex2(uint64_t fingerprint, uint32_t format, uint32_t w, uint32_t h, const std::string& payload,
    uint32_t type = 1, uint32_t mips = 1) {
    std::string data("LOTEX2\r\n", 8);
    auto u32 = [&](uint32_t value) { for (int i = 0; i != 4; ++i) data.push_back(char(value >> (8 * i))); };
    u32(64 + 4); u32(type); u32(uint32_t(fingerprint)); u32(uint32_t(fingerprint >> 32));
    u32(format); u32(w); u32(h); u32(w); u32(h); u32(mips); u32(uint32_t(payload.size())); u32(0); u32(4); u32(0);
    return data + "test" + payload;
}
// DDS header for a type-2 payload; dxgi != 0 appends the DX10 header.
std::string Dds(uint32_t w, uint32_t h, uint32_t mips, const char* fourCC, uint32_t dxgi = 0,
    uint32_t caps2 = 0, uint32_t miscFlag = 0) {
    std::string data("DDS ", 4);
    auto u32 = [&](uint32_t value) { for (int i = 0; i != 4; ++i) data.push_back(char(value >> (8 * i))); };
    u32(124); u32(0x21007); u32(h); u32(w); u32(0); u32(0); u32(mips);
    for (int i = 0; i != 11; ++i) u32(0);
    u32(32); u32(4); data.append(fourCC, 4); for (int i = 0; i != 5; ++i) u32(0);
    u32(0x401008); u32(caps2); u32(0); u32(0); u32(0);
    if (dxgi) { u32(dxgi); u32(3); u32(miscFlag); u32(1); u32(0); }
    return data;
}
void BlockCompressedTextures(const fs::path& root, uint64_t fingerprint) {
    // Manifest entries need the file at load time; each case rewrites it.
    Write(root / "tex/t.lotex2", "");
    Write(root / "tex/mod.ini", "api_version=1\nid=tex\ntexture:0123456789abcdef=t.lotex2\n");
    Reload();
    std::string error;
    auto read = [&](const std::string& file, uint32_t format = 18, uint32_t size = 8, uint64_t maxLevelBytes = UINT64_MAX) {
        Write(root / "tex/t.lotex2", file);
        error.clear();
        return ReadTextureReplacement(fingerprint, format, size, size, maxLevelBytes, &error);
    };
    // 8x8 BC1, two levels: 2x2 blocks then one block, 8 bytes each.
    std::string bc1(40, '\0');
    for (size_t i = 0; i != bc1.size(); ++i) bc1[i] = char(i * 7 + 1);
    for (const auto& dds : {Dds(8, 8, 2, "DXT1"), Dds(8, 8, 2, "DX10", kTextureBc1)}) {
        const auto data = read(Lotex2(fingerprint, 18, 8, 8, dds + bc1, 2, 2));
        assert(data && error.empty() && data->type == 2 && data->dxgiFormat == kTextureBc1 && data->scale == 1);
        assert(data->key == "test" && std::string(TexturePayloadName(*data)) == "BC1" && data->levels.size() == 2);
        assert(std::string(data->levels[0].begin(), data->levels[0].end()) == bc1.substr(0, 32));
        assert(std::string(data->levels[1].begin(), data->levels[1].end()) == bc1.substr(32));
    }
    // The size cap applies to level 0's compressed bytes (32), not RGBA8 (256),
    // so the renderer's 72 MiB level cap admits 8192x8192 BC7 (64 MiB).
    assert(TextureBcLevelBytes(8192, 8192, kTextureBc7) == 64ull << 20 && TextureBcLevelBytes(4, 4, kTextureBc4) == 8);
    assert(read(Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 2, "DXT1") + bc1, 2, 2), 18, 8, 32));
    assert(!read(Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 2, "DXT1") + bc1, 2, 2), 18, 8, 31) && !error.empty());
    // BC7 (16-byte blocks) for a DXT5 original and BC4 (ATI1, one level, no mip count) for G8.
    const auto bc7 = read(Lotex2(fingerprint, 20, 8, 8, Dds(8, 8, 2, "DX10", kTextureBc7) + std::string(80, 'x'), 2, 2), 20);
    assert(bc7 && bc7->dxgiFormat == kTextureBc7 && bc7->levels[0].size() == 64 && bc7->levels[1].size() == 16);
    const auto bc4 = read(Lotex2(fingerprint, 2, 8, 8, Dds(8, 8, 0, "ATI1") + std::string(32, 'y'), 2, 1), 2);
    assert(bc4 && bc4->dxgiFormat == kTextureBc4 && bc4->levels.size() == 1);
    // Rejections keep the original and report why.
    const std::string rejected[] = {
        Lotex2(fingerprint, 18, 8, 8, Dds(4, 8, 2, "DXT1") + bc1, 2, 2),               // DDS width differs
        Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 1, "DXT1") + bc1, 2, 2),               // DDS mip count differs
        Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 2, "DXT1", 0, 0xFE00) + bc1, 2, 2),    // cube map (legacy)
        Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 2, "DX10", kTextureBc1, 0, 4) + bc1, 2, 2), // cube map (DX10)
        Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 2, "DX10", 72) + bc1, 2, 2),           // BC1 sRGB
        Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 2, "DXT3") + std::string(80, 'z'), 2, 2), // BC2
        Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 2, "DXT1") + bc1.substr(1), 2, 2),     // truncated level
        Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 2, "DXT1") + bc1 + "!", 2, 2),         // trailing data
        Lotex2(fingerprint, 18, 8, 8, bc1, 2, 2),                                      // no DDS header
        Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 2, "DXT1") + bc1, 3, 2),               // unknown payload type
    };
    for (const auto& file : rejected) assert(!read(file) && !error.empty());
    // Level 0 must be whole blocks: a 6x6 A8R8G8B8 original cannot use type 2.
    assert(!read(Lotex2(fingerprint, 6, 6, 6, Dds(6, 6, 1, "DXT1") + std::string(32, 'w'), 2, 1), 6, 6) && !error.empty());
    // Devices without BC skip type 2 without reading or reporting it; type 1 still loads.
    Write(root / "tex/t.lotex2", Lotex2(fingerprint, 18, 8, 8, Dds(8, 8, 2, "DXT1") + bc1, 2, 2));
    bool skipped = false;
    error.clear();
    assert(!ReadTextureReplacement(fingerprint, 18, 8, 8, UINT64_MAX, &error, false, &skipped) && skipped && error.empty());
    Write(root / "tex/t.lotex2", Lotex2(fingerprint, 18, 8, 8, std::string(256, 'r')));
    assert(ReadTextureReplacement(fingerprint, 18, 8, 8, UINT64_MAX, &error, false, &skipped) && !skipped);
    fs::remove_all(root / "tex");
}
void Textures(const fs::path& root) {
    constexpr uint64_t fingerprint = 0x0123456789abcdefull;
    const AssetRequest texture{{AssetKind::Texture, "0123456789abcdef"}, {}};
    assert(OverlayRelativePath(texture.id) == fs::path("overlay/textures/fp-0123456789abcdef.lotex2"));
    for (const auto* bad : {"0123456789ABCDEF", "0123456789abcde", "0123456789abcdef0", "0123456789abcdeg", key.c_str()})
        assert(OverlayRelativePath({AssetKind::Texture, bad}).empty() && !Resolve({{AssetKind::Texture, bad}, {}}));
    Reload(); assert(!HasTextureReplacements());
    // One 2x2 level: the reader derives the 1x1 box-filtered level.
    const std::string pixels("\x00\x04\x08\x0c\x04\x08\x0c\x10\x08\x0c\x10\x14\x0c\x10\x14\x18", 16);
    Write(root / "tex/t.lotex2", Lotex2(fingerprint, 18, 2, 2, pixels));
    Write(root / "tex/mod.ini", "api_version=1\nid=tex\ntexture:0123456789abcdef=t.lotex2\n");
    Reload();
    assert(HasTextureReplacements() && Resolve(texture)->modId == "tex");
    const auto data = ReadTextureReplacement(fingerprint, 18, 2, 2, UINT64_MAX);
    assert(data && data->scale == 1 && data->key == "test" && data->levels.size() == 2);
    assert((data->levels[1] == std::vector<uint8_t>{6, 10, 14, 18}));
    std::string error;
    assert(!ReadTextureReplacement(fingerprint, 6, 2, 2, UINT64_MAX, &error) && !error.empty());
    assert(!ReadTextureReplacement(fingerprint, 18, 2, 2, 15, &error));
    const auto overlay = root / OverlayRelativePath(texture.id);
    Write(overlay, Lotex2(fingerprint, 18, 2, 2, pixels.substr(1)));
    assert(Resolve(texture)->modId == "@overlay" && !ReadTextureReplacement(fingerprint, 18, 2, 2, UINT64_MAX, &error));
    fs::remove_all(root / "tex"); fs::remove_all(overlay.parent_path());
    BlockCompressedTextures(root, fingerprint);
    Env("LO_MODS_MODE", "overlay"); Reload(); assert(!HasTextureReplacements());
    Env("LO_MODS_MODE", nullptr); Reload();
}
void Texts(const fs::path& root) {
    const std::string menu = "bin/xenon/loc/int/menu/menu_int.dat", jmd = "bin/xenon/scr/mes/int/a.jmd";
    const AssetRequest text{{AssetKind::Text, "BIN/Xenon/loc/int/menu/menu_int.dat"}, {}};
    assert(OverlayRelativePath(text.id) == fs::path("overlay/text/" + menu + ".json"));
    for (const auto* bad : {"", "/bin/a.dat", "bin/../a.dat", "bin/a/", "c:/a.dat"})
        assert(OverlayRelativePath({AssetKind::Text, bad}).empty());
    Reload(); assert(ListTexts().empty());
    Write(root / "lang/menu.json", "{}");
    Write(root / "lang/mod.ini", "api_version=1\nid=lang\ntext:" + menu + "=menu.json\ntext:" + jmd + "=missing.json\n");
    Reload();
    auto texts = ListTexts();
    assert(texts.size() == 1 && texts[0].id.key == menu && texts[0].modId == "lang" && Resolve(text)->modId == "lang");
    // The export's text/ folder under overlay/; other files there are ignored.
    Write(root / "overlay/text" / (menu + ".json"), "{}");
    Write(root / "overlay/text" / (jmd + ".json"), "{}");
    Write(root / "overlay/text/readme.txt", "x");
    texts = ListTexts();
    assert(texts.size() == 2 && texts[0].modId == "@overlay" && texts[1].id.key == jmd && Resolve(text)->modId == "@overlay");
    Env("LO_MODS_MODE", "standalone"); Reload();
    texts = ListTexts();
    assert(texts.size() == 1 && texts[0].modId == "lang");
    Env("LO_MODS_MODE", nullptr);
    fs::remove_all(root / "lang"); fs::remove_all(root / "overlay/text"); Reload();
    assert(ListTexts().empty());
}
void Languages(const fs::path& root) {
    const std::string menu = "bin/xenon/loc/int/menu/menu_int.dat", name = "Portugu\xc3\xaas (Brasil)";
    Reload(); assert(LanguagePacks().empty());
    Write(root / "pt/language.ini", "\xef\xbb\xbf# A new language\nid=PT-BR\nname=" + name + "\nbase=INT\n");
    Write(root / "pt/text" / (menu + ".json"), "{}");
    Write(root / "pt/text/readme.txt", "x");
    Write(root / "bad/language.ini", "id=bad\nname=Bad\n");
    Write(root / "zz-copy/language.ini", "id=pt-br\nname=Copy\nbase=int\n");
    Reload();
    const auto packs = LanguagePacks();
    assert(packs.size() == 1 && packs[0].id == "pt-br" && packs[0].name == name && packs[0].base == "int");
    const auto diagnostics = Diagnostics();
    assert(std::count_if(diagnostics.begin(), diagnostics.end(),
        [](const Diagnostic& d) { return d.manifest.filename() == "language.ini"; }) == 2);
    const auto texts = ListTexts(packs[0]);
    assert(texts.size() == 1 && texts[0].id.key == menu && texts[0].modId == "pt-br");
    assert(ListTexts().empty()); // A pack applies only when selected.
    Env("LO_MODS_MODE", "overlay"); Reload(); assert(LanguagePacks().size() == 1);
    Env("LO_MODS", "0"); Reload(); assert(LanguagePacks().empty() && ListTexts(packs[0]).empty());
    Env("LO_MODS", nullptr); Env("LO_MODS_MODE", nullptr);
    for (const auto* dir : {"pt", "bad", "zz-copy"}) fs::remove_all(root / dir);
    Reload(); assert(LanguagePacks().empty());
}
}
int main() {
    Environment environment;
    Temp temp;
    Keys();
    const auto root = temp.path / "mods";
    Resolution(root);
    Validation(root, temp.path / "outside");
    Textures(root);
    Texts(root);
    Languages(root);
    Shutdown(); assert(!Resolve(request));
    std::cout << "Mod API, image, texture, text and language pack contracts, manager isolation and reload tests passed\n";
}
