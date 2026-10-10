#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Language packs: the game's text files with translations applied, served in
// place of the originals. When the game opens an archive or index of a disc
// or DLC folder, every text file a pack translates is rebuilt (modding/
// text_format.h) and stored past the end of its archive; the index records
// of those files are pointed there with their new sizes. The game's loader
// takes offsets and sizes from the index, so translations may be longer.
// See docs/notes/language-pack-research.zh-CN.md.
namespace modding::text_overlay
{
// Bytes of a file that come from memory: the patched index at offset 0, or
// the rebuilt files after the file's real end (2048-byte aligned).
struct Range
{
    uint64_t offset = 0;
    std::shared_ptr<const std::vector<uint8_t>> bytes;
};

// For a file the game opens read-only. Empty unless it is an archive (.fpd)
// or index (.fpi) whose folder has translated text; the first call for a
// folder does the rebuilding.
std::vector<Range> RangesFor(const std::filesystem::path &file);

// Whether a language pack has a translation for this key of this member
// (for host text that replaces game text, such as the Quit to Desktop row).
bool Translates(const std::string &memberPath, const std::string &key);
}
