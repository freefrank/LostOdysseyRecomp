#pragma once
#include <os/platform.h>
#include <chrono>
#include <filesystem>

// iOS-only startup services. The rest of the runtime treats iOS as a Darwin
// host without process spawning, AppKit or a shader compiler.
#if LO_PLATFORM_IOS
namespace os::ios
{
// Creates the Documents layout (game/, cache/shaders/, shaders/), keeps the
// large folders out of iCloud backup and installs the host-shader store that
// ships in the app bundle when the Documents copy is missing or older.
void PrepareDocuments();

// Keeps SDL's Metal view (a UIView) the size of its UIWindow. SDL creates it
// with the window's bounds at that moment and does not refit it once the
// landscape-only app finishes rotating, which squeezed the picture into a
// portrait frame. Safe to call from any thread.
void FitMetalView(void* uiWindow, void* metalView);

enum class BatteryState { Unknown, Unplugged, Charging, Full };
enum class ThermalState { Nominal, Fair, Serious, Critical };
struct DeviceStatus
{
    int batteryPercent = -1; // -1 when iOS does not report it
    BatteryState battery = BatteryState::Unknown;
    bool lowPowerMode = false;
    ThermalState thermal = ThermalState::Nominal;
};
// Last values read on the main thread; each call also queues a fresh read, so
// a caller polling it (the debug menu each frame) sees changes within a frame.
DeviceStatus ReadDeviceStatus();

// <iCloud container>/Documents, or empty when iCloud is signed out or the app
// lacks the container. Blocking (first call can take seconds); not on the main thread.
std::filesystem::path UbiquityDocuments(const char* containerIdentifier);
// Starts downloading every iCloud placeholder under folder and waits until
// they are local or the timeout passes.
void DownloadUbiquitousItems(const std::filesystem::path& folder, std::chrono::milliseconds timeout);
}
#endif
