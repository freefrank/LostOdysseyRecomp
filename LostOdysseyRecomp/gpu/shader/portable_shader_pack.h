#pragma once

#include "resource_cpx_index_sha256.h"
#include "xenos_translator.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// A read-only, relocatable distribution artifact. NOT a replacement for the
// compiler-specific local success/failure caches. No DXC, GPU or game-data
// dependency in this library. All integers on disk are explicitly little endian.
namespace xenos::portable_pack {
using Digest = resources::Sha256Digest;
inline constexpr uint32_t Schema = 1;
inline constexpr uint32_t MaxRecords = 100000;
inline constexpr uint32_t MaxShaderBytes = 16u << 20;
inline constexpr uint32_t TargetBlockBytes = 1u << 20;
inline constexpr uint64_t MaxFileBytes = 8ull << 30;
inline constexpr std::string_view FileName = "portable_vk.lospv";

// Deliberately excludes local DXC binary hashes, installation paths and host
// CPU/OS. Layout revision and options are mandatory compatibility components.
Digest Contract(uint32_t translatorVersion, std::string_view options,
    std::string_view variant, std::string_view commonHlsl,
    std::string_view discovery, std::span<const uint8_t> loadedXex,
    uint32_t layoutRevision = 1);
std::filesystem::path DefaultPath(); // executable-dir/shaders/portable_vk.lospv

struct Record {
    uint64_t hash = 0;
    TranslatedShader info; // hlsl/errors are always empty when read
    std::vector<uint8_t> binary;
};
struct Report {
    uint32_t records = 0, uniqueBinaries = 0, blocks = 0, failuresOmitted = 0;
    uint64_t binaryBytes = 0, uniqueBinaryBytes = 0, compressedBytes = 0;
    uint64_t fileBytes = 0, indexBytes = 0, hlslBytesOmitted = 0, diagnosticBytesOmitted = 0;
    Digest contract{};
    std::string producer; // provenance, never compared with the local compiler
};

class Reader;
class Writer {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    Writer(const std::filesystem::path&, Digest contract, std::string_view producer,
           int compressionLevel = 9);
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    // Only successful SPIR-V binaries; text is counted but never serialized.
    void Add(uint64_t hash, const TranslatedShader&, std::span<const uint8_t> binary);
    void OmitFailure(size_t hlslBytes, size_t diagnosticBytes);
    // Offline re-export of existing records, preserving metadata and omitted counts.
    // Import into an empty writer before appending new records.
    void Import(Reader& source);
    Report Finish(); // atomic publication; destruction before Finish discards temp
};

class Reader {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    // Reads and validates the small index only. Payload blocks are integrity-checked
    // by SHA-256 and bounded-decompressed on first use. One block is retained.
    Reader(const std::filesystem::path&, const Digest& expectedContract);
    ~Reader();
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;
    std::optional<Record> Get(bool pixel, uint64_t hash);
    bool Contains(bool pixel, uint64_t hash) const; // index only; no payload I/O
    const Report& Info() const;
    void VerifyAll(); // offline release validation; does not create GPU modules
    uint64_t PayloadReadBytes() const;
    // Inspection is structural only, NOT an authorization/compatibility check.
    static Report Inspect(const std::filesystem::path&, bool verifyPayloads = false);
    friend class Writer;
};
} // namespace xenos::portable_pack
