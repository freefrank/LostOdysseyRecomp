#include <stdafx.h>
#include "battle_menu.h"
#include "menu_overlay.h"
#include "teleport.h"
#include "map_info.h"
#include "save_anywhere.h"
#include "translations.h"
#include "cheats.h"
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
        cheats::PollHostControls();
        if (IsOverlayVisible())
        {
            UpdateOverlaySnapshot();
        }
    }
}
