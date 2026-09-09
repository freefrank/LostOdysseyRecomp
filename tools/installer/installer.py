"""Lost Odyssey portable game importer. Frozen into InstallGame.exe for releases."""
import os
from pathlib import Path
import queue
import sys
import tempfile
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from tkinter import font as tkfont

from import_game import EDITIONS, Cancelled
import import_dlc
import import_content
from window_chrome import WindowChrome, enable_dpi_awareness


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


BG = '#101923'
CARD = '#172330'
INK = '#E7ECF1'
MUTED = '#A0AEBD'
NAVY = '#101923'
BLUE = '#DCE5EF'
GREEN = '#B3D5C6'
AMBER = '#D7B779'
BORDER = '#2B3B4D'
FIELD = '#111D29'

# The standalone installer currently uses English. Keep new import copy together
# so a later installer localization pass can translate it without changing logic.
IMPORT_COPY = {
    'heading': 'Import game content',
    'checking': 'Identifying and checking content…',
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
        count = len(result.discs)
        parts.append(f'{count} disc' + ('s' if count != 1 else ''))
    if result.packages:
        count = len(result.packages)
        parts.append(f'{count} DLC package' + ('s' if count != 1 else ''))
    return ' · '.join(parts)


class Installer:
    def __init__(self, root):
        self.root = root
        root.title('Lost Odyssey — Game data setup')
        self.scale = max(0.75, root.winfo_fpixels('1i') / 96)
        width = min(round(920 * self.scale), root.winfo_screenwidth() - 80)
        height = min(round(790 * self.scale), root.winfo_screenheight() - 100)
        root.geometry(f'{width}x{height}')
        root.minsize(round(640 * self.scale), min(round(660 * self.scale), height))
        root.configure(bg=BG)
        self.events = queue.Queue()
        self.cancel = threading.Event()
        self.busy = False
        self.inspected_source = None
        self.reviewed = None
        self.selected_files = ()
        self.completed_discs = set()
        self.path_warning = ''
        self.closing = False
        self.phase = 'select'
        self._indeterminate = False
        self._poll_id = None
        self._last_status = None
        self._last_progress = None
        self.latest = (0, 0, '')
        home = Path(sys.executable if getattr(sys, 'frozen', False) else __file__).resolve().parent
        self.home = home
        self.source = tk.StringVar()
        self.destination = tk.StringVar(value=initial_destination(home))

        self.configure_styles()
        self.chrome = WindowChrome(root, self.close, background=BG, foreground=INK,
                                   muted=MUTED, border=BORDER, on_dpi_changed=self.dpi_changed)
        self.make_header()
        self.make_content()
        self.source.trace_add('write', self.source_changed)
        self.destination.trace_add('write', lambda *_: self.update_path_preview())
        self.source_entry.bind('<FocusOut>', lambda _e: self.source_entry.xview_moveto(1))
        self.destination_entry.bind('<FocusOut>', lambda _e: self.destination_entry.xview_moveto(1))
        self.source_entry.bind('<Return>', lambda _e: self.check_source() if not self.busy else None)
        root.bind('<Control-o>', lambda _e: self.file() if not self.busy else None)
        root.bind('<Escape>', lambda _e: self.cancel_operation() if self.busy else None)
        root.protocol('WM_DELETE_WINDOW', self.close)
        self._poll_id = root.after(100, self.poll)
        self.update_path_preview()

    def configure_styles(self):
        style = ttk.Style(self.root)
        style.theme_use('clam')
        style.configure('.', background=CARD, foreground=INK, font=('Segoe UI', 10),
                        bordercolor=BORDER, lightcolor=BORDER, darkcolor=BORDER,
                        focuscolor=INK, troughcolor=FIELD)
        style.configure('App.TFrame', background=BG)
        style.configure('Card.TFrame', background=CARD)
        style.configure('Card.TLabel', background=CARD, foreground=INK, font=('Segoe UI', 10))
        style.configure('Field.TLabel', background=CARD, foreground=MUTED, font=('Segoe UI Semibold', 9))
        style.configure('Muted.TLabel', background=CARD, foreground=MUTED, font=('Segoe UI', 9))
        style.configure('Status.TLabel', background=BG, foreground=MUTED, font=('Segoe UI', 9))
        style.configure('TButton', padding=(12, 7), relief='flat', background=BORDER)
        style.map('TButton', background=[('pressed', '#475B70'), ('active', '#374B61')],
                  foreground=[('disabled', '#67798C')], bordercolor=[('focus', INK)])
        style.configure('Primary.TButton', background=BLUE, foreground=BG,
                        font=('Segoe UI Semibold', 10), padding=(20, 8))
        style.map('Primary.TButton', background=[('disabled', '#2B3B4D'), ('pressed', '#A8BACD'), ('active', '#F0F4F8')],
                  foreground=[('disabled', '#778A9D'), ('!disabled', BG)])
        style.configure('TEntry', fieldbackground=FIELD, foreground=INK, insertcolor=INK,
                        padding=(9, 7), borderwidth=1)
        style.map('TEntry', fieldbackground=[('disabled', '#17212D')],
                  foreground=[('disabled', MUTED)], bordercolor=[('focus', INK)])
        style.configure('Treeview', background=FIELD, fieldbackground=FIELD, foreground=INK,
                        font=('Segoe UI', 9), rowheight=round(30 * self.scale), borderwidth=0)
        style.map('Treeview', background=[('selected', '#34485D')], foreground=[('selected', '#FFFFFF')])
        style.configure('Treeview.Heading', background=CARD, foreground=MUTED,
                        font=('Segoe UI Semibold', 9), relief='flat', padding=(6, 7))
        style.map('Treeview.Heading', background=[('active', BORDER)])
        style.configure('Horizontal.TProgressbar', background=AMBER, thickness=4,
                        borderwidth=0, troughcolor=BORDER)
        style.configure('TScrollbar', background=BORDER, arrowcolor=MUTED,
                        borderwidth=0, arrowsize=12)

    def make_header(self):
        header = tk.Frame(self.root, bg=NAVY)
        header.pack(fill='x', padx=26, pady=(14, 20))
        tk.Label(header, text='GAME DATA SETUP', bg=NAVY, fg=AMBER,
                 font=('Segoe UI Semibold', 9)).pack(anchor='w')
        self.heading = tk.Label(header, text=IMPORT_COPY['heading'], bg=NAVY, fg=INK,
                                font=('Georgia', 25))
        self.heading.pack(anchor='w', pady=(5, 5))

    def card(self, parent):
        outer = tk.Frame(parent, bg=CARD)
        inner = ttk.Frame(outer, style='Card.TFrame', padding=(16, 12))
        inner.pack(fill='both', expand=True)
        return outer, inner

    def make_content(self):
        content = ttk.Frame(self.root, style='App.TFrame', padding=(26, 0, 26, 20))
        content.pack(fill='both', expand=True)
        content.columnconfigure(0, weight=1)
        content.rowconfigure(1, weight=1)
        self.controls = []

        source_card, source = self.card(content)
        source_card.grid(row=0, column=0, sticky='ew')
        source.columnconfigure(0, weight=1)
        ttk.Label(source, text='01  Source', style='Field.TLabel').grid(row=0, column=0, sticky='w')
        self.paths_button = ttk.Button(source, text='View paths', command=self.show_paths)
        self.paths_button.grid(row=0, column=1, sticky='e', padx=(10, 0), pady=(0, 9))
        self.source_entry = ttk.Entry(source, textvariable=self.source)
        self.source_entry.grid(row=2, column=0, columnspan=2, sticky='ew')
        source_actions = ttk.Frame(source, style='Card.TFrame')
        source_actions.grid(row=3, column=0, columnspan=2, sticky='ew', pady=(9, 0))
        source_actions.columnconfigure(0, weight=1)
        self.source_preview = ttk.Label(source_actions, text='', style='Muted.TLabel')
        self.source_preview.grid(row=0, column=0, sticky='w')
        folder = ttk.Button(source_actions, text='Folder…', command=lambda: self.folder(self.source, True))
        folder.grid(row=0, column=1, padx=(8, 0))
        file_button = ttk.Button(source_actions, text='Files…', command=self.file)
        file_button.grid(row=0, column=2, padx=(6, 0))
        self.check = ttk.Button(source_actions, text='Check source', command=self.check_source)
        self.check.grid(row=0, column=3, padx=(6, 0))
        self.controls.extend((self.source_entry, folder, file_button, self.check))

        review_card, review = self.card(content)
        review_card.grid(row=1, column=0, sticky='nsew', pady=(12, 0))
        review.columnconfigure(0, weight=1)
        review.rowconfigure(2, weight=1)
        self.review_title = ttk.Label(review, text='02  Content', style='Card.TLabel',
                                      font=('Segoe UI Semibold', 12))
        self.review_title.grid(row=0, column=0, sticky='w')
        self.review_detail = ttk.Label(review, text='',
                                       style='Muted.TLabel', justify='left')
        self.review_detail.grid(row=1, column=0, sticky='w', pady=(2, 9))
        self.review_detail.grid_remove()
        columns = ('disc', 'edition', 'format', 'identity', 'files', 'size')
        table = ttk.Frame(review, style='Card.TFrame')
        table.grid(row=2, column=0, sticky='nsew')
        table.columnconfigure(0, weight=1)
        table.rowconfigure(0, weight=1)
        self.discs = ttk.Treeview(table, columns=columns, show='headings', height=3, selectmode='browse')
        headings = ('Type', 'Edition / title', 'Source', 'Identity / Content ID', 'Files', 'Size')
        widths = (58, 170, 72, 230, 65, 85)
        for column, heading, width in zip(columns, headings, widths):
            self.discs.heading(column, text=heading, anchor='w')
            pixels = round(width * self.scale)
            self.discs.column(column, width=pixels, minwidth=pixels, anchor='w', stretch=column in ('edition', 'identity'))
        self.discs.grid(row=0, column=0, sticky='nsew')
        self.tree_y = ttk.Scrollbar(table, orient='vertical', command=self.discs.yview)
        self.tree_y.grid(row=0, column=1, sticky='ns')
        self.tree_x = ttk.Scrollbar(table, orient='horizontal', command=self.discs.xview)
        self.tree_x.grid(row=1, column=0, sticky='ew')
        self.discs.configure(yscrollcommand=self.tree_y.set, xscrollcommand=self.tree_x.set)
        self.discs.bind('<Double-1>', self.show_content_detail)
        self.discs.bind('<Return>', self.show_content_detail)

        dest_card, destination = self.card(content)
        dest_card.grid(row=2, column=0, sticky='ew', pady=(12, 0))
        destination.columnconfigure(0, weight=1)
        ttk.Label(destination, text='03  Game data location', style='Field.TLabel').grid(
            row=0, column=0, columnspan=2, sticky='w')
        self.destination_entry = ttk.Entry(destination, textvariable=self.destination)
        self.destination_entry.grid(row=2, column=0, sticky='ew', pady=(9, 0))
        dest_button = ttk.Button(destination, text='Folder…', command=lambda: self.folder(self.destination, False))
        dest_button.grid(row=2, column=1, padx=(8, 0), pady=(9, 0))
        self.controls.extend((self.destination_entry, dest_button))

        footer = ttk.Frame(content, style='App.TFrame')
        footer.grid(row=3, column=0, sticky='ew', pady=(15, 0))
        footer.columnconfigure(0, weight=1)
        self.phase_label = ttk.Label(footer, text='Select a source', style='Status.TLabel',
                                     foreground=INK, font=('Segoe UI Semibold', 10))
        self.phase_label.grid(row=0, column=0, columnspan=3, sticky='w', pady=(0, 9))
        self.bar = ttk.Progressbar(footer, maximum=100)
        self.bar.grid(row=1, column=0, columnspan=3, sticky='ew')
        self.status = ttk.Label(footer, text=self.latest[2], style='Status.TLabel', wraplength=540)
        self.status.grid(row=2, column=0, sticky='w', pady=(10, 0))
        self.stop = ttk.Button(footer, text='Cancel', command=self.cancel_operation, state='disabled')
        self.stop.grid(row=2, column=1, padx=(10, 6), pady=(10, 0))
        self.start = ttk.Button(footer, text='Import', command=self.run, state='disabled', style='Primary.TButton')
        self.start.grid(row=2, column=2, pady=(10, 0))
        review.bind('<Configure>', lambda e: self.review_detail.configure(wraplength=max(180, e.width - 36)))
        footer.bind('<Configure>', lambda e: self.status.configure(wraplength=max(160, e.width - 215)))
        source_actions.bind('<Configure>', lambda e: self.update_path_preview(
            max(1, e.width - sum(w.winfo_reqwidth() for w in (folder, file_button, self.check)) - 28)))

    def dpi_changed(self):
        previous = self.scale
        self.scale = self.chrome.scale
        self.configure_styles()
        self.root.minsize(round(640 * self.scale), round(660 * self.scale))
        if hasattr(self, 'discs'):
            for column in self.discs.cget('columns'):
                self.discs.column(column, width=round(self.discs.column(column, 'width') * self.scale / previous),
                                  minwidth=round(self.discs.column(column, 'minwidth') * self.scale / previous))

    def update_path_preview(self, width=None):
        if not hasattr(self, 'source_preview'):
            return
        text = f'{len(self.selected_files)} files selected' if len(self.selected_files) > 1 else self.source.get().strip()
        if width is not None:
            self._preview_width = width
        width = getattr(self, '_preview_width', 200)
        font = tkfont.Font(font=('Segoe UI', 9))
        if font.measure(text) > width:
            tail = text
            while tail and font.measure('…' + tail) > width:
                tail = tail[1:]
            text = '…' + tail if width >= font.measure('…') else ''
        self.source_preview.configure(text=text)

    def show_paths(self):
        sources = self.source_selection()
        text = 'Source\n' + ('\n'.join(sources) if any(sources) else 'No source selected')
        text += '\n\nGame data location\n' + self.destination.get()
        messagebox.showinfo('Selected paths', text, parent=self.root)

    def show_content_detail(self, _event=None):
        selected = self.discs.selection()
        if selected:
            values = self.discs.item(selected[0], 'values')
            labels = ('Type', 'Edition / title', 'Source', 'Identity / Content ID', 'Files', 'Size')
            messagebox.showinfo('Content details', '\n\n'.join(f'{label}\n{value}' for label, value in zip(labels, values)),
                                parent=self.root)
        return 'break'

    def set_phase(self, phase, text):
        self.phase = phase
        if hasattr(self, 'phase_label'):
            self.phase_label.configure(text=text, foreground=AMBER if phase in ('error', 'cancel') else INK)

    def cancel_operation(self):
        if self.busy:
            self.cancel.set()
            self.stop.configure(text='Cancelling…', state='disabled')
            self.set_phase('cancel', 'Finishing the current operation safely')
            self.latest = (0, 0, 'Cancellation requested. Completed content is kept; temporary files are cleaned up.')

    def source_changed(self, *_):
        if self.busy:
            return
        self.inspected_source = None
        self.reviewed = None
        self.selected_files = ()
        self.start.configure(state='disabled')
        self.set_phase('select', 'Check source')
        self.latest = (0, 0, '')
        self.review_title.configure(text='02  Content', foreground=INK)
        self.review_detail.configure(text='')
        self.review_detail.grid_remove()
        for item in self.discs.get_children():
            self.discs.delete(item)
        self.update_path_preview()

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
            self.update_path_preview()
            self.check_source()

    def set_busy(self, value):
        self.busy = value
        for control in self.controls:
            control.configure(state='disabled' if value else 'normal')
        self.stop.configure(text='Cancel', state='normal' if value else 'disabled')
        if not value and self._indeterminate:
            self.bar.stop()
            self.bar.configure(mode='determinate', value=0)
            self._indeterminate = False
            self._last_progress = 0
        if value:
            self.start.configure(state='disabled')
            self.set_phase(value, 'Checking your source' if value == 'scan' else 'Importing your content')
        elif self.inspected_source:
            self.start.configure(state='normal')

    def check_source(self):
        source = self.source.get().strip()
        if not source:
            messagebox.showerror('Choose a source', 'Choose a game file or folder first.', parent=self.root)
            return
        self.cancel.clear()
        self.inspected_source = None
        self.reviewed = None
        self.set_busy('scan')
        self.latest = (0, 0, IMPORT_COPY['checking'])
        selection = self.source_selection()
        self.review_title.configure(text='Checking source…', foreground=BLUE)
        self.review_detail.configure(text='')
        self.review_detail.grid_remove()

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
        self.review_title.configure(text=summary if summary else 'Import complete', foreground=GREEN)
        detail = ''
        if result.rejected:
            detail = f'{len(result.rejected)} excluded · details shown before import.'
        self.review_detail.configure(text=detail)
        if detail:
            self.review_detail.grid()
        else:
            self.review_detail.grid_remove()
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
        if getattr(self, '_poll_id', None):
            self.root.after_cancel(self._poll_id)
            self._poll_id = None
        done, total, label = self.latest
        indeterminate = bool(getattr(self, 'busy', False) and not total)
        if indeterminate != getattr(self, '_indeterminate', False):
            self.bar.stop()
            self.bar.configure(mode='indeterminate' if indeterminate else 'determinate')
            if indeterminate:
                self.bar.start(18)
            self._indeterminate = indeterminate
            self._last_progress = None
        if not indeterminate:
            progress = done * 100 / total if total else 0
            if progress != getattr(self, '_last_progress', None):
                self.bar['value'] = progress
                self._last_progress = progress
        prefix = f'{format_size(done)} / {format_size(total)}  ·  ' if total else ''
        status = prefix + label
        if status != getattr(self, '_last_status', None):
            self.status.configure(text=status)
            self._last_status = status
        try:
            event = self.events.get_nowait()
        except queue.Empty:
            self._poll_id = self.root.after(100, self.poll)
            return
        kind = event[0]
        self.set_busy(False)
        if getattr(self, 'closing', False):
            self.destroy()
            return
        if kind == 'scanned':
            _, source, result = event
            self.inspected_source = source
            self.show_scan(result)
            self.latest = (0, 0, '')
            self.set_phase('review', 'Ready to import')
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
                self.start.configure(text='Retry import')
                self.set_phase('error', title + ' · retry the remaining content')
                messagebox.showwarning(title, '\n\n'.join(messages), parent=self.root)
            elif '--return-to-game' in sys.argv and self.completed_discs and not self.path_warning:
                self.destroy()
                return
            else:
                self.latest = (done, total, 'Import complete.')
                self.set_phase('complete', 'Your content is ready')
                messages.append('Launch or restart LostOdysseyRecomp.exe to use the imported content.')
                (messagebox.showwarning if self.path_warning else messagebox.showinfo)(
                    'Import complete', '\n\n'.join(messages), parent=self.root)
        else:
            message = event[1]
            self.latest = (0, 0, message)
            if kind == 'error':
                self.set_phase('error', 'Needs attention · correct the issue, then check again')
                self.inspected_source = None
                self.start.configure(state='disabled')
                self.review_title.configure(text='Source needs attention', foreground=AMBER)
                self.review_detail.configure(text=message)
                self.review_detail.grid()
                messagebox.showerror('Could not import game data', message +
                                     '\n\nChoose another source or correct the issue, then retry.', parent=self.root)
            else:
                self.set_phase('cancel', 'Cancelled · choose a source or check again')
                messagebox.showinfo('Import cancelled', message + '\n\nNo source files were changed.', parent=self.root)
        self._poll_id = self.root.after(100, self.poll)

    def destroy(self):
        if getattr(self, '_poll_id', None):
            self.root.after_cancel(self._poll_id)
            self._poll_id = None
        if getattr(self, '_indeterminate', False):
            self.bar.stop()
        self.root.destroy()

    def close(self):
        if self.busy:
            self.closing = True
            self.cancel_operation()
            self.latest = (0, 0, 'Closing after the current operation finishes safely…')
        else:
            self.destroy()


if __name__ == '__main__':
    enable_dpi_awareness()
    root = tk.Tk()
    root.withdraw()
    set_window_icon(root)
    if '--self-test' in sys.argv:
        app = Installer(root)
        root.update_idletasks()
        assert len(app.controls) == 6 and not app.busy and app.inspected_source is None
        assert str(app.destination.get()).strip()
        if not (app.home / 'game-path.txt').is_file() or not (app.home / 'game-path.txt').read_text(encoding='utf-8').strip():
            assert Path(app.destination.get()) == (app.home.parent / 'game').resolve()
        app.destroy()
        raise SystemExit(0)
    Installer(root)
    root.deiconify()
    root.mainloop()
