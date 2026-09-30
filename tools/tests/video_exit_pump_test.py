#!/usr/bin/env python3
"""Compile video's event-pump prelude and test hidden SDL/Win32 shutdown.

Use an x64 Developer Command Prompt (tools/setup_windows.bat).
Inputs: video.cpp, SDL headers, existing static SDL library.
Outputs: extracted source, fixture, build/run logs, results.json.
No game, GPU, fullscreen transition, network access or save changes.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess

HARNESS = r"""
#define SDL_MAIN_HANDLED
#define NOMINMAX
#include <x86intrin.h>
#include <windows.h>
#include <SDL.h>
#include <SDL_syswm.h>
#include <atomic>
#include <cstdio>
#include <thread>
namespace video {
SDL_Window* g_window = nullptr;
std::atomic<bool> exitRequested{false};
unsigned ordinaryWork = 0, displayWork = 0;
bool ExitRequested() { return exitRequested.load(); }
void RequestExit() { exitRequested = true; }
void PollDisplayRefresh() { ++displayWork; }
#include "pump_under_test.inc"
}
constexpr UINT kProbe = WM_APP + 82, kExit = WM_APP + 83;
constexpr LRESULT kReply = 0x82;
WNDPROC originalProc = nullptr;
HWND hwnd = nullptr;
unsigned workAtNativeExit = 0;
int failures = 0;
void Check(bool result, const char* name) {
    std::printf("%s: %s\n", result ? "PASS" : "FAIL", name);
    if (!result) ++failures;
}
LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == kExit) {
        video::RequestExit();
        workAtNativeExit = video::ordinaryWork + video::displayWork;
    }
    if (msg == kProbe || msg == kExit) return kReply;
    return CallWindowProcW(originalProc, window, msg, wp, lp);
}
bool Probe(UINT message) {
    std::atomic<bool> done{false};
    LRESULT sent = 0;
    DWORD_PTR reply = 0;
    std::thread sender([&] {
        // Finite synchronous send models DXGI's dependency without a driver.
        sent = SendMessageTimeoutW(hwnd, message, 0, 0,
            SMTO_BLOCK | SMTO_ABORTIFHUNG, 750, &reply);
        done.store(true);
    });
    while (!done.load()) {
        video::PumpWindowEvents();
        MsgWaitForMultipleObjectsEx(0, nullptr, 8, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    sender.join();
    return sent != 0 && reply == static_cast<DWORD_PTR>(kReply);
}
int main() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 2;
    }
    video::g_window = SDL_CreateWindow("LO exit pump fixture", 0, 0, 64, 64, SDL_WINDOW_HIDDEN);
    if (!video::g_window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 2;
    }
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(video::g_window, &info)) {
        std::fprintf(stderr, "SDL_GetWindowWMInfo: %s\n", SDL_GetError());
        SDL_DestroyWindow(video::g_window); SDL_Quit(); return 2;
    }
    hwnd = info.info.win.window;
    originalProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
        reinterpret_cast<LONG_PTR>(&WindowProc)));
    if (!originalProc) {
        std::fprintf(stderr, "SetWindowLongPtrW: %lu\n", GetLastError());
        SDL_DestroyWindow(video::g_window); SDL_Quit(); return 2;
    }
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    video::PumpWindowEvents();
    Check(video::ordinaryWork == 1 && video::displayWork == 1, "normal application work");
    SDL_Event quit{}; quit.type = SDL_QUIT;
    Check(SDL_PushEvent(&quit) == 1, "queue quit event");
    video::PumpWindowEvents();
    Check(video::ExitRequested() && video::ordinaryWork == 1, "quit skips application work");
    SDL_Event input{}; input.type = SDL_USEREVENT;
    Check(SDL_PushEvent(&input) == 1, "queue late input event");
    Check(Probe(kProbe), "native synchronous message completes after exit request");
    Check(video::ordinaryWork == 1 && video::displayWork == 2, "no shutdown UI/display work");
    Check(SDL_HasEvent(SDL_USEREVENT) == SDL_FALSE, "shutdown discards late SDL events");
    video::exitRequested = false;
    Check(Probe(kExit), "native callback requests exit during pump");
    Check(video::ordinaryWork + video::displayWork == workAtNativeExit,
        "exit is checked after native dispatch");
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalProc));
    SDL_DestroyWindow(video::g_window); video::g_window = nullptr;
    video::PumpWindowEvents();
    SDL_Quit();
    std::printf("failures=%d\n", failures);
    return failures ? 1 : 0;
}
"""

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--sdl-include', required=True, type=Path)
    parser.add_argument('--sdl-lib', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--compiler', default='clang-cl')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This is a Windows native-message fixture.')
    source = args.source.read_text(encoding='utf-8-sig')
    # Compile the actual production gate, including SDL_QUIT handling. Replace
    # the unrelated graphics/UI body with an observable counter.
    begin = source.index('    void PumpWindowEvents()\n    {')
    end = source.index('        static uint64_t shownProgress = 0;', begin)
    prelude = source[begin:end] + '        ++ordinaryWork;\n    }\n'
    if 'SDL_PumpEvents();' not in prelude or 'RequestExit();' not in prelude:
        raise RuntimeError('Event-pump structure changed; review fixture extraction.')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    (output / 'pump_under_test.inc').write_text(prelude, encoding='utf-8')
    cpp = output / 'fixture.cpp'
    cpp.write_text(HARNESS, encoding='utf-8')
    exe = output / 'fixture.exe'
    command = [args.compiler, '/nologo', '/std:c++20', '/EHsc', '/MT', '/W4', '/WX',
               f'/I{args.sdl_include.resolve()}', str(cpp), f'/Fe{exe}',
               '/link', str(args.sdl_lib.resolve()), 'user32.lib', 'gdi32.lib', 'winmm.lib', 'imm32.lib',
               'ole32.lib', 'oleaut32.lib', 'version.lib', 'uuid.lib',
               'advapi32.lib', 'setupapi.lib', 'shell32.lib']
    build = subprocess.run(command, cwd=output, capture_output=True, timeout=120)
    (output / 'build.log').write_bytes(build.stdout + build.stderr)
    result = {'source': str(args.source.resolve()), 'build_exit': build.returncode,
              'scope': 'production event-pump prelude; hidden SDL/Win32; no GPU or gameplay'}
    if build.returncode:
        print((build.stdout + build.stderr).decode(errors='replace'))
        code = build.returncode
    else:
        run = subprocess.run([str(exe)], cwd=output, capture_output=True, timeout=15)
        (output / 'run.log').write_bytes(run.stdout + run.stderr)
        print((run.stdout + run.stderr).decode(errors='replace'))
        result['run_exit'] = code = run.returncode
    (output / 'results.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return code


if __name__ == '__main__':
    raise SystemExit(main())
