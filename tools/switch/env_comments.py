"""One-line descriptions of the console's env.txt settings, so every setting a tool writes there has a comment above it.

bench.py (a benchmark run's env.txt) and lo_debug.py's edit_env (the "env" command, pgo_train.py) pass their lines
through commented(): a KEY=VALUE line without a comment right above it gets "# <description>". A key that is not in
DESCRIPTIONS points to docs/SWITCH.md, which lists them all ("Diagnostics switches"). Keep the descriptions short and
in step with that table.
"""
from __future__ import annotations

DESCRIPTIONS = {
    # The release env.txt (tools/switch/release-env.txt) and the Graphics menu's env.txt overrides
    "LO_SWITCH_BACKEND": "Graphics back end: deko3d (the Switch's own API, faster) or NVK when unset",
    "LO_DEKO3D_GPU_FAST": "deko3d's GPU savings: depth compression, depth culling, fewer GPU waits, leaner vertex shaders",
    "LO_DEKO3D_LEAN_SHADERS": "Leaner game shaders (levels 1-3); level 3 needs shaders/deko3d_shaders_lean3.lodk",
    "LO_DYNAMIC_RESOLUTION": "Dynamic resolution 0/1; wins over Settings > Graphics > Dynamic resolution",
    "LO_DYNAMIC_RESOLUTION_MIN": "Lowest scene height dynamic resolution uses (rows); wins over Graphics > Minimum resolution",
    "LO_DYNAMIC_RESOLUTION_STEP": "Rows per dynamic resolution step (a multiple of 9, default 36)",
    "LO_SWITCH_RENDER_HEIGHT": "Scene height in rows (540, 720, ...); wins over Settings > Graphics > Render resolution",
    "LO_SWITCH_OUTPUT_HEIGHT": "540: present 960x540 and let the display scale it (deko3d)",
    "LO_SWITCH_UPSCALER": "Final upscale: bilinear, fsr, easu or rcas; wins over Settings > Graphics > FSR",
    "LO_SWITCH_RCAS_STOPS": "FSR sharpening in stops, 0 the sharpest (0.25); wins over Graphics > FSR sharpness",
    "LO_SWITCH_UI_NATIVE": "1: text, HUD and menus at the output's size while the scene stays smaller",
    "LO_NATIVE_MOVIES": "Cutscene video decoded natively with FFmpeg (faster, smoother); 0 uses the game's decoder",
    "LO_GUEST_AFFINITY": "Spread the game's threads across the Switch's three CPU cores",
    "LO_ZPD_MODE": "xenia: made-up occlusion query counts (faster) instead of host queries",
    "LO_PIPELINE_PREBUILD": "Prepare known graphics states in the background at start (all)",
    "LO_SWITCH_BUTTONS": "position: the physical A/B/X/Y layout instead of the letters",
    "LO_SWITCH_AUTO_SLEEP": "0: the console never sleeps while the game runs, as before (benchmarks); 1: the system's settings",
    "LO_SWITCH_IDLE_SECONDS": "Test only: <dim>,<sleep> seconds instead of the system's (0,0 keeps them); debug clients don't keep it on",
    "LO_SWITCH_HOME_SUSPEND": "0: keep running behind the HOME menu (benchmarks, debug server); 1: the HOME menu suspends",
    # Debug server and network log
    "LO_DEBUG_SERVER": "Debug server for tools/switch/lo_debug.py: 1 (or a port) on, 0 off; wins over the Debug settings",
    "LO_DEBUG_RESTART": "1: the debug server's restart command works here (a HOME-menu forwarder); 0: never",
    "LO_DEBUG_PORT": "Debug server port (default 6543)",
    "LO_NETLOG": "Stream the runtime log over TCP: 1 or a port (default 28780)",
    "LO_DEBUG_LOG": "Detailed runtime log lines",
    # Profiling and logging
    "LO_CPU_PROFILE": "1: sample the CPU threads and write a profile (docs/SWITCH.md, CPU profile)",
    "LO_CPU_PROFILE_SAMPLES": "Keep every CPU profile sample: 1, or a comma-separated list of thread names",
    "LO_FRAME_TIMING": "1: one line per second with command flush, waits and present times",
    "LO_RENDER_TIMING": "1: per-frame render timing with GPU timestamps",
    "LO_GPU_DRAW_TIMING_EVERY": "GPU time of every group of four draws, every Nth frame",
    "LO_KERNEL_TRACE": "1: log guest kernel calls such as file reads",
    "LO_PERF_INTERVAL": "Seconds between perf: lines; 0 turns off the perf: and hitch: lines",
    "LO_PERF_GPU": "0: no GPU timestamps for the perf: line",
    "LO_PERF_HITCH_MS": "A hitch: line for every frame longer than this (ms)",
    "LO_PERF_HITCHES": "hitch: lines per session (300)",
    "LO_IO_PROFILE": "1: log game-file read statistics",
    "LO_LOAD_PROFILE": "1: log thread dumps during long loads",
    "LO_WAIT_PROFILE": "1: log guest waits of 1 ms or more every 10 seconds",
    "LO_THREAD_PLACEMENT": "1: log each core's idle share and the busy threads' placement every 30 s",
    "LO_HEAP_CENSUS": "Heap census builds (-DLO_SWITCH_HEAP_CENSUS=ON): 1 (or a sampling period in bytes) counts allocations by size and call site",
    "LO_HEAP_CENSUS_SECONDS": "Heap census: seconds between reports (state/heap-census/<seconds>.txt)",
    "LO_THREAD_CORES": "With LO_THREAD_PLACEMENT=1: name=core:hexmask,... moves busy threads, for placement experiments",
    "LO_DEKO3D_BARRIER_TRACE": "Log every barrier of one frame in N",
    "LO_DEKO3D_ZCULL_STATS": "deko3d: log the Zcull counters per depth target (1); needs diagnostic_log=1",
    # Threads and scheduling
    "LO_MAIN_CORE": "Core Guest Main stays on; -1 leaves it free to move",
    "LO_XAUDIO_WORKER_PRIORITY": "Hex priority of the XAudio workers (0x2D); 0 keeps the default 0x3B",
    "LO_MOVIE_GUEST_VIDEO_PRIORITY": "Hex priority of the game's video thread during native movies (0x3F); 0 keeps the game's",
    # Renderer A/B switches
    "LO_DEKO3D_MERGE_BARRIERS": "deko3d: back-to-back GPU barriers become one (0/1); wins over the Debug page",
    "LO_DEKO3D_LEAN_BARRIERS": "deko3d: barriers before a write keep the texture cache (0/1); wins over the Debug page",
    "LO_DEKO3D_ZCULL": "deko3d: hierarchical depth culling (0/1); wins over the Debug page",
    "LO_DEKO3D_DEPTH_COMPRESSION": "deko3d: depth buffer compression (0/1); wins over the Debug page",
    "LO_DEKO3D_COLOR_COMPRESSION": "deko3d: colour render target compression (0/1, on by default); wins over the Debug page",
    "LO_DEKO3D_VS_OUTPUTS": "deko3d: vertex shaders write only what the pixel shader reads (0/1); wins over the Debug page",
    "LO_DEKO3D_EARLY_Z": "deko3d: 0 keeps depth tests after the pixel shader on every draw",
    "LO_DEKO3D_PRESENT_THREAD": "deko3d: the compositor's buffer calls on a worker thread (1); 0 on GPU CmdProc",
    "LO_DEKO3D_RECORD_THREAD": "deko3d: 1 records commands on a second thread",
    "LO_DEKO3D_OCCLUSION": "deko3d: hardware occlusion queries (in testing)",
    "LO_DEKO3D_DESCRIPTOR_CACHE": "deko3d: each texture's image descriptor made once",
    "LO_DEKO3D_RELAXED_DESCRIPTOR_EPOCH": "deko3d: relaxed descriptor counters",
    "LO_DEKO3D_FB_DEDUPE": "deko3d: skip re-binding the framebuffer already bound",
    "LO_TRIM_VARYINGS": "Keep only the vertex outputs the pixel shader uses (1); 0 keeps them all",
    "LO_TEXINFO_SPEC": "Texture-info shader specialization (1); 0 turns it off",
    "LO_RECT_LIST_GS": "0: rectangle lists without the geometry shader path",
    "LO_RT32": "1: 32-bit formats for the two 10-bit render target classes",
    "LO_RT_REUSE": "Render targets reused for consecutive draws",
    "LO_SKIP_RESTORE": "0 draws the game's EDRAM restores instead of skipping them",
    "LO_SKIP_NOOP_DRAWS": "0 draws the draws that write no colour, depth or stencil",
    "LO_SPARSE_CONSTANTS": "A draw's constants uploaded only for the slots its shaders read (1)",
    "LO_CP_SELF_WAITS": "Command processor: a write followed by a wait for it is matched at once (1)",
    "LO_CP_FUSED_PLAIN_RUN": "Command processor: constant runs in one pass",
    "LO_CP_WPTR_LOCKFREE": "Command processor: lock-free write pointer",
    "LO_UPLOAD_VECTOR_COPY": "Wide upload copies",
    "LO_DRAW_REUSE": "1: a draw reuses the previous frame's lookups for its position; 2 checks them",
    "LO_EARLY_QUERY_ANSWER": "The scene renderer's occlusion reads answered at once while counts are made up",
    "LO_RESOLVE_BLIT": "Same-format resolves through the blit shader, not the copy engine",
    "LO_RESOLVE_FOLD_TRANSFER": "1: fewer resolve passes (fold transfers)",
    "LO_RESOLVE_SHARE": "1: fewer resolve passes (share resolves)",
    "LO_RESIZE_FIX": "Dynamic resolution without flicker after a size change; 0 turns it off",
    # Benchmark runs (tools/switch/bench.py, docs/SWITCH-BENCHMARKS.md)
    "LO_BENCH": "Benchmark run: its name; the run presses the scenario's buttons, measures and reports",
    "LO_BENCH_FRAMES": "Benchmark: frames measured",
    "LO_BENCH_SETTLE": "Benchmark: frames to settle before measuring",
    "LO_BENCH_TIMEOUT": "Benchmark: seconds before a run that never starts measuring gives up",
    "LO_BENCH_START_DRAWS": "Benchmark: starts once a frame has this many draws (the scene is on screen)",
    "LO_BENCH_START_SWAP": "Benchmark: starts at this presented frame",
    "LO_BENCH_STICK": "Benchmark: left stick held while measuring (x,y)",
    "LO_BENCH_SHOT": "Benchmark: 1 takes a screenshot at the end",
    "LO_BENCH_EXIT": "Benchmark: 1 quits the game when the run ends",
    "LO_AUTO_BUTTONS": "Scripted presses: a@900 at a presented frame, a@30s at seconds since launch",
    "LO_AUTO_PULSE": "Scripted presses: frames each press is held",
    "LO_SCREENSHOT_PATH": "Where a screenshot is saved (next to env.txt)",
    "LO_SCREENSHOT_REQUEST": "Take a screenshot (debug capture)",
    "LO_FPS": "Frame rate target (30, 60, ...) for this run",
    "LO_SAVE_DIR": "Save folder for this run: the player's own saves are never read or written",
    "LO_TEST_INPUT_FILE": "Controller input played from a file (tools/scenario.py)",
}


def describe(key: str) -> str:
    return DESCRIPTIONS.get(key) or f"{key}: see docs/SWITCH.md (Diagnostics switches)"


def is_setting(line: str) -> bool:
    return "=" in line and not line.lstrip().startswith("#") and bool(line.split("=", 1)[0].strip())


def commented(lines: list[str]) -> list[str]:
    """The lines with "# <description>" above each KEY=VALUE line that has no comment right above it."""
    out: list[str] = []
    for line in lines:
        if is_setting(line) and not (out and out[-1].lstrip().startswith("#")):
            out.append(f"# {describe(line.split('=', 1)[0].strip())}")
        out.append(line)
    return out


def without(lines: list[str], keys: set[str]) -> list[str]:
    """The lines without the settings of these keys, nor the comment lines right above them."""
    out: list[str] = []
    for line in lines:
        if is_setting(line) and line.split("=", 1)[0].strip() in keys:
            while out and out[-1].lstrip().startswith("#"):
                out.pop()
            continue
        out.append(line)
    return out
