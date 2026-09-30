#pragma once

#include <string_view>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace updater
{
inline bool IsApplyPlanArgument(std::wstring_view argument)
{
    return argument == L"--apply-plan";
}

#ifdef _WIN32
// Use Windows argument boundaries, excluding argv[0], just as apply mode does.
// Paths containing the flag text must still initialize normal guest memory.
inline bool RequestsApplyMode(const wchar_t *commandLine)
{
    int count = 0;
    auto arguments = CommandLineToArgvW(commandLine, &count);
    bool requested = false;
    for (int i = 1; arguments && i < count; ++i)
        requested |= IsApplyPlanArgument(arguments[i]);
    if (arguments) LocalFree(arguments);
    return requested;
}
#endif
} // namespace updater
