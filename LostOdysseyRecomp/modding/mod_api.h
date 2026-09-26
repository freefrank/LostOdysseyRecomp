#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace modding {
inline constexpr uint32_t kModApiVersion = 1;

enum class AssetKind : uint32_t { Image = 1, Font = 2, Model = 3, Movie = 4 };

struct AssetId {
    AssetKind kind = AssetKind::Image;
    std::string key;
};

struct AssetRequest {
    AssetId id;
    std::filesystem::path originalPath;
};

struct ResolvedAsset {
    AssetId id;
    std::filesystem::path path;
    std::string modId;
    int32_t priority = 0;
};

class AssetProvider {
public:
    virtual ~AssetProvider() = default;
    virtual std::optional<ResolvedAsset> Resolve(const AssetRequest& request) = 0;
};

void Initialize(const std::filesystem::path& root);
void Shutdown();
std::optional<ResolvedAsset> Resolve(const AssetRequest& request);
bool RegisterProvider(AssetKind kind, AssetProvider* provider);
void UnregisterProvider(AssetKind kind, AssetProvider* provider);

// Canonical v1 key derived from the extractor manifest. This is intentionally
// independent of image_path so regenerated extraction folders do not break mods.
std::string MakeManifestKey(std::string_view package, uint32_t exportIndex, std::string_view object);
}
