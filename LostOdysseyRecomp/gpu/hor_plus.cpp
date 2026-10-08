#include <stdafx.h>

#include "gpu/renderer.h"
#include "gpu/movie_clear.h"
#include "gpu/frame_plan.h"
#include "gpu/aspect_layout.h"
#include "cpu/ppc_context.h"
#include "kernel/memory.h"

extern "C" PPC_FUNC(__imp__sub_82300E50);
extern "C" PPC_FUNC(__imp__sub_82303990);
extern "C" PPC_FUNC(__imp__sub_82309408);
extern "C" PPC_FUNC(__imp__sub_8230B308);
extern "C" PPC_FUNC(__imp__sub_8230D6B8);
extern "C" PPC_FUNC(__imp__sub_8230EC88);
extern "C" PPC_FUNC(__imp__sub_823E4B50);
extern "C" PPC_FUNC(__imp__sub_823EFC08);

namespace
{
bool ValidRange(uint32_t address, uint32_t size)
{
    return g_memory.base && address >= 0x10000 && uint64_t(address) + size <= 0x100000000ull;
}

uint32_t LoadWord(uint32_t address)
{
    return ValidRange(address, 4) ? __builtin_bswap32(*reinterpret_cast<const uint32_t*>(g_memory.base + address)) : 0;
}

void StoreWord(uint32_t address, uint32_t value)
{
    if (ValidRange(address, 4))
        *reinterpret_cast<uint32_t*>(g_memory.base + address) = __builtin_bswap32(value);
}

float LoadFloat(uint32_t address) { return std::bit_cast<float>(LoadWord(address)); }
void StoreFloat(uint32_t address, float value) { StoreWord(address, std::bit_cast<uint32_t>(value)); }

gpu::aspect_layout::Scale OutputScale()
{
    return gpu::aspect_layout::ForAspect(gpu::renderer::ActiveOutputAspect());
}

// The camera: Hor+ like OutputScale, or the narrower 4:3 view the Aspect ratio
// setting asks for. HUD, menus and movies keep OutputScale.
gpu::aspect_layout::Scale SceneScale()
{
    return gpu::aspect_layout::ForScene(gpu::renderer::ActiveOutputAspect(), gpu::frame_plan::NarrowTallView());
}

bool IsMainPerspectiveView(uint32_t view)
{
    // The common constructor is also used for subordinate orthographic views.
    // Both accepted callers build a main camera from a live family/target pair
    // and give it the full guest output rectangle.
    const uint32_t family = LoadWord(view);
    const uint32_t target = family ? LoadWord(family + 0x0C) : 0;
    const uint32_t displaySurface = LoadWord(0x83302A34);
    const float x = LoadFloat(view + 0x1C), y = LoadFloat(view + 0x20);
    const float width = LoadFloat(view + 0x24), height = LoadFloat(view + 0x28);
    const float projectionW = LoadFloat(view + 0x80 + 44), projectionQ = LoadFloat(view + 0x80 + 60);
    return displaySurface && target && LoadWord(target + 4) == displaySurface &&
        std::isfinite(x) && std::isfinite(y) && std::isfinite(width) && std::isfinite(height) &&
        x >= 0.0f && y >= 0.0f && width > 0.0f && height > 0.0f && x + width <= 1280.0f && y + height <= 720.0f &&
        std::isfinite(projectionW) && std::isfinite(projectionQ) && std::abs(projectionW) > 1e-6f && std::abs(projectionQ) < 1e-6f;
}

bool IsMainOutputView(uint32_t view)
{
    const uint32_t family = LoadWord(view);
    const uint32_t target = family ? LoadWord(family + 0x0C) : 0;
    const uint32_t displaySurface = LoadWord(0x83302A34);
    const float x = LoadFloat(view + 0x1C), y = LoadFloat(view + 0x20);
    const float width = LoadFloat(view + 0x24), height = LoadFloat(view + 0x28);
    return displaySurface && target && LoadWord(target + 4) == displaySurface &&
        std::isfinite(x) && std::isfinite(y) && std::isfinite(width) && std::isfinite(height) &&
        x >= 0.0f && y >= 0.0f && width > 0.0f && height > 0.0f && x + width <= 1280.0f && y + height <= 720.0f;
}

bool IsDisplayTarget(uint32_t target)
{
    const uint32_t displaySurface = LoadWord(0x83302A34);
    return displaySurface && target && LoadWord(target + 4) == displaySurface;
}

void ScaleProjection(uint32_t projection, const gpu::aspect_layout::Scale scale)
{
    if (scale.IsIdentity() || !ValidRange(projection, 64)) return;
    for (uint32_t row = 0; row < 4; ++row)
    {
        const uint32_t offset = row * 16;
        if (scale.x != 1.0f) StoreFloat(projection + offset, LoadFloat(projection + offset) * scale.x);
        if (scale.y != 1.0f) StoreFloat(projection + offset + 4, LoadFloat(projection + offset + 4) * scale.y);
    }
}

bool IsPrimaryCanvas(uint32_t canvas, uint32_t& matrix)
{
    const uint32_t stack = LoadWord(canvas + 0x0C);
    const uint32_t count = LoadWord(canvas + 0x10);
    if (!stack || !count || count > 64 || !ValidRange(stack, count * 64)) return false;
    matrix = stack + (count - 1) * 64;
    // Match the uncomposed canvas basis so translated, rotated and scaled HUD
    // transforms at the top of the stack remain eligible for composition.
    return IsDisplayTarget(LoadWord(canvas + 4)) && std::abs(LoadFloat(stack + 20) + 2.0f / 720.0f) < 1e-6f;
}

struct CanvasColumns
{
    std::array<uint32_t, 8> words{};
};

thread_local uint32_t topAnchoredMinimapCanvas = 0;
thread_local uint32_t fullMenuCanvas = 0;

CanvasColumns ComposeCanvasColumns(uint32_t matrix, const gpu::aspect_layout::Scale scale, bool topAnchored)
{
    CanvasColumns original;
    for (uint32_t row = 0; row < 4; ++row)
    {
        const uint32_t first = matrix + row * 16;
        original.words[row * 2] = LoadWord(first);
        original.words[row * 2 + 1] = LoadWord(first + 4);
        const float homogeneous = LoadFloat(first + 12);
        if (scale.x != 1.0f)
            StoreFloat(first, gpu::aspect_layout::CanvasComponent(LoadFloat(first), homogeneous, scale.x, 1.0f / 1280.0f));
        if (scale.y != 1.0f)
            StoreFloat(first + 4, gpu::aspect_layout::CanvasComponent(LoadFloat(first + 4), homogeneous, scale.y,
                -1.0f / 720.0f, topAnchored ? 1.0f - scale.y : 0.0f));
    }
    return original;
}

void RestoreCanvasColumns(uint32_t matrix, const CanvasColumns& original)
{
    for (uint32_t row = 0; row < 4; ++row)
    {
        StoreWord(matrix + row * 16, original.words[row * 2]);
        StoreWord(matrix + row * 16 + 4, original.words[row * 2 + 1]);
    }
}

thread_local bool movieMainOutput = false;

struct MovieScope
{
    bool previous;
    explicit MovieScope(bool current) : previous(movieMainOutput) { movieMainOutput = previous || current; }
    ~MovieScope() { movieMainOutput = previous; }
};

void EmitMovieBars(uint32_t device, uint32_t surfaceInfo, uint32_t colorInfo,
                   float x, float y, float width, float height, float safeLeft, float safeRight,
                   float safeTop, float safeBottom)
{
    if (!ValidRange(device + 0x2884, 4)) return;
    const uint32_t values[gpu::movie_clear::WordCount] = {gpu::movie_clear::Magic, surfaceInfo, colorInfo,
        std::bit_cast<uint32_t>(x), std::bit_cast<uint32_t>(y), std::bit_cast<uint32_t>(width), std::bit_cast<uint32_t>(height),
        std::bit_cast<uint32_t>(safeLeft), std::bit_cast<uint32_t>(safeRight),
        std::bit_cast<uint32_t>(safeTop), std::bit_cast<uint32_t>(safeBottom), gpu::movie_clear::Magic};
    gpu::frame_plan::EmitPrivatePacket(device, gpu::movie_clear::RegisterBase, values);
}
}

// Recompiler mid-assembly hook at 82301DA0.  The projection copy to view+0x80
// has completed; the original constructor has not derived VP, inverse VP, or
// the frustum yet.  The caller LR was saved in the constructor frame.
void HorPlusViewProjection(PPCRegister& r1, PPCRegister& r31)
{
    const uint32_t caller = LoadWord(r1.u32 + 0x1D8);
    const auto scale = SceneScale();
    if ((caller == 0x82300AFC || caller == 0x82307BEC) && IsMainPerspectiveView(r31.u32))
        ScaleProjection(r31.u32 + 0x80, scale);
}

// This alternate player-frustum path builds its own VP and frustum after the
// projection helper returns.  Restrict the strong wrapper to that exact call.
PPC_FUNC(sub_82300E50)
{
    const uint32_t caller = uint32_t(ctx.lr);
    const uint32_t projection = ctx.r3.u32;
    const auto scale = SceneScale();
    __imp__sub_82300E50(ctx, base);
    if (caller == 0x82988684)
        ScaleProjection(projection, scale);
}

// Keep the game HUD at 16:9 in the physical scene target.  The flush copies
// this state into either its immediate draw or queued command before returning.
PPC_FUNC(sub_82303990)
{
    const uint32_t canvas = ctx.r3.u32;
    uint32_t matrix = 0;
    const auto scale = OutputScale();
    const bool primary = !scale.IsIdentity() && IsPrimaryCanvas(canvas, matrix);
    CanvasColumns original;
    std::array<uint32_t, 4> scissor{};
    if (primary)
    {
        const bool topAnchored = canvas == topAnchoredMinimapCanvas;
        original = ComposeCanvasColumns(matrix, scale, topAnchored);
        // Canvas stores scissor as left, top, right, bottom.  Keep clip edges
        // on the same anchor as the transformed 2D vertices. An empty scissor
        // disables clipping in the guest, so retain its exact sentinel values.
        for (uint32_t edge = 0; edge < 4; ++edge)
            scissor[edge] = LoadWord(canvas + 0x2C + edge * 4);
        const auto fittedScissor = gpu::aspect_layout::FitScissor(scissor, scale, topAnchored ? 0.0f : 0.5f);
        for (uint32_t edge = 0; edge < 4; ++edge)
            StoreWord(canvas + 0x2C + edge * 4, fittedScissor[edge]);
    }
    __imp__sub_82303990(ctx, base);
    if (primary && ValidRange(matrix, 64))
    {
        RestoreCanvasColumns(matrix, original);
        for (uint32_t edge = 0; edge < 4; ++edge)
            StoreWord(canvas + 0x2C + edge * 4, scissor[edge]);
    }
}

namespace
{
void FlushCanvas(const PPCContext& ctx, uint8_t* base, uint32_t canvas)
{
    if (!LoadWord(canvas + 0x1C)) return;
    PPCContext flushContext = ctx;
    flushContext.r3.u64 = canvas;
    sub_82303990(flushContext, base);
}

void DrawMenuBars(const PPCContext& ctx, uint8_t* base, uint32_t canvas)
{
    const auto scale = OutputScale();
    uint32_t matrix = 0;
    if (scale.IsIdentity() || !IsPrimaryCanvas(canvas, matrix)) return;
    thread_local uint32_t scratch = 0;
    if (!scratch) scratch = g_pageAllocator.Alloc(g_pageAllocator.virtualRegion, 0x1000, 0x1000);
    if (!scratch) return;

    FlushCanvas(ctx, base, canvas);
    // Use the uncomposed display basis and opaque color for these final quads.
    // The native batch copies all of this state before it is restored here.
    std::array<uint32_t, 16> originalMatrix{};
    std::array<uint32_t, 4> originalScissor{};
    const uint32_t originalAlpha = LoadWord(canvas);
    const uint32_t basis = LoadWord(canvas + 0x0C);
    for (uint32_t i = 0; i < originalMatrix.size(); ++i)
    {
        originalMatrix[i] = LoadWord(matrix + i * 4);
        StoreWord(matrix + i * 4, LoadWord(basis + i * 4));
    }
    for (uint32_t i = 0; i < originalScissor.size(); ++i)
    {
        originalScissor[i] = LoadWord(canvas + 0x2C + i * 4);
        StoreWord(canvas + 0x2C + i * 4, 0);
    }
    StoreFloat(canvas, 1.0f);
    StoreFloat(scratch, 0); StoreFloat(scratch + 4, 0);
    StoreFloat(scratch + 8, 0); StoreFloat(scratch + 12, 1);

    for (const auto& rect : gpu::aspect_layout::MenuBars(scale))
    {
        if (rect.width <= 0 || rect.height <= 0) continue;
        PPCContext draw = ctx;
        // A call frame on the live guest stack: the dispatcher has returned, so
        // nothing below r1 is in use. The colour stays on the scratch page.
        draw.r1.u64 = (ctx.r1.u32 - 0x100) & ~0xFu;
        StoreWord(draw.r1.u32, ctx.r1.u32);
        StoreWord(draw.r1.u32 + 92, scratch); // FLinearColor RGBA
        StoreWord(draw.r1.u32 + 100, 0); // Native white texture fallback.
        StoreWord(draw.r1.u32 + 108, 0); // Opaque blend.
        draw.r3.u64 = canvas;
        draw.f1.f64 = rect.x; draw.f2.f64 = rect.y;
        draw.f3.f64 = rect.width; draw.f4.f64 = rect.height;
        draw.f5.f64 = 0; draw.f6.f64 = 0;
        draw.f7.f64 = 1; draw.f8.f64 = 1;
        __imp__sub_8230B308(draw, base);
    }
    FlushCanvas(ctx, base, canvas);
    StoreWord(canvas, originalAlpha);
    for (uint32_t i = 0; i < originalMatrix.size(); ++i)
        StoreWord(matrix + i * 4, originalMatrix[i]);
    for (uint32_t i = 0; i < originalScissor.size(); ++i)
        StoreWord(canvas + 0x2C + i * 4, originalScissor[i]);
}
}

// Native field minimap draw: r3 is the minimap manager and r4 is UCanvas.
// This owns its background, clipped map/icons, and the unclipped frame/text.
// Flush both boundaries because the final text can remain in an FCanvas batch
// after this function returns. Other HUD and menu draws keep their centred fit.
PPC_FUNC(sub_82309408)
{
    const uint32_t canvas = LoadWord(ctx.r4.u32 + 116);
    uint32_t matrix = 0;
    if (OutputScale().y >= 1.0f || !IsPrimaryCanvas(canvas, matrix))
    {
        __imp__sub_82309408(ctx, base);
        return;
    }
    FlushCanvas(ctx, base, canvas);
    const uint32_t previous = topAnchoredMinimapCanvas;
    topAnchoredMinimapCanvas = canvas;
    __imp__sub_82309408(ctx, base);
    FlushCanvas(ctx, base, canvas);
    topAnchoredMinimapCanvas = previous;
}

// This is the ordinary field-menu body, not the shared DrawRPMenu dispatcher.
// Its native state 0/1 returns without drawing; the remaining states include
// the menu opening/closing animation and its item/equipment/status pages.
PPC_FUNC(sub_8230EC88)
{
    const uint32_t state = LoadWord(ctx.r3.u32);
    uint32_t matrix = 0;
    if (uint32_t(ctx.lr) == 0x8230D758 && state != 0 && state != 1 &&
        !OutputScale().IsIdentity() && IsPrimaryCanvas(ctx.r4.u32, matrix))
        fullMenuCanvas = ctx.r4.u32;
    __imp__sub_8230EC88(ctx, base);
}

// Finish the full dispatcher first so later menu decorations cannot paint over
// the bars. Native quads retain command ordering in both Canvas execution paths.
PPC_FUNC(sub_8230D6B8)
{
    const uint32_t previous = fullMenuCanvas;
    fullMenuCanvas = 0;
    __imp__sub_8230D6B8(ctx, base);
    if (fullMenuCanvas) DrawMenuBars(ctx, base, fullMenuCanvas);
    fullMenuCanvas = previous;
}

PPC_FUNC(sub_823EFC08)
{
    MovieScope scope(IsMainOutputView(ctx.r6.u32));
    __imp__sub_823EFC08(ctx, base);
}

// FLO_MOVIE is the only call at this helper return address.  Compress its
// destination into the centred 16:9 safe region; OverlayColor and postprocess
// calls retain their full-target coverage.
PPC_FUNC(sub_823E4B50)
{
    const bool movie = movieMainOutput && uint32_t(ctx.lr) == 0x823EFF88;
    // The outer movie call receives its device in r4, but its 0x823EFF00
    // caller moves that value to r3 immediately before this helper.
    const uint32_t device = ctx.r3.u32;
    const float originalX = float(ctx.f1.f64), originalY = float(ctx.f2.f64);
    const float originalWidth = float(ctx.f3.f64), originalHeight = float(ctx.f4.f64);
    float safeLeft = originalX, safeRight = originalX + originalWidth;
    float safeTop = originalY, safeBottom = originalY + originalHeight;
    if (movie)
    {
        const auto scale = OutputScale();
        if (!scale.IsIdentity())
        {
            safeLeft = gpu::aspect_layout::FitBoundary(originalX, 1280.0f, scale.x);
            safeRight = safeLeft + originalWidth * scale.x;
            safeTop = gpu::aspect_layout::FitBoundary(originalY, 720.0f, scale.y);
            safeBottom = safeTop + originalHeight * scale.y;
            ctx.f1.f64 = safeLeft;
            ctx.f2.f64 = safeTop;
            ctx.f3.f64 = safeRight - safeLeft;
            ctx.f4.f64 = safeBottom - safeTop;
        }
    }
    __imp__sub_823E4B50(ctx, base);
    if (movie && safeRight > safeLeft && safeBottom > safeTop)
        EmitMovieBars(device, LoadWord(device + 0x2880), LoadWord(device + 0x2884),
            originalX, originalY, originalWidth, originalHeight, safeLeft, safeRight, safeTop, safeBottom);
}
