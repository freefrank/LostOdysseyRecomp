# Mod Organizer 2 tool for LostOdysseyRecomp: export the original artwork.
#
# Copy this file to <MO2>/plugins/ and restart MO2. With a "Lost Odyssey Recomp"
# instance selected, the Tools menu gets "Export Lost Odyssey assets". It runs
# LostOdysseyRecomp.exe --export-assets in the game folder (outside MO2's
# virtual file system) and writes textures, movies and text from your own game data
# to a new folder. Use the result as reference for your mods; do not share it.

from __future__ import annotations

import os
import re

from PyQt6.QtCore import QProcess, QUrl
from PyQt6.QtGui import QDesktopServices, QIcon
from PyQt6.QtWidgets import (
    QCheckBox,
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
    QVBoxLayout,
)

import mobase

GAME_NAME = "Lost Odyssey Recomp"
GAME_BINARY = "LostOdysseyRecomp.exe"
PROGRESS = re.compile(r"^progress\s+(\d+)\s*/\s*(\d+)\s*$")


class ExportDialog(QDialog):
    def __init__(self, parent, game_dir: str, default_output: str):
        super().__init__(parent)
        self.setWindowTitle("Export Lost Odyssey assets")
        self.setMinimumWidth(520)
        self._game_dir = game_dir
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
        form = QFormLayout()
        form.addRow("Output folder", folder_row)
        form.addRow("Export", kinds)
        form.addRow("Filter", self._filter)

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
        buttons = QHBoxLayout()
        buttons.addStretch(1)
        buttons.addWidget(self._start)
        buttons.addWidget(self._open)
        buttons.addWidget(self._close)

        layout = QVBoxLayout(self)
        layout.addWidget(info)
        layout.addLayout(form)
        layout.addWidget(self._bar)
        layout.addWidget(self._status)
        layout.addWidget(self._log)
        layout.addLayout(buttons)

    def _browse(self) -> None:
        start = self._folder.text() or os.path.dirname(self._game_dir)
        chosen = QFileDialog.getExistingDirectory(self, "Output folder", start)
        if chosen:
            self._folder.setText(chosen)

    def _open_folder(self) -> None:
        if self._output:
            QDesktopServices.openUrl(QUrl.fromLocalFile(self._output))

    def _running(self) -> bool:
        return (
            self._process is not None
            and self._process.state() != QProcess.ProcessState.NotRunning
        )

    def _set_busy(self, busy: bool) -> None:
        for widget in (self._folder, self._textures, self._movies, self._text, self._filter, self._start):
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
        if not kinds:
            self._fail("Select at least one of textures, movies and text.")
            return
        exe = os.path.join(self._game_dir, GAME_BINARY)
        if not os.path.isfile(exe):
            self._fail(f"{GAME_BINARY} was not found in {self._game_dir}.")
            return
        arguments = ["--export-assets", output, "--export-kinds", ",".join(kinds)]
        if self._filter.text().strip():
            arguments += ["--export-filter", self._filter.text().strip()]

        self._output = output
        self._buffer = ""
        self._last_line = ""
        self._errors = []
        self._log.clear()
        self._log.hide()
        self._open.setEnabled(False)
        self._bar.setRange(0, 0)  # Busy until the first progress line arrives.
        self._status.setText("Exporting...")
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
                self._status.setText(f"Exporting... {done}/{total}")
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
        if ok:
            self._status.setText(message)
            self._open.setEnabled(os.path.isdir(self._output))
            if QMessageBox.question(
                self, "Export finished", message + "\n\nOpen the output folder?"
            ) == QMessageBox.StandardButton.Yes:
                self._open_folder()
        else:
            self._status.setText("Export failed.")
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
        super().reject()


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
        return mobase.VersionInfo(1, 0, 0, 0)

    def requirements(self) -> list[mobase.IPluginRequirement]:
        return [mobase.PluginRequirementFactory.gameDependency(GAME_NAME)]

    def settings(self) -> list[mobase.PluginSetting]:
        return []

    def displayName(self) -> str:
        return "Export Lost Odyssey assets"

    def tooltip(self) -> str:
        return "Export the original textures, movies and text to a folder for reference."

    def icon(self) -> QIcon:
        return QIcon()

    def display(self) -> None:
        parent = self._parentWidget()
        game = self._organizer.managedGame()
        if game.gameName() != GAME_NAME:
            QMessageBox.information(
                parent, "Export Lost Odyssey assets",
                f"This tool needs an instance of the game \"{GAME_NAME}\".",
            )
            return
        game_dir = game.gameDirectory().absolutePath()
        base = os.path.normpath(
            os.path.join(self._organizer.basePath(), "lost-odyssey-export")
        )
        default = base
        counter = 2
        while os.path.exists(default) and (not os.path.isdir(default) or os.listdir(default)):
            default = f"{base}-{counter}"
            counter += 1
        # Kept on self so Python does not collect the dialog while it is open.
        self._dialog = ExportDialog(parent, game_dir, default)
        self._dialog.exec()
        self._dialog = None


def createPlugin() -> mobase.IPlugin:
    return LostOdysseyRecompExport()
