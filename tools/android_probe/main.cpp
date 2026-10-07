#include "memory_probe.h"
#include "vulkan_probe.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_system.h>
#include <android/log.h>
#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::atomic<bool> runProbe{true};
std::atomic<bool> runTone{false};

void Publish(const std::string& report) {
    std::istringstream lines(report);
    for (std::string line; std::getline(lines, line);)
        __android_log_print(ANDROID_LOG_INFO, "LOAndroidProbe", "%s", line.c_str());
    if (const char* path = SDL_GetAndroidInternalStoragePath()) {
        std::ofstream file(std::string(path) + "/probe-report.txt", std::ios::trunc);
        file << report;
        if (!file) __android_log_print(ANDROID_LOG_ERROR, "LOAndroidProbe", "Cannot write probe-report.txt");
    }
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) return;
    jclass type = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(type, "showProbeReport", "(Ljava/lang/String;)V");
    if (method) {
        jstring text = env->NewStringUTF(report.c_str());
        if (text) {
            env->CallVoidMethod(activity, method, text);
            env->DeleteLocalRef(text);
        }
    }
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
}

void OpenControllers(std::vector<SDL_Gamepad*>& controllers) {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; ++i) {
        const SDL_JoystickID id = ids[i];
        if (std::any_of(controllers.begin(), controllers.end(), [id](auto* pad) {
                return SDL_GetJoystickID(SDL_GetGamepadJoystick(pad)) == id;
            })) continue;
        if (auto* pad = SDL_OpenGamepad(id)) controllers.push_back(pad);
    }
    SDL_free(ids);
}
} // namespace

extern "C" JNIEXPORT void JNICALL
Java_io_github_freefrank_lostodyssey_probe_ProbeActivity_nativeRequestProbe(JNIEnv*, jclass) {
    runProbe.store(true);
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_freefrank_lostodyssey_probe_ProbeActivity_nativeRequestTone(JNIEnv*, jclass) {
    runTone.store(true);
}

extern "C" int SDL_main(int, char**) {
    // SDLActivity may restart the entry point without unloading libmain.so.
    runProbe.store(true);
    runTone.store(false);
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        Publish(std::string("SDL video initialization failed: ") + SDL_GetError());
        return 1;
    }
    const bool audioReady = SDL_InitSubSystem(SDL_INIT_AUDIO);
    const bool inputReady = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    SDL_Window* window = SDL_CreateWindow("Lost Odyssey Android Probe", 960, 540,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    std::string windowError = window ? "" : SDL_GetError();
    SDL_AudioSpec desired{};
    desired.freq = 48000;
    desired.format = SDL_AUDIO_F32;
    desired.channels = 2;
    SDL_AudioStream* audio = audioReady
        ? SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired, nullptr, nullptr)
        : nullptr;
    std::vector<SDL_Gamepad*> controllers;
    if (inputReady) OpenControllers(controllers);
    std::string report;
    bool running = true;
    bool foreground = true;
    while (running) {
        if (foreground && runProbe.exchange(false)) {
            std::ostringstream result;
            result << "Lost Odyssey Android port probe 0.1\nNo game/runtime compatibility claim.\n\n";
            result << lo::android_probe::ProbeMemory() << "\n";
            result << (window ? lo::android_probe::ProbeVulkan(window)
                              : "Vulkan window creation failed: " + windowError) << "\n";
            result << "Audio device: " << (audio ? "opened (use Test audio to hear a tone)" : "unavailable") << "\n";
            result << "SDL controllers: " << controllers.size() << "\n";
            for (auto* pad : controllers) result << "  " << SDL_GetGamepadName(pad) << "\n";
            result << "Report: app internal files/probe-report.txt; logcat tag LOAndroidProbe\n";
            report = result.str();
            Publish(report);
        }
        if (foreground && runTone.exchange(false)) {
            if (audio) {
                std::vector<float> samples(48000 * 2 / 5);
                for (size_t i = 0; i < samples.size() / 2; ++i)
                    samples[i * 2] = samples[i * 2 + 1] = 0.08f * std::sin(2.0f * 3.14159265f * 440.0f * i / 48000.0f);
                SDL_ClearAudioStream(audio);
                const bool queued = SDL_PutAudioStreamData(audio, samples.data(), int(samples.size() * sizeof(float)));
                SDL_ResumeAudioStreamDevice(audio);
                Publish(report + (queued ? "Audio tone queued; audibility needs listener confirmation.\n"
                                             : std::string("Audio queue failed: ") + SDL_GetError() + "\n"));
            } else Publish(report + "Audio test unavailable: no opened device.\n");
        }
        SDL_Event event;
        if (!SDL_WaitEventTimeout(&event, 100)) continue;
        switch (event.type) {
        case SDL_EVENT_QUIT: running = false; break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_AC_BACK || event.key.key == SDLK_ESCAPE) running = false;
            break;
        case SDL_EVENT_WILL_ENTER_BACKGROUND:
            foreground = false;
            if (audio) SDL_PauseAudioStreamDevice(audio);
            Publish(report + "Lifecycle: entered background; Vulkan probe resources already released.\n");
            break;
        case SDL_EVENT_DID_ENTER_FOREGROUND:
            foreground = true;
            runProbe.store(true);
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            OpenControllers(controllers);
            runProbe.store(true);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            controllers.erase(std::remove_if(controllers.begin(), controllers.end(), [](auto* pad) {
                if (SDL_GamepadConnected(pad)) return false;
                SDL_CloseGamepad(pad);
                return true;
            }), controllers.end());
            runProbe.store(true);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            Publish(report + "Controller button received: " + std::to_string(event.gbutton.button) + "\n");
            break;
        default: break;
        }
    }
    for (auto* pad : controllers) SDL_CloseGamepad(pad);
    if (audio) SDL_DestroyAudioStream(audio);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
