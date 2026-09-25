#include <stdafx.h>
#include <SDL.h>
#include <atomic>
#include <os/logger.h>
#include "quit_action_hook.h"

extern "C" PPC_FUNC(__imp__sub_8287A188);
extern "C" PPC_FUNC(__imp__sub_82878EF0);
extern "C" PPC_FUNC(__imp__sub_82878CB8);

namespace settings
{
// 822DBD90 ticks Settings at 832AD2B8 and the native System menu at
// 8326DAA8 on the same guest thread. The Settings wrapper calls this only
// after its retail close task has reached the completed state (<= 2).
void RequestMainMenuAfterSettingsClose(PPCContext& ctx, uint8_t* base, uint32_t settingsMenu)
{
    quit_action::RequestTitle(ctx, settingsMenu,
        [base](uint32_t menu) { return PPC_LOAD_U32(menu + 4); },
        [base](PPCContext& call) {
            LOG_INFO("settings: completed Settings close requested retail Title transition");
            __imp__sub_8287A188(call, base);
        });
}
} // namespace settings

PPC_FUNC(sub_8287A188)
{
    // System completion at 822E256C and modal-16 Yes at 822E26B8 both call
    // through this host override. Keep other retail title calls unchanged.
    if (settings::quit_action::IsSystemYesCaller(ctx.lr) ||
        (ctx.r3.u32 == settings::quit_action::SystemMenu && ctx.r4.u32 == 1))
    {
        if (settings::quit_action::IsSystemYesCaller(ctx.lr) &&
            ctx.r3.u32 == settings::quit_action::SystemMenu && ctx.r4.u32 == 1)
            LOG_INFO("settings: System Yes hook matched lr={:#x} r3={:#x} r4={}",
                ctx.lr, ctx.r3.u32, ctx.r4.u32);
        else
        {
            static std::atomic_flag reportedMismatch = ATOMIC_FLAG_INIT;
            if (!reportedMismatch.test_and_set())
                LOG_WARNING("settings: System transition candidate used retail path lr={:#x} r3={:#x} r4={}",
                    ctx.lr, ctx.r3.u32, ctx.r4.u32);
        }
    }
    settings::quit_action::Dispatch(ctx,
        [] {
            SDL_Event event{};
            event.type = SDL_QUIT;
            const int result = SDL_PushEvent(&event);
            if (result <= 0)
                LOG_WARNING("settings: System Quit to Desktop could not queue SDL_QUIT (result {}, SDL: {}); returning to game for retry",
                    result, SDL_GetError());
            else
                LOG_INFO("settings: System Quit to Desktop queued SDL_QUIT (result {})", result);
            return result > 0;
        },
        [base](PPCContext& call) {
            // Only 822E26B8 reaches here: mirror modal-16's non-Yes branch.
            // 822E256C uses its own retail close/state reset on return.
            const double animationTime = call.f28.f64;
            call.r3.s64 = int32_t(settings::quit_action::SystemMenu);
            call.lr = 0x822E26C0;
            __imp__sub_82878EF0(call, base);
            call.r5.s64 = 0;
            call.r3.s64 = int32_t(settings::quit_action::SystemMenu);
            call.f1.f64 = animationTime;
            call.lr = 0x822E26D0;
            __imp__sub_82878CB8(call, base);
            PPC_STORE_U32(settings::quit_action::SystemMenu, 15);
        },
        [base](PPCContext& call) { __imp__sub_8287A188(call, base); });
}
