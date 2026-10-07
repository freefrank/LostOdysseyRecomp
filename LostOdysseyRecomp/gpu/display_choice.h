#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace gpu::display_choice {
// A connected display in SDL order, with its desktop bounds.
struct Display {
    std::string name;
    int x = 0, y = 0, width = 0, height = 0;
    bool operator==(const Display&) const = default;
};
// A saved display is its SDL name plus its index, which tells equal names
// apart (monitors of one model report the same name). Returns the SDL display
// index, or -1 for automatic placement or a display that is not connected.
inline int Resolve(const std::vector<Display>& displays, const std::string& name, uint32_t index)
{
    if (name.empty()) return -1;
    if (index < displays.size() && displays[index].name == name) return int(index);
    for (size_t i = 0; i < displays.size(); ++i)
        if (displays[i].name == name) return int(i);
    return -1;
}
}
