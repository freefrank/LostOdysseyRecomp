"""Lost Odyssey portable game importer. Frozen into InstallGame.exe for releases."""
from pathlib import Path
import queue
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from import_game import install, Cancelled


class Installer:
    def __init__(self, root):
        self.root = root
        root.title('Lost Odyssey — Import game')
        root.minsize(680, 330)
        self.events = queue.Queue()
        self.cancel = threading.Event()
        self.busy = False
        self.latest = (0, 0, 'Select your game files to begin.')
        home = Path(sys.executable if getattr(sys, 'frozen', False) else __file__).resolve().parent
        self.home = home
        self.source = tk.StringVar()
        self.destination = tk.StringVar(value=str(home / 'game'))
        if (home / 'game-path.txt').is_file():
            self.destination.set((home / 'game-path.txt').read_text(encoding='utf-8').strip())
        frame = ttk.Frame(root, padding=24)
        frame.pack(fill='both', expand=True)
        frame.columnconfigure(0, weight=1)
        ttk.Label(frame, text='Import Lost Odyssey', font=('Segoe UI', 20)).grid(row=0, column=0, sticky='w')
        ttk.Label(frame, text='Select an ISO, extracted disc, default.xex, or GOD folder.\n'
                  'Your original files are kept. Additional discs can be imported later.').grid(
                      row=1, column=0, columnspan=3, sticky='w', pady=(8, 18))
        self.controls = []
        for row, label, variable in ((2, 'Game source', self.source), (4, 'Game destination', self.destination)):
            ttk.Label(frame, text=label).grid(row=row, column=0, sticky='w')
            entry = ttk.Entry(frame, textvariable=variable)
            entry.grid(row=row + 1, column=0, sticky='ew', pady=(4, 12))
            button = ttk.Button(frame, text='Folder…', command=lambda v=variable: self.folder(v))
            button.grid(row=row + 1, column=1, padx=8)
            self.controls.extend((entry, button))
        button = ttk.Button(frame, text='File…', command=self.file)
        button.grid(row=3, column=2)
        self.controls.append(button)
        self.bar = ttk.Progressbar(frame, maximum=100)
        self.bar.grid(row=6, column=0, columnspan=3, sticky='ew', pady=(8, 8))
        self.status = ttk.Label(frame, text=self.latest[2], wraplength=620)
        self.status.grid(row=7, column=0, columnspan=3, sticky='w')
        self.start = ttk.Button(frame, text='Import game', command=self.run)
        self.start.grid(row=8, column=2, pady=(20, 0))
        self.controls.append(self.start)
        self.stop = ttk.Button(frame, text='Cancel', command=self.cancel.set, state='disabled')
        self.stop.grid(row=8, column=1, padx=8, pady=(20, 0))
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(100, self.poll)

    def folder(self, variable):
        value = filedialog.askdirectory(parent=self.root)
        if value:
            variable.set(value)

    def file(self):
        value = filedialog.askopenfilename(parent=self.root, filetypes=[
            ('Game images / executable', '*.iso *.xex'), ('GOD header / all files', '*')])
        if value:
            self.source.set(value)

    def run(self):
        if not self.source.get().strip() or not self.destination.get().strip():
            messagebox.showerror('Select folders', 'Select a game source and destination.', parent=self.root)
            return
        source, destination = self.source.get(), self.destination.get()
        self.busy = True
        self.cancel.clear()
        for control in self.controls:
            control.configure(state='disabled')
        self.stop.configure(state='normal')

        def update(done, total, label):
            self.latest = (done, total, label)

        def worker():
            try:
                discs = install(source, destination, update, self.cancel.is_set)
                (self.home / 'game-path.txt').write_text(str(Path(destination).resolve()), encoding='utf-8')
                self.events.put(('complete', 'Imported discs: ' + ', '.join(map(str, discs)) +
                                 '\nLaunch LostOdysseyRecomp.exe in the release folder.'))
            except Cancelled as error:
                self.events.put(('cancelled', str(error)))
            except Exception as error:
                self.events.put(('error', str(error)))
        threading.Thread(target=worker, daemon=False).start()

    def poll(self):
        done, total, label = self.latest
        self.bar['value'] = done * 100 / total if total else 0
        self.status.configure(text=(f'{done / 1024**3:.2f} / {total / 1024**3:.2f} GiB — ' if total else '') + label)
        try:
            result, message = self.events.get_nowait()
        except queue.Empty:
            pass
        else:
            self.busy = False
            self.latest = (done, total, message.split('\n')[0])
            for control in self.controls:
                control.configure(state='normal')
            self.stop.configure(state='disabled')
            if result == 'complete' and '--return-to-game' in sys.argv:
                self.root.destroy()
                return
            (messagebox.showerror if result == 'error' else messagebox.showinfo)(
                'Import ' + result, message, parent=self.root)
        self.root.after(100, self.poll)

    def close(self):
        if self.busy:
            self.cancel.set()
            self.latest = (0, 0, 'Cancelling import; wait for cleanup before closing.')
        else:
            self.root.destroy()


if __name__ == '__main__':
    root = tk.Tk()
    if '--self-test' in sys.argv:
        root.withdraw()
        app = Installer(root)
        root.update_idletasks()
        assert len(app.controls) == 6 and not app.busy
        root.destroy()
        raise SystemExit(0)
    Installer(root)
    root.mainloop()
