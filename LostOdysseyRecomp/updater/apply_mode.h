#pragma once

#include <optional>

namespace updater
{
// Returns no value when the current command line is an ordinary launch.
// Otherwise it consumes the private staged-apply command line and returns its
// process exit code before runtime startup has touched user state.
std::optional<int> TryRunApplyMode();
} // namespace updater
