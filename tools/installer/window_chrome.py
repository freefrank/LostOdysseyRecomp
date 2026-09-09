"""Small Windows/Tk window frame; keeps the native taskbar and sizing contract."""
import ctypes
from ctypes import wintypes
import sys
import tkinter as tk


def enable_dpi_awareness():
    """Call before creating Tk. A frozen executable may already set this."""
    if sys.platform != 'win32':
        return
    try:
        ctypes.windll.user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
    except (AttributeError, OSError):
        try:
            ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except (AttributeError, OSError):
            pass


def resize_hit(x, y, width, height, border, maximized=False):
    """Return a Windows hit target for signed client-relative coordinates."""
    if maximized:
        return 1  # HTCLIENT
    left, right = x < border, x >= width - border
    top, bottom = y < border, y >= height - border
    if top:
        return 13 if left else 14 if right else 12
    if bottom:
        return 16 if left else 17 if right else 15
    return 10 if left else 11 if right else 1


class WindowChrome:
    """Remove nonclient painting without turning the app into a tool window.

    The standard WS_CAPTION/WS_THICKFRAME and original window procedure stay in
    place. Windows continues to own resize, snap, minimize, Alt+F4 and taskbar
    activation. Other platforms retain their native frame as a safe fallback.
    """
    def __init__(self, root, close, *, background, foreground, muted, border,
                 title='LOST ODYSSEY RECOMP', on_dpi_changed=None):
        self.root = root
        self.close = close
        self.on_dpi_changed = on_dpi_changed
        self.hwnd = None
        self.old_proc = None
        self.native = False
        self.scale = max(0.75, root.winfo_fpixels('1i') / 96)
        self.bar = tk.Frame(root, bg=background, height=round(42 * self.scale), highlightthickness=0)
        self.bar.pack(fill='x')
        self.bar.pack_propagate(False)
        self.caption = tk.Label(self.bar, text=title, bg=background, fg=muted,
                                font=('Segoe UI Semibold', 9), padx=20)
        self.caption.pack(side='left', fill='y')
        self.buttons = []
        for text, command, label in (
                ('\u2014', self.minimize, 'Minimize'),
                ('\u25a1', self.toggle_maximize, 'Maximize or restore'),
                ('\u00d7', close, 'Close')):
            button = tk.Button(self.bar, text=text, command=command,
                               bg=background, fg=foreground, activebackground=border,
                               activeforeground=foreground, relief='flat', bd=0,
                               highlightthickness=1, highlightbackground=background,
                               highlightcolor=foreground, takefocus=True,
                               font=('Segoe UI', 13), width=4, cursor='hand2')
            self.buttons.append(button)
            button.bind('<Enter>', lambda _e, b=button: b.configure(bg=border))
            button.bind('<Leave>', lambda _e, b=button: b.configure(bg=background))
            button.bind('<Return>', lambda _e, action=command: action())
        for button in reversed(self.buttons):
            button.pack(side='right', fill='y')
        for widget in (self.bar, self.caption):
            widget.bind('<ButtonPress-1>', self.drag)
            widget.bind('<Double-Button-1>', lambda _e: self.toggle_maximize())
            widget.bind('<ButtonPress-3>', self.system_menu)
        root.bind('<Alt-F4>', lambda _e: self._close_key(), add='+')
        root.bind('<Alt-space>', self.system_menu, add='+')
        root.bind('<Destroy>', self._destroy, add='+')
        root.update_idletasks()
        if sys.platform == 'win32':
            self._install_windows()

    def _close_key(self):
        self.close()
        return 'break'

    def minimize(self):
        self.root.iconify()

    def toggle_maximize(self):
        try:
            self.root.state('normal' if self.root.state() == 'zoomed' else 'zoomed')
        except tk.TclError:
            pass

    def drag(self, _event):
        if self.native:
            self.user32.ReleaseCapture()
            self.user32.SendMessageW(self.hwnd, 0xA1, 2, 0)  # HTCAPTION
        return 'break'

    def system_menu(self, event=None):
        if self.native:
            menu = self.user32.GetSystemMenu(self.hwnd, False)
            x = getattr(event, 'x_root', self.root.winfo_rootx() + 20)
            y = getattr(event, 'y_root', self.root.winfo_rooty() + 40)
            command = self.user32.TrackPopupMenu(menu, 0x100 | 0x2, x, y, 0, self.hwnd, None)
            if command:
                # Route Close through Tk's WM_DELETE_WINDOW and cancellation.
                self.user32.PostMessageW(self.hwnd, 0x112, command, 0)
        return 'break'

    def _install_windows(self):
        self.user32 = ctypes.WinDLL('user32', use_last_error=True)
        u = self.user32
        pointer = ctypes.c_ssize_t
        self.proc_type = ctypes.WINFUNCTYPE(pointer, wintypes.HWND, wintypes.UINT,
                                          wintypes.WPARAM, wintypes.LPARAM)
        u.GetAncestor.argtypes = [wintypes.HWND, wintypes.UINT]
        u.GetAncestor.restype = wintypes.HWND
        u.SetWindowLongPtrW.argtypes = [wintypes.HWND, ctypes.c_int, pointer]
        u.SetWindowLongPtrW.restype = pointer
        u.CallWindowProcW.argtypes = [pointer, wintypes.HWND, wintypes.UINT,
                                     wintypes.WPARAM, wintypes.LPARAM]
        u.CallWindowProcW.restype = pointer
        u.SendMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
        u.SendMessageW.restype = pointer
        u.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
        u.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
        u.IsZoomed.argtypes = [wintypes.HWND]
        u.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int,
                                   ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT]
        u.GetSystemMenu.argtypes = [wintypes.HWND, wintypes.BOOL]
        u.GetSystemMenu.restype = wintypes.HMENU
        u.TrackPopupMenu.argtypes = [wintypes.HMENU, wintypes.UINT, ctypes.c_int,
                                     ctypes.c_int, ctypes.c_int, wintypes.HWND, ctypes.c_void_p]
        u.TrackPopupMenu.restype = wintypes.UINT
        u.MonitorFromWindow.argtypes = [wintypes.HWND, wintypes.DWORD]
        u.MonitorFromWindow.restype = wintypes.HANDLE
        u.GetMonitorInfoW.argtypes = [wintypes.HANDLE, ctypes.c_void_p]
        self.hwnd = u.GetAncestor(self.root.winfo_id(), 2)
        self.callback = self.proc_type(self._window_proc)  # Keep alive until detached.
        ctypes.set_last_error(0)
        self.old_proc = u.SetWindowLongPtrW(self.hwnd, -4, ctypes.cast(self.callback, ctypes.c_void_p).value)
        if not self.old_proc:
            # A native frame is preferable to a broken window if subclassing fails.
            self.hwnd = None
            return
        self.native = True
        u.SetWindowPos(self.hwnd, None, 0, 0, 0, 0, 0x1 | 0x2 | 0x4 | 0x10 | 0x20)

    def _window_proc(self, hwnd, message, wparam, lparam):
        try:
            if message == 0x83:  # WM_NCCALCSIZE: the entire window is client area.
                return 0
            if message == 0x84:  # WM_NCHITTEST: let Windows perform border sizing.
                rect = wintypes.RECT()
                self.user32.GetWindowRect(hwnd, ctypes.byref(rect))
                x = ctypes.c_short(lparam & 0xFFFF).value - rect.left
                y = ctypes.c_short((lparam >> 16) & 0xFFFF).value - rect.top
                return resize_hit(x, y, rect.right - rect.left, rect.bottom - rect.top,
                                  max(5, round(7 * self.scale)), self.user32.IsZoomed(hwnd))
            if message == 0x24 and lparam:  # WM_GETMINMAXINFO: keep maximize above the taskbar.
                result = self.user32.CallWindowProcW(self.old_proc, hwnd, message, wparam, lparam)
                class MonitorInfo(ctypes.Structure):
                    _fields_ = [('size', wintypes.DWORD), ('monitor', wintypes.RECT),
                                ('work', wintypes.RECT), ('flags', wintypes.DWORD)]
                class MinMaxInfo(ctypes.Structure):
                    _fields_ = [('reserved', wintypes.POINT), ('max_size', wintypes.POINT),
                                ('max_position', wintypes.POINT), ('min_track', wintypes.POINT),
                                ('max_track', wintypes.POINT)]
                monitor = MonitorInfo(size=ctypes.sizeof(MonitorInfo))
                if self.user32.GetMonitorInfoW(self.user32.MonitorFromWindow(hwnd, 2), ctypes.byref(monitor)):
                    info = ctypes.cast(lparam, ctypes.POINTER(MinMaxInfo)).contents
                    info.max_position.x = monitor.work.left - monitor.monitor.left
                    info.max_position.y = monitor.work.top - monitor.monitor.top
                    info.max_size.x = monitor.work.right - monitor.work.left
                    info.max_size.y = monitor.work.bottom - monitor.work.top
                return result
            if message == 0x2E0:  # WM_DPICHANGED: Tk handles placement; refresh UI metrics.
                dpi = wparam & 0xFFFF
                self.root.after_idle(lambda: self._dpi_changed(dpi))
        except Exception:
            # Exceptions must never unwind across the native callback boundary.
            pass
        return self.user32.CallWindowProcW(self.old_proc, hwnd, message, wparam, lparam)

    def _dpi_changed(self, dpi):
        if self.root.winfo_exists():
            self.scale = dpi / 96
            self.root.tk.call('tk', 'scaling', dpi / 72)
            self.bar.configure(height=round(42 * self.scale))
            if self.on_dpi_changed:
                self.on_dpi_changed()

    def _destroy(self, event):
        if event.widget is self.root and self.native and self.old_proc:
            self.user32.SetWindowLongPtrW(self.hwnd, -4, self.old_proc)
            self.native = False
