#include "text_format.h"

#include <algorithm>
#include <array>
#include <cstdio>
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
        return Format::None;
    }
    if (!FolderAfter(path, "bin/xenon/scr/mes/").empty() && EndsWith(name, ".jmd")) return Format::Jmd;
    if (!FolderAfter(path, "bin/xenon/event/subtitle/").empty() && EndsWith(name, ".sub")) return Format::Subtitle;
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
    case Format::None: break;
    }
    return "none";
}

std::string Language(std::string_view path)
{
    for (const auto prefix : {"bin/xenon/loc/", "bin/xenon/scr/mes/", "bin/xenon/event/subtitle/"})
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
    case Format::None: throw std::runtime_error("not a text format");
    }
    if (unknownKeys) *unknownKeys = unknown;
    return result;
}
}
