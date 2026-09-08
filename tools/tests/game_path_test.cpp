#include "settings/game_path.h"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
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

    // A blank file selects ../game relative to the executable, even when the
    // caller's working directory points elsewhere.
    const auto packageGame = fixture.root / "game";
    const auto packageDisc = packageGame / "disc1";
    fixture.Marker(packageDisc);
    fixture.Config(" \t\r\n");
    CheckRoot(Resolve(fixture.exe), packageDisc, Source::DefaultSearch,
              "blank game-path.txt did not select ../game/disc1");

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

    // Direct EXE-directory startup remains usable when ../game is absent.
    std::error_code error;
    fs::remove_all(packageGame, error);
    fixture.Config("\n");
    const auto exeGame = fixture.exe;
    fixture.Marker(exeGame);
    CheckRoot(Resolve(fixture.exe), exeGame, Source::DefaultSearch,
              "EXE-directory default.xex was not recognized");
    fs::remove(exeGame / "default.xex", error);

    // The legacy nested game layout is a deterministic later fallback.
    const auto nested = fixture.exe / "game" / "disc1";
    fixture.Marker(nested);
    CheckRoot(Resolve(fixture.exe), nested, Source::DefaultSearch,
              "nested game/disc1 fallback was not recognized");

    // With no marker anywhere, the returned path still records the documented
    // package default so the loader/installer can report the missing resource.
    fs::remove_all(fixture.exe / "game", error);
    fixture.Config("\n");
    const auto noCandidate = Resolve(fixture.exe);
    Check(noCandidate.root == (fixture.exe / ".." / "game").lexically_normal(),
          "missing default did not retain ../game");
    Check(noCandidate.source == Source::Fallback && !noCandidate.valid,
          "missing default did not report fallback state");

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

    // A missing explicit path is returned unchanged and cannot fall through
    // to the valid nested layout above.
    const auto missing = fixture.root / "missing-explicit-game";
    const auto explicitResult = Resolve(fixture.exe, missing);
    Check(explicitResult.root == missing, "explicit path was rewritten");
    Check(explicitResult.source == Source::ExplicitArgument, "explicit source was lost");
    Check(!explicitResult.valid, "missing explicit path was treated as valid");

    fs::current_path(originalDirectory);
    std::cout << "PASS: game path discovery fixture\n";
    return 0;
}
