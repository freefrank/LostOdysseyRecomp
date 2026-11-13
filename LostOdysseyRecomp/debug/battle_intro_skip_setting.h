#pragma once
#include <settings/config.h>

namespace debug_menu
{
    // Debug Menu -> Cheats -> Battle -> Skip battle animation. The window thread sets it;
    // the battle intro patch reads it on the game thread.
    settings::BattleIntroSkip GetBattleIntroSkip();
    void SetBattleIntroSkip(settings::BattleIntroSkip mode);
}
