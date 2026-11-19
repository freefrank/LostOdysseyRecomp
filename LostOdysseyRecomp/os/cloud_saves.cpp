#include "cloud_saves.h"
#include <os/platform.h>

#if LO_PLATFORM_APPLE && defined(LO_ICLOUD_CONTAINER)
#include <os/logger.h>
#include <settings/config.h>
#if LO_PLATFORM_IOS
#include <os/ios_support.h>
#endif
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <system_error>
#include <thread>
#include <vector>

namespace os::cloud_saves
{
namespace
{
namespace fs = std::filesystem;
using Clock = fs::file_time_type::clock;

std::mutex g_mutex;
fs::path g_local;
fs::path g_cloud;       // <container>/Documents/Saves; empty when unavailable
bool g_enabled = false;
std::string g_lastSync = "not synced yet";

// Two seconds absorbs file systems that round modification times.
constexpr auto kTolerance = std::chrono::seconds(2);
constexpr size_t kBackupsKept = 10;

bool IsPlaceholder(const fs::path& name)
{
    const auto text = name.filename().string();
    return text.size() > 7 && text.front() == '.' && text.ends_with(".icloud");
}

// The slot's save time: the newest of the game's own files (save.bin); the
// host's ".lo-" metadata only counts for a slot without game data.
std::optional<fs::file_time_type> SlotTime(const fs::path& slot)
{
    std::error_code ec;
    std::optional<fs::file_time_type> data, any;
    for (const auto& entry : fs::directory_iterator(slot, ec))
    {
        if (!entry.is_regular_file(ec)) continue;
        if (IsPlaceholder(entry.path())) return std::nullopt; // not downloaded yet
        const auto time = entry.last_write_time(ec);
        if (ec) continue;
        if (!any || time > *any) any = time;
        if (!entry.path().filename().string().starts_with(".lo-") && (!data || time > *data)) data = time;
    }
    return data ? data : any;
}

std::vector<std::string> SlotNames(const fs::path& root)
{
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root, ec))
    {
        const auto name = entry.path().filename().string();
        if (entry.is_directory(ec) && !name.empty() && name.front() != '.') names.push_back(name);
    }
    return names;
}

// Copies a slot's files into destination through a sibling temporary folder,
// so a reader never sees half a slot. Modification times are kept so the next
// comparison sees both copies as equal.
bool CopySlot(const fs::path& source, const fs::path& destination, std::string& error)
{
    std::error_code ec;
    const auto temporary = destination.parent_path() / ("." + destination.filename().string() + ".sync-tmp");
    fs::remove_all(temporary, ec);
    fs::create_directories(temporary, ec);
    if (ec) { error = "create " + temporary.string() + ": " + ec.message(); return false; }
    for (const auto& entry : fs::directory_iterator(source, ec))
    {
        if (!entry.is_regular_file(ec) || IsPlaceholder(entry.path())) continue;
        const auto target = temporary / entry.path().filename();
        fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, ec);
        if (ec) { error = "copy " + entry.path().string() + ": " + ec.message(); fs::remove_all(temporary, ec); return false; }
        fs::last_write_time(target, entry.last_write_time(ec), ec);
    }
    if (ec) { error = "read " + source.string() + ": " + ec.message(); fs::remove_all(temporary, ec); return false; }
    fs::remove_all(destination, ec);
    fs::rename(temporary, destination, ec);
    if (ec) { error = "publish " + destination.string() + ": " + ec.message(); return false; }
    return true;
}

// Keeps the replaced local slot under save/.cloud-backup/<slot>-<time>.
void BackupLocalSlot(const fs::path& slot)
{
    std::error_code ec;
    const auto backups = g_local / ".cloud-backup";
    fs::create_directories(backups, ec);
    const auto stamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const auto target = backups / (slot.filename().string() + "-" + std::to_string(stamp));
    // A move keeps the files' save times; CopySlot then writes a fresh folder.
    fs::rename(slot, target, ec);
    if (ec) LOG_WARNING("iCloud saves: could not back up {}: {}", slot.string(), ec.message());
    else LOG_INFO("iCloud saves: kept the replaced local slot as {}", target.string());

    // Oldest first by the time in the name (<slot>-<seconds>).
    const auto stampOf = [](const fs::path& path) {
        const auto name = path.filename().string();
        const auto dash = name.rfind('-');
        return dash == std::string::npos ? 0ll : std::atoll(name.c_str() + dash + 1);
    };
    std::vector<fs::path> entries;
    for (const auto& entry : fs::directory_iterator(backups, ec)) entries.push_back(entry.path());
    if (entries.size() <= kBackupsKept) return;
    std::sort(entries.begin(), entries.end(), [&](const auto& a, const auto& b) { return stampOf(a) < stampOf(b); });
    for (size_t i = 0; i + kBackupsKept < entries.size(); ++i) fs::remove_all(entries[i], ec);
}

fs::path ResolveCloudRoot()
{
#if LO_PLATFORM_IOS
    const auto documents = os::ios::UbiquityDocuments(LO_ICLOUD_CONTAINER);
    return documents.empty() ? fs::path{} : documents / "Saves";
#else
    // macOS syncs every container of the account into ~/Library/Mobile
    // Documents, so this build reaches the iOS app's container by path; no
    // iCloud entitlement is needed. "iCloud.a.b" is stored as "iCloud~a~b".
    std::string folder = LO_ICLOUD_CONTAINER;
    std::replace(folder.begin(), folder.end(), '.', '~');
    const char* home = std::getenv("HOME");
    if (!home || !*home) return {};
    const auto container = fs::path(home) / "Library/Mobile Documents" / folder;
    std::error_code ec;
    // The container appears once the iOS app has used it on this account.
    if (!fs::is_directory(container, ec)) return {};
    return container / "Documents/Saves";
#endif
}

std::string Describe(int pulled, int pushed)
{
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char stamp[32] = {};
    std::tm local{};
    localtime_r(&now, &local);
    std::strftime(stamp, sizeof(stamp), "%H:%M", &local);
    if (pulled == 0 && pushed == 0) return std::string("up to date at ") + stamp;
    return std::to_string(pulled) + " from iCloud, " + std::to_string(pushed) + " to iCloud at " + stamp;
}
}

void Start(const fs::path& localSaves)
{
    std::lock_guard lock(g_mutex);
    g_local = localSaves;
    g_enabled = settings::GetConfig().icloudSaves;
    if (!g_enabled)
    {
        g_lastSync = "off";
        LOG_INFO("iCloud saves: off in settings");
        return;
    }
    g_cloud = ResolveCloudRoot();
    if (g_cloud.empty())
    {
        g_lastSync = "iCloud unavailable";
        LOG_INFO("iCloud saves: container {} unavailable (iCloud signed out, or not created yet)", LO_ICLOUD_CONTAINER);
        return;
    }
    std::error_code ec;
    fs::create_directories(g_cloud, ec);
    fs::create_directories(g_local, ec);
#if LO_PLATFORM_IOS
    // Slots saved on another device may still be placeholders here.
    os::ios::DownloadUbiquitousItems(g_cloud, std::chrono::seconds(15));
#endif
    int pulled = 0, pushed = 0;
    std::vector<std::string> names = SlotNames(g_local);
    for (auto& name : SlotNames(g_cloud))
        if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
    for (const auto& name : names)
    {
        const auto local = g_local / name, cloud = g_cloud / name;
        const auto localTime = fs::is_directory(local, ec) ? SlotTime(local) : std::nullopt;
        const auto cloudTime = fs::is_directory(cloud, ec) ? SlotTime(cloud) : std::nullopt;
        std::string error;
        if (cloudTime && (!localTime || *cloudTime > *localTime + kTolerance))
        {
            if (localTime) BackupLocalSlot(local);
            if (CopySlot(cloud, local, error)) ++pulled;
            else LOG_WARNING("iCloud saves: pull {} failed: {}", name, error);
        }
        else if (localTime && (!cloudTime || *localTime > *cloudTime + kTolerance))
        {
            if (CopySlot(local, cloud, error)) ++pushed;
            else LOG_WARNING("iCloud saves: push {} failed: {}", name, error);
        }
    }
    g_lastSync = Describe(pulled, pushed);
    LOG_INFO("iCloud saves: {} ({} slots, cloud folder {})", g_lastSync, names.size(), g_cloud.string());
}

void PushSlot(const fs::path& slot)
{
    {
        std::lock_guard lock(g_mutex);
        if (!g_enabled || g_cloud.empty()) return;
    }
    std::thread([slot] {
        std::lock_guard lock(g_mutex);
        std::string error;
        if (CopySlot(slot, g_cloud / slot.filename(), error))
        {
            g_lastSync = Describe(0, 1);
            LOG_INFO("iCloud saves: pushed {}", slot.filename().string());
        }
        else LOG_WARNING("iCloud saves: push {} failed: {}", slot.filename().string(), error);
    }).detach();
}

Status GetStatus()
{
    std::lock_guard lock(g_mutex);
    return {true, g_enabled, !g_cloud.empty(), g_lastSync};
}
}
#else
namespace os::cloud_saves
{
void Start(const std::filesystem::path&) {}
void PushSlot(const std::filesystem::path&) {}
Status GetStatus() { return {}; }
}
#endif
