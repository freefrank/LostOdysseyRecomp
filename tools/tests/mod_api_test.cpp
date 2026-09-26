#include <modding/image_mod.h>
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
        Reload(); assert(Resolve(request)->modId == "first");
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
}
int main() {
    Environment environment;
    Temp temp;
    Keys();
    const auto root = temp.path / "mods";
    Resolution(root);
    Validation(root, temp.path / "outside");
    Shutdown(); assert(!Resolve(request));
    std::cout << "Mod API, image contract, manager isolation and reload tests passed\n";
}
