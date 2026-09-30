#include "update.h"
#include "http.h"
#include "progress.h"
#include "posix_ui.h"
#include "../os/user_paths.h"

#include <cstdlib>
#if defined(__linux__) && !defined(_WIN32)
#include <unistd.h>
#endif

namespace updater
{
#if defined(__linux__) && !defined(_WIN32)
StartupResult PrepareAtStartup(const StartupOptions &options)
{
    StartupResult result;
    const char *disabled = std::getenv("LO_NO_UPDATE");
    if (!options.automaticUpdates || (disabled && std::string_view(disabled) != "0"))
    {
        result.status = StartupStatus::Disabled;
        result.detail = !options.automaticUpdates ? "automatic_updates=0" : "LO_NO_UPDATE";
        return result;
    }
    const bool flatpak = std::getenv("FLATPAK_ID") || std::filesystem::exists("/.flatpak-info");

    std::string error;
    const auto current = ParseVersion(options.currentVersion).value_or(*ParseVersion("0.0.0"));
    std::string releaseText;
    if (!ReadResponse(options.releaseApiUrl, 2 * 1024 * 1024, releaseText, error))
    {
        result.status = StartupStatus::Offline; result.detail = error; return result;
    }
    auto release = ParseGitHubRelease(releaseText, error);
    if (!release)
    {
        result.status = StartupStatus::InvalidRelease; result.detail = error; return result;
    }
    auto remote = ParseVersion(release->tag);
    if (!remote || !ShouldUpdateToLatest(current, *remote))
    {
        result.status = StartupStatus::UpToDate; result.detail = release->tag; return result;
    }
    if (flatpak)
    {
        ShowExternalUpdateNoticeSdl(release->tag, options.uiLanguage);
        result.status = StartupStatus::ExternalUpdateAvailable;
        result.detail = "Flatpak update available: " + release->tag + "; run flatpak update io.github.freefrank.LostOdysseyRecomp";
        return result;
    }

    const char *appImage = std::getenv("APPIMAGE");
    if (!appImage || *appImage == '\0')
    {
        result.status = StartupStatus::UnmanagedBuild;
        result.detail = "running build is not an AppImage";
        return result;
    }
    auto asset = SelectAsset(*release, "linux", "x64", error);
    if (!asset)
    {
        result.status = StartupStatus::NoCompatibleAsset; result.detail = error; return result;
    }
    const auto changelog = ReleaseChangelog(*release, options.uiLanguage);
    const bool accepted = options.confirmUpdate
        ? options.confirmUpdate(release->tag, changelog, options.uiLanguage)
        : ConfirmUpdateSdl(release->tag, changelog, options.uiLanguage);
    if (!accepted)
    {
        result.status = StartupStatus::Cancelled; result.detail = "user declined update"; return result;
    }
    const auto operationRoot = os::user_paths::StateDir() / ".update" /
                               ("operation-" + std::to_string(getpid()) + "-" + release->tag);
    std::error_code filesystemError;
    std::filesystem::create_directories(operationRoot, filesystemError);
    if (filesystemError) { result.status = StartupStatus::DownloadFailed; result.detail = "could not create update operation directory"; return result; }
    ProgressWindow progress(options.uiLanguage);
    const auto archive = operationRoot / asset->name;
    bool cancelled = false;
    if (!Download(asset->url, archive, asset->size, progress, error, cancelled))
    {
        result.status = cancelled ? StartupStatus::Cancelled : StartupStatus::DownloadFailed; result.detail = error; return result;
    }
    StagedUpdate update;
    update.version = release->tag;
    update.installRoot = std::filesystem::absolute(std::filesystem::path(appImage).parent_path());
    update.operationRoot = operationRoot;
    update.stageRoot = operationRoot / "stage";
    update.planPath = operationRoot / "apply-plan.json";
    std::filesystem::create_directories(update.stageRoot, filesystemError);
    if (filesystemError) { result.status = StartupStatus::IntegrityFailed; result.detail = "could not create AppImage staging directory"; return result; }
    const auto staged = update.stageRoot / std::filesystem::path(appImage).filename();
    std::filesystem::copy_file(archive, staged, std::filesystem::copy_options::overwrite_existing, filesystemError);
    if (filesystemError) { result.status = StartupStatus::IntegrityFailed; result.detail = "could not stage AppImage"; return result; }
    update.files.push_back({staged.filename()});
    if (!WriteApplyPlan(update, std::filesystem::absolute(appImage), CurrentLaunchArguments(), error, false))
    {
        result.status = StartupStatus::IntegrityFailed; result.detail = error; return result;
    }
    result.status = StartupStatus::Ready;
    result.detail = release->tag;
    result.update = std::move(update);
    return result;
}
#endif
} // namespace updater
