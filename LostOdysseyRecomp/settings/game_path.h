#pragma once

#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
        const auto utf8 = std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size());
        return std::filesystem::path(utf8);
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

    inline Resolution Resolve(const std::filesystem::path& executableDirectory,
                             const std::optional<std::filesystem::path>& explicitGame = std::nullopt)
    {
        const auto exeDirectory = AbsoluteFrom(executableDirectory, std::filesystem::current_path());

        // An explicit path is an isolation boundary. Keep it exactly as the
        // caller supplied it, including a missing path, and never search for a
        // different installation on its behalf.
        if (explicitGame)
        {
            // --game retains its historical directory-only contract. The
            // configured-file convenience forms below do not silently widen
            // that contract for an explicit launch.
            return { *explicitGame, Source::ExplicitArgument, HasDefaultXex(*explicitGame), false };
        }

        Resolution result;
        std::ifstream location(exeDirectory / "game-path.txt", std::ios::binary);
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
        // package default ../game beside the executable, then direct EXE and
        // legacy/dev layouts. Every candidate is checked for a real default.xex.
        const std::vector<std::filesystem::path> candidates = {
            exeDirectory / ".." / "game" / "disc1",
            exeDirectory / ".." / "game",
            exeDirectory,
            exeDirectory / "game" / "disc1",
            exeDirectory / "game",
            exeDirectory / ".." / ".." / ".." / "LostOdysseyRecompLib" / "private" / "disc1",
            exeDirectory / "LostOdysseyRecompLib" / "private" / "disc1",
        };
        for (const auto& candidate : candidates)
        {
            if (const auto root = RecognizeDefaultDirectory(candidate))
                return { *root, Source::DefaultSearch, true, result.configuredPathRejected };
        }

        // Keep a deterministic, useful path for the existing loader error and
        // installer handoff when no candidate exists.
        result.root = (exeDirectory / ".." / "game").lexically_normal();
        result.source = Source::Fallback;
        result.valid = false;
        return result;
    }
}
