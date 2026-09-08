#pragma once

#include <filesystem>
#include <vector>
#include <xbox.h>

namespace DlcContent
{
struct Installed
{
    XCONTENT_DATA data{};
    std::filesystem::path root;
    uint32_t licenseMask = 0;
};

// A disc-set shares its installed DLC across all four starting discs.
std::filesystem::path SharedRoot(const std::filesystem::path& gamePath);
// Validate metadata and file locations/sizes, without re-hashing payload bytes.
// The transactional importer verifies those hashes before publishing a package.
std::vector<Installed> Discover(const std::filesystem::path& gamePath);
}
