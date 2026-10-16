#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Language packs and model replacements: the game's text files with
// translations applied, and packages with replaced models, served in place of
// the originals. When the game opens an archive or index of a disc or DLC
// folder, every text file a pack translates (modding/text_format.h) and every
// package a mod replaces a model in (modding/model_import.h) is rebuilt and
// stored past the end of its archive; the index records of those files are
// pointed there with their new sizes. The game's loader takes offsets and
// sizes from the index, so rebuilt files may be longer. The rebuilt indexes and
// files are kept in cache/mods (LO_MODS_CACHE_DIR; none keeps them in memory)
// with a key of what they were made from, so later starts read them from there;
// each folder keeps the sets of its last two mod setups.
// See docs/notes/language-pack-research.zh-CN.md.
namespace modding::text_overlay
{
struct CachedFile;

// Bytes of a file that come from elsewhere: the patched index at offset 0, or
// the rebuilt files after the file's real end (2048-byte aligned), in memory
// or in a file of the mod cache.
struct Range
{
    uint64_t offset = 0, size = 0;
    std::shared_ptr<const std::vector<uint8_t>> bytes; // in memory, or
    std::shared_ptr<CachedFile> file;                  // a cache file
    std::shared_ptr<void> cachePin;                    // keeps the cache set while a handle holds this range
    // Copies count bytes from at (counted from offset); false if the cache file cannot be read.
    bool Read(uint64_t at, uint8_t *out, size_t count) const;
};

// For a file the game opens read-only. Empty unless it is an archive (.fpd)
// or index (.fpi) whose folder has translated text; the first call for a
// folder does the rebuilding.
std::vector<Range> RangesFor(const std::filesystem::path &file);

// Rebuilt files that would end past 4 GiB of their archive (the most the game
// can address in one) go into overflow archives the patched index adds
// (fpi_overflow.h). For an overflow archive of a folder whose files were
// rebuilt: its file in the mod cache, opened in its place; otherwise empty.
std::filesystem::path OverflowArchive(const std::filesystem::path &file, std::shared_ptr<void> *cachePin = nullptr);

// Whether a language pack has a translation for this key of this member
// (for host text that replaces game text, such as the Quit to Desktop row).
bool Translates(const std::string &memberPath, const std::string &key);

// The runtime prepares the rebuilt files of the other discs in the background
// after the first disc's (not on Android unless LO_MODS_PREBUILD=1;
// LO_MODS_PREBUILD=0 turns it off). Call before the game opens its archives.
void EnablePrebuild(std::chrono::seconds delay = std::chrono::seconds(30));
// Until the background preparation has nothing left (tools and tests).
void WaitForPrebuild();
// Stops the background preparation (at exit). What it has not finished stays
// out of the cache: a cache folder counts once its manifest is written.
void StopPrebuild();

// The selected language pack (mod_api.h LanguagePack id; empty for none). Its
// files replace other mods' translations of the same files. Set before the
// game opens its archives.
void SelectLanguagePack(const std::string &id);

// Live reload after the mod list changes in game (modding/hot_reload.h runs it).
// PrepareReload lists the mods again (call modding::Reload first) and, on a
// thread, prepares discFolder's rebuilt files for them: Ready, or Unchanged when
// the mods give the files in use. ArmReload makes the next open of that
// folder's LO.fpi switch to them (Committed): the game is about to reload its
// index, after closing every archive. The other discs' files are made again for
// the new mods. EndReload drops whatever is left (Idle).
enum class ReloadState { Idle, Preparing, Ready, Unchanged, Armed, Committed, Failed };
struct ReloadStatus
{
    ReloadState state = ReloadState::Idle;
    uint32_t done = 0, total = 0; // packages, while Preparing
    std::string error;            // Failed
};
void PrepareReload(const std::filesystem::path &discFolder);
ReloadStatus GetReloadStatus();
bool ArmReload(); // false unless Ready
void DisarmReload(); // back to Ready when the game did not reload its index
void EndReload();
}
