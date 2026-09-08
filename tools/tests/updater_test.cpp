#include "updater/update.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <thread>

#include "../../tools/XenosRecomp/thirdparty/smol-v/testing/external/miniz/miniz.h"

namespace fs = std::filesystem;

namespace
{
int failures = 0;

void Expect(bool value, const char *message)
{
    if (!value) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

bool Write(const fs::path &path, std::string_view value)
{
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(value.data(), std::streamsize(value.size()));
    return bool(output);
}

std::string Read(const fs::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::wstring Quote(std::wstring_view value)
{
    std::wstring result = L"\"";
    for (const auto c : value) { if (c == L'\"') result += L'\\'; result += c; }
    return result + L"\"";
}

bool CreatePackage(const fs::path &archivePath, std::string_view version,
                   const std::map<std::string, fs::path> &files, bool corruptFirstHash = false)
{
    std::string error;
    std::string manifest = "{\n  \"version\": \"" + std::string(version) +
                           "\",\n  \"development_build\": false,\n  \"files\": {\n";
    bool first = true;
    for (const auto &[name, source] : files)
    {
        if (!first) manifest += ",\n";
        first = false;
        auto hash = updater::Sha256File(source, error);
        if (corruptFirstHash) { hash.assign(64, '0'); corruptFirstHash = false; }
        manifest += "    \"" + name + "\": \"" + hash + "\"";
    }
    manifest += "\n  }\n}\n";
    mz_zip_archive archive{};
    const auto narrow = archivePath.string();
    if (!mz_zip_writer_init_file(&archive, narrow.c_str(), 0)) return false;
    bool ok = mz_zip_writer_add_mem(&archive, "LostOdysseyRecomp-test/manifest.json", manifest.data(), manifest.size(),
                                    MZ_BEST_COMPRESSION) != 0;
    for (const auto &[name, source] : files)
    {
        const auto archiveName = "LostOdysseyRecomp-test/" + name;
        const auto sourceName = source.string();
        ok = ok && mz_zip_writer_add_file(&archive, archiveName.c_str(), sourceName.c_str(), nullptr, 0,
                                          MZ_BEST_COMPRESSION) != 0;
    }
    ok = ok && mz_zip_writer_finalize_archive(&archive) != 0;
    mz_zip_writer_end(&archive);
    return ok;
}

updater::StagedUpdate BuildStaged(const fs::path &caseRoot, const fs::path &probe, const fs::path &helper,
                                  const fs::path &marker)
{
    const auto source = caseRoot / "source";
    const auto install = caseRoot / "install";
    const auto operation = install / ".update" / "operation-test";
    Write(source / "bin/runtime.dll", "new-runtime");
    fs::copy_file(probe, source / "LostOdysseyRecomp.exe", fs::copy_options::overwrite_existing);
    fs::copy_file(helper, source / "LostOdysseyUpdater.exe", fs::copy_options::overwrite_existing);
    const std::map<std::string, fs::path> files = {
        {"LostOdysseyRecomp.exe", source / "LostOdysseyRecomp.exe"},
        {"LostOdysseyUpdater.exe", source / "LostOdysseyUpdater.exe"},
        {"bin/runtime.dll", source / "bin/runtime.dll"}
    };
    fs::create_directories(operation);
    const auto zip = operation / "download.zip";
    Expect(CreatePackage(zip, "v9.9.9", files), "create synthetic release ZIP");
    updater::StagedUpdate staged;
    staged.installRoot = fs::absolute(install);
    std::string error;
    Expect(updater::StageArchive(zip, operation, "v9.9.9", staged, error), error.c_str());
    staged.installRoot = fs::absolute(install);
    fs::copy_file(staged.stageRoot / "LostOdysseyUpdater.exe", staged.runnerPath,
                  fs::copy_options::overwrite_existing);
    Expect(updater::WriteApplyPlan(staged, fs::absolute(install / "LostOdysseyRecomp.exe"),
                                  {L"--updated-marker", fs::absolute(marker).wstring()}, error), error.c_str());
    return staged;
}

bool LaunchAndWait(const std::wstring &command, DWORD &exitCode, DWORD timeout = 15000)
{
    auto mutableCommand = command;
    mutableCommand.push_back(L'\0');
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process))
        return false;
    CloseHandle(process.hThread);
    const auto wait = WaitForSingleObject(process.hProcess, timeout);
    const bool result = wait == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &exitCode);
    if (wait == WAIT_TIMEOUT) TerminateProcess(process.hProcess, ERROR_TIMEOUT);
    CloseHandle(process.hProcess);
    return result;
}

int ParentMode(const fs::path &helper, const fs::path &plan)
{
    const auto eventName = L"Local\\LostOdysseyUpdaterFixture-" + std::to_wstring(GetCurrentProcessId());
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
} // namespace

int wmain(int argc, wchar_t **argv)
{
    if (argc == 4 && std::wstring_view(argv[1]) == L"--parent") return ParentMode(argv[2], argv[3]);
    const fs::path root = fs::absolute("out/v0.5.0/updater/fixture-work");
    std::error_code filesystemError;
    fs::remove_all(root, filesystemError);
    fs::create_directories(root);
    const auto self = fs::absolute(argv[0]);
    const auto helper = self.parent_path() / "LostOdysseyUpdater.exe";
    const auto probe = self.parent_path() / "LoUpdaterProbe.exe";
    Expect(fs::is_regular_file(helper), "helper fixture binary exists");
    Expect(fs::is_regular_file(probe), "updated-process probe exists");

    auto a = updater::ParseVersion("v0.4.5"), b = updater::ParseVersion("v0.4.6"), pre = updater::ParseVersion("v0.4.6-rc.1");
    Expect(a && b && pre, "parse supported versions");
    Expect(updater::CompareVersions(*a, *b) < 0 && updater::CompareVersions(*pre, *b) < 0,
           "stable version ordering and no downgrade");
    std::string error;
    const std::string syntheticRelease = R"({"tag_name":"v0.4.6","assets":[
      {"name":"LostOdysseyRecomp-windows-x64-v0.4.6.zip","state":"uploaded","size":123,
       "digest":"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
       "browser_download_url":"https://github.com/freefrank/LostOdysseyRecomp/releases/download/v0.4.6/LostOdysseyRecomp-windows-x64-v0.4.6.zip"},
      {"name":"LostOdysseyRecomp-macos-arm64-v0.4.6.zip","state":"uploaded","size":456,
       "digest":"sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
       "browser_download_url":"https://github.com/freefrank/LostOdysseyRecomp/releases/download/v0.4.6/LostOdysseyRecomp-macos-arm64-v0.4.6.zip"}]})";
    auto release = updater::ParseGitHubRelease(syntheticRelease, error);
    auto asset = release ? updater::SelectAsset(*release, "windows", "x64", error) : std::nullopt;
    Expect(asset && asset->size == 123, "select exact OS/architecture asset with GitHub digest");
    Expect(!updater::IsSafePayloadPath("save/slot.bin", error) && !updater::IsSafePayloadPath("../escape", error),
           "reject user-data and traversal payload paths");

    const auto corruptRoot = root / "corrupt-case";
    Write(corruptRoot / "source/LostOdysseyRecomp.exe", "tampered-payload");
    fs::create_directories(corruptRoot / "operation");
    const std::map<std::string, fs::path> corruptFiles = {
        {"LostOdysseyRecomp.exe", corruptRoot / "source/LostOdysseyRecomp.exe"}
    };
    Expect(CreatePackage(corruptRoot / "operation/download.zip", "v9.9.9", corruptFiles, true),
           "create wrong-checksum archive");
    updater::StagedUpdate corruptStage;
    error.clear();
    Expect(!updater::StageArchive(corruptRoot / "operation/download.zip", corruptRoot / "operation", "v9.9.9",
                                  corruptStage, error) && error.find("SHA256 mismatch") != std::string::npos,
           "reject a ZIP whose payload does not match its manifest checksum");

    const auto rollbackRoot = root / "rollback-case";
    const auto marker1 = rollbackRoot / "marker.txt";
    Write(rollbackRoot / "install/LostOdysseyRecomp.exe", "old-game");
    Write(rollbackRoot / "install/LostOdysseyUpdater.exe", "old-helper");
    Write(rollbackRoot / "install/bin/runtime.dll", "old-runtime");
    Write(rollbackRoot / "install/settings.ini", "automatic_updates=1\n");
    Write(rollbackRoot / "install/save/slot.bin", "user-save");
    Write(rollbackRoot / "install/profile/user.dat", "user-profile");
    Write(rollbackRoot / "install/cache/shader.bin", "user-cache");
    auto rollbackStage = BuildStaged(rollbackRoot, probe, helper, marker1);
    auto rollbackPlan = updater::ReadApplyPlan(rollbackStage.planPath, error);
    Expect(rollbackPlan && updater::ValidateApplyPlan(*rollbackPlan, error), error.c_str());
    Expect(rollbackPlan && !updater::ApplyWithRollback(*rollbackPlan, {2}, error), "fault injection triggers rollback");
    Expect(Read(rollbackRoot / "install/LostOdysseyRecomp.exe") == "old-game" &&
           Read(rollbackRoot / "install/LostOdysseyUpdater.exe") == "old-helper" &&
           Read(rollbackRoot / "install/bin/runtime.dll") == "old-runtime", "all replaced files roll back");
    Expect(Read(rollbackRoot / "install/settings.ini") == "automatic_updates=1\n" &&
           Read(rollbackRoot / "install/save/slot.bin") == "user-save" &&
           Read(rollbackRoot / "install/profile/user.dat") == "user-profile" &&
           Read(rollbackRoot / "install/cache/shader.bin") == "user-cache", "user data survives rollback");

    SetEnvironmentVariableW(L"LO_UPDATER_SILENT", L"1");
    DWORD invalidExit = 0;
    Expect(LaunchAndWait(Quote(helper.wstring()) + L" --invalid", invalidExit) && invalidExit != 0,
           "helper rejects malformed handshake without touching installation");

    const auto successRoot = root / "success-case";
    const auto marker2 = successRoot / "updated.marker";
    Write(successRoot / "install/LostOdysseyRecomp.exe", "old-game");
    Write(successRoot / "install/LostOdysseyUpdater.exe", "old-helper");
    Write(successRoot / "install/bin/runtime.dll", "old-runtime");
    Write(successRoot / "install/settings.ini", "automatic_updates=0\n");
    Write(successRoot / "install/game-path.txt", "D:/owned-game\n");
    Write(successRoot / "install/save/slot.bin", "user-save");
    auto successStage = BuildStaged(successRoot, probe, helper, marker2);
    DWORD parentExit = 0;
    const std::wstring parentCommand = Quote(self.wstring()) + L" --parent " + Quote(successStage.runnerPath.wstring()) +
                                       L" " + Quote(successStage.planPath.wstring());
    Expect(LaunchAndWait(parentCommand, parentExit) && parentExit == 0, "helper becomes ready before old process exits");
    for (int i = 0; i < 100 && !fs::exists(marker2); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Expect(fs::exists(marker2), "helper replaces files then launches the updated executable");
    Expect(Read(successRoot / "install/bin/runtime.dll") == "new-runtime", "successful helper installs payload");
    Expect(Read(successRoot / "install/settings.ini") == "automatic_updates=0\n" &&
           Read(successRoot / "install/game-path.txt") == "D:/owned-game\n" &&
           Read(successRoot / "install/save/slot.bin") == "user-save", "successful update preserves settings, path and save");

    const auto offlineRoot = root / "offline-case/install";
    Write(offlineRoot / "LostOdysseyRecomp.exe", "formal-package-binary");
    const auto formalHash = updater::Sha256File(offlineRoot / "LostOdysseyRecomp.exe", error);
    Write(offlineRoot / "manifest.json", "{\"version\":\"v9.9.9\",\"development_build\":false,\"files\":{\"LostOdysseyRecomp.exe\":\"" + formalHash + "\"}}");
    updater::StartupOptions offlineOptions{"9.9.9", offlineRoot, offlineRoot / "LostOdysseyRecomp.exe", {}, true,
                                           0, "http://127.0.0.1:1/releases/latest"};
    auto offline = updater::PrepareAtStartup(offlineOptions);
    Expect(offline.status == updater::StartupStatus::Offline, "offline update check fails open to normal startup");
    SetEnvironmentVariableW(L"LO_NO_UPDATE", L"1");
    auto disabled = updater::PrepareAtStartup(offlineOptions);
    SetEnvironmentVariableW(L"LO_NO_UPDATE", nullptr);
    Expect(disabled.status == updater::StartupStatus::Disabled && disabled.detail == "LO_NO_UPDATE",
           "LO_NO_UPDATE bypasses networking before the formal-package check");

    if (failures) { std::cerr << failures << " updater fixture(s) failed\n"; return 1; }
    std::cout << "PASS: updater version/asset/integrity/staging/rollback/helper/preservation fixtures\n";
    return 0;
}
