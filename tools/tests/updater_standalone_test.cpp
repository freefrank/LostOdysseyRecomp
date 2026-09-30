#include "updater/standalone.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
namespace fs = std::filesystem;
namespace {
int failures = 0, checks = 0;
void Expect(bool ok, const char *what) { ++checks; if (!ok) { ++failures; std::cerr << "FAIL: " << what << '\n'; } }
void Write(const fs::path &p, const std::string &s) { std::ofstream(p, std::ios::binary | std::ios::trunc) << s; }
std::wstring Quote(const fs::path &p) { return L"\"" + p.wstring() + L"\""; }
updater::StartupStatus outcome;
int callbacks = 0;
fs::path readyProbe, readyMarker;
updater::StartupResult PrepareReady(const updater::StartupOptions &o) {
    updater::StagedUpdate staged;
    staged.version = "9.9.9";
    staged.installRoot = o.installRoot;
    staged.operationRoot = o.installRoot / ".update/standalone-fixture";
    staged.stageRoot = staged.operationRoot / "stage";
    staged.runnerPath = staged.operationRoot / "runner.exe";
    staged.planPath = staged.operationRoot / "apply-plan.json";
    fs::create_directories(staged.stageRoot);
    fs::copy_file(readyProbe, staged.stageRoot / "LostOdysseyRecomp.exe");
    fs::copy_file(o.installRoot / "LostOdysseyUpdater.exe", staged.stageRoot / "LostOdysseyUpdater.exe");
    fs::copy_file(o.installRoot / "LostOdysseyUpdater.exe", staged.runnerPath);
    std::string error;
    const auto gameSize = fs::file_size(staged.stageRoot / "LostOdysseyRecomp.exe");
    Write(staged.stageRoot / "manifest.json", "{\"version\":\"9.9.9\",\"development_build\":false,\"files\":{\"LostOdysseyRecomp.exe\":" + std::to_string(gameSize) + "}}");
    for (const char *name : {"LostOdysseyRecomp.exe", "LostOdysseyUpdater.exe", "manifest.json"})
        staged.files.push_back({name});
    if (!error.empty() || !updater::WriteApplyPlan(staged, o.executable,
            {L"--context-marker", readyMarker.wstring(), L"standalone-\u4e2d\u6587"}, error))
        return {updater::StartupStatus::IntegrityFailed, error, {}};
    return {updater::StartupStatus::Ready, "fixture", std::move(staged)};
}
updater::StartupResult Prepare(const updater::StartupOptions &o) {
    ++callbacks;
    Expect(o.currentVersion == "0.5.0" && o.automaticUpdates, "callback receives installed version and manual-check policy");
    return {outcome, "fixture", {}};
}
struct Child {
    PROCESS_INFORMATION pi{};
    bool Start(const fs::path &exe, const std::wstring &args, const fs::path &cwd) {
        STARTUPINFOW si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
        auto command = Quote(exe) + L" " + args;
        return CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                              nullptr, cwd.c_str(), &si, &pi) != FALSE;
    }
    DWORD Finish() {
        if (!pi.hProcess) return DWORD(-1);
        if (WaitForSingleObject(pi.hProcess, 15000) != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 99); WaitForSingleObject(pi.hProcess, 5000); return 99; }
        DWORD result = DWORD(-1); GetExitCodeProcess(pi.hProcess, &result); return result;
    }
    ~Child() { if (pi.hProcess) { if (WaitForSingleObject(pi.hProcess, 0) == WAIT_TIMEOUT) { TerminateProcess(pi.hProcess, 99); WaitForSingleObject(pi.hProcess, 5000); } CloseHandle(pi.hProcess); CloseHandle(pi.hThread); } }
};
}
int wmain(int argc, wchar_t **argv) {
    if (argc == 5 && std::wstring(argv[1]) == L"--ready") {
        readyProbe = argv[3]; readyMarker = argv[4];
        return updater::RunStandalone(argv[2], PrepareReady);
    }
    if (argc == 4 && std::wstring(argv[1]) == L"--hold") {
        HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, argv[2]);
        HANDLE stop = OpenEventW(SYNCHRONIZE, FALSE, argv[3]);
        if (!ready || !stop) return 2;
        SetEvent(ready); DWORD result = WaitForSingleObject(stop, 15000);
        CloseHandle(ready); CloseHandle(stop); return result == WAIT_OBJECT_0 ? 0 : 3;
    }
    SetEnvironmentVariableW(L"LO_UPDATER_SILENT", L"1");
    SetEnvironmentVariableW(L"LO_NO_UPDATE", L"1");
    const auto self = updater::CurrentExecutablePath();
    const auto helper = argc > 1 ? fs::absolute(argv[1]) : self.parent_path() / "LostOdysseyUpdater.exe";
    if (!fs::is_regular_file(helper)) { std::cerr << "Missing helper executable\n"; return 1; }
    const auto oldCwd = fs::current_path();
    const auto root = fs::temp_directory_path() / (L"LO standalone \u66f4\u65b0 " + std::to_wstring(GetCurrentProcessId()) + L" " + std::to_wstring(GetTickCount64()));
    const auto install = root / L"install \u6d4b\u8bd5";
    const auto unrelated = root / "unrelated cwd";
    fs::create_directories(install); fs::create_directories(unrelated);
    const auto localHelper = install / "LostOdysseyUpdater.exe";
    const auto game = install / "LostOdysseyRecomp.exe";
    fs::copy_file(helper, localHelper); fs::copy_file(self, game);
    fs::current_path(unrelated);
    std::string error;
    const auto gameSize = fs::file_size(game);
    auto manifest = [&](const std::string &version = "0.5.0", bool dev = false, bool entry = true) {
        Write(install / "manifest.json", "{\"version\":\"" + version + "\",\"development_build\":" + (dev ? "true" : "false") + ",\"files\":{\"" + (entry ? "LostOdysseyRecomp.exe" : "other.dll") + "\":" + std::to_string(gameSize) + "}}");
    };
    updater::StartupOptions options;
    auto configure = [&] { error.clear(); return updater::ConfigureStandalone(localHelper, options, error); };
    Expect(configure() && options.currentVersion == "0.0.0", "missing metadata permits recovery update");
    Expect(fs::equivalent(options.runnerSource, localHelper), "standalone apply runner uses the running helper");
    { Child child; Expect(child.Start(localHelper, L"", unrelated), "real noargs helper starts hidden"); Expect(child.Finish() == 0, "real noargs helper without manifest respects opt-out"); }
    Write(install / "manifest.json", "not json"); Expect(configure(), "malformed manifest permits recovery update");
    manifest("0.5.0", true); Expect(configure(), "development package permits update");
    manifest("invalid"); Expect(configure(), "invalid installed version permits recovery update");
    manifest("0.5.0", false, false); Expect(configure(), "missing executable manifest entry permits update");
    manifest();
    Expect(configure(), "formal older installed version accepted with newer helper");
    Expect(options.currentVersion == "0.5.0", "installed manifest version selected");
    Write(install / "source-version.txt", "0.5.4\n");
    Expect(configure() && options.currentVersion == "0.5.4", "payload version wins over stale manifest");
    Write(install / "manifest.json", "{\"version\":\"0.5.0\"}");
    fs::remove(install / "source-version.txt");
    Expect(configure() && options.currentVersion == "0.5.0", "version-only manifest accepted without hashes");
    manifest();
    Expect(fs::equivalent(options.installRoot, install) && fs::equivalent(options.executable, game), "Unicode install resolved independently of cwd");
    fs::rename(game, install / "game.saved"); Expect(configure() && options.currentVersion == "0.0.0", "missing game permits fresh install despite stale metadata");
    Expect(fs::equivalent(options.runnerSource, localHelper), "missing game still has a usable apply runner");
    fs::rename(install / "game.saved", game);
    Write(game, "tampered"); Expect(configure(), "modified game permits update");
    fs::copy_file(self, game, fs::copy_options::overwrite_existing);
    for (auto status : {updater::StartupStatus::UpToDate, updater::StartupStatus::Offline, updater::StartupStatus::Disabled, updater::StartupStatus::Cancelled}) {
        outcome = status;
        Expect(updater::RunStandalone(localHelper, Prepare) == (status == updater::StartupStatus::Offline ? 1 : 0), "injected standalone outcome exit code");
    }
    Expect(callbacks == 4, "all controller outcomes reached injected callback");
    { Child child; Expect(child.Start(localHelper, L"", unrelated), "formal noargs helper starts hidden"); Expect(child.Finish() == 0, "real noargs helper respects LO_NO_UPDATE without network"); }
    { Child child; Expect(child.Start(localHelper, L"--bogus", unrelated), "invalid-args helper starts hidden"); Expect(child.Finish() != 0, "real helper rejects bogus arguments"); }
    const auto readyName = L"Local\\LOStandaloneReady" + std::to_wstring(GetCurrentProcessId());
    const auto stopName = L"Local\\LOStandaloneStop" + std::to_wstring(GetCurrentProcessId());
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, readyName.c_str());
    HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, stopName.c_str());
    const auto holdArgs = L"--hold \"" + readyName + L"\" \"" + stopName + L"\"";
    {
        Child child; Expect(child.Start(game, holdArgs, unrelated), "fake installed game starts hidden");
        Expect(WaitForSingleObject(ready, 5000) == WAIT_OBJECT_0, "fake installed game ready");
        Expect(!updater::StandaloneGameClosed(game, error), "running exact installed game blocks update");
        SetEvent(stop); Expect(child.Finish() == 0, "fake installed game exits on event");
    }
    ResetEvent(ready); ResetEvent(stop);
    fs::copy_file(self, unrelated / "LostOdysseyRecomp.exe");
    {
        Child child; Expect(child.Start(unrelated / "LostOdysseyRecomp.exe", holdArgs, unrelated), "other installation fake game starts hidden");
        Expect(WaitForSingleObject(ready, 5000) == WAIT_OBJECT_0, "other installation fake game ready");
        Expect(updater::StandaloneGameClosed(game, error), "same basename in other installation does not block");
        SetEvent(stop); Expect(child.Finish() == 0, "other installation fake game exits on event");
    }
    CloseHandle(ready); CloseHandle(stop);
    fs::remove(game);
    fs::remove(install / "manifest.json");
    Expect(configure() && options.currentVersion == "0.0.0", "updater-only folder is installable");
    const auto marker = root / "ready-context.txt";
    const auto probe = helper.parent_path() / "LoUpdaterProbe.exe";
    Expect(fs::is_regular_file(probe), "WIN32 updated-process probe exists");
    {
        Child child;
        Expect(child.Start(self, L"--ready " + Quote(localHelper) + L" " + Quote(probe) + L" " + Quote(marker), unrelated),
               "standalone Ready parent starts hidden from unrelated cwd");
        Expect(child.Finish() == 0, "standalone Ready handshake parent exits normally");
    }
    const auto resultPath = install / ".update/last-result.txt";
    std::string result;
    for (int i = 0; i < 200; ++i) {
        std::ifstream input(resultPath);
        result.assign(std::istreambuf_iterator<char>(input), {});
        if (result.find("updated=9.9.9") != std::string::npos) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    Expect(result.find("updated=9.9.9") != std::string::npos, "real helper installs staged standalone update");
    Expect(!fs::exists(marker), "updated game never launches without consent in silent mode");
    std::ifstream installed(game, std::ios::binary), expected(probe, std::ios::binary);
    Expect(fs::file_size(game) == fs::file_size(probe) &&
        std::equal(std::istreambuf_iterator<char>(installed), std::istreambuf_iterator<char>(),
                   std::istreambuf_iterator<char>(expected), std::istreambuf_iterator<char>()),
           "game payload replaced with exact probe");
    fs::current_path(oldCwd);
    std::error_code cleanup;
    for (int i = 0; i < 100; ++i) {
        fs::remove_all(root, cleanup);
        if (!cleanup) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    Expect(!cleanup, "fixture-owned temporary directory removed");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
