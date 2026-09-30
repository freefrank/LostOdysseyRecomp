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
enum class AssetKind : uint32_t { Image = 1, Font = 2, Model = 3, Movie = 4 };
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
void Initialize(const std::filesystem::path& root);
void Reload();
void Shutdown();
uint64_t Generation();
std::filesystem::path Root();
ResolutionMode Mode();
std::vector<Diagnostic> Diagnostics();
std::optional<ResolvedAsset> Resolve(const AssetRequest& request);
// Providers are trusted host extensions, not DLLs loaded from mod packages.
// Shared ownership keeps an in-flight callback alive during unregistration.
bool RegisterProvider(AssetKind kind, std::shared_ptr<AssetProvider> provider);
void UnregisterProvider(AssetKind kind, const AssetProvider* provider);

// Zero-based extractor export index. Package separators/ASCII case are normalized;
// object case and UTF-8 bytes are preserved. Invalid identities return empty.
std::string MakeManifestKey(std::string_view package, uint32_t exportIndex, std::string_view object);
std::filesystem::path OverlayRelativePath(const AssetId& id);
}
