"""Inventory -> Mod tool contract checks, using synthetic data and artwork only."""
from contextlib import closing, redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
import zipfile

from PIL import Image
from tools.asset_inventory import inventory
from tools.modding import lo_mod as mod

MENU = "bin/xenon/loc/int/menu/rpmenurescommon_int.xxx"
FONT = "bin/xenon/loc/int/menu/rpfontscommon_int.xxx"
SHA = "a" * 64
VARIANT = "b" * 64


class ModCatalogTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="lo-mod-catalog-")
        self.root = Path(self.temp.name)
        self.path = self.root / "目录.sqlite"
        with closing(sqlite3.connect(self.path)) as db:
            db.executescript(inventory.SCHEMA)
            db.executemany("INSERT INTO metadata VALUES (?,?)", [("schema_version", "1"), ("complete", "true")])
            db.commit()

    def tearDown(self):
        self.temp.cleanup()

    def asset(self, package=MENU, index=26, name="UI_MAIN_00", sha=SHA, width=512, height=1024,
              cls="Texture2D", outer=0, fmt=7, property_error="", key_error="", status="ok"):
        with closing(sqlite3.connect(self.path)) as db:
            db.execute("INSERT OR IGNORE INTO payloads(sha256,status) VALUES (?,?)", (sha, status))
            db.execute("""INSERT INTO exports(sha256,export_index,name,class_name,outer_ref,
                width,height,format,property_error) VALUES (?,?,?,?,?,?,?,?,?)""",
                       (sha, index, name, cls, outer, width, height, fmt, property_error))
            db.execute("INSERT INTO assets(path,sha256,export_index,mod_key,key_error) VALUES (?,?,?,?,?)",
                       (package, sha, index, mod.make_key(package, index, name), key_error))
            if not db.execute("SELECT 1 FROM files WHERE sha256=? AND path=?", (sha, package)).fetchone():
                db.executemany("INSERT INTO files(disc,archive,path,offset,length,sha256) VALUES (?,?,?,?,?,?)",
                               [(f"disc{n}", "xenon_loc.fpd", package, 2048 * n, 512, sha) for n in (1, 2)])
            db.commit()

    def cli(self, *args):
        output, error = io.StringIO(), io.StringIO()
        with redirect_stdout(output), redirect_stderr(error):
            code = mod.main(list(args))
        return code, output.getvalue(), error.getvalue()

    def init(self, *extra):
        return self.cli("init", "--database", str(self.path), "--object", "UI_MAIN_00",
                        "--image", "art.png", "--id", "catalog-demo", "--output", str(self.root / "mod.json"), *extra)

    def test_database_init_pack_round_trip(self):
        self.asset()
        before = self.path.read_bytes()
        items = mod.database_catalog(self.path, package=MENU.upper().replace("/", "\\"), runtime_only=True)
        self.assertEqual(len(items), 1)  # Repeated disc sources do not duplicate an asset.
        item = items[0]
        self.assertEqual(item["consumer"], "native_menu_atlas")
        self.assertEqual(item["sha256"], SHA)
        self.assertEqual(len(item["sources"]), 2)
        self.assertEqual(item["sources"][0]["offset"], 2048)
        self.assertEqual(item["image_path"], "")  # Inventory does not extract game artwork.
        self.assertEqual(self.init()[0], 0)
        self.assertEqual(self.path.read_bytes(), before)
        Image.new("RGBA", (512, 1024), (0x12, 0x34, 0x56, 0x78)).save(self.root / "art.png")
        for layout in ("standalone", "overlay"):
            archive_path = self.root / f"{layout}.zip"
            self.assertEqual(mod.pack(self.root / "mod.json", archive_path, layout), 1)
            with zipfile.ZipFile(archive_path) as archive:
                name = next(name for name in archive.namelist() if name.endswith(".lotex"))
                data = archive.read(name)
                self.assertEqual(mod.inspect(data), {"key": item["key"], "width": 512, "height": 1024,
                                                     "format": "RGBA8", "overlay_path": item["overlay_path"]})
                self.assertEqual(data[-4:], b"\x12\x34\x56\x78")
                if layout == "overlay":
                    self.assertEqual(name, "mods/" + item["overlay_path"])
                else:
                    self.assertEqual(name, "mods/catalog-demo/" + item["overlay_path"])
                    self.assertIn("api_version=2\n", archive.read("mods/catalog-demo/mod.ini").decode())

    def test_variants_require_explicit_selection_even_with_equal_dimensions(self):
        self.asset()
        self.asset(sha=VARIANT)
        self.assertEqual(len(mod.database_catalog(self.path)), 2)
        self.assertEqual(len(mod.database_catalog(self.path, limit=1)), 1)
        code, _, error = self.init()
        self.assertEqual(code, 2)
        self.assertIn("2 eligible resources/content variants", error)
        self.assertFalse((self.root / "mod.json").exists())
        self.assertEqual(self.init("--content-sha256", VARIANT.upper())[0], 0)
        with self.assertRaisesRegex(ValueError, "64-digit"):
            mod.database_catalog(self.path, content_sha256="abcd")

    def test_font_candidates_need_consumed_owner_and_texture_preconditions(self):
        # A realistic-looking page name is insufficient; native references are not indexed.
        self.asset(FONT, 0, "Maru23", cls="Font")
        self.asset(FONT, 1, "AnyPageName", outer=1)
        self.asset(FONT, 2, "Arial18", cls="Font")
        self.asset(FONT, 3, "Maru23_PageA", outer=3)
        self.asset(FONT, 4, "WrongFormat", outer=1, fmt=2)
        self.asset(FONT, 5, "WrongSize", outer=1, width=129)
        self.asset(FONT, 6, "LocTit1", cls="Font", property_error="broken native metadata")
        self.asset(FONT, 7, "BrokenOwner", outer=7)
        self.asset(FONT, 8, "TooSmall", outer=1, width=64)
        items = mod.database_catalog(self.path, runtime_only=True)
        self.assertEqual(len(items), 1)
        self.assertEqual(items[0]["consumer"], "native_font_page_candidate")
        self.assertTrue(items[0]["key"].endswith("#1:AnyPageName"))
        args = ("init", "--database", str(self.path), "--object", "AnyPageName", "--image", "art.png",
                "--id", "font-test", "--output", str(self.root / "font.json"))
        self.assertEqual(self.cli(*args)[0], 2)
        self.assertEqual(self.cli(*args, "--allow-unwired")[0], 0)

    def test_unwired_requires_opt_in_and_filtering_precedes_limit(self):
        self.asset("aaa/unwired.xxx")
        self.asset(sha=VARIANT)
        items = mod.database_catalog(self.path, runtime_only=True, limit=1)
        self.assertEqual(items[0]["consumer"], "native_menu_atlas")
        code, _, error = self.init("--package", "aaa/unwired.xxx")
        self.assertEqual(code, 2)
        self.assertIn("no current runtime consumer", error)
        self.assertEqual(self.init("--package", "aaa/unwired.xxx", "--allow-unwired")[0], 0)

    def test_bad_or_ineligible_metadata_is_not_a_supported_image(self):
        self.asset(index=0, cls="StaticMesh")
        self.asset(index=1, property_error="bad SizeX")
        self.asset(index=2, key_error="invalid object")
        self.asset(index=3, width=None)
        self.asset(index=4, sha=VARIANT, status="error")
        self.assertEqual(mod.database_catalog(self.path), [])
        self.asset(index=5)
        with closing(sqlite3.connect(self.path)) as db:
            db.execute("UPDATE assets SET mod_key='a.xxx#5:Wrong' WHERE export_index=5")
            db.commit()
        with self.assertRaisesRegex(ValueError, "inconsistent Mod key"):
            mod.database_catalog(self.path)
        with closing(sqlite3.connect(self.path)) as db:
            db.execute("UPDATE exports SET name='invalid:name' WHERE export_index=5")
            db.commit()
        with self.assertRaisesRegex(ValueError, "invalid identity"):
            mod.database_catalog(self.path)

    def test_missing_unknown_or_incomplete_database_fails_without_creation(self):
        missing = self.root / "missing.sqlite"
        code, _, error = self.cli("catalog", "--database", str(missing))
        self.assertEqual(code, 2)
        self.assertIn("unable to open", error)
        self.assertFalse(missing.exists())
        for version, complete in (("2", "true"), ("true", "true"), ("1", "false"), ("1", "1")):
            with closing(sqlite3.connect(self.path)) as db:
                db.executemany("UPDATE metadata SET value=? WHERE key=?",
                               [(version, "schema_version"), (complete, "complete")])
                db.commit()
            self.assertEqual(self.cli("catalog", "--database", str(self.path))[0], 2)

    def test_database_only_flags_are_not_silently_ignored(self):
        for flag, value in (("--content-sha256", SHA), ("--limit", "1")):
            code, _, error = self.cli("catalog", "--manifest", "unused.csv", flag, value)
            self.assertEqual(code, 2)
            self.assertIn("require --database", error)
        for limit in (0, 10001):
            self.assertEqual(self.cli("catalog", "--database", str(self.path), "--limit", str(limit))[0], 2)


if __name__ == "__main__":
    unittest.main()
