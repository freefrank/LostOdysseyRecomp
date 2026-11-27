#pragma once

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

// RecompLauncher passes the game's data folder as RECOMPLAUNCHER_DATA. Its
// backups, cloud saves, Uninstall and Move look after that folder; files
// written beside the executable are outside it.
namespace os::launcher_data
{
    // What earlier portable builds wrote beside the executable, worth keeping.
    // Caches, logs and debug captures are rebuilt or not needed.
    inline constexpr std::array ProgramFolderItems = {
        "settings.ini", "mod-list.ini", "log-collection.ini", "shader-preparation-requested.txt",
        "achievement.wav", "game-path.txt", "save", "profile", "mods", "shaders"};
    inline constexpr const char* CopiedMarker = ".copied-from-program-folder";

    inline std::filesystem::path Directory()
    {
        const char* value = std::getenv("RECOMPLAUNCHER_DATA");
        if (value == nullptr || *value == '\0') return {};
        std::error_code error;
        auto path = std::filesystem::absolute(std::filesystem::path(value), error);
        return error ? std::filesystem::path(value) : path.lexically_normal();
    }

    namespace detail
    {
        inline bool Exists(const std::filesystem::path& path, bool& exists)
        {
            std::error_code error;
            const auto status = std::filesystem::symlink_status(path, error);
            if (error && status.type() != std::filesystem::file_type::not_found) return false;
            exists = std::filesystem::exists(status);
            return true;
        }

        inline bool WriteMarker(const std::filesystem::path& data)
        {
            const auto marker = data / CopiedMarker;
            const auto part = data / (std::string(CopiedMarker) + ".part");
            {
                std::ofstream out(part, std::ios::binary | std::ios::trunc);
                out << "1\n";
                out.flush();
                if (!out) return false;
            }
            std::error_code error;
            std::filesystem::rename(part, marker, error);
            return !error;
        }
    }

    // Copies the items above from the program folder into the data folder once.
    // Each item goes to <name>.part first and is renamed into place only when
    // the whole copy succeeded; the marker is written last. The originals are
    // never moved or deleted. Until the marker exists this returns false, and
    // the caller keeps using the program folder for that start; the next start
    // removes leftover .part items and tries again. On this adoption start the
    // old settings.ini replaces the one in the data folder (RecompLauncher
    // writes that file before the game starts); no other item is copied over
    // an existing one.
    inline bool AdoptProgramFolder(const std::filesystem::path& program, const std::filesystem::path& data)
    {
        bool exists = false;
        if (!detail::Exists(data / CopiedMarker, exists)) return false;
        if (exists) return true;
        std::error_code error;
        std::filesystem::create_directories(data, error);
        if (error) return false;
        for (const std::string item : ProgramFolderItems)
        {
            const auto source = program / item;
            const auto target = data / item;
            const auto part = data / (item + ".part");
            std::filesystem::remove_all(part, error);
            if (error) return false;
            if (!detail::Exists(source, exists)) return false;
            if (!exists) continue;
            const bool replaces = item == "settings.ini";
            if (!detail::Exists(target, exists)) return false;
            if (exists && !replaces) continue;
            std::filesystem::copy(source, part,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks, error);
            if (!error) std::filesystem::rename(part, target, error);
            if (error)
            {
                std::error_code ignored;
                std::filesystem::remove_all(part, ignored);
                return false;
            }
        }
        return detail::WriteMarker(data);
    }
}
