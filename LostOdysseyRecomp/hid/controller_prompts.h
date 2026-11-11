#pragma once

#include <SDL3/SDL.h>
#include "controller_glyphs.h"
#include "prompt_family.h"
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

inline Family FamilyOf(SDL_GamepadType type, uint16_t vendor, uint16_t product)
{
    if (IsPlayStation(type)) return Family::PlayStation;
    if (type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO || type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT ||
        type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT || type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR)
        return Family::Switch;
    if (vendor == 0x28DE && product == 0x1205) return Family::SteamDeck; // Valve Steam Deck
    return Family::Xbox;
}

// Owned by the caller's input lock/event loop. Multiple pads can drive guest
// input at once, but prompts follow the last newly active physical controller.
class ActiveController
{
    struct Device
    {
        SDL_JoystickID id;
        Family family;
        bool positional = false; // Steam Input keeps Xbox positions (face_buttons.h)
        uint32_t buttons = 0;
        bool stick = false;
    };
    std::vector<Device> devices_;
    SDL_JoystickID active_ = 0;

public:
    void Connected(SDL_JoystickID id, Family family, bool positional = false)
    {
        if (std::none_of(devices_.begin(), devices_.end(), [=](const Device &d) { return d.id == id; }))
        {
            devices_.push_back({id, family, positional});
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
    Family ActiveFamily() const
    {
        if (active_ == UINT32_MAX) return Family::Keyboard;
        for (const auto &device : devices_)
            if (device.id == active_) return device.family;
        return Family::Xbox;
    }
    bool PlayStation() const { return ActiveFamily() == Family::PlayStation; }
    bool ActivePositional() const
    {
        for (const auto &device : devices_)
            if (device.id == active_) return device.positional;
        return false;
    }
    bool KeyboardActive() const { return active_ == UINT32_MAX; }
};
}
