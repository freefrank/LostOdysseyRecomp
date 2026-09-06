#pragma once
#include <filesystem>
#include <cstdint>

namespace DiscSet
{
// Installation validation, not an executable authenticator. The importer
// checks SHA256; here execution metadata prevents mounting another edition.
struct Identity { uint32_t edition = 0; uint32_t disc = 0; };
Identity ReadIdentity(const std::filesystem::path& directory);
bool Validate(const std::filesystem::path& directory, Identity expected);
}
