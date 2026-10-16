# Mod Organizer 2 tool for LostOdysseyRecomp: export the original artwork.
#
# Copy this file to <MO2>/plugins/ and restart MO2. With a "Lost Odyssey Recomp"
# instance selected, the Tools menu gets "Export Lost Odyssey assets". It runs
# LostOdysseyRecomp.exe --export-assets in the game folder (outside MO2's
# virtual file system) and writes textures, movies, text, 3D models and animations (glTF) from
# your own game data to a new folder. Use the result as reference for your mods; do
# not share it. "Scan game data" lists everything once, grouped and named with the
# game's own place and character names, and shows what the current settings would
# export, with a preview of the selected texture, movie, text file, model or
# animation set (a picture the game draws). Select rows (Ctrl/Shift) and use
# "Export selected" to export only those.

from __future__ import annotations

import csv
import glob
import hashlib
import json
import os
import re
import shutil
import tempfile
import time
from functools import partial

from PyQt6.QtCore import QAbstractTableModel, QDir, QModelIndex, QProcess, Qt, QTimer, QUrl
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
OLD_GAME_NEW = (
    "This version of LostOdysseyRecomp cannot export models, animations or selections; update the game."
)
OLD_GAME_PREVIEW = "This version of LostOdysseyRecomp cannot draw previews; update the game."
OLD_GAME_NAMES = "Update the game and scan again to see place and character names."
KIND_ARGUMENT = {"texture": "textures", "movie": "movies", "text": "text", "model": "models",
                 "animation": "animations"}
# A scan lists every kind; the boxes only filter the table. The exporter's
# default kinds leave out models and animations.
SCAN_KINDS = ",".join(KIND_ARGUMENT.values())
GLB_KINDS = ("model", "animation")  # Rows exported as one glTF binary each.
MODEL_CLASSES = {"SkeletalMesh": "Skeletal", "StaticMesh": "Static", "StaticMeshRPG": "Static (map)"}
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
COLUMNS = ("Kind", "Group", "Name", "Size", "Format / Language", "Path")
# Catalog row: list.csv columns (label: the game's name on "name" rows), then
# the text the filter searches and the row's group.
KIND, NAME, PATH, WIDTH, HEIGHT, FORMAT, LANGUAGE, ENTRIES, SIZE, LABEL, LOWER, GROUP = range(12)
PREVIEW_LINES = 200
PREVIEW_CHARS = 40000
# Groups by the folder a file comes from, in table order.
GROUP_FOLDERS = (
    ("Party", ("bin/xenon/chr/pc/",)),
    ("NPCs", ("bin/xenon/chr/npcmain/", "bin/xenon/chr/npc/")),
    ("Animals", ("bin/xenon/chr/animal/",)),
    ("Monsters", ("bin/xenon/chr/enemy/", "bin/xenon/chr/boss/")),
    ("Maps", ("bin/xenon/field/",)),
    ("Battle arenas", ("bin/xenon/battle/",)),
    ("World map", ("bin/xenon/world/",)),
    ("Weapons", ("bin/xenon/obj/weapon/",)),
    ("Objects", ("bin/xenon/obj/",)),
    ("Effects", ("bin/xenon/vfx/",)),
    ("Cutscenes", ("bin/xenon/event/",)),
)
GROUPS = tuple(name for name, _ in GROUP_FOLDERS) + ("Other", "Movies", "Text")
ALL_GROUPS = "All groups"
# Map folders (bin/xenon/field/u36_0/) and character codes in file and object
# names: pc_000a0_m, en_043a0_bx, rt0230_pc040a0_d (a cutscene), np_u41a0.
PLACE = re.compile(r"^bin/xenon/(?:field|battle)/([a-z0-9]+_[a-z0-9]+)/")
CHARACTER = re.compile(r"(?:^|_)(pc|mn|np|no|en|bs|an)_?([a-z0-9]{3})([a-z][0-9])(?=_|$)")
PREVIEW_KEEP = 400  # Preview pictures kept in the plugin's data folder.
EXPORT_DAYS = 14  # Days an "Export and show" copy is kept there.


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
    """Textures and models are keyed <package>#<index>:<object>; show the object."""
    name = row[NAME]
    if row[KIND] in ("texture", "model", "animation"):
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
    if row[KIND] == "animation":
        return f"{int(row[WIDTH] or 0):,} sequences · {int(row[ENTRIES] or 0):,} frames"
    if row[KIND] == "model":
        return f"{int(row[WIDTH] or 0):,} verts · {int(row[HEIGHT] or 0):,} tris"
    return f"{row[ENTRIES]} lines"


def catalog_format(row: list[str]) -> str:
    if row[KIND] == "texture":
        return row[FORMAT]
    if row[KIND] == "movie":
        return os.path.splitext(row[NAME])[1].lstrip(".").upper()
    if row[KIND] == "animation":
        return "Animation"
    if row[KIND] == "model":
        return MODEL_CLASSES.get(row[FORMAT], row[FORMAT])
    return row[LANGUAGE]


def unknown_option(line: str) -> bool:
    """True when an exporter error line is an old build rejecting an option or kind."""
    text = line.lower()
    return "unknown" in text or "unrecognized" in text or "--export-select" in text \
        or "models" in text or "animations" in text or "model-textures" in text


def exported_file(folder: str, kind_folder: str, key: str) -> str:
    """The file an export wrote for a key (models/index.csv), or empty."""
    base = os.path.join(folder, kind_folder)
    try:
        with open(os.path.join(base, "index.csv"), encoding="utf-8", newline="") as file:
            found = next((r for r in csv.DictReader(file) if r.get("key") == key), None)
    except (OSError, csv.Error):
        return ""
    path = os.path.join(base, os.path.normpath(found["file"])) if found and found.get("file") else ""
    return path if os.path.isfile(path) else ""


def write_selection(path: str, names: list[str]) -> None:
    """The --export-select file: one list.csv name per line."""
    with open(path, "w", encoding="utf-8", newline="\n") as file:
        file.write("".join(name + "\n" for name in names))


def read_catalog(path: str) -> list[list[str]]:
    """Reads list.csv from --export-list; raises OSError or ValueError. Lists of
    older games have no label column and no "name" rows."""
    rows: list[list[str]] = []
    with open(path, encoding="utf-8", newline="") as file:
        reader = csv.reader(file)
        header = next(reader, None)
        if not header or header[:3] != ["kind", "name", "path"]:
            raise ValueError("This is not a list.csv from the exporter.")
        for line in reader:
            if len(line) >= 9:
                rows.append(line[:9] + [line[9] if len(line) > 9 else "", line[PATH].lower(), ""])
    return rows


def catalog_names(rows: list[list[str]]) -> dict[str, dict[str, str]]:
    """The game's place and character names from the "name" rows: language ->
    map or character code (u36_0, pc_000, en_043a0) -> name."""
    names: dict[str, dict[str, str]] = {}
    for row in rows:
        if row[KIND] == "name" and row[LABEL]:
            names.setdefault(row[LANGUAGE], {}).setdefault(row[NAME], row[LABEL])
    return names


def row_group(row: list[str]) -> str:
    if row[KIND] == "movie":
        return "Movies"
    if row[KIND] == "text":
        return "Text"
    path = row[PATH].lower()
    for name, folders in GROUP_FOLDERS:
        if path.startswith(folders):
            return name
    return "Other"


def row_label(row: list[str], names: dict[str, str]) -> str:
    """The Name column: the object, with the game's name of its place or character
    in front when the game has one: "Uhra - Main Street (u36_0) / StaticMesh_705",
    "Kaim (pc_000a0_m)". Without one it stays the code."""
    name = catalog_name(row)
    if row[KIND] not in ("texture", "model", "animation"):
        return name
    path = row[PATH].lower()
    place = PLACE.match(path)
    if place:
        where = names.get(place[1])
        return f"{where} ({place[1]}) / {name}" if where else name
    stem = os.path.splitext(path.rsplit("/", 1)[-1])[0]
    for text in (stem, name.lower()):
        match = CHARACTER.search(text)
        if match:
            code = f"{match[1]}_{match[2]}{match[3]}"
            # Party members by character, any outfit: pc_000a0, pc_000d0 -> pc_000.
            who = names.get(code) or (names.get(code[:6]) if code.startswith("pc_") else None)
            return f"{who} ({name})" if who else name
    return name


def label_rows(rows: list[list[str]], names: dict[str, dict[str, str]], language: str) -> None:
    """Fills each row's group, label and filter text in the given language (English
    where that language has no name), then sorts the rows by group and label."""
    chosen = dict(names.get("int", {}))
    chosen.update(names.get(language, {}))
    for row in rows:
        row[GROUP] = row_group(row)
        row[LABEL] = row_label(row, chosen)
        row[LOWER] = f"{row[PATH]} {row[NAME]} {row[LABEL]}".lower()
    order = {name: index for index, name in enumerate(GROUPS)}
    rows.sort(key=lambda row: (order.get(row[GROUP], len(order)), row[LABEL].lower(), row[NAME]))


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

    def rows(self) -> list[list[str]]:
        return self._rows

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
                row[KIND], row[GROUP], row[LABEL] or catalog_name(row), catalog_size(row),
                catalog_format(row), row[PATH],
            )[column]
        if role == Qt.ItemDataRole.ToolTipRole and column in (2, 5):
            return row[NAME] if column == 2 else row[PATH]
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
        self._names: dict[str, dict[str, str]] = {}  # The game's place and character names.
        self._labelled = ""  # Language the catalog's labels are in.
        self._wanted: list[str] | None = None  # Selected row.
        self._play: list[str] | None = None  # Movie, model or animation set to open once exported.
        self._busy = False
        self._new_options = False  # The running export needs a newer game build.
        self._helper: QProcess | None = None
        self._helper_job: tuple[tuple, str, bool] | None = None  # Key, folder, preview picture.
        self._helper_errors: list[str] = []
        self._done: dict[tuple, str] = {}  # Job key -> export folder.
        self._failed: dict[tuple, str] = {}
        self._saved: dict[tuple, str] = {}  # Job key -> where "Export and show" put the file.
        self._output = ""
        self._process: QProcess | None = None
        self._buffer = ""
        self._last_line = ""
        self._errors: list[str] = []

        info = QLabel(
            "Exports the original textures (PNG), movies, text (JSON, the "
            "starting point for a translation), 3D models and animations (glTF) from your "
            "own game data. The files are for reference only; do not share them."
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
        self._models = QCheckBox("Models")
        self._models.setToolTip("All models are about 52,000 files and 4 GB.")
        self._model_textures = QCheckBox("With textures")
        self._model_textures.setChecked(True)
        self._model_textures.setEnabled(False)
        self._model_textures.setToolTip("Write each material's colour texture beside the models, linked from them.")
        self._animations = QCheckBox("Animations")
        self._animations.setToolTip("Every animation set with its character model, as glTF (about 2.5 GB).")
        kinds = QHBoxLayout()
        kinds.addWidget(self._textures)
        kinds.addWidget(self._movies)
        kinds.addWidget(self._text)
        kinds.addWidget(self._models)
        kinds.addWidget(self._model_textures)
        kinds.addWidget(self._animations)
        kinds.addStretch(1)
        self._filter = QLineEdit()
        self._filter.setPlaceholderText(
            "optional: only files whose path contains this text; after Scan game data also names, such as Kaim"
        )
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
        for box in (self._textures, self._movies, self._text, self._models, self._animations):
            box.toggled.connect(self._schedule_refresh)
        self._models.toggled.connect(self._model_textures.setEnabled)
        self._filter.textChanged.connect(self._schedule_refresh)
        self._language.currentIndexChanged.connect(self._relabel)
        self._language.currentIndexChanged.connect(self._schedule_refresh)
        self._pack.textChanged.connect(self._schedule_refresh)
        self._pack.textChanged.connect(self._update_export_selected)
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(250)
        self._timer.timeout.connect(self._refresh)

        self._scan = QPushButton("Scan game data")
        self._scan.setToolTip("List every texture, movie, text file, model and animation set once, to preview an export")
        self._scan.clicked.connect(self._begin_scan)
        self._group = QComboBox()
        self._group.addItem(ALL_GROUPS)
        for name in GROUPS:
            self._group.addItem(name)
        self._group.setToolTip("Show only one group, such as the party, monsters or maps")
        self._group.currentIndexChanged.connect(self._schedule_refresh)
        self._count = QLabel(SCAN_HINT)
        self._count.setWordWrap(True)
        self._model = CatalogModel(self)
        self._table = QTableView()
        self._table.setModel(self._model)
        self._table.setToolTip(
            "Double-click a row to put its path into Filter. Ctrl/Shift selects several rows."
        )
        self._table.setSelectionBehavior(QTableView.SelectionBehavior.SelectRows)
        self._table.setSelectionMode(QTableView.SelectionMode.ExtendedSelection)
        self._table.setEditTriggers(QTableView.EditTrigger.NoEditTriggers)
        self._table.verticalHeader().hide()
        self._table.verticalHeader().setDefaultSectionSize(22)
        self._table.horizontalHeader().setStretchLastSection(True)
        for column, width in enumerate((70, 90, 300, 90, 110)):
            self._table.setColumnWidth(column, width)
        self._table.doubleClicked.connect(self._use_path)
        self._table.selectionModel().currentRowChanged.connect(self._selected)
        self._table.selectionModel().selectionChanged.connect(self._update_export_selected)
        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.setContentsMargins(0, 0, 0, 0)
        scan_row = QHBoxLayout()
        scan_row.addWidget(self._scan)
        scan_row.addStretch(1)
        scan_row.addWidget(QLabel("Show"))
        scan_row.addWidget(self._group)
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
        self._model_info = QLabel("")
        self._model_info.setWordWrap(True)
        self._model_image = ImageLabel()
        self._model_image.setMinimumSize(160, 160)
        self._model_status = QLabel("")
        self._model_status.setWordWrap(True)
        self._model_status.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._model_note = QLabel("")
        self._model_note.setWordWrap(True)
        self._model_note.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
        self._open_model_button = QPushButton("Export and show the file")
        self._open_model_button.setToolTip(
            "Export this row as a .glb file and show it in Explorer, ready to drag into Blender"
        )
        self._open_model_button.clicked.connect(self._open_model_clicked)
        model_page = QWidget()
        model_layout = QVBoxLayout(model_page)
        model_layout.addWidget(self._model_info)
        model_layout.addWidget(self._model_image, 1)
        model_layout.addWidget(self._model_status)
        model_layout.addWidget(self._open_model_button)
        model_layout.addWidget(self._model_note)
        self._pages = QStackedWidget()
        for page in (self._message, image_page, self._preview_text, movie_page, model_page):
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
        self._export_selected = QPushButton("Export selected")
        self._export_selected.setToolTip("Export only the rows selected in the table")
        self._export_selected.clicked.connect(self._begin_selected)
        self._export_selected.setEnabled(False)
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
        buttons.addWidget(self._export_selected)
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
        self._prune_kept()
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
        self._saved.clear()
        self._catalog = []
        self._names = {}
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
            self._models, self._animations, self._group,
        ):
            widget.setEnabled(not busy)
        self._model_textures.setEnabled(not busy and self._models.isChecked())
        self._busy = busy
        self._update_export_selected()

    def _fail(self, message: str) -> None:
        self._status.setText(message)

    def _output_folder(self) -> str | None:
        """The validated output folder, or None after showing why not."""
        output = os.path.abspath(self._folder.text().strip())
        if not self._folder.text().strip():
            self._fail("Choose an output folder.")
            return None
        if os.path.exists(output) and (not os.path.isdir(output) or os.listdir(output)):
            self._fail("The output folder must not exist or must be empty.")
            return None
        return output

    def _begin(self) -> None:
        output = self._output_folder()
        if output is None:
            return
        boxes = (
            ("textures", self._textures), ("movies", self._movies),
            ("text", self._text), ("models", self._models), ("animations", self._animations),
        )
        kinds = [name for name, box in boxes if box.isChecked()]
        pack = self._pack.text().strip()
        language = self._language.currentData()
        if pack and not PACK_ID.match(pack):
            self._fail("The language pack id may use only letters, digits, '-', '_' and '.', such as pt-br.")
            return
        if not kinds and not pack:
            self._fail("Select at least one of textures, movies, text, models and animations.")
            return
        exe = os.path.join(self._game_dir, GAME_BINARY)
        if not os.path.isfile(exe):
            self._fail(f"{GAME_BINARY} was not found in {self._game_dir}.")
            return
        if not pack and self._catalog and self._narrowed():
            # Filter also matches names, which the exporter does not know: export
            # exactly the rows the table shows.
            rows = self._model.rows()
            if not rows:
                self._fail("No row matches Filter and Show, so there is nothing to export.")
                return
            self._export_rows(output, exe, rows)
            return
        arguments = ["--export-assets", output]
        if pack:
            # Text only, English unless another language is chosen.
            arguments += ["--export-language-pack", pack]
        else:
            arguments += ["--export-kinds", ",".join(kinds)]
            if "models" in kinds and self._model_textures.isChecked():
                arguments.append("--export-model-textures")
        if language:
            arguments += ["--export-language", language]
        if self._filter.text().strip():
            arguments += ["--export-filter", self._filter.text().strip()]

        self._output = output
        self._new_options = "models" in kinds or "animations" in kinds
        self._launch("export", exe, arguments)

    def _selected_rows(self) -> list[list[str]]:
        rows = (self._model.row_at(index) for index in self._table.selectionModel().selectedRows())
        return [row for row in rows if row is not None]

    def _update_export_selected(self, *_) -> None:
        count = len(self._selected_rows())
        self._export_selected.setText(f"Export selected ({count})" if count else "Export selected")
        self._export_selected.setEnabled(
            count > 0 and not self._busy and not self._running() and not self._pack.text().strip()
        )

    def _selection_arguments(self, output: str, selection_file: str,
                             rows: list[list[str]]) -> list[str]:
        """Exporter arguments that write exactly the given catalog rows."""
        kinds = [name for kind, name in KIND_ARGUMENT.items() if any(r[KIND] == kind for r in rows)]
        arguments = ["--export-assets", output, "--export-select", selection_file,
                     "--export-kinds", ",".join(kinds)]
        languages = {row[LANGUAGE] for row in rows}
        if all(row[KIND] == "text" for row in rows) and len(languages) == 1 and "" not in languages:
            arguments += ["--export-language", languages.pop()]
        if any(row[KIND] == "model" for row in rows) and self._model_textures.isChecked():
            arguments.append("--export-model-textures")
        return arguments

    def _narrowed(self) -> bool:
        """True when Filter or Show leaves out rows the export kinds would include."""
        return bool(self._filter.text().strip()) or self._group.currentText() != ALL_GROUPS

    def _begin_selected(self) -> None:
        rows = self._selected_rows()
        if not rows or self._running() or self._pack.text().strip():
            return
        output = self._output_folder()
        if output is None:
            return
        exe = self._exe()
        if exe is None:
            return
        self._export_rows(output, exe, rows)

    def _export_rows(self, output: str, exe: str, rows: list[list[str]]) -> None:
        selection_file = self._session_dir("select") + ".txt"
        try:
            write_selection(selection_file, [row[NAME] for row in rows])
        except OSError as error:
            self._fail(f"Could not write the selection: {error}")
            return
        self._output = output
        self._new_options = True
        self._launch("export", exe, self._selection_arguments(output, selection_file, rows))

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
            old = self._new_options and any(unknown_option(line) for line in self._errors)
            self._status.setText(OLD_GAME_NEW if old else NO_DATA if no_data else "Export failed.")
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
        # The kinds too: a catalog scanned with other kinds lacks rows.
        stamp = f"{game}|{info.st_size}|{info.st_mtime_ns}|{SCAN_KINDS}"
        name = hashlib.sha1(stamp.encode("utf-8")).hexdigest()[:12]
        return os.path.join(self._cache_dir, f"catalog-{name}.csv") if self._cache_dir else ""

    def _load_cache(self) -> None:
        path = self._cache_file()
        if not path or not os.path.isfile(path):
            return
        try:
            self._set_catalog(read_catalog(path))
        except (OSError, ValueError, csv.Error):
            self._set_catalog([])
        self._refresh()

    def _set_catalog(self, rows: list[list[str]]) -> None:
        self._names = catalog_names(rows)
        self._catalog = [row for row in rows if row[KIND] != "name"]
        self._labelled = ""
        self._relabel()

    def _names_language(self) -> str:
        """Names follow Text language where the game has them, else English."""
        language = self._language.currentData() or "int"
        return language if language in self._names else "int"

    def _relabel(self, *_) -> None:
        language = self._names_language()
        if self._catalog and language != self._labelled:
            label_rows(self._catalog, self._names, language)
            self._labelled = language

    def _begin_scan(self) -> None:
        if self._running():
            return
        exe = self._exe()
        if exe is None:
            return
        self._scan_dir = self._session_dir("scan")
        self._launch("scan", exe, ["--export-assets", self._scan_dir, "--export-list", "--export-kinds", SCAN_KINDS])

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
            # Lists, but has no model or animation kinds.
            old_kinds = any(unknown_option(line) for line in self._errors)
            self._status.setText(OLD_GAME if old else NO_DATA if no_data else
                                 OLD_GAME_NEW if old_kinds else "Scan failed.")
            self._log.setPlainText("\n".join(self._errors[-20:]) or "No error message was printed.")
            self._log.show()
            return
        self._set_catalog(rows)
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
        boxes = (
            ("texture", self._textures), ("movie", self._movies),
            ("text", self._text), ("model", self._models), ("animation", self._animations),
        )
        return {name for name, box in boxes if box.isChecked()}, language, text

    def _refresh(self) -> None:
        self._timer.stop()
        if not self._catalog:
            self._model.set_rows([])
            self._count.setText(SCAN_HINT)
            return
        kinds, language, text = self._selection()
        group = self._group.currentText()
        rows = [
            row for row in self._catalog
            if row[KIND] in kinds
            and (group == ALL_GROUPS or row[GROUP] == group)
            and (not text or text in row[LOWER])
            and (row[KIND] != "text" or not language or row[LANGUAGE] == language)
        ]
        self._model.set_rows(rows)
        counts = {"texture": 0, "movie": 0, "text": 0, "model": 0, "animation": 0}
        for row in rows:
            counts[row[KIND]] += 1
        movies = "movie" if counts["movie"] == 1 else "movies"
        text = (f"{counts['texture']:,} textures, {counts['movie']:,} {movies}, {counts['text']:,} text files, "
                f"{counts['model']:,} models, {counts['animation']:,} animation sets will be exported")
        if not self._names and (counts["model"] or counts["animation"]):
            text += ". " + OLD_GAME_NAMES
        self._count.setText(text)
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
        # A slow preview for a row the user has left would hold up the new one.
        if self._helper_job is not None and self._play is None:
            wanted = self._wanted
            keys = () if wanted is None else (self._job(wanted)[0], self._preview_key(wanted))
            if self._helper_job[0] not in keys:
                self._stop_helper()
        self._pump()

    def _show_message(self, text: str) -> None:
        self._message.setText(text)
        self._pages.setCurrentIndex(0)

    def _show_movie(self, row: list[str], note: str) -> None:
        self._movie_info.setText(f"{row[NAME]}\n{catalog_size(row)}")
        self._movie_note.setText(note)
        self._play_button.setEnabled(self._play is None)
        self._pages.setCurrentIndex(3)

    def _show_model(self, row: list[str], note: str) -> None:
        if row[KIND] == "animation":
            detail = (f"{int(row[WIDTH] or 0):,} sequences, {int(row[HEIGHT] or 0):,} tracks, "
                      f"{int(row[ENTRIES] or 0):,} frames")
        else:
            detail = (f"{row[FORMAT]}\n{int(row[WIDTH] or 0):,} vertices, "
                      f"{int(row[HEIGHT] or 0):,} triangles, {row[ENTRIES]} bones")
        self._model_info.setText(f"{row[LABEL] or catalog_name(row)}\n{detail}\n{row[PATH]}")
        self._model_note.setText(note)
        self._open_model_button.setEnabled(self._play is None)
        self._pages.setCurrentIndex(4)

    def _show_preview(self, row: list[str], picture: str = "", message: str = "") -> None:
        pixmap = QPixmap(picture) if picture else None
        if picture and (pixmap is None or pixmap.isNull()):
            message, pixmap = "The preview picture could not be read.", None
        self._model_image.set_source(pixmap)
        if pixmap is not None:
            message = ("The character these animations move, standing still. Blender plays the animations."
                       if row[KIND] == "animation" else "")
        self._model_status.setText(message)

    def _job(self, row: list[str], preview: bool = False) -> tuple[tuple, list[str]]:
        """Cache key and exporter arguments that produce the row's files, or with
        preview its picture."""
        kind, path = row[KIND], row[PATH]
        if kind == "text":
            arguments = ["--export-kinds", "text", "--export-filter", path.lower()]
            if row[LANGUAGE]:
                arguments += ["--export-language", row[LANGUAGE]]
            return ("text", path, row[LANGUAGE]), arguments
        if kind in GLB_KINDS:  # The key is selected through a one-line file, see _start_helper.
            arguments = ["--export-kinds", KIND_ARGUMENT[kind]]
            if preview:
                return self._preview_key(row), arguments + ["--export-preview"]
            textures = kind == "model" and self._model_textures.isChecked()
            if textures:
                arguments.append("--export-model-textures")
            return (kind, row[NAME], textures), arguments
        name = "textures" if kind == "texture" else "movies"
        return (kind, path), ["--export-kinds", name, "--export-filter", path.lower()]

    @staticmethod
    def _preview_key(row: list[str]) -> tuple:
        return ("preview", row[NAME])

    def _game_stamp(self) -> str:
        """Changes when the game folder or its LostOdysseyRecomp.exe changes."""
        try:
            info = os.stat(os.path.join(self._game_dir, GAME_BINARY))
        except OSError:
            return ""
        return f"{os.path.normcase(os.path.abspath(self._game_dir))}|{info.st_size}|{info.st_mtime_ns}"

    def _kept_path(self, folder: str, *parts) -> str:
        """A file or folder in the plugin's data folder named after the game and
        parts, or empty without a data folder."""
        stamp = self._game_stamp()
        if not self._cache_dir or not stamp:
            return ""
        name = hashlib.sha1("|".join((stamp,) + tuple(str(p) for p in parts)).encode("utf-8")).hexdigest()[:16]
        return os.path.join(self._cache_dir, folder, name)

    def _prune_kept(self) -> None:
        """Keeps the newest preview pictures and recent "Export and show" copies."""
        if not self._cache_dir:
            return
        try:
            previews = os.path.join(self._cache_dir, "previews")
            if os.path.isdir(previews):
                files = sorted((entry for entry in os.scandir(previews) if entry.is_file()),
                               key=lambda entry: entry.stat().st_mtime, reverse=True)
                for entry in files[PREVIEW_KEEP:]:
                    os.remove(entry.path)
            exports = os.path.join(self._cache_dir, "exports")
            if os.path.isdir(exports):
                for entry in os.scandir(exports):
                    if time.time() - entry.stat().st_mtime > EXPORT_DAYS * 86400:
                        shutil.rmtree(entry.path, ignore_errors=True)
        except OSError:
            pass

    def _open_model_clicked(self) -> None:
        row = self._wanted
        if row is None or row[KIND] not in GLB_KINDS:
            return
        self._failed.pop(self._job(row)[0], None)
        self._play = row
        self._pump()

    def _play_clicked(self) -> None:
        row = self._wanted
        if row is None or row[KIND] != "movie":
            return
        self._failed.pop(self._job(row)[0], None)
        self._play = row
        self._pump()

    def _next_job(self) -> tuple[list[str], bool] | None:
        """Serves what is cached; returns the row that needs a helper run, if any,
        and whether the run draws its preview picture."""
        if self._play is not None:
            key = self._job(self._play)[0]
            if self._play[KIND] in GLB_KINDS and key not in self._done:
                kept = self._kept_path("exports", *key)
                if kept and os.path.isfile(os.path.join(kept, KIND_ARGUMENT[self._play[KIND]], "index.csv")):
                    self._done[key] = kept  # Exported in an earlier session.
            if key in self._done:
                if self._play[KIND] in GLB_KINDS:
                    self._open_model(self._play)
                else:
                    self._open_movie(self._play)
                self._play = None
            elif key not in self._failed:
                if self._wanted is not None and self._wanted[KIND] == "movie":
                    self._show_movie(self._wanted, "Loading preview...")
                elif self._wanted is not None and self._wanted[KIND] in GLB_KINDS:
                    self._show_model(self._wanted, "Exporting...")
                return self._play, False
            else:
                self._play = None
        row = self._wanted
        if row is None:
            return None
        key = self._job(row)[0]
        if row[KIND] == "movie":
            self._show_movie(row, self._failed.get(key, ""))
        elif row[KIND] in GLB_KINDS:
            self._show_model(row, self._failed.get(key) or self._saved.get(key, ""))
            picture = self._preview_key(row)
            kept = self._kept_path("previews", row[NAME])
            if picture not in self._done and kept and os.path.isfile(kept + ".png"):
                self._done[picture] = kept + ".png"
            if picture in self._done:
                self._show_preview(row, self._done[picture])
            elif picture in self._failed:
                self._show_preview(row, message=self._failed[picture])
            else:
                self._show_preview(row, message="Drawing a preview...")
                return row, True
        elif key in self._done:
            self._render(row, self._done[key])
        elif key in self._failed:
            self._show_message(self._failed[key])
        else:
            self._show_message("Loading preview...")
            return row, False
        return None

    def _pump(self) -> None:
        job = self._next_job()
        if job is not None and self._helper is None:
            self._start_helper(*job)

    def _start_helper(self, row: list[str], preview: bool = False) -> None:
        key, arguments = self._job(row, preview)
        folder = self._session_dir("job")
        if row[KIND] in GLB_KINDS and not preview:
            # Kept for later sessions: the user may still drag the file into Blender.
            kept = self._kept_path("exports", *key)
            if kept:
                shutil.rmtree(kept, ignore_errors=True)
                folder = kept
        if row[KIND] in GLB_KINDS:
            selection = self._session_dir("select") + ".txt"
            try:
                write_selection(selection, [row[NAME]])
            except OSError as error:
                self._failed[key] = f"Could not write the selection: {error}"
                self._pump()
                return
            arguments = arguments + ["--export-select", selection]
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
        self._helper_job = (key, folder, preview)
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
        key, folder, preview = self._helper_job
        self._helper = None
        self._helper_job = None
        process.deleteLater()
        if ok and preview:
            picture = exported_file(folder, "models", key[1]) or exported_file(folder, "animations", key[1])
            if picture:
                kept = self._kept_path("previews", key[1])
                try:
                    os.makedirs(os.path.dirname(kept), exist_ok=True)
                    shutil.copyfile(picture, kept + ".png")
                    picture = kept + ".png"
                except OSError:
                    pass  # Shown from the session folder instead.
                self._done[key] = picture
            else:
                self._failed[key] = "The game drew no preview for this row."
        elif ok:
            self._done[key] = folder
        else:
            no_data = any("no game data" in line for line in self._helper_errors)
            old = key[0] in GLB_KINDS + ("preview",) and any(unknown_option(line) for line in self._helper_errors)
            self._failed[key] = (
                (OLD_GAME_PREVIEW if preview else OLD_GAME_NEW) if old
                else NO_DATA if no_data else "\n".join(self._helper_errors[-10:])
            )
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

    def _open_model(self, row: list[str]) -> None:
        """Shows the exported .glb in Explorer, selected, to drag into Blender."""
        key = self._job(row)[0]
        path = exported_file(self._done[key], KIND_ARGUMENT[row[KIND]], row[NAME])
        if not path:
            base = os.path.join(self._done[key], KIND_ARGUMENT[row[KIND]])
            files = glob.glob(os.path.join(glob.escape(base), "**", "*.glb"), recursive=True)
            path = files[0] if files else ""
        if not path:
            self._failed[key] = "The exporter did not write this file."
            self._show_model(row, self._failed[key])
            return
        path = os.path.normpath(path)
        if os.name == "nt":
            QProcess.startDetached("explorer.exe", ["/select,", QDir.toNativeSeparators(path)])
        else:
            QDesktopServices.openUrl(QUrl.fromLocalFile(os.path.dirname(path)))
        self._saved[key] = (f"Saved as {path}\nDrag this file into Blender's window to open it "
                            f"(wiki page Open models in Blender).")
        self._show_model(row, self._saved[key])


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
        return "Exports the original textures, movies, text, models and animations from your own game data."

    def version(self) -> mobase.VersionInfo:
        return mobase.VersionInfo(1, 4, 0, 0)

    def requirements(self) -> list[mobase.IPluginRequirement]:
        return [mobase.PluginRequirementFactory.gameDependency(GAME_NAME)]

    def settings(self) -> list[mobase.PluginSetting]:
        return [mobase.PluginSetting(
            "game_folder", "The folder exports read, when it is not MO2's game folder (set with Change...)", ""
        )]

    def displayName(self) -> str:
        return "Export Lost Odyssey assets"

    def tooltip(self) -> str:
        return "Export the original textures, movies, text, models and animations to a folder for reference."

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
