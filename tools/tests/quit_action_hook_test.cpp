// Injectable guest-call contract. No game data or SDL event loop required.
#include <settings/quit_action_hook.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
union Register { uint64_t u64 = 0; int64_t s64; uint32_t u32; };
struct Context { Register r3, r4; uint32_t lr = 0; };

void Check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

void CheckSequence(std::string_view code, std::initializer_list<std::string_view> instructions,
                   const char* message)
{
    size_t cursor = 0;
    for (const auto instruction : instructions)
    {
        const size_t found = code.find(instruction, cursor);
        Check(found != code.npos, message);
        cursor = found + instruction.size();
    }
}

void CheckGeneratedGuestCallers()
{
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const auto read = [](const std::filesystem::path& path) {
        std::ifstream input(path);
        Check(bool(input), "generated caller source must be available");
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    };
    const std::string generated = read(root / "LostOdysseyRecompLib/ppc/ppc_recomp.3.cpp");
    const auto section = [&](std::string_view begin, std::string_view end) {
        const size_t first = generated.find(begin);
        const size_t last = first == std::string::npos ? first : generated.find(end, first + begin.size());
        Check(first != std::string::npos && last != std::string::npos, "generated guest labels exist");
        return std::string_view(generated).substr(first, last - first);
    };
    CheckSequence(section("loc_822E1398:", "loc_822E13E0:"),
        {"switch (ctx.r11.u32)", "case 5:", "goto loc_822E2560;"},
        "System task state 6 reaches the in-game observed caller");
    CheckSequence(section("loc_822E2560:", "loc_822E2588:"),
        {"ctx.r4.s64 = 1;", "ctx.r3.u64 = ctx.r30.u64;", "ctx.lr = 0x822E256C;",
         "sub_8287A188(ctx, base);", "loc_822E256C:", "sub_828717D8(ctx, base);",
         "ctx.r11.s64 = 0;", "PPC_STORE_U32(ctx.r14.u32 + 0, ctx.r11.u32);"},
        "System task caller must always perform its retail close and state reset on return");
    CheckSequence(section("loc_822E2680:", "loc_822E26DC:"),
        {"sub_82860018(ctx, base);", "ctx.cr6.compare<int32_t>(ctx.r3.s32, 4, ctx.xer);",
         "if (!ctx.cr6.eq) goto loc_822E26BC;", "ctx.r4.s64 = 1;",
         "ctx.lr = 0x822E26B8;", "sub_8287A188(ctx, base);", "loc_822E26BC:",
         "sub_82878EF0(ctx, base);", "sub_82878CB8(ctx, base);",
         "PPC_STORE_U32(ctx.r30.u32 + 0, ctx.r11.u32);"},
        "modal-16 Yes and its separate cancellation branch must remain distinct");
    const std::string hook = read(root / "LostOdysseyRecomp/settings/quit_action_hook.cpp");
    CheckSequence(std::string_view(hook).substr(0, hook.find("PPC_FUNC(sub_8287A188)")),
        {"RequestMainMenuAfterSettingsClose", "__imp__sub_8287A188(call, base);"},
        "Settings must bypass the host System quit hook");
}
}

int main()
{
    using namespace settings::quit_action;
    CheckGeneratedGuestCallers();
    Context ctx{};
    int original = 0, pushed = 0, recovered = 0;
    uint32_t state = 3;
    auto guestState = [&](uint32_t menu) {
        Check(menu == SettingsMenu, "Settings state read address");
        return state;
    };
    auto guestTitle = [&](Context& call) {
        ++original;
        Check(call.r3.s64 == int32_t(SystemMenu) && call.r4.s64 == 1,
            "Settings must call the original guest transition with System menu and code 1");
    };
    Check(!RequestTitle(ctx, 0x1234, guestState, guestTitle) && original == 0,
        "unrelated Settings menu cannot transition");
    Check(!RequestTitle(ctx, SettingsMenu, guestState, guestTitle) && original == 0,
        "unfinished retail close cannot transition");
    state = 1;
    Check(RequestTitle(ctx, SettingsMenu, guestState, guestTitle) && original == 1,
        "completed Settings close calls original guest title transition");

    auto recover = [&](Context&) { ++recovered; };
    auto originalAction = [&](Context&) { ++original; };
    bool canPush = true;
    auto quitEvent = [&] { ++pushed; return canPush; };
    ctx.lr = SystemTaskYesCaller;
    ctx.r3.u32 = SystemMenu;
    ctx.r4.u32 = 1;
    Dispatch(ctx, quitEvent, recover, originalAction);
    Check(pushed == 1 && recovered == 0 && original == 1,
        "observed System task Yes only queues SDL_QUIT");
    canPush = false;
    Dispatch(ctx, quitEvent, recover, originalAction);
    Check(pushed == 2 && recovered == 0 && original == 1,
        "failed System task queue leaves retail close/state reset to its caller, not modal recovery or Title");

    ctx.lr = ModalYesCaller;
    canPush = true;
    Dispatch(ctx, quitEvent, recover, originalAction);
    Check(pushed == 3 && recovered == 0 && original == 1,
        "separate modal-16 Yes also queues SDL_QUIT");
    canPush = false;
    Dispatch(ctx, quitEvent, recover, originalAction);
    Check(pushed == 4 && recovered == 1 && original == 1,
        "failed modal-16 queue uses only its own cancellation path");

    ctx.lr = SystemTaskYesCaller - 4;
    Dispatch(ctx, quitEvent, recover, originalAction);
    ctx.lr = ModalYesCaller - 4;
    Dispatch(ctx, quitEvent, recover, originalAction);
    ctx.lr = SystemTaskYesCaller;
    ctx.r3.u32 = SettingsMenu;
    Dispatch(ctx, quitEvent, recover, originalAction);
    ctx.r3.u32 = SystemMenu;
    ctx.r4.u32 = 0;
    Dispatch(ctx, quitEvent, recover, originalAction);
    ctx.lr = 0x822E9ECC;
    ctx.r4.u32 = 1;
    Dispatch(ctx, quitEvent, recover, originalAction);
    Check(pushed == 4 && recovered == 1 && original == 6,
        "wrong caller/menu/code preserve original; unrelated return-to-Title cannot quit desktop");
}
