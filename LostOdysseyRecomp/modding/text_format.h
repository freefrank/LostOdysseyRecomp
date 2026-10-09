#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// The game's text files as translatable entries, and the same files rebuilt
// with translations. --export-assets (text) and language packs share this, so
// an exported key always finds its string again. See
// docs/notes/language-pack-research.zh-CN.md for the formats.
namespace modding::text
{
enum class Format : uint8_t
{
    None,
    MenuDat,    // loc/<l>/menu/menu_<l>.dat: DAT$ block, strings by text ID
    StringBank, // loc/<l>/battle/text/*data.bin: names and descriptions
    Jmd,        // loc/<l>/battle/text/*.jmd, scr/mes/<l>/*.jmd: messages and dialogue
    Subtitle,   // event/subtitle/<l>/[<voice>/]*.sub: cutscene subtitles
    StaffRoll,  // loc/<l>/staffroll/roll_<l>.bin: credits script
    Coalesced,  // rpgame/localization/coalesced.<l>: UE3 localized INI files
};

// path: the member path as the FPI names it, lower case with '/'
// (bin/xenon/loc/int/menu/menu_int.dat). None for anything else.
Format Detect(std::string_view path);
const char *Name(Format format);
// The three-letter language folder or suffix of a text member ("int"), or empty.
std::string Language(std::string_view path);

// key: stable within the member, and the same across languages where the
// format numbers its strings (text IDs, table indices). text: UTF-8 with the
// codes that are not text written as tokens (EncodeUnits).
struct Entry
{
    std::string key;
    std::string text;
};
using Replacements = std::unordered_map<std::string, std::string>;

// member: decoded bytes (CPX already unpacked). Throws std::runtime_error on
// data the format reader does not understand.
std::vector<Entry> Parse(Format format, std::span<const uint8_t> member);
// Rebuilds member with replacements applied: keys it does not contain keep the
// original text, keys the member does not have are ignored and counted in
// *unknownKeys. Rebuild(f, m, {}) and Rebuild(f, m, <Parse(f, m) as a map>)
// both return m byte for byte. Throws std::runtime_error on malformed input or
// a replacement whose tokens do not decode.
std::vector<uint8_t> Rebuild(Format format, std::span<const uint8_t> member, const Replacements &replacements,
                             size_t *unknownKeys = nullptr);

// UTF-16 code units -> UTF-8 text with tokens, losslessly (DecodeText reverses it):
//  - Unicode scalar values become UTF-8, except '{', C0 controls other than
//    '\n', private-use units (U+E000..U+F8FF), U+FFFE/U+FFFF and lone
//    surrogates; each of those becomes {XXXX} (four upper-case hex digits).
//  - A private-use code listed in withParameter also takes the unit after it:
//    {E10D:0500}. The game reads that unit as a number, not as a character.
std::string EncodeUnits(std::u16string_view units, std::span<const char16_t> withParameter = {});
// Throws std::runtime_error on malformed UTF-8 or a malformed token.
std::u16string DecodeText(std::string_view text);

// Big-endian UTF-16 helpers for the format readers.
std::u16string ReadUtf16Be(std::span<const uint8_t> bytes);
void AppendUtf16Be(std::vector<uint8_t> &out, std::u16string_view units);

// Per-format readers. MenuDat and StringBank live in text_menu.cpp, the rest
// in text_format.cpp; Parse/Rebuild dispatch to them.
std::vector<Entry> ParseMenuDat(std::span<const uint8_t> member);
std::vector<uint8_t> RebuildMenuDat(std::span<const uint8_t> member, const Replacements &replacements, size_t *unknownKeys);
std::vector<Entry> ParseStringBank(std::span<const uint8_t> member);
std::vector<uint8_t> RebuildStringBank(std::span<const uint8_t> member, const Replacements &replacements, size_t *unknownKeys);
}
