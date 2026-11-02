#pragma once
#include "import_game.h"
#include <iosfwd>
#include <optional>

namespace install::prepare_content
{
struct Request
{
    std::vector<std::filesystem::path> sources;
    std::filesystem::path destination;
};

// Returns no request for normal game launches. Malformed prepare commands throw Error.
std::optional<Request> ParseArguments(int argc, char* const argv[]);
// No windows, renderer, network, or graphics preference changes. Commits the game path with the import.
int Run(const Request& request, const std::filesystem::path& executableDirectory,
        std::ostream& output, std::ostream& errors, const Cancelled& cancelled = {});
}
