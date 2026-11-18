#pragma once
#include <os/platform.h>

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
}
#endif
