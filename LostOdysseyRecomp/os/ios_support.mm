#include "ios_support.h"
#if LO_PLATFORM_IOS
#include <os/logger.h>
#include <os/user_paths.h>
#include <gpu/shader/builtin_shader_store.h>

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

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
