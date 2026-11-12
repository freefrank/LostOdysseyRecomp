#include "text_format.h"
#include "ue3_package.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
#include <stdexcept>

namespace modding::text
{
namespace
{
void Require(bool ok, const char *message)
{
    if (!ok) throw std::runtime_error(message);
}

bool StartsWith(std::string_view value, std::string_view prefix) { return value.substr(0, prefix.size()) == prefix; }
bool EndsWith(std::string_view value, std::string_view suffix)
{
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

// The folder after `prefix` when the path is prefix<lang>/...
std::string FolderAfter(std::string_view path, std::string_view prefix)
{
    if (!StartsWith(path, prefix)) return {};
    const auto rest = path.substr(prefix.size());
    const auto slash = rest.find('/');
    return slash == std::string_view::npos ? std::string() : std::string(rest.substr(0, slash));
}

std::string FileName(std::string_view path) { return std::string(path.substr(path.rfind('/') + 1)); }

void AppendUtf8(std::string &out, uint32_t point)
{
    if (point < 0x80) out += char(point);
    else if (point < 0x800) { out += char(0xc0 | point >> 6); out += char(0x80 | (point & 63)); }
    else if (point < 0x10000)
    {
        out += char(0xe0 | point >> 12); out += char(0x80 | (point >> 6 & 63)); out += char(0x80 | (point & 63));
    }
    else
    {
        out += char(0xf0 | point >> 18); out += char(0x80 | (point >> 12 & 63));
        out += char(0x80 | (point >> 6 & 63)); out += char(0x80 | (point & 63));
    }
}

void AppendToken(std::string &out, char16_t unit)
{
    char token[8];
    std::snprintf(token, sizeof(token), "{%04X}", unsigned(unit));
    out += token;
}

// Plain UTF-8 (no tokens) -> UTF-16, for formats stored as UTF-8.
std::u16string Utf8ToUnits(std::string_view text)
{
    std::u16string units;
    for (size_t i = 0; i < text.size();)
    {
        const auto byte = uint8_t(text[i]);
        uint32_t point = 0;
        size_t length = 1;
        if (byte < 0x80) point = byte;
        else if ((byte & 0xe0) == 0xc0) { point = byte & 0x1f; length = 2; }
        else if ((byte & 0xf0) == 0xe0) { point = byte & 0x0f; length = 3; }
        else if ((byte & 0xf8) == 0xf0) { point = byte & 0x07; length = 4; }
        else throw std::runtime_error("invalid UTF-8");
        Require(length <= text.size() - i, "truncated UTF-8");
        for (size_t k = 1; k < length; ++k)
        {
            const auto next = uint8_t(text[i + k]);
            Require((next & 0xc0) == 0x80, "invalid UTF-8");
            point = point << 6 | (next & 0x3f);
        }
        constexpr uint32_t smallest[] = {0, 0, 0x80, 0x800, 0x10000};
        Require(point >= smallest[length] && point <= 0x10ffff && (point < 0xd800 || point > 0xdfff), "invalid UTF-8");
        if (point >= 0x10000)
        {
            point -= 0x10000;
            units += char16_t(0xd800 + (point >> 10));
            units += char16_t(0xdc00 + (point & 0x3ff));
        }
        else units += char16_t(point);
        i += length;
    }
    return units;
}

// UTF-16 without tokens -> UTF-8, for formats stored as UTF-8 (lone surrogates are invalid there).
std::string UnitsToUtf8(std::u16string_view units)
{
    std::string out;
    for (size_t i = 0; i < units.size(); ++i)
    {
        uint32_t point = units[i];
        if (point >= 0xd800 && point <= 0xdbff && i + 1 < units.size() && units[i + 1] >= 0xdc00 && units[i + 1] <= 0xdfff)
            point = 0x10000 + ((point - 0xd800) << 10) + (units[++i] - 0xdc00);
        else Require(point < 0xd800 || point > 0xdfff, "lone surrogate in UTF-8 text");
        AppendUtf8(out, point);
    }
    return out;
}

uint32_t Le32(std::span<const uint8_t> b, size_t p)
{
    Require(p <= b.size() && 4 <= b.size() - p, "read out of bounds");
    return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 | uint32_t(b[p + 3]) << 24;
}
int32_t Be32(std::span<const uint8_t> b, size_t p)
{
    Require(p <= b.size() && 4 <= b.size() - p, "read out of bounds");
    return int32_t(uint32_t(b[p]) << 24 | uint32_t(b[p + 1]) << 16 | uint32_t(b[p + 2]) << 8 | b[p + 3]);
}
void PutLe32(std::vector<uint8_t> &out, uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) out.push_back(uint8_t(value >> shift));
}
void PutBe32(std::vector<uint8_t> &out, int32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8) out.push_back(uint8_t(uint32_t(value) >> shift));
}

// Looks up a replacement and decodes it; nullptr when the key has none.
const std::string *Find(const Replacements &replacements, const std::string &key)
{
    const auto it = replacements.find(key);
    return it == replacements.end() ? nullptr : &it->second;
}

// Line-based formats keep the line text only; a translation may not add a line.
std::u16string DecodeLine(const std::string &text)
{
    auto units = DecodeText(text);
    Require(units.find_first_of(u"\r\n") == std::u16string::npos, "a line translation contains a line break");
    return units;
}

// ---- JMD: u32 LE payload size, u32 LE offsets (relative to +4), UTF-16BE messages ----

// Private-use codes followed by a numeric parameter unit (wait frames, window
// and portrait, variables); counted over every JMD in both editions.
constexpr std::array<char16_t, 8> kJmdParameterCodes = {0xE00D, 0xE100, 0xE104, 0xE105, 0xE10D, 0xE110, 0xE111, 0xE112};

struct JmdTable
{
    uint32_t size = 0;
    std::vector<uint32_t> offsets;
    std::vector<bool> valid;                  // false: an offset past the payload (junk slot, kept verbatim)
    std::vector<std::span<const uint8_t>> messages;
    std::span<const uint8_t> trailing;
};

JmdTable ReadJmd(std::span<const uint8_t> d)
{
    JmdTable table;
    table.size = Le32(d, 0);
    Require(table.size <= d.size() - 4, "JMD payload size past the end");
    const auto first = table.size ? Le32(d, 4) : 0;
    Require(first % 4 == 0 && first <= table.size, "invalid JMD offset table");
    const size_t count = first / 4;
    std::vector<uint32_t> starts;
    for (size_t i = 0; i < count; ++i)
    {
        table.offsets.push_back(Le32(d, 4 + 4 * i));
        table.valid.push_back(table.offsets.back() < table.size);
        if (table.valid.back()) starts.push_back(table.offsets.back());
    }
    // Offsets can be out of order or shared: a message ends at the next larger offset.
    std::sort(starts.begin(), starts.end());
    starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
    starts.push_back(table.size);
    for (size_t i = 0; i < count; ++i)
    {
        if (!table.valid[i]) { table.messages.emplace_back(); continue; }
        const auto begin = table.offsets[i];
        const auto end = *std::upper_bound(starts.begin(), starts.end(), begin);
        Require((end - begin) % 2 == 0, "odd-length JMD message");
        table.messages.push_back(d.subspan(4 + begin, end - begin));
    }
    table.trailing = d.subspan(4 + size_t(table.size));
    return table;
}

std::vector<Entry> ParseJmd(std::span<const uint8_t> d)
{
    const auto table = ReadJmd(d);
    std::vector<Entry> entries;
    for (size_t i = 0; i < table.messages.size(); ++i)
        if (table.valid[i])
            entries.push_back({std::to_string(i), EncodeUnits(ReadUtf16Be(table.messages[i]), kJmdParameterCodes)});
    return entries;
}

std::vector<uint8_t> RebuildJmd(std::span<const uint8_t> d, const Replacements &replacements, size_t *unknownKeys)
{
    const auto table = ReadJmd(d);
    const size_t count = table.offsets.size();
    std::vector<std::vector<uint8_t>> messages(count);
    size_t known = 0;
    for (size_t i = 0; i < count; ++i)
    {
        if (!table.valid[i]) continue;
        if (const auto text = Find(replacements, std::to_string(i)))
        {
            ++known;
            AppendUtf16Be(messages[i], DecodeText(*text));
        }
        else messages[i].assign(table.messages[i].begin(), table.messages[i].end());
    }
    if (unknownKeys) *unknownKeys = replacements.size() - known;
    // Original physical order; slots that shared an offset stay shared while their text matches.
    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return table.offsets[a] < table.offsets[b]; });
    std::vector<uint32_t> offsets(count);
    std::map<uint32_t, size_t> firstAt; // original offset -> slot written there
    std::vector<uint8_t> blobs;
    const uint32_t base = uint32_t(4 * count);
    for (const auto i : order)
    {
        if (!table.valid[i]) { offsets[i] = table.offsets[i]; continue; }
        if (const auto it = firstAt.find(table.offsets[i]); it != firstAt.end() && messages[it->second] == messages[i])
        {
            offsets[i] = offsets[it->second];
            continue;
        }
        firstAt.emplace(table.offsets[i], i);
        offsets[i] = base + uint32_t(blobs.size());
        blobs.insert(blobs.end(), messages[i].begin(), messages[i].end());
    }
    std::vector<uint8_t> out;
    PutLe32(out, base + uint32_t(blobs.size()));
    for (const auto offset : offsets) PutLe32(out, offset);
    out.insert(out.end(), blobs.begin(), blobs.end());
    out.insert(out.end(), table.trailing.begin(), table.trailing.end());
    return out;
}

// ---- Line formats: subtitles (UTF-8 "start,end,text") and the credits script (UTF-16LE) ----

struct Line
{
    size_t begin = 0, end = 0;     // text span within the line, in units of the file's encoding
    bool translatable = false;
};

// Splits at '\n'; a '\r' before it stays outside the text span.
template <class Text>
std::vector<Line> SplitLines(const Text &text)
{
    std::vector<Line> lines;
    size_t begin = 0;
    while (begin <= text.size())
    {
        auto end = text.find('\n', begin);
        const bool last = end == Text::npos;
        if (last) end = text.size();
        auto stop = end;
        if (stop > begin && text[stop - 1] == '\r') --stop;
        lines.push_back({begin, stop, false});
        if (last) break;
        begin = end + 1;
    }
    return lines;
}

constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";

// "631,644,Hey hey!": the text after the second comma when both fields are numbers.
bool SubtitleText(std::string_view line, size_t &textBegin)
{
    size_t pos = 0;
    for (int field = 0; field < 2; ++field)
    {
        const auto start = pos;
        while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9') ++pos;
        if (pos == start || pos >= line.size() || line[pos] != ',') return false;
        ++pos;
    }
    textBegin = pos;
    return true;
}

std::string SubtitleBody(std::span<const uint8_t> d, size_t &bomSize)
{
    std::string body(reinterpret_cast<const char *>(d.data()), d.size());
    bomSize = StartsWith(body, kUtf8Bom) ? kUtf8Bom.size() : 0;
    return body.substr(bomSize);
}

std::vector<Line> SubtitleLines(const std::string &body)
{
    auto lines = SplitLines(body);
    for (auto &line : lines)
    {
        size_t textBegin = 0;
        if (SubtitleText(std::string_view(body).substr(line.begin, line.end - line.begin), textBegin))
        {
            line.begin += textBegin;
            line.translatable = true;
        }
    }
    return lines;
}

std::vector<Entry> ParseSubtitle(std::span<const uint8_t> d)
{
    size_t bom = 0;
    const auto body = SubtitleBody(d, bom);
    std::vector<Entry> entries;
    const auto lines = SubtitleLines(body);
    for (size_t i = 0; i < lines.size(); ++i)
        if (lines[i].translatable)
            entries.push_back({std::to_string(i + 1),
                               EncodeUnits(Utf8ToUnits(std::string_view(body).substr(lines[i].begin, lines[i].end - lines[i].begin)))});
    return entries;
}

std::vector<uint8_t> RebuildSubtitle(std::span<const uint8_t> d, const Replacements &replacements, size_t *unknownKeys)
{
    size_t bom = 0;
    const auto body = SubtitleBody(d, bom);
    const auto lines = SubtitleLines(body);
    std::string out(reinterpret_cast<const char *>(d.data()), bom);
    size_t copied = 0, known = 0;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (!lines[i].translatable) continue;
        const auto text = Find(replacements, std::to_string(i + 1));
        if (!text) continue;
        ++known;
        out += body.substr(copied, lines[i].begin - copied);
        out += UnitsToUtf8(DecodeLine(*text));
        copied = lines[i].end;
    }
    out += body.substr(copied);
    if (unknownKeys) *unknownKeys = replacements.size() - known;
    return {out.begin(), out.end()};
}

// The credits script: UTF-16LE with a BOM, one command or text per line.
std::u16string ReadUtf16Le(std::span<const uint8_t> d)
{
    Require(d.size() % 2 == 0, "odd-length UTF-16 file");
    std::u16string units(d.size() / 2, u'\0');
    for (size_t i = 0; i < units.size(); ++i) units[i] = char16_t(d[2 * i] | d[2 * i + 1] << 8);
    return units;
}
void AppendUtf16Le(std::vector<uint8_t> &out, std::u16string_view units)
{
    for (const auto unit : units) { out.push_back(uint8_t(unit)); out.push_back(uint8_t(unit >> 8)); }
}

std::vector<Line> StaffRollLines(const std::u16string &units)
{
    const size_t bom = !units.empty() && units[0] == 0xfeff ? 1 : 0;
    auto lines = SplitLines(units);
    for (auto &line : lines)
    {
        line.begin = std::max(line.begin, bom);
        // Shown text sits between '~'; lines with only layout codes or "~ ~" spacers are left out.
        const auto text = std::u16string_view(units).substr(line.begin, line.end - line.begin);
        for (size_t open = text.find('~'); open != std::u16string_view::npos && !line.translatable;)
        {
            const auto close = text.find('~', open + 1);
            if (close == std::u16string_view::npos) break;
            const auto shown = text.substr(open + 1, close - open - 1);
            line.translatable = shown.find_first_not_of(u" \t") != std::u16string_view::npos;
            open = text.find('~', close + 1);
        }
    }
    return lines;
}

std::vector<Entry> ParseStaffRoll(std::span<const uint8_t> d)
{
    const auto units = ReadUtf16Le(d);
    std::vector<Entry> entries;
    const auto lines = StaffRollLines(units);
    for (size_t i = 0; i < lines.size(); ++i)
        if (lines[i].translatable)
            entries.push_back({std::to_string(i + 1),
                               EncodeUnits(std::u16string_view(units).substr(lines[i].begin, lines[i].end - lines[i].begin))});
    return entries;
}

std::vector<uint8_t> RebuildStaffRoll(std::span<const uint8_t> d, const Replacements &replacements, size_t *unknownKeys)
{
    const auto units = ReadUtf16Le(d);
    const auto lines = StaffRollLines(units);
    std::u16string out;
    size_t copied = 0, known = 0;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        if (!lines[i].translatable) continue;
        const auto text = Find(replacements, std::to_string(i + 1));
        if (!text) continue;
        ++known;
        out += units.substr(copied, lines[i].begin - copied);
        out += DecodeLine(*text);
        copied = lines[i].end;
    }
    out += units.substr(copied);
    if (unknownKeys) *unknownKeys = replacements.size() - known;
    std::vector<uint8_t> bytes;
    AppendUtf16Le(bytes, out);
    return bytes;
}

// ---- UE3 Coalesced localization: i32 BE string count, then (file name, contents) FStrings ----

struct FString
{
    std::u16string value;
    bool wide = false; // UTF-16LE (negative length) rather than Latin-1
};

FString ReadFString(std::span<const uint8_t> d, size_t &pos)
{
    const auto length = Be32(d, pos);
    pos += 4;
    FString result;
    if (length == 0) return result;
    Require(length >= -(1 << 24) && length <= (1 << 24), "invalid FString length");
    if (length > 0)
    {
        Require(size_t(length) <= d.size() - pos && d[pos + length - 1] == 0, "invalid FString");
        for (int32_t i = 0; i + 1 < length; ++i) result.value += char16_t(d[pos + i]);
        pos += size_t(length);
        return result;
    }
    const size_t units = size_t(-int64_t(length));
    Require(units * 2 <= d.size() - pos, "invalid FString");
    result.wide = true;
    for (size_t i = 0; i + 1 < units; ++i) result.value += char16_t(d[pos + 2 * i] | d[pos + 2 * i + 1] << 8);
    Require(d[pos + 2 * units - 2] == 0 && d[pos + 2 * units - 1] == 0, "unterminated FString");
    pos += units * 2;
    return result;
}

void WriteFString(std::vector<uint8_t> &out, const FString &string)
{
    // A Latin-1 string that gained other characters is written as UTF-16.
    const bool wide = string.wide || std::any_of(string.value.begin(), string.value.end(), [](char16_t c) { return c > 0xff; });
    if (string.value.empty() && !string.wide) { PutBe32(out, 0); return; }
    if (wide)
    {
        PutBe32(out, -int32_t(string.value.size() + 1));
        AppendUtf16Le(out, string.value);
        out.push_back(0); out.push_back(0);
        return;
    }
    PutBe32(out, int32_t(string.value.size() + 1));
    for (const auto c : string.value) out.push_back(uint8_t(c));
    out.push_back(0);
}

struct CoalescedFile
{
    FString name, body;
};

std::vector<CoalescedFile> ReadCoalesced(std::span<const uint8_t> d, size_t &end)
{
    const auto count = Be32(d, 0);
    Require(count >= 0 && count % 2 == 0 && count <= 4096, "invalid Coalesced count");
    std::vector<CoalescedFile> files;
    size_t pos = 4;
    for (int32_t i = 0; i < count / 2; ++i)
    {
        auto name = ReadFString(d, pos);
        files.push_back({std::move(name), ReadFString(d, pos)});
    }
    end = pos;
    return files;
}

// The INI files the game shows text from; the others are editor and tool strings.
std::string CoalescedStem(const std::u16string &name)
{
    std::string stem;
    for (const auto c : name.substr(name.find_last_of(u"\\/") + 1)) stem += char(c >= 'A' && c <= 'Z' ? c + 32 : c);
    stem = stem.substr(0, stem.find('.'));
    return stem == "rpgame" || stem == "engine" || stem == "core" ? stem : std::string();
}

struct IniValue
{
    std::string key;
    size_t begin = 0, end = 0; // value span inside the body, without surrounding quotes
};

std::vector<IniValue> IniValues(const std::u16string &body, const std::string &stem)
{
    std::vector<IniValue> values;
    std::string section;
    std::map<std::string, int> seen;
    for (const auto &line : SplitLines(body))
    {
        const auto text = std::u16string_view(body).substr(line.begin, line.end - line.begin);
        const auto first = text.find_first_not_of(u" \t");
        if (first == std::u16string_view::npos || text[first] == ';') continue;
        if (text[first] == '[')
        {
            const auto close = text.find(']', first);
            section = close == std::u16string_view::npos ? std::string() : UnitsToUtf8(text.substr(first + 1, close - first - 1));
            continue;
        }
        const auto equals = text.find('=');
        if (equals == std::u16string_view::npos) continue;
        auto name = std::u16string(text.substr(first, equals - first));
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
        auto key = stem + "/" + section + "/" + UnitsToUtf8(name);
        if (const int n = ++seen[key]; n > 1) key += "#" + std::to_string(n);
        size_t begin = line.begin + equals + 1, end = line.end;
        if (end - begin >= 2 && body[begin] == '"' && body[end - 1] == '"') { ++begin; --end; }
        values.push_back({std::move(key), begin, end});
    }
    return values;
}

std::vector<Entry> ParseCoalesced(std::span<const uint8_t> d)
{
    size_t end = 0;
    std::vector<Entry> entries;
    for (const auto &file : ReadCoalesced(d, end))
    {
        const auto stem = CoalescedStem(file.name.value);
        if (stem.empty()) continue;
        for (const auto &value : IniValues(file.body.value, stem))
            entries.push_back({value.key, EncodeUnits(std::u16string_view(file.body.value).substr(value.begin, value.end - value.begin))});
    }
    return entries;
}

std::vector<uint8_t> RebuildCoalesced(std::span<const uint8_t> d, const Replacements &replacements, size_t *unknownKeys)
{
    size_t end = 0, known = 0;
    auto files = ReadCoalesced(d, end);
    std::vector<uint8_t> out;
    PutBe32(out, int32_t(files.size() * 2));
    for (auto &file : files)
    {
        if (const auto stem = CoalescedStem(file.name.value); !stem.empty())
        {
            std::u16string body;
            size_t copied = 0;
            for (const auto &value : IniValues(file.body.value, stem))
            {
                const auto text = Find(replacements, value.key);
                if (!text) continue;
                ++known;
                body += file.body.value.substr(copied, value.begin - copied);
                body += DecodeLine(*text);
                copied = value.end;
            }
            body += file.body.value.substr(copied);
            file.body.value = std::move(body);
        }
        WriteFString(out, file.name);
        WriteFString(out, file.body);
    }
    out.insert(out.end(), d.begin() + end, d.end());
    if (unknownKeys) *unknownKeys = replacements.size() - known;
    return out;
}

// ---- xmlbin (loc/<l>/field/name_data.xmb): a node stream that names its strings by their offset in a pool ----
// Header (LE): "xmlbin\0\0", u32 0x01000001, u32 1, u32 node offset, u32 node size, u32 pool offset,
// u32 pool size. Nodes: 0x00 (1 byte); 0x10/0x20 element: u16 name, u16 count; 0x40 attribute: u16 name,
// u16 value. Pool entry: u16 length, high bit set = that many bytes of ASCII, clear = UTF-16LE bytes.

struct XmlBinString
{
    std::u16string value;
    bool ascii = false;
};

struct XmlBinToken
{
    size_t at = 0;    // offset of the token byte in the file
    uint8_t kind = 0; // 0x10, 0x20 or 0x40
    uint16_t first = 0, second = 0;
};

struct XmlBin
{
    uint32_t nodeOffset = 0, nodeSize = 0, poolOffset = 0;
    std::unordered_map<uint16_t, XmlBinString> pool;
    std::vector<XmlBinToken> tokens;
};

uint16_t Le16(std::span<const uint8_t> b, size_t p) { return uint16_t(b[p] | b[p + 1] << 8); }

XmlBin ReadXmlBin(std::span<const uint8_t> d)
{
    Require(d.size() >= 32 && std::memcmp(d.data(), "xmlbin\0\0", 8) == 0, "not an xmlbin file");
    XmlBin x;
    x.nodeOffset = Le32(d, 16);
    x.nodeSize = Le32(d, 20);
    x.poolOffset = Le32(d, 24);
    const uint32_t poolSize = Le32(d, 28);
    Require(uint64_t(x.nodeOffset) + x.nodeSize <= x.poolOffset && uint64_t(x.poolOffset) + poolSize <= d.size(),
            "xmlbin sections out of range");
    for (size_t p = 0; p < poolSize;)
    {
        Require(p + 2 <= poolSize && p <= 0xFFFF, "xmlbin pool entry out of range");
        const uint16_t length = Le16(d, x.poolOffset + p);
        const size_t bytes = length & 0x7FFF, at = x.poolOffset + p + 2;
        Require(p + 2 + bytes <= poolSize, "xmlbin string out of range");
        XmlBinString s;
        s.ascii = (length & 0x8000) != 0;
        if (s.ascii)
            for (size_t i = 0; i < bytes; ++i) s.value.push_back(char16_t(d[at + i]));
        else
            s.value = ReadUtf16Le(d.subspan(at, bytes));
        x.pool.emplace(uint16_t(p), std::move(s));
        p += 2 + bytes;
    }
    // The node section starts with a u32; the tokens follow.
    for (size_t p = x.nodeOffset + 4; p < size_t(x.nodeOffset) + x.nodeSize;)
    {
        const uint8_t kind = d[p];
        if (kind == 0) { ++p; continue; }
        if (kind != 0x10 && kind != 0x20 && kind != 0x40) break; // trailing bytes, kept as they are
        Require(p + 5 <= size_t(x.nodeOffset) + x.nodeSize, "xmlbin token out of range");
        XmlBinToken t{p, kind, Le16(d, p + 1), Le16(d, p + 3)};
        Require(x.pool.contains(t.first) && (kind != 0x40 || x.pool.contains(t.second)), "xmlbin token names no string");
        x.tokens.push_back(t);
        p += 5;
    }
    return x;
}

// Attributes whose name ends in "ID" are numbers the game looks up; the rest are text.
template <typename Visit> void XmlBinTexts(const XmlBin &x, Visit visit)
{
    int record = -1;
    for (const auto &t : x.tokens)
    {
        if (t.kind == 0x20) { ++record; continue; }
        if (t.kind != 0x40) continue;
        const auto &name = x.pool.at(t.first).value;
        if (name.size() >= 2 && name.ends_with(u"ID")) continue;
        visit(t, std::to_string(std::max(record, 0)) + "." + UnitsToUtf8(name));
    }
}

std::vector<Entry> ParseXmlBin(std::span<const uint8_t> d)
{
    const auto x = ReadXmlBin(d);
    std::vector<Entry> entries;
    XmlBinTexts(x, [&](const XmlBinToken &t, std::string key) { entries.push_back({std::move(key), EncodeUnits(x.pool.at(t.second).value)}); });
    return entries;
}

std::vector<uint8_t> RebuildXmlBin(std::span<const uint8_t> d, const Replacements &replacements, size_t *unknownKeys)
{
    const auto x = ReadXmlBin(d);
    std::unordered_map<size_t, std::u16string> values; // token offset -> new value
    size_t known = 0;
    XmlBinTexts(x, [&](const XmlBinToken &t, const std::string &key) {
        if (const auto text = Find(replacements, key))
        {
            ++known;
            auto value = DecodeText(*text);
            if (value != x.pool.at(t.second).value) values.emplace(t.at, std::move(value));
        }
    });
    if (unknownKeys) *unknownKeys = replacements.size() - known;
    if (values.empty()) return {d.begin(), d.end()};
    // A new pool with the strings still named, in the order they are first named.
    std::vector<uint8_t> pool;
    std::map<std::pair<bool, std::u16string>, uint16_t> placed;
    auto place = [&](const std::u16string &value, bool asciiAllowed) {
        const bool ascii = asciiAllowed && std::all_of(value.begin(), value.end(), [](char16_t c) { return c < 0x80; });
        const auto key = std::make_pair(ascii, value);
        if (const auto it = placed.find(key); it != placed.end()) return it->second;
        const size_t bytes = ascii ? value.size() : value.size() * 2;
        Require(bytes <= 0x7FFF && pool.size() + 2 + bytes <= 0x10000, "xmlbin string pool over 64 KiB");
        const auto offset = uint16_t(pool.size());
        const auto length = uint16_t(bytes | (ascii ? 0x8000 : 0));
        pool.push_back(uint8_t(length));
        pool.push_back(uint8_t(length >> 8));
        if (ascii) for (const auto c : value) pool.push_back(uint8_t(c));
        else AppendUtf16Le(pool, value);
        placed.emplace(key, offset);
        return offset;
    };
    std::vector<uint8_t> out(d.begin(), d.begin() + x.poolOffset);
    auto put16 = [&](size_t at, uint16_t v) { out[at] = uint8_t(v); out[at + 1] = uint8_t(v >> 8); };
    for (const auto &t : x.tokens)
    {
        const auto &first = x.pool.at(t.first);
        put16(t.at + 1, place(first.value, first.ascii));
        if (t.kind != 0x40) continue;
        const auto &second = x.pool.at(t.second);
        if (const auto it = values.find(t.at); it != values.end())
            put16(t.at + 3, place(it->second, true));
        else
            put16(t.at + 3, place(second.value, second.ascii));
    }
    out.insert(out.end(), pool.begin(), pool.end());
    for (int i = 0; i < 4; ++i) out[28 + i] = uint8_t(pool.size() >> (8 * i));
    return out;
}

// ---- Thousand Years of Dreams (event/1000/<l>/*.xxx): UE3 packages; each InterpTrackLO_FontRect
// export has Keys, an array of structs whose Title (StrProperty) is one piece of a page, shown at
// Postion (Vector) inside Width x Hight, scaled by FontScaleW/FontScaleH. Key "<track>#<n>" is that
// Title. Rebuild also takes "<track>#<n>.x", ".y", ".w", ".h" and ".scale" (both FontScale values),
// so a language pack can lay a page out again; Parse does not list them.

constexpr std::string_view kFontRect = "InterpTrackLO_FontRect";

// A property tag: where it starts, where its value starts, and the value's size.
struct DreamTag
{
    std::string name, type;
    size_t begin = 0, value = 0, size = 0;
};

// The tags of a property list from r.pos up to "None"; r ends after "None".
std::vector<DreamTag> DreamTags(const ue3::Package &package, ue3::Reader &r)
{
    std::vector<DreamTag> tags;
    for (;;)
    {
        Require(tags.size() < 65536, "property count exceeds limit");
        DreamTag tag;
        tag.begin = r.pos;
        tag.name = package.Name(r);
        if (tag.name == "None") return tags;
        tag.type = package.Name(r);
        tag.size = r.U32();
        r.U32();
        if (tag.type == "StructProperty") package.Name(r);
        if (tag.type == "BoolProperty") r.U32();
        tag.value = r.pos;
        r.Take(tag.size);
        tags.push_back(std::move(tag));
    }
}

void PutBe32At(std::vector<uint8_t> &out, size_t at, uint32_t value)
{
    for (int i = 0; i < 4; ++i) out[at + i] = uint8_t(value >> (24 - 8 * i));
}

// One key: the original element bytes with Title and the layout values replaced.
// keyName: "<track>#<n>". Returns whether a replacement applied.
bool RewriteDreamKey(const ue3::Package &package, std::span<const uint8_t> d, ue3::Reader &r, const std::string &keyName,
                     const Replacements &replacements, std::vector<uint8_t> &out, size_t &known)
{
    const size_t begin = r.pos;
    const auto tags = DreamTags(package, r);
    const size_t end = r.pos; // after "None"
    bool applied = false;
    auto number = [&](const char *suffix) -> std::optional<float> {
        const auto text = Find(replacements, keyName + suffix);
        if (!text) return std::nullopt;
        ++known;
        applied = true;
        return std::stof(*text);
    };
    const auto title = Find(replacements, keyName);
    if (title) { ++known; applied = true; }
    const auto x = number(".x"), y = number(".y"), w = number(".w"), h = number(".h"), scale = number(".scale");
    size_t copied = begin;
    for (const auto &tag : tags)
    {
        std::vector<uint8_t> value;
        auto setFloat = [&](size_t offset, float v) {
            const auto bits = std::bit_cast<uint32_t>(v);
            for (int i = 0; i < 4; ++i) value[offset + i] = uint8_t(bits >> (24 - 8 * i));
        };
        value.assign(d.begin() + tag.value, d.begin() + tag.value + tag.size);
        bool changed = false;
        if (tag.name == "Title" && tag.type == "StrProperty" && title)
        {
            // An unchanged Title keeps its bytes (an empty narrow one is stored with its NUL).
            size_t pos = tag.value;
            const auto original = ReadFString(d.first(tag.value + tag.size), pos);
            auto text = DecodeText(*title);
            if (text != original.value)
            {
                value.clear();
                WriteFString(value, FString{std::move(text), original.wide});
                changed = true;
            }
        }
        else if (tag.name == "Postion" && tag.size == 12 && (x || y))
        {
            if (x) setFloat(0, *x);
            if (y) setFloat(4, *y);
            changed = true;
        }
        else if (tag.type == "FloatProperty" && tag.size == 4)
        {
            const std::optional<float> v = tag.name == "Width" ? w : tag.name == "Hight" ? h :
                                           tag.name == "FontScaleW" || tag.name == "FontScaleH" ? scale : std::nullopt;
            if (v) { setFloat(0, *v); changed = true; }
        }
        if (!changed) continue;
        // The tag header up to its size field, the new size, the rest of the header, the value.
        out.insert(out.end(), d.begin() + copied, d.begin() + tag.begin + 16);
        const size_t sizeAt = out.size();
        out.insert(out.end(), d.begin() + tag.begin + 16, d.begin() + tag.value);
        PutBe32At(out, sizeAt, uint32_t(value.size()));
        out.insert(out.end(), value.begin(), value.end());
        copied = tag.value + tag.size;
    }
    out.insert(out.end(), d.begin() + copied, d.begin() + end);
    return applied;
}

template <typename Visit> void DreamTracks(const ue3::Package &package, Visit visit)
{
    for (const auto &e : package.exports)
        if (e.size && package.ClassName(e) == kFontRect) visit(e);
}

std::vector<Entry> ParseDream(std::span<const uint8_t> d)
{
    const ue3::Package package(d);
    std::vector<Entry> entries;
    DreamTracks(package, [&](const ue3::Export &e) {
        auto r = ue3::ObjectReader(package, e);
        r.I32();
        for (const auto &tag : DreamTags(package, r))
        {
            if (tag.name != "Keys" || tag.type != "ArrayProperty") continue;
            ue3::Reader keys{d.first(tag.value + tag.size), tag.value};
            const auto count = ue3::Count(keys, 8);
            for (uint32_t k = 0; k < count; ++k)
                for (const auto &key : DreamTags(package, keys))
                    if (key.name == "Title" && key.type == "StrProperty")
                    {
                        size_t pos = key.value;
                        entries.push_back({e.name + "#" + std::to_string(k), EncodeUnits(ReadFString(d.first(key.value + key.size), pos).value)});
                    }
        }
    });
    return entries;
}

std::vector<uint8_t> RebuildDream(std::span<const uint8_t> d, const Replacements &replacements, size_t *unknownKeys)
{
    const ue3::Package package(d);
    size_t known = 0;
    std::map<size_t, std::vector<uint8_t>> objects; // export index -> new serialized object
    for (size_t i = 0; i < package.exports.size(); ++i)
    {
        const auto &e = package.exports[i];
        if (!e.size || package.ClassName(e) != kFontRect) continue;
        auto r = ue3::ObjectReader(package, e);
        std::vector<uint8_t> object;
        bool changed = false;
        r.I32();
        size_t copied = e.offset;
        for (const auto &tag : DreamTags(package, r))
        {
            if (tag.name != "Keys" || tag.type != "ArrayProperty") continue;
            ue3::Reader keys{d.first(tag.value + tag.size), tag.value};
            const auto count = ue3::Count(keys, 8);
            std::vector<uint8_t> value(d.begin() + tag.value, d.begin() + tag.value + 4);
            for (uint32_t k = 0; k < count; ++k)
                changed |= RewriteDreamKey(package, d, keys, e.name + "#" + std::to_string(k), replacements, value, known);
            object.insert(object.end(), d.begin() + copied, d.begin() + tag.begin + 16);
            const size_t sizeAt = object.size();
            object.insert(object.end(), d.begin() + tag.begin + 16, d.begin() + tag.value);
            PutBe32At(object, sizeAt, uint32_t(value.size()));
            object.insert(object.end(), value.begin(), value.end());
            copied = tag.value + tag.size;
        }
        if (!changed) continue;
        object.insert(object.end(), d.begin() + copied, d.begin() + e.offset + e.size);
        objects.emplace(i, std::move(object));
    }
    if (unknownKeys) *unknownKeys = replacements.size() - known;
    if (objects.empty()) return {d.begin(), d.end()};
    // Objects in file order; the bytes around them stay, the export table gets the new offsets and sizes.
    std::vector<size_t> order;
    for (size_t i = 0; i < package.exports.size(); ++i)
        if (package.exports[i].size) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return package.exports[a].offset < package.exports[b].offset; });
    std::vector<uint8_t> out;
    std::vector<std::pair<size_t, std::pair<uint32_t, uint32_t>>> table; // record -> (size, offset)
    size_t copied = 0;
    for (const auto i : order)
    {
        const auto &e = package.exports[i];
        Require(e.offset >= copied, "overlapping UE3 export data");
        out.insert(out.end(), d.begin() + copied, d.begin() + e.offset);
        const auto it = objects.find(i);
        const uint32_t offset = uint32_t(out.size());
        if (it != objects.end()) out.insert(out.end(), it->second.begin(), it->second.end());
        else out.insert(out.end(), d.begin() + e.offset, d.begin() + e.offset + e.size);
        table.push_back({e.record, {uint32_t(out.size() - offset), offset}});
        copied = size_t(e.offset) + e.size;
    }
    out.insert(out.end(), d.begin() + copied, d.end());
    for (const auto &[record, values] : table)
    {
        PutBe32At(out, record + 32, values.first);
        PutBe32At(out, record + 36, values.second);
    }
    return out;
}
} // namespace

Format Detect(std::string_view path)
{
    const auto name = FileName(path);
    if (const auto lang = FolderAfter(path, "bin/xenon/loc/"); !lang.empty())
    {
        const auto base = "bin/xenon/loc/" + lang + "/";
        if (path == base + "menu/menu_" + lang + ".dat") return Format::MenuDat;
        if (StartsWith(path, base + "battle/text/"))
        {
            if (EndsWith(name, ".jmd")) return Format::Jmd;
            if (EndsWith(name, "data.bin")) return Format::StringBank;
        }
        if (path == base + "staffroll/roll_" + lang + ".bin") return Format::StaffRoll;
        if (path == base + "field/name_data.xmb") return Format::XmlBin;
        return Format::None;
    }
    if (!FolderAfter(path, "bin/xenon/scr/mes/").empty() && EndsWith(name, ".jmd")) return Format::Jmd;
    if (!FolderAfter(path, "bin/xenon/event/subtitle/").empty() && EndsWith(name, ".sub")) return Format::Subtitle;
    if (const auto lang = FolderAfter(path, "bin/xenon/event/1000/"); !lang.empty() && EndsWith(name, "_" + lang + ".xxx") &&
        path == "bin/xenon/event/1000/" + lang + "/" + name)
        return Format::Dream;
    if (StartsWith(path, "rpgame/localization/coalesced.") && path.find('/', 30) == std::string_view::npos) return Format::Coalesced;
    return Format::None;
}

const char *Name(Format format)
{
    switch (format)
    {
    case Format::MenuDat: return "menu_dat";
    case Format::StringBank: return "string_bank";
    case Format::Jmd: return "jmd";
    case Format::Subtitle: return "subtitle";
    case Format::StaffRoll: return "staff_roll";
    case Format::Coalesced: return "coalesced";
    case Format::XmlBin: return "xmlbin";
    case Format::Dream: return "dream";
    case Format::None: break;
    }
    return "none";
}

std::string Language(std::string_view path)
{
    for (const auto prefix : {"bin/xenon/loc/", "bin/xenon/scr/mes/", "bin/xenon/event/subtitle/", "bin/xenon/event/1000/"})
        if (auto lang = FolderAfter(path, prefix); !lang.empty()) return lang;
    if (StartsWith(path, "rpgame/localization/coalesced.")) return std::string(path.substr(30));
    return {};
}

std::string EncodeUnits(std::u16string_view units, std::span<const char16_t> withParameter)
{
    std::string out;
    out.reserve(units.size());
    for (size_t i = 0; i < units.size(); ++i)
    {
        const char16_t unit = units[i];
        if (unit >= 0xe000 && unit <= 0xf8ff)
        {
            if (i + 1 < units.size() && std::find(withParameter.begin(), withParameter.end(), unit) != withParameter.end())
            {
                char token[16];
                std::snprintf(token, sizeof(token), "{%04X:%04X}", unsigned(unit), unsigned(units[i + 1]));
                out += token;
                ++i;
            }
            else AppendToken(out, unit);
            continue;
        }
        if (unit >= 0xd800 && unit <= 0xdbff && i + 1 < units.size() && units[i + 1] >= 0xdc00 && units[i + 1] <= 0xdfff)
        {
            AppendUtf8(out, 0x10000 + ((unit - 0xd800) << 10) + (units[i + 1] - 0xdc00));
            ++i;
            continue;
        }
        if ((unit >= 0xd800 && unit <= 0xdfff) || unit == '{' || (unit < 0x20 && unit != '\n') || unit >= 0xfffe)
            AppendToken(out, unit);
        else AppendUtf8(out, unit);
    }
    return out;
}

std::u16string DecodeText(std::string_view text)
{
    std::u16string units;
    auto hex = [&](size_t at) {
        Require(at + 4 <= text.size(), "malformed {XXXX} token");
        unsigned value = 0;
        for (size_t k = at; k < at + 4; ++k)
        {
            const char c = text[k];
            const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 :
                              c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            Require(digit >= 0, "malformed {XXXX} token");
            value = value << 4 | unsigned(digit);
        }
        return char16_t(value);
    };
    size_t plain = 0;
    for (size_t i = 0; i < text.size();)
    {
        if (text[i] != '{') { ++i; continue; }
        units += Utf8ToUnits(text.substr(plain, i - plain));
        Require(i + 6 <= text.size(), "malformed {XXXX} token");
        units += hex(i + 1);
        if (text[i + 5] == '}') i += 6;
        else
        {
            // {XXXX:YYYY}: a code and its parameter unit.
            Require(text[i + 5] == ':' && i + 11 <= text.size() && text[i + 10] == '}', "malformed {XXXX:YYYY} token");
            units += hex(i + 6);
            i += 11;
        }
        plain = i;
    }
    units += Utf8ToUnits(text.substr(plain));
    return units;
}

std::u16string ReadUtf16Be(std::span<const uint8_t> bytes)
{
    Require(bytes.size() % 2 == 0, "odd-length UTF-16 text");
    std::u16string units(bytes.size() / 2, u'\0');
    for (size_t i = 0; i < units.size(); ++i) units[i] = char16_t(bytes[2 * i] << 8 | bytes[2 * i + 1]);
    return units;
}

void AppendUtf16Be(std::vector<uint8_t> &out, std::u16string_view units)
{
    for (const auto unit : units) { out.push_back(uint8_t(unit >> 8)); out.push_back(uint8_t(unit)); }
}

std::vector<Entry> Parse(Format format, std::span<const uint8_t> member)
{
    switch (format)
    {
    case Format::MenuDat: return ParseMenuDat(member);
    case Format::StringBank: return ParseStringBank(member);
    case Format::Jmd: return ParseJmd(member);
    case Format::Subtitle: return ParseSubtitle(member);
    case Format::StaffRoll: return ParseStaffRoll(member);
    case Format::Coalesced: return ParseCoalesced(member);
    case Format::XmlBin: return ParseXmlBin(member);
    case Format::Dream: return ParseDream(member);
    case Format::None: break;
    }
    throw std::runtime_error("not a text format");
}

std::vector<uint8_t> Rebuild(Format format, std::span<const uint8_t> member, const Replacements &replacements, size_t *unknownKeys)
{
    size_t unknown = 0;
    std::vector<uint8_t> result;
    switch (format)
    {
    case Format::MenuDat: result = RebuildMenuDat(member, replacements, &unknown); break;
    case Format::StringBank: result = RebuildStringBank(member, replacements, &unknown); break;
    case Format::Jmd: result = RebuildJmd(member, replacements, &unknown); break;
    case Format::Subtitle: result = RebuildSubtitle(member, replacements, &unknown); break;
    case Format::StaffRoll: result = RebuildStaffRoll(member, replacements, &unknown); break;
    case Format::Coalesced: result = RebuildCoalesced(member, replacements, &unknown); break;
    case Format::XmlBin: result = RebuildXmlBin(member, replacements, &unknown); break;
    case Format::Dream: result = RebuildDream(member, replacements, &unknown); break;
    case Format::None: throw std::runtime_error("not a text format");
    }
    if (unknownKeys) *unknownKeys = unknown;
    return result;
}
}
