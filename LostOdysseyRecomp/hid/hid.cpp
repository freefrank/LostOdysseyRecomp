#include <stdafx.h>
#include "hid.h"
#include <kernel/xdm.h>
#include <os/logger.h>
#include <atomic>
extern std::atomic<uint32_t> g_presentedSwaps;
#include <vector>
#include <SDL.h>

// SDL game controller -> XInput state. Player 1 only for now; the keyboard
// mirrors the pad so the game can be driven without a controller.

namespace
{
    SDL_GameController* g_controller = nullptr;
    Mutex g_hidMutex;
    uint32_t g_packet = 0;

    void OpenFirstController()
    {
        if (g_controller)
            return;
        int count = SDL_NumJoysticks();
        // Prefer an Xbox-type pad when several controllers are attached.
        std::vector<int> order;
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < count; i++)
            {
                SDL_GameControllerType type = SDL_GameControllerTypeForIndex(i);
                bool xbox = type == SDL_CONTROLLER_TYPE_XBOX360 || type == SDL_CONTROLLER_TYPE_XBOXONE;
                if ((pass == 0) == xbox)
                    order.push_back(i);
            }
        for (int i : order)
        {
            if (SDL_IsGameController(i))
            {
                g_controller = SDL_GameControllerOpen(i);
                if (g_controller)
                {
                    LOG_INFO("controller: {} ({} joysticks)", SDL_GameControllerName(g_controller), count);
                    break;
                }
                LOG_WARNING("controller: open failed for joystick {}: {}", i, SDL_GetError());
            }
            else
                LOG_INFO("controller: joystick {} '{}' has no game controller mapping", i, SDL_JoystickNameForIndex(i));
        }
        if (!g_controller)
            LOG_INFO("controller: none yet ({} joysticks), keyboard fallback active", count);
    }
}

void hid::Init()
{
    static bool initialised = false;
    if (initialised)
        return;
    initialised = true;
    // RawInput delivers WM_INPUT to the thread that initialised the joystick
    // subsystem; ours never pumps messages for it. Use XInput/WGI polled from
    // SDL's own joystick thread instead so button state updates regardless of
    // which thread pumps events.
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
    SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "1");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
    {
        LOG_WARNING("SDL controller init failed: {}", SDL_GetError());
        return;
    }
    OpenFirstController();
}

static std::atomic<bool> g_externalPump{ false };

void hid::SetExternalEventPump(bool external)
{
    g_externalPump = external;
}

void hid::HandleControllerEvent(uint32_t eventType, int32_t which)
{
    std::lock_guard lock(g_hidMutex);
    if (eventType == SDL_CONTROLLERDEVICEADDED)
    {
        // Re-evaluate so a later-enumerated Xbox pad wins over a non-Xbox one.
        if (g_controller)
        {
            SDL_GameControllerType current = SDL_GameControllerGetType(g_controller);
            bool currentIsXbox = current == SDL_CONTROLLER_TYPE_XBOX360 || current == SDL_CONTROLLER_TYPE_XBOXONE;
            if (currentIsXbox)
                return;
            SDL_GameControllerClose(g_controller);
            g_controller = nullptr;
        }
        OpenFirstController();
    }
    else if (eventType == SDL_CONTROLLERDEVICEREMOVED && g_controller &&
        SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_controller)) == which)
    {
        LOG_INFO("controller removed");
        SDL_GameControllerClose(g_controller);
        g_controller = nullptr;
    }
}

void hid::Poll()
{
    if (g_externalPump)
        return;
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED)
            HandleControllerEvent(e.type, e.cdevice.which);
        else if (e.type == SDL_QUIT)
            std::_Exit(0);
    }
}

uint32_t hid::GetState(uint32_t dwUserIndex, XAMINPUT_STATE* pState)
{
    if (dwUserIndex != 0)
        return ERROR_DEVICE_NOT_CONNECTED;

    Poll();

    std::lock_guard lock(g_hidMutex);
    pState->dwPacketNumber = ++g_packet;
    auto& gp = pState->Gamepad;

    if (g_controller)
    {
        if (!g_externalPump)
            SDL_GameControllerUpdate();
        auto btn = [&](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(g_controller, b) != 0; };
        auto axis = [&](SDL_GameControllerAxis a) { return SDL_GameControllerGetAxis(g_controller, a); };

        if (btn(SDL_CONTROLLER_BUTTON_DPAD_UP)) gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_UP;
        if (btn(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_DOWN;
        if (btn(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_LEFT;
        if (btn(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_RIGHT;
        if (btn(SDL_CONTROLLER_BUTTON_START)) gp.wButtons |= XAMINPUT_GAMEPAD_START;
        if (btn(SDL_CONTROLLER_BUTTON_BACK)) gp.wButtons |= XAMINPUT_GAMEPAD_BACK;
        if (btn(SDL_CONTROLLER_BUTTON_LEFTSTICK)) gp.wButtons |= XAMINPUT_GAMEPAD_LEFT_THUMB;
        if (btn(SDL_CONTROLLER_BUTTON_RIGHTSTICK)) gp.wButtons |= XAMINPUT_GAMEPAD_RIGHT_THUMB;
        if (btn(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) gp.wButtons |= XAMINPUT_GAMEPAD_LEFT_SHOULDER;
        if (btn(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) gp.wButtons |= XAMINPUT_GAMEPAD_RIGHT_SHOULDER;
        if (btn(SDL_CONTROLLER_BUTTON_A)) gp.wButtons |= XAMINPUT_GAMEPAD_A;
        if (btn(SDL_CONTROLLER_BUTTON_B)) gp.wButtons |= XAMINPUT_GAMEPAD_B;
        if (btn(SDL_CONTROLLER_BUTTON_X)) gp.wButtons |= XAMINPUT_GAMEPAD_X;
        if (btn(SDL_CONTROLLER_BUTTON_Y)) gp.wButtons |= XAMINPUT_GAMEPAD_Y;

        gp.bLeftTrigger = uint8_t(axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
        gp.bRightTrigger = uint8_t(axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7);
        gp.sThumbLX = axis(SDL_CONTROLLER_AXIS_LEFTX);
        gp.sThumbLY = int16_t(-axis(SDL_CONTROLLER_AXIS_LEFTY) - 1);
        gp.sThumbRX = axis(SDL_CONTROLLER_AXIS_RIGHTX);
        gp.sThumbRY = int16_t(-axis(SDL_CONTROLLER_AXIS_RIGHTY) - 1);
    }

    {
        static const bool trace = getenv("LO_TRACE_INPUT") != nullptr;
        static uint16_t lastButtons = 0;
        if (trace && gp.wButtons != lastButtons)
        {
            LOG_INFO("input: buttons {:#06x} (controller {})", gp.wButtons, g_controller ? "yes" : "no");
            lastButtons = gp.wButtons;
        }
    }

    // Test hooks for unattended runs, driven by the presented-swap counter:
    //   LO_AUTO_START=<swap>            pulse a button for 20 polls every 240 swaps
    //   LO_AUTO_BUTTONS=<letters>       from that swap on (s=START, a, b, x, y,
    //                                   u/d/l/r = dpad); the last letter repeats.
    //   LO_AUTO_BUTTONS=a@1250,s@1500   swap-stamped form: each entry pulses its
    //                                   button for 20 polls starting at that swap
    //                                   (LO_AUTO_START not needed).
    {
        static const uint32_t autoStartSwap = getenv("LO_AUTO_START") ? strtoul(getenv("LO_AUTO_START"), nullptr, 10) : 0;
        static const char* seq = getenv("LO_AUTO_BUTTONS") ? getenv("LO_AUTO_BUTTONS") : "s";
        static const bool stamped = strchr(seq, '@') != nullptr;
        // LO_AUTO_PULSE=<polls>: how many consecutive polls a press is held (default 20).
        static const uint32_t pulseLength = getenv("LO_AUTO_PULSE") ? std::max(1ul, strtoul(getenv("LO_AUTO_PULSE"), nullptr, 10)) : 20;
        static uint32_t totalPolls = 0, pollsAtLastPress = 0;
        const uint32_t swaps = ::g_presentedSwaps.load();
        char pressed = 0;
        totalPolls++;

        if (stamped)
        {
            // Parse once: "<letter>@<swap>" entries separated by commas.
            struct Entry { char button; uint32_t swap; };
            static std::vector<Entry> entries = [] {
                std::vector<Entry> v;
                for (const char* p = seq; *p;)
                {
                    while (*p == ',' || *p == ' ') p++;
                    if (!*p) break;
                    char b = *p++;
                    uint32_t at = 0;
                    if (*p == '@') at = strtoul(p + 1, const_cast<char**>(&p), 10);
                    v.push_back({ b, at });
                    while (*p && *p != ',') p++;
                }
                return v;
            }();
            static size_t index = 0;
            static uint32_t polls = 0;
            if (index < entries.size() && swaps >= entries[index].swap)
            {
                if (polls == 0)
                {
                    LOG_INFO("auto input: '{}' at swap {} ({} polls since the previous press)", entries[index].button, swaps, totalPolls - pollsAtLastPress);
                    pollsAtLastPress = totalPolls;
                }
                pressed = entries[index].button;
                if (++polls >= pulseLength) { polls = 0; index++; }
            }
        }
        else if (autoStartSwap && swaps >= autoStartSwap)
        {
            static uint32_t pulseFrame = 0, pulsePolls = 0, pulseIndex = 0, lastPulseFrame = 0;
            if (swaps >= pulseFrame + 240) { pulseFrame = swaps; pulsePolls = 0; }
            if (pulsePolls < pulseLength)
            {
                pulsePolls++;
                if (lastPulseFrame != pulseFrame) { if (lastPulseFrame) pulseIndex++; lastPulseFrame = pulseFrame; }
                size_t n = strlen(seq);
                pressed = n ? seq[pulseIndex < n ? pulseIndex : n - 1] : 's';
                if (pulsePolls == 1)
                    LOG_INFO("auto input: '{}' at swap {}", pressed, swaps);
            }
        }

        switch (pressed)
        {
        case 0: break;
        case 'a': gp.wButtons |= XAMINPUT_GAMEPAD_A; break;
        case 'b': gp.wButtons |= XAMINPUT_GAMEPAD_B; break;
        case 'x': gp.wButtons |= XAMINPUT_GAMEPAD_X; break;
        case 'y': gp.wButtons |= XAMINPUT_GAMEPAD_Y; break;
        case 'u': gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_UP; break;
        case 'd': gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_DOWN; break;
        case 'l': gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_LEFT; break;
        case 'r': gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_RIGHT; break;
        default: gp.wButtons |= XAMINPUT_GAMEPAD_START; break;
        }
    }

    // Keyboard fallback (only when SDL video is up; harmless otherwise).
    int numKeys = 0;
    const Uint8* keys = SDL_GetKeyboardState(&numKeys);
    if (keys && numKeys)
    {
        if (keys[SDL_SCANCODE_UP]) gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_UP;
        if (keys[SDL_SCANCODE_DOWN]) gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_DOWN;
        if (keys[SDL_SCANCODE_LEFT]) gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_LEFT;
        if (keys[SDL_SCANCODE_RIGHT]) gp.wButtons |= XAMINPUT_GAMEPAD_DPAD_RIGHT;
        if (keys[SDL_SCANCODE_RETURN]) gp.wButtons |= XAMINPUT_GAMEPAD_START;
        if (keys[SDL_SCANCODE_BACKSPACE]) gp.wButtons |= XAMINPUT_GAMEPAD_BACK;
        if (keys[SDL_SCANCODE_Z]) gp.wButtons |= XAMINPUT_GAMEPAD_A;
        if (keys[SDL_SCANCODE_X]) gp.wButtons |= XAMINPUT_GAMEPAD_B;
        if (keys[SDL_SCANCODE_A]) gp.wButtons |= XAMINPUT_GAMEPAD_X;
        if (keys[SDL_SCANCODE_S]) gp.wButtons |= XAMINPUT_GAMEPAD_Y;
        if (keys[SDL_SCANCODE_Q]) gp.wButtons |= XAMINPUT_GAMEPAD_LEFT_SHOULDER;
        if (keys[SDL_SCANCODE_W]) gp.wButtons |= XAMINPUT_GAMEPAD_RIGHT_SHOULDER;
        if (keys[SDL_SCANCODE_I]) gp.sThumbLY = 32767;
        if (keys[SDL_SCANCODE_K]) gp.sThumbLY = -32768;
        if (keys[SDL_SCANCODE_J]) gp.sThumbLX = -32768;
        if (keys[SDL_SCANCODE_L]) gp.sThumbLX = 32767;
    }

    return ERROR_SUCCESS;
}

uint32_t hid::SetState(uint32_t dwUserIndex, XAMINPUT_VIBRATION* pVibration)
{
    if (dwUserIndex != 0)
        return ERROR_DEVICE_NOT_CONNECTED;
    std::lock_guard lock(g_hidMutex);
    if (g_controller)
        SDL_GameControllerRumble(g_controller, pVibration->wLeftMotorSpeed, pVibration->wRightMotorSpeed, 100);
    return ERROR_SUCCESS;
}

uint32_t hid::GetCapabilities(uint32_t dwUserIndex, XAMINPUT_CAPABILITIES* pCaps)
{
    if (dwUserIndex != 0)
        return ERROR_DEVICE_NOT_CONNECTED;
    memset(pCaps, 0, sizeof(*pCaps));
    pCaps->Type = XAMINPUT_DEVTYPE_GAMEPAD;
    pCaps->SubType = XAMINPUT_DEVSUBTYPE_GAMEPAD;
    pCaps->Flags = 0;
    pCaps->Gamepad.wButtons = 0xFFFF;
    pCaps->Gamepad.bLeftTrigger = 0xFF;
    pCaps->Gamepad.bRightTrigger = 0xFF;
    pCaps->Gamepad.sThumbLX = (int16_t)0xFFC0;
    pCaps->Gamepad.sThumbLY = (int16_t)0xFFC0;
    pCaps->Gamepad.sThumbRX = (int16_t)0xFFC0;
    pCaps->Gamepad.sThumbRY = (int16_t)0xFFC0;
    pCaps->Vibration.wLeftMotorSpeed = 0xFFFF;
    pCaps->Vibration.wRightMotorSpeed = 0xFFFF;
    return ERROR_SUCCESS;
}
