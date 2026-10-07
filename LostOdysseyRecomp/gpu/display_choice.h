#pragma once
#include <algorithm>
#include <cstdint>
#include <numeric>
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
// The display left (direction < 0) or right of `current` by horizontal center,
// wrapping around like Windows' Win+Shift+arrow; -1 with fewer than two.
inline int Adjacent(const std::vector<Display>& displays, int current, int direction)
{
    if (current < 0 || size_t(current) >= displays.size() || displays.size() < 2) return -1;
    std::vector<int> order(displays.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        const auto& da = displays[size_t(a)];
        const auto& db = displays[size_t(b)];
        const long long ax = 2LL * da.x + da.width, bx = 2LL * db.x + db.width;
        return ax != bx ? ax < bx : 2LL * da.y + da.height < 2LL * db.y + db.height;
    });
    const int n = int(order.size());
    const int at = int(std::find(order.begin(), order.end(), current) - order.begin());
    return order[size_t(((at + (direction < 0 ? -1 : 1)) % n + n) % n)];
}
}
