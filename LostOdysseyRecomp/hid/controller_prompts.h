#pragma once

#include <SDL.h>
#include "controller_glyphs.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace hid::prompts
{
inline bool IsPlayStation(SDL_GameControllerType type)
{
    return type == SDL_CONTROLLER_TYPE_PS3 || type == SDL_CONTROLLER_TYPE_PS4 ||
           type == SDL_CONTROLLER_TYPE_PS5;
}

// Owned by the caller's input lock/event loop. Multiple pads can drive guest
// input at once, but prompts follow the last newly active physical controller.
class ActiveController
{
    struct Device
    {
        SDL_JoystickID id;
        SDL_GameControllerType type;
        uint32_t buttons = 0;
        bool stick = false;
    };
    std::vector<Device> devices_;
    SDL_JoystickID active_ = -1;

public:
    void Connected(SDL_JoystickID id, SDL_GameControllerType type)
    {
        if (std::none_of(devices_.begin(), devices_.end(), [=](const Device &d) { return d.id == id; }))
        {
            devices_.push_back({id, type});
            if (active_ == -1) active_ = id;
        }
    }
    void Disconnected(SDL_JoystickID id)
    {
        std::erase_if(devices_, [=](const Device &d) { return d.id == id; });
        if (active_ == id) active_ = devices_.empty() ? -1 : devices_.front().id;
    }
    void Keyboard() { active_ = -2; }
    void Observe(SDL_JoystickID id, uint32_t buttons, bool stick)
    {
        for (auto &device : devices_)
            if (device.id == id)
            {
                if ((buttons & ~device.buttons) || (stick && !device.stick)) active_ = id;
                device.buttons = buttons;
                device.stick = stick;
                break;
            }
    }
    bool PlayStation() const
    {
        for (const auto &device : devices_)
            if (device.id == active_) return IsPlayStation(device.type);
        return false;
    }
};
}
