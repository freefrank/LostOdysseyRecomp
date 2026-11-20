#include "ios_support.h"
#if LO_PLATFORM_IOS
#include <os/logger.h>
#include <os/user_paths.h>
#include <gpu/shader/builtin_shader_store.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <atomic>
#include <thread>
#include <vector>
#include <filesystem>
#include <system_error>

namespace os::ios
{
namespace
{
void ExcludeFromBackup(const std::filesystem::path& path)
{
    @autoreleasepool
    {
        NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()] isDirectory:YES];
        NSError* error = nil;
        if (![url setResourceValue:@YES forKey:NSURLIsExcludedFromBackupKey error:&error])
            LOG_WARNING("ios: could not exclude {} from backup: {}", path.string(),
                error ? error.localizedDescription.UTF8String : "unknown error");
    }
}

std::filesystem::path BundleResource(const char* name)
{
    @autoreleasepool
    {
        NSString* path = [[NSBundle mainBundle] pathForResource:[NSString stringWithUTF8String:name] ofType:nil];
        return path ? std::filesystem::path(path.UTF8String) : std::filesystem::path{};
    }
}
}

namespace
{
std::atomic<int> g_batteryPercent{-1};
std::atomic<int> g_batteryState{int(BatteryState::Unknown)};
std::atomic<bool> g_lowPowerMode{false};
std::atomic<int> g_thermalState{int(ThermalState::Nominal)};
std::atomic<bool> g_statusQueued{false};

// UIDevice is main-thread API.
void UpdateDeviceStatus()
{
    UIDevice* device = UIDevice.currentDevice;
    device.batteryMonitoringEnabled = YES;
    const float level = device.batteryLevel;
    g_batteryPercent.store(level < 0.0f ? -1 : int(level * 100.0f + 0.5f));
    BatteryState battery = BatteryState::Unknown;
    switch (device.batteryState)
    {
    case UIDeviceBatteryStateUnplugged: battery = BatteryState::Unplugged; break;
    case UIDeviceBatteryStateCharging: battery = BatteryState::Charging; break;
    case UIDeviceBatteryStateFull: battery = BatteryState::Full; break;
    default: break;
    }
    g_batteryState.store(int(battery));
    NSProcessInfo* process = NSProcessInfo.processInfo;
    g_lowPowerMode.store(process.lowPowerModeEnabled);
    ThermalState thermal = ThermalState::Nominal;
    switch (process.thermalState)
    {
    case NSProcessInfoThermalStateFair: thermal = ThermalState::Fair; break;
    case NSProcessInfoThermalStateSerious: thermal = ThermalState::Serious; break;
    case NSProcessInfoThermalStateCritical: thermal = ThermalState::Critical; break;
    default: break;
    }
    g_thermalState.store(int(thermal));
    g_statusQueued.store(false);
}
}

std::filesystem::path UbiquityDocuments(const char* containerIdentifier)
{
    @autoreleasepool
    {
        NSURL* container = [NSFileManager.defaultManager
            URLForUbiquityContainerIdentifier:[NSString stringWithUTF8String:containerIdentifier]];
        if (!container) return {};
        return std::filesystem::path(container.path.UTF8String) / "Documents";
    }
}

void DownloadUbiquitousItems(const std::filesystem::path& folder, std::chrono::milliseconds timeout)
{
    // iCloud keeps a not-yet-downloaded file as ".<name>.icloud".
    const auto pending = [&] {
        std::vector<std::filesystem::path> items;
        std::error_code error;
        for (auto it = std::filesystem::recursive_directory_iterator(folder, error);
             it != std::filesystem::recursive_directory_iterator(); it.increment(error))
        {
            const auto name = it->path().filename().string();
            if (name.size() > 7 && name.front() == '.' && name.ends_with(".icloud"))
                items.push_back(it->path().parent_path() / name.substr(1, name.size() - 8));
        }
        return items;
    };
    auto items = pending();
    if (items.empty()) return;
    LOG_INFO("ios: downloading {} iCloud save files", items.size());
    @autoreleasepool
    {
        for (const auto& item : items)
        {
            NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:item.c_str()]];
            NSError* error = nil;
            if (![NSFileManager.defaultManager startDownloadingUbiquitousItemAtURL:url error:&error])
                LOG_WARNING("ios: iCloud download of {} failed: {}", item.string(),
                    error ? error.localizedDescription.UTF8String : "unknown error");
        }
    }
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!(items = pending()).empty() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    if (!items.empty()) LOG_WARNING("ios: {} iCloud save files still downloading; those slots sync next start", items.size());
}

DeviceStatus ReadDeviceStatus()
{
    if ([NSThread isMainThread]) UpdateDeviceStatus();
    else if (!g_statusQueued.exchange(true)) dispatch_async(dispatch_get_main_queue(), ^{ UpdateDeviceStatus(); });
    DeviceStatus status;
    status.batteryPercent = g_batteryPercent.load();
    status.battery = BatteryState(g_batteryState.load());
    status.lowPowerMode = g_lowPowerMode.load();
    status.thermal = ThermalState(g_thermalState.load());
    return status;
}

void FitMetalView(void* uiWindow, void* metalView)
{
    if (!uiWindow || !metalView) return;
    UIWindow* window = (__bridge UIWindow*)uiWindow;
    UIView* view = (__bridge UIView*)metalView;
    auto fit = ^{
        const CGRect before = view.frame;
        view.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        view.transform = CGAffineTransformIdentity;
        view.frame = window.bounds;
        [view setNeedsLayout];
        [view layoutIfNeeded];
        LOG_INFO("ios: Metal view {}x{} -> {}x{} points (window {}x{}, scale {})", before.size.width, before.size.height,
            view.frame.size.width, view.frame.size.height, window.bounds.size.width, window.bounds.size.height,
            view.layer.contentsScale);
    };
    if ([NSThread isMainThread]) fit();
    else dispatch_async(dispatch_get_main_queue(), fit);
}

void PrepareDocuments()
{
    const auto documents = os::user_paths::DataDir();
    std::error_code error;
    for (const char* folder : {"game", "cache/shaders", "shaders", "logs"})
        std::filesystem::create_directories(documents / folder, error);
    // Game data (about 21 GB for four discs), caches and the 235 MB shader pack
    // can all be restored or downloaded again; saves and settings stay backed up.
    for (const char* folder : {"game", "cache", "shaders", "logs"})
        ExcludeFromBackup(documents / folder);

    // Host (builtin) shaders were compiled by DXC on a Mac; iOS has no compiler.
    const auto storeName = xenos::cache::BuiltinStorePath({}, xenos::cache::Format::Spirv).filename();
    const auto bundled = BundleResource(storeName.c_str());
    if (bundled.empty())
    {
        LOG_WARNING("ios: no bundled host shader store {}; host shaders will be missing", storeName.string());
        return;
    }
    const auto installed = documents / "cache/shaders" / storeName;
    const bool missing = !std::filesystem::exists(installed, error);
    const bool stale = !missing &&
        std::filesystem::last_write_time(installed, error) < std::filesystem::last_write_time(bundled, error);
    if (missing || stale)
    {
        std::filesystem::copy_file(bundled, installed, std::filesystem::copy_options::overwrite_existing, error);
        if (error)
            LOG_WARNING("ios: could not install host shader store {}: {}", installed.string(), error.message());
        else
            LOG_INFO("ios: installed bundled host shader store {}", installed.string());
    }
}
}
#endif
