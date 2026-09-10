"""Direct installer UI checks. Native windows remain offscreen and never activate."""
import ctypes
from ctypes import wintypes
from pathlib import Path
import sys
import tkinter as tk
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'installer'))
import installer
from window_chrome import WindowChrome, resize_hit


class ResizeTargets(unittest.TestCase):
    def test_edges_corners_and_client(self):
        for point, expected in [((0, 0), 13), ((799, 0), 14), ((0, 599), 16),
                                ((799, 599), 17), ((0, 250), 10), ((799, 250), 11),
                                ((400, 0), 12), ((400, 599), 15), ((400, 250), 1)]:
            self.assertEqual(resize_hit(*point, 800, 600, 7), expected)
        self.assertEqual(resize_hit(-2, -2, 800, 600, 7, maximized=True), 1)


@unittest.skipUnless(sys.platform == 'win32', 'Windows window lifecycle')
class InstallerUI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        installer.enable_dpi_awareness()
        cls.user32 = ctypes.windll.user32
        cls.user32.GetForegroundWindow.restype = wintypes.HWND
        cls.user32.ShowWindow.argtypes = [wintypes.HWND, ctypes.c_int]
        cls.user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]

    def setUp(self):
        self.foreground = self.user32.GetForegroundWindow()
        self.root = tk.Tk()
        self.root.withdraw()
        self.app = installer.Installer(self.root)
        self.root.geometry(f'{round(920*self.app.scale)}x{round(790*self.app.scale)}-30000-30000')
        self.root.update_idletasks()
        self.user32.ShowWindow(self.app.chrome.hwnd, 4)  # SW_SHOWNOACTIVATE
        self.root.update()
        self.assertEqual(self.foreground, self.user32.GetForegroundWindow())
        self.assertTrue(self.app.chrome.native)
        rect = wintypes.RECT()
        self.user32.GetWindowRect(self.app.chrome.hwnd, ctypes.byref(rect))
        self.assertLess(rect.right, 0)
        self.assertLess(rect.bottom, 0)

    def tearDown(self):
        if self.root.winfo_exists():
            self.app.destroy()
        self.assertEqual(self.foreground, self.user32.GetForegroundWindow())

    def test_narrow_layout_keeps_actions_inside_window_and_scrolls_table(self):
        minimum = self.root.minsize()
        self.root.geometry(f'{minimum[0]}x{minimum[1]}-30000-30000')
        self.root.update()
        left, top = self.root.winfo_rootx(), self.root.winfo_rooty()
        right, bottom = left+self.root.winfo_width(), top+self.root.winfo_height()
        for widget in self.app.controls + [self.app.start, self.app.stop, self.app.paths_button,
                                            self.app.tree_x, self.app.tree_y]:
            self.assertGreaterEqual(widget.winfo_rootx(), left)
            self.assertGreaterEqual(widget.winfo_rooty(), top)
            self.assertLessEqual(widget.winfo_rootx()+widget.winfo_width(), right)
            self.assertLessEqual(widget.winfo_rooty()+widget.winfo_height(), bottom)
        self.assertGreater(self.app.discs.winfo_height(), 40)
        for n in range(30):
            self.app.discs.insert('', 'end', values=('DLC', f'Package {n}', 'STFS', 'F'*40, 10, '1 MiB'))
        self.root.update_idletasks()
        self.assertLess(self.app.discs.yview()[1], 1)
        self.assertLess(self.app.discs.xview()[1], 1)

    def test_long_paths_remain_available_after_visual_ellipsis(self):
        path = 'D:/' + 'Very long folder name/'*15 + 'disc.iso'
        self.app.source.set(path)
        self.app.update_path_preview(100)
        self.assertTrue(self.app.source_preview.cget('text').startswith('…'))
        self.assertEqual(self.app.source.get(), path)
        with patch.object(installer.messagebox, 'showinfo') as show:
            self.app.show_paths()
        self.assertIn(path, show.call_args.args[1])

    def test_busy_cancel_and_retry_state(self):
        self.app.set_busy('scan')
        self.app.latest = (0, 0, 'Checking')
        self.app.poll()
        self.assertEqual(str(self.app.bar.cget('mode')), 'indeterminate')
        self.assertEqual(str(self.app.start.cget('state')), 'disabled')
        self.app.cancel_operation()
        self.assertTrue(self.app.cancel.is_set())
        self.assertEqual(str(self.app.stop.cget('state')), 'disabled')
        self.app.events.put(('cancelled', 'Cancelled'))
        with patch.object(installer.messagebox, 'showinfo'):
            self.app.poll()
        self.assertFalse(self.app.busy)
        self.assertEqual(self.app.phase, 'cancel')
        self.assertEqual(str(self.app.check.cget('state')), 'normal')

    def test_close_waits_for_worker_completion_without_dialog(self):
        self.app.set_busy('scan')
        self.app.close()
        self.assertTrue(self.app.closing)
        self.assertTrue(self.app.cancel.is_set())
        self.assertTrue(self.root.winfo_exists())
        self.app.events.put(('cancelled', 'Cancelled'))
        with patch.object(installer.messagebox, 'showinfo') as show, patch.object(self.app, 'destroy') as destroy:
            self.app.poll()
        destroy.assert_called_once()
        show.assert_not_called()

    def test_native_client_hit_test_and_caption_styles_are_retained(self):
        u = self.app.chrome.user32
        u.GetWindowLongPtrW.argtypes = [wintypes.HWND, ctypes.c_int]
        u.GetWindowLongPtrW.restype = ctypes.c_ssize_t
        style = u.GetWindowLongPtrW(self.app.chrome.hwnd, -16)
        self.assertEqual(style & 0x00CF0000, 0x00CF0000)  # caption, size, min/max, system menu
        rect = wintypes.RECT()
        u.GetWindowRect(self.app.chrome.hwnd, ctypes.byref(rect))
        for x, y, expected in [(rect.left+1, rect.top+1, 13), (rect.right-1, rect.bottom-1, 17),
                                (rect.left+100, rect.top+50, 1)]:
            packed = (x & 0xFFFF) | ((y & 0xFFFF) << 16)
            self.assertEqual(u.SendMessageW(self.app.chrome.hwnd, 0x84, 0, packed), expected)

    def test_unchanged_poll_does_not_invalidate_status_or_progress(self):
        self.app.poll()
        progress_writes = []
        original = type(self.app.bar).__setitem__
        def changed(widget, key, value):
            progress_writes.append((key, value))
            return original(widget, key, value)
        with patch.object(self.app.status, 'configure', wraps=self.app.status.configure) as status, \
                patch.object(type(self.app.bar), '__setitem__', changed):
            self.app.poll()
            self.app.poll()
        status.assert_not_called()
        self.assertEqual(progress_writes, [])

    def test_dpi_change_updates_minimum_and_table_metrics(self):
        self.app.chrome._dpi_changed(120)
        self.root.update_idletasks()
        self.assertEqual(self.app.scale, 1.25)
        self.assertEqual(self.root.minsize(), (800, 825))
        self.assertEqual(self.app.discs.column('disc', 'minwidth'), round(58 * 1.25))

    def test_minimize_uses_native_iconic_state_without_activation(self):
        self.user32.IsIconic.argtypes = [wintypes.HWND]
        self.app.chrome.minimize()
        self.root.update()
        self.assertTrue(self.user32.IsIconic(self.app.chrome.hwnd))


@unittest.skipUnless(sys.platform == 'win32', 'Windows message queue')
class DragDispatch(unittest.TestCase):
    def test_drag_is_queued_with_signed_screen_position(self):
        # A message-only HWND cannot activate or display a desktop window.
        u = ctypes.WinDLL('user32', use_last_error=True)
        u.CreateWindowExW.argtypes = [wintypes.DWORD, wintypes.LPCWSTR, wintypes.LPCWSTR,
                                     wintypes.DWORD, ctypes.c_int, ctypes.c_int,
                                     ctypes.c_int, ctypes.c_int, wintypes.HWND,
                                     wintypes.HMENU, wintypes.HINSTANCE, ctypes.c_void_p]
        u.CreateWindowExW.restype = wintypes.HWND
        u.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
        u.PeekMessageW.argtypes = [ctypes.POINTER(wintypes.MSG), wintypes.HWND,
                                  wintypes.UINT, wintypes.UINT, wintypes.UINT]
        u.DestroyWindow.argtypes = [wintypes.HWND]
        hwnd = u.CreateWindowExW(0, 'STATIC', '', 0, 0, 0, 0, 0, -3, None, None, None)
        self.assertTrue(hwnd)
        chrome = WindowChrome.__new__(WindowChrome)
        chrome.native, chrome.hwnd, chrome.user32 = True, hwnd, u
        try:
            self.assertEqual(chrome.drag(SimpleNamespace(x_root=-1234, y_root=456)), 'break')
            message = wintypes.MSG()
            self.assertTrue(u.PeekMessageW(ctypes.byref(message), hwnd, 0xA1, 0xA1, 1),
                            'Move must remain queued until the Tk binding returns')
            self.assertEqual(message.wParam, 2)
            self.assertEqual(ctypes.c_short(message.lParam & 0xFFFF).value, -1234)
            self.assertEqual(ctypes.c_short((message.lParam >> 16) & 0xFFFF).value, 456)
        finally:
            u.DestroyWindow(hwnd)


if __name__ == '__main__':
    unittest.main()
