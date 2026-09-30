#include "updater/update.h"
#include "updater/apply_mode.h"
#include "updater/standalone.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    if (const auto applyResult = updater::TryRunApplyMode()) return *applyResult;

    int argumentCount = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    const bool hasUnexpectedArguments = arguments && argumentCount > 1;
    if (arguments) LocalFree(arguments);
    if (hasUnexpectedArguments) return 1;
    return updater::RunStandalone(updater::CurrentExecutablePath());
}
