#include <install/host.h>
#include <install/installer_ui.h>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace {
install::InstallerResult nextResult;
std::filesystem::path shownDestination;
unsigned shown = 0;
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
}

install::InstallerResult install::ShowInstallerUI(const std::filesystem::path&,
                                                  const std::filesystem::path& source,
                                                  const std::filesystem::path& destination)
{
    Require(source.empty(), "installer must let the user choose a source");
    ++shown;
    shownDestination = destination;
    return nextResult;
}

int main()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("lo-install-host-fixture-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "disc1");
    std::ofstream(root / "disc1" / "default.xex") << "fixture";
    auto boot = root / "disc1";
    const auto executable = root / "bin";

    Require(install::RunHost(executable, &boot) == install::HostResult::AlreadyPresent && shown == 0,
            "ordinary launch skips importer for an existing game");
    nextResult = {.cancelled = true};
    boot = root / "disc1";
    Require(install::RunHost(executable, &boot, true) == install::HostResult::Cancelled &&
            shown == 1 && shownDestination == root && boot == root / "disc1" &&
            std::filesystem::exists(root / "disc1" / "default.xex"),
            "forced import opens existing installation root and cancellation keeps the game path");
    nextResult = {.success = true, .destination = root};
    Require(install::RunHost(executable, &boot, true) == install::HostResult::Installed &&
            shown == 2 && shownDestination == root && boot == root / "disc1",
            "subsequent forced import reopens and recognizes existing boot disc (DLC-only case)");
    std::filesystem::remove_all(root);
    return 0;
}
