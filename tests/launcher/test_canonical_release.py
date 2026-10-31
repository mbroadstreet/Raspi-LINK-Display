"""Canonical release contracts; modeled Linux acquisition, not native acceptance.

Authentic pinned build bytes and real validation/binding/metadata policy run here.
FD/path syscalls are explicit seams on Windows; no application or control runs.
"""
import contextlib
import hashlib
import json
import os
from pathlib import Path
import stat
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import test_launcher


class CanonicalReleaseTests(unittest.TestCase):
    setUpClass = classmethod(test_launcher.LauncherTests.setUpClass.__func__)

    def setUp(self):
        fixture = Path(__file__).resolve().parent / 'fixtures' / 'pitouch-build'
        self.selection = (fixture / 'BUILD-SELECTION.json').read_bytes()
        self.receipt = (fixture / 'native-build-success.json').read_bytes()
        self.identity = (self.m.BOUNDED_ELF, self.m.BOUNDED_SELECTION_SHA,
                         self.m.BOUNDED_RECEIPT_SHA)

    def selected(self, root):
        return self.identity + (str(root / 'bin/link-pi-display'), str(root),
                                str(root / 'config/profile.conf'))

    def metadata(self, **changes):
        values = dict(st_dev=1, st_ino=2, st_mode=stat.S_IFREG | 0o600,
                      st_uid=1000, st_nlink=1, st_size=20,
                      st_mtime_ns=30, st_ctime_ns=40)
        values.update(changes)
        return SimpleNamespace(**values)

    @contextlib.contextmanager
    def acquisition(self, root, raws=None, stats=None):
        """Only native directory/FD primitives modeled; product validation is real."""
        with contextlib.ExitStack() as stack:
            stack.enter_context(patch.object(self.m, 'BOUNDED_RELEASE_ROOT', root))
            directory = stack.enter_context(patch.object(
                self.m, 'bounded_directory', return_value=contextlib.nullcontext(50)))
            for flag in ('O_NOFOLLOW', 'O_CLOEXEC', 'O_NONBLOCK'):
                if not hasattr(os, flag):
                    stack.enter_context(patch.object(os, flag, 0, create=True))
            stack.enter_context(patch.object(os, 'getuid', return_value=1000, create=True))
            opened = stack.enter_context(patch.object(os, 'open', side_effect=[51, 52]))
            closed = stack.enter_context(patch.object(os, 'close'))
            stack.enter_context(patch.object(os, 'fstat', return_value=self.metadata()))
            stack.enter_context(patch.object(os, 'stat', side_effect=stats,
                                             return_value=self.metadata()))
            read = stack.enter_context(patch.object(self.m, 'bounded_read_fd',
                side_effect=raws if raws is not None else [self.selection, self.receipt]))
            yield directory, opened, closed, read

    def test_authentic_pins_and_three_field_identity(self):
        self.assertEqual(hashlib.sha256(self.selection).hexdigest(), self.identity[1])
        self.assertEqual(hashlib.sha256(self.receipt).hexdigest(), self.identity[2])
        self.assertEqual(self.m.bounded_validate_build(self.selection, self.receipt), self.identity)
        self.assertEqual(json.loads(self.selection)['root'], self.m.BOUNDED_AS_BUILT_ROOT)

    def test_mutation_and_mutable_inputs_rejected_after_warm_validation(self):
        self.assertEqual(self.m.bounded_validate_build(self.selection, self.receipt), self.identity)
        for selection, receipt in ((self.selection+b' ', self.receipt),
                                   (self.selection, self.receipt+b' '),
                                   (bytearray(self.selection), self.receipt),
                                   (self.selection, bytearray(self.receipt))):
            with self.subTest(selection_type=type(selection), receipt_type=type(receipt)), self.assertRaises((ValueError, TypeError)):
                self.m.bounded_validate_build(selection, receipt)

    def test_helper_root_resolved_at_load_not_current_directory_or_later_file(self):
        root = Path(os.environ['TMPDIR']).resolve() / 'canonical-relocated-test'
        namespace = {'__file__': str(root / 'tools/link-displayctl'), '__name__': 'canonical_fixture'}
        exec(compile(test_launcher.SOURCE.read_bytes(), str(test_launcher.SOURCE), 'exec'), namespace)
        self.assertEqual(namespace['BOUNDED_RELEASE_ROOT'], root)
        namespace['__file__'] = str(root / 'other/tools/link-displayctl')
        with patch.object(Path, 'cwd', return_value=root / 'unrelated'):
            spec = namespace['make_spec'](namespace['parser']().parse_args(['start']), {})
        self.assertEqual(spec, dict(project=str(root), binary=str(root / 'bin/link-pi-display'),
                                    config=str(root / 'config/profile.conf'), windowed=False))

    def test_default_and_existing_precedence(self):
        root = self.m.BOUNDED_RELEASE_ROOT
        args = self.m.parser().parse_args(['start'])
        self.assertEqual(self.m.make_spec(args, {}), dict(project=str(root),
            binary=str(root / 'bin/link-pi-display'), config=str(root / 'config/profile.conf'), windowed=False))
        def spec(name):
            base = root / name
            return dict(project=str(base), binary=str(base / 'custom-app'),
                        config=str(base / 'custom.conf'), windowed=True)
        settings, previous, explicit = spec('settings'), spec('previous'), spec('explicit')
        self.assertEqual(self.m.make_spec(args, settings, previous), previous)
        args.settings = 'supplied-settings.json'
        self.assertEqual(self.m.make_spec(args, settings, previous), settings)
        for key in ('project', 'binary', 'config', 'windowed'):
            setattr(args, key, explicit[key])
        self.assertEqual(self.m.make_spec(args, settings, previous), explicit)
        args = self.m.parser().parse_args(['start', '--project', explicit['project']])
        self.assertEqual(self.m.make_spec(args, {})['binary'], str(Path(explicit['project']) / 'bin/link-pi-display'))
        self.assertEqual(self.m.make_spec(args, {})['config'], str(Path(explicit['project']) / 'config/profile.conf'))

    def test_relocation_reacquires_fixed_provenance_and_changes_only_path_identity(self):
        roots = [self.m.BOUNDED_RELEASE_ROOT / name for name in ('release-a', 'release-b')]
        results = []
        for root in roots:
            with self.acquisition(root) as (directory, opened, closed, read):
                result = self.m.bounded_build_evidence()
                self.assertEqual(result, self.selected(root))
                directory.assert_called_once_with(root / 'provenance')
                self.assertEqual([call.args[0] for call in opened.call_args_list],
                                 ['BUILD-SELECTION.json', 'native-build-success.json'])
                self.assertTrue(all(call.kwargs == {'dir_fd': 50} for call in opened.call_args_list))
                self.assertEqual([call.args[0] for call in closed.call_args_list], [51, 52])
                self.assertEqual(read.call_count, 2)
                results.append(result)
        self.assertEqual(results[0][:3], results[1][:3])
        self.assertNotEqual(self.m.bounded_build_binding(results[0]), self.m.bounded_build_binding(results[1]))

    def test_warm_validator_does_not_cache_relocated_paths_or_skip_reacquisition(self):
        root = self.m.BOUNDED_RELEASE_ROOT
        for raws, accepted in (([self.selection, self.receipt], True),
                               ([self.selection, self.receipt+b' '], False)):
            with self.acquisition(root, raws) as (_, opened, closed, read):
                if accepted:
                    self.assertEqual(self.m.bounded_build_evidence(), self.selected(root))
                else:
                    with self.assertRaises(ValueError): self.m.bounded_build_evidence()
                self.assertEqual(opened.call_count, 2)
                self.assertEqual(read.call_count, 2)
                self.assertEqual(closed.call_count, 2)

    def test_every_build_binding_component_matters(self):
        selected = self.selected(self.m.BOUNDED_RELEASE_ROOT)
        digest = self.m.bounded_build_binding(selected)
        for index in range(6):
            changed = list(selected); changed[index] += 'changed'
            with self.subTest(index=index):
                self.assertNotEqual(digest, self.m.bounded_build_binding(tuple(changed)))

    def test_old_build_binding_rejected_and_canonical_binding_accepted(self):
        selected = self.selected(self.m.BOUNDED_RELEASE_ROOT)
        old = self.m.BOUNDED_AS_BUILT_ROOT
        historical = self.identity + (old + '/candidate-build-session03/link-pi-display',
                                      old + '/source', old + '/profile.conf')
        body = dict(version=2, profile=self.m.BOUNDED_PROFILE, elf_sha256=self.m.BOUNDED_ELF,
                    boot='12345678-1234-1234-1234-123456789abc', uid=1000,
                    record_binding='a'*64, build_binding=self.m.bounded_build_binding(selected))
        with self.acquisition(self.m.BOUNDED_RELEASE_ROOT):
            raw = self.m.bounded_canonical(dict(body=body, checksum=self.m.bounded_digest(body)))
            self.assertEqual(self.m.bounded_decode(raw), body)
        body['build_binding'] = self.m.bounded_build_binding(historical)
        with self.acquisition(self.m.BOUNDED_RELEASE_ROOT), self.assertRaisesRegex(ValueError, 'Invalid bounded proof'):
            self.m.bounded_decode(self.m.bounded_canonical(dict(body=body, checksum=self.m.bounded_digest(body))))

    def test_unsafe_file_custody_refused_without_read_or_repair(self):
        for changes in (dict(st_mode=stat.S_IFREG | 0o644), dict(st_uid=1001),
                        dict(st_nlink=2), dict(st_mode=stat.S_IFLNK | 0o600),
                        dict(st_mode=stat.S_IFIFO | 0o600)):
            with self.subTest(changes=changes), self.acquisition(self.m.BOUNDED_RELEASE_ROOT) as (_, opened, closed, read), patch.object(os, 'fstat', return_value=self.metadata(**changes)):
                with self.assertRaisesRegex(ValueError, 'Unsafe proof file'):
                    self.m.bounded_build_evidence()
                read.assert_not_called()
                self.assertEqual(opened.call_count, 1)
                closed.assert_called_once_with(51)

    def test_path_replacement_or_changed_metadata_refused_and_fd_closed(self):
        for changed in (dict(st_ino=999), dict(st_size=21), dict(st_mtime_ns=31)):
            with self.subTest(changed=changed), self.acquisition(self.m.BOUNDED_RELEASE_ROOT,
                    stats=[self.metadata(**changed)]) as (_, _, closed, _):
                with self.assertRaisesRegex(ValueError, 'changed during read'):
                    self.m.bounded_build_evidence()
                closed.assert_called_once_with(51)

    def test_unsafe_directory_or_denied_acquisition_does_not_fallback(self):
        for error in (ValueError('Unsafe proof ancestor'), PermissionError('denied')):
            with self.subTest(error=type(error)), patch.object(self.m, 'bounded_directory', side_effect=error), patch.object(os, 'open') as opened:
                with self.assertRaises(type(error)): self.m.bounded_build_evidence()
                opened.assert_not_called()


if __name__ == '__main__':
    unittest.main(verbosity=2)
