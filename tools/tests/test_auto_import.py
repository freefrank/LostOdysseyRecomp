"""Only automatic recognition and grouped UI dispatch; no windows or old suites."""
import hashlib
from pathlib import Path
import queue
import struct
import sys
import tempfile
import threading
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'installer'))
import import_content as auto
import import_dlc as dlc
import import_game as game
import installer
from test_dlc_import import CONTENT_ID, PAYLOAD, stfs
from test_import_game import EU_MEDIA, files, iso, xex


class AutoImportTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.source = self.root / 'source'
        self.source.mkdir()
        self.destination = self.root / 'game'
        hashes = patch.dict(game.SUPPORTED, {n: hashlib.sha256(xex(n)).hexdigest() for n in range(1, 5)})
        hashes.start()
        self.addCleanup(hashes.stop)

    def folder(self, number, name=None, parent=None):
        path = (parent or self.source) / (name or f'source{number}')
        path.mkdir(parents=True)
        for filename, data in files(number).items():
            (path / filename).write_bytes(data)
        return path

    def package(self, name='package', **kwargs):
        path = self.source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        stfs(path, **kwargs)
        return path

    def assert_disc(self, number):
        for name, data in files(number).items():
            self.assertEqual((self.destination / f'disc{number}' / name).read_bytes(), data)

    def test_nested_mixed_uses_content_and_validates_image_once(self):
        image = self.source / 'nested' / 'disc-unknown-extension.bin'
        image.parent.mkdir()
        image.write_bytes(iso(files(1)))
        self.folder(2)
        package = self.package('nested/addon.iso')  # Actual STFS, despite the name.
        calls = []
        original = game.Image.__init__
        def counted(reader, path, *args, **kwargs):
            calls.append(path)
            original(reader, path, *args, **kwargs)
        with patch.object(game.Image, '__init__', counted):
            result = auto.scan(self.source)
        self.assertEqual(calls, [image])
        self.assertEqual([d.info['disc'] for d in result.discs], [1, 2])
        self.assertEqual([p.path for p in result.packages], [package])
        rows = installer.review_rows(result)
        self.assertEqual([r[0] for r in rows], ['Disc 1', 'Disc 2', 'DLC'])
        self.assertIn(CONTENT_ID, rows[-1][3])

    def test_four_selected_paths_are_one_disc_group(self):
        paths = [self.folder(1) / 'default.xex', self.folder(3)]
        for number, padding in ((2, 0), (4, 2 * 1024**2)):
            path = self.source / f'disc{number}.unknown'
            path.write_bytes(bytes(padding) + iso(files(number)))
            paths.append(path)
        before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths if p.is_file()}
        selection = auto.scan(paths + [paths[0]])
        self.assertEqual([d.info['disc'] for d in selection.discs], [1, 2, 3, 4])
        save = Mock(return_value='')
        result = auto.install(selection, self.destination, save_path=save)
        self.assertFalse(result.error)
        self.assertEqual(result.discs, (1, 2, 3, 4))
        save.assert_called_once_with(str(self.destination))
        for number in range(1, 5):
            self.assert_disc(number)
        self.assertEqual(before, {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in before})

    def test_cross_selection_duplicate_and_edition_conflicts(self):
        first = self.folder(1)
        duplicate = self.folder(1, 'duplicate')
        with self.assertRaisesRegex(game.ImportError, 'multiple sources for Disc 1'):
            auto.scan((first, duplicate))
        other = self.folder(2)
        data = xex(2, media=EU_MEDIA[2], version=3)
        (other / 'default.xex').write_bytes(data)
        with patch.dict(game.EU_SUPPORTED, {2: hashlib.sha256(data).hexdigest()}):
            with self.assertRaisesRegex(game.ImportError, 'Cannot mix'):
                auto.scan((first, other))
        self.assertFalse(self.destination.exists())

    def test_live_god_header_and_data_are_not_dlc(self):
        header = self.source / 'god-header'
        data = header.with_name(header.name + '.data')
        data.mkdir()
        (data / 'Data0000').write_bytes(bytes(8192) + iso(files(1)))
        raw = bytearray(0x400)
        raw[:4] = b'LIVE'
        struct.pack_into('>I', raw, 0x344, 0x7000)
        struct.pack_into('>I', raw, 0x360, 0x4D5307FA)
        struct.pack_into('>I', raw, 0x3A9, 1)
        header.write_bytes(raw)
        with patch.object(dlc, 'scan', side_effect=AssertionError('GOD must use the disc adapter')):
            result = auto.scan((header, data))
        self.assertEqual(len(result.discs), 1)
        self.assertEqual(result.discs[0].format, 'GOD')
        self.assertFalse(result.packages)

    def test_invalid_candidates_are_excluded_with_reasons(self):
        valid = self.folder(1)
        fake = self.source / 'fake.iso'
        fake.write_bytes(b'not an ISO')
        wrong = self.package('wrong-game', title=0x12345678)
        unsupported = self.package('saved-game', content_type=1)
        result = auto.scan((valid, fake, wrong, unsupported))
        self.assertEqual(len(result.discs), 1)
        self.assertFalse(result.packages)
        errors = ' '.join(error for _, error in result.rejected)
        self.assertIn('No XDVDFS', errors)
        self.assertIn('12345678', errors)
        self.assertIn('Unsupported Xbox 360 content type', errors)

    def test_mixed_import_uses_shared_root_and_preserves_existing_data(self):
        disc = self.folder(1)
        package = self.package()
        existing = self.folder(2, 'disc2', self.destination)
        for name in ('save.dat', 'config.toml', 'shader.cache'):
            (self.destination / name).write_bytes(b'keep ' + name.encode())
        save = Mock(return_value='')
        result = auto.install(auto.scan((disc, package)), existing, save_path=save)
        self.assertFalse(result.error)
        save.assert_called_once_with(str(self.destination))
        self.assertEqual(result.discs, (1,))
        self.assertEqual(result.dlc['imported'], [CONTENT_ID])
        self.assertEqual((self.destination / 'dlc' / CONTENT_ID / 'content.bin').read_bytes(), PAYLOAD)
        self.assert_disc(1)
        self.assert_disc(2)
        for name in ('save.dat', 'config.toml', 'shader.cache'):
            self.assertEqual((self.destination / name).read_bytes(), b'keep ' + name.encode())

    def test_cancel_after_discs_keeps_discs_and_retries_only_dlc(self):
        selection = auto.scan((self.folder(1), self.package()))
        cancel = threading.Event()
        def save(destination):
            cancel.set()
            return ''
        first = auto.install(selection, self.destination, cancelled=cancel.is_set, save_path=save)
        self.assertTrue(first.cancelled)
        self.assertEqual(first.discs, (1,))
        self.assertFalse(first.remaining.discs)
        self.assertEqual(first.remaining.packages, selection.packages)
        self.assert_disc(1)
        self.assertFalse((self.destination / 'dlc' / CONTENT_ID).exists())
        cancel.clear()
        save_again = Mock(side_effect=AssertionError('DLC retry must not rewrite game-path.txt'))
        retry = auto.install(first.remaining, self.destination, cancelled=cancel.is_set, save_path=save_again)
        self.assertFalse(retry.error)
        self.assertEqual(retry.dlc['imported'], [CONTENT_ID])
        save_again.assert_not_called()

    def test_dlc_failure_retains_discs_and_path_warning(self):
        package = self.package()
        selection = auto.scan((self.folder(1), package))
        installed = self.destination / 'dlc' / CONTENT_ID
        installed.mkdir(parents=True)
        (installed / 'keep').write_bytes(b'existing package')
        save = Mock(return_value='game-path.txt is read-only')
        result = auto.install(selection, self.destination, save_path=save)
        self.assertTrue(result.error)
        self.assertFalse(result.cancelled)
        self.assertEqual(result.discs, (1,))
        self.assertEqual(result.warning, 'game-path.txt is read-only')
        self.assertFalse(result.remaining.discs)
        self.assertEqual(result.remaining.packages, selection.packages)
        self.assertEqual((installed / 'keep').read_bytes(), b'existing package')
        self.assert_disc(1)

    def test_disc_conflict_stops_before_dlc_or_path_save(self):
        selection = auto.scan((self.folder(1), self.package()))
        self.folder(1, 'disc1', self.destination)
        save = Mock()
        with patch.object(dlc, 'install', side_effect=AssertionError('Must stop after disc failure')) as next_group:
            result = auto.install(selection, self.destination, save_path=save)
        self.assertIn('already installed', result.error)
        self.assertFalse(result.discs)
        self.assertEqual(result.remaining, selection)
        next_group.assert_not_called()
        save.assert_not_called()
        self.assert_disc(1)

    def test_reviewed_disc_change_stops_group_before_copy(self):
        source = self.folder(1)
        selection = auto.scan((source, self.folder(2)))
        (source / 'default.xex').write_bytes(xex(3))
        result = auto.install(selection, self.destination)
        self.assertIn('changed after review', result.error)
        self.assertFalse(self.destination.exists())

    def test_scan_cancel_is_propagated_during_image_search(self):
        path = self.source / 'padded.unknown'
        path.write_bytes(bytes(3 * 1024**2) + iso(files()))
        with self.assertRaises(game.Cancelled):
            auto.scan(path, cancelled=lambda: True)
        event = threading.Event()
        original_open = Path.open
        class Reader:
            def __init__(self, stream):
                self.stream = stream
            def __getattr__(self, name):
                return getattr(self.stream, name)
            def __enter__(self):
                return self
            def __exit__(self, *args):
                self.stream.close()
            def read(self, size=-1):
                data = self.stream.read(size)
                if size == 1024**2 + game.SECTOR:
                    event.set()
                return data
        def opened(candidate, *args, **kwargs):
            stream = original_open(candidate, *args, **kwargs)
            return Reader(stream) if candidate == path else stream
        with patch.object(Path, 'open', opened):
            with self.assertRaises(game.Cancelled):
                auto.scan(path, cancelled=event.is_set)

    def app(self):
        app = installer.Installer.__new__(installer.Installer)
        app.root = Mock()
        app.cancel = threading.Event()
        app.events = queue.Queue()
        app.latest = (0, 0, '')
        app.set_busy = Mock()
        app.start = Mock()
        app.status = Mock()
        app.review_title = Mock()
        app.review_detail = Mock()
        app.discs = Mock()
        app.discs.get_children.return_value = ()
        app.bar = {}
        app.completed_discs = set()
        app.path_warning = ''
        app.inspected_source = 'source'
        return app

    def test_controller_partial_result_keeps_only_dlc_then_returns_to_game(self):
        selection = auto.scan((self.folder(1), self.package()))
        remaining = auto.Scan(packages=selection.packages)
        app = self.app()
        app.events.put(('import_result', auto.Result((1,), None, remaining, str(self.destination), error='cancelled', cancelled=True)))
        with patch.object(installer.messagebox, 'showwarning') as warning, patch.object(sys, 'argv', ['installer', '--return-to-game']):
            app.poll()
        self.assertEqual(app.reviewed, remaining)
        self.assertIn('Completed discs kept installed: 1', warning.call_args.args[1])
        app.root.destroy.assert_not_called()
        app.events.put(('import_result', auto.Result((), {'imported': [CONTENT_ID], 'unchanged': []}, auto.Scan(), str(self.destination))))
        with patch.object(sys, 'argv', ['installer', '--return-to-game']):
            app.poll()
        app.root.destroy.assert_called_once()

    def test_controller_dlc_only_does_not_return_to_missing_game(self):
        app = self.app()
        app.events.put(('import_result', auto.Result((), {'imported': [CONTENT_ID], 'unchanged': []}, auto.Scan(), str(self.destination))))
        with patch.object(installer.messagebox, 'showinfo'), patch.object(sys, 'argv', ['installer', '--return-to-game']):
            app.poll()
        app.root.destroy.assert_not_called()
        app.start.configure.assert_called_with(text='Import', state='disabled')


if __name__ == '__main__':
    unittest.main()
