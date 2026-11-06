#pragma once
#include <cmath>
#include <cstdint>

namespace debug_menu::menu_animations
{
inline constexpr uint32_t CampMenu = 0x8326DAA8;
inline constexpr uint32_t FormationMenu = 0x832AEB04;
inline constexpr uint32_t SaveLoadMenu = 0x832688E8;
inline constexpr uint32_t DreamsMenu = 0x832BE15C;
inline constexpr uint32_t FocusCursor = CampMenu + 148028;
inline constexpr uint32_t FormationCursor = FormationMenu + 48876;
inline constexpr uint32_t SaveCursor = SaveLoadMenu + 5752;
inline constexpr uint32_t DreamsCursor = DreamsMenu + 4024;
inline thread_local bool campUpdate = false;
inline thread_local bool scaledPanel = false;

// Scope the native camp task and its separately ticked Formation (state 7)
// and storage (state 16) pages, including their heap-allocated visual cells.
// Dreams only opts in during selector open/close, never story presentation.
// Settings and other independently ticked scenes keep their own time.
class ScopedCampUpdate
{
    bool previous;
  public:
    explicit ScopedCampUpdate(uint32_t menu, uint32_t parentState = 0, uint32_t childState = 0) : previous(campUpdate)
    {
        campUpdate = menu == CampMenu || (menu == FormationMenu && parentState == 7) ||
            (menu == SaveLoadMenu && parentState == 16) ||
            (menu == DreamsMenu && parentState == 14 && (childState == 2 || childState == 4));
    }
    ~ScopedCampUpdate() { campUpdate = previous; }
    ScopedCampUpdate(const ScopedCampUpdate&) = delete;
    ScopedCampUpdate& operator=(const ScopedCampUpdate&) = delete;
};

enum class Update { Tween, Panel };
inline bool IsFocusCursor(uint32_t object)
{
    return object == FocusCursor || object == FormationCursor || object == SaveCursor || object == DreamsCursor;
}
// Both guest functions return void. Only the animation call sees the larger
// f1; restore the incoming register afterwards so the caller's next subsystem
// cannot inherit the accelerated delta. Normal forwards the original call
// without touching any register, including its volatile outputs.
template<class Context, class Original>
void Dispatch(Context& ctx, unsigned rate, Update update, Original&& original)
{
    if (!campUpdate || scaledPanel || IsFocusCursor(ctx.r3.u32) || (rate != 2 && rate != 4) ||
        !std::isfinite(ctx.f1.f64) || ctx.f1.f64 <= 0 || !std::isfinite(ctx.f1.f64 * rate))
    {
        original(ctx);
        return;
    }
    struct Restore
    {
        Context& ctx;
        decltype(ctx.f1) incoming;
        bool panel;
        ~Restore() { ctx.f1 = incoming; scaledPanel = panel; }
    } restore{ctx, ctx.f1, scaledPanel};
    // A tile panel advances both its base tween and its child tile timeline
    // with the scaled f31. Neither may receive another multiplier.
    if (update == Update::Panel) scaledPanel = true;
    ctx.f1.f64 *= rate;
    original(ctx);
}
} // namespace debug_menu::menu_animations
