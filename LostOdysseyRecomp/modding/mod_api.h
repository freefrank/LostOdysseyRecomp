#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace modding {
// Highest mod.ini api_version this runtime reads (1 and 2 are accepted).
inline constexpr uint32_t kModApiVersion = 2;
// Texture keys are renderer fingerprints: exactly 16 lowercase hex digits.
// Text keys are archive member paths (bin/xenon/loc/int/menu/menu_int.dat); the
// file is the translation JSON that --export-assets writes for that member.
enum class AssetKind : uint32_t { Image = 1, Font = 2, Model = 3, Movie = 4, Texture = 5, Text = 6 };
enum class ResolutionMode : uint32_t { Combined, Standalone, Overlay };
struct AssetId { AssetKind kind = AssetKind::Image; std::string key; };
struct AssetRequest { AssetId id; std::filesystem::path originalPath; };
struct ResolvedAsset {
    AssetId id;
    std::filesystem::path path;
    std::string modId;
    int32_t priority = 0;
};
struct Diagnostic { std::filesystem::path manifest; size_t line = 0; std::string message; };
// A mods folder with a language.ini (id, name, base) is a language pack: its
// text/ folder holds translation JSON in the --export-assets text/ layout. It
// applies only while the player has it selected in Settings, in every mode.
// id is lower case; base is a three-letter game language code (int, jpn...).
struct LanguagePack { std::string id, name, base; std::filesystem::path folder; };
// One folder under the mods root that holds a mod.ini, valid or not.
struct ModInfo {
    std::string id;                                 // manifest id, else the folder name
    std::string name, version, author, description; // api_version=2 metadata; empty when unset
    std::filesystem::path folder;
    int32_t priority = 0;
    uint32_t apiVersion = 0;                        // 0: mod.ini rejected (see problems)
    bool manifestEnabled = true;                    // mod.ini enabled=
    bool listEnabled = true;                        // mod-list.ini: false only for <id>=off
    bool active = false;                            // contributes files in this snapshot
    size_t overlayFiles = 0;                        // files found under overlay/ (active mods only)
    size_t manifestEntries = 0;                     // valid resource lines in mod.ini
    std::vector<Diagnostic> problems;
};
class AssetProvider {
public:
    virtual ~AssetProvider() = default;
    virtual std::optional<ResolvedAsset> Resolve(const AssetRequest& request) = 0;
};

// Snapshot replacement; decoded caches must observe Generation().
// LO_MODS_DIR overrides root; LO_MODS=0 disables all resolution, including providers.
// LO_MODS_MODE=overlay isolates external managers from mod folders AND providers.
// The default combined mode keeps top-level overlay > provider > mod folders.
// Mod folders follow modList (mod-list.ini, the in-game manager's order), then
// priority. Each api_version=2 folder's overlay/ is listed once here, so
// Resolve is a hash lookup. Containment is lexical: files may resolve through
// a VFS (MO2) or symlinks. An empty modList means no list file.
void Initialize(const std::filesystem::path& root, const std::filesystem::path& modList = {});
void Reload();
void Shutdown();
uint64_t Generation();
std::filesystem::path Root();
ResolutionMode Mode();
bool Enabled();
std::vector<Diagnostic> Diagnostics();
std::vector<std::string> ModIds(); // Active mod folders, sorted.
// Every mod folder found, active or not, highest precedence first. Empty when
// mods are disabled (LO_MODS=0, invalid LO_MODS_MODE).
std::vector<ModInfo> ListMods();
// The mod-list.ini path given to Initialize (empty if none).
std::filesystem::path ModListPath();
// Writes mod-list.ini (temp file + rename): order is highest precedence first,
// true = on. Lines of the current file whose id order omits (mods that are not
// installed right now, comments) stay right after the id that preceded them.
// Takes effect at the next Initialize/Reload. False and *error on failure.
bool SaveModList(const std::vector<std::pair<std::string, bool>>& order, std::string* error = nullptr);
// Cheap pre-check before fingerprinting uploads: a mod texture file or entry,
// a texture provider or an overlay/textures folder existed at Initialize.
bool HasTextureReplacements();
// Every text replacement Resolve would return, one per member path (top-level
// overlay files first, then mod folders in order). Providers are not asked.
std::vector<ResolvedAsset> ListTexts();
// Installed language packs by folder name; a repeated id keeps the first.
std::vector<LanguagePack> LanguagePacks();
// The pack's text/ files, one per member path.
std::vector<ResolvedAsset> ListTexts(const LanguagePack& pack);
std::optional<ResolvedAsset> Resolve(const AssetRequest& request);
// Providers are trusted host extensions, not DLLs loaded from mod packages.
// Shared ownership keeps an in-flight callback alive during unregistration.
bool RegisterProvider(AssetKind kind, std::shared_ptr<AssetProvider> provider);
void UnregisterProvider(AssetKind kind, const AssetProvider* provider);

// Zero-based extractor export index. Package separators/ASCII case are normalized;
// object case and UTF-8 bytes are preserved. Invalid identities return empty.
std::string MakeManifestKey(std::string_view package, uint32_t exportIndex, std::string_view object);
// Textures: overlay/textures/fp-<16 hex digits>.lotex2; text: overlay/text/<member
// path>.json, the export's text/ layout; other kinds hash the key. The same
// names work in a mod folder's own overlay/ (api_version=2).
std::filesystem::path OverlayRelativePath(const AssetId& id);
}
