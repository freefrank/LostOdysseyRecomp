#pragma once

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../os/user_paths.h"

namespace settings::game_path
{
    enum class Source
    {
        ExplicitArgument,
        ConfiguredFile,
        DefaultSearch,
        Fallback,
    };

    struct Resolution
    {
        std::filesystem::path root;
        Source source = Source::Fallback;
        bool valid = false;
        bool configuredPathRejected = false;
    };

    inline bool HasDefaultXex(const std::filesystem::path& directory)
    {
        std::error_code error;
        return std::filesystem::is_regular_file(directory / "default.xex", error);
    }

    inline std::string Trim(std::string value)
    {
        // game-path.txt is UTF-8, so trim only ASCII whitespace and leave all
        // non-ASCII path bytes untouched.
        if (value.size() >= 3 && static_cast<unsigned char>(value[0]) == 0xef &&
            static_cast<unsigned char>(value[1]) == 0xbb &&
            static_cast<unsigned char>(value[2]) == 0xbf)
            value.erase(0, 3);

        size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
            ++begin;
        size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
            --end;
        return value.substr(begin, end - begin);
    }

    inline std::filesystem::path AbsoluteFrom(const std::filesystem::path& path,
                                              const std::filesystem::path& base)
    {
        const auto joined = path.is_absolute() ? path : base / path;
        std::error_code error;
        const auto absolute = std::filesystem::absolute(joined, error);
        return (error ? joined : absolute).lexically_normal();
    }

    inline std::filesystem::path PathFromUtf8(std::string_view value)
    {
#ifndef _WIN32
        std::string str(value);
        for (char& c : str)
        {
            if (c == '\\')
                c = '/';
        }
        if (str.size() >= 2 && std::isalpha(static_cast<unsigned char>(str[0])) && str[1] == ':')
        {
            char drive = static_cast<char>(std::tolower(static_cast<unsigned char>(str[0])));
            std::string sub = (str.size() >= 3 && str[2] == '/') ? str.substr(3) : str.substr(2);
            str = std::string("/mnt/") + drive + "/" + sub;
        }
        const auto utf8 = std::u8string(reinterpret_cast<const char8_t*>(str.data()), str.size());
        return std::filesystem::path(utf8);
#else
        const auto utf8 = std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size());
        return std::filesystem::path(utf8);
#endif
    }

    inline std::optional<std::filesystem::path> Recognize(const std::filesystem::path& candidate)
    {
        std::error_code error;
        if (std::filesystem::is_regular_file(candidate, error))
        {
            // A configured XEX is accepted only when it is the boot image the
            // runtime will load. The returned root always contains that image.
            if (candidate.filename() == "default.xex")
                return candidate.parent_path();
            return std::nullopt;
        }

        if (!std::filesystem::is_directory(candidate, error))
            return std::nullopt;
        if (HasDefaultXex(candidate))
            return candidate;
        if (HasDefaultXex(candidate / "disc1"))
            return candidate / "disc1";
        return std::nullopt;
    }

    inline std::optional<std::filesystem::path> RecognizeDefaultDirectory(
        const std::filesystem::path& candidate)
    {
        if (HasDefaultXex(candidate))
            return candidate.lexically_normal();
        return std::nullopt;
    }

    inline std::filesystem::path DefaultGameRoot(const std::filesystem::path& executableDirectory)
    {
        if (!os::user_paths::UsePortableLayout())
            return (os::user_paths::DataDir() / "game").lexically_normal();
        return (executableDirectory / "game").lexically_normal();
    }

    inline Resolution Resolve(const std::filesystem::path& executableDirectory,
                             const std::optional<std::filesystem::path>& explicitGame = std::nullopt)
    {
        const auto exeDirectory = AbsoluteFrom(executableDirectory, std::filesystem::current_path());

        // An explicit path is an isolation boundary. Recognize only the
        // supplied candidate (install root, disc1, or default.xex). A missing
        // path stays as supplied and never falls through to another install.
        if (explicitGame)
        {
            auto candidate = *explicitGame;
#ifndef _WIN32
            candidate = PathFromUtf8(candidate.string());
#endif
            if (const auto root = Recognize(candidate))
                return { *root, Source::ExplicitArgument, true, false };
            return { candidate, Source::ExplicitArgument, false, false };
        }

        Resolution result;
        const auto configPath = os::user_paths::UsePortableLayout()
            ? exeDirectory / "game-path.txt"
            : os::user_paths::ConfigDir() / "game-path.txt";
        std::ifstream location(configPath, std::ios::binary);
        std::string configured;
        if (std::getline(location, configured) && !(configured = Trim(std::move(configured))).empty())
        {
            const auto configuredPath = AbsoluteFrom(PathFromUtf8(configured), exeDirectory);
            if (const auto root = Recognize(configuredPath))
                return { *root, Source::ConfiguredFile, true, false };
            // A non-empty configured path is an explicit user choice. Keep it
            // for the loader's existing error/installer flow rather than
            // accidentally selecting an older installation elsewhere.
            return { configuredPath, Source::ConfiguredFile, false, true };
        }

        // The order is intentional: an empty file (or no file) first gets the
        // package default ./game beside the executable, then direct EXE,
        // parent ../game and legacy/dev layouts. Every candidate is checked for a real default.xex.
        std::vector<std::filesystem::path> candidates;
        if (!os::user_paths::UsePortableLayout())
        {
            candidates.push_back(os::user_paths::DataDir() / "game" / "disc1");
            candidates.push_back(os::user_paths::DataDir() / "game");
        }
        candidates.push_back(exeDirectory / "game" / "disc1");
        candidates.push_back(exeDirectory / "game");
        candidates.push_back(exeDirectory);
        candidates.push_back(exeDirectory / ".." / "game" / "disc1");
        candidates.push_back(exeDirectory / ".." / "game");
        candidates.push_back(exeDirectory / ".." / ".." / ".." / "LostOdysseyRecompLib" / "private" / "disc1");
        candidates.push_back(exeDirectory / "LostOdysseyRecompLib" / "private" / "disc1");
        for (const auto& candidate : candidates)
        {
            if (const auto root = RecognizeDefaultDirectory(candidate))
                return { *root, Source::DefaultSearch, true, result.configuredPathRejected };
        }

        // Keep a deterministic, useful path for the existing loader error and
        // installer handoff when no candidate exists. Non-portable layouts use
        // the XDG data game directory instead of a read-only install tree.
        result.root = DefaultGameRoot(exeDirectory);
        result.source = Source::Fallback;
        result.valid = false;
        return result;
    }
}
