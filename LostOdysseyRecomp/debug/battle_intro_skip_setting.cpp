#include <stdafx.h>
#include <os/logger.h>
#include "battle_intro_skip_setting.h"
#include <atomic>

namespace
{
    std::atomic<settings::BattleIntroSkip>& Mode()
    {
        static std::atomic<settings::BattleIntroSkip> mode{settings::GetConfig().battleIntroSkip};
        return mode;
    }
}

settings::BattleIntroSkip debug_menu::GetBattleIntroSkip()
{
    return Mode().load(std::memory_order_relaxed);
}

void debug_menu::SetBattleIntroSkip(settings::BattleIntroSkip mode)
{
    mode = settings::ValidBattleIntroSkip(mode);
    Mode().store(mode, std::memory_order_relaxed);
    if (!settings::SaveBattleIntroSkip(mode))
        LOG_WARNING("debug menu: failed to persist the battle intro skip setting");
    LOG_INFO("debug menu: battle intro skip {}", uint32_t(mode));
}
