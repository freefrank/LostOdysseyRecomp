#pragma once
// The published list of distribution shader packs and the choice of the one a
// runtime can read. Packs are assets of the "shader-packs" GitHub prerelease,
// named by renderer and contract; index.json on the same release lists them.
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace updater::shader_pack
{
inline constexpr std::string_view ReleaseTag = "shader-packs";
inline constexpr std::string_view IndexFileName = "index.json";
inline constexpr size_t MaxIndexBytes = 1u << 20;
inline constexpr uint64_t MaxPackBytes = 8ull << 30;
// The pipeline recipe corpus (gpu/pipeline_cache.h, file version 2) is listed
// like a pack under this renderer name; runtimes before it skip the entry. The
// contract is the SHA-256 of "lo-pipeline-recipes-v2": a new recipe format gets
// a new string, and the index keeps one corpus per format.
inline constexpr std::string_view CorpusRenderer = "pipeline-corpus";
inline constexpr std::string_view CorpusContract = "1d7f0a17386480c1fcfa5e912f7fb8db7359c5ceb6e8957b9b1045628233fec2";
inline constexpr std::string_view CorpusFileName = "pipelines_corpus.bin";
inline constexpr uint64_t MaxCorpusBytes = 64ull << 20;

struct IndexEntry
{
    std::string renderer; // vulkan, d3d12, metal or pipeline-corpus
    std::string contract; // 64 lowercase hex digits
    std::string file;     // asset name on the same release
    std::string sha256;   // 64 lowercase hex digits
    uint64_t size = 0;
};

std::optional<std::vector<IndexEntry>> ParseIndex(std::string_view text, std::string &error);
// Null when nothing matches. Duplicate matches are rejected by ParseIndex.
const IndexEntry *Select(const std::vector<IndexEntry> &entries, std::string_view renderer,
                         std::string_view contract);
bool SafeAssetName(std::string_view name);
bool LowerHex64(std::string_view text);

// The default index on the configured release repository, or a test override.
std::string IndexUrl(const char *overrideUrl);
// Assets are always fetched beside the index, never from a URL in its text.
std::string AssetUrl(std::string_view indexUrl, std::string_view file);

// One contract per line: the player chose to compile locally for it.
bool Declined(const std::filesystem::path &record, std::string_view contract);
bool RecordDecline(const std::filesystem::path &record, std::string_view contract, std::string &error);
}
