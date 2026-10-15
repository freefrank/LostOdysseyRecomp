#pragma once
#include <cstdint>
#include <string>

// What the GPU-stopped dialog adds for a lost Vulkan device on Windows (#337,
// #533). AMD's driver-level FSR Upscaling / Radeon Super Resolution makes
// Vulkan lose the device seconds after the first frames, while DirectX 12
// keeps working. A loss that early, before any gameplay frame, is a boot
// failure: the dialog offers DirectX 12 for the next start and never
// switches without the player's answer.
namespace gpu::device_loss_hint
{
inline constexpr uint32_t AmdVendorId = 0x1002;
inline constexpr int32_t VkErrorDeviceLost = -4;
// Measured from the renderer going live, not from process start: a first
// start may compile pipelines for minutes before the first frame.
inline constexpr double EarlySeconds = 60.0;

struct Context
{
    bool windows = false;
    bool vulkan = false;       // the stopped device is Vulkan
    bool deviceLost = false;   // VK_ERROR_DEVICE_LOST, not another failure
    uint32_t vendorId = 0;     // PCI vendor of the stopped device
    // settings.ini chose Vulkan and LO_GRAPHICS_API does not override it, so
    // writing graphics_backend=0 changes the next start.
    bool settingsChoseVulkan = false;
    double secondsLive = -1.0; // since the renderer went live; negative = unknown
    bool gameplaySeen = false; // a 3D game frame reached the screen
};

struct Advice
{
    bool amdHint = false;    // name AMD's driver upscaling and DirectX 12
    bool offerD3D12 = false; // ask to start with DirectX 12 next time
};

inline Advice Decide(const Context& context)
{
    Advice advice;
    const bool lostVulkan = context.windows && context.vulkan && context.deviceLost;
    advice.amdHint = lostVulkan && context.vendorId == AmdVendorId;
    advice.offerD3D12 = lostVulkan && context.settingsChoseVulkan && !context.gameplaySeen &&
        context.secondsLive >= 0.0 && context.secondsLive <= EarlySeconds;
    return advice;
}

inline constexpr const char* StoppedText =
    "The graphics device stopped working, so Lost Odyssey has to close. "
    "Progress since your last save is lost. The logs folder has the details.";

// The dialog text. With offerD3D12 it ends in a yes/no question.
inline std::string Text(const Advice& advice)
{
    std::string text = StoppedText;
    if (advice.amdHint)
        text += advice.offerD3D12
            ? "\n\nAMD: turn off FSR Upscaling / Radeon Super Resolution in AMD Software."
            : "\n\nAMD: turn off FSR Upscaling / Radeon Super Resolution in AMD Software, "
              "or switch to DirectX 12: set graphics_backend=0 in settings.ini.";
    if (advice.offerD3D12)
        text += "\n\nStart with DirectX 12 next time? This sets graphics_backend=0 in settings.ini.";
    return text;
}

inline constexpr const char* SaveFailedText =
    "Lost Odyssey could not update settings.ini. "
    "To use DirectX 12, set graphics_backend=0 in settings.ini in the game folder.";
} // namespace gpu::device_loss_hint
