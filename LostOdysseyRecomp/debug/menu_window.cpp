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
            fast_forward::Enable(config.fastForward);
            LOG_INFO("debug menu: fast-forward {} ({}, {}x)", config.fastForward ? "on" : "off",
                config.fastForwardMode == 1 ? "toggle" : "hold", config.fastForwardRate);
            return true;
        }();
        (void)restored;
        cheats::PollHostControls();
        if (IsOverlayVisible())
        {
            UpdateOverlaySnapshot();
        }
    }
}
