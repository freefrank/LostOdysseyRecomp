#include <stdafx.h>
#include "quit_text_hook.h"
#include "config.h"
#include <kernel/heap.h>
#include <kernel/memory.h>

extern "C" PPC_FUNC(__imp__sub_8230BA20);

PPC_FUNC(sub_8230BA20)
{
    if (settings::quit_text::TextIndex(ctx.r4.u32) == std::size(settings::quit_text::TextIds))
    {
        __imp__sub_8230BA20(ctx, base);
        return;
    }

    static settings::quit_text::Cache cache;
    settings::quit_text::Lookup(ctx, base, settings::GameLanguage(), cache,
        [](PPCContext& call, uint8_t* guest) { __imp__sub_8230BA20(call, guest); },
        [](size_t size) { return g_memory.MapVirtual(g_userHeap.Alloc(size)); });
}
