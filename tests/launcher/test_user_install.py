#!/usr/bin/env python3
"""Isolated installer contract; never uses the real HOME or XDG roots."""
import contextlib
import importlib.machinery
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'tools/link-display-user-install'


class InstallerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        loader = importlib.machinery.SourceFileLoader('user_install', str(TOOL))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        cls.m = importlib.util.module_from_spec(spec)
        loader.exec_module(cls.m)

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='link-desktop-test-')
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.data = self.root / 'private stage'
        self.helper = str(self.root / 'helper with space %f $cash')
        self.env = mock.patch.dict(os.environ, {'HOME': str(self.root / 'home'),
                                               'XDG_DATA_HOME': str(self.root / 'xdg')})
        self.env.start()
        self.addCleanup(self.env.stop)

    def plan(self, op='install', helper=None, unit=None, receipt=None):
        return self.m.make_plan(op, str(self.data), helper or self.helper, unit, receipt)

    def apply(self, p):
        return self.m.apply_plan(p)

    def install(self):
        p = self.plan()
        self.apply(p)
        return p

    def test_01_dryrun_no_writes(self):
        p = self.plan()
        self.assertFalse(self.data.exists())
        self.assertEqual(p['operation'], 'install')

    def test_02_cli_dryrun(self):
        r = subprocess.run([sys.executable, '-B', str(TOOL), 'install', '--stage',
                            str(self.data), '--helper', self.helper], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(json.loads(r.stdout)['operation'], 'install')
        self.assertFalse(self.data.exists())

    def test_03_install_and_idempotence(self):
        self.install()
        p = self.plan()
        self.assertEqual(p['changes'], [])
        before = sorted(str(x) for x in self.data.rglob('*'))
        self.apply(p)
        self.assertEqual(before, sorted(str(x) for x in self.data.rglob('*')))

    def test_04_exec_roundtrip(self):
        # Decode desktop-value escapes, then Exec quoting, then literal %%.
        for name in ['a b helper', '%f% helper', '$a/"b`\\helper']:
            path = str(self.root / name)
            value = self.m.exec_value(path, 'raspi-link-display-desktop-test.service')
            unescaped = value.replace('\\\\', '\\')
            import shlex
            tokens = shlex.split(unescaped)
            tokens = [t.replace('\\$', '$').replace('\\`', '`').replace('%%', '%') for t in tokens]
            self.assertEqual(tokens, ['/usr/bin/python3', path, 'start', '--desktop', '--unit', 'raspi-link-display-desktop-test.service'])

    def test_05_desktop_metadata(self):
        self.install()
        text = (self.data / 'applications/raspi-link-display.desktop').read_text()
        self.assertIn('Terminal=false\n', text)
        self.assertIn('Icon=raspi-link-display\n', text)
        self.assertNotIn('Path=', text)
        self.assertNotIn('--unit', text)
        self.assertNotIn('@EXEC@', text)

    def test_06_candidate_unit(self):
        p = self.plan(unit='raspi-link-display-desktop-test.service')
        self.apply(p)
        self.assertIn('--unit raspi-link-display-desktop-test.service', (self.data / 'applications/raspi-link-display.desktop').read_text())

    def test_07_invalid_helper(self):
        for helper in ['relative/helper', '/opt/a=bad/helper', '/opt/a\nhelper', '/opt/a\thelper']:
            with self.subTest(helper=helper), self.assertRaises(self.m.Refusal):
                self.plan(helper=helper)

    def test_08_invalid_unit(self):
        for unit in ['bad unit.service', '../bad.service', 'a;bad.service', 'ableton-link-pi-display.service', 'default.service', 'bad']:
            with self.subTest(unit=unit), self.assertRaises(self.m.Refusal):
                self.plan(unit=unit)

    def test_09_collision(self):
        target = self.data / 'applications/raspi-link-display.desktop'
        target.parent.mkdir(parents=True)
        target.write_text('user file')
        with self.assertRaises(self.m.Refusal):
            self.plan()
        self.assertEqual(target.read_text(), 'user file')

    def test_10_changed_predecessor(self):
        p = self.plan()
        self.data.mkdir()
        target = self.data / 'applications'
        target.mkdir()
        (target / 'raspi-link-display.desktop').write_text('raced')
        with self.assertRaises(self.m.Refusal):
            self.apply(p)

    def test_11_drift_refuses_install(self):
        self.install()
        target = self.data / 'applications/raspi-link-display.desktop'
        target.write_text('edited by user')
        with self.assertRaises(self.m.Refusal):
            self.plan()

    def test_12_remove_retains_changed(self):
        self.install()
        target = self.data / 'applications/raspi-link-display.desktop'
        target.write_text('edited by user')
        p = self.plan('remove')
        self.assertEqual(p['retained'], [str(target)])
        self.apply(p)
        self.assertEqual(target.read_text(), 'edited by user')
        self.assertFalse((self.data / 'icons/hicolor/scalable/apps/raspi-link-display.svg').exists())
        self.assertEqual(self.plan('remove')['changes'], [])

    def test_13_remove_idempotent(self):
        self.install()
        self.apply(self.plan('remove'))
        self.assertEqual(self.plan('remove')['changes'], [])

    def test_14_recovery(self):
        self.install()
        removal = self.plan('remove')
        receipt = self.apply(removal)
        self.assertTrue(Path(receipt).is_file())
        self.apply(self.plan('recover', receipt=receipt))
        self.assertEqual(self.plan()['changes'], [])

    def test_15_recovery_drift(self):
        self.install()
        receipt = self.apply(self.plan('remove'))
        target = self.data / 'applications/raspi-link-display.desktop'
        target.write_text('new user file')
        with self.assertRaises(self.m.Refusal):
            self.plan('recover', receipt=receipt)
        self.assertEqual(target.read_text(), 'new user file')

    def test_16_xdg(self):
        self.assertEqual(self.m.data_home(), str(self.root / 'xdg'))
        with mock.patch.dict(os.environ, {'XDG_DATA_HOME': 'relative'}):
            with self.assertRaises(self.m.Refusal):
                self.m.data_home()
        with mock.patch.dict(os.environ, {'XDG_DATA_HOME': ''}):
            self.assertEqual(self.m.data_home(), str(self.root / 'home/.local/share'))

    def test_17_symlink(self):
        self.data.mkdir()
        outside = self.root / 'outside'
        outside.mkdir()
        try:
            (self.data / 'applications').symlink_to(outside, target_is_directory=True)
        except OSError as exc:
            self.skipTest('symlink privilege unavailable: ' + str(exc))
        with self.assertRaises(self.m.Refusal):
            self.plan()
        self.assertEqual(list(outside.iterdir()), [])

    def test_18_unsafe_mode(self):
        if os.name == 'nt':
            self.skipTest('POSIX mode checks require native Linux')
        self.data.mkdir(mode=0o777)
        self.data.chmod(0o777)
        with self.assertRaises(self.m.Refusal):
            self.plan()

    def test_19_unsafe_owner(self):
        self.data.mkdir()
        if not hasattr(os, 'getuid'):
            self.skipTest('UID checks require native Linux')
        with mock.patch.object(os, 'getuid', return_value=os.getuid() + 1):
            with self.assertRaises(self.m.Refusal):
                self.plan()

    def test_20_cli_apply_plan(self):
        p = self.plan()
        saved = self.root / 'plan.json'
        saved.write_text(json.dumps(p))
        r = subprocess.run([sys.executable, '-B', str(TOOL), 'install', '--stage', str(self.data),
                            '--helper', self.helper, '--apply-plan', str(saved)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertTrue((self.data / 'applications/raspi-link-display.desktop').exists())

    def test_21_tampered_plan(self):
        p = self.plan()
        p['changes'][0]['after'] = 'eA=='
        with self.assertRaises(self.m.Refusal):
            self.apply(p)

    def test_22_hardlink_refusal(self):
        self.install()
        target = self.data / 'applications/raspi-link-display.desktop'
        os.link(str(target), str(self.root / 'hardlink'))
        with self.assertRaises(self.m.Refusal):
            self.plan()


    def test_23_owned_update_and_restore(self):
        self.install()
        before = (self.data / 'applications/raspi-link-display.desktop').read_bytes()
        receipt = self.apply(self.plan(helper=str(self.root / 'replacement helper')))
        self.assertNotEqual(before, (self.data / 'applications/raspi-link-display.desktop').read_bytes())
        record = json.loads(Path(receipt).read_text())
        self.assertEqual(self.m.decoded(record['changes'][0]['before']['bytes']), before)
        self.apply(self.plan('recover', receipt=receipt))
        self.assertEqual(self.plan()['changes'], [])

    def test_24_interrupted_transaction_recovery(self):
        # Fail the second new output after the journal and first output exist.
        original = self.m.exclusive_write
        def fail_icon(path, data, mode=0o644):
            if str(path).endswith('raspi-link-display.svg'):
                raise OSError('injected write failure')
            return original(path, data, mode)
        with mock.patch.object(self.m, 'exclusive_write', side_effect=fail_icon):
            with self.assertRaises(OSError):
                self.install()
        receipts = list((self.data / self.m.STATE / 'recovery').glob('*.json'))
        self.assertEqual(len(receipts), 1)
        self.apply(self.plan('recover', receipt=str(receipts[0])))
        self.assertFalse((self.data / self.m.DESKTOP).exists())
        self.assertFalse((self.data / self.m.ICON).exists())
        self.assertFalse((self.data / self.m.MANIFEST).exists())

    def test_25_same_bytes_changed_identity(self):
        self.install()
        p = self.plan(helper=str(self.root / 'replacement helper'))
        target = self.data / self.m.DESKTOP
        before = target.read_bytes()
        target.unlink()
        target.write_bytes(before)
        with self.assertRaises(self.m.Refusal):
            self.apply(p)

    def test_26_default_unit_refused(self):
        with self.assertRaises(self.m.Refusal):
            self.plan(unit='raspi-link-display.service')

    def test_27_manifest_symlink(self):
        self.install()
        target = self.data / self.m.MANIFEST
        outside = self.root / 'user-manifest'
        outside.write_bytes(target.read_bytes())
        target.unlink()
        try:
            target.symlink_to(outside)
        except OSError as exc:
            self.skipTest('symlink privilege unavailable: ' + str(exc))
        with self.assertRaises(self.m.Refusal):
            self.plan('remove')
        self.assertTrue(outside.exists())

    def test_28_relative_stage(self):
        with self.assertRaises(self.m.Refusal):
            self.m.make_plan('install', 'relative', self.helper)


    def test_29_changed_mode_retained(self):
        if os.name == 'nt':
            self.skipTest('POSIX file modes require native Linux')
        self.install()
        target = self.data / self.m.DESKTOP
        target.chmod(0o600)
        p = self.plan('remove')
        self.assertIn(str(target), p['retained'])
        self.apply(p)
        self.assertTrue(target.exists())
        with self.assertRaises(self.m.Refusal):
            self.plan()

    def test_30_default_home_fallback(self):
        with mock.patch.dict(os.environ, {'XDG_DATA_HOME': '', 'HOME': ''}):
            with self.assertRaises(self.m.Refusal):
                self.m.data_home()


    @unittest.skipIf(os.name == 'nt', 'POSIX metadata endpoint checks require native Linux')
    def test_31_recover_install_mode_drift_refuses_preserves(self):
        receipt = self.apply(self.plan())
        target = self.data / self.m.DESKTOP
        before = target.read_bytes()
        target.chmod(0o600)
        with self.assertRaises(self.m.Refusal):
            self.plan('recover', receipt=receipt)
        self.assertEqual(target.read_bytes(), before)
        self.assertEqual(target.stat().st_mode & 0o777, 0o600)

    @unittest.skipIf(os.name == 'nt', 'POSIX metadata endpoint checks require native Linux')
    def test_32_recover_update_mode_drift(self):
        self.install()
        receipt = self.apply(self.plan(helper=str(self.root / 'updated')))
        target = self.data / self.m.DESKTOP
        target.chmod(0o600)
        before = target.read_bytes()
        with self.assertRaises(self.m.Refusal):
            self.plan('recover', receipt=receipt)
        self.assertEqual(target.read_bytes(), before)
        self.assertEqual(target.stat().st_mode & 0o777, 0o600)

    @unittest.skipIf(os.name == 'nt', 'POSIX metadata endpoint checks require native Linux')
    def test_33_interrupted_prefix_mode_drift(self):
        original = self.m.exclusive_write
        def fail_icon(path, data, mode=0o644):
            if str(path).endswith('raspi-link-display.svg'):
                raise OSError('injected interrupted prefix')
            return original(path, data, mode)
        with mock.patch.object(self.m, 'exclusive_write', side_effect=fail_icon):
            with self.assertRaises(OSError):
                self.install()
        receipt = next((self.data / self.m.STATE / 'recovery').glob('*.json'))
        target = self.data / self.m.DESKTOP
        target.chmod(0o600)
        with self.assertRaises(self.m.Refusal):
            self.plan('recover', receipt=str(receipt))
        self.assertTrue(target.exists())
        self.assertEqual(target.stat().st_mode & 0o777, 0o600)

    @unittest.skipIf(os.name == 'nt', 'POSIX metadata endpoint checks require native Linux')
    def test_34_restore_actual_predecessor_mode(self):
        self.install()
        target = self.data / self.m.MANIFEST
        before = target.read_bytes()
        target.chmod(0o400)
        receipt = self.apply(self.plan(helper=str(self.root / 'updated')))
        self.apply(self.plan('recover', receipt=receipt))
        self.assertEqual(target.read_bytes(), before)
        self.assertEqual(target.stat().st_mode & 0o777, 0o400)

    def test_35_old_incomplete_journal_refused(self):
        receipt = self.apply(self.plan())
        record = json.loads(Path(receipt).read_text())
        for entry in record['changes']:
            entry.pop('after_metadata', None)
        Path(receipt).write_text(json.dumps(record))
        target = self.data / self.m.DESKTOP
        before = target.read_bytes()
        with self.assertRaises(self.m.Refusal):
            self.plan('recover', receipt=receipt)
        self.assertEqual(target.read_bytes(), before)

    def test_36_recover_install_completed(self):
        receipt = self.apply(self.plan())
        self.apply(self.plan('recover', receipt=receipt))
        for rel in (self.m.DESKTOP, self.m.ICON, self.m.MANIFEST):
            self.assertFalse((self.data / rel).exists())

    def test_37_recover_update_bytes_drift(self):
        self.install()
        receipt = self.apply(self.plan(helper=str(self.root / 'updated')))
        target = self.data / self.m.DESKTOP
        target.write_bytes(b'user drift')
        with self.assertRaises(self.m.Refusal):
            self.plan('recover', receipt=receipt)
        self.assertEqual(target.read_bytes(), b'user drift')

    def test_38_snapshot_rejects_fd_identity_or_metadata_change(self):
        target = self.root / 'snapshot'
        target.write_bytes(b'constant')
        native = self.m.os.fstat
        from types import SimpleNamespace
        for field in ['st_ino', 'st_size', 'st_mtime_ns', 'st_ctime_ns', 'st_mode', 'st_uid', 'st_nlink']:
            calls = []
            def changed(fd):
                st = native(fd)
                values = {k: getattr(st, k) for k in dir(st) if k.startswith('st_')}
                calls.append(fd)
                if len(calls) == 2:
                    values[field] += 1
                return SimpleNamespace(**values)
            with self.subTest(field=field), mock.patch.object(self.m.os, 'fstat', side_effect=changed):
                with self.assertRaises(self.m.Refusal):
                    self.m.snapshot(target)

    @unittest.skipIf(os.name == 'nt', 'POSIX interrupted update requires native Linux')
    def test_39_interrupted_update_before_and_after_endpoints(self):
        self.install()
        before = {rel: (self.data / rel).read_bytes() for rel in (self.m.DESKTOP, self.m.ICON, self.m.MANIFEST)}
        original = self.m.os.replace
        def fail_manifest(src, dst):
            if str(dst).endswith('manifest.json'):
                raise OSError('injected update prefix')
            return original(src, dst)
        old = set((self.data / self.m.STATE / 'recovery').glob('*.json'))
        with mock.patch.object(self.m.os, 'replace', side_effect=fail_manifest):
            with self.assertRaises(OSError):
                self.apply(self.plan(helper=str(self.root / 'updated')))
        receipt = (set((self.data / self.m.STATE / 'recovery').glob('*.json')) - old).pop()
        self.apply(self.plan('recover', receipt=str(receipt)))
        for rel, data in before.items():
            self.assertEqual((self.data / rel).read_bytes(), data)

    @unittest.skipIf(os.name == 'nt', 'POSIX Gio fixture is Linux-only')
    def test_40_gio_metadata_and_benign_argv(self):
        # Deliberately execute ONLY private capture scripts, never the helper/app.
        try:
            from gi.repository import Gio, GLib
        except ImportError:
            self.skipTest('Gio Python bindings unavailable; native argv gate remains open')
        import time
        for name in ['plain-helper', 'a b helper', '%f% helper', '$a "b`\\helper', "single'quote; & (helper)"]:
            for unit in [None, 'raspi-link-display-desktop-test.service']:
                with self.subTest(name=name, unit=unit):
                    script = self.root / name
                    capture = self.root / 'captured.json'
                    capture.unlink(missing_ok=True)
                    script.write_text('#!/usr/bin/python3\nimport json,sys\nfrom pathlib import Path\nPath(' + repr(str(capture)) + ').write_text(json.dumps(sys.argv))\n')
                    script.chmod(0o700)
                    value = self.m.exec_value(str(script), unit)
                    data = (ROOT / 'share/applications/raspi-link-display.desktop.in').read_text().replace('@EXEC@', value)
                    key = GLib.KeyFile()
                    key.load_from_data(data, len(data.encode()), GLib.KeyFileFlags.NONE)
                    app = Gio.DesktopAppInfo.new_from_keyfile(key)
                    self.assertIsNotNone(app)
                    pids = []
                    def launched(app_info, pid, user_data):
                        pids.append(pid)
                    self.assertTrue(app.launch_uris_as_manager(
                        [], None, GLib.SpawnFlags.DO_NOT_REAP_CHILD, None, None, launched, None))
                    self.assertEqual(len(pids), 1)
                    deadline = time.monotonic() + 5
                    status = None
                    while time.monotonic() < deadline:
                        pid, child_status = os.waitpid(pids[0], os.WNOHANG)
                        if pid == pids[0]:
                            status = child_status
                            break
                        time.sleep(0.01)
                    self.assertIsNotNone(status, 'private capture child did not exit')
                    self.assertEqual(os.waitstatus_to_exitcode(status), 0)
                    self.assertTrue(capture.exists(), value)
                    expected = [str(script), 'start', '--desktop'] + (['--unit', unit] if unit else [])
                    self.assertEqual(json.loads(capture.read_text()), expected)


    @unittest.skipIf(os.name == 'nt', 'POSIX metadata endpoint checks require native Linux')
    def test_41_interrupted_update_before_endpoint_mode_drift(self):
        self.install()
        original = self.m.os.replace
        def fail_manifest(src, dst):
            if str(dst).endswith('manifest.json'):
                raise OSError('injected update prefix')
            return original(src, dst)
        old = set((self.data / self.m.STATE / 'recovery').glob('*.json'))
        with mock.patch.object(self.m.os, 'replace', side_effect=fail_manifest):
            with self.assertRaises(OSError):
                self.apply(self.plan(helper=str(self.root / 'updated')))
        receipt = (set((self.data / self.m.STATE / 'recovery').glob('*.json')) - old).pop()
        target = self.data / self.m.MANIFEST
        before = target.read_bytes()
        target.chmod(0o400)
        with self.assertRaises(self.m.Refusal):
            self.plan('recover', receipt=str(receipt))
        self.assertEqual(target.read_bytes(), before)
        self.assertEqual(target.stat().st_mode & 0o777, 0o400)

    def test_42_path_replaced_during_snapshot_refused(self):
        target = self.root / 'raced-snapshot'
        target.write_bytes(b'constant')
        original = self.m.os.fdopen
        @contextlib.contextmanager
        def replace_after_close(fd, *args, **kwargs):
            with original(fd, *args, **kwargs) as stream:
                yield stream
            # Both kernels allow replacement here. Windows forbids replacing
            # Python's still-open handle; do not skip the identity-race gate.
            replacement = self.root / 'replacement'
            replacement.write_bytes(b'constant')
            os.replace(str(replacement), str(target))
        with mock.patch.object(self.m.os, 'fdopen', side_effect=replace_after_close):
            with self.assertRaises(self.m.Refusal):
                self.m.snapshot(target)

    @unittest.skipIf(os.name == 'nt', 'POSIX mode-only restore requires native Linux')
    def test_43_mode_only_predecessor_restored(self):
        self.install()
        target = self.data / self.m.MANIFEST
        before = target.read_bytes()
        target.chmod(0o400)
        receipt = self.apply(self.plan())
        self.assertEqual(target.read_bytes(), before)
        self.assertEqual(target.stat().st_mode & 0o777, 0o600)
        self.apply(self.plan('recover', receipt=receipt))
        self.assertEqual(target.read_bytes(), before)
        self.assertEqual(target.stat().st_mode & 0o777, 0o400)


if __name__ == '__main__':
    unittest.main(verbosity=2)
