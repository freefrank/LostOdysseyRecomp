#include <settings/restart.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace
{
void Require(bool condition, const char *message)
{
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}

DWORD Run(const std::wstring &executable, const std::wstring &arguments)
{
    std::wstring command = settings::restart::QuoteArgument(executable) + L" " + arguments;
    command.push_back(L'\0');
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    Require(CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE,
            "could not launch restart fixture child");
    CloseHandle(process.hThread);
    Require(WaitForSingleObject(process.hProcess, 15000) == WAIT_OBJECT_0, "restart fixture child timed out");
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    return code;
}

std::vector<std::wstring> Arguments()
{
    int count = 0;
    auto values = CommandLineToArgvW(GetCommandLineW(), &count);
    std::vector<std::wstring> result;
    for (int i = 0; values && i < count; ++i) result.emplace_back(values[i]);
    if (values) LocalFree(values);
    return result;
}
}

int main()
{
    const auto handshake = settings::restart::WaitForParentIfRestartChild();
    if (handshake == settings::restart::ChildHandshake::Invalid) return 73;
    const auto args = Arguments();
    if (handshake == settings::restart::ChildHandshake::Waited)
    {
        for (size_t i = 1; i + 1 < args.size(); ++i)
            if (args[i] == L"--handshake-child")
            {
                std::ofstream(args[i + 1], std::ios::binary) << "parent-exited-before-child-work\n";
                return 0;
            }
        return 74;
    }

    for (size_t i = 1; i + 1 < args.size(); ++i)
        if (args[i] == L"--handshake-parent")
        {
            wchar_t executable[32768]{};
            Require(GetModuleFileNameW(nullptr, executable, DWORD(std::size(executable))) != 0,
                    "fixture executable path unavailable");
            const std::wstring childArguments = L"--handshake-child " + settings::restart::QuoteArgument(args[i + 1]);
            return settings::restart::LaunchWaitingProcess(executable, childArguments, 5000) ? 0 : 75;
        }

    settings::Config before, after = before;
    Require(!settings::restart::Required(before, after), "unchanged config requires restart");
    after.width = 1920;
    Require(!settings::restart::Required(before, after), "live display change requires restart");
    after.graphicsBackend = settings::GraphicsBackend::Vulkan;
    Require(settings::restart::Required(before, after), "backend change did not require restart");
    after = before;
    after.gameLanguage = 2;
    Require(settings::restart::Required(before, after), "game language change did not require restart");

    wchar_t executable[32768]{};
    Require(GetModuleFileNameW(nullptr, executable, DWORD(std::size(executable))) != 0,
            "fixture executable path unavailable");
    const auto marker = std::filesystem::temp_directory_path() /
        (L"lo-restart-" + std::to_wstring(GetCurrentProcessId()) + L".txt");
    std::error_code ignored;
    std::filesystem::remove(marker, ignored);
    Require(Run(executable, L"--handshake-parent " + settings::restart::QuoteArgument(marker.wstring())) == 0,
            "handshake parent failed");
    for (int i = 0; i < 100 && !std::filesystem::exists(marker); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    std::ifstream markerInput(marker, std::ios::binary);
    std::string markerText;
    std::getline(markerInput, markerText);
    Require(markerText == "parent-exited-before-child-work", "child ran before its parent exited");
    std::filesystem::remove(marker, ignored);

    Require(Run(executable, L"--wait-process") == 73, "missing restart PID did not fail closed");
    Require(Run(executable, L"--wait-process abc --restart-ready event") == 73,
            "invalid restart PID did not fail closed");
    Require(Run(executable, L"--restart-ready event") == 73, "unpaired restart event did not fail closed");
    std::puts("restart contract: classification, fail-closed parsing, ready/wait ordering passed");
    return 0;
}
