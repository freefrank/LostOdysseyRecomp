#!/usr/bin/env python3
import csv
import importlib.util
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
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
            manifest = archive.read("mods/test/mod.ini").decode()
            self.assertIn("api_version=1", manifest)
            self.assertIn("image:" + KEY + "=images/", manifest)
            payload = next(name for name in archive.namelist() if name.endswith(".lotex"))
            self.assertEqual(mod.inspect(archive.read(payload))["key"], KEY)
        with self.assertRaises(FileExistsError):
            mod.pack(self.spec, output, "standalone")
        self.assertTrue(output.exists())

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
                                   "--id", "t4", "--test", "nearest4"]), 0)
        info = mod.inspect((out4 / "t4" / "textures" / "fp-00000000000000ab.lotex2").read_bytes())
        self.assertEqual((info["scale"], info["payload"]), (4, "8x8"))
        self.assertIn("texture:00000000000000ab=textures/fp-00000000000000ab.lotex2",
                      (out4 / "t4" / "mod.ini").read_text())
        with self.assertRaises(ValueError):
            mod.inspect(data + b"\0")


if __name__ == "__main__":
    unittest.main()
