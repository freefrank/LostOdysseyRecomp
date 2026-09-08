"""Lost Odyssey portable game importer. Frozen into InstallGame.exe for releases."""
import os
from pathlib import Path
import queue
import sys
import tempfile
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from import_game import EDITIONS, Cancelled
import import_dlc
import import_content


ICON_NAME = 'lost-odyssey-recomp.ico'


def icon_path():
    if getattr(sys, 'frozen', False):
        root = Path(getattr(sys, '_MEIPASS', Path(sys.executable).resolve().parent))
    else:
        root = Path(__file__).resolve().parents[2] / 'assets'
    candidate = root / ICON_NAME
    return candidate if candidate.is_file() else None


def set_window_icon(root):
    candidate = icon_path()
    if candidate is None:
        return
    try:
        root.iconbitmap(default=str(candidate))
    except tk.TclError:
        # Tk builds without native ICO support can still use the embedded EXE icon.
        pass


BG = '#F1F4F7'
CARD = '#FFFFFF'
INK = '#182230'
MUTED = '#607080'
NAVY = '#17283B'
BLUE = '#2867A8'
GREEN = '#287A55'
AMBER = '#9A641C'
BORDER = '#D9E0E7'

# The standalone installer currently uses English. Keep new import copy together
# so a later installer localization pass can translate it without changing logic.
IMPORT_COPY = {
    'heading': 'Import your game discs and DLC',
    'source': 'Choose files or a folder. Game discs and Xbox 360 DLC packages are identified automatically.',
    'checking': 'Identifying game discs and DLC, then checking their content…',
    'review': 'Only the listed content will be imported. Existing discs and DLC are never overwritten.',
    'destination': 'Choose your game data folder. DLC is shared by all discs.',
}


def format_size(size):
    return f'{size / 1024**3:.2f} GiB' if size >= 1024**3 else f'{size / 1024**2:.1f} MiB'


def initial_destination(home):
    configured = ''
    if (home / 'game-path.txt').is_file():
        configured = (home / 'game-path.txt').read_text(encoding='utf-8').strip()
    if configured:
        path = Path(configured)
        return str((path if path.is_absolute() else home / path).resolve())
    return str((home.parent / 'game').resolve())


def review_rows(result):
    rows = []
    for disc in result.discs:
        identity = {'sha256': 'Audited SHA256', 'md5': 'Audited MD5'}[disc.info['identity']]
        rows.append((f"Disc {disc.info['disc']}", EDITIONS[disc.info['edition']]['label'],
                     disc.format, identity, disc.files, format_size(disc.bytes)))
    for package in result.packages:
        info = package.info
        rows.append(('DLC', info['display_name'], info['format'],
                     f"{info['title_id']} / {info['content_id']}", package.files, format_size(package.bytes)))
    return rows


def selection_summary(result):
    parts = []
    if result.discs:
        parts.append(f'{len(result.discs)} disc(s)')
    if result.packages:
        parts.append(f'{len(result.packages)} DLC package(s)')
    return ' + '.join(parts)


class Installer:
    def __init__(self, root):
        self.root = root
        root.title('Lost Odyssey — Game data setup')
        root.geometry('820x650')
        root.minsize(760, 600)
        root.configure(bg=BG)
        self.events = queue.Queue()
        self.cancel = threading.Event()
        self.busy = False
        self.inspected_source = None
        self.reviewed = None
        self.selected_files = ()
        self.completed_discs = set()
        self.path_warning = ''
        self.latest = (0, 0, 'Choose files or a folder containing your game discs or DLC.')
        home = Path(sys.executable if getattr(sys, 'frozen', False) else __file__).resolve().parent
        self.home = home
        self.source = tk.StringVar()
        self.destination = tk.StringVar(value=initial_destination(home))
        self.source.trace_add('write', self.source_changed)

        self.configure_styles()
        self.make_header()
        self.make_content()
        root.protocol('WM_DELETE_WINDOW', self.close)
        root.after(100, self.poll)

    def configure_styles(self):
        style = ttk.Style(self.root)
        if 'vista' in style.theme_names():
            style.theme_use('vista')
        style.configure('App.TFrame', background=BG)
        style.configure('Card.TFrame', background=CARD)
        style.configure('Card.TLabel', background=CARD, foreground=INK, font=('Segoe UI', 10))
        style.configure('Field.TLabel', background=CARD, foreground=MUTED, font=('Segoe UI Semibold', 9))
        style.configure('Muted.TLabel', background=CARD, foreground=MUTED, font=('Segoe UI', 9))
        style.configure('Status.TLabel', background=BG, foreground=MUTED, font=('Segoe UI', 9))
        style.configure('Treeview', font=('Segoe UI', 9), rowheight=28, borderwidth=0)
        style.configure('Treeview.Heading', font=('Segoe UI Semibold', 9))
        style.configure('Horizontal.TProgressbar', thickness=5)

    def make_header(self):
        header = tk.Frame(self.root, bg=NAVY, height=128)
        header.pack(fill='x')
        header.pack_propagate(False)
        text = tk.Frame(header, bg=NAVY)
        text.pack(fill='both', expand=True, padx=30, pady=20)
        tk.Label(text, text='LOST ODYSSEY RECOMP  /  GAME DATA', bg=NAVY, fg='#D6B66E',
                 font=('Segoe UI Semibold', 9)).pack(anchor='w')
        tk.Label(text, text=IMPORT_COPY['heading'], bg=NAVY, fg='white',
                 font=('Segoe UI Semibold', 23)).pack(anchor='w', pady=(3, 1))
        tk.Label(text, text='Your source files stay untouched. You can add the remaining discs later.',
                 bg=NAVY, fg='#C8D2DC', font=('Segoe UI', 10)).pack(anchor='w')

    def card(self, parent):
        outer = tk.Frame(parent, bg=BORDER)
        inner = ttk.Frame(outer, style='Card.TFrame', padding=(18, 14))
        inner.pack(fill='both', expand=True, padx=1, pady=1)
        return outer, inner

    def make_content(self):
        content = ttk.Frame(self.root, style='App.TFrame', padding=(28, 20, 28, 18))
        content.pack(fill='both', expand=True)
        content.columnconfigure(0, weight=1)
        content.rowconfigure(1, weight=1)
        self.controls = []

        source_card, source = self.card(content)
        source_card.grid(row=0, column=0, sticky='ew')
        source.columnconfigure(0, weight=1)
        ttk.Label(source, text='1  SELECT AND CHECK YOUR SOURCE', style='Field.TLabel').grid(
            row=0, column=0, columnspan=4, sticky='w')
        self.source_hint = ttk.Label(source, text=IMPORT_COPY['source'],
                                    style='Muted.TLabel', wraplength=700)
        self.source_hint.grid(row=1, column=0, columnspan=4, sticky='w', pady=(3, 10))
        self.source_entry = ttk.Entry(source, textvariable=self.source)
        self.source_entry.grid(row=2, column=0, sticky='ew')
        folder = ttk.Button(source, text='Folder…', command=lambda: self.folder(self.source, True))
        folder.grid(row=2, column=1, padx=(8, 0))
        file_button = ttk.Button(source, text='Files…', command=self.file)
        file_button.grid(row=2, column=2, padx=(6, 0))
        self.check = ttk.Button(source, text='Check source', command=self.check_source)
        self.check.grid(row=2, column=3, padx=(6, 0))
        self.controls.extend((self.source_entry, folder, file_button, self.check))

        review_card, review = self.card(content)
        review_card.grid(row=1, column=0, sticky='nsew', pady=(12, 0))
        review.columnconfigure(0, weight=1)
        review.rowconfigure(2, weight=1)
        self.review_title = ttk.Label(review, text='No source checked', style='Card.TLabel',
                                      font=('Segoe UI Semibold', 12))
        self.review_title.grid(row=0, column=0, sticky='w')
        self.review_detail = ttk.Label(review, text='The installer will identify and validate your content before copying.',
                                       style='Muted.TLabel')
        self.review_detail.grid(row=1, column=0, sticky='w', pady=(2, 9))
        columns = ('disc', 'edition', 'format', 'identity', 'files', 'size')
        self.discs = ttk.Treeview(review, columns=columns, show='headings', height=4, selectmode='none')
        headings = ('Type', 'Edition / title', 'Source', 'Identity / Content ID', 'Files', 'Size')
        widths = (58, 130, 72, 150, 60, 82)
        for column, heading, width in zip(columns, headings, widths):
            self.discs.heading(column, text=heading)
            self.discs.column(column, width=width, minwidth=width, anchor='w', stretch=column in ('edition', 'identity'))
        self.discs.grid(row=2, column=0, sticky='nsew')

        dest_card, destination = self.card(content)
        dest_card.grid(row=2, column=0, sticky='ew', pady=(12, 0))
        destination.columnconfigure(0, weight=1)
        ttk.Label(destination, text='2  CHOOSE THE GAME DATA LOCATION', style='Field.TLabel').grid(
            row=0, column=0, columnspan=2, sticky='w')
        self.destination_hint = ttk.Label(destination, text=IMPORT_COPY['destination'], style='Muted.TLabel')
        self.destination_hint.grid(row=1, column=0, columnspan=2, sticky='w', pady=(3, 9))
        self.destination_entry = ttk.Entry(destination, textvariable=self.destination)
        self.destination_entry.grid(row=2, column=0, sticky='ew')
        dest_button = ttk.Button(destination, text='Folder…', command=lambda: self.folder(self.destination, False))
        dest_button.grid(row=2, column=1, padx=(8, 0))
        self.controls.extend((self.destination_entry, dest_button))

        footer = ttk.Frame(content, style='App.TFrame')
        footer.grid(row=3, column=0, sticky='ew', pady=(15, 0))
        footer.columnconfigure(0, weight=1)
        self.bar = ttk.Progressbar(footer, maximum=100)
        self.bar.grid(row=0, column=0, columnspan=3, sticky='ew')
        self.status = ttk.Label(footer, text=self.latest[2], style='Status.TLabel', wraplength=540)
        self.status.grid(row=1, column=0, sticky='w', pady=(7, 0))
        self.stop = ttk.Button(footer, text='Cancel', command=self.cancel.set, state='disabled')
        self.stop.grid(row=1, column=1, padx=(10, 6), pady=(7, 0))
        self.start = tk.Button(footer, text='Import', command=self.run, state='disabled',
                               bg=BLUE, activebackground='#1F568E', fg='white', activeforeground='white',
                               disabledforeground='#C3CCD5', relief='flat', bd=0, padx=17, pady=7,
                               font=('Segoe UI Semibold', 9), cursor='hand2')
        self.start.grid(row=1, column=2, pady=(7, 0))

    def source_changed(self, *_):
        if self.busy:
            return
        self.inspected_source = None
        self.reviewed = None
        self.selected_files = ()
        self.start.configure(state='disabled')
        self.review_title.configure(text='Source changed — check again', foreground=INK)
        self.review_detail.configure(text='No files will be copied until the source passes validation.')
        for item in self.discs.get_children():
            self.discs.delete(item)

    def source_selection(self):
        return self.selected_files or (self.source.get().strip(),)

    def folder(self, variable, inspect):
        value = filedialog.askdirectory(parent=self.root, initialdir=variable.get() or None)
        if value:
            variable.set(value)
            if inspect:
                self.check_source()

    def file(self):
        values = filedialog.askopenfilenames(parent=self.root, title='Select game discs or DLC packages',
                                            filetypes=[('Game files and DLC packages (any filename)', '*')])
        if values:
            self.source.set(values[0] if len(values) == 1 else f'{len(values)} selected files')
            self.selected_files = tuple(values)
            self.check_source()

    def set_busy(self, value):
        self.busy = value
        for control in self.controls:
            control.configure(state='disabled' if value else 'normal')
        self.stop.configure(state='normal' if value else 'disabled')
        if value:
            self.start.configure(state='disabled')
        elif self.inspected_source:
            self.start.configure(state='normal')

    def check_source(self):
        source = self.source.get().strip()
        if not source:
            messagebox.showerror('Choose a source', 'Choose a game file or folder first.', parent=self.root)
            return
        self.cancel.clear()
        self.set_busy('scan')
        self.latest = (0, 0, IMPORT_COPY['checking'])
        selection = self.source_selection()
        self.review_title.configure(text='Checking source…', foreground=BLUE)
        self.review_detail.configure(text='Nested folders are searched without changing any files.')

        def worker():
            try:
                result = import_content.scan(selection, cancelled=self.cancel.is_set)
                self.events.put(('scanned', source, result))
            except Cancelled as error:
                self.events.put(('cancelled', str(error)))
            except Exception as error:
                self.events.put(('error', str(error)))
        threading.Thread(target=worker, daemon=False).start()

    def show_scan(self, result):
        self.reviewed = result
        for item in self.discs.get_children():
            self.discs.delete(item)
        for row in review_rows(result):
            self.discs.insert('', 'end', values=row)
        summary = selection_summary(result)
        self.review_title.configure(text=f'{summary} ready' if summary else 'Import complete', foreground=GREEN)
        detail = IMPORT_COPY['review']
        if result.rejected:
            detail += f' {len(result.rejected)} unsupported or invalid candidate(s) excluded; details appear before import.'
        self.review_detail.configure(text=detail, wraplength=690)
        self.start.configure(text='Import', state='normal' if summary else 'disabled')

    def run(self):
        source = self.source.get().strip()
        destination = self.destination.get().strip()
        if not destination:
            messagebox.showerror('Choose a destination', 'Choose where the imported game data should be stored.', parent=self.root)
            return
        if source != self.inspected_source:
            messagebox.showerror('Check the source', 'Check the current source before importing.', parent=self.root)
            return
        reviewed = self.reviewed
        if not reviewed or not (reviewed.discs or reviewed.packages):
            return
        details = '\n'.join(f"Disc {d.info['disc']} — {EDITIONS[d.info['edition']]['label']} ({d.format})"
                            for d in reviewed.discs)
        if reviewed.packages:
            details += '\n' + '\n'.join(f"DLC: {p.info['display_name']}\n  {p.info['content_id']}" for p in reviewed.packages)
        if reviewed.rejected:
            details += '\n\nExcluded candidates:\n' + '\n'.join(f'{p.name}: {error}' for p, error in reviewed.rejected[:8])
        try:
            target = import_dlc.game_root(destination)
        except (ValueError, OSError) as error:
            messagebox.showerror('Choose a destination', str(error), parent=self.root)
            return
        details += f'\n\nGame data: {target}'
        if reviewed.packages:
            details += f'\nDLC: {target / "dlc"}\n\nRestart the game after importing DLC.'
        if reviewed.discs and reviewed.packages:
            details += '\n\nDiscs are imported first. If DLC fails or is cancelled, completed discs stay installed and only DLC remains to retry.'
        if not messagebox.askokcancel('Import this content?', details.strip(), parent=self.root):
            return
        self.cancel.clear()
        self.set_busy('import')
        self.latest = (0, 0, 'Preparing the transactional import…')

        def update(done, total, label):
            self.latest = (done, total, label)

        def worker():
            try:
                result = import_content.install(reviewed, target, update, self.cancel.is_set, self.save_game_path)
                self.events.put(('import_result', result))
            except Cancelled as error:
                self.events.put(('cancelled', str(error)))
            except Exception as error:
                self.events.put(('error', str(error)))
        threading.Thread(target=worker, daemon=False).start()

    def save_game_path(self, destination):
        temporary = None
        try:
            with tempfile.NamedTemporaryFile('w', encoding='utf-8', dir=self.home,
                                             prefix='.game-path-', delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(str(Path(destination).resolve()))
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, self.home / 'game-path.txt')
            return ''
        except OSError as error:
            if temporary:
                temporary.unlink(missing_ok=True)
            return f'Game data was imported, but game-path.txt could not be updated: {error}'

    def poll(self):
        done, total, label = self.latest
        self.bar['value'] = done * 100 / total if total else 0
        prefix = f'{format_size(done)} / {format_size(total)}  ·  ' if total else ''
        self.status.configure(text=prefix + label)
        try:
            event = self.events.get_nowait()
        except queue.Empty:
            self.root.after(100, self.poll)
            return
        kind = event[0]
        self.set_busy(False)
        if kind == 'scanned':
            _, source, result = event
            self.inspected_source = source
            self.show_scan(result)
            self.latest = (0, 0, 'Source check complete. Review the listed content, then import when ready.')
        elif kind == 'import_result':
            result = event[1]
            self.completed_discs.update(result.discs)
            if result.warning:
                self.path_warning = result.warning
            messages = []
            if self.completed_discs:
                messages.append('Completed discs kept installed: ' + ', '.join(map(str, sorted(self.completed_discs))))
            if result.dlc:
                messages.append(f"Imported DLC packages: {len(result.dlc['imported'])}\n"
                                f"Already installed and verified: {len(result.dlc['unchanged'])}")
            messages.append(f'Game data: {result.destination}')
            if self.path_warning:
                messages.append(self.path_warning)
            self.show_scan(result.remaining)
            if result.error:
                title = 'Import partially completed' if result.discs else ('Import cancelled' if result.cancelled else 'Import needs attention')
                messages.append(result.error)
                messages.append('Only the content still listed remains to import. Correct the issue, then click Import to retry.')
                self.latest = (0, 0, title + '. ' + selection_summary(result.remaining) + ' remaining.')
                self.review_title.configure(text='Remaining content — ready to retry', foreground=AMBER)
                messagebox.showwarning(title, '\n\n'.join(messages), parent=self.root)
            elif '--return-to-game' in sys.argv and self.completed_discs and not self.path_warning:
                self.root.destroy()
                return
            else:
                self.latest = (done, total, 'Import complete.')
                messages.append('Launch or restart LostOdysseyRecomp.exe to use the imported content.')
                (messagebox.showwarning if self.path_warning else messagebox.showinfo)(
                    'Import complete', '\n\n'.join(messages), parent=self.root)
        else:
            message = event[1]
            self.latest = (0, 0, message)
            if kind == 'error':
                self.inspected_source = None
                self.start.configure(state='disabled')
                self.review_title.configure(text='Source needs attention', foreground=AMBER)
                self.review_detail.configure(text=message)
                messagebox.showerror('Could not import game data', message +
                                     '\n\nChoose another source or correct the issue, then retry.', parent=self.root)
            else:
                messagebox.showinfo('Import cancelled', message + '\n\nNo source files were changed.', parent=self.root)
        self.root.after(100, self.poll)

    def close(self):
        if self.busy:
            self.cancel.set()
            self.latest = (0, 0, 'Cancelling; waiting for the current file operation to finish safely…')
        else:
            self.root.destroy()


if __name__ == '__main__':
    root = tk.Tk()
    set_window_icon(root)
    if '--self-test' in sys.argv:
        root.withdraw()
        app = Installer(root)
        root.update_idletasks()
        assert len(app.controls) == 6 and not app.busy and app.inspected_source is None
        assert str(app.destination.get()).strip()
        if not (app.home / 'game-path.txt').is_file() or not (app.home / 'game-path.txt').read_text(encoding='utf-8').strip():
            assert Path(app.destination.get()) == (app.home.parent / 'game').resolve()
        root.destroy()
        raise SystemExit(0)
    Installer(root)
    root.mainloop()
