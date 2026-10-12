#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace install::ui
{
struct DirectoryItem
{
    std::string name;
    std::filesystem::path path;
    bool isDirectory = false;
    bool isDriveOrRoot = false;
};

struct CreateFolderResult
{
    std::filesystem::path path;
    std::string error;
    explicit operator bool() const { return error.empty(); }
};

inline CreateFolderResult CreateFolder(const std::filesystem::path& parent, std::string_view utf8Name)
{
    auto accessError = [](const std::error_code& ec) {
        return ec == std::errc::permission_denied || ec == std::errc::read_only_file_system;
    };
    if (utf8Name.empty() || utf8Name == "." || utf8Name == ".." ||
        utf8Name.find('/') != std::string_view::npos ||
        utf8Name.find('\\') != std::string_view::npos ||
        utf8Name.find('\0') != std::string_view::npos)
        return {{}, "Enter a folder name without path separators."};

    const auto name = std::u8string(reinterpret_cast<const char8_t*>(utf8Name.data()), utf8Name.size());
    const auto target = parent / std::filesystem::path(name);
    std::error_code ec;
    if (!std::filesystem::is_directory(parent, ec))
        return {{}, accessError(ec) ? "Permission denied or destination is read-only."
                                   : "The current destination is not an accessible directory."};
    if (std::filesystem::exists(target, ec))
        return {{}, "A file or folder with that name already exists."};
    if (ec)
        return {{}, accessError(ec) ? "Permission denied or destination is read-only."
                                   : "Could not inspect the destination: " + ec.message()};
    if (!std::filesystem::create_directory(target, ec))
    {
        if (accessError(ec))
            return {{}, "Permission denied or destination is read-only."};
        return {{}, "Could not create folder: " + (ec ? ec.message() : std::string("folder already exists"))};
    }
    return {target, {}};
}

// Returns system roots / drives (e.g. C:\, D:\ on Windows, / on Unix)
inline std::vector<DirectoryItem> GetSystemRoots()
{
    std::vector<DirectoryItem> roots;
#ifdef _WIN32
    DWORD drives = GetLogicalDrives();
    for (char c = 'A'; c <= 'Z'; ++c)
    {
        if (drives & (1 << (c - 'A')))
        {
            std::string rootStr = std::string(1, c) + ":\\";
            roots.push_back({ rootStr, std::filesystem::path(rootStr), true, true });
        }
    }
#elif defined(__ANDROID__)
    // Shared storage volumes: the internal one and every mounted SD card or
    // USB drive. Apps cannot list /storage, so the Java side passes the
    // removable volumes as LO_STORAGE_VOLUMES (':'-separated). Reading them
    // needs All files access.
    roots.push_back({ "Internal storage", std::filesystem::path("/storage/emulated/0"), true, true });
    std::string_view volumes;
    if (const char* value = std::getenv("LO_STORAGE_VOLUMES")) volumes = value;
    while (!volumes.empty())
    {
        const auto end = volumes.find(':');
        const std::filesystem::path volume(volumes.substr(0, end));
        if (!volume.empty()) roots.push_back({ "SD card " + volume.filename().string(), volume, true, true });
        volumes = end == std::string_view::npos ? std::string_view() : volumes.substr(end + 1);
    }
#else
    roots.push_back({ "/", std::filesystem::path("/"), true, true });
#endif
    return roots;
}

// Where source browsing starts when no folder was given.
inline std::filesystem::path DefaultSourceDirectory()
{
#if defined(__ANDROID__)
    std::error_code ec;
    const std::filesystem::path download("/storage/emulated/0/Download");
    return std::filesystem::is_directory(download, ec) ? download : std::filesystem::path("/storage/emulated/0");
#else
    return std::filesystem::current_path();
#endif
}

// Lists entries inside directory, sorted alphabetically, directories first
inline std::vector<DirectoryItem> ListDirectory(const std::filesystem::path& dir)
{
    std::vector<DirectoryItem> items;
    std::error_code ec;

    if (!dir.empty() && dir != dir.root_path())
    {
        items.push_back({ ".. (Parent Directory)", dir.parent_path(), true, false });
    }

    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
        return items;

    std::vector<DirectoryItem> dirs;
    std::vector<DirectoryItem> files;

    for (const auto& entry : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec))
    {
        if (ec) break;
        bool isDir = entry.is_directory(ec);
        std::u8string u8fn = entry.path().filename().u8string();
        std::string filename(reinterpret_cast<const char*>(u8fn.data()), u8fn.size());
        if (filename.empty()) continue;

        if (isDir)
        {
            dirs.push_back({ filename, entry.path(), true, false });
        }
        else
        {
            files.push_back({ filename, entry.path(), false, false });
        }
    }

    auto comp = [](const DirectoryItem& a, const DirectoryItem& b) {
        return a.name < b.name;
    };
    std::sort(dirs.begin(), dirs.end(), comp);
    std::sort(files.begin(), files.end(), comp);

    items.insert(items.end(), dirs.begin(), dirs.end());
    items.insert(items.end(), files.begin(), files.end());

    return items;
}
}
