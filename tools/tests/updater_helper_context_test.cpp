#include "updater/update.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>

namespace fs = std::filesystem;

namespace
{
std::wstring Quote(std::wstring_view value)
{
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (const auto character : value)
    {
        if (character == L'\\') { ++slashes; continue; }
        if (character == L'\"') result.append(slashes * 2 + 1, L'\\');
        else result.append(slashes, L'\\');
        slashes = 0;
        result.push_back(character);
    }
    result.append(slashes * 2, L'\\');
    return result + L"\"";
}

std::string Utf8(std::wstring_view value)
{
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), int(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string result(size_t(size), '\0');
    if (size) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), int(value.size()),
                                  result.data(), size, nullptr, nullptr);
    return result;
}

int ParentMode(const fs::path &helper, const fs::path &plan)
{
    const auto eventName = L"Local\\LostOdysseyUpdaterContext-" + std::to_wstring(GetCurrentProcessId());
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, eventName.c_str());
    if (!ready) return 20;
    std::wstring command = Quote(helper.wstring()) + L" --apply-plan " + Quote(plan.wstring()) +
        L" --wait-process " + std::to_wstring(GetCurrentProcessId()) + L" --restart-ready " + Quote(eventName);
    command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(helper.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process))
    {
        CloseHandle(ready);
        return 21;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    const auto wait = WaitForSingleObject(ready, 10000);
    CloseHandle(ready);
    return wait == WAIT_OBJECT_0 ? 0 : 22;
}

bool LaunchParent(const fs::path &self, const fs::path &helper, const fs::path &plan, const fs::path &working)
{
    std::wstring command = Quote(self.wstring()) + L" --parent " + Quote(helper.wstring()) + L" " + Quote(plan.wstring());
    command.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(self.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        working.c_str(), &startup, &process)) return false;
    CloseHandle(process.hThread);
    const auto wait = WaitForSingleObject(process.hProcess, 15000);
    DWORD exitCode = 1;
    const bool ok = wait == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &exitCode) && exitCode == 0;
    CloseHandle(process.hProcess);
    return ok;
}
} // namespace

int wmain(int argc, wchar_t **argv)
{
    if (argc == 4 && std::wstring_view(argv[1]) == L"--parent") return ParentMode(argv[2], argv[3]);
    const auto self = fs::absolute(argv[0]);
    const auto binaryRoot = self.parent_path();
    const auto helper = binaryRoot / "LostOdysseyUpdater.exe";
    const auto probe = binaryRoot / "LoUpdaterProbe.exe";
    const auto root = fs::absolute("out/v0.5.0/updater/context-work");
    const auto install = root / "install";
    const auto operation = install / ".update/context-operation";
    const auto stage = operation / "stage";
    const auto caller = root / fs::path(L"caller-´-′-中文");
    const auto marker = root / "context.txt";
    std::error_code filesystemError;
    fs::remove_all(root, filesystemError);
    fs::create_directories(stage, filesystemError);
    fs::create_directories(caller, filesystemError);
    fs::copy_file(probe, install / "LostOdysseyRecomp.exe", fs::copy_options::overwrite_existing, filesystemError);
    fs::copy_file(probe, stage / "LostOdysseyRecomp.exe", fs::copy_options::overwrite_existing, filesystemError);
    std::string error;
    updater::StagedUpdate update;
    update.version = "v9.9.9";
    update.installRoot = install;
    update.operationRoot = operation;
    update.stageRoot = stage;
    update.planPath = operation / "apply-plan.json";
    update.files = {{"LostOdysseyRecomp.exe", updater::Sha256File(stage / "LostOdysseyRecomp.exe", error)}};
    const std::wstring unicodeArgument = L"context-´-′-中文";
    if (!error.empty() || !updater::WriteApplyPlan(update, install / "LostOdysseyRecomp.exe",
                                                    {L"--context-marker", marker.wstring(), unicodeArgument}, error))
    {
        std::cerr << "FAIL: could not prepare context fixture: " << error << '\n';
        return 1;
    }
    SetEnvironmentVariableW(L"LO_UPDATER_SILENT", L"1");
    if (!LaunchParent(self, helper, update.planPath, caller))
    {
        std::cerr << "FAIL: helper handshake parent failed\n";
        return 1;
    }
    for (int i = 0; i < 100 && !fs::exists(marker); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::ifstream input(marker, std::ios::binary);
    const std::string context((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const std::string expectedCwd = "cwd=" + Utf8(fs::weakly_canonical(caller).wstring()) + "\n";
    const std::string expectedArgument = "arg=" + Utf8(unicodeArgument) + "\n";
    if (context.find(expectedCwd) == std::string::npos || context.find(expectedArgument) == std::string::npos)
    {
        std::cerr << "FAIL: helper changed caller cwd or Unicode argument\n" << context;
        return 1;
    }
    std::cout << "PASS: updater helper preserves caller cwd and Unicode arguments\n";
    return 0;
}
