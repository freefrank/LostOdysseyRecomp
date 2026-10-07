#define SDL_MAIN_HANDLED
#include <stdafx.h>
#include <hid/hid.h>
#include <hid/controller_prompts.h>
#if LO_PLATFORM_ANDROID
#include <hid/android_touch.h>
#endif
#include <debug/menu_overlay.h>
#include <SDL3/SDL.h>
#include <condition_variable>
#include <future>

std::atomic<uint32_t> g_presentedSwaps{0};
namespace
{
std::atomic<uint32_t> g_settingsFilterCalls{0};
std::atomic<bool> g_overlayVisible{false};
std::mutex g_overlayStubMutex;
std::condition_variable g_overlayStubCv;
bool g_blockOverlay = false;
bool g_overlayEntered = false;
}
namespace settings
{
bool FilterInput(uint16_t&, int16_t, int16_t)
{
    ++g_settingsFilterCalls;
    return false;
}
}
namespace frame_timing { uint64_t InputTick() { return 0; } }
namespace debug_menu
{
void ToggleOverlay()
{
    std::unique_lock lock(g_overlayStubMutex);
    if (!g_blockOverlay) return;
    g_overlayEntered = true;
    g_overlayStubCv.notify_all();
    g_overlayStubCv.wait(lock, [] { return !g_blockOverlay; });
}
bool IsOverlayVisible() { return g_overlayVisible.load(); }
void HandleInput(InputAction) {}
}

static void Check(bool value, const char* message)
{
    if (!value) { fprintf(stderr, "FAIL: %s (%s)\n", message, SDL_GetError()); std::exit(1); }
}

// Last motor speeds SDL sent to the virtual rumble pad (SDL skips repeats).
static int g_rumbleLow = -1, g_rumbleHigh = -1;
static bool SDLCALL RecordRumble(void*, Uint16 low, Uint16 high)
{
    g_rumbleLow = low;
    g_rumbleHigh = high;
    return true;
}

static SDL_JoystickID AttachVirtualGamepad()
{
    SDL_VirtualJoystickDesc desc{};
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    return SDL_AttachVirtualJoystick(&desc);
}

int main()
{
    using namespace hid::prompts;
    Check(IsPlayStation(SDL_GAMEPAD_TYPE_PS3) && IsPlayStation(SDL_GAMEPAD_TYPE_PS4) &&
          IsPlayStation(SDL_GAMEPAD_TYPE_PS5), "PlayStation SDL types");
    Check(!IsPlayStation(SDL_GAMEPAD_TYPE_XBOXONE) && !IsPlayStation(SDL_GAMEPAD_TYPE_UNKNOWN),
          "non-PlayStation SDL types");
    Check(Symbol(Face::Y) == L'\u25b3' && Symbol(Face::B) == L'\u25cb' &&
          Symbol(Face::A) == L'\u00d7' && Symbol(Face::X) == L'\u25a1', "four physical face mappings");
    auto lines = [](Face face) {
        std::vector<std::array<int, 4>> result;
        DrawFace(face, 0, 0, 20, [&](int x, int y, int x2, int y2) {
            result.push_back({x, y, x2, y2});
        });
        return result;
    };
    Check(lines(Face::Y).size() == 3 && lines(Face::Y)[0] == std::array{10, 2, 19, 18}, "triangle strokes");
    Check(lines(Face::B).size() == 8 && lines(Face::B)[0] == std::array{7, 2, 13, 2}, "circle strokes");
    Check(lines(Face::A).size() == 2 && lines(Face::A)[0] == std::array{3, 3, 17, 17}, "cross strokes");
    Check(lines(Face::X).size() == 4 && lines(Face::X)[0] == std::array{3, 3, 17, 3}, "square strokes");
    ActiveController prompts;
    prompts.Connected(11, SDL_GAMEPAD_TYPE_PS4);
    Check(prompts.PlayStation(), "PS4 connected");
    prompts.Connected(12, SDL_GAMEPAD_TYPE_XBOXONE);
    prompts.Observe(12, 1u << SDL_GAMEPAD_BUTTON_SOUTH, false);
    Check(!prompts.PlayStation(), "Xbox activity takes over");
    prompts.Observe(11, 0, true);
    Check(prompts.PlayStation(), "PS4 stick takes over");
    prompts.Keyboard();
    Check(!prompts.PlayStation(), "keyboard takes over");
    prompts.Observe(11, 0, true);
    Check(!prompts.PlayStation(), "held PS4 stick does not override keyboard");
    prompts.Observe(11, 0, false);
    prompts.Observe(11, 1u << SDL_GAMEPAD_BUTTON_EAST, false);
    Check(prompts.PlayStation(), "new PS4 button press takes over");
    prompts.Disconnected(11);
    Check(!prompts.PlayStation(), "PS4 removal falls back to Xbox");
    prompts.Connected(13, SDL_GAMEPAD_TYPE_PS5);
    prompts.Observe(13, 1u << SDL_GAMEPAD_BUTTON_NORTH, false);
    Check(prompts.PlayStation(), "PS5 button takes over");
    prompts.Disconnected(13);
    Check(!prompts.PlayStation(), "PS5 removal falls back to Xbox");
    prompts.Disconnected(12);
    Check(!prompts.PlayStation(), "all controllers removed");

    hid::Init();
    const SDL_JoystickID first = AttachVirtualGamepad();
    const SDL_JoystickID second = AttachVirtualGamepad();
    Check(first != 0 && second != 0, "attach two controllers");
    auto* a = SDL_OpenJoystick(first);
    auto* b = SDL_OpenJoystick(second);
    Check(a && b, "open virtual joysticks");
    auto sample = [] {
        SDL_UpdateJoysticks();
        XAMINPUT_STATE state;
        memset(&state, 0xff, sizeof(state)); // GetState must clear old caller state.
        Check(hid::GetState(0, &state) == 0, "get state");
        return state.Gamepad;
    };
    sample(); // consume added events
#if LO_PLATFORM_ANDROID
    Check(!hid::HasConnectedController(), "SDL virtual pads do not trigger physical-controller hiding");
#endif
    Check(!hid::UsesPlayStationPrompts(), "virtual non-PS controller retains regular prompts");
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_SOUTH, 1);
    Check(sample().wButtons & XAMINPUT_GAMEPAD_A, "first controller A");
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_SOUTH, 0);
    SDL_SetJoystickVirtualButton(b, SDL_GAMEPAD_BUTTON_EAST, 1);
    auto state = sample();
    Check((state.wButtons & XAMINPUT_GAMEPAD_B) && !(state.wButtons & XAMINPUT_GAMEPAD_A), "second controller takes over");
    hid::HandleKeyboardEvent(SDL_SCANCODE_Z, true);
    state = sample();
    Check(!hid::UsesPlayStationPrompts(), "keyboard retains regular prompts");
    Check((state.wButtons & (XAMINPUT_GAMEPAD_A | XAMINPUT_GAMEPAD_B)) == (XAMINPUT_GAMEPAD_A | XAMINPUT_GAMEPAD_B), "keyboard with connected controllers");
    hid::HandleKeyboardEvent(SDL_SCANCODE_Z, false);
    Check(!(sample().wButtons & XAMINPUT_GAMEPAD_A), "keyboard release");
    SDL_SetJoystickVirtualButton(b, SDL_GAMEPAD_BUTTON_EAST, 0);
    SDL_SetJoystickVirtualAxis(a, SDL_GAMEPAD_AXIS_LEFTX, 1000);
    SDL_SetJoystickVirtualAxis(b, SDL_GAMEPAD_AXIS_LEFTX, 20000);
    Check(sample().sThumbLX == 20000, "idle pad does not overwrite moving pad");
    hid::HandleKeyboardEvent(SDL_SCANCODE_J, true);
    Check(sample().sThumbLX == -32768, "keyboard stick overrides pad while held");
    hid::HandleKeyboardEvent(SDL_SCANCODE_J, false);
    Check(sample().sThumbLX == 20000, "pad resumes after keyboard release");
    SDL_SetJoystickVirtualAxis(b, SDL_GAMEPAD_AXIS_LEFTX, 0);
    Check(sample().sThumbLX == 0, "idle drift deadzone");
    SDL_SetJoystickVirtualAxis(b, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 32767);
    Check(sample().bRightTrigger == 255, "second pad trigger");
    SDL_SetJoystickVirtualAxis(b, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
    hid::HandleKeyboardEvent(SDL_SCANCODE_R, true);
    Check(sample().bRightTrigger == 255, "keyboard trigger");
    hid::ClearKeyboardState();
    Check(sample().bRightTrigger == 0, "focus loss releases keyboard");

#if LO_PLATFORM_ANDROID
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_SOUTH, 1);
    SDL_SetJoystickVirtualAxis(b, SDL_GAMEPAD_AXIS_LEFTX, 20000);
    SDL_SetJoystickVirtualAxis(a, SDL_GAMEPAD_AXIS_RIGHTX, 16000);
    SDL_SetJoystickVirtualAxis(b, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 28000);
    SDL_SetJoystickVirtualAxis(a, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 14000);
    hid::android_touch::Update(XAMINPUT_GAMEPAD_B, 160, 210, 1000, 0, 25000, 0);
    state = sample();
    auto controllerTrigger = [](SDL_Joystick* joystick, SDL_GamepadAxis axis) {
        auto* controller = SDL_GetGamepadFromID(SDL_GetJoystickID(joystick));
        Check(controller != nullptr, "find virtual game controller");
        return uint8_t(std::max(0, int(SDL_GetGamepadAxis(controller, axis))) >> 7);
    };
    const auto controllerLeftTrigger = controllerTrigger(b, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    const auto controllerRightTrigger = controllerTrigger(a, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    Check(controllerLeftTrigger > 160 && controllerRightTrigger < 210,
          "controller and touch trigger strength setup");
    Check((state.wButtons & (XAMINPUT_GAMEPAD_A | XAMINPUT_GAMEPAD_B)) ==
          (XAMINPUT_GAMEPAD_A | XAMINPUT_GAMEPAD_B), "SDL and touch buttons merge");
    Check(state.bLeftTrigger == controllerLeftTrigger && state.bRightTrigger == 210,
          "each trigger keeps its stronger source");
    Check(state.sThumbLX == 20000 && state.sThumbRX == 25000,
          "quiet touch stick preserves SDL; active touch stick wins");
    hid::android_touch::Update(XAMINPUT_GAMEPAD_B, 160, 210, 26000, 0, 1000, 0);
    state = sample();
    Check(state.sThumbLX == 26000 && state.sThumbRX == 16000,
          "stick selection follows the active source independently");
    hid::android_touch::Clear();
    state = sample();
    Check((state.wButtons & XAMINPUT_GAMEPAD_A) && !(state.wButtons & XAMINPUT_GAMEPAD_B),
          "clearing touch leaves SDL button held");
    Check(state.bLeftTrigger == controllerLeftTrigger && state.bRightTrigger == controllerRightTrigger &&
          state.sThumbLX == 20000 && state.sThumbRX == 16000,
          "clearing touch leaves SDL triggers and sticks held");
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_SOUTH, 0);
    SDL_SetJoystickVirtualAxis(b, SDL_GAMEPAD_AXIS_LEFTX, 0);
    SDL_SetJoystickVirtualAxis(a, SDL_GAMEPAD_AXIS_RIGHTX, 0);
    SDL_SetJoystickVirtualAxis(b, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, -32768);
    SDL_SetJoystickVirtualAxis(a, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, -32768);
    sample();
#endif

    g_overlayVisible = true;
    g_settingsFilterCalls = 0;
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_SOUTH, 1);
    Check(sample().wButtons == 0, "debug overlay did not consume game input");
    Check(g_settingsFilterCalls.load() == 0, "settings menu consumed debug overlay input");
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_SOUTH, 0);
    sample();
    Check(g_settingsFilterCalls.load() == 0, "settings menu ran while debug overlay was visible");
    g_overlayVisible = false;
    sample();
    Check(g_settingsFilterCalls.load() != 0, "settings input did not resume after debug overlay closed");

    sample();
    {
        std::lock_guard lock(g_overlayStubMutex);
        g_blockOverlay = true;
        g_overlayEntered = false;
    }
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 1);
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 1);
    auto menuAction = std::async(std::launch::async, sample);
    {
        std::unique_lock lock(g_overlayStubMutex);
        Check(g_overlayStubCv.wait_for(lock, std::chrono::seconds(2), [] { return g_overlayEntered; }),
              "debug menu action did not start");
    }
    auto keyboardEvent = std::async(std::launch::async, [] { hid::HandleKeyboardEvent(SDL_SCANCODE_Z, true); });
    Check(keyboardEvent.wait_for(std::chrono::seconds(1)) == std::future_status::ready,
          "debug menu action retained HID device lock");
    {
        std::lock_guard lock(g_overlayStubMutex);
        g_blockOverlay = false;
    }
    g_overlayStubCv.notify_all();
    menuAction.get();
    keyboardEvent.get();
    hid::ClearKeyboardState();
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, 0);
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, 0);
    sample();

    SDL_SetJoystickVirtualButton(b, SDL_GAMEPAD_BUTTON_EAST, 1);
    sample();
    SDL_CloseJoystick(b);
    Check(SDL_DetachVirtualJoystick(second), "detach second");
    Check(!(sample().wButtons & XAMINPUT_GAMEPAD_B), "disconnect releases state");
    SDL_SetJoystickVirtualButton(a, SDL_GAMEPAD_BUTTON_SOUTH, 1);
    Check(sample().wButtons & XAMINPUT_GAMEPAD_A, "remaining controller works");
    SDL_CloseJoystick(a);
    Check(SDL_DetachVirtualJoystick(first), "detach first");
    sample();
    hid::HandleKeyboardEvent(SDL_SCANCODE_Z, true);
    Check(sample().wButtons & XAMINPUT_GAMEPAD_A, "keyboard with no controllers");
    hid::ClearKeyboardState();
    Check(sample().wButtons == 0, "neutral state does not retain caller bits");
    const SDL_JoystickID reconnected = AttachVirtualGamepad();
    Check(reconnected != 0, "reattach controller");
    auto* c = SDL_OpenJoystick(reconnected);
    Check(c != nullptr, "open reattached controller");
    // Runtime mode: another thread pumps and forwards events to HID.
    hid::SetExternalEventPump(true);
    hid::HandleControllerEvent(SDL_EVENT_GAMEPAD_ADDED, reconnected);
    hid::HandleControllerEvent(SDL_EVENT_GAMEPAD_ADDED, reconnected); // duplicate notification
    SDL_SetJoystickVirtualButton(c, SDL_GAMEPAD_BUTTON_NORTH, 1);
    Check(sample().wButtons & XAMINPUT_GAMEPAD_Y, "reattached controller with external event pump");
    const auto instance = SDL_GetJoystickID(c);
    SDL_CloseJoystick(c);
    SDL_DetachVirtualJoystick(reconnected);
    hid::HandleControllerEvent(SDL_EVENT_GAMEPAD_REMOVED, instance);
    Check(sample().wButtons == 0, "external hot-unplug clears held input");

    // The Vibration setting scales the guest's motor speeds and rescales a running rumble.
    SDL_VirtualJoystickDesc rumbleDesc{};
    SDL_INIT_INTERFACE(&rumbleDesc);
    rumbleDesc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    rumbleDesc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    rumbleDesc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    rumbleDesc.Rumble = RecordRumble;
    const SDL_JoystickID rumblePad = SDL_AttachVirtualJoystick(&rumbleDesc);
    Check(rumblePad != 0, "attach rumble controller");
    hid::HandleControllerEvent(SDL_EVENT_GAMEPAD_ADDED, rumblePad);
    XAMINPUT_VIBRATION vibration{};
    vibration.wLeftMotorSpeed = 0xFFFF;
    vibration.wRightMotorSpeed = 0x8000;
    Check(hid::SetState(0, &vibration) == 0 && g_rumbleLow == 0xFFFF && g_rumbleHigh == 0x8000,
          "default strength passes the guest speeds through");
    hid::SetVibrationStrength(50);
    Check(g_rumbleLow == 0x7FFF && g_rumbleHigh == 0x4000, "strength change rescales the running rumble");
    vibration.wLeftMotorSpeed = 0x2000;
    hid::SetState(0, &vibration);
    Check(g_rumbleLow == 0x1000 && g_rumbleHigh == 0x4000, "new guest request uses the strength");
    hid::SetVibrationStrength(0);
    Check(g_rumbleLow == 0 && g_rumbleHigh == 0, "zero strength stops the running rumble");
    vibration.wLeftMotorSpeed = 0xFFFF;
    hid::SetState(0, &vibration);
    Check(g_rumbleLow == 0 && g_rumbleHigh == 0, "zero strength keeps rumble off");
    hid::PreviewVibration();
    Check(g_rumbleLow == 0 && g_rumbleHigh == 0, "preview does not interrupt a guest rumble");
    hid::SetVibrationStrength(250);
    Check(g_rumbleLow == 0xFFFF && g_rumbleHigh == 0x8000, "strength is bounded to retail");
    vibration = {};
    hid::SetState(0, &vibration);
    Check(g_rumbleLow == 0 && g_rumbleHigh == 0, "guest stop clears rumble");
    g_rumbleLow = g_rumbleHigh = -1;
    hid::SetVibrationStrength(30);
    Check(g_rumbleLow == -1, "strength change without guest rumble sends nothing");
    hid::PreviewVibration();
    Check(g_rumbleLow == 19660 && g_rumbleHigh == 19660, "preview pulses at the strongest guest level times strength");
    hid::SetVibrationStrength(100);
    SDL_DetachVirtualJoystick(rumblePad);
    SDL_Quit();
    puts("PASS: SDL PS types, PS/nonPS active device, keyboard, hotplug, four face mappings; virtual input merger; rumble strength");
    return 0;
}
