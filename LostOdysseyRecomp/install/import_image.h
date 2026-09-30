#pragma once

#include <cstdint>
#include <array>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "import_crypto.h"
#include "import_game.h"

namespace install
{

struct Entry
{
    std::string name;     // Relative path using forward slashes
    uint64_t offset = 0;  // File byte offset inside image (or 0 for folder)
    uint64_t size = 0;    // File size in bytes
};

class ImageReader
{
public:
    virtual ~ImageReader() = default;
    virtual Kind GetKind() const = 0;
    virtual uint64_t GetLimit() const = 0;
    virtual void Read(uint64_t offset, void* buffer, size_t size) = 0;
    virtual std::vector<Entry> GetEntries() = 0;
};

// ----------------------------------------------------------------------------
// FolderSource
// ----------------------------------------------------------------------------
class FolderSource
{
public:
    explicit FolderSource(std::filesystem::path path) : path_(std::move(path)) {}
    const std::filesystem::path& GetPath() const { return path_; }
    std::vector<Entry> GetEntries(const Cancelled& cancelled = {});

private:
    std::filesystem::path path_;
};

// ----------------------------------------------------------------------------
// IsoImageReader: supports standard, stripped, and arbitrary-padded ISOs
// ----------------------------------------------------------------------------
class IsoImageReader : public ImageReader
{
public:
    explicit IsoImageReader(std::filesystem::path path, const Cancelled& cancelled = {});
    ~IsoImageReader() override = default;

    Kind GetKind() const override { return Kind::Iso; }
    uint64_t GetLimit() const override { return limit_; }
    void Read(uint64_t offset, void* buffer, size_t size) override;
    std::vector<Entry> GetEntries() override;

private:
    std::filesystem::path path_;
    std::ifstream stream_;
    uint64_t base_ = 0;
    uint64_t limit_ = 0;
};

// ----------------------------------------------------------------------------
// GodImageReader: reads Game On Demand (Data0000, Data0001, ...) chunks
// ----------------------------------------------------------------------------
class GodImageReader : public ImageReader
{
public:
    explicit GodImageReader(std::filesystem::path path, const Cancelled& cancelled = {});
    ~GodImageReader() override = default;

    Kind GetKind() const override { return Kind::God; }
    uint64_t GetLimit() const override { return limit_; }
    void Read(uint64_t offset, void* buffer, size_t size) override;
    std::vector<Entry> GetEntries() override;

private:
    std::filesystem::path path_;
    std::vector<std::ifstream> files_;
    uint64_t limit_ = 0;
};

// ----------------------------------------------------------------------------
// StfsPackage: reads Xbox 360 STFS DLC package (CON/LIVE/PIRS)
// ----------------------------------------------------------------------------
class StfsPackage
{
public:
    struct StfsEntry
    {
        std::string path;
        uint32_t size = 0;
        std::vector<uint32_t> blocks;
        bool isDirectory = false;
    };

    explicit StfsPackage(std::filesystem::path path, const Cancelled& cancelled = {});
    ~StfsPackage() = default;

    const std::filesystem::path& GetPath() const { return path_; }
    const DlcPackageInfo& GetInfo() const { return info_; }
    const std::vector<uint8_t>& GetHeader() const { return header_; }
    const std::vector<StfsEntry>& GetEntries() const { return entries_; }

    void ReadBlock(uint32_t blockNumber, void* outBuffer);

private:
    void ReadRaw(uint64_t offset, void* buffer, size_t size);
    uint64_t DataOffset(uint32_t blockNumber);
    uint64_t TableOffset(uint32_t blockNumber, int level);
    const std::vector<uint8_t>& GetTable(uint32_t blockNumber, int level);
    std::vector<uint32_t> ReadChain(uint32_t start, uint32_t count);
    void ParseEntries();

    std::filesystem::path path_;
    std::ifstream stream_;
    uint64_t length_ = 0;
    Cancelled cancelled_;

    uint32_t copies_ = 2;
    uint32_t rootCopy_ = 0;
    uint32_t totalBlocks_ = 0;
    uint32_t tableCount_ = 0;
    uint32_t tableStart_ = 0;
    int topLevel_ = 0;
    uint64_t base_ = 0;

    std::vector<uint8_t> header_;
    DlcPackageInfo info_;
    std::vector<StfsEntry> entries_;

    // Block tables cache: key is (level << 32) | tableNumber
    std::map<uint64_t, std::vector<uint8_t>> tables_;
    std::set<uint32_t> claimedBlocks_;
};

} // namespace install
