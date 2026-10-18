#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace settings::title_entry
{
// State 8 queries these actions through the input object passed in r4. Its
// query (822A07E8) reads the mask at +(338 + action) * 4. HID filtering stops
// new presses; these masks also cover actions latched before the modal opened.
inline constexpr std::array<uint32_t, 7> MenuActions{1, 2, 8, 13, 14, 17, 18};
inline constexpr uint32_t MenuActionMasks = 338 * 4;

template<class Read, class Write, class Tick>
void TickWithMenuInputMuted(bool captured, uint32_t input, Read&& read, Write&& write, Tick&& tick)
{
    if (!captured || !input)
    {
        tick();
        return;
    }
    std::array<uint32_t, MenuActions.size()> masks;
    for (std::size_t i = 0; i < masks.size(); ++i)
        masks[i] = read(input + MenuActionMasks + MenuActions[i] * 4);
    struct Restore
    {
        uint32_t input;
        Write& write;
        const std::array<uint32_t, MenuActions.size()>& masks;
        ~Restore()
        {
            for (std::size_t i = 0; i < masks.size(); ++i)
                write(input + MenuActionMasks + MenuActions[i] * 4, masks[i]);
        }
    } restore{input, write, masks};
    for (const auto action : MenuActions)
        write(input + MenuActionMasks + action * 4, 0);
    tick(); // Keep the title's child updates/drawing running with no menu actions.
}
} // namespace settings::title_entry
