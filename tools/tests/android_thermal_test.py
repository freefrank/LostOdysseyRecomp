"""Exercise the production Android thermal path with a fake libandroid.

Uses a host C++ compiler; these are ABI/control-flow checks, not NDK or device
tests. No sleeps, affinity changes, thermal policy or real Android API calls.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

THERMAL = """#pragma once
struct AThermalManager;
enum AThermalStatus { ATHERMAL_STATUS_ERROR=-1, ATHERMAL_STATUS_NONE=0,
                     ATHERMAL_STATUS_SEVERE=3 };
"""
LOGGER = """#pragma once
#include <cstdio>
template<class... T> void TestLog(const char* text, T...) { std::puts(text); }
#define LOG_INFO(...) TestLog(__VA_ARGS__)
#define LOG_WARNING(...) TestLog(__VA_ARGS__)
"""
LIBRARY = r"""#include <android/thermal.h>
#include <cmath>
#include <cstdlib>
struct AThermalManager {};
static AThermalManager manager;
static int acquired=0, released=0, statuses=0, headrooms=0;
extern "C" AThermalManager* AThermal_acquireManager() {
    ++acquired;
#ifdef NULL_MANAGER
    return nullptr;
#else
    return &manager;
#endif
}
extern "C" void AThermal_releaseManager(AThermalManager* value) {
    if(value!=&manager) std::abort();
    ++released;
}
#ifndef NO_STATUS
extern "C" AThermalStatus AThermal_getCurrentThermalStatus(AThermalManager* value) {
    if(value!=&manager) std::abort();
    ++statuses; return ATHERMAL_STATUS_SEVERE;
}
#endif
#ifndef NO_HEADROOM
extern "C" float AThermal_getThermalHeadroom(AThermalManager* value,int forecast) {
    if(value!=&manager || forecast!=0) std::abort();
    ++headrooms;
#ifdef NAN_HEADROOM
    return NAN;
#else
    return 0.75f;
#endif
}
#endif
extern "C" int test_count(int index) {
    return index==0?acquired:index==1?released:index==2?statuses:headrooms;
}
"""
MAIN = r"""#include <os/host_scheduling.h>
#include <dlfcn.h>
#include <cstdlib>
#include <cstdio>
static int (*count)(int);
static int expectedRelease;
static void Released() { if(count && count(1)!=expectedRelease) std::abort(); }
int main(int argc,char** argv) {
    if(argc!=4) return 2;
    void* library=dlopen("libandroid.so",RTLD_NOW|RTLD_LOCAL);
    count=library?reinterpret_cast<int(*)(int)>(dlsym(library,"test_count")):nullptr;
    const int acquired=std::atoi(argv[1]),queries=std::atoi(argv[2]),headrooms=std::atoi(argv[3]);
    expectedRelease=queries;
    // The production function's statics are constructed after this callback,
    // so its thermal manager must be released before Released runs at exit.
    std::atexit(Released);
    for(int i=0;i<1000;++i) os::scheduling::OnRenderFrame();
    if(count && (count(0)!=acquired || count(2)!=queries || count(3)!=headrooms)) return 1;
    std::puts("PASS thermal queries are bounded and resources released");
}
"""


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--cxx", default="g++")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="lo-thermal-") as temporary:
        root = Path(temporary)
        (root / "android").mkdir()
        (root / "os").mkdir()
        (root / "android/thermal.h").write_text(THERMAL)
        (root / "os/logger.h").write_text(LOGGER)
        (root / "library.cpp").write_text(LIBRARY)
        (root / "main.cpp").write_text(MAIN)
        binary = root / "thermal-test"
        subprocess.run([args.cxx, "-std=c++20", "-D__ANDROID__", "-I", str(root),
                        "-I", str(ROOT / "LostOdysseyRecomp"), str(root / "main.cpp"),
                        str(ROOT / "LostOdysseyRecomp/os/host_scheduling.cpp"),
                        "-ldl", "-pthread", "-o", str(binary)], check=True)
        cases = [("off", None, [], (0, 0, 0), None),
                 ("zero", "0", [], (0, 0, 0), None),
                 ("malformed", "true", [], (0, 0, 0), None),
                 ("available", "1", [], (1, 1, 1), "forecast_seconds=0"),
                 ("api30", "1", ["NO_HEADROOM"], (1, 1, 0), "headroom=unavailable"),
                 ("unsupported-headroom", "1", ["NAN_HEADROOM"], (1, 1, 1), "headroom=unavailable"),
                 ("missing-status", "1", ["NO_STATUS"], (0, 0, 0), "API unavailable"),
                 ("null-manager", "1", ["NULL_MANAGER"], (1, 0, 0), "API unavailable"),
                 ("missing-library", "1", None, (0, 0, 0), "API unavailable")]
        for name, enabled, definitions, expected, message in cases:
            directory = root / name
            directory.mkdir()
            if definitions is not None:
                subprocess.run([args.cxx, "-std=c++20", "-shared", "-fPIC", "-I", str(root),
                                *("-D" + item for item in definitions), str(root / "library.cpp"),
                                "-o", str(directory / "libandroid.so")], check=True)
            env = os.environ.copy()
            env.pop("LO_THERMAL_STATS", None)
            env.pop("LO_FRAME_STATS", None)
            env["LD_LIBRARY_PATH"] = str(directory)
            if enabled is not None:
                env["LO_THERMAL_STATS"] = enabled
            result = subprocess.run([str(binary), *map(str, expected)], env=env,
                                    capture_output=True, text=True, check=True)
            if message and result.stdout.count(message) != 1:
                raise AssertionError((name, result.stdout))
            if message is None and "thermal stats:" in result.stdout:
                raise AssertionError((name, "disabled path logged thermal data"))
            print("PASS", name)
    print("9 host thermal lifecycle checks passed; no device result claimed")


if __name__ == "__main__":
    main()
