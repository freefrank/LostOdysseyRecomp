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

    namespace detail
    {
        // Names the items this copy renamed into the data folder, so a run
        // that fails partway can take back exactly those.
        inline constexpr const char* CopyJournal = ".copying-items";

        inline bool Journal(const std::filesystem::path& data, const std::string& item)
        {
            std::ofstream out(data / CopyJournal, std::ios::binary | std::ios::app);
            out << item << '\n';
            out.flush();
            return static_cast<bool>(out);
        }

        // Removes the journaled items and the journal; nothing else in the
        // data folder, and never the originals.
        inline bool UndoJournaled(const std::filesystem::path& data)
        {
            const auto journal = data / CopyJournal;
            bool exists = false;
            if (!Exists(journal, exists)) return false;
            if (!exists) return true;
            {
                std::ifstream in(journal, std::ios::binary);
                if (!in) return false;
                std::error_code error;
                for (std::string line; std::getline(in, line);)
                    for (const std::string item : ProgramFolderItems)
                        if (line == item)
                        {
                            std::filesystem::remove_all(data / item, error);
                            if (error) return false;
                        }
            }
            std::error_code error;
            std::filesystem::remove(journal, error);
            return !error;
        }
    }

    // Copies the items above from the program folder into the data folder once.
    // Each item goes to <name>.part first, is journaled, and is renamed into
    // place only when its copy succeeded; the marker is written last. The
    // originals are never moved or deleted. Until the marker exists this
    // returns false and the caller keeps using the program folder for that
    // start, so the items this run renamed into place are removed again: the
    // next start copies them afresh, with whatever changed in the program
    // folder meanwhile. On this adoption start the old settings.ini replaces
    // the one in the data folder (RecompLauncher writes that file before the
    // game starts); no other item is copied over an existing one.
    inline bool AdoptProgramFolder(const std::filesystem::path& program, const std::filesystem::path& data)
    {
        bool exists = false;
        if (!detail::Exists(data / CopiedMarker, exists)) return false;
        if (exists) return true;
        std::error_code error;
        std::filesystem::create_directories(data, error);
        if (error) return false;
        // A run that stopped before its marker left journaled items behind.
        if (!detail::UndoJournaled(data)) return false;
        const auto fail = [&] {
            detail::UndoJournaled(data);
            return false;
        };
        for (const std::string item : ProgramFolderItems)
        {
            const auto source = program / item;
            const auto target = data / item;
            const auto part = data / (item + ".part");
            std::filesystem::remove_all(part, error);
            if (error) return fail();
            if (!detail::Exists(source, exists)) return fail();
            if (!exists) continue;
            const bool replaces = item == "settings.ini";
            if (!detail::Exists(target, exists)) return fail();
            if (exists && !replaces) continue;
            std::filesystem::copy(source, part,
                std::filesystem::copy_options::recursive | std::filesystem::copy_options::copy_symlinks, error);
            if (!error && !detail::Journal(data, item)) error = std::make_error_code(std::errc::io_error);
            if (!error) std::filesystem::rename(part, target, error);
            if (error)
            {
                std::error_code ignored;
                std::filesystem::remove_all(part, ignored);
                return fail();
            }
        }
        if (!detail::WriteMarker(data)) return fail();
        std::filesystem::remove(data / detail::CopyJournal, error);
        return true;
    }
}
