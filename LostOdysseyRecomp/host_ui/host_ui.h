#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <limits>
#include <vector>
#include <string>
#include "../debug/fast_forward.h"

namespace host_ui
{
    // Global pause state for host in-game overlay menus (e.g. F1 Debug Menu)
    inline std::atomic<bool> g_gamePaused{false};
    inline std::atomic<bool> g_stopping{false};
    inline std::mutex g_pauseMutex;
    inline std::condition_variable g_pauseCv;
    inline std::chrono::steady_clock::time_point g_pauseStartTime{};
    inline std::chrono::steady_clock::duration g_accumulatedPausedDuration{0};

    inline bool IsGamePaused()
    {
        return g_gamePaused.load(std::memory_order_relaxed);
    }

    inline bool IsStopping()
    {
        return g_stopping.load(std::memory_order_relaxed);
    }

    inline void RequestStop()
    {
        {
            std::lock_guard<std::mutex> lock(g_pauseMutex);
            g_stopping.store(true, std::memory_order_release);
            debug_menu::fast_forward::Enable(false);
            debug_menu::fast_forward::SetPaused(false);
            if (g_gamePaused.load(std::memory_order_relaxed))
            {
                if (g_pauseStartTime != std::chrono::steady_clock::time_point{})
                {
                    g_accumulatedPausedDuration += std::chrono::steady_clock::now() - g_pauseStartTime;
                    g_pauseStartTime = {};
                }
                g_gamePaused.store(false, std::memory_order_release);
            }
        }
        g_pauseCv.notify_all();
    }

    inline void SetGamePaused(bool paused)
    {
        {
            std::lock_guard<std::mutex> lock(g_pauseMutex);
            if (!g_stopping.load(std::memory_order_relaxed))
            {
                bool wasPaused = g_gamePaused.load(std::memory_order_relaxed);
                if (paused && !wasPaused)
                {
                    debug_menu::fast_forward::SetPaused(true);
                    g_pauseStartTime = std::chrono::steady_clock::now();
                    g_gamePaused.store(true, std::memory_order_release);
                }
                else if (!paused && wasPaused)
                {
                    debug_menu::fast_forward::SetPaused(false);
                    if (g_pauseStartTime != std::chrono::steady_clock::time_point{})
                    {
                        g_accumulatedPausedDuration += std::chrono::steady_clock::now() - g_pauseStartTime;
                        g_pauseStartTime = {};
                    }
                    g_gamePaused.store(false, std::memory_order_release);
                }
            }
        }
        if (!paused || g_stopping.load(std::memory_order_relaxed))
        {
            g_pauseCv.notify_all();
        }
    }

    // mftb and KeTimeStampBundle use this same continuous, scaled guest clock.
    inline uint64_t GetActiveGameTimeNs()
    {
        return debug_menu::fast_forward::GameTimeNs();
    }

    // Keep host-side stale-snapshot checks and UI deadlines at wall-time speed.
    inline uint64_t GetUnscaledActiveGameTimeNs()
    {
        std::lock_guard<std::mutex> lock(g_pauseMutex);
        const auto now = std::chrono::steady_clock::now();
        auto pausedDuration = g_accumulatedPausedDuration;
        if (g_gamePaused.load(std::memory_order_relaxed) && g_pauseStartTime != std::chrono::steady_clock::time_point{})
        {
            pausedDuration += (now - g_pauseStartTime);
        }
        const auto activeTime = now - pausedDuration;
        return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(activeTime.time_since_epoch()).count());
    }

    inline uint64_t GetActiveGameTimeMs()
    {
        return GetUnscaledActiveGameTimeNs() / 1000000ull;
    }

    // Called by guest threads or wait routines to block while paused
    inline void WaitIfPaused()
    {
        if (!g_gamePaused.load(std::memory_order_relaxed) || g_stopping.load(std::memory_order_relaxed))
            return;

        std::unique_lock<std::mutex> lock(g_pauseMutex);
        g_pauseCv.wait(lock, [] {
            return !g_gamePaused.load(std::memory_order_relaxed) || g_stopping.load(std::memory_order_relaxed);
        });
    }

    // 1280x720 32-bit software frame buffer for overlays
    constexpr uint32_t kOverlayWidth = 1280;
    constexpr uint32_t kOverlayHeight = 720;

    struct PixelBuffer
    {
        uint32_t width = kOverlayWidth;
        uint32_t height = kOverlayHeight;
        // Packed so little-endian memory is R, G, B, A for R8G8B8A8_UNORM uploads.
        std::vector<uint32_t> pixels;

        bool Resize(uint32_t w = kOverlayWidth, uint32_t h = kOverlayHeight)
        {
            if (!w || !h || size_t(w) > std::numeric_limits<size_t>::max() / size_t(h))
            {
                width = height = 0;
                pixels.clear();
                return false;
            }
            width = w;
            height = h;
            pixels.assign(size_t(w) * h, 0);
            return true;
        }

        void Clear(uint32_t color = 0)
        {
            std::fill(pixels.begin(), pixels.end(), color);
        }
    };
}
