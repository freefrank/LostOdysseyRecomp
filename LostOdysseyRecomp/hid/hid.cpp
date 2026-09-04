#include <stdafx.h>
#include "hid.h"
#include <kernel/xdm.h>
#include <os/logger.h>
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
        for (int i = 0; i < SDL_NumJoysticks(); i++)
        {
            if (SDL_IsGameController(i))
            {
                g_controller = SDL_GameControllerOpen(i);
                if (g_controller)
                {
                    LOG_INFO("controller: {}", SDL_GameControllerName(g_controller));
                    break;
                }
            }
        }
    }
}

void hid::Init()
{
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
    {
        LOG_WARNING("SDL controller init failed: {}", SDL_GetError());
        return;
    }
    OpenFirstController();
}

void hid::Poll()
{
    std::lock_guard lock(g_hidMutex);
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        if (e.type == SDL_CONTROLLERDEVICEADDED)
            OpenFirstController();
        else if (e.type == SDL_CONTROLLERDEVICEREMOVED && g_controller &&
            SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_controller)) == e.cdevice.which)
        {
            SDL_GameControllerClose(g_controller);
            g_controller = nullptr;
        }
        else if (e.type == SDL_QUIT)
        {
            std::_Exit(0);
        }
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
