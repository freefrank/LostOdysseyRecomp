#pragma once
// Custom controller and keyboard layouts (Settings → Gameplay → Controls):
// which controller button and which key produce each Xbox 360 input the game
// reads. Free of SDL calls, so settings.ini and the menu tests use it without
// linking SDL; hid.cpp checks the numbers below against SDL's enums.
#include <array>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace hid::input_map
{
// The Xbox 360 inputs, in Controls page order. The keyboard also moves the
// left stick; controller sticks are not remapped.
enum Input : uint8_t
{
    A, B, X, Y, LeftShoulder, RightShoulder, LeftTrigger, RightTrigger,
    Back, Start, LeftThumb, RightThumb, DpadUp, DpadDown, DpadLeft, DpadRight,
    StickUp, StickDown, StickLeft, StickRight,
};
inline constexpr int ControllerInputCount = 16; // A .. DpadRight
inline constexpr int KeyboardInputCount = 20;   // A .. StickRight
// XAMINPUT_GAMEPAD_* bit of each input; the triggers are analog.
inline constexpr uint16_t ButtonBits[ControllerInputCount] = {
    0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0, 0,
    0x0020, 0x0010, 0x0040, 0x0080, 0x0001, 0x0002, 0x0004, 0x0008};
// settings.ini names: SDL's gamepad mapping names (as in gamecontrollerdb, the
// same in every SDL version), the stick directions in its half-axis form.
inline constexpr std::string_view InputNames[KeyboardInputCount] = {
    "a", "b", "x", "y", "leftshoulder", "rightshoulder", "lefttrigger", "righttrigger",
    "back", "start", "leftstick", "rightstick", "dpup", "dpdown", "dpleft", "dpright",
    "-lefty", "+lefty", "-leftx", "+leftx"};

// Controller sources: SDL_GamepadButton 0-25 after hid's face-button
// normalisation (face_buttons.h: "a" is the button labelled A on a Nintendo
// pad), then the triggers, which count as pressed past half way.
using Source = uint8_t;
inline constexpr Source GuideSource = 5; // never bound: system overlays own it
inline constexpr Source LeftTriggerSource = 26, RightTriggerSource = 27, SourceCount = 28;
inline constexpr uint8_t TriggerPressed = 128; // of 255
inline constexpr std::string_view SourceNames[SourceCount] = {
    "a", "b", "x", "y", "back", "guide", "start", "leftstick", "rightstick", "leftshoulder", "rightshoulder",
    "dpup", "dpdown", "dpleft", "dpright", "misc1", "paddle1", "paddle2", "paddle3", "paddle4", "touchpad",
    "misc2", "misc3", "misc4", "misc5", "misc6", "lefttrigger", "righttrigger"};
inline constexpr bool Bindable(Source source) { return source < SourceCount && source != GuideSource; }
// Sources a capture may record.
inline constexpr uint32_t BindableSources = ((1u << SourceCount) - 1) & ~(1u << GuideSource);

using ControllerMap = std::array<Source, ControllerInputCount>;
inline constexpr ControllerMap DefaultController = {
    0, 1, 2, 3, 9, 10, LeftTriggerSource, RightTriggerSource, 4, 6, 7, 8, 11, 12, 13, 14};

// Keyboard: SDL scancodes, which are USB HID usage ids and stay the same
// everywhere (SDL's key names do not), 0 for no key.
using KeyboardMap = std::array<uint16_t, KeyboardInputCount>;
inline constexpr uint16_t NoKey = 0, ScancodeCount = 512;
inline constexpr uint16_t EscapeKey = 41; // cancels a capture
inline constexpr uint16_t F1Key = 58;     // opens the debug menu
// The keys from before the Controls page; the stick clicks had none.
inline constexpr KeyboardMap DefaultKeyboard = {
    29 /* Z */, 27 /* X */, 4 /* A */, 22 /* S */, 20 /* Q */, 26 /* W */, 8 /* E */, 21 /* R */,
    42 /* Backspace */, 40 /* Return */, NoKey, NoKey, 82 /* Up */, 81 /* Down */, 80 /* Left */, 79 /* Right */,
    12 /* I */, 14 /* K */, 13 /* J */, 15 /* L */};
inline constexpr bool BindableKey(uint32_t scancode)
{
    return scancode >= 4 && scancode < ScancodeCount && scancode != EscapeKey && scancode != F1Key;
}

// Binds input to value. The input that had value takes input's old one, so
// nothing is bound twice and no input loses its binding.
template <typename Map>
void Assign(Map &map, int input, typename Map::value_type value)
{
    for (size_t other = 0; other < map.size(); ++other)
        if (int(other) != input && map[other] == value) map[other] = map[size_t(input)];
    map[size_t(input)] = value;
}
// The input other than `input` bound to value, or -1.
template <typename Map>
int BoundElsewhere(const Map &map, int input, typename Map::value_type value)
{
    for (size_t other = 0; other < map.size(); ++other)
        if (int(other) != input && map[other] == value) return int(other);
    return -1;
}

// An invalid entry gets its default. When an entry binds what an earlier one
// binds, it takes its default if nothing else has it, else a controller input
// takes the first default button nothing has (there always is one) and a key
// is left unbound. "a:b" alone thus reads as the swap it means.
template <typename Map>
bool UsedElsewhere(const Map &map, size_t input, typename Map::value_type value)
{
    for (size_t other = 0; other < map.size(); ++other)
        if (other != input && map[other] == value) return true;
    return false;
}
template <typename Map>
bool RepeatsEarlier(const Map &map, size_t input)
{
    for (size_t earlier = 0; earlier < input; ++earlier)
        if (map[earlier] == map[input]) return true;
    return false;
}
inline ControllerMap ValidController(ControllerMap map)
{
    for (size_t i = 0; i < map.size(); ++i)
        if (!Bindable(map[i])) map[i] = DefaultController[i];
    for (size_t i = 0; i < map.size(); ++i)
    {
        if (!RepeatsEarlier(map, i)) continue;
        map[i] = DefaultController[i];
        for (size_t free = 0; UsedElsewhere(map, i, map[i]) && free < DefaultController.size(); ++free)
            map[i] = DefaultController[free];
    }
    return map;
}
inline KeyboardMap ValidKeyboard(KeyboardMap map)
{
    for (size_t i = 0; i < map.size(); ++i)
        if (map[i] != NoKey && !BindableKey(map[i])) map[i] = DefaultKeyboard[i];
    for (size_t i = 0; i < map.size(); ++i)
        if (map[i] != NoKey && RepeatsEarlier(map, i))
            map[i] = DefaultKeyboard[i] != NoKey && !UsedElsewhere(map, i, DefaultKeyboard[i]) ? DefaultKeyboard[i] : NoKey;
    return map;
}

// settings.ini text: "input:value" pairs separated by commas, every input
// listed. Unknown or invalid pairs keep their default.
template <typename Pair>
void ForEachPair(std::string_view text, Pair pair)
{
    while (!text.empty())
    {
        const size_t comma = text.find(',');
        const auto entry = text.substr(0, comma);
        if (const size_t colon = entry.find(':'); colon != std::string_view::npos)
            pair(entry.substr(0, colon), entry.substr(colon + 1));
        if (comma == std::string_view::npos) break;
        text.remove_prefix(comma + 1);
    }
}
inline int InputIndex(std::string_view name, int count)
{
    for (int i = 0; i < count; ++i)
        if (InputNames[i] == name) return i;
    return -1;
}
inline std::string FormatController(const ControllerMap &map)
{
    std::string text;
    for (int i = 0; i < ControllerInputCount; ++i)
    {
        if (i) text += ',';
        text += InputNames[i];
        text += ':';
        text += SourceNames[map[i] < SourceCount ? map[i] : DefaultController[i]];
    }
    return text;
}
inline ControllerMap ParseController(std::string_view text)
{
    ControllerMap map = DefaultController;
    ForEachPair(text, [&](std::string_view name, std::string_view value) {
        const int input = InputIndex(name, ControllerInputCount);
        for (Source source = 0; input >= 0 && source < SourceCount; ++source)
            if (SourceNames[source] == value) map[input] = source;
    });
    return ValidController(map);
}
inline std::string FormatKeyboard(const KeyboardMap &map)
{
    std::string text;
    for (int i = 0; i < KeyboardInputCount; ++i)
    {
        if (i) text += ',';
        text += InputNames[i];
        text += ':';
        text += map[i] == NoKey ? std::string("none") : std::to_string(map[i]);
    }
    return text;
}
inline KeyboardMap ParseKeyboard(std::string_view text)
{
    KeyboardMap map = DefaultKeyboard;
    ForEachPair(text, [&](std::string_view name, std::string_view value) {
        const int input = InputIndex(name, KeyboardInputCount);
        uint32_t scancode = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), scancode);
        if (input < 0) return;
        if (value == "none") map[input] = NoKey;
        else if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && scancode < ScancodeCount)
            map[input] = uint16_t(scancode);
    });
    return ValidKeyboard(map);
}

// The guest-facing buttons and triggers from one controller's sources:
// pressed has bit s set for each pressed source; the triggers are 0-255. A
// button bound to a trigger presses past half way; a trigger bound to a
// button pulls fully.
struct Pad
{
    uint16_t buttons = 0;
    uint8_t leftTrigger = 0, rightTrigger = 0;
};
inline Pad ApplyController(const ControllerMap &map, uint32_t pressed, uint8_t leftTrigger, uint8_t rightTrigger)
{
    Pad pad;
    for (int i = 0; i < ControllerInputCount; ++i)
        if (ButtonBits[i] && ((pressed >> map[i]) & 1)) pad.buttons |= ButtonBits[i];
    const auto analog = [&](Source source) -> uint8_t {
        return source == LeftTriggerSource ? leftTrigger : source == RightTriggerSource ? rightTrigger
             : ((pressed >> source) & 1) ? 255 : 0;
    };
    pad.leftTrigger = analog(map[LeftTrigger]);
    pad.rightTrigger = analog(map[RightTrigger]);
    return pad;
}
// Held keys on top of a guest state: buttons are added, a trigger key pulls
// fully, a stick key pushes the left stick to its edge.
template <typename Held>
void ApplyKeyboard(const KeyboardMap &map, Held held, uint16_t &buttons, uint8_t &leftTrigger, uint8_t &rightTrigger,
                   int16_t &leftX, int16_t &leftY)
{
    const auto down = [&](Input input) { return map[input] != NoKey && held(map[input]); };
    for (int i = 0; i < ControllerInputCount; ++i)
        if (ButtonBits[i] && down(Input(i))) buttons |= ButtonBits[i];
    if (down(LeftTrigger)) leftTrigger = 255;
    if (down(RightTrigger)) rightTrigger = 255;
    if (down(StickUp)) leftY = 32767;
    if (down(StickDown)) leftY = -32768;
    if (down(StickLeft)) leftX = -32768;
    if (down(StickRight)) leftX = 32767;
}
} // namespace hid::input_map
