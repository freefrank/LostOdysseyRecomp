"""Operate only this task's isolated game and its supported command files."""
from pathlib import Path
from ctypes import wintypes
import argparse,ctypes,hashlib,json,os,shutil,subprocess,time
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[2]
p=argparse.ArgumentParser()
p.add_argument('action',choices=['start','status','input','teleport','save-enable','shot','stop','mapjump'])
p.add_argument('values',nargs='*')
p.add_argument('--run',default='run-01')
p.add_argument('--seed',type=Path,default=HERE/'seed-uhra-native')
p.add_argument('--wait',type=float,default=1)
p.add_argument('--kernel-trace',action='store_true')
p.add_argument('--exe',type=Path)
p.add_argument('--exe-sha256')
p.add_argument('--cache',type=Path,help='Reuse only an already isolated cache owned by this task')
p.add_argument('--mapjump-support',action='store_true',help='Enable the task-specific one-shot diagnostic bridge')
a=p.parse_args();d=HERE/a.run;statefile=d/'session.json'
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE
kernel.CloseHandle.argtypes=[wintypes.HANDLE]
kernel.QueryFullProcessImageNameW.argtypes=[wintypes.HANDLE,wintypes.DWORD,wintypes.LPWSTR,ctypes.POINTER(wintypes.DWORD)]
kernel.GetProcessTimes.argtypes=[wintypes.HANDLE,*([ctypes.POINTER(wintypes.FILETIME)]*4)]
kernel.GetExitCodeProcess.argtypes=[wintypes.HANDLE,ctypes.POINTER(wintypes.DWORD)]
kernel.TerminateProcess.argtypes=[wintypes.HANDLE,wintypes.UINT]
def identity(handle):
    path=ctypes.create_unicode_buffer(32768);size=wintypes.DWORD(len(path))
    if not kernel.QueryFullProcessImageNameW(handle,0,path,ctypes.byref(size)):raise ctypes.WinError()
    times=[wintypes.FILETIME() for _ in range(4)]
    if not kernel.GetProcessTimes(handle,*(ctypes.byref(v) for v in times)):raise ctypes.WinError()
    return dict(exe=path.value,creation=(times[0].dwHighDateTime<<32)|times[0].dwLowDateTime)
def inventory(seed):
    return {str(f.relative_to(seed)):sha(f) for name in ('save','profile') for f in (seed/name).rglob('*') if f.is_file()}
def save():statefile.write_text(json.dumps(state,indent=2),encoding='utf-8')
def command(name,text):
    state['serial']+=1
    pending=d/(name+'.pending');pending.write_text(str(state['serial'])+' '+text+'\n',encoding='ascii')
    for attempt in range(60):
        try:os.replace(pending,d/name);break
        except PermissionError:
            if attempt==59:raise
            time.sleep(.02)
    save()
if a.action=='start':
    d.mkdir(exist_ok=False)
    exe=a.exe.resolve() if a.exe else HERE/'official-v0.4.2/LostOdysseyRecomp.exe'
    expected=a.exe_sha256 if a.exe else '13f1294bbb54efbf9a712a066441cb019ba5497cf1242bde6905e6c8bc1757b6'
    assert expected and sha(exe)==expected, 'Executable identity must be explicit and verified'
    seed=a.seed.resolve();initial=inventory(seed)
    shutil.copytree(seed/'save',d/'save');shutil.copytree(seed/'profile',d/'profile')
    if seed==ROOT/'out/audio-diagnostics-dialogue':
        # Copy only the chosen intact native container; other copied slots are
        # task-owned disposable duplicates, never source or user save data.
        for slot in (d/'save').iterdir():
            if slot.name!='user03':
                assert slot.resolve().is_relative_to(d.resolve())
                shutil.rmtree(slot)
    cache=a.cache.resolve() if a.cache else d/'shader-cache'
    if a.cache:
        assert cache.is_relative_to(HERE.resolve()) and cache.is_dir(), 'Only this task owned cache may be reused'
    else:
        cache.mkdir()
        for source in [ROOT/'out/issue7-map22-save/semantics-reload-01/shader-cache',ROOT/'out/battle-taa-fix/field-fixed-aa3/shader-cache']:
            if source.resolve()==cache.resolve():continue
            if source.is_dir():shutil.copytree(source,cache,dirs_exist_ok=True);break
    (d/'settings.ini').write_text('ui_language=1\ndebug_language=0\ngame_language=1\nwidth=1280\nheight=720\nwindow_mode=0\ninternal_resolution=720\nantialiasing=1\nscaling_quality=0\nframe_rate=30\n',encoding='ascii')
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('LO_')}
    env.update(LO_BACKGROUND='1',LO_AUDIO_MUTE='1',LO_TRACE_MAP_INFO='1',LO_LOG_FILE=str(d/'runtime.log'),LO_SHADER_CACHE_DIR=str(cache),
        LO_TEST_INPUT_FILE=str(d/'input.txt'),LO_TEST_INPUT_TICKS='1',LO_TELEPORT_COMMAND_FILE=str(d/'teleport.txt'),
        LO_SAVE_ANYWHERE_REQUEST=str(d/'save-enable.txt'),LO_SCREENSHOT_REQUEST=str(d/'shots.txt'),
        LO_SCREENSHOT_PATH=str(d/'shot.ppm'),LO_SCREENSHOT_PRESENTED='1')
    if a.mapjump_support:env['LO_ISSUE12_MAPJUMP_COMMAND_FILE']=str(d/'mapjump.txt')
    env.update({k:os.environ[k] for k in ('LO_ISSUE12_PROBE_FILE','LO_ISSUE12_SKIP_STALE','LO_ISSUE12_RT_DELAY_US','LO_ISSUE12_RT_DELAY_ARMED_US','LO_ISSUE12_ARM_ON_SWAP','LO_ISSUE12_ARM_SECONDS','LO_GC_RENDER_FLUSH','LO_TRACE_THREADS') if k in os.environ})
    with (d/'console.log').open('wb') as console:
        args=[str(exe),'--game',str(ROOT/'LostOdysseyRecompLib/private/disc1')]
        if not a.kernel_trace:args.append('--quiet-kernel')
        child=subprocess.Popen(args,cwd=d,env=env,
            stdout=console,stderr=console,creationflags=subprocess.CREATE_NO_WINDOW)
    state=dict(pid=child.pid,**identity(int(child._handle)),serial=0,seed=str(seed),seed_hashes=initial,exe_sha256=sha(exe),
        environment={k:v for k,v in env.items() if k.startswith('LO_')})
    save();print(json.dumps(dict(pid=state['pid'],exe=state['exe'],run=str(d))))
else:
    state=json.loads(statefile.read_text())
    handle=kernel.OpenProcess(0x1000|0x0001,False,state['pid'])
    if not handle:raise RuntimeError('Owned process no longer available')
    try:
        current=identity(handle)
        if current!={k:state[k] for k in ('exe','creation')}:raise RuntimeError('Owned process identity changed')
        code=wintypes.DWORD();kernel.GetExitCodeProcess(handle,ctypes.byref(code))
        if code.value!=259:raise RuntimeError(f'Owned game exited {code.value:08X}')
        if a.action=='input':command('input.txt',' '.join(a.values))
        elif a.action=='teleport':command('teleport.txt',' '.join(a.values))
        elif a.action=='save-enable':command('save-enable.txt',' '.join(a.values))
        elif a.action=='shot':command('shots.txt','1')
        elif a.action=='mapjump':
            assert len(a.values)==1 and a.values[0] in ['z0g_9_scrw','xxx_x_scrw']
            assert not state.get('mapjump_sent') and state['environment'].get('LO_ISSUE12_MAPJUMP_COMMAND_FILE')==str(d/'mapjump.txt')
            pending=d/'mapjump.pending';pending.write_text(a.values[0]+'\n',encoding='ascii');os.replace(pending,d/'mapjump.txt')
            state['mapjump_sent']=a.values[0];save()
        elif a.action=='stop':
            kernel.TerminateProcess(handle,1);state['owned_process_stopped']=True;state['seed_unchanged']=inventory(Path(state['seed']))==state['seed_hashes'];state['exe_unchanged']=sha(Path(state['exe']))==state['exe_sha256'];save()
        time.sleep(min(a.wait,10))
        if (d/'runtime.log').exists():
            lines=(d/'runtime.log').read_text(encoding='utf-8',errors='strict').splitlines()
            if a.action=='status':
                relevant=[s for s in lines if any(k in s for k in ('current map','teleport status','[crash]','starting guest','shader preparation'))]
                print('\n'.join(relevant[-8:]))
            print('\n'.join(lines[-8:]))
        if a.action=='shot':
            from PIL import Image
            shot=max(d.glob('shot_*.ppm'),key=lambda f:f.stat().st_mtime)
            output=shot.with_suffix('.png');Image.open(shot).save(output);print('SCREENSHOT',output)
    finally:kernel.CloseHandle(handle)
