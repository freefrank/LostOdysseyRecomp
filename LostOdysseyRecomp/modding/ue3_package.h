#pragma once
#include <bit>
#include <cstdint>
#include <cstring>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Reading the game's UE3 packages: big endian (Xbox 360), file version 0x1a3
// (419), licensee 0x29/0x2a. See tools/asset_inventory/decode.cpp. Used by the
// asset export and the model import; every malformed read throws.
namespace modding::ue3
{
constexpr uint32_t kPackageMagic = 0x9e2a83c1;

inline void Require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }

struct Reader
{
    std::span<const uint8_t> bytes;
    size_t pos = 0;
    std::span<const uint8_t> Take(size_t count)
    {
        Require(pos <= bytes.size() && count <= bytes.size() - pos, "truncated package data");
        const auto value = bytes.subspan(pos, count); pos += count; return value;
    }
    uint8_t Byte() { return Take(1)[0]; }
    uint16_t U16() { const auto b = Take(2); return uint16_t(b[0] << 8 | b[1]); }
    float F32() { return std::bit_cast<float>(U32()); }
    uint32_t U32()
    {
        const auto b = Take(4);
        return uint32_t(b[0]) << 24 | uint32_t(b[1]) << 16 | uint32_t(b[2]) << 8 | b[3];
    }
    int32_t I32() { return int32_t(U32()); }
    std::string String()
    {
        const auto size = I32();
        Require(size != 0 && size >= -32768 && size <= 32768, "invalid UE string length");
        if (size > 0)
        {
            const auto b = Take(size_t(size)); Require(b.back() == 0, "unterminated UE string");
            return {reinterpret_cast<const char *>(b.data()), b.size() - 1};
        }
        // FString UTF-16 payloads are little endian even in Xbox 360 packages.
        const auto b = Take(size_t(-int64_t(size)) * 2);
        Require(b[b.size() - 1] == 0 && b[b.size() - 2] == 0, "unterminated UTF-16 UE string");
        std::string result;
        for (size_t i = 0; i + 2 < b.size(); i += 2)
        {
            uint32_t point = uint32_t(b[i]) | uint32_t(b[i + 1]) << 8;
            if (point >= 0xd800 && point <= 0xdbff)
            {
                Require(i + 4 < b.size(), "invalid UTF-16 surrogate");
                const uint32_t low = uint32_t(b[i + 2]) | uint32_t(b[i + 3]) << 8;
                Require(low >= 0xdc00 && low <= 0xdfff, "invalid UTF-16 surrogate");
                point = 0x10000 + ((point - 0xd800) << 10) + low - 0xdc00; i += 2;
            }
            else Require(point < 0xdc00 || point > 0xdfff, "invalid UTF-16 surrogate");
            if (point < 0x80) result += char(point);
            else if (point < 0x800) { result += char(0xc0 | point >> 6); result += char(0x80 | (point & 63)); }
            else if (point < 0x10000)
            {
                result += char(0xe0 | point >> 12); result += char(0x80 | (point >> 6 & 63)); result += char(0x80 | (point & 63));
            }
            else
            {
                result += char(0xf0 | point >> 18); result += char(0x80 | (point >> 12 & 63));
                result += char(0x80 | (point >> 6 & 63)); result += char(0x80 | (point & 63));
            }
        }
        return result;
    }
};

// record: where the export's table entry starts; its SerialSize and
// SerialOffset are the big-endian words at record + 32 and record + 36.
struct Export { std::string name; int32_t classRef = 0; uint32_t offset = 0, size = 0; size_t record = 0; };
struct Package
{
    std::span<const uint8_t> bytes;
    std::vector<std::string> names, imports;
    std::vector<int32_t> importOuters; // object reference of each import's outer
    std::vector<Export> exports;
    std::string Name(Reader &r) const
    {
        const auto index = r.U32(), number = r.U32();
        Require(index < names.size() && number <= 1000000, "invalid UE name reference");
        return names[index] + (number ? "_" + std::to_string(number - 1) : "");
    }
    explicit Package(std::span<const uint8_t> data) : bytes(data)
    {
        Reader r{data};
        Require(data.size() >= 44 && r.U32() == kPackageMagic, "not a UE3 package");
        const auto version = r.U32();
        Require(version == 0x002a01a3 || version == 0x002901a3, "unsupported UE3 package version");
        r.U32(); r.String(); r.U32(); // Header size, folder name, package flags.
        const auto nameCount = r.U32(), nameOffset = r.U32(), exportCount = r.U32(), exportOffset = r.U32();
        const auto importCount = r.U32(), importOffset = r.U32(), dependsOffset = r.U32();
        Require(nameCount <= 1000000 && exportCount <= 1000000 && importCount <= 1000000, "UE3 table count exceeds limit");
        Require(r.pos <= nameOffset && nameOffset <= importOffset && importOffset <= exportOffset &&
                exportOffset <= dependsOffset && dependsOffset <= data.size(), "invalid UE3 table offsets");
        r = {data.first(importOffset), nameOffset};
        for (uint32_t i = 0; i < nameCount; ++i) { names.push_back(r.String()); r.Take(8); }
        r = {data.first(exportOffset), importOffset};
        for (uint32_t i = 0; i < importCount; ++i) { Name(r); Name(r); importOuters.push_back(r.I32()); imports.push_back(Name(r)); }
        r = {data.first(dependsOffset), exportOffset};
        for (uint32_t i = 0; i < exportCount; ++i)
        {
            Export e; e.record = r.pos; e.classRef = r.I32(); r.I32(); r.I32(); e.name = Name(r);
            r.I32(); r.Take(8); e.size = r.U32(); e.offset = r.U32();
            // Version 419: ComponentMap, ExportFlags, NetObjects, package GUID.
            const auto components = r.U32();
            Require(components <= (r.bytes.size() - r.pos) / 12, "invalid export component map");
            for (uint32_t c = 0; c < components; ++c) { Name(r); r.I32(); }
            r.U32();
            const auto netObjects = r.U32();
            Require(netObjects <= (r.bytes.size() - r.pos) / 4, "invalid export net objects");
            r.Take(size_t(netObjects) * 4 + 16);
            Require(!e.size || (e.offset <= data.size() && e.size <= data.size() - e.offset), "export data outside package");
            Require(e.classRef >= 0 ? size_t(e.classRef) <= exportCount : uint64_t(-int64_t(e.classRef)) <= imports.size(),
                    "export class outside package tables");
            exports.push_back(std::move(e));
        }
    }
    std::string ClassName(const Export &e) const
    {
        if (e.classRef < 0) return imports[size_t(-int64_t(e.classRef)) - 1];
        if (e.classRef > 0) return exports[size_t(e.classRef) - 1].name;
        return "Class";
    }
    // An object reference: export index + 1, -(import index + 1), or 0 for none.
    const Export *ExportAt(int32_t ref) const { return ref > 0 && size_t(ref) <= exports.size() ? &exports[size_t(ref) - 1] : nullptr; }
    std::string RefName(int32_t ref) const
    {
        if (const auto *e = ExportAt(ref)) return e->name;
        if (ref < 0 && uint64_t(-int64_t(ref)) <= imports.size()) return imports[size_t(-int64_t(ref)) - 1];
        return "None";
    }
};

// The object's serialized bytes, positioned at its first byte.
inline Reader ObjectReader(const Package &package, const Export &e) { return {package.bytes.first(size_t(e.offset) + e.size), e.offset}; }

// Element count of an array whose elements take at least `size` bytes.
inline uint32_t Count(Reader &r, size_t size)
{
    const auto count = r.U32();
    Require(count <= (r.bytes.size() - r.pos) / size, "invalid array count");
    return count;
}
inline void SkipArray(Reader &r, size_t size) { r.Take(size_t(Count(r, size)) * size); }

// Tagged properties up to "None"; objects start with their net index, structs
// inside arrays do not. visit(name, type, value reader, bool value).
template <typename Visit> void Properties(const Package &package, Reader &r, bool object, Visit &&visit)
{
    if (object) r.I32();
    for (size_t count = 0;; ++count)
    {
        Require(count < 65536, "property count exceeds limit");
        const auto name = package.Name(r);
        if (name == "None") return;
        const auto type = package.Name(r);
        const auto size = r.U32(); r.U32();
        if (type == "StructProperty") package.Name(r);
        uint32_t boolean = 0;
        if (type == "BoolProperty") boolean = r.U32();
        Reader value{r.Take(size)};
        visit(name, type, value, boolean);
    }
}

// An object name as a file name: control characters and <>:"/\|?* become '_',
// and Windows device names get a leading '_'. The export and the model import
// name files with it.
inline std::string FileStem(std::string_view object)
{
    std::string name;
    for (const unsigned char c : object) name += c < 32 || std::strchr("<>:\"/\\|?*", c) ? '_' : char(c);
    std::string upper = name;
    for (auto &c : upper) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    static const std::set<std::string> reserved{"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
        "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    return reserved.count(upper) ? "_" + name : name;
}
}
