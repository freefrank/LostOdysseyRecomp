import hashlib
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'installer'))
import import_game as imp


ASIA_MEDIA = {1: 0x39F7D748, 2: 0x0EF8CEA8, 3: 0x309E3386, 4: 0x7B21A91D}
EU_MEDIA = {1: 0x368DE6DD, 2: 0x1888BE4E, 3: 0x6DD59D08, 4: 0x0C0E80B5}


def xex(disc=1, title=0x4d5307fa, media=None, version=4, base=None, discs=4):
    data = bytearray(128)
    data[:4] = b'XEX2'
    struct.pack_into('>I', data, 20, 1)
    struct.pack_into('>II', data, 24, 0x40006, 32)
    struct.pack_into('>IIII4B', data, 32, media if media is not None else ASIA_MEDIA[disc],
                     version, version if base is None else base, title, 2, 0, disc, discs)
    return bytes(data)


def files(disc=1):
    names = ['LO.fpd', 'LO.fpi'] + [f'xenon_{n}.fpd' for n in
             ('battle', 'chr', 'event', 'field', 'loc', 'mov', 'obj', 'scr', 'snd', 'sys', 'vfx', 'world')]
    return {'default.xex': xex(disc), **{n: (n * 4).encode() for n in names}}


def iso(contents):
    data = bytearray(128 * 2048)
    data[0x10000:0x10014] = data[0x107ec:0x10800] = imp.MAGIC
    struct.pack_into('<II', data, 0x10014, 40, 2048)
    offset = 0
    for i, (name, payload) in enumerate(contents.items()):
        encoded = name.encode('ascii')
        next_offset = (offset + 14 + len(encoded) + 3) & ~3
        right = next_offset // 4 if i + 1 < len(contents) else 0
        struct.pack_into('<HHIIBB', data, 40 * 2048 + offset, 0, right, 50 + i, len(payload), 0, len(encoded))
        data[40 * 2048 + offset + 14:40 * 2048 + offset + 14 + len(encoded)] = encoded
        data[(50 + i) * 2048:(50 + i) * 2048 + len(payload)] = payload
        offset = next_offset
    return data


class ImportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.dest = self.root / 'installed'
        self.hashes = patch.dict(imp.SUPPORTED, {i: hashlib.sha256(xex(i)).hexdigest() for i in range(1, 5)})
        self.hashes.start()
        self.addCleanup(self.hashes.stop)

    def folder(self, disc=1):
        source = self.root / f'source{disc}'
        source.mkdir()
        for name, data in files(disc).items():
            (source / name).write_bytes(data)
        return source

    def check_output(self, disc=1):
        for name, data in files(disc).items():
            self.assertEqual((self.dest / f'disc{disc}' / name).read_bytes(), data)

    def eu_folder(self, disc=1):
        source = self.folder(disc)
        data = xex(disc, media=EU_MEDIA[disc], version=3)
        (source / 'default.xex').write_bytes(data)
        hashes = patch.dict(imp.EU_SUPPORTED, {disc: hashlib.sha256(data).hexdigest()})
        hashes.start()
        self.addCleanup(hashes.stop)
        return source

    def test_europe_four_discs(self):
        for n in range(1, 5):
            self.eu_folder(n)
        self.assertEqual(imp.install(self.root, self.dest.parent / 'eu-installed'), [1, 2, 3, 4])

    def test_mixed_source_editions_rejected(self):
        self.folder(1)
        self.eu_folder(2)
        with self.assertRaisesRegex(imp.ImportError, 'Cannot mix'):
            imp.install(self.root, self.dest.parent / 'mixed')

    def test_mixed_existing_edition_rejected_without_metadata(self):
        imp.install(self.folder(1), self.dest)
        (self.dest / 'disc1' / 'import-info.json').unlink()
        with self.assertRaisesRegex(imp.ImportError, 'Cannot mix'):
            imp.install(self.eu_folder(2), self.dest)
        self.check_output()
        self.assertFalse((self.dest / 'disc2').exists())
        self.assertFalse((self.dest / '.import.lock').exists())

    def test_europe_incremental_import(self):
        self.assertEqual(imp.install(self.eu_folder(1), self.dest), [1])
        self.assertEqual(imp.install(self.eu_folder(2), self.dest), [2])

    def test_byte_different_xex_metadata_is_diagnostic_only(self):
        source = self.folder()
        with (source / 'default.xex').open('ab') as f:
            f.write(b'modified')
        with self.assertRaisesRegex(imp.ImportError, 'Metadata resembles Europe / Asia.*metadata alone'):
            imp.install(source, self.dest)
        self.assertFalse(self.dest.exists())

    def test_unknown_xex_reports_identity(self):
        source = self.folder()
        (source / 'default.xex').write_bytes(xex(media=0x11112222))
        with self.assertRaisesRegex(imp.ImportError, 'Media ID 11112222.*MD5.*SHA256'):
            imp.install(source, self.dest)
        self.assertFalse(self.dest.exists())

    def test_optional_md5_identity(self):
        source = self.folder()
        digest = hashlib.md5(xex()).hexdigest()
        with patch.dict(imp.SUPPORTED, {1: 'not-this-file'}), patch.dict(imp.SUPPORTED_MD5, {1: digest}):
            inspected = imp.scan(source)
        self.assertEqual(inspected.discs[0].info['identity'], 'md5')

    def test_folder_and_xex_then_add_disc(self):
        self.assertEqual(imp.install(self.folder() / 'default.xex', self.dest), [1])
        self.assertEqual(imp.install(self.folder(2), self.dest), [2])
        self.check_output(1)
        self.check_output(2)

    def test_iso_and_padded_iso(self):
        for padding in (0, 0x20000):
            with self.subTest(padding=padding):
                source = self.root / f'{padding}.iso'
                source.write_bytes(bytes(padding) + iso(files()))
                self.dest = self.root / f'installed{padding}'
                imp.install(source, self.dest)
                self.check_output()

    def test_god_outer_folder(self):
        source = self.root / 'content' / '4D5307FA' / '00007000' / 'header.data'
        source.mkdir(parents=True)
        # Insert the two hash blocks preceding the first group of data blocks.
        (source / 'Data0000').write_bytes(bytes(8192) + iso(files()))
        imp.install(self.root / 'content', self.dest)
        self.check_output()

    def test_god_cross_hash_group_read(self):
        source = self.root / 'cross.data'
        source.mkdir()
        a, b = b'A' * (204 * 4096), b'B' * 4096
        (source / 'Data0000').write_bytes(bytes(8192) + a + bytes(4096) + b)
        with imp.ExitStack() as stack:
            image = imp.Image(source, stack)
            self.assertEqual(image.read(len(a) - 16, 32), b'A' * 16 + b'B' * 16)

    def test_deep_nested_layout_and_bad_sibling(self):
        source = self.root / 'library' / 'vendor' / 'collection' / 'lost-odyssey' / 'set' / 'disc-one'
        source.mkdir(parents=True)
        for name, data in files().items():
            (source / name).write_bytes(data)
        bad = self.root / 'library' / 'unrelated.iso'
        bad.write_bytes(b'not an xbox image')
        inspected = imp.scan(self.root / 'library')
        self.assertEqual([disc.info['disc'] for disc in inspected.discs], [1])
        self.assertEqual(len(inspected.rejected), 1)
        imp.install(self.root / 'library', self.dest)
        self.check_output()

    def test_unreadable_sibling_does_not_hide_valid_disc(self):
        valid = self.folder()
        sibling = self.root / 'unreadable'
        sibling.mkdir()
        original = imp.linked
        def unreadable(path):
            if path == sibling:
                raise PermissionError('synthetic access failure')
            return original(path)
        with patch.object(imp, 'linked', side_effect=unreadable):
            self.assertEqual(imp.discover(self.root), [valid])

    def test_duplicate_disc_sources_are_ambiguous(self):
        self.folder(1)
        duplicate = self.root / 'backup' / 'disc1'
        duplicate.mkdir(parents=True)
        for name, data in files().items():
            (duplicate / name).write_bytes(data)
        with self.assertRaisesRegex(imp.ImportError, 'multiple sources for Disc 1.*closer folder'):
            imp.scan(self.root)

    def test_cancel_rolls_back(self):
        source = self.folder()
        written = False
        def update(done, total, label):
            nonlocal written
            written |= done > 0
        with self.assertRaises(imp.Cancelled):
            imp.install(source, self.dest, update, lambda: written)
        self.assertEqual(list(self.dest.iterdir()), [])
        self.assertTrue((source / 'default.xex').exists())

    def test_source_check_can_be_cancelled(self):
        source = self.folder()
        with self.assertRaisesRegex(imp.Cancelled, 'check cancelled'):
            imp.scan(source, cancelled=lambda: True)

    def test_existing_disc_preserved(self):
        source = self.folder()
        imp.install(source, self.dest)
        with self.assertRaisesRegex(imp.ImportError, 'already installed'):
            imp.install(source, self.dest)
        self.check_output()

    def test_wrong_game(self):
        source = self.folder()
        (source / 'default.xex').write_bytes(xex(title=0x4d5307df))
        with self.assertRaisesRegex(imp.ImportError, 'Wrong game'):
            imp.install(source, self.dest)
        self.assertFalse(self.dest.exists())

    def test_incomplete_disc(self):
        source = self.folder()
        (source / 'xenon_chr.fpd').unlink()
        with self.assertRaisesRegex(imp.ImportError, 'Incomplete'):
            imp.install(source, self.dest)

    def test_bad_names(self):
        for name in ('../escape', 'C:drive', 'CON.txt', 'bad.', 'x\\y', '..'):
            with self.subTest(name=name), self.assertRaises(imp.ImportError):
                imp.component(name)

    def test_directory_cycle(self):
        data = iso(files())
        struct.pack_into('<H', data, 40 * 2048 + 2, 1)
        source = self.root / 'bad.iso'
        source.write_bytes(data)
        with self.assertRaises((imp.ImportError, UnicodeDecodeError)):
            imp.install(source, self.dest)
        self.assertFalse(self.dest.exists())

    def test_missing_god_chunk(self):
        source = self.root / 'bad.data'
        source.mkdir()
        (source / 'Data0001').write_bytes(b'')
        with imp.ExitStack() as stack, self.assertRaises(imp.ImportError):
            imp.Image(source, stack)

    def test_nested_destination(self):
        source = self.folder()
        with self.assertRaisesRegex(imp.ImportError, 'separate'):
            imp.install(source, source / 'game')

    def test_concurrent_import_is_rejected(self):
        source = self.folder()
        self.dest.mkdir()
        (self.dest / '.import.lock').write_text('other operation')
        with self.assertRaisesRegex(imp.ImportError, 'Another import'):
            imp.install(source, self.dest)
        self.assertEqual((self.dest / '.import.lock').read_text(), 'other operation')

    def test_source_change_rolls_back(self):
        source = self.folder()
        original = imp.prepare
        def changed(*args, **kwargs):
            result = original(*args, **kwargs)
            (source / 'default.xex').write_bytes(xex(2))
            return result
        with patch.object(imp, 'prepare', side_effect=changed), self.assertRaisesRegex(imp.ImportError, 'changed'):
            imp.install(source, self.dest)
        self.assertEqual(list(self.dest.iterdir()), [])

    def test_publish_failure_rolls_back_all_discs(self):
        self.folder(1)
        self.folder(2)
        original = Path.rename
        def fail_second(path, target):
            if path.name == 'disc2' and path.parent.name.startswith('.import-'):
                raise OSError('synthetic publish failure')
            return original(path, target)
        with patch.object(Path, 'rename', fail_second), self.assertRaisesRegex(OSError, 'synthetic publish'):
            imp.install(self.root, self.dest)
        self.assertEqual(list(self.dest.iterdir()), [])


if __name__ == '__main__':
    unittest.main()
