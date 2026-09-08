#pragma once

// Windows: installs last-resort SEH, std::terminate and SIGABRT reports.
// Fast-fail, external termination and an unusable process/OS remain outside
// this best-effort diagnostic path. Call once during process initialization.
void InstallCrashHandler();
