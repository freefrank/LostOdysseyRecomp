"""Compile the actual guest-thread control, launch and close code with a fake guest body.

No emulator/game execution is claimed; the test releases a blocked fake guest
only after checking that closing the last handle did not wait for it.
"""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]

def between(text, begin, end):
    if text.count(begin) != 1 or text.count(end) != 1:
        raise RuntimeError("Production extraction boundary changed")
    return text[text.index(begin):text.index(end)]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='clang++')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    header = (ROOT/'LostOdysseyRecomp/cpu/guest_thread.h').read_text()
    source = (ROOT/'LostOdysseyRecomp/cpu/guest_thread.cpp').read_text()
    structs = between(header, 'struct GuestThreadParams', 'struct GuestThread\n')
    functions = between(source, 'static void GuestThreadFunc', 'template <typename ThreadType>')
    cpp = r'''
#include "kernel/dispatcher_wait.h"
#include "os/thread_name.h"
#include <atomic>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
using namespace std::chrono_literals;
struct KernelObject { virtual ~KernelObject()=default; virtual kernel::wait::Target* WaitTarget(){return nullptr;} };
'''+structs+r'''
static std::atomic<bool> releaseGuest{false};
struct GuestThread {
    static uint32_t Start(const GuestThreadParams&) {
        releaseGuest.wait(false);
        return 0;
    }
};
'''+functions+r'''
static void Check(bool ok, const char* message) { if(!ok) throw std::runtime_error(message); }
int main() try {
    auto handle=std::make_shared<GuestThreadHandle>(GuestThreadParams{});
    auto retained=handle;
    auto state=handle->control;
    handle.reset();
    Check(state->completion.Wait(0)==kernel::wait::Timeout,"closed alias terminated worker");
    auto close=std::async(std::launch::async,[h=std::move(retained)]() mutable { h.reset(); });
    const bool nonblocking=close.wait_for(150ms)==std::future_status::ready;
    // Always release even on failure, so the old join implementation is a
    // bounded test failure rather than hanging the test runner.
    releaseGuest=true; releaseGuest.notify_all();close.get();
    Check(nonblocking,"closing last handle joined a live guest thread");
    auto a=std::async(std::launch::async,[&]{return state->completion.Wait(1000);});
    auto b=std::async(std::launch::async,[&]{return state->completion.Wait(1000);});
    Check(a.get()==0 && b.get()==0,"completion cannot serve multiple waiters");
    std::weak_ptr<GuestThreadHandle::Control> weak=state;state.reset();
    const auto deadline=std::chrono::steady_clock::now()+1s;
    while(!weak.expired() && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
    Check(weak.expired(),"completed worker leaked its control block");
    // A suspended worker also owns its control independently of the handle.
    releaseGuest=false;
    auto suspended=std::make_unique<GuestThreadHandle>(GuestThreadParams{0,0,1,0});
    auto control=suspended->control;suspended.reset();
    Check(control->completion.Wait(0)==kernel::wait::Timeout,"suspended close completed worker");
    control->suspended=false;control->suspended.notify_all();
    releaseGuest=true;releaseGuest.notify_all();
    Check(control->completion.Wait(1000)==0,"detached suspended worker could not resume");
    std::cout<<"PASS production guest-thread lifetime: alias close, nonblocking last close, multiple waits, state release, suspended close\n";
} catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
'''
    (out/'thread.cpp').write_text(cpp)
    flags=['-std=c++20','-pthread','-I'+str(ROOT/'LostOdysseyRecomp')]
    flags+=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if args.sanitize else ['-O2']
    command=[args.cxx,*flags,str(out/'thread.cpp'),'-o',str(out/'thread')]
    build=subprocess.run(command, capture_output=True,text=True,timeout=120)
    (out/'build.log').write_text(build.stdout+build.stderr)
    if build.returncode: print(build.stderr);return build.returncode
    run=subprocess.run([str(out/'thread')],capture_output=True,text=True,timeout=15)
    (out/'run.log').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr,end='')
    return run.returncode
if __name__=='__main__': raise SystemExit(main())
