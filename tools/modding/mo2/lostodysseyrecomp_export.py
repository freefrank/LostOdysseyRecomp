# Mod Organizer 2 tool for LostOdysseyRecomp: export the original artwork.
#
# Copy this file to <MO2>/plugins/ and restart MO2. With a "Lost Odyssey Recomp"
# instance selected, the Tools menu gets "Export Lost Odyssey assets". It runs
# LostOdysseyRecomp.exe --export-assets in the game folder (outside MO2's
# virtual file system) and writes textures, movies and text from your own game data
# to a new folder. Use the result as reference for your mods; do not share it.
# "Scan game data" lists everything once and shows what the current settings
# would export, with a preview of the selected texture, movie or text file.

from __future__ import annotations

import csv
import hashlib
import json
import os
import re
import shutil
import tempfile
from functools import partial

from PyQt6.QtCore import QAbstractTableModel, QModelIndex, QProcess, Qt, QTimer, QUrl
from PyQt6.QtGui import QDesktopServices, QIcon, QPixmap
from PyQt6.QtWidgets import (
    QCheckBox,
    QComboBox,
    QDialog,
    QFileDialog,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMessageBox,
    QPlainTextEdit,
    QProgressBar,
    QPushButton,
    QSizePolicy,
    QSplitter,
    QStackedWidget,
    QTableView,
    QVBoxLayout,
    QWidget,
)

import mobase

GAME_NAME = "Lost Odyssey Recomp"
GAME_BINARY = "LostOdysseyRecomp.exe"
PROGRESS = re.compile(r"^progress\s+(\d+)\s*/\s*(\d+)\s*$")
PACK_ID = re.compile(r"^[A-Za-z0-9._-]{1,128}$")
LANGUAGES = (
    ("All languages", ""),
    ("English (int)", "int"),
    ("Japanese (jpn)", "jpn"),
    ("German (deu)", "deu"),
    ("French (fra)", "fra"),
    ("Spanish (spa)", "spa"),
    ("Italian (ita)", "ita"),
    ("Korean (kor)", "kor"),
    ("Traditional Chinese (chi)", "chi"),
    ("Simplified Chinese (sch)", "sch"),
)
SCAN_HINT = "Scan game data to preview what will be exported."
OLD_GAME = "This version of LostOdysseyRecomp cannot list assets; update the game."
NO_DATA = (
    "LostOdysseyRecomp in this folder found no game data. Start the game once from this folder "
    "to set it up, or use Change... to choose the folder you start the game from."
)


def game_folder_problem(folder: str) -> str:
    """Why a folder cannot be the game folder; empty when it holds the game."""
    if not folder or not os.path.isdir(folder):
        return f"{folder or 'No folder'} does not exist."
    if not os.path.isfile(os.path.join(folder, GAME_BINARY)):
        return f"{folder} has no {GAME_BINARY}."
    return ""
COLUMNS = ("Kind", "Name", "Size", "Format / Language", "Path")
# Catalog row: list.csv columns, then the lower-cased path for the filter.
KIND, NAME, PATH, WIDTH, HEIGHT, FORMAT, LANGUAGE, ENTRIES, SIZE, LOWER = range(10)
PREVIEW_LINES = 200
PREVIEW_CHARS = 40000


def clean_language_pack(pack: str) -> tuple[str, int, int]:
    """Copies a language pack to share/<name> beside it with only the entries
    that differ from original/text (written by the language pack export), the
    same as lo_mod.py language-clean. Returns (output folder, files, entries)."""
    pack = os.path.abspath(pack)
    if not os.path.isfile(os.path.join(pack, "language.ini")):
        raise ValueError("This folder has no language.ini. Choose the language pack folder.")
    reference = os.path.join(os.path.dirname(pack), "original", "text")
    if not os.path.isdir(reference):
        raise ValueError(
            "No original folder next to the pack. Clean only works on a pack made with "
            "the Language pack box of this tool."
        )
    target = os.path.join(os.path.dirname(pack), "share", os.path.basename(pack))
    if os.path.isdir(target) and os.listdir(target):
        raise ValueError(f"{target} already exists. Delete it first.")
    files = entries = 0
    text = os.path.join(pack, "text")
    for folder, _, names in os.walk(text):
        for name in sorted(names):
            if not name.endswith(".json"):
                continue
            source = os.path.join(folder, name)
            relative = os.path.relpath(source, text)
            with open(source, encoding="utf-8") as file:
                data = json.load(file)
            base_file = os.path.join(reference, relative)
            if os.path.isfile(base_file):
                with open(base_file, encoding="utf-8") as file:
                    base = json.load(file)
                data = {key: value for key, value in data.items() if base.get(key) != value}
            if not data:
                continue
            destination = os.path.join(target, "text", relative)
            os.makedirs(os.path.dirname(destination), exist_ok=True)
            with open(destination, "w", encoding="utf-8", newline="\n") as file:
                file.write(json.dumps(data, ensure_ascii=False, indent=2) + "\n")
            files += 1
            entries += len(data)
    os.makedirs(target, exist_ok=True)
    shutil.copyfile(os.path.join(pack, "language.ini"), os.path.join(target, "language.ini"))
    return target, files, entries


def catalog_name(row: list[str]) -> str:
    """Textures are keyed <package>#<index>:<object>; show the object."""
    name = row[NAME]
    if row[KIND] == "texture":
        start = name.find("#")
        colon = name.find(":", max(start, 0))
        if colon >= 0:
            return name[colon + 1:]
    return name


def catalog_size(row: list[str]) -> str:
    if row[KIND] == "texture":
        return f"{row[WIDTH]}×{row[HEIGHT]}"
    if row[KIND] == "movie":
        return f"{int(row[SIZE] or 0) / 1048576:.1f} MB"
    return f"{row[ENTRIES]} lines"


def catalog_format(row: list[str]) -> str:
    if row[KIND] == "texture":
        return row[FORMAT]
    if row[KIND] == "movie":
        return os.path.splitext(row[NAME])[1].lstrip(".").upper()
    return row[LANGUAGE]


def read_catalog(path: str) -> list[list[str]]:
    """Reads list.csv from --export-list; raises OSError or ValueError."""
    rows: list[list[str]] = []
    with open(path, encoding="utf-8", newline="") as file:
        reader = csv.reader(file)
        header = next(reader, None)
        if not header or header[:3] != ["kind", "name", "path"]:
            raise ValueError("This is not a list.csv from the exporter.")
        for line in reader:
            if len(line) >= 9:
                rows.append(line[:9] + [line[PATH].lower()])
    return rows


class CatalogModel(QAbstractTableModel):
    def __init__(self, parent=None):
        super().__init__(parent)
        self._rows: list[list[str]] = []

    def set_rows(self, rows: list[list[str]]) -> None:
        self.beginResetModel()
        self._rows = rows
        self.endResetModel()

    def row_at(self, index: QModelIndex) -> list[str] | None:
        return self._rows[index.row()] if index.isValid() else None

    def rowCount(self, parent=QModelIndex()) -> int:
        return 0 if parent.isValid() else len(self._rows)

    def columnCount(self, parent=QModelIndex()) -> int:
        return 0 if parent.isValid() else len(COLUMNS)

    def headerData(self, section, orientation, role=Qt.ItemDataRole.DisplayRole):
        if orientation == Qt.Orientation.Horizontal and role == Qt.ItemDataRole.DisplayRole:
            return COLUMNS[section]
        return None

    def data(self, index, role=Qt.ItemDataRole.DisplayRole):
        if not index.isValid():
            return None
        row = self._rows[index.row()]
        column = index.column()
        if role == Qt.ItemDataRole.DisplayRole:
            return (
                row[KIND], catalog_name(row), catalog_size(row), catalog_format(row), row[PATH]
            )[column]
        if role == Qt.ItemDataRole.ToolTipRole and column in (1, 4):
            return row[NAME] if column == 1 else row[PATH]
        return None


class ImageLabel(QLabel):
    """Shows a pixmap scaled to fit, keeping the aspect ratio, at most 2x."""

    def __init__(self):
        super().__init__()
        self._pixmap: QPixmap | None = None
        self.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.setMinimumSize(120, 120)
        self.setSizePolicy(QSizePolicy.Policy.Ignored, QSizePolicy.Policy.Ignored)

    def set_source(self, pixmap: QPixmap | None) -> None:
        self._pixmap = pixmap
        self._rescale()

    def resizeEvent(self, event) -> None:
        super().resizeEvent(event)
        self._rescale()

    def _rescale(self) -> None:
        if self._pixmap is None or self._pixmap.isNull():
            self.clear()
            return
        limit = self._pixmap.size() * 2
        size = self.size().boundedTo(limit)
        self.setPixmap(self._pixmap.scaled(
            size, Qt.AspectRatioMode.KeepAspectRatio, Qt.TransformationMode.SmoothTransformation
        ))


class ExportDialog(QDialog):
    def __init__(self, parent, game_dir: str, default_output: str, cache_dir: str = "",
                 instance_dir: str = "", save_game_dir=None):
        super().__init__(parent)
        self.setWindowTitle("Export Lost Odyssey assets")
        self.setMinimumWidth(900)
        self.resize(1200, 760)
        self._game_dir = game_dir
        self._instance_dir = instance_dir or game_dir  # MO2's game folder
        self._save_game_dir = save_game_dir  # Remembers a folder chosen with Change...
        self._cache_dir = cache_dir
        self._mode = "export"
        self._scan_dir = ""
        self._session = ""  # Temp folder for scans and previews, removed on close.
        self._jobs = 0
        self._catalog: list[list[str]] = []
        self._wanted: list[str] | None = None  # Selected row.
        self._play: list[str] | None = None  # Movie to open once exported.
        self._helper: QProcess | None = None
        self._helper_job: tuple[tuple, str] | None = None
        self._helper_errors: list[str] = []
        self._done: dict[tuple, str] = {}  # Job key -> export folder.
        self._failed: dict[tuple, str] = {}
        self._output = ""
        self._process: QProcess | None = None
        self._buffer = ""
        self._last_line = ""
        self._errors: list[str] = []

        info = QLabel(
            "Exports the original textures (PNG), movies and text (JSON, the "
            "starting point for a translation) from your own game data. The files "
            "are for reference only; do not share them."
        )
        info.setWordWrap(True)
        self._game = QLineEdit(game_dir)
        self._game.setReadOnly(True)
        self._game.setToolTip("The folder you start the game from: it holds LostOdysseyRecomp.exe.")
        change = QPushButton("Change...")
        change.clicked.connect(self._choose_game)
        self._change = change
        game_row = QHBoxLayout()
        game_row.addWidget(self._game, 1)
        game_row.addWidget(change)
        self._game_warning = QLabel("")
        self._game_warning.setWordWrap(True)
        self._game_warning.setStyleSheet("color: #e0a030;")
        self._game_warning.hide()
        self._folder = QLineEdit(default_output)
        browse = QPushButton("Browse...")
        browse.clicked.connect(self._browse)
        folder_row = QHBoxLayout()
        folder_row.addWidget(self._folder, 1)
        folder_row.addWidget(browse)
        self._textures = QCheckBox("Textures")
        self._textures.setChecked(True)
        self._movies = QCheckBox("Movies")
        self._movies.setChecked(True)
        self._text = QCheckBox("Text")
        self._text.setChecked(True)
        kinds = QHBoxLayout()
        kinds.addWidget(self._textures)
        kinds.addWidget(self._movies)
        kinds.addWidget(self._text)
        kinds.addStretch(1)
        self._filter = QLineEdit()
        self._filter.setPlaceholderText("optional: only names containing this text")
        self._language = QComboBox()
        for label, code in LANGUAGES:
            self._language.addItem(label, code)
        self._pack = QLineEdit()
        self._pack.setPlaceholderText(
            "optional: id of a new language, such as pt-br (exports text only)"
        )
        form = QFormLayout()
        form.addRow("Game folder", game_row)
        form.addRow("", self._game_warning)
        form.addRow("Output folder", folder_row)
        form.addRow("Export", kinds)
        form.addRow("Filter", self._filter)
        form.addRow("Text language", self._language)
        form.addRow("Language pack", self._pack)
        for box in (self._textures, self._movies, self._text):
            box.toggled.connect(self._schedule_refresh)
        self._filter.textChanged.connect(self._schedule_refresh)
        self._language.currentIndexChanged.connect(self._schedule_refresh)
        self._pack.textChanged.connect(self._schedule_refresh)
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(250)
        self._timer.timeout.connect(self._refresh)

        self._scan = QPushButton("Scan game data")
        self._scan.setToolTip("List every texture, movie and text file once, to preview an export")
        self._scan.clicked.connect(self._begin_scan)
        self._count = QLabel(SCAN_HINT)
        self._count.setWordWrap(True)
        self._model = CatalogModel(self)
        self._table = QTableView()
        self._table.setModel(self._model)
        self._table.setToolTip("Double-click a row to put its path into Filter.")
        self._table.setSelectionBehavior(QTableView.SelectionBehavior.SelectRows)
        self._table.setSelectionMode(QTableView.SelectionMode.SingleSelection)
        self._table.setEditTriggers(QTableView.EditTrigger.NoEditTriggers)
        self._table.verticalHeader().hide()
        self._table.verticalHeader().setDefaultSectionSize(22)
        self._table.horizontalHeader().setStretchLastSection(True)
        for column, width in enumerate((70, 220, 90, 120)):
            self._table.setColumnWidth(column, width)
        self._table.doubleClicked.connect(self._use_path)
        self._table.selectionModel().currentRowChanged.connect(self._selected)
        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(0, 0, 0, 0)
        scan_row = QHBoxLayout()
        scan_row.addWidget(self._scan)
        scan_row.addStretch(1)
        left_layout.addLayout(scan_row)
        left_layout.addWidget(self._table, 1)
        left_layout.addWidget(self._count)

        self._message = QLabel("")
        self._message.setWordWrap(True)
        self._message.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._image = ImageLabel()
        self._image_caption = QLabel("")
        self._image_caption.setAlignment(Qt.AlignmentFlag.AlignCenter)
        image_page = QWidget()
        image_layout = QVBoxLayout(image_page)
        image_layout.addWidget(self._image, 1)
        image_layout.addWidget(self._image_caption)
        self._preview_text = QPlainTextEdit()
        self._preview_text.setReadOnly(True)
        self._movie_info = QLabel("")
        self._movie_info.setWordWrap(True)
        self._movie_note = QLabel("")
        self._movie_note.setWordWrap(True)
        self._play_button = QPushButton("Play")
        self._play_button.clicked.connect(self._play_clicked)
        movie_page = QWidget()
        movie_layout = QVBoxLayout(movie_page)
        movie_layout.addWidget(self._movie_info)
        movie_layout.addWidget(self._play_button)
        movie_layout.addWidget(self._movie_note)
        movie_layout.addStretch(1)
        self._pages = QStackedWidget()
        for page in (self._message, image_page, self._preview_text, movie_page):
            self._pages.addWidget(page)
        self._show_message("Select a row to preview it.")

        splitter = QSplitter(Qt.Orientation.Horizontal)
        splitter.addWidget(left)
        splitter.addWidget(self._pages)
        splitter.setStretchFactor(0, 3)
        splitter.setStretchFactor(1, 2)
        splitter.setSizes([700, 460])

        self._bar = QProgressBar()
        self._bar.setRange(0, 1)
        self._bar.setValue(0)
        self._status = QLabel("")
        self._status.setWordWrap(True)
        self._log = QPlainTextEdit()
        self._log.setReadOnly(True)
        self._log.setMaximumHeight(120)
        self._log.hide()

        self._start = QPushButton("Start")
        self._start.clicked.connect(self._begin)
        self._open = QPushButton("Open folder")
        self._open.clicked.connect(self._open_folder)
        self._open.setEnabled(False)
        self._close = QPushButton("Close")
        self._close.clicked.connect(self.reject)
        self._clean = QPushButton("Clean language pack...")
        self._clean.setToolTip(
            "Copy a translated language pack with only the lines you translated, ready to share"
        )
        self._clean.clicked.connect(self._clean_pack)
        buttons = QHBoxLayout()
        buttons.addWidget(self._clean)
        buttons.addStretch(1)
        buttons.addWidget(self._start)
        buttons.addWidget(self._open)
        buttons.addWidget(self._close)

        layout = QVBoxLayout(self)
        layout.addWidget(info)
        layout.addLayout(form)
        layout.addWidget(splitter, 1)
        layout.addWidget(self._bar)
        layout.addWidget(self._status)
        layout.addWidget(self._log)
        layout.addLayout(buttons)

        self.finished.connect(self._cleanup)
        self._show_game_folder()
        self._load_cache()

    def _show_game_folder(self) -> None:
        """The game folder row and, when it is wrong or not MO2's, what to do."""
        self._game.setText(self._game_dir)
        problem = game_folder_problem(self._game_dir)
        if problem:
            note = (f"{problem} Use Change... to choose your own LostOdysseyRecomp folder, the one you "
                    f"start the game from. MO2 also needs that folder to run the game with mods: see "
                    f"\"If MO2 points to the wrong folder\" in the wiki page Mod Organizer 2.")
        elif os.path.normcase(os.path.abspath(self._game_dir)) != os.path.normcase(os.path.abspath(self._instance_dir)):
            note = (f"Exports read this folder. MO2 still runs the game from {self._instance_dir}; to play "
                    f"with mods, point the MO2 instance at this folder too (wiki page Mod Organizer 2).")
        else:
            note = ""
        self._game_warning.setText(note)
        self._game_warning.setVisible(bool(note))

    def _choose_game(self) -> None:
        start = self._game_dir if os.path.isdir(self._game_dir) else os.path.dirname(self._instance_dir)
        chosen = QFileDialog.getExistingDirectory(
            self, "Choose the folder you start the game from (it holds LostOdysseyRecomp.exe)", start
        )
        if not chosen:
            return
        chosen = os.path.normpath(chosen)
        problem = game_folder_problem(chosen)
        if problem:
            QMessageBox.warning(self, "Game folder", f"{problem}\n\nChoose the folder that holds {GAME_BINARY}.")
            return
        self._stop_helper()
        self._game_dir = chosen
        if self._save_game_dir is not None:
            self._save_game_dir(chosen)
        # Previews and the catalog belong to the other folder's game.
        self._done.clear()
        self._failed.clear()
        self._catalog = []
        self._model.set_rows([])
        self._count.setText(SCAN_HINT)
        self._status.setText("")
        self._show_game_folder()
        self._load_cache()

    def _browse(self) -> None:
        start = self._folder.text() or os.path.dirname(self._game_dir)
        chosen = QFileDialog.getExistingDirectory(self, "Output folder", start)
        if chosen:
            self._folder.setText(chosen)

    def _open_folder(self) -> None:
        if self._output:
            QDesktopServices.openUrl(QUrl.fromLocalFile(self._output))

    def _clean_pack(self) -> None:
        start = self._folder.text() or os.path.dirname(self._game_dir)
        pack = QFileDialog.getExistingDirectory(
            self, "Choose the language pack folder (the one with language.ini)", start
        )
        if not pack:
            return
        try:
            target, files, entries = clean_language_pack(pack)
        except (OSError, ValueError) as error:
            QMessageBox.warning(self, "Clean language pack", str(error))
            return
        if QMessageBox.question(
            self,
            "Clean language pack",
            f"{entries} translated lines in {files} files were copied to\n{target}\n\n"
            "Zip that folder to share the pack. Open it now?",
        ) == QMessageBox.StandardButton.Yes:
            QDesktopServices.openUrl(QUrl.fromLocalFile(target))

    def _running(self) -> bool:
        return (
            self._process is not None
            and self._process.state() != QProcess.ProcessState.NotRunning
        )

    def _set_busy(self, busy: bool) -> None:
        for widget in (
            self._folder, self._textures, self._movies, self._text, self._filter,
            self._language, self._pack, self._start, self._clean, self._scan, self._change,
        ):
            widget.setEnabled(not busy)

    def _fail(self, message: str) -> None:
        self._status.setText(message)

    def _begin(self) -> None:
        output = os.path.abspath(self._folder.text().strip())
        if not self._folder.text().strip():
            self._fail("Choose an output folder.")
            return
        if os.path.exists(output) and (not os.path.isdir(output) or os.listdir(output)):
            self._fail("The output folder must not exist or must be empty.")
            return
        boxes = (("textures", self._textures), ("movies", self._movies), ("text", self._text))
        kinds = [name for name, box in boxes if box.isChecked()]
        pack = self._pack.text().strip()
        language = self._language.currentData()
        if pack and not PACK_ID.match(pack):
            self._fail("The language pack id may use only letters, digits, '-', '_' and '.', such as pt-br.")
            return
        if not kinds and not pack:
            self._fail("Select at least one of textures, movies and text.")
            return
        exe = os.path.join(self._game_dir, GAME_BINARY)
        if not os.path.isfile(exe):
            self._fail(f"{GAME_BINARY} was not found in {self._game_dir}.")
            return
        arguments = ["--export-assets", output]
        if pack:
            # Text only, English unless another language is chosen.
            arguments += ["--export-language-pack", pack]
        else:
            arguments += ["--export-kinds", ",".join(kinds)]
        if language:
            arguments += ["--export-language", language]
        if self._filter.text().strip():
            arguments += ["--export-filter", self._filter.text().strip()]

        self._output = output
        self._launch("export", exe, arguments)

    def _launch(self, mode: str, exe: str, arguments: list[str]) -> None:
        self._mode = mode
        self._buffer = ""
        self._last_line = ""
        self._errors = []
        self._log.clear()
        self._log.hide()
        self._open.setEnabled(False)
        self._bar.setRange(0, 0)  # Busy until the first progress line arrives.
        self._status.setText("Scanning..." if mode == "scan" else "Exporting...")
        self._set_busy(True)

        # Started directly so the export reads the real game folder, not MO2's VFS.
        process = QProcess(self)
        process.setProgram(exe)
        process.setArguments(arguments)
        process.setWorkingDirectory(self._game_dir)
        process.readyReadStandardOutput.connect(self._on_stdout)
        process.readyReadStandardError.connect(self._on_stderr)
        process.errorOccurred.connect(self._on_error)
        process.finished.connect(self._on_finished)
        self._process = process
        process.start()

    def _on_stdout(self) -> None:
        if self._process is None:
            return
        self._buffer += bytes(self._process.readAllStandardOutput()).decode("utf-8", "replace")
        *lines, self._buffer = self._buffer.split("\n")
        for line in lines:
            line = line.strip()
            if not line:
                continue
            match = PROGRESS.match(line)
            if match:
                done, total = int(match[1]), int(match[2])
                self._bar.setRange(0, max(total, 1))
                self._bar.setValue(min(done, max(total, 1)))
                verb = "Scanning" if self._mode == "scan" else "Exporting"
                self._status.setText(f"{verb}... {done}/{total}")
            else:
                self._last_line = line

    def _on_stderr(self) -> None:
        if self._process is None:
            return
        text = bytes(self._process.readAllStandardError()).decode("utf-8", "replace")
        self._errors += [line.strip() for line in text.splitlines() if line.strip()]

    def _on_error(self, error: QProcess.ProcessError) -> None:
        if error == QProcess.ProcessError.FailedToStart:
            self._errors.append(f"Could not start {GAME_BINARY}.")
            self._finish(False, "")

    def _on_finished(self, code: int, status: QProcess.ExitStatus) -> None:
        if self._process is None:
            return
        self._on_stdout()
        if self._buffer.strip():
            self._last_line = self._buffer.strip()
            self._buffer = ""
        if status == QProcess.ExitStatus.NormalExit and code == 0:
            self._finish(True, self._last_line or "Export finished.")
        else:
            if status != QProcess.ExitStatus.NormalExit:
                self._errors.append("The exporter crashed.")
            else:
                self._errors.append(f"The exporter exited with code {code}.")
            self._finish(False, "")

    def _finish(self, ok: bool, message: str) -> None:
        if self._process is None:
            return
        self._process = None
        self._set_busy(False)
        self._bar.setRange(0, 1)
        self._bar.setValue(1 if ok else 0)
        if self._mode == "scan":
            self._finish_scan(ok, message)
        elif ok:
            self._status.setText(message)
            self._open.setEnabled(os.path.isdir(self._output))
            if QMessageBox.question(
                self, "Export finished", message + "\n\nOpen the output folder?"
            ) == QMessageBox.StandardButton.Yes:
                self._open_folder()
        else:
            no_data = any("no game data" in line for line in self._errors)
            self._status.setText(NO_DATA if no_data else "Export failed.")
            self._log.setPlainText("\n".join(self._errors[-20:]) or "No error message was printed.")
            self._log.show()

    def reject(self) -> None:
        if self._running():
            answer = QMessageBox.question(
                self, "Export running", "Stop the export and close this window?"
            )
            if answer != QMessageBox.StandardButton.Yes:
                return
            process, self._process = self._process, None
            process.kill()
            process.waitForFinished(3000)
        self._stop_helper()
        super().reject()

    def _cleanup(self) -> None:
        self._stop_helper()
        if self._session:
            shutil.rmtree(self._session, ignore_errors=True)
            self._session = ""

    # Catalog

    def _exe(self) -> str | None:
        exe = os.path.join(self._game_dir, GAME_BINARY)
        if os.path.isfile(exe):
            return exe
        self._fail(f"{GAME_BINARY} was not found in {self._game_dir}.")
        return None

    def _session_dir(self, name: str) -> str:
        """A new, not yet existing folder inside the session temp folder."""
        if not self._session:
            self._session = tempfile.mkdtemp(prefix="lo-export-preview-")
        self._jobs += 1
        return os.path.join(self._session, f"{name}{self._jobs}")

    def _cache_file(self) -> str:
        exe = os.path.join(self._game_dir, GAME_BINARY)
        try:
            info = os.stat(exe)
        except OSError:
            return ""
        game = os.path.normcase(os.path.abspath(self._game_dir))
        stamp = f"{game}|{info.st_size}|{info.st_mtime_ns}"
        name = hashlib.sha1(stamp.encode("utf-8")).hexdigest()[:12]
        return os.path.join(self._cache_dir, f"catalog-{name}.csv") if self._cache_dir else ""

    def _load_cache(self) -> None:
        path = self._cache_file()
        if not path or not os.path.isfile(path):
            return
        try:
            self._catalog = read_catalog(path)
        except (OSError, ValueError, csv.Error):
            self._catalog = []
        self._refresh()

    def _begin_scan(self) -> None:
        if self._running():
            return
        exe = self._exe()
        if exe is None:
            return
        self._scan_dir = self._session_dir("scan")
        self._launch("scan", exe, ["--export-assets", self._scan_dir, "--export-list"])

    def _finish_scan(self, ok: bool, message: str) -> None:
        rows: list[list[str]] = []
        if ok:
            source = os.path.join(self._scan_dir, "list.csv")
            try:
                rows = read_catalog(source)
                self._save_cache(source)
            except (OSError, ValueError, csv.Error) as error:
                ok = False
                self._errors.append(f"Could not read the list: {error}")
        shutil.rmtree(self._scan_dir, ignore_errors=True)
        if not ok:
            old = any("--export-list" in line for line in self._errors)
            no_data = any("no game data" in line for line in self._errors)
            self._status.setText(OLD_GAME if old else NO_DATA if no_data else "Scan failed.")
            self._log.setPlainText("\n".join(self._errors[-20:]) or "No error message was printed.")
            self._log.show()
            return
        self._catalog = rows
        self._status.setText(message or "Scan finished.")
        self._refresh()

    def _save_cache(self, source: str) -> None:
        path = self._cache_file()
        if not path:
            return
        try:
            os.makedirs(self._cache_dir, exist_ok=True)
            for name in os.listdir(self._cache_dir):
                if name.startswith("catalog-") and name.endswith(".csv"):
                    os.remove(os.path.join(self._cache_dir, name))
            shutil.copyfile(source, path + ".tmp")
            os.replace(path + ".tmp", path)
        except OSError:
            pass  # The scan still works for this session.

    # Table

    def _schedule_refresh(self, *_) -> None:
        self._timer.start()

    def _selection(self) -> tuple[set[str], str, str]:
        """Kinds, text language and filter the current settings would export."""
        pack = self._pack.text().strip()
        language = self._language.currentData()
        text = self._filter.text().strip().lower()
        if pack:
            return {"text"}, language or "int", text
        boxes = (("texture", self._textures), ("movie", self._movies), ("text", self._text))
        return {name for name, box in boxes if box.isChecked()}, language, text

    def _refresh(self) -> None:
        self._timer.stop()
        if not self._catalog:
            self._model.set_rows([])
            self._count.setText(SCAN_HINT)
            return
        kinds, language, text = self._selection()
        rows = [
            row for row in self._catalog
            if row[KIND] in kinds
            and (not text or text in row[LOWER])
            and (row[KIND] != "text" or not language or row[LANGUAGE] == language)
        ]
        self._model.set_rows(rows)
        counts = {"texture": 0, "movie": 0, "text": 0}
        for row in rows:
            counts[row[KIND]] += 1
        movies = "movie" if counts["movie"] == 1 else "movies"
        self._count.setText(
            f"{counts['texture']:,} textures, {counts['movie']:,} {movies}, "
            f"{counts['text']:,} text files will be exported"
        )
        self._selected(self._table.currentIndex())

    def _use_path(self, index: QModelIndex) -> None:
        row = self._model.row_at(index)
        if row is not None:
            self._filter.setText(row[PATH])

    # Preview

    def _selected(self, index: QModelIndex, _previous=None) -> None:
        self._wanted = self._model.row_at(index)
        if self._wanted is None:
            self._show_message("Select a row to preview it.")
        self._pump()

    def _show_message(self, text: str) -> None:
        self._message.setText(text)
        self._pages.setCurrentIndex(0)

    def _show_movie(self, row: list[str], note: str) -> None:
        self._movie_info.setText(f"{row[NAME]}\n{catalog_size(row)}")
        self._movie_note.setText(note)
        self._play_button.setEnabled(self._play is None)
        self._pages.setCurrentIndex(3)

    def _job(self, row: list[str]) -> tuple[tuple, list[str]]:
        """Cache key and exporter arguments that produce the row's files."""
        kind, path = row[KIND], row[PATH]
        if kind == "text":
            arguments = ["--export-kinds", "text", "--export-filter", path.lower()]
            if row[LANGUAGE]:
                arguments += ["--export-language", row[LANGUAGE]]
            return ("text", path, row[LANGUAGE]), arguments
        name = "textures" if kind == "texture" else "movies"
        return (kind, path), ["--export-kinds", name, "--export-filter", path.lower()]

    def _play_clicked(self) -> None:
        row = self._wanted
        if row is None or row[KIND] != "movie":
            return
        self._failed.pop(self._job(row)[0], None)
        self._play = row
        self._pump()

    def _next_job(self) -> list[str] | None:
        """Serves what is cached; returns the row that needs a helper run, if any."""
        if self._play is not None:
            key = self._job(self._play)[0]
            if key in self._done:
                self._open_movie(self._play)
                self._play = None
            elif key not in self._failed:
                if self._wanted is not None and self._wanted[KIND] == "movie":
                    self._show_movie(self._wanted, "Loading preview...")
                return self._play
            else:
                self._play = None
        row = self._wanted
        if row is None:
            return None
        key = self._job(row)[0]
        if row[KIND] == "movie":
            self._show_movie(row, self._failed.get(key, ""))
        elif key in self._done:
            self._render(row, self._done[key])
        elif key in self._failed:
            self._show_message(self._failed[key])
        else:
            self._show_message("Loading preview...")
            return row
        return None

    def _pump(self) -> None:
        row = self._next_job()
        if row is not None and self._helper is None:
            self._start_helper(row)

    def _start_helper(self, row: list[str]) -> None:
        key, arguments = self._job(row)
        folder = self._session_dir("job")
        self._helper_errors = []
        process = QProcess(self)
        process.setProgram(os.path.join(self._game_dir, GAME_BINARY))
        process.setArguments(["--export-assets", folder] + arguments)
        process.setWorkingDirectory(self._game_dir)
        process.readyReadStandardOutput.connect(process.readAllStandardOutput)
        process.readyReadStandardError.connect(partial(self._helper_stderr, process))
        process.errorOccurred.connect(partial(self._helper_error, process))
        process.finished.connect(partial(self._helper_finished, process))
        self._helper = process
        self._helper_job = (key, folder)
        process.start()

    def _helper_stderr(self, process: QProcess) -> None:
        text = bytes(process.readAllStandardError()).decode("utf-8", "replace")
        self._helper_errors += [line.strip() for line in text.splitlines() if line.strip()]

    def _helper_error(self, process: QProcess, error: QProcess.ProcessError) -> None:
        if error == QProcess.ProcessError.FailedToStart:
            self._helper_errors.append(f"Could not start {GAME_BINARY}.")
            self._helper_end(process, False)

    def _helper_finished(self, process: QProcess, code: int, status: QProcess.ExitStatus) -> None:
        self._helper_stderr(process)
        ok = status == QProcess.ExitStatus.NormalExit and code == 0
        if not ok:
            self._helper_errors.append(
                f"The exporter exited with code {code}."
                if status == QProcess.ExitStatus.NormalExit else "The exporter crashed."
            )
        self._helper_end(process, ok)

    def _helper_end(self, process: QProcess, ok: bool) -> None:
        if process is not self._helper or self._helper_job is None:
            return
        key, folder = self._helper_job
        self._helper = None
        self._helper_job = None
        process.deleteLater()
        if ok:
            self._done[key] = folder
        else:
            no_data = any("no game data" in line for line in self._helper_errors)
            self._failed[key] = NO_DATA if no_data else "\n".join(self._helper_errors[-10:])
        self._pump()

    def _stop_helper(self) -> None:
        process, self._helper = self._helper, None
        self._helper_job = None
        if process is not None and process.state() != QProcess.ProcessState.NotRunning:
            process.kill()
            process.waitForFinished(3000)

    def _render(self, row: list[str], folder: str) -> None:
        if row[KIND] == "texture":
            self._render_texture(row, folder)
        else:
            self._render_text(row, folder)

    def _render_texture(self, row: list[str], folder: str) -> None:
        base = os.path.join(folder, "textures")
        try:
            with open(os.path.join(base, "index.csv"), encoding="utf-8", newline="") as file:
                found = next((r for r in csv.DictReader(file) if r.get("key") == row[NAME]), None)
        except OSError:
            found = None
        pixmap = QPixmap(os.path.join(base, os.path.normpath(found["file"]))) if found else None
        if pixmap is None or pixmap.isNull():
            self._show_message("The exporter did not write this texture.")
            return
        self._image.set_source(pixmap)
        self._image_caption.setText(f"{found['width']}×{found['height']} {found['format']}")
        self._pages.setCurrentIndex(1)

    def _render_text(self, row: list[str], folder: str) -> None:
        path = os.path.join(folder, "text", os.path.normpath(row[PATH]) + ".json")
        lines: list[str] = []
        size = 0
        more = False
        try:
            with open(path, encoding="utf-8") as file:
                for line in file:
                    if len(lines) >= PREVIEW_LINES or size >= PREVIEW_CHARS:
                        more = True
                        break
                    lines.append(line.rstrip("\n"))
                    size += len(line)
        except OSError:
            self._show_message("The exporter did not write this text file.")
            return
        if more:
            lines.append("...")
        self._preview_text.setPlainText("\n".join(lines)[:PREVIEW_CHARS + 1000])
        self._pages.setCurrentIndex(2)

    def _open_movie(self, row: list[str]) -> None:
        key = self._job(row)[0]
        path = os.path.join(self._done[key], "movies", row[NAME])
        if os.path.isfile(path):
            QDesktopServices.openUrl(QUrl.fromLocalFile(path))
        else:
            self._failed[key] = "The exporter did not write this movie."


class LostOdysseyRecompExport(mobase.IPluginTool):
    def __init__(self):
        super().__init__()
        self._organizer: mobase.IOrganizer | None = None
        self._dialog: ExportDialog | None = None

    def init(self, organizer: mobase.IOrganizer) -> bool:
        self._organizer = organizer
        return True

    def name(self) -> str:
        return "Lost Odyssey Recomp Asset Export"

    def author(self) -> str:
        return "dotSlash"

    def description(self) -> str:
        return "Exports the original textures, movies and text from your own game data."

    def version(self) -> mobase.VersionInfo:
        return mobase.VersionInfo(1, 2, 0, 0)

    def requirements(self) -> list[mobase.IPluginRequirement]:
        return [mobase.PluginRequirementFactory.gameDependency(GAME_NAME)]

    def settings(self) -> list[mobase.PluginSetting]:
        return [mobase.PluginSetting(
            "game_folder", "The folder exports read, when it is not MO2's game folder (set with Change...)", ""
        )]

    def displayName(self) -> str:
        return "Export Lost Odyssey assets"

    def tooltip(self) -> str:
        return "Export the original textures, movies and text to a folder for reference."

    def icon(self) -> QIcon:
        return QIcon()

    def _cache_dir(self) -> str:
        data_path = getattr(self._organizer, "pluginDataPath", None)
        if data_path is not None:
            return os.path.join(data_path(), "lostodysseyrecomp_export")
        return os.path.join(self._organizer.basePath(), "lost-odyssey-export-cache")

    def display(self) -> None:
        parent = self._parentWidget()
        game = self._organizer.managedGame()
        if game.gameName() != GAME_NAME:
            QMessageBox.information(
                parent, "Export Lost Odyssey assets",
                f"This tool needs an instance of the game \"{GAME_NAME}\".",
            )
            return
        instance_dir = os.path.normpath(game.gameDirectory().absolutePath())
        chosen = str(self._organizer.pluginSetting(self.name(), "game_folder") or "")
        game_dir = chosen if chosen and not game_folder_problem(chosen) else instance_dir
        base = os.path.normpath(
            os.path.join(self._organizer.basePath(), "lost-odyssey-export")
        )
        default = base
        counter = 2
        while os.path.exists(default) and (not os.path.isdir(default) or os.listdir(default)):
            default = f"{base}-{counter}"
            counter += 1
        # Kept on self so Python does not collect the dialog while it is open.
        self._dialog = ExportDialog(
            parent, game_dir, default, self._cache_dir(), instance_dir,
            lambda folder: self._organizer.setPluginSetting(self.name(), "game_folder", folder),
        )
        self._dialog.exec()
        self._dialog = None


def createPlugin() -> mobase.IPlugin:
    return LostOdysseyRecompExport()
