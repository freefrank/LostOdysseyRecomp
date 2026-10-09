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

// ---- menu_<l>.dat: "DAT$" relocatable block, big-endian ----
// Header: +0x14 section count, +0x18 data base, +0x28 string pool, +0x2c pool size, +0x30 relocation
// table, +0x34 relocation count. Sections at 0x40, 16 bytes each: name (pool pointer), data offset from
// the data base, record count, u16 record size. Each relocation is (type << 24 | file offset of a u32).
// Type 0x22 holds a pool offset to a UTF-16BE string; type 0x32 points into the data region (stored
// little-endian) and never moves. The pool is padded with zeros to 16 bytes.

constexpr size_t kSectionTable = 0x40;
constexpr uint32_t kPoolPointer = 0x22, kDataPointer = 0x32;
constexpr size_t kPoolAlignment = 16;

struct PoolString
{
    uint32_t offset = 0;
    std::u16string units;
    std::string key;           // empty: not exported (names, strings no text pointer uses)
    bool sectionName = false;
    bool usedByData = false;
    bool usedByName = false;   // also a font or sprite name, which must not change
};

struct PoolPointer
{
    uint32_t pos = 0;  // file offset of the u32
    size_t string = 0;
    bool name = false; // in MFontMap or MSpriteDescData
};

struct MenuDat
{
    uint32_t poolOffset = 0, poolSize = 0, relocOffset = 0;
    std::vector<PoolString> strings;          // pool order, padding excluded
    std::vector<size_t> exported;             // strings indices, in order of first reference
    std::vector<PoolPointer> pointers;        // every pool pointer outside the section table
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
        dat.strings.push_back({uint32_t(o), ReadUtf16Be(pool.subspan(o, e - o)), {}});
        o = e + 2;
    }
    auto stringAt = [&](uint32_t value) {
        const auto it = at.find(value);
        Require(it != at.end(), "DAT$ pointer is not at the start of a string");
        return it->second;
    };

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
        const auto &name = dat.strings[stringAt(Be32(d, p))].units;
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

    // A string's key comes from its first pointer in relocation order.
    for (uint64_t i = 0; i < relocCount; ++i)
    {
        const auto raw = Be32(d, size_t(dat.relocOffset + 4 * i));
        const uint32_t type = raw >> 24, pos = raw & 0xffffff;
        Require(pos % 4 == 0 && pos >= kSectionTable && uint64_t(pos) + 4 <= dat.poolOffset, "invalid DAT$ relocation");
        if (type == kDataPointer) continue;
        Require(type == kPoolPointer, "unknown DAT$ relocation type");
        const auto index = stringAt(Be32(d, pos));
        auto &string = dat.strings[index];
        if (pos < sectionsEnd)
        {
            Require((pos - kSectionTable) % 16 == 0, "unexpected DAT$ section table pointer");
            string.sectionName = true;
            dat.pointers.push_back({pos, index, true});
            continue;
        }
        string.usedByData = true;
        const auto section = std::find_if(table.begin(), table.end(), [&](const Section &s) {
            return pos >= s.begin && pos < s.begin + s.count * s.size;
        });
        dat.pointers.push_back({pos, index, section != table.end() && section->names});
        if (dat.pointers.back().name)
        {
            string.usedByName = true;
            continue;
        }
        if (!string.key.empty()) continue;
        if (section != table.end())
        {
            const auto record = (pos - section->begin) / section->size, field = (pos - section->begin) % section->size;
            if (section->texts && field == 8) string.key = idKeys.at({size_t(section - table.begin()), record});
            else string.key = std::to_string(section - table.begin()) + "." + std::to_string(record) + "." + std::to_string(field);
        }
        else string.key = "p." + std::to_string(string.offset);
        dat.exported.push_back(index);
    }

    // Section names are loader identifiers, not text; the loader would not find a translated one.
    size_t contentEnd = 0;
    for (const auto &string : dat.strings)
    {
        Require(!(string.sectionName && string.usedByData), "DAT$ section name shared with data");
        if (string.sectionName || string.usedByData || !string.units.empty())
            contentEnd = string.offset + 2 * string.units.size() + 2;
    }
    // Unused empty strings after the last real one are the pool's alignment padding.
    Require((contentEnd + kPoolAlignment - 1) / kPoolAlignment * kPoolAlignment == dat.poolSize, "unexpected DAT$ pool padding");
    while (!dat.strings.empty() && dat.strings.back().offset >= contentEnd) dat.strings.pop_back();
    return dat;
}

// ---- battle/text/*data.bin: big-endian string banks ----
// +0 end of the records, +4 string table, +8 string count, +12 1 (strings are UTF-16).
// [16, +0): one record per game ID, {u8 flag (nonzero: the game skips it), u8, u16 string index...}.
// At +0 a layout descriptor: u32, then field sizes "1 1 2 [2...]" and a 0: one index for the explain
// banks, seven name columns for namedata. Table: (offset from the table, byte length incl. NUL) per
// string, then the UTF-16BE strings.
//
// Keys are game IDs, "<record>" or "<record>.<column>" of the first record that uses a string. Table
// indices are not stable: each language stores identical strings once, so the string count differs
// (itemexplaindata 664-671), while the records are the same in every language and in the DLC copies.

struct StringBank
{
    uint32_t table = 0, first = 0;            // first string, relative to the table
    size_t stringsEnd = 0;                    // absolute
    std::vector<std::span<const uint8_t>> strings; // without the NUL
    std::vector<std::string> keys;            // per string; empty when no record uses it
    std::vector<size_t> exported;             // string indices, in order of first use
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
        bank.strings.push_back(bytes.first(length - 2));
        pos += length;
    }
    bank.stringsEnd = size_t(bank.table + pos);

    bank.keys.resize(bank.strings.size());
    for (size_t r = 0; r < (records - 16) / width; ++r)
    {
        const size_t o = 16 + r * width;
        if (d[o]) continue;
        for (size_t c = 0; c < columns; ++c)
        {
            const auto index = Be16(d, o + 2 + 2 * c);
            Require(index < count, "string bank record points past the table");
            if (!bank.keys[index].empty()) continue;
            bank.keys[index] = columns == 1 ? std::to_string(r) : std::to_string(r) + "." + std::to_string(c);
            bank.exported.push_back(index);
        }
    }
    return bank;
}
} // namespace

std::vector<Entry> ParseMenuDat(std::span<const uint8_t> member)
{
    const auto dat = ReadMenuDat(member);
    std::vector<Entry> entries;
    for (const auto index : dat.exported) entries.push_back({dat.strings[index].key, EncodeUnits(dat.strings[index].units)});
    return entries;
}

std::vector<uint8_t> RebuildMenuDat(std::span<const uint8_t> member, const Replacements &replacements, size_t *unknownKeys)
{
    const auto dat = ReadMenuDat(member);
    // Repack the pool in its original order; only the pool pointers and two header words change.
    // A translated string that is also a font or sprite name keeps its slot for the names, and
    // the text pointers move to a translated copy after the last string.
    std::vector<uint8_t> pool;
    std::vector<uint32_t> moved(dat.strings.size()), translated(dat.strings.size());
    std::vector<std::pair<size_t, std::u16string>> copies;
    size_t known = 0;
    for (size_t i = 0; i < dat.strings.size(); ++i)
    {
        const auto &string = dat.strings[i];
        moved[i] = translated[i] = uint32_t(pool.size());
        const auto text = string.key.empty() ? nullptr : Find(replacements, string.key);
        auto units = text ? DecodeString(*text) : string.units;
        known += text != nullptr;
        if (string.usedByName && units != string.units)
        {
            copies.emplace_back(i, std::move(units));
            units = string.units;
        }
        AppendUtf16Be(pool, units);
        pool.insert(pool.end(), 2, 0);
    }
    for (const auto &[i, units] : copies)
    {
        translated[i] = uint32_t(pool.size());
        AppendUtf16Be(pool, units);
        pool.insert(pool.end(), 2, 0);
    }
    pool.resize((pool.size() + kPoolAlignment - 1) / kPoolAlignment * kPoolAlignment, 0);
    Require(uint64_t(dat.poolOffset) + pool.size() <= UINT32_MAX, "DAT$ string pool too large");
    if (unknownKeys) *unknownKeys = replacements.size() - known;

    std::vector<uint8_t> out(member.begin(), member.begin() + dat.poolOffset);
    for (const auto &pointer : dat.pointers)
        SetBe32(out, pointer.pos, pointer.name ? moved[pointer.string] : translated[pointer.string]);
    SetBe32(out, 0x2c, uint32_t(pool.size()));
    SetBe32(out, 0x30, uint32_t(dat.poolOffset + pool.size()));
    out.insert(out.end(), pool.begin(), pool.end());
    out.insert(out.end(), member.begin() + dat.relocOffset, member.end());
    return out;
}

std::vector<Entry> ParseStringBank(std::span<const uint8_t> member)
{
    const auto bank = ReadStringBank(member);
    std::vector<Entry> entries;
    for (const auto index : bank.exported) entries.push_back({bank.keys[index], EncodeUnits(ReadUtf16Be(bank.strings[index]))});
    return entries;
}

std::vector<uint8_t> RebuildStringBank(std::span<const uint8_t> member, const Replacements &replacements, size_t *unknownKeys)
{
    const auto bank = ReadStringBank(member);
    std::vector<uint8_t> table, strings;
    size_t known = 0;
    for (size_t i = 0; i < bank.strings.size(); ++i)
    {
        const auto text = bank.keys[i].empty() ? nullptr : Find(replacements, bank.keys[i]);
        const auto begin = strings.size();
        if (text)
        {
            ++known;
            AppendUtf16Be(strings, DecodeString(*text));
        }
        else strings.insert(strings.end(), bank.strings[i].begin(), bank.strings[i].end());
        strings.insert(strings.end(), 2, 0);
        Require(bank.first + strings.size() <= UINT32_MAX, "string bank too large");
        PutBe32(table, uint32_t(bank.first + begin));
        PutBe32(table, uint32_t(strings.size() - begin));
    }
    if (unknownKeys) *unknownKeys = replacements.size() - known;
    // Header, records and layout as they were; any gap before the first string and trailing bytes too.
    std::vector<uint8_t> out(member.begin(), member.begin() + bank.table);
    out.insert(out.end(), table.begin(), table.end());
    out.insert(out.end(), member.begin() + bank.table + table.size(), member.begin() + bank.table + bank.first);
    out.insert(out.end(), strings.begin(), strings.end());
    out.insert(out.end(), member.begin() + bank.stringsEnd, member.end());
    return out;
}
}
