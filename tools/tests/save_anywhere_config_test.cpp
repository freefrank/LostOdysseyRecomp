// Bounded production config and debug-toggle fixture; run from an isolated working directory.
#include <stdafx.h>
#include <os/logger.h>
#include <gpu/dlss_status_log.h>
#undef LOG_INFO
#define LOG_INFO(...) ((void)0)
#undef LOG_WARNING
#define LOG_WARNING(...) ((void)0)
#include "../../LostOdysseyRecomp/settings/config.cpp"
#include "../../LostOdysseyRecomp/debug/save_anywhere.cpp"

extern "C" PPC_FUNC(__imp__sub_822E0E10) {}
extern "C" PPC_FUNC(__imp__sub_82876EA8) {}

namespace
{
void Check(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

std::string Contents()
{
    std::ifstream input("settings.ini");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void Write(const char* text)
{
    std::ofstream output("settings.ini", std::ios::trunc);
    output << text;
    Check(bool(output), "write isolated settings.ini");
}

void CheckFreshProcess(const wchar_t* executable, const char* expected)
{
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --restore " +
        (expected[0] == '1' ? L"1" : L"0");
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    Check(CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
        &startup, &process) != FALSE, "start isolated restart check");
    Check(WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0, "restart check completed");
    DWORD exitCode = 1;
    Check(GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == 0, "startup restoration");
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc == 3 && std::wstring_view(argv[1]) == L"--restore")
    {
        const bool expected = std::wstring_view(argv[2]) == L"1";
        Check(settings::GetConfig().saveAnywhere == expected, "startup config flag");
        Check(debug_menu::SaveAnywhereEnabled() == expected, "startup debug switch");
        return 0;
    }
    Check(argc == 1, "unexpected arguments");
    wchar_t executable[32768]{};
    Check(GetModuleFileNameW(nullptr, executable, DWORD(std::size(executable))) != 0, "own executable path");
    Check(!std::filesystem::exists("settings.ini"), "run in empty isolated directory");
    Check(!settings::GetConfig().saveAnywhere && !debug_menu::SaveAnywhereEnabled(), "missing file defaults off");
    CheckFreshProcess(executable, "0");

    Write("width=1600\n");
    Check(!settings::Read().saveAnywhere, "missing key defaults off");
    CheckFreshProcess(executable, "0");
    Write("save_anywhere=2\n");
    Check(!settings::Read().saveAnywhere, "invalid key defaults off");

    debug_menu::SetSaveAnywhereEnabled(true);
    Check(debug_menu::SaveAnywhereEnabled() && settings::GetConfig().saveAnywhere,
        "runtime enable takes effect and updates config");
    Check(settings::Read().saveAnywhere && Contents().find("save_anywhere=1\n") != std::string::npos,
        "enabled value survives disk readback");
    CheckFreshProcess(executable, "1");

    settings::Config preview = settings::GetConfig();
    preview.width = 2000;
    settings::PreviewConfig(preview);
    debug_menu::SetSaveAnywhereEnabled(false);
    Check(!debug_menu::SaveAnywhereEnabled() && !settings::GetConfig().saveAnywhere,
        "runtime disable takes effect");
    Check(settings::Read().width == 1280, "toggle does not commit unconfirmed graphics preview");
    Check(!settings::Read().saveAnywhere && Contents().find("save_anywhere=0\n") != std::string::npos,
        "disabled value survives disk readback");
    CheckFreshProcess(executable, "0");

    debug_menu::SetSaveAnywhereEnabled(true);
    settings::Config graphics = settings::GetConfig();
    graphics.width = 1800;
    Check(settings::SaveConfig(graphics), "save ordinary settings");
    Check(settings::Read().saveAnywhere, "ordinary save retains debug-only preference");
    Check(settings::SaveDebugLanguage(1) && settings::Read().saveAnywhere,
        "debug language save retains save-anywhere preference");
    std::puts("PASS isolated save-anywhere toggle, INI roundtrip and fresh-process restore");
    return 0;
}
