#!/usr/bin/env python3
"""Local TAA live controls; uses only the Python standard library."""
import argparse
import json
import math
import os
import re
from pathlib import Path
import struct
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit, parse_qs
import zlib

DEFAULTS = dict(aa=3, jitter=1, history=1, bloom=-1, hdr=0, materials=1,
    stationary=1, coverage=1, jitter_scale=1, gpu_timing=0, history_weight=.85, stationary_weight=31/33,
    motion_min=.002, motion_max=.125, depth_absolute=.00001,
    depth_relative=.01, acceptance=0, mv_debug=0, mv_consume=1, snap_stationary=0, stationary_color_clip=0, history_fp16=0, stationary_multi_surface=0, moving_bilinear_fallback=0)
ENUMS = dict(aa=(-1,0,1,2,3), jitter=(-1,0,1), bloom=(-1,0,1),
    acceptance=(0,1,2), **{k:(0,1) for k in ('history','hdr','materials',
    'stationary','coverage','mv_debug','mv_consume','snap_stationary','stationary_color_clip','history_fp16','stationary_multi_surface','moving_bilinear_fallback','gpu_timing')})
RANGES = dict(jitter_scale=(0,1), history_weight=(0,.95), stationary_weight=(0,.995),
    motion_min=(0,16), motion_max=(0,16), depth_absolute=(0,1), depth_relative=(0,1))
TARGETS = {name:0xffff0001+i for i,name in enumerate(
    ('source','output','depth','mv','motion_depths','reactive'))}

def validate(changes, current):
    if not isinstance(changes, dict) or set(changes)-set(DEFAULTS):
        raise ValueError('未知参数或参数不是对象')
    result = {**current, **changes}
    for key, value in result.items():
        if type(value) not in (int,float) or not math.isfinite(value):
            raise ValueError(key+' 必须为有限数值')
        if key in ENUMS and value not in ENUMS[key]:
            raise ValueError(key+' 超出选项范围')
        if key in ENUMS: result[key]=int(value)
        if key in RANGES and not RANGES[key][0] <= value <= RANGES[key][1]:
            raise ValueError(key+' 超出数值范围')
    if result['motion_max'] <= result['motion_min']:
        raise ValueError('motion_max 必须大于 motion_min')
    return result

def atomic_write(path, content):
    temporary = path.with_suffix(path.suffix+'.tmp')
    temporary.write_text(content, encoding='utf-8')
    os.replace(temporary, path)

def ppm(path):
    data = path.read_bytes()
    pos = 0
    tokens = []
    while len(tokens)<4:
        while pos<len(data) and data[pos] in b' \t\r\n': pos+=1
        if pos<len(data) and data[pos]==35:
            pos=data.index(b'\n',pos)+1
            continue
        start=pos
        while pos<len(data) and data[pos] not in b' \t\r\n': pos+=1
        if start==pos: raise ValueError('无效 PPM')
        tokens.append(data[start:pos])
    if tokens[0]!=b'P6' or tokens[3]!=b'255': raise ValueError('仅支持 P6 8-bit PPM')
    width,height=int(tokens[1]),int(tokens[2])
    if data[pos:pos+2]==b'\r\n': pos+=2
    else: pos+=1
    pixels=data[pos:]
    if width<=0 or height<=0 or len(pixels)!=width*height*3: raise ValueError('PPM 尚未写完或格式无效')
    return width,height,pixels

def png(width,height,pixels):
    def chunk(name,data):
        return struct.pack('!I',len(data))+name+data+struct.pack('!I',zlib.crc32(name+data)&0xffffffff)
    rows=b''.join(b'\0'+pixels[y*width*3:(y+1)*width*3] for y in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')

def trace_pixel(path,x,y):
    match=re.fullmatch(r'trace_f(\d+)_a([0-9a-fA-F]+)_n(\d+)_(\d+)x(\d+)_fmt(\d+)\.bin',path.name)
    if not match: raise ValueError('未知 trace 文件命名格式')
    frame,address,occurrence,width,height,fmt=match.groups()
    address=int(address,16);width=int(width);height=int(height)
    layouts={0xffff0001:('rgba8','4B',4),0xffff0002:('rgba8','4B',4),
        0xffff0003:('depth_float32','<f',4),0xffff0004:('motion_half2','<2e',4),
        0xffff0005:('motion_depths_float2','<2f',8),0xffff0006:('reactive_uint8','B',1)}
    if address not in layouts: raise ValueError('此 trace 地址没有已知像素协议')
    kind,format_string,bpp=layouts[address]
    if address in (0xffff0001,0xffff0002):
        color_formats={20:('rgba8','4B',4),10:('rgba16_float','<4e',8)}
        if int(fmt) not in color_formats: raise ValueError('source/output trace 颜色格式不受支持')
        kind,format_string,bpp=color_formats[int(fmt)]
    data=path.read_bytes()
    if width<=0 or height<=0 or len(data)!=width*height*bpp: raise ValueError('trace 长度与像素协议不一致或尚未写完')
    if not (0<=x<width and 0<=y<height): raise ValueError('像素坐标越界')
    values=list(struct.unpack_from(format_string,data,(y*width+x)*bpp))
    finite=[math.isfinite(v) for v in values]
    return dict(frame=int(frame),address=f'{address:08x}',occurrence=int(occurrence),
        width=width,height=height,format_id=int(fmt),x=x,y=y,type=kind,
        values=[v if f else None for v,f in zip(values,finite)],finite=all(finite),finite_channels=finite)

class App:
    def __init__(self, directory, game=None):
        self.directory=directory.resolve(); self.directory.mkdir(parents=True,exist_ok=True)
        self.controls=dict(DEFAULTS); self.serial=0; self.control_serial=0; self.lock=threading.Lock(); self.game=game
        self.launch_config=None
        self.control_serial=self.next_serial()
        atomic_write(self.directory/'control.txt','serial='+str(self.control_serial)+'\n'+''.join(f'{k}={v}\n' for k,v in self.controls.items()))
    def next_serial(self):
        self.serial=max(self.serial+1,time.time_ns()//1000000)
        return self.serial
    def launch_game(self, initial=False):
        if not self.launch_config or (not initial and self.game is None):
            raise ValueError('本面板未启动过游戏，请使用启动脚本')
        if self.game is not None and self.game.poll() is None:
            raise ValueError('游戏仍在运行，请先正常关闭游戏后再启动')
        executable,run_dir,env=self.launch_config
        self.game=subprocess.Popen([executable],cwd=run_dir,env=env.copy())
        return dict(pid=self.game.pid,status='running')
    def file(self,name):
        if Path(name).name!=name or not ((name.startswith('shot') and name.endswith('.ppm')) or (name.startswith('trace_') and name.endswith('.bin'))):
            raise ValueError('文件不在允许范围')
        path=(self.directory/name).resolve()
        if path.parent!=self.directory: raise ValueError('无效路径')
        return path
    def files(self):
        return sorted([dict(name=p.name,size=p.stat().st_size,modified=p.stat().st_mtime)
            for p in self.directory.iterdir() if p.is_file() and
            ((p.name.startswith('shot') and p.suffix=='.ppm') or (p.name.startswith('trace_') and p.suffix=='.bin'))],key=lambda x:x['modified'],reverse=True)

def handler(app):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self,*args): pass
        def reply(self,data,status=200,kind='application/json'):
            if kind=='application/json': data=json.dumps(data,allow_nan=False).encode()
            self.send_response(status); self.send_header('Content-Type',kind)
            self.send_header('Content-Length',str(len(data))); self.send_header('Cache-Control','no-store')
            self.send_header('X-Content-Type-Options','nosniff'); self.end_headers(); self.wfile.write(data)
        def safe_origin(self):
            host=self.headers.get('Host','')
            allowed={f'127.0.0.1:{self.server.server_port}',f'localhost:{self.server.server_port}'}
            if host not in allowed: raise ValueError('Host 不允许')
            origin=self.headers.get('Origin')
            if origin is not None and origin not in {'http://'+host}: raise ValueError('Origin 不允许')
        def do_GET(self):
            try:
                self.safe_origin()
                url=urlsplit(self.path); query=parse_qs(url.query)
                if url.path=='/': return self.reply(Path(__file__).with_suffix('.html').read_bytes(),kind='text/html; charset=utf-8')
                if url.path=='/api/state':
                    try: state=json.loads((app.directory/'state.json').read_text(encoding='utf-8-sig'))
                    except (OSError,ValueError): state=None
                    return self.reply(dict(service='lorecomp-taa-debug',can_relaunch=bool(app.launch_config and app.game is not None and app.game.poll() is not None),requested=app.controls,requested_serial=app.control_serial,state=state,
                        game=dict(pid=app.game.pid,status='running' if app.game.poll() is None else 'exited') if app.game else None))
                if url.path=='/api/files': return self.reply(app.files())
                path=app.file(query.get('file',[''])[0])
                if url.path=='/api/download': return self.reply(path.read_bytes(),kind='application/octet-stream')
                if url.path not in ('/api/preview','/api/pixel'): return self.reply({'error':'not found'},404)
                if url.path=='/api/pixel' and path.suffix=='.bin':
                    return self.reply(trace_pixel(path,int(query.get('x',[0])[0]),int(query.get('y',[0])[0])))
                width,height,pixels=ppm(path)
                x=int(query.get('x',[0])[0]); y=int(query.get('y',[0])[0])
                if url.path=='/api/pixel':
                    if not (0<=x<width and 0<=y<height): raise ValueError('像素坐标越界')
                    return self.reply(dict(x=x,y=y,width=width,height=height,type='rgb8',rgb=list(pixels[(y*width+x)*3:(y*width+x)*3+3])))
                w=int(query.get('w',[width-x])[0]); h=int(query.get('h',[height-y])[0])
                if not (0<=x<width and 0<=y<height and 0<w<=width-x and 0<h<=height-y): raise ValueError('ROI 越界')
                cropped=b''.join(pixels[((y+row)*width+x)*3:((y+row)*width+x+w)*3] for row in range(h))
                return self.reply(png(w,h,cropped),kind='image/png')
            except (ValueError,OSError,KeyError) as error: self.reply({'error':str(error)},400)
        def do_POST(self):
            try:
                self.safe_origin()
                length=int(self.headers.get('Content-Length','0'))
                if not 0<length<=16384: raise ValueError('请求长度无效')
                body=json.loads(self.rfile.read(length))
                if not isinstance(body,dict): raise ValueError('请求必须是对象')
                with app.lock:
                    route=urlsplit(self.path).path
                    if route=='/api/launch':
                        if body: raise ValueError('启动接口不接受参数，复用本面板原启动配置')
                        return self.reply(dict(game=app.launch_game()))
                    elif route=='/api/controls':
                        values=validate(body,app.controls); serial=app.next_serial()
                        atomic_write(app.directory/'control.txt','serial='+str(serial)+'\n'+''.join(f'{k}={v}\n' for k,v in values.items()))
                        app.controls=values; app.control_serial=serial
                    elif route in ('/api/screenshot','/api/trace'):
                        frames=body.get('frames',1); limit=32 if route.endswith('screenshot') else 8
                        if type(frames)!=int or not 1<=frames<=limit: raise ValueError('帧数超出范围')
                        if route.endswith('screenshot'):
                            serial=app.next_serial(); atomic_write(app.directory/'shots.txt',f'{serial} {frames}\n')
                        else:
                            targets=body.get('targets',['source','output'])
                            if not isinstance(targets,list) or not 1<=len(targets)<=4 or any(type(t)!=str or t not in TARGETS for t in targets) or len(set(targets))!=len(targets): raise ValueError('请选择 1–4 个不同 trace 目标')
                            serial=app.next_serial(); atomic_write(app.directory/'trace.txt',f'{serial} {frames} '+ ' '.join(f'{TARGETS[t]:08x} 1' for t in targets)+'\n')
                    else: return self.reply({'error':'not found'},404)
                self.reply(dict(serial=serial,files=app.files()))
            except (ValueError,OSError,TypeError) as error: self.reply({'error':str(error)},400)
    return Handler

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--control-dir',type=Path,default=Path(__file__).resolve().parents[1]/'out'/'taa-live')
    parser.add_argument('--run-dir',type=Path); parser.add_argument('--port',type=int,default=8769)
    parser.add_argument('--exe',default='LostOdysseyRecomp.exe',help='游戏可执行文件名或完整路径')
    parser.add_argument('--launch',action='store_true')
    continuation=parser.add_mutually_exclusive_group()
    continuation.add_argument('--auto-continue',action='store_true',help='自动菜单按键序列，并非直接加载存档')
    continuation.add_argument('--native-continue',action='store_true',help='初始化后由原生 Continue 读取最新存档，不发送菜单按键')
    args=parser.parse_args(); app=App(args.control_dir)
    server=ThreadingHTTPServer(('127.0.0.1',args.port),handler(app))
    if args.launch:
        if not args.run_dir: parser.error('--launch 需要 --run-dir')
        executable=Path(args.exe)
        if not executable.is_absolute(): executable=args.run_dir/executable
        if not executable.is_file(): parser.error('找不到 --exe 指定的游戏程序')
        env=os.environ.copy(); env.update(LO_TAA_LIVE_DIR=str(app.directory),LO_SCREENSHOT_REQUEST=str(app.directory/'shots.txt'),LO_SCREENSHOT_PATH=str(app.directory/'shot.ppm'),LO_RESOLVE_TRACE_REQUEST=str(app.directory/'trace.txt'),LO_GRAPHICS_API='vulkan',LO_MV_ENABLE='1',LO_MV_REPLAY='1',LO_MV_CONSUME='1')
        for key in ('LO_DEBUG_AUTO_CONTINUE','LO_AUTO_BUTTONS','LO_AUTO_PULSE'):
            env.pop(key,None)
        if args.auto_continue:
            env['LO_AUTO_BUTTONS']='s@120,a@240,a@360,a@480,a@700,a@900'
            env['LO_AUTO_PULSE']='6'
        elif args.native_continue:
            env['LO_DEBUG_AUTO_CONTINUE']='1'
        app.launch_config=(str(executable.resolve()),args.run_dir.resolve(),env)
        app.launch_game(initial=True)
    print(f'TAA debug: http://127.0.0.1:{args.port}',flush=True)
    try: server.serve_forever()
    except KeyboardInterrupt: pass
    finally: server.server_close()

if __name__=='__main__': main()
