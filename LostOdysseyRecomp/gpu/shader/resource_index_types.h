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
}
