#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace modding {
inline constexpr uint32_t kModApiVersion = 1;
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
class AssetProvider {
public:
    virtual ~AssetProvider() = default;
    virtual std::optional<ResolvedAsset> Resolve(const AssetRequest& request) = 0;
};

// Snapshot replacement; decoded caches must observe Generation().
// LO_MODS_DIR overrides root; LO_MODS=0 disables all resolution, including providers.
// LO_MODS_MODE=overlay isolates external managers from manifests AND providers.
// The default combined mode preserves overlay > provider > standalone precedence.
// Containment is lexical: files may resolve through a VFS (MO2) or symlinks.
void Initialize(const std::filesystem::path& root);
void Reload();
void Shutdown();
uint64_t Generation();
std::filesystem::path Root();
ResolutionMode Mode();
bool Enabled();
std::vector<Diagnostic> Diagnostics();
std::vector<std::string> ModIds(); // Enabled standalone mods, sorted.
// Cheap pre-check before fingerprinting uploads: a manifest texture entry, a
// texture provider or an overlay/textures folder existed at Initialize.
bool HasTextureReplacements();
// Every text replacement Resolve would return, one per member path (overlay
// files first, then manifest entries by priority). Providers are not asked.
std::vector<ResolvedAsset> ListTexts();
std::optional<ResolvedAsset> Resolve(const AssetRequest& request);
// Providers are trusted host extensions, not DLLs loaded from mod packages.
// Shared ownership keeps an in-flight callback alive during unregistration.
bool RegisterProvider(AssetKind kind, std::shared_ptr<AssetProvider> provider);
void UnregisterProvider(AssetKind kind, const AssetProvider* provider);

// Zero-based extractor export index. Package separators/ASCII case are normalized;
// object case and UTF-8 bytes are preserved. Invalid identities return empty.
std::string MakeManifestKey(std::string_view package, uint32_t exportIndex, std::string_view object);
// Textures: overlay/textures/fp-<16 hex digits>.lotex2; text: overlay/text/<member
// path>.json, the export's text/ layout; other kinds hash the key.
std::filesystem::path OverlayRelativePath(const AssetId& id);
}
