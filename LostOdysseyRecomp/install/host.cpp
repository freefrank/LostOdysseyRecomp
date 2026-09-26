#include "host.h"

#include "../settings/first_run.h"
#include "../settings/game_path.h"
#include "installer_ui.h"

namespace install
{
HostResult RunHost(const std::filesystem::path& executableDirectory, std::filesystem::path* gameRoot, bool force)
{
    if (!gameRoot)
        return HostResult::Failed;

    if (!force)
    {
        if (const auto recognized = settings::game_path::Recognize(*gameRoot))
        {
            *gameRoot = *recognized;
            return HostResult::AlreadyPresent;
        }
    }

    // Recognize returns the boot disc path for a multi-disc install. The UI
    // expects the common parent, otherwise adding disc 2 would nest it in disc1.
    auto destination = *gameRoot;
    if (destination.filename() == "disc1" && settings::game_path::HasDefaultXex(destination))
        destination = destination.parent_path();
    auto res = ShowInstallerUI(executableDirectory, {}, destination);
    if (res.cancelled)
        return HostResult::Cancelled;
    if (!res.success)
        return HostResult::Failed;

    if (const auto recognized = settings::game_path::Recognize(res.destination))
    {
        *gameRoot = *recognized;
        return HostResult::Installed;
    }
    if (const auto recognized = settings::game_path::Recognize(*gameRoot))
    {
        *gameRoot = *recognized;
        return HostResult::Installed;
    }

    return HostResult::Failed;
}
}
