"""Exercise live-control protocol with a temporary directory, without a game."""
import importlib.util
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch
from urllib.request import Request, urlopen
from urllib.error import HTTPError

spec=importlib.util.spec_from_file_location('taa_debug',Path(__file__).resolve().parents[1]/'taa_debug.py')
panel=importlib.util.module_from_spec(spec);spec.loader.exec_module(panel)

class ProtocolTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.app=panel.App(self.root)
        self.server=panel.ThreadingHTTPServer(('127.0.0.1',0),panel.handler(self.app))
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
        self.url=f'http://127.0.0.1:{self.server.server_port}'
    def tearDown(self):
        self.server.shutdown();self.server.server_close();self.thread.join();self.temp.cleanup()
    def request(self,path,body=None,headers=None):
        req=Request(self.url+path,data=json.dumps(body).encode() if body is not None else None,headers=headers or {})
        with urlopen(req) as response:return response.read()
    def test_controls_and_independent_capture_serial(self):
        response=json.loads(self.request('/api/controls',{'history_weight':.6,'snap_stationary':1}))
        text=(self.root/'control.txt').read_text()
        self.assertIn('history_weight=0.6\n',text);self.assertIn('serial='+str(response['serial']),text)
        self.assertIn('snap_stationary=1\n',text)
        self.request('/api/screenshot',{'frames':32})
        self.assertEqual(json.loads(self.request('/api/state'))['requested_serial'],response['serial'])
        self.assertEqual((self.root/'shots.txt').read_text().split()[1],'32')
        self.request('/api/trace',{'frames':2,'targets':['source','reactive']})
        self.assertTrue((self.root/'trace.txt').read_text().endswith('2 ffff0001 1 ffff0006 1\n'))
    def test_reject_invalid_and_cross_origin(self):
        original=(self.root/'control.txt').read_text()
        for body in ({'history_weight':float('nan')},{'motion_min':1,'motion_max':.5},{'aa':8},{'unknown':1}):
            with self.assertRaises(HTTPError):self.request('/api/controls',body)
        with self.assertRaises(HTTPError):self.request('/api/controls',{}, {'Origin':'https://example.com'})
        with self.assertRaises(HTTPError):self.request('/api/screenshot',{'frames':33})
        with self.assertRaises(HTTPError):self.request('/api/trace',{'targets':['source']*2})
        self.assertEqual((self.root/'control.txt').read_text(),original)
    def test_startup_normalization_and_trace_pixels(self):
        self.assertGreater(self.app.control_serial,0)
        self.assertIn('snap_stationary=0\n',(self.root/'control.txt').read_text())
        self.request('/api/controls',{'aa':1.0})
        self.assertIn('aa=1\n',(self.root/'control.txt').read_text())
        cases=[('ffff0001','4B',[12,34,56,78],'rgba8'),
            ('ffff0002','4B',[90,80,70,60],'rgba8'),
            ('ffff0003','<f',[.25],'depth_float32'),
            ('ffff0004','<2e',[.5,-.25],'motion_half2'),
            ('ffff0005','<2f',[1.,float('inf')],'motion_depths_float2'),
            ('ffff0006','B',[255],'reactive_uint8')]
        for address,fmt,values,kind in cases:
            format_id=20 if address in ('ffff0001','ffff0002') else 99
            name=f'trace_f42_a{address}_n1_1x1_fmt{format_id}.bin'
            (self.root/name).write_bytes(panel.struct.pack(fmt,*values))
            result=json.loads(self.request('/api/pixel?file='+name))
            self.assertEqual((result['frame'],result['width'],result['height'],result['type']),(42,1,1,kind))
            self.assertEqual(result['values'],[v if panel.math.isfinite(v) else None for v in values])
            self.assertEqual(result['finite'],all(panel.math.isfinite(v) for v in values))
        invalid='trace_f43_affff0003_n1_2x1_fmt99.bin'
        (self.root/invalid).write_bytes(b'\0'*4)
        with self.assertRaises(HTTPError):self.request('/api/pixel?file='+invalid)
        unknown='trace_f43_a12345678_n1_1x1_fmt99.bin'
        (self.root/unknown).write_bytes(b'\0'*4)
        with self.assertRaises(HTTPError):self.request('/api/pixel?file='+unknown)
    def test_relaunch_only_owned_exited_game(self):
        with self.assertRaises(HTTPError):self.request('/api/launch',{})
        self.app.launch_config=('debug.exe',self.root,{'LO_DEBUG_AUTO_CONTINUE':'1'})
        self.app.game=Mock(pid=100)
        self.app.game.poll.return_value=None
        with patch.object(panel.subprocess,'Popen') as spawn:
            with self.assertRaises(HTTPError):self.request('/api/launch',{})
            spawn.assert_not_called()
            self.app.game.poll.return_value=0
            state=json.loads(self.request('/api/state'))
            self.assertEqual(state['service'],'lorecomp-taa-debug')
            self.assertTrue(state['can_relaunch'])
            replacement=Mock(pid=101);replacement.poll.return_value=None
            spawn.return_value=replacement
            result=json.loads(self.request('/api/launch',{}))
            self.assertEqual(result['game']['pid'],101)
            spawn.assert_called_once_with(['debug.exe'],cwd=self.root,env={'LO_DEBUG_AUTO_CONTINUE':'1'})
            self.assertFalse(json.loads(self.request('/api/state'))['can_relaunch'])
    def test_stationary_color_clip_parameter(self):
        self.assertEqual(self.app.controls['stationary_color_clip'],0)
        self.request('/api/controls',{'stationary_color_clip':1.0})
        self.assertIn('stationary_color_clip=1\n',(self.root/'control.txt').read_text())
        with self.assertRaises(HTTPError):self.request('/api/controls',{'stationary_color_clip':2})
    def test_stationary_multi_surface_parameter(self):
        self.assertEqual(self.app.controls['stationary_multi_surface'],0)
        self.request('/api/controls',{'stationary_multi_surface':1.0})
        self.assertIn('stationary_multi_surface=1\n',(self.root/'control.txt').read_text())
        with self.assertRaises(HTTPError):self.request('/api/controls',{'stationary_multi_surface':2})
    def test_moving_bilinear_fallback_parameter(self):
        self.assertEqual(self.app.controls['moving_bilinear_fallback'],0)
        self.request('/api/controls',{'moving_bilinear_fallback':1.0})
        self.assertIn('moving_bilinear_fallback=1\n',(self.root/'control.txt').read_text())
        with self.assertRaises(HTTPError):self.request('/api/controls',{'moving_bilinear_fallback':2})
    def test_jitter_scale_and_gpu_timing_controls(self):
        self.request('/api/controls',{'jitter_scale':.5,'gpu_timing':1})
        control=(self.root/'control.txt').read_text()
        self.assertIn('jitter_scale=0.5\n',control)
        self.assertIn('gpu_timing=1\n',control)
        for changes in ({'jitter_scale':1.1},{'gpu_timing':2}):
            with self.assertRaises(HTTPError):self.request('/api/controls',changes)
        self.assertEqual((self.root/'control.txt').read_text(),control)
    def test_stationary_weight_diagnostic_range(self):
        self.request('/api/controls',{'stationary_weight':.995})
        original=(self.root/'control.txt').read_text()
        for changes in ({'stationary_weight':.996},{'history_weight':.996}):
            with self.assertRaises(HTTPError):self.request('/api/controls',changes)
        self.assertEqual((self.root/'control.txt').read_text(),original)
    def test_history_fp16_parameter_and_color_trace(self):
        self.assertEqual(self.app.controls['history_fp16'],0)
        self.request('/api/controls',{'history_fp16':1.0})
        self.assertIn('history_fp16=1\n',(self.root/'control.txt').read_text())
        with self.assertRaises(HTTPError):self.request('/api/controls',{'history_fp16':2})
        for address in ('ffff0001','ffff0002'):
            for fmt,packing,values,kind in [(20,'4B',[1,2,3,255],'rgba8'),(10,'<4e',[.125,.5,2.,float('inf')],'rgba16_float')]:
                name=f'trace_f44_a{address}_n1_1x1_fmt{fmt}.bin'
                (self.root/name).write_bytes(panel.struct.pack(packing,*values))
                result=json.loads(self.request('/api/pixel?file='+name))
                self.assertEqual(result['type'],kind)
                self.assertEqual(result['values'],[v if panel.math.isfinite(v) else None for v in values])
                (self.root/name).write_bytes(b'\0'*(4 if fmt==10 else 8))
                with self.assertRaises(HTTPError):self.request('/api/pixel?file='+name)
            unsupported=f'trace_f44_a{address}_n1_1x1_fmt11.bin'
            (self.root/unsupported).write_bytes(b'\0'*8)
            with self.assertRaises(HTTPError):self.request('/api/pixel?file='+unsupported)
    def test_ppm_crop_pixel_and_file_boundary(self):
        (self.root/'shot_1.ppm').write_bytes(b'P6\n2 1\n255\n'+bytes([10,20,30,40,50,60]))
        pixel=json.loads(self.request('/api/pixel?file=shot_1.ppm&x=1&y=0'))
        self.assertEqual(pixel['rgb'],[40,50,60])
        image=self.request('/api/preview?file=shot_1.ppm&x=1&y=0&w=1&h=1')
        self.assertEqual(image[:8],b'\x89PNG\r\n\x1a\n')
        self.assertEqual(panel.struct.unpack('!II',image[16:24]),(1,1))
        with self.assertRaises(HTTPError):self.request('/api/download?file=../secret.ppm')
        with self.assertRaises(HTTPError):self.request('/api/preview?file=shot_1.ppm&x=3')

if __name__=='__main__':unittest.main()
