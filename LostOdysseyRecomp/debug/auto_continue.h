#pragma once

namespace debug_menu
{
    // Opt-in, once per process: use the game's native newest-save Continue flow.
    // Does not select an arbitrary slot or persist a setting.
    bool AutoContinueEnabled();
}
