#include "text_format.h"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace modding::text
{
namespace
{
void Require(bool ok, const char *message)
{
    if (!ok) throw std::runtime_error(message);
}

uint32_t Be32(std::span<const uint8_t> b, size_t p)
{
    Require(p <= b.size() && 4 <= b.size() - p, "read out of bounds");
    return uint32_t(b[p]) << 24 | uint32_t(b[p + 1]) << 16 | uint32_t(b[p + 2]) << 8 | b[p + 3];
}
uint16_t Be16(std::span<const uint8_t> b, size_t p)
{
    Require(p <= b.size() && 2 <= b.size() - p, "read out of bounds");
    return uint16_t(b[p] << 8 | b[p + 1]);
}
void PutBe32(std::vector<uint8_t> &out, uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(uint8_t(value >> shift));
}
void SetBe32(std::vector<uint8_t> &out, size_t p, uint32_t value)
{
    for (int i = 0; i < 4; ++i) out[p + i] = uint8_t(value >> (24 - 8 * i));
}
void SetBe16(std::vector<uint8_t> &out, size_t p, uint16_t value)
{
    out[p] = uint8_t(value >> 8);
    out[p + 1] = uint8_t(value);
}

const std::string *Find(const Replacements &replacements, const std::string &key)
{
    const auto it = replacements.find(key);
    return it == replacements.end() ? nullptr : &it->second;
}

// Both formats end strings at the first NUL, so a translation may not contain one.
std::u16string DecodeString(const std::string &text)
{
    auto units = DecodeText(text);
    Require(units.find(u'\0') == std::u16string::npos, "a translation contains a NUL character");
    return units;
}

bool Blank(std::u16string_view units) { return units.find_first_not_of(u' ') == std::u16string_view::npos; }

// Both formats store each distinct string once and point at it from every
// place that shows it. Keys belong to those places (sites), so two text IDs
// that share a string in one language can still be translated apart.
struct Site
{
    std::string key; // empty: a name the game looks up; it keeps the original string
    size_t string = 0;
};

// Strings after replacements: a string keeps its original text while any site
// (or a name, `fixed`) still shows it, otherwise it takes its first site's
// text; a site whose text differs points to a copy after the original strings.
// Identical texts share one copy, and without changes nothing is copied, so
// the file comes back byte for byte.
struct Placement
{
    std::vector<std::u16string> strings; // the originals' slots, then the copies
    std::vector<size_t> target;          // per site
    size_t known = 0;                    // replacement keys a site used
};

Placement Place(const std::vector<std::u16string> &originals, const std::vector<bool> &fixed,
                const std::vector<Site> &sites, const Replacements &replacements)
{
    Placement placement;
    placement.strings = originals;
    placement.target.resize(sites.size());
    std::vector<std::u16string> texts(sites.size());
    auto keep = fixed;
    for (size_t i = 0; i < sites.size(); ++i)
    {
        const auto s = sites[i].string;
        const auto text = sites[i].key.empty() ? nullptr : Find(replacements, sites[i].key);
        placement.known += text != nullptr;
        texts[i] = text ? DecodeString(*text) : originals[s];
        if (texts[i] == originals[s]) keep[s] = true;
    }
    std::vector<bool> claimed(originals.size(), false);
    for (size_t i = 0; i < sites.size(); ++i)
    {
        const auto s = sites[i].string;
        if (keep[s] || claimed[s]) continue;
        placement.strings[s] = texts[i];
        claimed[s] = true;
    }
    std::map<std::u16string, size_t> copies;
    for (size_t i = 0; i < sites.size(); ++i)
    {
        const auto s = sites[i].string;
        if (texts[i] == placement.strings[s]) { placement.target[i] = s; continue; }
        const auto [it, added] = copies.emplace(texts[i], placement.strings.size());
        if (added) placement.strings.push_back(texts[i]);
        placement.target[i] = it->second;
    }
    return placement;
}

std::vector<Entry> SiteEntries(const std::vector<std::u16string> &strings, const std::vector<Site> &sites)
{
    std::vector<Entry> entries;
    for (const auto &site : sites)
        if (!site.key.empty() && !Blank(strings[site.string]))
            entries.push_back({site.key, EncodeUnits(strings[site.string])});
    return entries;
}

// ---- menu_<l>.dat: "DAT$" relocatable block, big-endian ----
// Header: +0x14 section count, +0x18 data base, +0x28 string pool, +0x2c pool size, +0x30 relocation
// table, +0x34 relocation count. Sections at 0x40, 16 bytes each: name (pool pointer), data offset from
// the data base, record count, u16 record size. Each relocation is (type << 24 | file offset of a u32).
// Type 0x22 holds a pool offset to a UTF-16BE string; type 0x32 points into the data region (stored
// little-endian) and never moves. The pool is padded with zeros to 16 bytes.
//
// Keys: "id.<text id>" for MStringDescData (the game looks texts up by ID), "<section>.<record>.<field
// offset>" elsewhere. Section names and the MFontMap / MSpriteDescData entries (language codes, font
// and sprite names) are identifiers the loader looks up, so they are not exported.

constexpr size_t kSectionTable = 0x40;
constexpr uint32_t kPoolPointer = 0x22, kDataPointer = 0x32;
constexpr size_t kPoolAlignment = 16;

struct MenuDat
{
    uint32_t poolOffset = 0, poolSize = 0, relocOffset = 0;
    std::vector<uint32_t> offsets;       // pool offset per string, pool order, padding excluded
    std::vector<std::u16string> strings;
    std::vector<bool> fixed;             // a section, font or sprite name uses the string
    std::vector<Site> sites;             // every pool pointer, in relocation order
    std::vector<uint32_t> positions;     // file offset of each site's u32
};

MenuDat ReadMenuDat(std::span<const uint8_t> d)
{
    Require(d.size() >= kSectionTable && d[0] == 'D' && d[1] == 'A' && d[2] == 'T' && d[3] == '$', "not a DAT$ block");
    Require(Be32(d, 0x10) == kSectionTable, "unexpected DAT$ section table offset");
    MenuDat dat;
    const uint64_t sections = Be32(d, 0x14), dataBase = Be32(d, 0x18);
    dat.poolOffset = Be32(d, 0x28);
    dat.poolSize = Be32(d, 0x2c);
    dat.relocOffset = Be32(d, 0x30);
    const uint64_t relocCount = Be32(d, 0x34);
    const uint64_t sectionsEnd = kSectionTable + 16 * sections;
    Require(sectionsEnd <= dataBase && dataBase <= dat.poolOffset, "invalid DAT$ layout");
    Require(uint64_t(dat.poolOffset) + dat.poolSize == dat.relocOffset && dat.poolSize % kPoolAlignment == 0,
            "invalid DAT$ string pool");
    Require(uint64_t(dat.relocOffset) + 4 * relocCount == d.size(), "invalid DAT$ relocation table");

    // Pool strings in physical order.
    const auto pool = d.subspan(dat.poolOffset, dat.poolSize);
    std::map<uint32_t, size_t> at;
    for (size_t o = 0; o < pool.size();)
    {
        size_t e = o;
        while (e + 1 < pool.size() && (pool[e] | pool[e + 1])) e += 2;
        Require(e + 1 < pool.size(), "unterminated DAT$ string");
        at.emplace(uint32_t(o), dat.strings.size());
        dat.offsets.push_back(uint32_t(o));
        dat.strings.push_back(ReadUtf16Be(pool.subspan(o, e - o)));
        o = e + 2;
    }
    auto stringAt = [&](uint32_t value) {
        const auto it = at.find(value);
        Require(it != at.end(), "DAT$ pointer is not at the start of a string");
        return it->second;
    };
    dat.fixed.assign(dat.strings.size(), false);
    std::vector<bool> sectionName(dat.strings.size(), false), usedByData(dat.strings.size(), false);

    struct Section
    {
        uint64_t begin = 0, count = 0, size = 0;
        bool texts = false; // MStringDescData: +0 text ID, +8 string
        bool names = false; // MFontMap (language codes, font names), MSpriteDescData (sprite names)
    };
    std::vector<Section> table;
    for (uint64_t i = 0; i < sections; ++i)
    {
        const size_t p = kSectionTable + 16 * i;
        Section section{dataBase + Be32(d, p + 4), Be32(d, p + 8), Be16(d, p + 12)};
        Require(section.size > 0 && section.begin + section.count * section.size <= dat.poolOffset, "invalid DAT$ section");
        const auto &name = dat.strings[stringAt(Be32(d, p))];
        section.texts = name == u"MStringDescData";
        section.names = name == u"MFontMap" || name == u"MSpriteDescData";
        Require(!section.texts || section.size >= 12, "invalid MStringDescData record");
        table.push_back(section);
    }
    // The game looks texts up by ID; an ID that repeats gets #2, #3 in record order.
    std::map<std::pair<size_t, uint64_t>, std::string> idKeys;
    for (size_t s = 0; s < table.size(); ++s)
    {
        if (!table[s].texts) continue;
        std::map<uint32_t, int> seen;
        for (uint64_t r = 0; r < table[s].count; ++r)
        {
            const auto id = Be32(d, size_t(table[s].begin + r * table[s].size));
            auto key = "id." + std::to_string(id);
            if (const int n = ++seen[id]; n > 1) key += "#" + std::to_string(n);
            idKeys.emplace(std::pair(s, r), std::move(key));
        }
    }

    for (uint64_t i = 0; i < relocCount; ++i)
    {
        const auto raw = Be32(d, size_t(dat.relocOffset + 4 * i));
        const uint32_t type = raw >> 24, pos = raw & 0xffffff;
        Require(pos % 4 == 0 && pos >= kSectionTable && uint64_t(pos) + 4 <= dat.poolOffset, "invalid DAT$ relocation");
        if (type == kDataPointer) continue;
        Require(type == kPoolPointer, "unknown DAT$ relocation type");
        const auto index = stringAt(Be32(d, pos));
        Site site{{}, index};
        if (pos < sectionsEnd)
        {
            Require((pos - kSectionTable) % 16 == 0, "unexpected DAT$ section table pointer");
            sectionName[index] = dat.fixed[index] = true;
        }
        else
        {
            usedByData[index] = true;
            const auto section = std::find_if(table.begin(), table.end(), [&](const Section &s) {
                return pos >= s.begin && pos < s.begin + s.count * s.size;
            });
            if (section == table.end()) site.key = "p." + std::to_string(pos);
            else if (section->names) dat.fixed[index] = true;
            else
            {
                const auto record = (pos - section->begin) / section->size, field = (pos - section->begin) % section->size;
                site.key = section->texts && field == 8 ? idKeys.at({size_t(section - table.begin()), record}) :
                           std::to_string(section - table.begin()) + "." + std::to_string(record) + "." + std::to_string(field);
            }
        }
        dat.sites.push_back(std::move(site));
        dat.positions.push_back(pos);
    }

    // Section names are loader identifiers, not text; the loader would not find a translated one.
    size_t contentEnd = 0;
    for (size_t i = 0; i < dat.strings.size(); ++i)
    {
        Require(!(sectionName[i] && usedByData[i]), "DAT$ section name shared with data");
        if (sectionName[i] || usedByData[i] || !dat.strings[i].empty())
            contentEnd = dat.offsets[i] + 2 * dat.strings[i].size() + 2;
    }
    // Unused empty strings after the last real one are the pool's alignment padding.
    Require((contentEnd + kPoolAlignment - 1) / kPoolAlignment * kPoolAlignment == dat.poolSize, "unexpected DAT$ pool padding");
    while (!dat.offsets.empty() && dat.offsets.back() >= contentEnd)
    {
        dat.offsets.pop_back();
        dat.strings.pop_back();
        dat.fixed.pop_back();
    }
    return dat;
}

// ---- battle/text/*data.bin: big-endian string banks ----
// +0 end of the records, +4 string table, +8 string count, +12 1 (strings are UTF-16).
// [16, +0): one record per game ID, {u8 flag (nonzero: the game skips it), u8, u16 string index...}.
// At +0 a layout descriptor: u32, then field sizes "1 1 2 [2...]" and a 0: one index for the explain
// banks, seven name columns for namedata. Table: (offset from the table, byte length incl. NUL) per
// string, then the UTF-16BE strings.
//
// Keys are game IDs, "<record>" or "<record>.<column>". Table indices are not stable: each language
// stores identical strings once, so the string count differs (itemexplaindata 664-671), while the
// records are the same in every language and in the DLC copies.

struct StringBank
{
    uint32_t table = 0, first = 0;            // first string, relative to the table
    size_t stringsEnd = 0;                    // absolute
    std::vector<std::u16string> strings;
    std::vector<Site> sites;                  // every used record column, in record order
    std::vector<size_t> positions;            // file offset of each site's u16 index
};

StringBank ReadStringBank(std::span<const uint8_t> d)
{
    StringBank bank;
    const uint32_t records = Be32(d, 0);
    bank.table = Be32(d, 4);
    const uint64_t count = Be32(d, 8);
    Require(Be32(d, 12) == 1, "unsupported string bank encoding");
    Require(records >= 16 && uint64_t(records) + 4 < bank.table && bank.table <= d.size() &&
                count <= (d.size() - bank.table) / 8, "invalid string bank header");

    std::vector<uint8_t> fields;
    size_t p = records + 4;
    while (p < bank.table && d[p]) fields.push_back(d[p++]);
    Require(p < bank.table, "unterminated string bank layout");
    for (; p < bank.table; ++p) Require(d[p] == 0, "unexpected string bank layout");
    Require(fields.size() >= 3 && fields[0] == 1 && fields[1] == 1, "unexpected string bank layout");
    for (size_t i = 2; i < fields.size(); ++i) Require(fields[i] == 2, "unexpected string bank layout");
    const size_t columns = fields.size() - 2, width = 2 + 2 * columns;
    Require((records - 16) % width == 0, "string bank records do not fill their area");

    // Contiguous strings in table order, so a rebuild reproduces them exactly.
    bank.first = count ? Be32(d, bank.table) : uint32_t(8 * count);
    Require(bank.first >= 8 * count, "string bank strings overlap the table");
    uint64_t pos = bank.first;
    for (uint64_t i = 0; i < count; ++i)
    {
        const auto offset = Be32(d, size_t(bank.table + 8 * i)), length = Be32(d, size_t(bank.table + 8 * i + 4));
        Require(offset == pos && length >= 2 && length % 2 == 0 && bank.table + pos + length <= d.size(),
                "invalid string bank table");
        const auto bytes = d.subspan(size_t(bank.table + pos), length);
        Require(bytes[length - 2] == 0 && bytes[length - 1] == 0, "unterminated string bank string");
        bank.strings.push_back(ReadUtf16Be(bytes.first(length - 2)));
        pos += length;
    }
    bank.stringsEnd = size_t(bank.table + pos);

    for (size_t r = 0; r < (records - 16) / width; ++r)
    {
        const size_t o = 16 + r * width;
        if (d[o]) continue;
        for (size_t c = 0; c < columns; ++c)
        {
            const auto index = Be16(d, o + 2 + 2 * c);
            Require(index < count, "string bank record points past the table");
            bank.sites.push_back({columns == 1 ? std::to_string(r) : std::to_string(r) + "." + std::to_string(c), index});
            bank.positions.push_back(o + 2 + 2 * c);
        }
    }
    return bank;
}
} // namespace

std::vector<Entry> ParseMenuDat(std::span<const uint8_t> member)
{
    const auto dat = ReadMenuDat(member);
    return SiteEntries(dat.strings, dat.sites);
}

std::vector<uint8_t> RebuildMenuDat(std::span<const uint8_t> member, const Replacements &replacements, size_t *unknownKeys)
{
    const auto dat = ReadMenuDat(member);
    const auto placement = Place(dat.strings, dat.fixed, dat.sites, replacements);
    if (unknownKeys) *unknownKeys = replacements.size() - placement.known;
    // Repack the pool (original order, then copies); only the pool pointers and two header words change.
    std::vector<uint8_t> pool;
    std::vector<uint32_t> offsets;
    for (const auto &units : placement.strings)
    {
        offsets.push_back(uint32_t(pool.size()));
        AppendUtf16Be(pool, units);
        pool.insert(pool.end(), 2, 0);
    }
    pool.resize((pool.size() + kPoolAlignment - 1) / kPoolAlignment * kPoolAlignment, 0);
    Require(uint64_t(dat.poolOffset) + pool.size() <= UINT32_MAX, "DAT$ string pool too large");

    std::vector<uint8_t> out(member.begin(), member.begin() + dat.poolOffset);
    for (size_t i = 0; i < dat.sites.size(); ++i) SetBe32(out, dat.positions[i], offsets[placement.target[i]]);
    SetBe32(out, 0x2c, uint32_t(pool.size()));
    SetBe32(out, 0x30, uint32_t(dat.poolOffset + pool.size()));
    out.insert(out.end(), pool.begin(), pool.end());
    out.insert(out.end(), member.begin() + dat.relocOffset, member.end());
    return out;
}

std::vector<Entry> ParseStringBank(std::span<const uint8_t> member)
{
    const auto bank = ReadStringBank(member);
    return SiteEntries(bank.strings, bank.sites);
}

std::vector<uint8_t> RebuildStringBank(std::span<const uint8_t> member, const Replacements &replacements, size_t *unknownKeys)
{
    const auto bank = ReadStringBank(member);
    const auto placement = Place(bank.strings, std::vector<bool>(bank.strings.size(), false), bank.sites, replacements);
    if (unknownKeys) *unknownKeys = replacements.size() - placement.known;
    Require(placement.strings.size() <= 0xffff, "string bank has too many strings");
    // Copies extend the table (count at +8, records repointed); a gap before the first string stays.
    const uint32_t first = uint32_t(8 * placement.strings.size()) + (bank.first - uint32_t(8 * bank.strings.size()));
    std::vector<uint8_t> table, strings;
    for (const auto &units : placement.strings)
    {
        const auto begin = strings.size();
        AppendUtf16Be(strings, units);
        strings.insert(strings.end(), 2, 0);
        Require(first + strings.size() <= UINT32_MAX, "string bank too large");
        PutBe32(table, uint32_t(first + begin));
        PutBe32(table, uint32_t(strings.size() - begin));
    }
    // Header, records and layout as they were; any gap before the first string and trailing bytes too.
    std::vector<uint8_t> out(member.begin(), member.begin() + bank.table);
    SetBe32(out, 8, uint32_t(placement.strings.size()));
    for (size_t i = 0; i < bank.sites.size(); ++i) SetBe16(out, bank.positions[i], uint16_t(placement.target[i]));
    out.insert(out.end(), table.begin(), table.end());
    out.insert(out.end(), member.begin() + bank.table + 8 * bank.strings.size(), member.begin() + bank.table + bank.first);
    out.insert(out.end(), strings.begin(), strings.end());
    out.insert(out.end(), member.begin() + bank.stringsEnd, member.end());
    return out;
}
}
