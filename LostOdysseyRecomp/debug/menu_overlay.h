#pragma once

namespace host_ui
{
    struct Rasterizer;
}

namespace debug_menu
{
    enum class InputAction
    {
        None,
        Up,
        Down,
        Left,
        Right,
        Confirm,
        Cancel,
        PrevTab,
        NextTab,
        PrevCategory,
        NextCategory
    };

    void ToggleOverlay();
    bool IsOverlayVisible();
    void UpdateOverlaySnapshot();
    void HandleInput(InputAction action);
    void RenderOverlay(host_ui::Rasterizer& rasterizer);
}
