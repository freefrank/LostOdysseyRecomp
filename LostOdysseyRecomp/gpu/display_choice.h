#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace gpu::display_choice {
// A saved display is its SDL name plus its index, which only tells equal names
// apart. Returns the SDL display index, or -1 for automatic placement or a
// display that is not connected.
inline int Resolve(const std::vector<std::string>& names, const std::string& name, uint32_t index)
{
    if (name.empty()) return -1;
    if (index < names.size() && names[index] == name) return int(index);
    for (size_t i = 0; i < names.size(); ++i)
        if (names[i] == name) return int(i);
    return -1;
}
}
