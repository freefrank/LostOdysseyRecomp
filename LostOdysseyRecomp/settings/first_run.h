#pragma once
#include <filesystem>
namespace settings {
// Runs before XamInit/guest language caching and before the renderer starts.
bool FirstRunSetup();
// Updates gameRoot when the user chooses a different validated data folder.
bool FirstRunSetup(std::filesystem::path *gameRoot);
}
