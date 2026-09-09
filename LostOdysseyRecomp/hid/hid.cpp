#include <stdafx.h>
#include "hid.h"
#include <kernel/xdm.h>
#include <os/logger.h>
#include <os/shader_log.h>
#include <atomic>
extern std::atomic<uint32_t> g_presentedSwaps;
#include <vector>
#include <SDL.h>
#include <settings/menu.h>
#include <debug/frame_timing.h>
#include "test_input_pulse.h"

// SDL game controller -> XInput state. Player 1 only for now; the keyboard
// mirrors the pad so the game can be driven without a controller.

namespace
{
    std::vector<SDL_GameController*> g_controllers;
    std::array<Uint8, SDL_NUM_SCANCODES> g_keys{};
    Mutex g_hidMutex;
    uint32_t g_packet = 0;

    void OpenControllers()
    {
        for (int i = 0; i < SDL_NumJoysticks(); ++i)
        {
            const auto id = SDL_JoystickGetDeviceInstanceID(i);
            const bool opened = std::any_of(g_controllers.begin(), g_controllers.end(), [&](auto* pad) {
                return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == id;
            });
            if (opened || !SDL_IsGameController(i)) continue;
            if (auto* pad = SDL_GameControllerOpen(i))
            {
                g_controllers.push_back(pad);
                LOG_INFO("controller added: {} instance={} ({} connected)", SDL_GameControllerName(pad), id, g_controllers.size());
            }
            else LOG_WARNING("controller: open failed: {}", SDL_GetError());
        }
    }

    void MergeStick(int16_t& x, int16_t& y, int16_t candidateX, int16_t candidateY, int deadzone)
    {
        const auto magnitude = [](int16_t a, int16_t b) { return int64_t(a) * a + int64_t(b) * b; };
        if (magnitude(candidateX, candidateY) > int64_t(deadzone) * deadzone &&
            magnitude(candidateX, candidateY) > magnitude(x, y))
        { x = candidateX; y = candidateY; }
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
    OpenControllers();
}

static std::atomic<bool> g_externalPump{ false };

void hid::SetExternalEventPump(bool external)
{
    g_externalPump = external;
}

void hid::HandleControllerEvent(uint32_t eventType, int32_t which)
{
    std::lock_guard lock(g_hidMutex);
    if (eventType == SDL_CONTROLLERDEVICEADDED) OpenControllers();
    else if (eventType == SDL_CONTROLLERDEVICEREMOVED)
    {
        std::erase_if(g_controllers, [&](auto* pad) {
            if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) != which) return false;
            LOG_INFO("controller removed: instance={}", which);
            SDL_GameControllerClose(pad);
            return true;
        });
    }
}

void hid::HandleKeyboardEvent(int32_t scancode, bool pressed)
{
    std::lock_guard lock(g_hidMutex);
    if (scancode >= 0 && scancode < SDL_NUM_SCANCODES) g_keys[scancode] = pressed;
}

void hid::ClearKeyboardState()
{
    std::lock_guard lock(g_hidMutex);
    g_keys.fill(0);

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
        else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
            HandleKeyboardEvent(e.key.keysym.scancode, e.type == SDL_KEYDOWN);
        else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            ClearKeyboardState();
        else if (e.type == SDL_QUIT)
            (os::shaderlog::CloseForExit(), std::_Exit(0));
    }
}

uint32_t hid::GetState(uint32_t dwUserIndex, XAMINPUT_STATE* pState)
{
    if (dwUserIndex != 0)
        return ERROR_DEVICE_NOT_CONNECTED;

    Poll();

    std::lock_guard lock(g_hidMutex);
    *pState = {};
    pState->dwPacketNumber = ++g_packet;
    auto& gp = pState->Gamepad;

    if (!g_externalPump) SDL_GameControllerUpdate();
    for (auto* controller : g_controllers)
    {
        if (!SDL_GameControllerGetAttached(controller)) continue;
        auto btn = [&](SDL_GameControllerButton b) { return SDL_GameControllerGetButton(controller, b) != 0; };
        auto axis = [&](SDL_GameControllerAxis a) { return SDL_GameControllerGetAxis(controller, a); };

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

        gp.bLeftTrigger = std::max(gp.bLeftTrigger, uint8_t(std::max(0, int(axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT))) >> 7));
        gp.bRightTrigger = std::max(gp.bRightTrigger, uint8_t(std::max(0, int(axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT))) >> 7));
        auto flip = [](int16_t value) { return int16_t(std::min(32767, -int(value))); };
        MergeStick(gp.sThumbLX, gp.sThumbLY, axis(SDL_CONTROLLER_AXIS_LEFTX), flip(axis(SDL_CONTROLLER_AXIS_LEFTY)), 7849);
        MergeStick(gp.sThumbRX, gp.sThumbRY, axis(SDL_CONTROLLER_AXIS_RIGHTX), flip(axis(SDL_CONTROLLER_AXIS_RIGHTY)), 8689);
    }

    {
        static const bool trace = getenv("LO_TRACE_INPUT") != nullptr;
        static uint16_t lastButtons = 0;
        if (trace && gp.wButtons != lastButtons)
        {
            LOG_INFO("input: buttons {:#06x} (controller {})", gp.wButtons, !g_controllers.empty() ? "yes" : "no");
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
        case 'k': gp.wButtons |= XAMINPUT_GAMEPAD_BACK; break;
        case 'L': gp.wButtons |= XAMINPUT_GAMEPAD_LEFT_SHOULDER; break;
        case 'R': gp.wButtons |= XAMINPUT_GAMEPAD_RIGHT_SHOULDER; break;
        default: gp.wButtons |= XAMINPUT_GAMEPAD_START; break;
        }
    }

    // Reproducible field movement for local scene tests: x,y,firstSwap,lastSwap.
    // Disabled by default, bounded to one interval, no persistent controller state.
    {
        struct StickReplay { int x=0, y=0; unsigned first=0, last=0; bool valid=false; };
        static const StickReplay replay = [] {
            StickReplay r;
            if (const char* s = getenv("LO_AUTO_STICK"))
                r.valid = sscanf(s, "%d,%d,%u,%u", &r.x, &r.y, &r.first, &r.last) == 4 && r.last > r.first;
            return r;
        }();
        const uint32_t frame = g_presentedSwaps.load();
        if (replay.valid && frame >= replay.first && frame < replay.last)
        {
            gp.sThumbLX = int16_t(std::clamp(replay.x, -32768, 32767));
            gp.sThumbLY = int16_t(std::clamp(replay.y, -32768, 32767));
        }
    }

    // Event-thread snapshot: keyboard remains available with any number of pads.
    const auto& keys = g_keys;
    if (!keys.empty())
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
        if (keys[SDL_SCANCODE_E]) gp.bLeftTrigger = 255;
        if (keys[SDL_SCANCODE_R]) gp.bRightTrigger = 255;
        if (keys[SDL_SCANCODE_I]) gp.sThumbLY = 32767;
        if (keys[SDL_SCANCODE_K]) gp.sThumbLY = -32768;
        if (keys[SDL_SCANCODE_J]) gp.sThumbLX = -32768;
        if (keys[SDL_SCANCODE_L]) gp.sThumbLX = 32767;
    }

    // Background integration input, opt-in per test process. A new serial starts
    // One bounded pulse: "serial hexButtonMask leftX leftY polls [LT RT]".
    // Optional analog triggers are 0..255; the legacy five fields imply zero. No OS input.
    static const bool inputTicks = [] { const char* value = getenv("LO_TEST_INPUT_TICKS"); return value && strcmp(value, "1") == 0; }();
    uint32_t traceInputSerial = 0;
    uint64_t traceInputTick = 0;
    bool traceInputPending = false, traceInputActive = false;
    if (const char* path = getenv("LO_TEST_INPUT_FILE"))
    {
        static unsigned lastSerial = 0, pollsLeft = 0, buttons = 0, pollCount = 0;
        static TestInputPulse tickPulse;
        const uint64_t tick = inputTicks ? frame_timing::InputTick() : 0;
        static int leftX = 0, leftY = 0, leftTrigger = 0, rightTrigger = 0;
        if (++pollCount % 12 == 0)
        {
            if (FILE* file = fopen(path, "r"))
            {
                unsigned serial = 0, mask = 0, duration = 0;
                int x = 0, y = 0, lt = 0, rt = 0;
                const int fields = fscanf(file, "%u %x %d %d %u %d %d",
                    &serial, &mask, &x, &y, &duration, &lt, &rt);
                const bool valid = fields == 5 || fields == 7;
                fclose(file);
                if (valid && serial != lastSerial)
                {
                    lastSerial = serial;
                    buttons = mask & 0xffff;
                    leftX = std::clamp(x, -32768, 32767);
                    leftY = std::clamp(y, -32768, 32767);
                    leftTrigger = std::clamp(lt, 0, 255);
                    rightTrigger = std::clamp(rt, 0, 255);
                    pollsLeft = std::min(duration, 6000u);
                    if (inputTicks) tickPulse.Set(tick, pollsLeft);
                    LOG_INFO("background test input: serial={} buttons={:#x} stick={},{} polls={} triggers={},{}",
                        serial, buttons, leftX, leftY, pollsLeft, leftTrigger, rightTrigger);
                    if (inputTicks)
                        LOG_INFO("test input accepted: serial={} controller=0 tick={} start={} ticks={} mode=engine_tick",
                            serial, tick, tickPulse.start, tickPulse.duration);
                }
            }
        }
        // Test trigger state releases at the pulse boundary, including a
        // zero-duration command; never leave the prior RT value latched.
        gp.bLeftTrigger = 0;
        gp.bRightTrigger = 0;
        const bool active = inputTicks ? tickPulse.Active(tick) : pollsLeft != 0;
        if (active)
        {
            if (!inputTicks) --pollsLeft;
            gp.bLeftTrigger = uint8_t(leftTrigger);
            gp.bRightTrigger = uint8_t(rightTrigger);
            gp.wButtons |= uint16_t(buttons);
            gp.sThumbLX = int16_t(leftX);
            gp.sThumbLY = int16_t(leftY);
        }
        traceInputSerial = lastSerial;
        traceInputTick = tick;
        traceInputPending = inputTicks && tickPulse.Pending(tick);
        traceInputActive = active;
    }

    const uint16_t beforeMenuButtons = gp.wButtons;
    const bool menuFiltered = settings::FilterInput(gp.wButtons, gp.sThumbLX, gp.sThumbLY);
    if (menuFiltered) {
        gp.sThumbLX=gp.sThumbLY=gp.sThumbRX=gp.sThumbRY=0;
        gp.bLeftTrigger=gp.bRightTrigger=0;
    }
    if (inputTicks && traceInputSerial)
    {
        static uint64_t lastTick = ~uint64_t(0);
        static uint32_t lastSerial = 0, calls = 0;
        static bool wasActive = false, wasPending = false;
        if (traceInputTick != lastTick || traceInputSerial != lastSerial)
        {
            if (traceInputSerial != lastSerial || traceInputActive || traceInputPending || wasActive || wasPending)
                LOG_INFO("test input returned: serial={} controller=0 tick={} pending={} active={} buttons={:#x} before_menu={:#x} menu_filtered={} previous_tick_reads={}",
                    traceInputSerial, traceInputTick, traceInputPending, traceInputActive, gp.wButtons, beforeMenuButtons, menuFiltered, calls);
            lastTick = traceInputTick; lastSerial = traceInputSerial; calls = 0;
            wasActive = traceInputActive; wasPending = traceInputPending;
        }
        ++calls;
    }
    // Trace the final guest-facing value, including any opt-in test override.
    static const bool ringTrace = getenv("LO_RING_TRACE") != nullptr;
    static uint32_t lastTraceSwap = ~0u;
    const uint32_t traceSwap = g_presentedSwaps.load();
    if (ringTrace && traceSwap != lastTraceSwap)
    {
        lastTraceSwap = traceSwap;
        LOG_INFO("ring input: swap={} LT={} RT={} buttons={:#x}", traceSwap,
            gp.bLeftTrigger, gp.bRightTrigger, gp.wButtons);
    }
    return ERROR_SUCCESS;
}

uint32_t hid::SetState(uint32_t dwUserIndex, XAMINPUT_VIBRATION* pVibration)
{
    // Keep local debugging quiet. Opt in explicitly to restore controller rumble.
    static const bool rumbleEnabled = [] {
        const char* value = getenv("LO_CONTROLLER_RUMBLE");
        return value && strcmp(value, "1") == 0;
    }();
    if (dwUserIndex != 0)
        return ERROR_DEVICE_NOT_CONNECTED;
    if (!rumbleEnabled) return ERROR_SUCCESS;
    std::lock_guard lock(g_hidMutex);
    for (auto* controller : g_controllers)
        SDL_GameControllerRumble(controller, pVibration->wLeftMotorSpeed, pVibration->wRightMotorSpeed, 100);
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
