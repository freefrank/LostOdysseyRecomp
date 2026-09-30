#include "settings/game_path.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;
using settings::game_path::Resolve;
using settings::game_path::Source;

namespace
{
    struct Fixture
    {
        fs::path root = fs::temp_directory_path() / "lo-game-path-fixture";
        fs::path exe = root / "package";

        Fixture()
        {
            std::error_code error;
            fs::remove_all(root, error);
            fs::create_directories(exe);
        }

        ~Fixture()
        {
            std::error_code error;
            fs::remove_all(root, error);
        }

        void Marker(const fs::path& directory)
        {
            fs::create_directories(directory);
            std::ofstream(directory / "default.xex", std::ios::binary) << "fixture marker";
        }

        void Config(std::string value)
        {
            std::ofstream output(exe / "game-path.txt", std::ios::binary);
            output << value;
        }
    };

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    void CheckRoot(const settings::game_path::Resolution& result, const fs::path& expected,
                   Source source, const char* message)
    {
        Check(result.root == expected, message);
        Check(result.source == source, "unexpected path source");
        Check(result.valid, "expected a valid default.xex marker");
    }
}

int main()
{
    Fixture fixture;
    const auto originalDirectory = fs::current_path();
    const auto unrelatedDirectory = fixture.root / "unrelated-working-directory";
    fs::create_directories(unrelatedDirectory);
    fs::current_path(unrelatedDirectory);

    // A blank file selects ./game relative to the executable, even when the
    // caller's working directory points elsewhere.
    const auto adjacentGame = fixture.exe / "game";
    const auto adjacentDisc = adjacentGame / "disc1";
    fixture.Marker(adjacentDisc);
    fixture.Config(" \t\r\n");
    CheckRoot(Resolve(fixture.exe), adjacentDisc, Source::DefaultSearch,
              "blank game-path.txt did not select ./game/disc1");

    // When both adjacent ./game and parent ../game exist, adjacent ./game takes priority.
    const auto packageGame = fixture.root / "game";
    const auto packageDisc = packageGame / "disc1";
    fixture.Marker(packageDisc);
    CheckRoot(Resolve(fixture.exe), adjacentDisc, Source::DefaultSearch,
              "adjacent ./game/disc1 did not have priority over ../game/disc1");

    // Parent ../game is still recognized as a fallback when adjacent ./game is absent.
    std::error_code error;
    fs::remove_all(adjacentGame, error);
    CheckRoot(Resolve(fixture.exe), packageDisc, Source::DefaultSearch,
              "fallback parent ../game/disc1 was not recognized");

    // A configured direct game directory has priority over defaults.
    const auto direct = fixture.root / "direct-game";
    fixture.Marker(direct);
    fixture.Config("../direct-game\n");
    CheckRoot(Resolve(fixture.exe), direct, Source::ConfiguredFile,
              "configured direct game directory was ignored");

    // An imported installation root resolves to its disc1 directory.
    const auto imported = fixture.root / "imported-install";
    const auto importedDisc = imported / "disc1";
    fixture.Marker(importedDisc);
    fixture.Config("../imported-install\n");
    CheckRoot(Resolve(fixture.exe), importedDisc, Source::ConfiguredFile,
              "configured imported disc root was not recognized");

    // A configured XEX file maps to its containing game directory.
    const auto xexDirectory = fixture.root / "selected-xex";
    fixture.Marker(xexDirectory);
    fixture.Config("../selected-xex/default.xex\n");
    CheckRoot(Resolve(fixture.exe), xexDirectory, Source::ConfiguredFile,
              "configured default.xex was not recognized");

    // Direct EXE-directory startup remains usable when ./game and ../game are absent.
    fs::remove_all(packageGame, error);
    fixture.Config("\n");
    const auto exeGame = fixture.exe;
    fixture.Marker(exeGame);
    CheckRoot(Resolve(fixture.exe), exeGame, Source::DefaultSearch,
              "EXE-directory default.xex was not recognized");
    fs::remove(exeGame / "default.xex", error);

    // The nested game layout is recognized.
    const auto nested = fixture.exe / "game" / "disc1";
    fixture.Marker(nested);
    CheckRoot(Resolve(fixture.exe), nested, Source::DefaultSearch,
              "nested game/disc1 was not recognized");

    // With no marker anywhere, the returned path still records the documented
    // package default ./game so the loader/installer can report the missing resource.
    fs::remove_all(fixture.exe / "game", error);
    fixture.Config("\n");
    const auto noCandidate = Resolve(fixture.exe);
    Check(noCandidate.root == (fixture.exe / "game").lexically_normal(),
          "missing default did not retain ./game");
    Check(noCandidate.source == Source::Fallback && !noCandidate.valid,
          "missing default did not report fallback state");

    // When game-path.txt is missing entirely, fallback path is also ./game.
    fs::remove(fixture.exe / "game-path.txt", error);
    const auto noConfigResult = Resolve(fixture.exe);
    Check(noConfigResult.root == (fixture.exe / "game").lexically_normal(),
          "missing game-path.txt did not fallback to ./game");
    Check(noConfigResult.source == Source::Fallback && !noConfigResult.valid,
          "missing game-path.txt did not report fallback state");

    // Unicode configuration is read as UTF-8 and remains independent of cwd.
    const auto unicodeDirectory = fs::path(std::u8string(u8"游戏´′"));
    const auto unicode = fixture.root / unicodeDirectory / "disc1";
    fixture.Marker(unicode);
    fixture.Config(std::string(reinterpret_cast<const char*>(u8"../游戏´′/disc1\n")));
    CheckRoot(Resolve(fixture.exe), unicode, Source::ConfiguredFile,
              "Unicode relative game path was not preserved");

    // A non-empty but missing configuration remains the selected path. It
    // must reach the existing installer/error path instead of another install.
    const auto missingConfigured = fixture.root / "missing-configured-game";
    fixture.Config(std::string("../missing-configured-game\n"));
    const auto configuredResult = Resolve(fixture.exe);
    Check(configuredResult.root == missingConfigured, "missing configured path was replaced");
    Check(configuredResult.source == Source::ConfiguredFile, "missing configured source was lost");
    Check(!configuredResult.valid && configuredResult.configuredPathRejected,
          "missing configured path was not reported as rejected");

    // --game accepts the same layout forms as game-path.txt: install root,
    // disc1, or default.xex. Recognition stays inside the supplied candidate.
    const auto explicitImported = fixture.root / "explicit-imported";
    const auto explicitImportedDisc = explicitImported / "disc1";
    fixture.Marker(explicitImportedDisc);
    CheckRoot(Resolve(fixture.exe, explicitImported), explicitImportedDisc, Source::ExplicitArgument,
              "explicit imported disc root was not recognized");
    CheckRoot(Resolve(fixture.exe, explicitImportedDisc), explicitImportedDisc, Source::ExplicitArgument,
              "explicit disc1 directory was rewritten");
    CheckRoot(Resolve(fixture.exe, explicitImportedDisc / "default.xex"), explicitImportedDisc,
              Source::ExplicitArgument, "explicit default.xex was not recognized");

    // A missing explicit path is returned unchanged and cannot fall through
    // to the valid nested layout above.
    const auto missing = fixture.root / "missing-explicit-game";
    const auto explicitResult = Resolve(fixture.exe, missing);
    Check(explicitResult.root == missing, "explicit path was rewritten");
    Check(explicitResult.source == Source::ExplicitArgument, "explicit source was lost");
    Check(!explicitResult.valid, "missing explicit path was treated as valid");

#ifndef _WIN32
    {
        const bool previousPortable = os::user_paths::g_usePortableLayout;
        os::user_paths::g_usePortableLayout = false;
        const auto previousEnv = [](const char* name) -> std::optional<std::string> {
            if (const char* value = std::getenv(name)) return value;
            return std::nullopt;
        };
        const auto savedDataHome = previousEnv("XDG_DATA_HOME");
        const auto savedConfigHome = previousEnv("XDG_CONFIG_HOME");
        const auto dataHome = fixture.root / "xdg-data";
        const auto configHome = fixture.root / "xdg-config";
        setenv("XDG_DATA_HOME", dataHome.c_str(), 1);
        setenv("XDG_CONFIG_HOME", configHome.c_str(), 1);
        const auto dataGame = dataHome / "lost-odyssey-recomp" / "game";
        const auto dataDisc = dataGame / "disc1";
        fixture.Marker(dataDisc);
        fixture.Marker(adjacentDisc);
        CheckRoot(Resolve(fixture.exe), dataDisc, Source::DefaultSearch,
                  "non-portable XDG game did not take priority over adjacent game");

        fs::remove_all(dataGame, error);
        CheckRoot(Resolve(fixture.exe), adjacentDisc, Source::DefaultSearch,
                  "non-portable adjacent game fallback was not recognized");

        fs::remove_all(adjacentGame, error);
        const auto nonPortable = Resolve(fixture.exe);
        Check(nonPortable.root == (dataHome / "lost-odyssey-recomp" / "game").lexically_normal(),
              "non-portable fallback did not use the XDG data game directory");
        Check(nonPortable.source == Source::Fallback && !nonPortable.valid,
              "non-portable missing default did not report fallback state");
        os::user_paths::g_usePortableLayout = previousPortable;
        const auto restoreEnv = [](const char* name, const std::optional<std::string>& value) {
            if (value) setenv(name, value->c_str(), 1);
            else unsetenv(name);
        };
        restoreEnv("XDG_DATA_HOME", savedDataHome);
        restoreEnv("XDG_CONFIG_HOME", savedConfigHome);
    }
#endif

    fs::current_path(originalDirectory);
    std::cout << "PASS: game path discovery fixture\n";
    return 0;
}
