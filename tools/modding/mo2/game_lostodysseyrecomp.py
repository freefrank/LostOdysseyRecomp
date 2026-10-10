# Mod Organizer 2 support for LostOdysseyRecomp.
#
# Copy this file to <MO2>/plugins/basic_games/games/, restart MO2, create an
# instance for "Lost Odyssey Recomp" and browse to the folder that holds
# LostOdysseyRecomp.exe. MO2 maps every enabled mod onto the game's mods/
# folder; logs, settings, saves and caches stay outside the virtual folder.

from __future__ import annotations

import mobase

from ..basic_game import BasicGame


def _is_directory(entry: mobase.FileTreeEntry | None) -> bool:
    return entry is not None and entry.isDir()


def _is_mod_root(tree: mobase.IFileTree) -> bool:
    # overlay/ (manager-ordered files), or one folder per standalone mod.ini or
    # language pack (language.ini).
    folders = [entry for entry in tree if _is_directory(entry)]
    return bool(folders) and all(
        folder.name().casefold() == "overlay"
        or folder.exists("mod.ini")
        or folder.exists("language.ini")
        for folder in folders
    )


class LostOdysseyRecompModDataChecker(mobase.ModDataChecker):
    def dataLooksValid(
        self, filetree: mobase.IFileTree
    ) -> mobase.ModDataChecker.CheckReturn:
        if _is_mod_root(filetree):
            return mobase.ModDataChecker.VALID
        # Packages built by lo_mod.py keep the game's mods/ folder on top.
        mods = filetree.find("mods")
        if _is_directory(mods) and _is_mod_root(mods):
            return mobase.ModDataChecker.FIXABLE
        return mobase.ModDataChecker.INVALID

    def fix(self, filetree: mobase.IFileTree) -> mobase.IFileTree:
        mods = filetree.find("mods")
        if _is_directory(mods):
            filetree.merge(mods)
            mods.detach()
        return filetree


class LostOdysseyRecompGame(BasicGame):
    Name = "Lost Odyssey Recomp Support Plugin"
    Author = "dotSlash"
    Version = "1.1.0"
    Description = "Adds support for LostOdysseyRecomp mods."

    GameName = "Lost Odyssey Recomp"
    GameShortName = "lostodysseyrecomp"
    GameBinary = "LostOdysseyRecomp.exe"
    GameDataPath = "mods"
    GameSupportURL = (
        "https://github.com/freefrank/LostOdysseyRecomp/wiki/Mod-Organizer-2"
    )

    def init(self, organizer: mobase.IOrganizer) -> bool:
        super().init(organizer)
        self._register_feature(LostOdysseyRecompModDataChecker())
        return True
