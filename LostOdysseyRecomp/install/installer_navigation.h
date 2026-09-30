#pragma once

#include <cstdlib>
#include <cstdint>

namespace install::ui
{
enum class Direction { None, Left, Right, Up, Down };

class StickNavigation
{
public:
    Direction Update(int x, int y, uint64_t now)
    {
        const int threshold = held == Direction::None ? 16000 : 10000;
        Direction direction = Direction::None;
        if (std::abs(x) >= threshold && std::abs(x) >= std::abs(y))
            direction = x < 0 ? Direction::Left : Direction::Right;
        else if (std::abs(y) >= threshold)
            direction = y < 0 ? Direction::Up : Direction::Down;

        if (direction == Direction::None)
        {
            held = direction;
            return direction;
        }
        if (direction != held)
        {
            held = direction;
            nextRepeat = now + 350;
            return direction;
        }
        if (now >= nextRepeat)
        {
            nextRepeat = now + 120;
            return direction;
        }
        return Direction::None;
    }

private:
    Direction held = Direction::None;
    uint64_t nextRepeat = 0;
};
}
