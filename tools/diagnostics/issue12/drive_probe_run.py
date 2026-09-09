"""Drive one isolated Issue-12 probe run up to Melvi's hand-in dialogue.

Usage (from out/issue12-triage/runtime):
  python drive_probe_run.py --run run-11 --exe ../probe-build/bin-nopoll/LostOdysseyRecomp.exe --sha <sha256> \
      [--delay-us 150] [--skip-stale 1] [--official]

It starts the game via session_probe.py (or session.py with --official), waits for the
title screen, navigates Continue -> Last Saved Game, waits for map 109, replays the walk to
Melvi, presses A once and writes a screenshot. It never presses the final A: inspect the
screenshot; if the dialogue box "Ah, you've gathered the flowers..." is NOT shown yet, run
  python drive_probe_run.py --run run-11 --step forward-talk
which walks forward 10 ticks, presses A and takes another screenshot. When the dialogue is
shown, run
  python drive_probe_run.py --run run-11 --step final-a
which presses the A that triggers the hand-in transition (crash / probe detection), waits
12 s and prints the probe/crash status.
"""
import argparse, json, os, re, subprocess, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
p = argparse.ArgumentParser()
p.add_argument('--run', required=True)
p.add_argument('--exe')
p.add_argument('--sha')
p.add_argument('--official', action='store_true', help='use session.py and the frozen official v0.4.2 EXE')
p.add_argument('--delay-us', default='0')
p.add_argument('--armed-delay-us', default='0', help='per-draw delay applied for --arm-seconds after the Nth SetSkeletalMesh event')
p.add_argument('--arm-on-swap', default='2')
p.add_argument('--arm-seconds', default='3')
p.add_argument('--gc-flush', default=None, help='set LO_GC_RENDER_FLUSH (gcflush builds only)')
p.add_argument('--skip-stale', default='1')
p.add_argument('--step', choices=['full', 'forward-talk', 'final-a'], default='full')
a = p.parse_args()
RUN = HERE / a.run
helper = 'session.py' if a.official else 'session_probe.py'
PY = sys.executable

def cmd(*args, wait=1):
    r = subprocess.run([PY, helper, *args, '--run', a.run, '--wait', str(wait)], cwd=HERE, capture_output=True, text=True, errors='replace')
    return r.stdout

def log():
    f = RUN / 'runtime.log'
    return f.read_text(encoding='utf-8', errors='replace') if f.exists() else ''

def shot():
    lines = cmd('shot', wait=3).strip().splitlines()
    return lines[-1] if lines else 'no screenshot'

def status():
    probe = RUN / 'probe.log'
    stale = probe.read_text(encoding='utf-8', errors='replace').count('### STALE') if probe.exists() else 0
    print(json.dumps(dict(crash='[crash]' in log(), stale_detections=stale, probe_log=str(probe), probe_bytes=probe.stat().st_size if probe.exists() else 0)))

if a.step == 'forward-talk':
    cmd('input', '0', '0', '24000', '10', wait=4)
    cmd('input', '1000', '0', '0', '3', wait=6)
    print(shot()); status(); sys.exit(0)
if a.step == 'final-a':
    cmd('input', '1000', '0', '0', '3', wait=10)
    time.sleep(2)
    print(shot()); status(); sys.exit(0)

env = dict(os.environ)
if not a.official:
    env['LO_ISSUE12_PROBE_FILE'] = str(RUN / 'probe.log').replace('\\', '/')
    env['LO_ISSUE12_SKIP_STALE'] = a.skip_stale
    env['LO_ISSUE12_RT_DELAY_US'] = a.delay_us
    env['LO_ISSUE12_RT_DELAY_ARMED_US'] = a.armed_delay_us
    env['LO_ISSUE12_ARM_ON_SWAP'] = a.arm_on_swap
    env['LO_ISSUE12_ARM_SECONDS'] = a.arm_seconds
    if a.gc_flush is not None: env['LO_GC_RENDER_FLUSH'] = a.gc_flush
    env['LO_TRACE_THREADS'] = '1'
start = [PY, helper, 'start', '--run', a.run, '--seed', 'checkpoints/flowers-ten', '--cache', 'run-01/shader-cache', '--wait', '1']
if not a.official:
    start += ['--exe', a.exe, '--exe-sha256', a.sha]
out = subprocess.run(start, cwd=HERE, env=env, capture_output=True, text=True, errors='replace').stdout
print(out.strip().splitlines()[-1])
t0 = time.time()
while time.time() - t0 < 420 and not re.search(r'heartbeat: swap #(1[3-9]\d\d|[2-9]\d\d\d)', log()):
    time.sleep(2)
print('title wait', round(time.time() - t0))
cmd('input', '1000', '0', '0', '3', wait=8)   # Press START screen
cmd('input', '10', '0', '0', '3', wait=8)     # Start -> main menu (Continue highlighted)
cmd('input', '1000', '0', '0', '3', wait=8)   # Continue
cmd('input', '1000', '0', '0', '3', wait=10)  # Last Saved Game (slot user08, 10/10 flowers)
t0 = time.time()
while time.time() - t0 < 120 and 'current map available=true id=109' not in log():
    time.sleep(2)
print('map109', 'current map available=true id=109' in log(), round(time.time() - t0))
time.sleep(30)
cmd('input', '0', '24000', '24000', '25', wait=4)
cmd('input', '0', '-24000', '24000', '20', wait=4)
cmd('input', '0', '0', '24000', '8', wait=4)
cmd('input', '0', '-24000', '0', '8', wait=4)
print([l for l in cmd('teleport', 'status', wait=3).splitlines() if 'teleport status' in l][-1:])
cmd('input', '1000', '0', '0', '3', wait=6)   # talk (may or may not open the dialogue)
print(shot()); status()
