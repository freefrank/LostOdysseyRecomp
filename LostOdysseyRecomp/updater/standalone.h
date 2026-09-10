#pragma once

#include "update.h"

namespace updater
{
// Resolve the installed package, not the version of a separately copied helper.
bool ConfigureStandalone(const std::filesystem::path &helper, StartupOptions &options, std::string &error);
bool StandaloneGameClosed(const std::filesystem::path &executable, std::string &error);

// The callback lets the host fixture exercise the controller without public downloads.
using PrepareUpdate = StartupResult (*)(const StartupOptions &);
int RunStandalone(const std::filesystem::path &helper, PrepareUpdate prepare = PrepareAtStartup);
} // namespace updater
