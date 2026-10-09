#pragma once

#include <SDL3/SDL.h>
#include "controller_glyphs.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace hid::prompts
{
inline bool IsPlayStation(SDL_GamepadType type)
{
    return type == SDL_GAMEPAD_TYPE_PS3 || type == SDL_GAMEPAD_TYPE_PS4 ||
           type == SDL_GAMEPAD_TYPE_PS5;
}

// Owned by the caller's input lock/event loop. Multiple pads can drive guest
// input at once, but prompts follow the last newly active physical controller.
class ActiveController
{
    struct Device
    {
        SDL_JoystickID id;
        SDL_GamepadType type;
        uint32_t buttons = 0;
        bool stick = false;
    };
    std::vector<Device> devices_;
    SDL_JoystickID active_ = 0;

public:
    void Connected(SDL_JoystickID id, SDL_GamepadType type)
    {
        if (std::none_of(devices_.begin(), devices_.end(), [=](const Device &d) { return d.id == id; }))
        {
            devices_.push_back({id, type});
            if (active_ == 0) active_ = id;
        }
    }
    void Disconnected(SDL_JoystickID id)
    {
        std::erase_if(devices_, [=](const Device &d) { return d.id == id; });
        if (active_ == id) active_ = devices_.empty() ? 0 : devices_.front().id;
    }
    void Keyboard() { active_ = UINT32_MAX; }
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
    bool KeyboardActive() const { return active_ == UINT32_MAX; }
};
}
