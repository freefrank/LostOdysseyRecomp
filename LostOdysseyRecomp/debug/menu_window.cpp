#include <stdafx.h>
#include "battle_menu.h"
#include "menu_overlay.h"
#include "teleport.h"
#include "map_info.h"
#include "save_anywhere.h"
#include "translations.h"
#include "cheats.h"
#include "fast_forward.h"
#include <settings/config.h>
#include <os/logger.h>
#include <gpu/renderer.h>
#include <host_ui/host_ui.h>

namespace debug_menu
{
    namespace
    {
        // Fast-forward changes in the player log: the setting, and each start
        // and stop of the boost. At most one line every 2 s; a skipped change
        // is reported with the next line, or as the state 2 s later.
        void LogFastForward()
        {
            using clock = std::chrono::steady_clock;
            static fast_forward::Status last{};
            static bool logged = false;
            static clock::time_point lastLine{};
            static uint32_t skipped = 0;
            const auto now = clock::now();
            const auto status = fast_forward::GetStatus();
            const bool changed = !logged || status.enabled != last.enabled || status.mode != last.mode ||
                status.multiplier != last.multiplier || status.active != last.active || status.keepAudioPitch != last.keepAudioPitch;
            if (changed)
            {
                last = status;
                ++skipped;
            }
            if (!skipped || (logged && now - lastLine < std::chrono::seconds(2)))
                return;
            if (logged || status.enabled)
                LOG_NOTICE("fast-forward: {} {} {}x, {}, audio {}{}", status.enabled ? "enabled" : "disabled",
                    status.mode == fast_forward::Mode::Toggle ? "toggle" : "hold", status.multiplier,
                    status.active ? "running" : "not running",
                    status.keepAudioPitch ? "keep pitch" : "speed up",
                    skipped > 1 ? fmt::format(" ({} changes)", skipped) : std::string());
            logged = true;
            lastLine = now;
            skipped = 0;
        }
    }

    void Toggle()
    {
        ToggleOverlay();
    }

    void Update()
    {
        // The F1 fast-forward choices from settings.ini, once, before the first trigger poll.
        static const bool restored = [] {
            const auto config = settings::GetConfig();
            fast_forward::SetRate(config.fastForwardRate);
            fast_forward::SetMode(config.fastForwardMode == 1 ? fast_forward::Mode::Toggle : fast_forward::Mode::Hold);
            fast_forward::SetKeepAudioPitch(config.fastForwardKeepAudioPitch);
            fast_forward::Enable(config.fastForward);
            return true;
        }();
        (void)restored;
        PollResetNotice();
        cheats::PollHostControls();
        LogFastForward();
        if (IsOverlayVisible())
        {
            UpdateOverlaySnapshot();
        }
    }
}
