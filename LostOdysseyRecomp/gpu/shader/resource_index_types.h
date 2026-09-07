#pragma once
#include <cstdint>
#include <span>
#include <string_view>

namespace xenos::resources {
struct IndexEntry {
    uint64_t offset;
    uint32_t size;
    uint64_t hash;
    bool pixel;
};
struct IndexFile {
    std::string_view name;
    uint64_t size;
    uint64_t fingerprint;
    std::span<const IndexEntry> entries;
};
// Complete encoded-package identity; offsets refer to decoded microcode bytes.
// Empty entries certify a known package containing no supported containers.
struct CpxIndexPackage {
    std::string_view sha256;
    uint32_t storedSize;
    uint32_t decodedSize;
    std::span<const IndexEntry> entries;
};
// Location bindings describe a known archive layout, not whole-content integrity.
// UINT32_MAX denotes a known non-CPX extent.
struct CpxIndexExtent { uint64_t offset; uint32_t size; uint32_t package; };
struct CpxIndexArchive {
    std::string_view fpiSha256;
    std::string_view name;
    uint64_t size;
    std::span<const CpxIndexExtent> extents;
};

enum class ScanStage { CacheValidation, IndexedExtraction, FallbackScan };
enum class ScanUnit { Files, Bytes, Entries };
struct ScanProgress {
    ScanStage stage;
    uint64_t completed;
    uint64_t total;
    ScanUnit unit;
};
}
