#include "update.h"
#include "progress.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#endif

#include <array>
#include <cstdlib>
#include <chrono>
#include <cwchar>
#include <fstream>
#include <limits>

namespace updater
{
namespace
{
#ifdef _WIN32
struct InternetHandle
{
    HINTERNET value = nullptr;
    ~InternetHandle() { if (value) WinHttpCloseHandle(value); }
};

std::wstring Utf16(std::string_view value)
{
    if (value.empty()) return {};
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), int(value.size()), nullptr, 0);
    if (!count) return {};
    std::wstring result(size_t(count), L'\0');
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), int(value.size()), result.data(), count)) return {};
    return result;
}

bool OpenRequest(std::string_view url, InternetHandle &session, InternetHandle &connection,
                 InternetHandle &request, std::string &error)
{
    const auto wide = Utf16(url);
    if (wide.empty()) { error = "update URL is not valid UTF-8"; return false; }
    URL_COMPONENTSW components{};
    components.dwStructSize = sizeof(components);
    components.dwHostNameLength = DWORD(-1);
    components.dwUrlPathLength = DWORD(-1);
    components.dwExtraInfoLength = DWORD(-1);
    if (!WinHttpCrackUrl(wide.c_str(), DWORD(wide.size()), 0, &components) ||
        (components.nScheme != INTERNET_SCHEME_HTTPS && components.nScheme != INTERNET_SCHEME_HTTP))
    {
        error = "update URL is invalid";
        return false;
    }
    std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength) path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    session.value = WinHttpOpen(L"LostOdysseyRecomp-Updater/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.value) { error = "WinHTTP session could not be created"; return false; }
    WinHttpSetTimeouts(session.value, 1500, 1500, 3000, 5000);
    connection.value = WinHttpConnect(session.value, host.c_str(), components.nPort, 0);
    if (!connection.value) { error = "WinHTTP connection could not be created"; return false; }
    const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    request.value = WinHttpOpenRequest(connection.value, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request.value) { error = "WinHTTP request could not be created"; return false; }
    DWORD redirectLimit = 3;
    WinHttpSetOption(request.value, WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS, &redirectLimit, sizeof(redirectLimit));
    const wchar_t headers[] = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if (!WinHttpSendRequest(request.value, headers, DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.value, nullptr))
    {
        error = "update request failed with Win32 error " + std::to_string(GetLastError());
        return false;
    }
    DWORD status = 0, statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) || status != 200)
    {
        error = "update server returned HTTP " + std::to_string(status);
        return false;
    }
    return true;
}

bool ReadResponse(std::string_view url, size_t limit, std::string &body, std::string &error)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
    InternetHandle session, connection, request;
    if (!OpenRequest(url, session, connection, request, error)) return false;
    std::array<char, 64 * 1024> buffer{};
    while (true)
    {
        if (std::chrono::steady_clock::now() >= deadline)
        {
            error = "update metadata exceeded its overall time limit";
            return false;
        }
        DWORD read = 0;
        if (!WinHttpReadData(request.value, buffer.data(), DWORD(buffer.size()), &read))
        {
            error = "could not read update response";
            return false;
        }
        if (!read) break;
        if (body.size() + read > limit) { error = "update response exceeded its size limit"; return false; }
        body.append(buffer.data(), read);
    }
    return true;
}

bool Download(std::string_view url, const std::filesystem::path &destination, uint64_t expectedSize,
              ProgressWindow &progress, std::string &error, bool &cancelled)
{
    if (!expectedSize || expectedSize > 1024ull * 1024 * 1024)
    {
        error = "update asset size is outside the supported range";
        return false;
    }
    InternetHandle session, connection, request;
    if (!OpenRequest(url, session, connection, request, error)) return false;
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) { error = "could not create update download"; return false; }
    std::array<char, 64 * 1024> buffer{};
    uint64_t total = 0;
    while (true)
    {
        if (progress.Cancelled()) { cancelled = true; error = "update cancelled by user"; return false; }
        DWORD read = 0;
        if (!WinHttpReadData(request.value, buffer.data(), DWORD(buffer.size()), &read))
        {
            error = "could not read update download";
            return false;
        }
        if (!read) break;
        if (total > expectedSize || read > expectedSize - total)
        {
            error = "update download exceeded the declared asset size";
            return false;
        }
        output.write(buffer.data(), read);
        total += read;
        progress.SetDownloadProgress(total, expectedSize);
    }
    output.flush();
    if (!output || total != expectedSize) { error = "update download size mismatch"; return false; }
    return true;
}
#endif

std::optional<PackageManifest> ReadInstalledManifest(const std::filesystem::path &path, std::string &error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (text.size() > 4 * 1024 * 1024) { error = "installed manifest exceeds size limit"; return std::nullopt; }
    return ParsePackageManifest(text, error);
}

} // namespace

StartupResult PrepareAtStartup(const StartupOptions &options)
{
    StartupResult result;
    char disabledValue[8]{};
#ifdef _WIN32
    const bool environmentDisabled = GetEnvironmentVariableA("LO_NO_UPDATE", disabledValue, DWORD(std::size(disabledValue))) > 0 &&
                                     std::string_view(disabledValue) != "0";
#else
    const char *disabled = std::getenv("LO_NO_UPDATE");
    const bool environmentDisabled = disabled && std::string_view(disabled) != "0";
#endif
    if (!options.automaticUpdates || environmentDisabled)
    {
        result.status = StartupStatus::Disabled;
        result.detail = environmentDisabled ? "LO_NO_UPDATE" : "automatic_updates=0";
        return result;
    }
    std::string error;
    auto installed = ReadInstalledManifest(options.installRoot / "manifest.json", error);
    if (!installed)
    {
        result.status = StartupStatus::UnmanagedBuild;
        result.detail = error.empty() ? "no adjacent release manifest" : error;
        return result;
    }
    const auto current = ParseVersion(options.currentVersion);
    const auto package = ParseVersion(installed->version);
    if (installed->developmentBuild || !current || !package || CompareVersions(*current, *package) != 0)
    {
        result.status = StartupStatus::CurrentPackageMismatch;
        result.detail = installed->developmentBuild ? "development package" : "source/package version mismatch";
        return result;
    }
#ifdef _WIN32
    std::string releaseText;
    if (!ReadResponse(options.releaseApiUrl, 2 * 1024 * 1024, releaseText, error))
    {
        result.status = StartupStatus::Offline;
        result.detail = error;
        return result;
    }
    auto release = ParseGitHubRelease(releaseText, error);
    if (!release)
    {
        result.status = StartupStatus::InvalidRelease;
        result.detail = error;
        return result;
    }
    auto remote = ParseVersion(release->tag);
    if (!remote || !ShouldUpdateToLatest(*current, *remote))
    {
        result.status = StartupStatus::UpToDate;
        result.detail = release->tag;
        return result;
    }
    auto asset = SelectAsset(*release, "windows", "x64", error);
    if (!asset)
    {
        result.status = StartupStatus::NoCompatibleAsset;
        result.detail = error;
        return result;
    }
    const auto operationName = "operation-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64());
    const auto operationRoot = std::filesystem::absolute(options.installRoot / ".update" / operationName);
    std::error_code filesystemError;
    std::filesystem::create_directories(operationRoot, filesystemError);
    if (filesystemError)
    {
        result.status = StartupStatus::DownloadFailed;
        result.detail = "could not create update operation directory";
        return result;
    }
    ProgressWindow progress(options.uiLanguage);
    const auto archive = operationRoot / "download.zip";
    bool cancelled = false;
    if (!Download(asset->url, archive, asset->size, progress, error, cancelled))
    {
        std::filesystem::remove_all(operationRoot, filesystemError);
        result.status = cancelled ? StartupStatus::Cancelled : StartupStatus::DownloadFailed;
        result.detail = error;
        return result;
    }
    progress.SetPhase(ProgressPhase::Verifying);
    if (Sha256File(archive, error) != asset->sha256)
    {
        std::filesystem::remove_all(operationRoot, filesystemError);
        result.status = StartupStatus::IntegrityFailed;
        result.detail = error.empty() ? "GitHub asset digest mismatch" : error;
        return result;
    }
    StagedUpdate update;
    update.installRoot = std::filesystem::absolute(options.installRoot);
    progress.SetPhase(ProgressPhase::CheckingPackage);
    if (!StageArchive(archive, operationRoot, release->tag, update, error))
    {
        std::filesystem::remove_all(operationRoot, filesystemError);
        result.status = StartupStatus::IntegrityFailed;
        result.detail = error;
        return result;
    }
    update.installRoot = std::filesystem::absolute(options.installRoot);
    const auto stagedHelper = update.stageRoot / "LostOdysseyUpdater.exe";
    if (!std::filesystem::is_regular_file(stagedHelper) ||
        !std::filesystem::copy_file(stagedHelper, update.runnerPath, std::filesystem::copy_options::overwrite_existing,
                                    filesystemError))
    {
        std::filesystem::remove_all(operationRoot, filesystemError);
        result.status = StartupStatus::IntegrityFailed;
        result.detail = "update package does not contain a usable helper";
        return result;
    }
    if (!WriteApplyPlan(update, std::filesystem::absolute(options.executable), options.launchArguments, error))
    {
        std::filesystem::remove_all(operationRoot, filesystemError);
        result.status = StartupStatus::IntegrityFailed;
        result.detail = error;
        return result;
    }
    progress.SetPhase(ProgressPhase::Ready);
    result.status = StartupStatus::Ready;
    result.detail = release->tag;
    result.update = std::move(update);
    return result;
#else
    result.status = StartupStatus::UnmanagedBuild;
    result.detail = "updater is not implemented for this platform";
    return result;
#endif
}

std::wstring ApplyHelperArguments(const std::filesystem::path &planPath)
{
    std::wstring escaped = L"\"";
    for (const auto character : planPath.wstring())
    {
        if (character == L'\"') escaped += L'\\';
        escaped += character;
    }
    escaped += L"\"";
    return L"--apply-plan " + escaped;
}

std::filesystem::path CurrentExecutablePath()
{
#ifdef _WIN32
    std::wstring value(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, value.data(), DWORD(value.size()));
    if (length && length < value.size()) { value.resize(length); return std::filesystem::path(value); }
#endif
    return {};
}

std::vector<std::wstring> CurrentLaunchArguments()
{
    std::vector<std::wstring> result;
#ifdef _WIN32
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 1; arguments && i < count; ++i)
    {
        const std::wstring_view argument(arguments[i]);
        if ((argument == L"--wait-process" || argument == L"--restart-ready") && i + 1 < count)
        {
            ++i;
            continue;
        }
        result.emplace_back(argument);
    }
    if (arguments) LocalFree(arguments);
#endif
    return result;
}
} // namespace updater
