"""Focused DLC-only fixtures. No game, installer self-test, or existing suites."""
from contextlib import ExitStack
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import threading
import queue
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'installer'))
import import_dlc as dlc
import installer

CONTENT_ID = '1234567890ABCDEF1234567890ABCDEF12345678'
PAYLOAD = b'Lost Odyssey DLC fixture\x00\x01\x02' * 220


def stfs(path, *, copies=1, total=3, file_blocks=(1, 2), payload=PAYLOAD,
         content_id=CONTENT_ID, name='content.bin', parent=0xffff, nested=False,
         table_blocks=(0,), title=0x4d5307fa, content_type=2, mutate=None):
    """Build sparse logical volumes by inserting table pages at group boundaries.

    This independent layout builder does not call production address functions.
    Unused data pages remain zero; only referenced file/table pages are allocated.
    """
    physical, data_pages, tables = 0, {}, {}
    for number in range(total):
        if number == 170 ** 2:
            tables[(2, 0)] = physical
            physical += copies
        if number == 170 or (number and number % (170 ** 2) == 0):
            tables[(1, number // (170 ** 2))] = physical
            physical += copies
        if number % 170 == 0:
            tables[(0, number // 170)] = physical
            physical += copies
        data_pages[number] = physical
        physical += 1
    data = bytearray(0xa000 + physical * 4096)
    data[:4] = b'LIVE' if copies == 1 else b'CON '
    data[0x32c:0x340] = bytes.fromhex(content_id)
    struct.pack_into('>IIIQ', data, 0x340, 0xa000, content_type, 2, len(payload))
    struct.pack_into('>I', data, 0x360, title)
    data[0x379:0x37c] = bytes((0x24, 0, 1 if copies == 1 else 2))
    data[0x37c:0x37e] = len(table_blocks).to_bytes(2, 'little')
    data[0x37e:0x381] = table_blocks[0].to_bytes(3, 'little')
    struct.pack_into('>II', data, 0x395, total, 0)
    label = 'Test DLC'.encode('utf-16-be')
    data[0x411:0x411 + len(label)] = label
    struct.pack_into('>QII', data, 0x22c, 0, 5, 1)
    active = {key: ((sum(key) + 1) % 2 if copies == 2 else 0) for key in tables}
    top = 2 if total > 170 ** 2 else int(total > 170)
    active[(top, 0)] = int(copies == 2)
    buffers = {key: bytearray(4096) for key in tables}
    used = {}
    def directory_record(filename, directory, parent_index, size, blocks):
        record = bytearray(64)
        encoded = filename.encode('utf-8')
        record[:len(encoded)] = encoded
        record[40] = len(encoded) | (128 if directory else 0)
        record[41:44] = ((size + 4095) // 4096).to_bytes(3, 'little')
        record[44:47] = len(blocks).to_bytes(3, 'little')
        record[47:50] = (blocks[0] if blocks else 0).to_bytes(3, 'little')
        struct.pack_into('>HI', record, 50, parent_index, size)
        return record
    listing = bytearray(len(table_blocks) * 4096)
    if nested:
        listing[:64] = directory_record('folder', True, 0xffff, 0, ())
        listing[64:128] = directory_record(name, False, 0, len(payload), file_blocks)
    else:
        listing[:64] = directory_record(name, False, parent, len(payload), file_blocks)
    for index, block in enumerate(table_blocks):
        used[block] = (bytes(listing[index * 4096:(index + 1) * 4096]),
                       table_blocks[index + 1] if index + 1 < len(table_blocks) else dlc.END)
    for index, block in enumerate(file_blocks):
        page = payload[index * 4096:(index + 1) * 4096].ljust(4096, b'\0')
        used[block] = (page, file_blocks[index + 1] if index + 1 < len(file_blocks) else dlc.END)
    if mutate:
        mutate(data, used, listing)
    for number, (page, next_block) in used.items():
        offset = 0xa000 + data_pages[number] * 4096
        data[offset:offset + 4096] = page
        index = (number % 170) * 24
        buffers[(0, number // 170)][index:index + 24] = (
            hashlib.sha1(page).digest() + bytes((0x80,)) + next_block.to_bytes(3, 'big'))
    for level in range(top + 1):
        for key, table in buffers.items():
            if key[0] != level:
                continue
            if level < top:
                parent_key = (level + 1, key[1] // 170)
                index = (key[1] % 170) * 24
                buffers[parent_key][index:index + 24] = hashlib.sha1(table).digest() + struct.pack('>I', active[key] << 30)
            offset = 0xa000 + (tables[key] + active[key]) * 4096
            data[offset:offset + 4096] = table
    data[0x381:0x395] = hashlib.sha1(buffers[(top, 0)]).digest()
    path.write_bytes(data)
    return data_pages, tables


class DlcTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / 'source-package'
        self.destination = self.root / 'game'

    def make(self, **kwargs):
        stfs(self.source, **kwargs)
        return self.source

    def assert_clean(self):
        self.assertFalse((self.destination / '.import.lock').exists())
        self.assertFalse(list(self.destination.glob('.dlc-import-*')))
        self.assertFalse(list((self.destination / 'dlc').glob('*')))

    def test_simple_extract_and_metadata(self):
        self.make(nested=True)
        before = self.source.read_bytes()
        result = dlc.install(dlc.scan(self.source), self.destination)
        target = self.destination / 'dlc' / CONTENT_ID
        self.assertEqual(result['imported'], [CONTENT_ID])
        self.assertEqual((target / 'folder/content.bin').read_bytes(), PAYLOAD)
        record = (target / '.lo-content').read_bytes()
        self.assertEqual(len(record), 308)
        self.assertEqual(struct.unpack_from('>II', record), (1, 2))
        self.assertEqual(record[264:305], CONTENT_ID.encode() + b'\0')
        manifest = json.loads((target / '.lo-dlc.json').read_text())
        self.assertEqual(manifest['license_mask'], 5)
        self.assertEqual(manifest['files'][0]['sha256'], hashlib.sha256(PAYLOAD).hexdigest())
        self.assertEqual((target / '.lo-dlc-header').read_bytes(), before[:0xa000])
        self.assertEqual(self.source.read_bytes(), before)

    def test_nonconsecutive_two_levels_and_secondary_tables(self):
        for copies in (1, 2):
            with self.subTest(copies=copies):
                self.make(copies=copies, total=172, file_blocks=(1, 171))
                destination = self.root / f'game-{copies}'
                dlc.install(self.source, destination)
                self.assertEqual((destination / 'dlc' / CONTENT_ID / 'content.bin').read_bytes(), PAYLOAD)

    def test_three_level_hash_tree(self):
        self.make(copies=2, total=28902, file_blocks=(1, 28901))
        dlc.install(self.source, self.destination)
        self.assertEqual((self.destination / 'dlc' / CONTENT_ID / 'content.bin').read_bytes(), PAYLOAD)

    def test_multiple_directory_blocks(self):
        self.make(total=173, file_blocks=(1, 172), table_blocks=(0, 171))
        dlc.install(self.source, self.destination)
        self.assertEqual((self.destination / 'dlc' / CONTENT_ID / 'content.bin').read_bytes(), PAYLOAD)

    def test_wrong_title_and_non_dlc_rejected(self):
        for params, message in ((dict(title=0x4d5307df), 'Wrong game'), (dict(content_type=1), 'not Marketplace')):
            with self.subTest(params=params):
                self.make(**params)
                with self.assertRaisesRegex(dlc.ImportError, message):
                    dlc.scan(self.source)

    def test_truncated(self):
        self.make()
        self.source.write_bytes(self.source.read_bytes()[:-1])
        with self.assertRaisesRegex(dlc.ImportError, 'Truncated'):
            dlc.scan(self.source)

    def test_bad_hash_table(self):
        self.make()
        with self.source.open('r+b') as stream:
            stream.seek(0xa000)
            stream.write(b'bad')
        with self.assertRaisesRegex(dlc.ImportError, 'hash table checksum'):
            dlc.scan(self.source)

    def test_bad_data_hash_rolls_back(self):
        pages, _ = stfs(self.source)
        with self.source.open('r+b') as stream:
            stream.seek(0xa000 + pages[1] * 4096)
            stream.write(b'bad')
        with self.assertRaisesRegex(dlc.ImportError, 'data block checksum'):
            dlc.install(self.source, self.destination)
        self.assert_clean()

    def test_cyclic_and_short_chain(self):
        for next_block in (1, dlc.END):
            def mutate(data, used, listing):
                used[1] = (used[1][0], next_block)
            self.make(mutate=mutate)
            with self.assertRaises(dlc.ImportError):
                dlc.scan(self.source)

    def test_unsafe_names_and_parent(self):
        for name in ('..', '../escape', 'C:evil', 'CON', '.lo-content', '.lo-anything'):
            with self.subTest(name=name):
                self.make(name=name)
                with self.assertRaises(dlc.ImportError):
                    dlc.scan(self.source)
        self.make(parent=90)
        with self.assertRaisesRegex(dlc.ImportError, 'parent'):
            dlc.scan(self.source)

    def test_cancel_during_extract(self):
        self.make()
        stop = [False]
        with self.assertRaises(dlc.Cancelled):
            dlc.install(self.source, self.destination, lambda *args: stop.__setitem__(0, True), lambda: stop[0])
        self.assert_clean()

    def test_publish_failure_rolls_back(self):
        self.make()
        other = self.root / 'package-2'
        stfs(other, content_id='F' * 40)
        original = Path.rename
        def rename(path, target):
            if path.name == 'F' * 40:
                raise OSError('Injected publish failure')
            return original(path, target)
        with patch.object(Path, 'rename', rename), self.assertRaisesRegex(OSError, 'publish failure'):
            dlc.install(dlc.scan([self.source, other]), self.destination)
        self.assert_clean()

    def test_duplicate_verifies_payload_and_conflicts_do_not_overwrite(self):
        self.make()
        dlc.install(self.source, self.destination)
        self.assertEqual(dlc.install(self.source, self.destination)['unchanged'], [CONTENT_ID])
        target = self.destination / 'dlc' / CONTENT_ID / 'content.bin'
        target.write_bytes(b'x' * len(PAYLOAD))
        with self.assertRaisesRegex(dlc.ImportError, 'damaged'):
            dlc.install(self.source, self.destination)
        self.assertEqual(target.read_bytes(), b'x' * len(PAYLOAD))

    def test_different_source_same_id_rejected(self):
        self.make()
        dlc.install(self.source, self.destination)
        self.make(payload=b'x' * len(PAYLOAD))
        with self.assertRaisesRegex(dlc.ImportError, 'different'):
            dlc.install(self.source, self.destination)
        self.assertEqual((self.destination / 'dlc' / CONTENT_ID / 'content.bin').read_bytes(), PAYLOAD)

    def test_review_identity_change(self):
        self.make()
        reviewed = dlc.scan(self.source)
        self.make(payload=b'y' * len(PAYLOAD))
        with self.assertRaisesRegex(dlc.ImportError, 'since review'):
            dlc.install(reviewed, self.destination)
        self.assert_clean()

    def test_disc_root_and_multifile_directory_scan(self):
        self.make()
        (self.root / 'unrelated.iso').write_bytes(b'not a DLC')
        disc = self.destination / 'Disc2'
        disc.mkdir(parents=True)
        (disc / 'default.xex').write_bytes(b'test game path marker')
        self.assertEqual(dlc.game_root(disc), self.destination.resolve())
        result = dlc.scan(self.root)
        self.assertEqual(len(result.packages), 1)
        dlc.install(result, disc)
        self.assertTrue((self.destination / 'dlc' / CONTENT_ID).is_dir())
        self.assertFalse((disc / 'dlc').exists())

    def test_destination_link_rejected(self):
        self.make()
        real = self.root / 'real'
        real.mkdir()
        try:
            self.destination.symlink_to(real, target_is_directory=True)
        except OSError:
            self.skipTest('Host does not allow symlink creation')
        with self.assertRaisesRegex(dlc.ImportError, 'links'):
            dlc.install(self.source, self.destination)

    def test_modified_manifest_cannot_validate_modified_payload(self):
        self.make()
        dlc.install(self.source, self.destination)
        target = self.destination / 'dlc' / CONTENT_ID
        changed = b'x' * len(PAYLOAD)
        (target / 'content.bin').write_bytes(changed)
        manifest_file = target / '.lo-dlc.json'
        manifest = json.loads(manifest_file.read_text())
        manifest['files'][0]['sha256'] = hashlib.sha256(changed).hexdigest()
        manifest_file.write_text(json.dumps(manifest))
        with self.assertRaisesRegex(dlc.ImportError, 'damaged'):
            dlc.install(self.source, self.destination)

    def test_installer_relative_game_path_uses_installer_home(self):
        home = self.root / 'release'
        home.mkdir()
        (home / 'game-path.txt').write_text('../game/disc2', encoding='utf-8')
        self.assertEqual(Path(installer.initial_destination(home)), (home / '../game/disc2').resolve())

    def test_fixed_physical_address_vectors(self):
        # Fixed STFS boundary vectors, independent of the builder and reader.
        reader = dlc.Stfs.__new__(dlc.Stfs)
        reader.base = 0
        reader.total_blocks = 30000
        for copies, expected in ((1, (170, 173, 29070, 29074)),
                                 (2, (171, 176, 29241, 29248))):
            reader.copies = copies
            self.assertEqual(tuple(reader.data_offset(n) // 4096 for n in (169, 170, 28899, 28900)), expected)
        reader.copies = 1
        self.assertEqual(tuple(reader.table_offset(28900, level) // 4096 for level in (0, 1, 2)),
                         (29073, 29072, 29071))

    def test_gui_dlc_confirmation_to_import_does_not_change_game_path(self):
        self.make()
        app = installer.Installer.__new__(installer.Installer)
        app.source = SimpleNamespace(get=lambda: str(self.source))
        app.destination = SimpleNamespace(get=lambda: str(self.destination))
        app.inspected_source = str(self.source)
        app.reviewed = installer.import_content.scan(self.source)
        app.cancel = threading.Event()
        app.root = object()
        app.set_busy = Mock()
        app.save_game_path = Mock(side_effect=AssertionError('DLC must not change game-path.txt'))
        app.events = queue.Queue()
        app.latest = (0, 0, '')
        class ImmediateThread:
            def __init__(self, target, **kwargs):
                self.target = target
            def start(self):
                self.target()
        with patch.object(installer.messagebox, 'askokcancel', return_value=False):
            app.run()
        self.assertFalse(self.destination.exists())
        with patch.object(installer.messagebox, 'askokcancel', return_value=True) as confirm, \
                patch.object(installer.threading, 'Thread', ImmediateThread):
            app.run()
        self.assertIn(CONTENT_ID, confirm.call_args.args[1])
        event = app.events.get_nowait()
        self.assertEqual(event[0], 'import_result')
        self.assertFalse(event[1].error)
        app.save_game_path.assert_not_called()
        self.assertEqual((self.destination / 'dlc' / CONTENT_ID / 'content.bin').read_bytes(), PAYLOAD)

    def test_gui_multi_file_selection_reaches_scan(self):
        app = installer.Installer.__new__(installer.Installer)
        app.root = object()
        app.selected_files = ()
        app.source = SimpleNamespace(set=lambda value: setattr(app, 'selected_files', ()))
        app.check_source = Mock()
        values = (str(self.root / 'A'), str(self.root / 'B'))
        with patch.object(installer.filedialog, 'askopenfilenames', return_value=values):
            app.file()
        self.assertEqual(app.source_selection(), values)
        app.check_source.assert_called_once()


def create_runtime_fixture(destination):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    package = destination / 'synthetic-live.stfs'
    stfs(package, nested=True)
    game = destination / 'game'
    (game / 'disc2').mkdir(parents=True, exist_ok=True)
    (game / 'disc2/default.xex').write_bytes(b'fixture root marker; not game code')
    result = dlc.install(package, game)
    identity = {'synthetic': True, 'source_sha256': hashlib.sha256(package.read_bytes()).hexdigest(),
                'game_root': str(game.resolve()), 'content_id': CONTENT_ID,
                'file': 'folder/content.bin', 'payload_hex': PAYLOAD.hex(), 'result': result}
    (destination / 'expected.json').write_text(json.dumps(identity, indent=2), encoding='utf-8')
    return identity


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--runtime-fixture':
        print(json.dumps(create_runtime_fixture(sys.argv[2]), indent=2))
    else:
        unittest.main()
