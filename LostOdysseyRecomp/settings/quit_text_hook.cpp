#include <stdafx.h>
#include "quit_text_hook.h"
#include "config.h"
#include "language_selection.h"
#include <kernel/heap.h>
#include <kernel/memory.h>
#include <modding/text_overlay.h>

extern "C" PPC_FUNC(__imp__sub_8230BA20);

// A language pack that translates one of these IDs in the game's menu text
// keeps its own text; the rebuilt menu file already holds it.
static bool PackTranslates(uint32_t id)
{
    const auto& codes = settings::language::Codes;
    const auto language = settings::GameLanguage();
    if (language == 0 || language >= std::size(codes))
        return false;
    const std::string code = codes[language];
    return modding::text_overlay::Translates("bin/xenon/loc/" + code + "/menu/menu_" + code + ".dat", "id." + std::to_string(id));
}

PPC_FUNC(sub_8230BA20)
{
    const auto index = settings::quit_text::TextIndex(ctx.r4.u32);
    static const auto packed = [] {
        std::array<bool, std::size(settings::quit_text::TextIds)> result{};
        for (size_t i = 0; i < result.size(); ++i) result[i] = PackTranslates(settings::quit_text::TextIds[i]);
        return result;
    }();
    if (index == std::size(settings::quit_text::TextIds) || packed[index])
    {
        __imp__sub_8230BA20(ctx, base);
        return;
    }

    static settings::quit_text::Cache cache;
    settings::quit_text::Lookup(ctx, base, settings::GameLanguage(), cache,
        [](PPCContext& call, uint8_t* guest) { __imp__sub_8230BA20(call, guest); },
        [](size_t size) { return g_memory.MapVirtual(g_userHeap.Alloc(size)); });
}
