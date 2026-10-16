#!/usr/bin/env python3
import contextlib
import csv
import importlib.util
import io
import json
import os
import shutil
import struct
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock
from PIL import Image

SPEC = importlib.util.spec_from_file_location("lo_mod", Path(__file__).parents[1] / "modding/lo_mod.py")
mod = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(mod)
KEY = "bin/xenon/loc/int/menu/test.xxx#21:Icon_Page_0"


class ModToolsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="lo-mod-tools-")
        self.root = Path(self.temp.name)
        self.spec = self.root / "mod.json"
        self.png = self.root / "art.png"
        Image.new("RGBA", (2, 1), (0x12, 0x34, 0x56, 0x78)).save(self.png)
        self.data = {"api_version": 1, "id": "test", "priority": 10,
                     "images": [{"key": KEY, "source": "art.png", "width": 2, "height": 1}]}
        self.save()

    def tearDown(self):
        self.temp.cleanup()

    def save(self):
        self.spec.write_text(json.dumps(self.data), encoding="utf-8")

    def test_keys(self):
        self.assertEqual(KEY, mod.make_key("BIN\\XENON//LOC/./int/menu/test.xxx", 21, "Icon_Page_0"))
        for package in ("", ".", "../a", "a/../b", "a/", "/a", "C:\\a", "a#b", " a"):
            with self.assertRaises(ValueError):
                mod.make_key(package, 0, "Object")
        self.assertEqual(mod.canonical_key(KEY.replace("#21:", "#00021:")), KEY)
        self.assertEqual(mod.overlay_path(KEY), "overlay/images/key-fnv1a64-5177565cdd08cbcf.lotex")
        self.assertEqual(mod.overlay_path("MV_00V", "movie"), "overlay/movies/mv_00v.mp4")
        for bad in ("", "../x", "mv.00v", "a/b", "a\b", "mv-00v", "a" * 65):
            with self.assertRaises(ValueError, msg=bad):
                mod.overlay_path(bad, "movie")
        mod.overlay_path("a" * 64, "movie")

    def test_language_clean(self):
        menu = "bin/xenon/loc/int/menu/menu_int.dat.json"
        pack, original = self.root / "pt-br", self.root / "original" / "text"
        for folder, values in ((pack / "text", {"a": "Estado", "b": "Item"}), (original, {"a": "Status", "b": "Item"})):
            for name, data in ((menu, values), ("bin/xenon/scr/mes/int/x.jmd.json", {"c": "Same"})):
                (folder / name).parent.mkdir(parents=True, exist_ok=True)
                (folder / name).write_text(json.dumps(data), encoding="utf-8")
        (pack / "language.ini").write_text("id=pt-br\nname=Português\nbase=int\n", encoding="utf-8")
        glyphs = pack / "fonts/bin/xenon/loc/int/menu/rpfontscommon_int.xxx/Maru23.lofont"
        glyphs.parent.mkdir(parents=True)
        glyphs.write_bytes(b"glyphs")
        stats = mod.language_clean(pack, None, None)
        share = self.root / "share" / "pt-br"
        self.assertEqual((stats["files"], stats["entries"], stats["dropped_files"]), (1, 1, 1))
        self.assertEqual(json.loads((share / "text" / menu).read_text(encoding="utf-8")), {"a": "Estado"})
        self.assertFalse((share / "text/bin/xenon/scr").exists())
        self.assertTrue((share / "language.ini").is_file())
        # The pack's own glyphs go with it.
        self.assertEqual((share / glyphs.relative_to(pack)).read_bytes(), b"glyphs")
        with self.assertRaises(FileExistsError):
            mod.language_clean(pack, None, None)

    def test_shaping_input(self):
        zwj = "\u200d"
        self.assertEqual(mod.shaping_input("\ufe91"), ("\u0628" + zwj, True, False))        # beh initial
        self.assertEqual(mod.shaping_input("\ufe92"), (zwj + "\u0628" + zwj, True, True))   # beh medial
        self.assertEqual(mod.shaping_input("\ufe90"), (zwj + "\u0628", False, True))        # beh final
        self.assertEqual(mod.shaping_input("\ufe8f"), ("\u0628", False, False))             # beh isolated
        self.assertEqual(mod.shaping_input("\ufefc"), (zwj + "\u0644\u0627", False, True))  # lam-alef final
        self.assertEqual(mod.shaping_input("\u0640"), ("\u0640", True, True))               # tatweel
        self.assertEqual(mod.shaping_input("A"), ("A", False, False))

    def test_font_glyphs(self):
        """A pack's letters the export's font lacks become one LOFONT1 file with outlined glyphs."""
        try:
            import freetype  # noqa: F401
            import uharfbuzz  # noqa: F401
        except ImportError:
            self.skipTest("font-glyphs needs freetype-py and uharfbuzz")
        candidates = [os.environ.get("LO_TEST_TTF", ""), "C:/Windows/Fonts/arial.ttf",
                      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf"]
        ttf = next((Path(c) for c in candidates if c and Path(c).is_file()), None)
        if not ttf:
            self.skipTest("no TrueType font with Arabic letters (set LO_TEST_TTF)")
        # An export with one font: H and I drawn white with a one-pixel black outline.
        package = "bin/xenon/loc/int/menu/rpfontscommon_int.xxx"
        export, pack = self.root / "export", self.root / "ar"
        page = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
        for x0 in (0, 16):
            page.paste((0, 0, 0, 255), (x0 + 2, 4, x0 + 12, 24))
            page.paste((255, 255, 255, 255), (x0 + 3, 5, x0 + 11, 23))
        (export / "textures" / package).mkdir(parents=True)
        page.save(export / "textures" / package / "Maru23_PageA.9.png")
        meta = {"package": package, "font": "Maru23", "export_index": 6, "kerning": -2,
                "pages": [{"export_index": 9, "name": "Maru23_PageA", "width": 64, "height": 32, "format": 7,
                           "png": package + "/Maru23_PageA.9.png"}],
                "glyphs": [[0x48, 0, 0, 14, 28, 0], [0x49, 16, 0, 14, 28, 0], [0x20, 32, 0, 8, 28, 0]]}
        (export / "fonts" / package).mkdir(parents=True)
        (export / "fonts" / package / "Maru23.json").write_text(json.dumps(meta), encoding="utf-8")
        (pack / "text").mkdir(parents=True)
        (pack / "language.ini").write_text("id=ar\nname=Arabic\nbase=int\n", encoding="utf-8")
        # Beh initial, medial and final, lam-alef; H and the space are in the font already.
        (pack / "text/x.json").write_text(json.dumps({"a": "\ufe91\ufe92\ufe90 \ufefb{E001}H"}), encoding="utf-8")
        rows = mod.font_glyphs(export, pack, ttf)
        self.assertEqual([(r["font"], r["glyphs"]) for r in rows], [("Maru23", 4)])
        data = (pack / "fonts" / package / "Maru23.lofont").read_bytes()
        magic, header, count, width, height, zero = mod.LOFONT_HEADER.unpack_from(data)
        self.assertEqual((magic, header, count, width, zero), (mod.LOFONT_MAGIC, 32, 4, 64, 0))
        glyphs = [mod.LOFONT_GLYPH.unpack_from(data, 32 + 12 * i) for i in range(count)]
        self.assertEqual(sorted(g[0] for g in glyphs), [0xFE90, 0xFE91, 0xFE92, 0xFEFB])
        self.assertTrue(all(g[4] == 28 and g[1] + g[3] <= width and g[2] + g[4] <= height for g in glyphs))
        self.assertEqual(len(data), 32 + 12 * count + width * height * 4)
        atlas = Image.frombytes("RGBA", (width, height), data[32 + 12 * count:])
        medial = next(g for g in glyphs if g[0] == 0xFE92)
        cell = atlas.crop((medial[1], medial[2], medial[1] + medial[3], medial[2] + medial[4]))
        # The medial form joins on both sides: ink reaches the left edge and the advance on the right
        # (the cell is two pixels wider than the advance for the font's kerning of -2).
        alpha = cell.getchannel("A")
        self.assertTrue(any(alpha.getpixel((0, y)) for y in range(cell.height)))
        self.assertTrue(any(alpha.getpixel((cell.width - 3, y)) for y in range(cell.height)))
        self.assertFalse(any(alpha.getpixel((cell.width - 1, y)) for y in range(cell.height)))

    def test_utf8_case(self):
        key = mod.make_key("BIN/纹理.xxx", 3, "图集")
        self.assertEqual(key, "bin/纹理.xxx#3:图集")
        self.assertEqual(mod.inspect(mod.encode(key, 1, 1, bytes(4)))["key"], key)

    def test_lotex(self):
        raw = b"\x12\x34\x56\x78" * 2
        data = mod.encode(KEY, 2, 1, raw)
        self.assertEqual(mod.inspect(data)["width"], 2)
        self.assertEqual(data[-8:], raw)
        self.assertEqual(data[:24], mod.HEADER.pack(mod.MAGIC, 2, 1, len(KEY.encode()), 1))
        for bad in (b"", data[:-1], data + b"x", b"?" + data[1:], data[:20] + b"\x02" + data[21:]):
            with self.assertRaises(ValueError):
                mod.inspect(bad)

    def test_standalone(self):
        output = self.root / "standalone.zip"
        self.assertEqual(mod.pack(self.spec, output, "standalone"), 1)
        with zipfile.ZipFile(output) as archive:
            # Mod folder (format v2): files under the mod's own overlay/, found by name.
            self.assertEqual(sorted(archive.namelist()), ["mods/test/mod.ini", "mods/test/" + mod.overlay_path(KEY)])
            self.assertEqual(archive.read("mods/test/mod.ini").decode(),
                             "api_version=2\nid=test\npriority=10\nenabled=true\n")
            self.assertEqual(mod.inspect(archive.read("mods/test/" + mod.overlay_path(KEY)))["key"], KEY)
        with self.assertRaises(FileExistsError):
            mod.pack(self.spec, output, "standalone")
        self.assertTrue(output.exists())

    def test_metadata(self):
        self.data.update({"name": "Menu art", "author": "spec author"})
        self.save()
        output = self.root / "meta.zip"
        self.assertEqual(mod.main(["pack", str(self.spec), "--output", str(output), "--author", "Ünïcode 作者",
                                   "--version", "1.2"]), 0)
        with zipfile.ZipFile(output) as archive:
            self.assertEqual(archive.read("mods/test/mod.ini").decode(),
                             "api_version=2\nid=test\nname=Menu art\nversion=1.2\nauthor=Ünïcode 作者\npriority=10\nenabled=true\n")
        for field, value in (("name", "n" * 129), ("description", "a\nb"), ("author", " "), ("version", "\x85")):
            with self.assertRaises(ValueError):
                mod.manifest("x", 0, {field: value})
        self.assertIn("name=" + "é" * 64 + "\n", mod.manifest("x", 0, {"name": "é" * 64}))  # 128 bytes

    def test_overlay(self):
        output = self.root / "overlay.zip"
        mod.pack(self.spec, output, "overlay")
        with zipfile.ZipFile(output) as archive:
            self.assertEqual(archive.namelist(), ["mods/" + mod.overlay_path(KEY)])
        second = self.root / "second.zip"
        mod.pack(self.spec, second, "overlay")
        self.assertEqual(output.read_bytes(), second.read_bytes())

    def test_size_mismatch_cleans_output(self):
        self.data["images"][0]["width"] = 3
        self.save()
        output = self.root / "invalid.zip"
        with self.assertRaises(ValueError):
            mod.pack(self.spec, output, "overlay")
        self.assertFalse(output.exists())

    def test_duplicate_key(self):
        self.data["images"] *= 2
        self.save()
        with self.assertRaises(ValueError):
            mod.pack(self.spec, self.root / "duplicate.zip", "overlay")

    def test_reject_traversal_and_bool_version(self):
        self.data["images"][0]["source"] = "../art.png"
        self.save()
        with self.assertRaises(ValueError):
            mod.pack(self.spec, self.root / "bad.zip", "overlay")
        self.data["api_version"] = True
        self.save()
        with self.assertRaises(ValueError):
            mod.pack(self.spec, self.root / "bad-version.zip", "overlay")

    def test_catalog_and_init(self):
        manifest = self.root / "manifest.csv"
        fields = ["package", "export_index", "object", "status", "cls", "width", "height", "image_path"]
        with manifest.open("w", newline="", encoding="utf-8-sig") as file:
            writer = csv.DictWriter(file, fieldnames=fields)
            writer.writeheader()
            row = dict(zip(fields, ["bin/xenon/loc/int/menu/test.xxx", "21", "Icon_Page_0", "exported", "Texture2D", "2", "1", "art.png"]))
            writer.writerow(row)
            writer.writerow(row)  # Shared key on another disc is de-duplicated.
            writer.writerow({**row, "status": "failed"})
        self.assertEqual(len(mod.catalog(manifest)), 1)
        output = self.root / "initialized.json"
        self.assertEqual(mod.main(["init", "--manifest", str(manifest), "--object", "Icon_Page_0",
                                   "--image", "art.png", "--id", "example", "--output", str(output)]), 0)
        self.assertEqual(json.loads(output.read_text())["images"][0]["key"], KEY)
        self.assertEqual(mod.pack(output, self.root / "initialized.zip", "standalone"), 1)

    def test_init_from_export_index(self):
        textures = self.root / "export" / "textures"
        atlas_key = "bin/xenon/loc/int/menu/rpmenurescommon_int.xxx#5:UI_MAIN_00"
        (textures / "pkg").mkdir(parents=True)
        Image.new("RGBA", (512, 1024), (1, 2, 3, 4)).save(textures / "pkg" / "UI_MAIN_00.5.png")
        Image.new("RGBA", (2, 1)).save(textures / "pkg" / "Icon_Page_0.21.png")
        index = textures / "index.csv"
        index.write_text("key,file,width,height,format\n"
                         f"{atlas_key},pkg/UI_MAIN_00.5.png,512,1024,DXT5\n"
                         f"{KEY},pkg/Icon_Page_0.21.png,2,1,A8R8G8B8\n", encoding="utf-8")
        output = self.root / "from-export" / "mod.json"
        args = ["init", "--export-index", str(index), "--id", "export", "--output", str(output)]
        self.assertEqual(mod.main(args + ["--object", "UI_MAIN_00"]), 0)
        art = output.parent / "art" / "UI_MAIN_00.png"
        self.assertEqual(art.read_bytes(), (textures / "pkg" / "UI_MAIN_00.5.png").read_bytes())
        image = json.loads(output.read_text())["images"][0]
        self.assertEqual((image["key"], image["source"], image["width"], image["height"]),
                         (atlas_key, "art/UI_MAIN_00.png", 512, 1024))
        self.assertEqual(mod.main(args + ["--object", "UI_MAIN_00"]), 2)  # Never overwrites.
        other = self.root / "other" / "mod.json"
        args[-1] = str(other)
        self.assertEqual(mod.main(args + ["--object", "Icon_Page_0"]), 2)  # No runtime consumer.
        self.assertFalse((other.parent / "art").exists())
        self.assertEqual(mod.main(args + ["--object", "Icon_Page_0", "--allow-unwired"]), 0)
        self.assertEqual(mod.main(args + ["--object", "Missing"]), 2)

    def test_texture_pack(self):
        textures = self.root / "export" / "textures"
        (textures / "pkg").mkdir(parents=True)
        Image.new("RGBA", (2, 2), (100, 100, 100, 200)).save(textures / "pkg" / "Icon_Page_0.21.png")
        index = textures / "index.csv"
        index.write_text("key,file,width,height,format,fingerprint,fingerprint_tiled\n"
                         f"{KEY},pkg/Icon_Page_0.21.png,2,2,DXT5,00000000000000ab,0\n"
                         f"{KEY.replace('#21', '#22')},pkg/Icon_Page_0.21.png,2,2,DXT5,00000000000000ab,0\n",
                         encoding="utf-8")
        images = ["--index", str(index), "--images", str(textures), "--images-index", str(index)]
        out = self.root / "packs"
        self.assertEqual(mod.main(["texture-pack", *images, "--output", str(out), "--mips"]), 0)
        data = (out / "overlay" / "textures" / "fp-00000000000000ab.lotex2").read_bytes()
        info = mod.inspect(data)
        self.assertEqual((info["fingerprint"], info["original_format"], info["scale"], info["mips"], info["key"]),
                         ("00000000000000ab", "DXT5", 1, 2, KEY))
        self.assertEqual(len(data), 64 + len(KEY) + 16 + 4)
        self.assertEqual(data[:8], b"LOTEX2\r\n")
        self.assertEqual(mod.main(["texture-pack", *images, "--output", str(out)]), 0)  # Never overwrites.
        self.assertEqual(len(list((out / "overlay" / "textures").iterdir())), 1)
        out4 = self.root / "nearest4"
        self.assertEqual(mod.main(["texture-pack", *images, "--output", str(out4), "--layout", "standalone",
                                   "--id", "t4", "--test", "nearest4", "--name", "Nearest 4x"]), 0)
        info = mod.inspect((out4 / "t4" / "overlay" / "textures" / "fp-00000000000000ab.lotex2").read_bytes())
        self.assertEqual((info["scale"], info["payload"]), (4, "8x8"))
        ini = (out4 / "t4" / "mod.ini").read_text()
        self.assertEqual(ini, "api_version=2\nid=t4\nname=Nearest 4x\npriority=0\nenabled=true\n")
        # Adding to the folder keeps its mod.ini; a v1 mod.ini is refused (v1 mods do not load overlay/).
        (out4 / "t4" / "overlay" / "textures" / "fp-00000000000000ab.lotex2").unlink()
        self.assertEqual(mod.main(["texture-pack", *images, "--output", str(out4), "--layout", "standalone",
                                   "--id", "t4", "--name", "Other"]), 0)
        self.assertEqual((out4 / "t4" / "mod.ini").read_text(), ini)
        (out4 / "v1").mkdir()
        (out4 / "v1" / "mod.ini").write_text("api_version=1\nid=v1\n")
        self.assertEqual(mod.main(["texture-pack", *images, "--output", str(out4), "--layout", "standalone", "--id", "v1"]), 2)
        self.assertFalse((out4 / "v1" / "overlay").exists())
        self.assertEqual(mod.main(["texture-pack", *images, "--output", str(out4), "--name", "x"]), 2)  # overlay layout
        with self.assertRaises(ValueError):
            mod.inspect(data + b"\0")

    def make_overlay(self) -> Path:
        mods = self.root / "mods"
        (mods / "overlay" / "textures").mkdir(parents=True)
        (mods / "overlay" / "textures" / "fp-00000000000000ab.lotex2").write_bytes(b"tex")
        (mods / "overlay" / "text" / "bin").mkdir(parents=True)
        (mods / "overlay" / "text" / "bin" / "a.dat.json").write_text("{}")
        return mods

    def test_overlay_to_mod(self):
        mods = self.make_overlay()
        args = ["overlay-to-mod", str(mods), "--id", "hd", "--name", "4x HD Texture Pack", "--version", "1.0"]
        self.assertEqual(mod.main(args + ["--dry-run"]), 0)
        self.assertFalse((mods / "hd").exists())
        self.assertTrue((mods / "overlay" / "textures" / "fp-00000000000000ab.lotex2").exists())
        self.assertEqual(mod.main(args), 0)
        self.assertEqual((mods / "hd" / "overlay" / "textures" / "fp-00000000000000ab.lotex2").read_bytes(), b"tex")
        self.assertTrue((mods / "hd" / "overlay" / "text" / "bin" / "a.dat.json").exists())
        self.assertEqual((mods / "hd" / "mod.ini").read_text(),
                         "api_version=2\nid=hd\nname=4x HD Texture Pack\nversion=1.0\npriority=0\nenabled=true\n")
        self.assertFalse((mods / "overlay").exists())  # Empty after the move.
        # Refusals: the mod folder exists, no overlay/, reserved id, nothing to move, bad metadata.
        self.make_overlay()
        self.assertEqual(mod.main(args), 2)
        self.assertEqual(mod.main(["overlay-to-mod", str(self.root / "missing"), "--id", "x"]), 2)
        self.assertEqual(mod.main(["overlay-to-mod", str(mods), "--id", "overlay"]), 2)
        self.assertEqual(mod.main(["overlay-to-mod", str(mods), "--id", "x", "--name", "n" * 129]), 2)
        empty = self.root / "empty"
        (empty / "overlay" / "other").mkdir(parents=True)
        self.assertEqual(mod.main(["overlay-to-mod", str(empty), "--id", "x"]), 2)
        self.assertFalse((mods / "x").exists() or (empty / "x").exists())

    def test_overlay_to_mod_rolls_back(self):
        mods = self.make_overlay()
        real = os.rename
        calls = []

        def fail_second(old, new):
            calls.append(old)
            if len(calls) == 2:
                raise OSError("simulated failure")
            real(old, new)
        with mock.patch.object(mod.os, "rename", fail_second):
            with self.assertRaises(OSError):
                mod.overlay_to_mod(mods, "hd")
        self.assertFalse((mods / "hd").exists())
        self.assertTrue((mods / "overlay" / "textures" / "fp-00000000000000ab.lotex2").exists())
        self.assertTrue((mods / "overlay" / "text" / "bin" / "a.dat.json").exists())

    def run_tool(self, *args) -> tuple[int, str, str]:
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = mod.main([str(a) for a in args])
        return code, out.getvalue(), err.getvalue()

    def make_archive_mod(self) -> Path:
        src = self.root / "src"
        files = {
            "mod.ini": b"api_version=2\nid=big\n",
            "README.txt": b"readme",
            "docs/notes.txt": b"notes",
            "overlay/Textures/FP-00000000000000AB.LOTEX2":
                mod.encode2(0xab, 6, (2, 2), (2, 2), [bytes(range(16))], KEY),
            "overlay/images/" + mod.overlay_path(KEY).rpartition("/")[2]: mod.encode(KEY, 2, 1, bytes(8)),
            "overlay/fonts/key-fnv1a64-0000000000000001.loasset": b"",
            "overlay/Text/Bin/Menu/A.Dat.JSON": b'{"a": "b"}',
            "overlay/images/readme.txt": b"not an asset",
        }
        for name, data in files.items():
            (src / name).parent.mkdir(parents=True, exist_ok=True)
            (src / name).write_bytes(data)
        return src

    def parse_archive(self, data: bytes) -> tuple[tuple, list[tuple[str, int, int]]]:
        head = struct.unpack_from("<8sIIQQI", data)
        pos, entries = head[3], []
        for _ in range(head[2]):
            offset, size, length = struct.unpack_from("<QQI", data, pos)
            entries.append((data[pos + 20:pos + 20 + length].decode(), offset, size))
            pos += 20 + length
        self.assertEqual(pos, head[3] + head[4])
        return head, entries

    def test_archive_round_trip(self):
        src, dest = self.make_archive_mod(), self.root / "dest"
        code, out, err = self.run_tool("archive", src, dest)
        self.assertEqual(code, 0, err)
        self.assertIn("4 entries", out)
        self.assertIn("readme.txt", err)  # Skipped with a warning.
        self.assertFalse((dest / "overlay.loarc.tmp").exists())
        self.assertFalse((dest / "overlay").exists())
        for name in ("mod.ini", "README.txt", "docs/notes.txt"):
            self.assertEqual((dest / name).read_bytes(), (src / name).read_bytes())
        data = (dest / "overlay.loarc").read_bytes()
        head, entries = self.parse_archive(data)
        self.assertEqual(head[:3], (b"LOARC1\r\n", 64, 4))
        self.assertEqual(head[5], 1 << 1 | 1 << 2 | 1 << 5 | 1 << 6)
        self.assertEqual(data[36:64], bytes(28))
        names = [e[0] for e in entries]
        self.assertEqual(names, sorted(names, key=str.encode))
        self.assertEqual(names, ["overlay/fonts/key-fnv1a64-0000000000000001.loasset", mod.overlay_path(KEY),
                                 "overlay/text/bin/menu/a.dat.json", "overlay/textures/fp-00000000000000ab.lotex2"])
        for name, offset, size in entries:
            self.assertTrue(offset % 4096 == 0 and offset >= 4096 and offset + size <= head[3], name)
        self.assertEqual(head[3], max(o + s for _, o, s in entries))
        # Extract round trip and --list.
        out_dir = self.root / "out"
        code, listing, _ = self.run_tool("extract", dest / "overlay.loarc", out_dir, "--list")
        self.assertEqual((code, out_dir.exists()), (0, False))
        self.assertEqual([line.split("\t") for line in listing.splitlines()], [[n, str(s)] for n, _, s in entries])
        self.assertEqual(self.run_tool("extract", dest / "overlay.loarc", out_dir)[0], 0)
        for source, name in (("overlay/Textures/FP-00000000000000AB.LOTEX2", "overlay/textures/fp-00000000000000ab.lotex2"),
                             ("overlay/Text/Bin/Menu/A.Dat.JSON", "overlay/text/bin/menu/a.dat.json"),
                             ("overlay/fonts/key-fnv1a64-0000000000000001.loasset",) * 2):
            self.assertEqual((out_dir / name).read_bytes(), (src / source).read_bytes())
        self.assertEqual(mod.inspect((out_dir / "overlay/textures/fp-00000000000000ab.lotex2").read_bytes())["key"], KEY)
        self.assertEqual(self.run_tool("extract", dest / "overlay.loarc", out_dir)[0], 2)  # Never overwrites.

    def test_archive_refusals(self):
        src, dest = self.make_archive_mod(), self.root / "dest"
        self.assertEqual(self.run_tool("archive", src, dest, "--quick")[0], 0)
        self.assertEqual(self.run_tool("archive", src, dest)[0], 2)  # Not empty.
        self.assertEqual(self.run_tool("archive", src, dest, "--force")[0], 0)
        self.assertEqual(self.run_tool("archive", src, src / "inside")[0], 2)
        (src / "mod.ini").write_text("api_version=1\nid=big\n")
        self.assertEqual(self.run_tool("archive", src, self.root / "v1")[0], 2)
        self.assertFalse((self.root / "v1").exists())

    def test_archive_collision(self):
        probe = self.root / "Probe"
        probe.mkdir()
        (self.root / "probe").mkdir(exist_ok=True)
        if len(list(self.root.glob("[Pp]robe"))) < 2:
            self.skipTest("case-insensitive filesystem")
        src = self.root / "case"
        (src / "overlay" / "textures").mkdir(parents=True)
        (src / "mod.ini").write_text("api_version=2\nid=case\n")
        for name in ("FP-00000000000000AB.lotex2", "fp-00000000000000ab.lotex2"):
            (src / "overlay" / "textures" / name).write_bytes(b"x")
        dest = self.root / "case-out"
        code, _, err = self.run_tool("archive", src, dest)
        self.assertEqual(code, 2)
        self.assertIn("both map to", err)
        self.assertFalse((dest / "overlay.loarc").exists() or (dest / "overlay.loarc.tmp").exists())

    def test_archive_rejects_corruption(self):
        src, dest = self.make_archive_mod(), self.root / "dest"
        self.assertEqual(self.run_tool("archive", src, dest)[0], 0)
        good = (dest / "overlay.loarc").read_bytes()
        head, entries = self.parse_archive(good)
        records, pos = [], head[3]
        for name, _, _ in entries:
            length = 20 + len(name.encode())
            records.append(good[pos:pos + length])
            pos += length

        def with_index(blob: bytes) -> bytes:
            return good[:head[3]] + blob

        sizes = [e[2] for e in entries]
        first, second = [i for i, s in enumerate(sizes) if s][:2]
        overlap = bytearray(with_index(b"".join(records)))
        struct.pack_into("<Q", overlap, head[3] + sum(map(len, records[:second])), entries[first][1])
        bad_mask = bytearray(good)
        struct.pack_into("<I", bad_mask, 32, head[5] | 1 << 3)
        unaligned = bytearray(good)
        struct.pack_into("<Q", unaligned, head[3], entries[0][1] + 1)
        cases = {
            "magic": b"LOARC2\r\n" + good[8:],
            "unsorted": with_index(b"".join(reversed(records))),
            "overlap": bytes(overlap),
            "kind_mask": bytes(bad_mask),
            "unaligned": bytes(unaligned),
            "truncated": good[:-3],
        }
        for label, data in cases.items():
            bad = self.root / f"bad-{label}.loarc"
            bad.write_bytes(data)
            out_dir = self.root / f"bad-{label}"
            code, _, err = self.run_tool("extract", bad, out_dir)
            self.assertEqual(code, 2, label)
            self.assertFalse(out_dir.exists(), label)
        # The deep check compares data with what was hashed from the source.
        entries_by_name = mod.loarc_scan(src / "overlay")
        digests, _ = mod.loarc_write(entries_by_name, self.root / "deep.loarc")
        mod.loarc_read(self.root / "deep.loarc", digests)
        flipped = bytearray((self.root / "deep.loarc").read_bytes())
        flipped[4096] ^= 1
        (self.root / "deep.loarc").write_bytes(flipped)
        with self.assertRaises(ValueError):
            mod.loarc_read(self.root / "deep.loarc", digests)

    def test_archive_names(self):
        for name in ("overlay/text/a/b.json", "overlay/textures/fp-0123456789abcdef.lotex2",
                     "overlay/models/bin/xenon/chr/pc/pc_000a0_m.xxx/pc_000a0_m.0.glb", "overlay/text/ü.json",
                     "overlay/movies/mv_00v.mp4", "overlay/movies/" + "a" * 64 + ".mp4"):
            mod.loarc_kind(name)
        for name in ("overlay/text/A.json", "overlay/text/a.JSON", "overlay/text/../a.json", "overlay/text/a//b.json",
                     "overlay/text/c:/a.json", "overlay/text/a\\b.json", "overlay/text/.json", "overlay/text/" + "a" * 1025 + ".json",
                     "overlay/textures/FP-0123456789abcdef.lotex2", "overlay/images/key-fnv1a64-0123456789abcdef.loasset",
                     "overlay/fonts/sub/key-fnv1a64-0123456789abcdef.loasset", "other/a", "/overlay/text/a.json",
                     "overlay/movies/MV_00V.mp4", "overlay/movies/mv.00v.mp4", "overlay/movies/.mp4",
                     "overlay/movies/" + "a" * 65 + ".mp4", "overlay/movies/mv_00v.loasset",
                     "overlay/movies/key-fnv1a64-0123456789abcdef.loasset", "overlay/movies/sub/mv_00v.mp4",
                     "overlay/models/key-fnv1a64-0123456789abcdef.loasset", "overlay/models/pc_000a0_m.0.glb",
                     "overlay/models/a.xxx/pc_000a0_m.glb", "overlay/models/a.xxx/PC_000A0_M.0.glb",
                     "overlay/models/a.xxx/../pc_000a0_m.0.glb"):
            with self.assertRaises(ValueError, msg=name):
                mod.loarc_kind(name)

    @staticmethod
    def mp4(order: tuple[bytes, ...]) -> bytes:
        return b"".join(struct.pack(">I4s", 8 + len(body), kind) + body for kind, body in
                        ((b"ftyp", b"isom"), *((kind, b"xx") for kind in order)))

    def test_movie_atom_order(self):
        front, back, none = self.root / "front.mp4", self.root / "back.mp4", self.root / "none.mp4"
        front.write_bytes(self.mp4((b"moov", b"mdat")))
        back.write_bytes(self.mp4((b"mdat", b"moov")))
        none.write_bytes(self.mp4((b"free",)))
        moov, mdat = mod.mp4_atom_order(front)
        self.assertTrue(moov < mdat)
        moov, mdat = mod.mp4_atom_order(back)
        self.assertTrue(moov > mdat)
        self.assertEqual(mod.mp4_atom_order(none), (None, None))
        with self.assertRaises(ValueError):
            mod.check_movie(none)
        with mock.patch.object(mod.shutil, "which", return_value=None):
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                mod.check_movie(front)
            self.assertIn("ffprobe not found", err.getvalue())
            self.assertNotIn("moov", err.getvalue())
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                mod.check_movie(back)
            self.assertIn("faststart", err.getvalue())
            # Archive: a movie that is not an MP4 is skipped with a warning; a valid name is stored.
            src = self.root / "msrc"
            for name, data in (("mv_00v.mp4", front.read_bytes()), ("MV_01V.MP4", back.read_bytes()), ("mv_02v.mp4", b"junk")):
                (src / "overlay/movies" / name).parent.mkdir(parents=True, exist_ok=True)
                (src / "overlay/movies" / name).write_bytes(data)
            err = io.StringIO()
            with contextlib.redirect_stderr(err):
                found = mod.loarc_scan(src / "overlay")
            self.assertEqual(sorted(found), ["overlay/movies/mv_00v.mp4", "overlay/movies/mv_01v.mp4"])
            self.assertIn("mv_02v.mp4", err.getvalue())

    @unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"), "ffmpeg not available")
    def test_movie_ffprobe(self):
        def make(name: str, rate: str, size: str, *extra: str) -> Path:
            path = self.root / name
            call = ["ffmpeg", "-v", "error", "-f", "lavfi", "-i", f"color=c=red:s={size}:r={rate}:d=0.5", "-an",
                    *extra, "-movflags", "+faststart", str(path)]
            subprocess.run(call, check=True)
            return path
        err = io.StringIO()
        with contextlib.redirect_stderr(err):
            mod.check_movie(make("good.mp4", "60000/1001", "640x360", "-c:v", "libx264", "-pix_fmt", "yuv420p"))
            mod.check_movie(make("good60.mp4", "60", "640x360", "-c:v", "libx264", "-pix_fmt", "yuv420p"))
        self.assertEqual(err.getvalue(), "")
        with contextlib.redirect_stderr(err):
            mod.check_movie(make("odd.mp4", "20", "640x480", "-c:v", "libx264", "-pix_fmt", "yuv420p"))
        self.assertIn("frame rate", err.getvalue())
        self.assertIn("16:9", err.getvalue())
        with self.assertRaises(ValueError):
            mod.check_movie(make("yuv444.mp4", "30000/1001", "640x360", "-c:v", "libx264", "-pix_fmt", "yuv444p"))
        with self.assertRaises(ValueError):
            mod.check_movie(make("mpeg4.mp4", "30000/1001", "640x360", "-c:v", "mpeg4", "-pix_fmt", "yuv420p"))

    @unittest.skipUnless(mod.DEFAULT_TEXCONV.is_file(), "texconv not available")
    def test_texture_pack_dds(self):
        textures = self.root / "export" / "textures"
        textures.mkdir(parents=True)
        Image.new("RGBA", (16, 16), (200, 0, 0, 255)).save(textures / "a.png")
        rows = [("DXT1", "01"), ("G8", "02"), ("A8R8G8B8", "03"), ("DXT5", "04")]
        index = textures / "index.csv"
        index.write_text("key,file,width,height,format,fingerprint\n" + "".join(
            f"{KEY.replace('#21', '#' + str(i))},a.png,16,16,{fmt},00000000000000{fp}\n"
            for i, (fmt, fp) in enumerate(rows)), encoding="utf-8")
        args = ["texture-pack", "--index", str(index), "--images", str(textures), "--images-index", str(index),
                "--payload", "dds", "--texconv", str(mod.DEFAULT_TEXCONV)]
        out = self.root / "dds"
        self.assertEqual(mod.main([*args, "--output", str(out)]), 0)
        folder = out / "overlay" / "textures"
        infos = {fp: mod.inspect((folder / f"fp-00000000000000{fp}.lotex2").read_bytes()) for _, fp in rows}
        self.assertEqual([infos[fp]["dds_format"] for _, fp in rows], ["BC1_UNORM", "BC4_UNORM", "BC7_UNORM", "BC7_UNORM"])
        for info in infos.values():
            self.assertEqual((info["payload"], info["mips"], info["scale"]), ("16x16", 5, 1))
        data = (folder / "fp-0000000000000004.lotex2").read_bytes()
        self.assertEqual(int.from_bytes(data[12:16], "little"), 2)
        self.assertEqual(int.from_bytes(data[44:48], "little"), 5)
        # A8R8G8B8 stores B, G, R, A: the red source ends up in the blue channel.
        dds = folder.parent / "swap.dds"
        raw = (folder / "fp-0000000000000003.lotex2").read_bytes()
        dds.write_bytes(raw[int.from_bytes(raw[8:12], "little"):])
        subprocess.run([str(mod.DEFAULT_TEXCONV), "-nologo", "-y", "-ft", "png", "-f", "R8G8B8A8_UNORM",
                        "-o", str(folder.parent), str(dds)], check=True, capture_output=True)
        with Image.open(folder.parent / "swap.png") as decoded:
            r, g, b, a = decoded.convert("RGBA").getpixel((0, 0))
        self.assertTrue(b > 150 and r < 50 and a > 200, (r, g, b, a))
        self.assertEqual(mod.main([*args, "--output", str(out)]), 0)  # Never overwrites.
        with self.assertRaises(ValueError):
            mod.inspect(data[:-4])


if __name__ == "__main__":
    unittest.main()
