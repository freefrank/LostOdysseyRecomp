#include <stdafx.h>
#include <settings/config.h>
#include "menu_animations.h"

extern "C" PPC_FUNC(__imp__sub_822A4950);
extern "C" PPC_FUNC(__imp__sub_82328F90);
extern "C" PPC_FUNC(__imp__sub_822EBAA0);
extern "C" PPC_FUNC(__imp__sub_822ED938);
extern "C" PPC_FUNC(__imp__sub_822ECEE8);

namespace
{
template<class Original>
void CampChildTick(PPCContext& ctx, uint8_t* base, uint32_t menu, Original original)
{
    if (ctx.r3.u32 != menu)
    {
        original(ctx, base);
        return;
    }
    const debug_menu::menu_animations::ScopedCampUpdate animations(menu,
        PPC_LOAD_U32(debug_menu::menu_animations::CampMenu),
        menu == debug_menu::menu_animations::DreamsMenu ? PPC_LOAD_U32(menu + 4) : 0);
    original(ctx, base);
}
}

// Camp command 7 starts this separate Formation task; the engine ticks it
// outside the main camp function. Parent state 7 lasts through its close.
PPC_FUNC(sub_822EBAA0)
{
    CampChildTick(ctx, base, debug_menu::menu_animations::FormationMenu, __imp__sub_822EBAA0);
}

// The storage page also has a separate engine task; I/O and input keep the
// original f1, with only its visual calls inside the scope accelerated.
PPC_FUNC(sub_822ED938)
{
    CampChildTick(ctx, base, debug_menu::menu_animations::SaveLoadMenu, __imp__sub_822ED938);
}

// Only opening/closing the Dreams selector; interactive selection and the
// subsequent story reader retain their original presentation timing.
PPC_FUNC(sub_822ECEE8)
{
    CampChildTick(ctx, base, debug_menu::menu_animations::DreamsMenu, __imp__sub_822ECEE8);
}

// Pure visual delay/position/opacity/scale tween. The owning scene's input,
// cursor and derived updates keep the engine's original delta.
PPC_FUNC(sub_822A4950)
{
    if (!debug_menu::menu_animations::campUpdate)
    {
        __imp__sub_822A4950(ctx, base);
        return;
    }
    debug_menu::menu_animations::Dispatch(ctx, settings::MenuAnimationRate(),
        debug_menu::menu_animations::Update::Tween,
        [base](PPCContext& call) { __imp__sub_822A4950(call, base); });
}

// Sliding panels have a second tile timeline after their base tween update.
PPC_FUNC(sub_82328F90)
{
    if (!debug_menu::menu_animations::campUpdate)
    {
        __imp__sub_82328F90(ctx, base);
        return;
    }
    debug_menu::menu_animations::Dispatch(ctx, settings::MenuAnimationRate(),
        debug_menu::menu_animations::Update::Panel,
        [base](PPCContext& call) { __imp__sub_82328F90(call, base); });
}
