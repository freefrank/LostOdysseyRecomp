#!/usr/bin/env python3
"""Run the production guest-return and GPU-owner exit flow with driver stubs.

No game data or GPU is required. The real command-processor startup/shutdown
and video's owner-exit bodies are compiled with bounded fake workers; a global
destructor trap detects unsafe shared-state teardown after guest main returns.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def function(source: str, signature: str) -> str:
    if source.count(signature) != 1:
        raise RuntimeError(f"Production extraction boundary changed: {signature}")
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for position in range(opening, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[start:position + 1]
    raise RuntimeError(f"Unclosed function: {signature}")


HARNESS = r'''
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
#include "os/guest_code_thread.h"
#define LOG_INFO(...) ((void)0)
#define LOG_NOTICE(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
template <typename T> using be = T;
static const char* mode = nullptr;
static std::thread::id gpuOwner;
static std::atomic<bool> idleExitChecked{false};
static void Record(const char* event) { std::puts(event); }
namespace os::shaderlog { void CloseForExit() { Record("shader_log_closed"); } }
namespace host_ui { void RequestStop() { Record("pause_released"); } }
struct Memory {
    uint32_t registers[0x3000]{};
    void* Translate(uint32_t address) { return &registers[(address - 0x7FC80000u) / 4]; }
} g_memory;
namespace gpu {
constexpr uint32_t REGISTER_COUNT = 0x3000, MMIO_BASE = 0x7FC80000;
constexpr uint32_t REG_RB_EDRAM_TIMING = 0, REG_RB_BC_CONTROL = 1;
constexpr uint32_t REG_D1MODE_V_COUNTER = 2, REG_INTERRUPT_STATUS = 3, REG_D1MODE_VIEWPORT_SIZE = 4;
class CommandProcessor {
    std::vector<uint32_t> m_registers;
    std::atomic<uint64_t> m_constantGeneration[2]{};
    std::atomic<bool> m_running{false};
    std::thread m_worker, m_vsync, m_interruptThread;
    std::mutex m_writePtrMutex, m_interruptMutex;
    std::condition_variable m_writePtrChanged, m_interruptCv, m_waitProgress;
public:
    bool Init();
    void Shutdown();
    void RequestStopForExit();
    void MarkConstantsChanged(uint32_t, uint64_t) {}
    void WorkerMain() {
        if (std::strcmp(mode, "already-stopped") == 0) { m_running = false; return; }
        while (m_running) std::this_thread::yield();
    }
    void VsyncMain() { while (m_running) std::this_thread::yield(); }
    void InterruptMain() { while (m_running) std::this_thread::yield(); }
} g_commandProcessor;
namespace renderer {
    void WaitDebugCaptureArchive() {
        if (std::this_thread::get_id() != gpuOwner) std::_Exit(71);
        Record("capture_drained");
    }
}
namespace video {
    std::atomic<bool> g_exitRequested{false};
    bool Init() {
        gpuOwner = std::this_thread::get_id();
        return std::strcmp(mode, "headless") != 0;
    }
    bool ExitRequested() {
        const bool requested = g_exitRequested.load();
        if (std::strcmp(mode, "already-stopped") == 0 &&
            std::this_thread::get_id() == gpuOwner && !requested)
            idleExitChecked = true;
        return requested;
    }
    void RequestSkipShaderPreparation() { Record("preparation_stop_requested"); }
    void Shutdown() {
        if (std::this_thread::get_id() != gpuOwner) std::_Exit(72);
        Record("gpu_owner_cleanup");
    }
    void RequestExit();
    [[noreturn]] void FinishRequestedExit();
}
}
struct SharedStateDestructorTrap {
    ~SharedStateDestructorTrap() { Record("unsafe_global_teardown"); std::_Exit(70); }
} destructorTrap;
'''


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="g++")
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    cp = (ROOT / "LostOdysseyRecomp/gpu/command_processor.cpp").read_text()
    video = (ROOT / "LostOdysseyRecomp/gpu/video.cpp").read_text()
    main_source = (ROOT / "LostOdysseyRecomp/main.cpp").read_text()
    tail_marker = '    LOG_INFO("guest main thread returned");'
    if main_source.count(tail_marker) != 1:
        raise RuntimeError("Guest-return extraction boundary changed")
    guest_body = function(main_source, "static int RunGuest(uint32_t entry)")
    main_tail = guest_body[guest_body.index(tail_marker):]
    cpp = HARNESS + "\nnamespace gpu {\n" + "\n".join(
        function(cp, signature) for signature in (
            "bool CommandProcessor::Init()",
            "void CommandProcessor::RequestStopForExit()",
            "void CommandProcessor::Shutdown()",
        )
    ) + "\nnamespace video {\n" + "\n".join(
        function(video, signature) for signature in (
            "void RequestExit()",
            "[[noreturn]] void FinishRequestedExit()",
        )
    ) + r'''
} }
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    mode = argv[1];
    if (!gpu::g_commandProcessor.Init()) return 3;
    if (std::strcmp(mode, "already-stopped") == 0)
        while (!idleExitChecked) std::this_thread::yield();
    Record("fake_guest_returned");
''' + main_tail
    (out / "exit.cpp").write_text(cpp)
    executable = out / "exit"
    build = subprocess.run(
        [args.cxx, "-std=c++20", "-pthread", "-Wall", "-Wextra", "-Werror",
         "-I" + str(ROOT / "LostOdysseyRecomp"),
         str(out / "exit.cpp"), "-o", str(executable)],
        capture_output=True, text=True, timeout=60,
    )
    (out / "build.log").write_text(build.stdout + build.stderr)
    if build.returncode:
        print(build.stdout + build.stderr)
        return build.returncode
    import os
    for mode_name in ("normal", "headless", "no-renderer", "already-stopped"):
        environment = os.environ.copy()
        environment.pop("LO_HEADLESS", None)
        environment.pop("LO_NO_RENDERER", None)
        if mode_name == "headless":
            environment["LO_HEADLESS"] = "1"
        elif mode_name == "no-renderer":
            environment["LO_NO_RENDERER"] = "1"
        run = subprocess.run([str(executable), mode_name], env=environment,
                             capture_output=True, text=True, timeout=5)
        output = run.stdout + run.stderr
        (out / f"{mode_name}.log").write_text(output)
        events = run.stdout.splitlines()
        if run.returncode != 0 or "unsafe_global_teardown" in events:
            raise RuntimeError(f"Guest return failed ({mode_name}, {run.returncode}):\n{output}")
        if "shader_log_closed" not in events:
            raise RuntimeError(f"Guest return did not close the shader log ({mode_name}):\n{output}")
        if mode_name != "already-stopped":
            required = ["capture_drained", "gpu_owner_cleanup", "shader_log_closed"]
            cursor = -1
            for event in required:
                cursor = events.index(event, cursor + 1)
        else:
            if "gpu_owner_cleanup" in events:
                raise RuntimeError(f"Already-stopped fixture failed to reach fallback:\n{output}")
        path = "fallback process exit" if mode_name == "already-stopped" else "owner exit"
        print(f"PASS production guest-main return: {mode_name}; {path}, no global teardown")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
