"""Read-only diagnostic and desktop adapter fixtures; no native UI claim."""
import contextlib
import io
import json
import os
import sys
import re
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import test_launcher


class ObservationTests(unittest.TestCase):
    setUpClass = classmethod(test_launcher.LauncherTests.setUpClass.__func__)
    setUp = test_launcher.LauncherTests.setUp
    record = test_launcher.LauncherTests.record
    owned_state = test_launcher.LauncherTests.owned_state
    linux_main_context = test_launcher.LauncherTests.linux_main_context

    @contextlib.contextmanager
    def facts(self, state=None, record=None, pids=None, scanned=None):
        record = record or self.record()
        with contextlib.ExitStack() as s:
            mocks = {}
            values = {'unit_state': state or self.owned_state(record), 'state_path': Path('unused'),
                      'read_state': record, 'process_identity': self.record()['identity'],
                      'cgroup_pids': [123] if pids is None else pids,
                      'observation_processes': [] if scanned is None else scanned,
                      'mapped_windows': []}
            for k, v in values.items():
                mocks[k] = s.enter_context(patch.object(self.m, k, return_value=v, create=True))
            mocks['save_state'] = s.enter_context(patch.object(self.m, 'save_state'))
            yield mocks

    def test_running_is_read_only(self):
        with self.facts() as f:
            r = self.m.observe(self.record()['unit'])
            self.assertEqual(r['observed_liveness'], 'running')
            self.assertEqual(r['phase'], 'running')
            f['save_state'].assert_not_called()

    def test_absent_historical_running_bytes_unchanged(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / (self.record()['unit'] + '.json')
            p.write_text(json.dumps(self.record()))
            before = p.read_bytes()
            with self.facts(state={'LoadState': 'not-found', 'ActiveState': 'inactive', 'MainPID': '0'}, pids=[]) as f:
                f['read_state'].side_effect = lambda _: json.loads(p.read_text())
                r = self.m.observe(self.record()['unit'])
            self.assertEqual(r['phase'], 'running')
            self.assertEqual(r['observed_liveness'], 'unknown')
            self.assertEqual(p.read_bytes(), before)

    def test_failed_result_separate_and_mainpid_zero_residual(self):
        for pids, expected in (([], 'unknown'), ([456], 'residual-process')):
            st = self.owned_state(ActiveState='failed', MainPID='0', Result='exit-code', ExecMainStatus='7')
            with self.facts(state=st, pids=pids):
                r = self.m.observe(self.record()['unit'])
            self.assertEqual(r['observed_liveness'], expected)
            self.assertEqual((r['result'], r['exit_status']), ('exit-code', '7'))

    def test_active_mainpid_zero_with_authenticated_survivors_is_residual(self):
        with self.facts(state=self.owned_state(MainPID='0'), pids=[456]):
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'residual-process')
        # Observation must not weaken the original control-side refusal.
        with self.assertRaises(self.m.LaunchError):
            self.m.require_owned(self.owned_state(MainPID='0'), self.record())

    def test_absent_pinned_survivor_is_not_stopped(self):
        with self.facts(state={'LoadState': 'not-found', 'MainPID': '0'}, pids=[], scanned=[{'pid': 123, 'kind': 'owned'}]):
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'residual-process')

    def test_unmanaged_conflict(self):
        with self.facts(scanned=[{'pid': 999, 'kind': 'conflict'}]):
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'foreign/conflicting')

    def test_inspection_failures_unknown_and_unavailable(self):
        for name in ('unit_state', 'read_state', 'cgroup_pids', 'observation_processes'):
            with self.subTest(name=name), self.facts() as f:
                f[name].side_effect = self.m.LaunchError('inspection denied')
                r = self.m.observe(self.record()['unit'])
                self.assertEqual(r['observed_liveness'], 'unknown')
                self.assertTrue(r['observation_issues'])

    def test_foreign_marker_policy_and_pid_reuse(self):
        for changes in ({'Environment': ''}, {'Restart': 'always'}, {'InvocationID': 'c'*32}):
            with self.facts(state=self.owned_state(**changes)):
                self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'foreign/conflicting')
        with self.facts() as f:
            f['process_identity'].return_value = dict(self.record()['identity'], start_ticks='999')
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'foreign/conflicting')

    def test_pending_wrapper_and_transition(self):
        for active in ('activating', 'deactivating'):
            with self.facts(state=self.owned_state(ActiveState=active)):
                self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'transitional')
        record = self.record('pending')
        with self.facts(record=record) as f:
            f['process_identity'].return_value = dict(self.record()['identity'], executable=str(Path('/usr/bin/env').resolve()))
            self.assertEqual(self.m.observe(record['unit'])['observed_liveness'], 'transitional')

    def test_identity_and_manager_bracket_changes(self):
        with self.facts() as f:
            f['unit_state'].side_effect = [self.owned_state(), self.owned_state(MainPID='0')]
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'transitional')
        with self.facts() as f:
            f['process_identity'].side_effect = [self.record()['identity'], self.record()['identity'], None]
            self.assertNotEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'running')

    def test_unlinked_pin_and_literal_suffix(self):
        with self.facts() as f:
            f['process_identity'].return_value = dict(self.record()['identity'], executable=self.record()['identity']['executable']+' (deleted)', exe_unlinked=True)
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'running')
        with self.facts() as f:
            f['process_identity'].return_value = dict(self.record()['identity'], executable=self.record()['identity']['executable']+' (deleted)')
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'foreign/conflicting')

    def test_status_unknown_prints_json_and_fails(self):
        with self.facts() as f, self.linux_main_context(), contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()) as err:
            f['unit_state'].side_effect = self.m.LaunchError('manager denied')
            self.assertEqual(self.m.main(['status']), 1)
            self.assertEqual(json.loads(out.getvalue())['observed_liveness'], 'unknown')
            self.assertIn('manager denied', err.getvalue())

    def test_desktop_only_start(self):
        self.m.validate_options(self.m.parser().parse_args(['start', '--desktop']))
        for action in ('restart', 'status', 'stop', 'doctor', 'logs'):
            with self.assertRaises(self.m.LaunchError):
                self.m.validate_options(self.m.parser().parse_args([action, '--desktop']))

    def test_desktop_failure_outside_lock_original_error(self):
        held = []
        @contextlib.contextmanager
        def lock(_):
            held.append(True)
            try:
                yield
            finally:
                held.pop()
        def present(args, error):
            self.assertFalse(held)
            self.assertIn('original failure', error)
        with self.linux_main_context(), patch.object(self.m, 'lifecycle_lock', side_effect=lock), patch.object(self.m, 'start', side_effect=self.m.LaunchError('original failure')), patch.object(self.m, 'present_desktop_error', side_effect=present, create=True) as ui, contextlib.redirect_stderr(io.StringIO()) as err:
            self.assertEqual(self.m.main(['start', '--desktop']), 1)
            ui.assert_called_once()
            self.assertIn('original failure', err.getvalue())

    def test_desktop_success_quiet_no_presentation(self):
        with self.linux_main_context(), patch.object(self.m, 'start'), patch.object(self.m, 'present_desktop_error', create=True) as ui, contextlib.redirect_stdout(io.StringIO()) as out:
            self.assertEqual(self.m.main(['start', '--desktop']), 0)
            self.assertEqual(out.getvalue(), '')
            ui.assert_not_called()

    def test_error_record_bounded_private_and_symlink_refused(self):
        with tempfile.TemporaryDirectory() as d, patch.dict(os.environ, {'XDG_STATE_HOME': d}), patch.object(self.m.os, 'getuid', return_value=Path(d).stat().st_uid, create=True):
            original_stat = Path.stat
            def linux_permissions(path, *a, **kw):
                value = original_stat(path, *a, **kw)
                if os.name != 'nt':
                    return value
                from types import SimpleNamespace
                return SimpleNamespace(st_uid=value.st_uid, st_mode=(value.st_mode & ~0o777) | 0o700)
            with patch.object(Path, 'stat', linux_permissions):
                p = self.m.desktop_error_record('private\n' * 10000)
            self.assertLessEqual(p.stat().st_size, 8192)
            self.assertIn('private', p.read_text())
            if os.name != 'nt':
                self.assertEqual(p.stat().st_mode & 0o777, 0o600)
            with patch.object(Path, 'is_symlink', return_value=True):
                with self.assertRaises(self.m.LaunchError):
                    self.m.desktop_error_record('error')

    def test_record_race_is_transitional(self):
        with self.facts() as f:
            f['read_state'].side_effect = [self.record(), dict(self.record(), phase='stopped')]
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'transitional')

    def test_residual_pid_evidence_and_ownership(self):
        with self.facts(state={'LoadState': 'not-found', 'MainPID': '0'}, pids=[], scanned=[{'pid': 123, 'kind': 'owned'}]):
            r = self.m.observe(self.record()['unit'])
            self.assertIn(123, r['owned_pids'])
            self.assertEqual(r['ownership'], 'verified')
            self.assertTrue(r['observation_reasons'])

    def test_presenter_launch_failure_uses_xmessage(self):
        import subprocess
        def invoke(argv, **kwargs):
            if '--error' in argv:
                raise self.m.LaunchError('zenity failed')
            return subprocess.CompletedProcess(argv, 0, '', '')
        with self.linux_main_context(), self.facts(), patch.object(self.m, 'desktop_error_record'), patch.object(self.m, 'stock_presenter', side_effect=lambda name: '/usr/bin/' + name), patch.object(self.m, 'command', side_effect=invoke) as run:
            self.m.present_desktop_error(self.m.parser().parse_args(['start', '--desktop']), 'error')
            self.assertEqual(run.call_args.args[0][0], '/usr/bin/xmessage')

    def test_doctor_uses_observation_and_does_not_lock(self):
        with self.linux_main_context(), self.facts(), patch.object(self.m, 'unmanaged_instances', return_value=[]), patch.object(self.m, 'lifecycle_lock') as lock, contextlib.redirect_stdout(io.StringIO()) as out:
            self.assertEqual(self.m.main(['doctor']), 0)
            self.assertEqual(json.loads(out.getvalue())['observation']['observed_liveness'], 'running')
            lock.assert_not_called()

    def test_status_does_not_lock_or_save(self):
        with self.linux_main_context(), self.facts() as f, patch.object(self.m, 'lifecycle_lock') as lock, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(self.m.main(['status']), 0)
            lock.assert_not_called()
            f['save_state'].assert_not_called()

    def test_native_scan_missing_proc_and_permission_are_not_absence(self):
        with tempfile.TemporaryDirectory() as d, patch.object(self.m, 'PROC_ROOT', Path(d) / 'missing'):
            with self.assertRaises(self.m.LaunchError):
                self.m.observation_processes(self.record()['unit'], self.record(), set())
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / '123'
            p.mkdir()
            with patch.object(self.m, 'PROC_ROOT', Path(d)), patch.object(self.m.os, 'getuid', return_value=p.stat().st_uid, create=True), patch.object(Path, 'read_text', side_effect=PermissionError('denied')):
                with self.assertRaises(self.m.LaunchError):
                    self.m.observation_processes(self.record()['unit'], self.record(), set())

    def test_native_scan_retains_unit_group_conflict_and_pin(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / '123'
            p.mkdir()
            (p / 'status').write_text('Uid: '+ ' '.join([str(p.stat().st_uid)]*4)+'\n')
            (p / 'stat').write_text('123 (app) S ' + ' '.join(str(n) for n in range(4, 23)))
            (p / 'cgroup').write_text('0::/user.slice/' + self.record()['unit'] + '\n')
            (p / 'cmdline').write_bytes(b'/other/app\0')
            identity = dict(self.record()['identity'], start_ticks='22')
            with patch.object(self.m, 'PROC_ROOT', Path(d)), patch.object(self.m.os, 'getuid', return_value=p.stat().st_uid, create=True), patch.object(self.m, 'process_identity', return_value=identity):
                self.assertEqual(self.m.observation_processes(self.record()['unit'], self.record(), set()), [{'pid': 123, 'kind': 'conflict'}])
                record = self.record()
                record['identity'] = identity
                self.assertEqual(self.m.observation_processes(record['unit'], record, set()), [{'pid': 123, 'kind': 'owned'}])

    @contextlib.contextmanager
    def denied_scan(self, *, argv=b'/usr/lib/systemd/systemd\0', group=None, pid=777, owned=False):
        """Synthetic proc discovery; only the executable permission boundary is mocked."""
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / str(pid)
            p.mkdir()
            (p / 'status').write_text('Uid: '+ ' '.join([str(p.stat().st_uid)]*4)+'\n')
            (p / 'stat').write_text(str(pid) + ' (systemd) S ' + ' '.join(str(n) for n in range(4, 23)))
            (p / 'cgroup').write_text('0::' + (group or '/unrelated/init.scope') + '\n')
            (p / 'cmdline').write_bytes(argv)
            original_stat = Path.stat
            def denied(path, *a, **kw):
                if path == p / 'exe':
                    raise PermissionError('protected executable')
                return original_stat(path, *a, **kw)
            with patch.object(self.m, 'PROC_ROOT', Path(d)), patch.object(self.m.os, 'getuid', return_value=p.stat().st_uid, create=True), patch.object(Path, 'stat', denied):
                yield p

    def test_unrelated_exe_denial_preserves_running_with_explicit_coverage(self):
        native_scan = self.m.observation_processes
        native_identity = self.m.process_identity
        with self.facts() as f, self.denied_scan():
            f['process_identity'].side_effect = lambda pid: self.record()['identity'] if pid == 123 else native_identity(pid)
            f['observation_processes'].side_effect = native_scan
            r = self.m.status(self.record()['unit'], emit=False)
            self.assertEqual(r['observed_liveness'], 'running')
            self.assertEqual(r['owned_pids'], [123])
            self.assertFalse(r['discovery_coverage']['complete'])
            self.assertEqual(r['discovery_coverage']['limitations'][0]['pid'], 777)
            self.assertIn('conflict_discovery', r['unavailable_facts'])
            self.assertEqual(r['observation_issues'], [])
            f['save_state'].assert_not_called()

    def test_denied_potential_candidates_and_owned_remain_errors(self):
        cases = ({'argv': b'/opt/link-pi-display\0'},
                 {'group': '/user.slice/' + self.record()['unit']},
                 {'pid': 123}, {'argv': b''})
        for case in cases:
            with self.subTest(case=case), self.denied_scan(**case):
                with self.assertRaises(self.m.LaunchError):
                    self.m.observation_processes(self.record()['unit'], self.record(), set())
        with self.denied_scan():
            with self.assertRaises(self.m.LaunchError):
                self.m.observation_processes(self.record()['unit'], self.record(), {777})

    def test_limited_discovery_cannot_prove_stopped_or_residual(self):
        native_scan = self.m.observation_processes
        native_identity = self.m.process_identity
        for state, pids in (({'LoadState': 'not-found', 'MainPID': '0'}, []),
                            (self.owned_state(MainPID='0'), [456])):
            with self.facts(state=state, pids=pids) as f, self.denied_scan():
                f['process_identity'].side_effect = lambda pid: self.record()['identity'] if pid == 123 else native_identity(pid)
                f['observation_processes'].side_effect = native_scan
                r = self.m.observe(self.record()['unit'])
                self.assertEqual(r['observed_liveness'], 'unknown')
                self.assertIsNone(r['owned_pids'])
                with self.assertRaises(self.m.LaunchError):
                    self.m.require_observation(r)
                f['save_state'].assert_not_called()

    def test_unrelated_identity_failure_without_permission_is_not_ignored(self):
        with self.denied_scan() as p, patch.object(Path, 'stat', return_value=p.stat()), patch.object(self.m, 'process_identity', return_value=None), patch.object(self.m.os, 'readlink', return_value='/usr/bin/other'):
            with self.assertRaises(self.m.LaunchError):
                self.m.observation_processes(self.record()['unit'], self.record(), set())

    def test_denied_comm_candidate_is_not_hidden_by_unrelated_argv(self):
        with self.denied_scan() as p:
            (p / 'stat').write_text('777 (link-pi-display) S ' + ' '.join(str(n) for n in range(4, 23)))
            with self.assertRaises(self.m.LaunchError):
                self.m.observation_processes(self.record()['unit'], self.record(), set())

    def test_denied_discovery_churn_is_not_a_coverage_only_warning(self):
        with self.denied_scan() as p:
            native_identity = self.m.process_identity
            def churn(pid):
                (p / 'cgroup').write_text('0::/changed/scope\n')
                return native_identity(pid)
            with patch.object(self.m, 'process_identity', side_effect=churn):
                with self.assertRaises(self.m.LaunchError):
                    self.m.observation_processes(self.record()['unit'], self.record(), set())

    def test_denied_candidate_status_is_unknown_not_running(self):
        native_scan = self.m.observation_processes
        native_identity = self.m.process_identity
        for case in ({'argv': b'/opt/link-pi-display\0'}, {'argv': b''},
                     {'group': '/user.slice/' + self.record()['unit']}):
            with self.subTest(case=case), self.facts() as f, self.denied_scan(**case):
                f['process_identity'].side_effect = lambda pid: self.record()['identity'] if pid == 123 else native_identity(pid)
                f['observation_processes'].side_effect = native_scan
                r = self.m.observe(self.record()['unit'])
                self.assertEqual(r['observed_liveness'], 'unknown')
                self.assertIsNone(r['owned_pids'])
                with self.assertRaises(self.m.LaunchError):
                    self.m.require_observation(r)
                f['save_state'].assert_not_called()

    def test_owned_cgroup_denial_is_unknown_and_refuses(self):
        with self.facts() as f:
            f['cgroup_pids'].side_effect = self.m.LaunchError('native cgroup denied')
            r = self.m.observe(self.record()['unit'])
            self.assertEqual(r['observed_liveness'], 'unknown')
            with self.assertRaises(self.m.LaunchError):
                self.m.require_observation(r)
            f['save_state'].assert_not_called()

    def test_known_conflict_wins_over_incomplete_discovery(self):
        scanned = self.m.ProcessDiscovery()
        scanned.append({'pid': 999, 'kind': 'conflict'})
        scanned.coverage = {'complete': False, 'limitations': [{'pid': 777, 'reason': 'denied'}]}
        with self.facts(scanned=scanned):
            r = self.m.observe(self.record()['unit'])
            self.assertEqual(r['observed_liveness'], 'foreign/conflicting')
            self.assertIsNone(r['owned_pids'])
            with self.assertRaises(self.m.LaunchError):
                self.m.require_observation(r)

    def test_discovery_match_never_grants_native_ownership(self):
        with self.denied_scan(argv=b'/opt/link-pi-display\0') as p:
            identity = dict(self.record()['identity'], pid=777, start_ticks='22', executable='/usr/bin/other')
            with patch.object(self.m, 'process_identity', return_value=identity):
                self.assertEqual(self.m.observation_processes(self.record()['unit'], self.record(), set()),
                                 [{'pid': 777, 'kind': 'conflict'}])

    def test_cgroup_collection_has_observation_budget(self):
        with self.facts() as f:
            self.m.observe(self.record()['unit'])
            self.assertIn('deadline', f['cgroup_pids'].call_args.kwargs)

    def test_stock_presenter_cannot_use_user_path_override(self):
        with patch.object(self.m.shutil, 'which', return_value='/home/pi/bin/zenity'):
            self.assertIsNone(self.m.stock_presenter('zenity'))

    def test_foreign_does_not_claim_owned_pids(self):
        with self.facts(state=self.owned_state(Environment='')):
            r = self.m.observe(self.record()['unit'])
            self.assertEqual(r['ownership'], 'unverified')
            self.assertIsNone(r['owned_pids'])

    def test_window_observation_total_budget(self):
        import subprocess
        with patch.object(self.m.shutil, 'which', return_value='/usr/bin/tool'), patch.object(self.m.time, 'monotonic', side_effect=[100, 104, 104, 104]), patch.object(self.m, 'command', return_value=subprocess.CompletedProcess([], 0, '0x1', '')):
            with self.assertRaises(self.m.LaunchError):
                self.m.mapped_windows(123, self.record()['desktop'])

    def test_main_read_failure_during_ownership_check_is_unknown(self):
        with self.facts() as f:
            f['process_identity'].side_effect = [self.record()['identity'], None, None]
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'], 'unknown')

    def test_ui_generic_plain_bounded_validated_environment(self):
        with self.linux_main_context(), self.facts(), patch.object(self.m, 'desktop_error_record', return_value=Path('private')), patch.object(self.m, 'stock_presenter', return_value='/usr/bin/zenity'), patch.object(self.m, 'command') as run:
            self.m.present_desktop_error(self.m.parser().parse_args(['start', '--desktop']), '<secret> DISPLAY=private')
            argv = run.call_args_list[0].args[0]
            self.assertIn('--no-markup', argv)
            self.assertNotIn('<secret>', ' '.join(argv))
            self.assertLessEqual(run.call_args.kwargs['timeout'], 20)


    @contextlib.contextmanager
    def late_boundary(self, f, boundary):
        """Change only the closing bracket; keep initial authentication valid."""
        state, record = self.owned_state(), self.record()
        f['unit_state'].side_effect = [state, dict(state, MainPID='0', ActiveState='deactivating')] if boundary == 'manager' else None
        f['process_identity'].side_effect = [record['identity'], record['identity'], None] if boundary == 'identity' else None
        f['read_state'].side_effect = [record, dict(record, phase='stopped')] if boundary == 'record' else None
        yield

    def test_late_bracket_incomplete_coverage_is_unknown(self):
        import copy
        for boundary in ('manager', 'identity', 'record'):
            scanned = self.m.ProcessDiscovery()
            scanned.coverage = {'complete': False, 'limitations': [{'pid': 777, 'reason': 'unrelated exe PermissionError'}]}
            with self.subTest(boundary=boundary), self.facts(scanned=scanned) as f, self.late_boundary(f, boundary):
                original = copy.deepcopy(self.record())
                r = self.m.observe(original['unit'])
                self.assertEqual(r['observed_liveness'], 'unknown')
                self.assertEqual(r['ownership'], 'unverified')
                self.assertIsNone(r['owned_pids'])
                self.assertFalse(r['discovery_coverage']['complete'])
                self.assertIn('conflict_discovery', r['unavailable_facts'])
                self.assertIn('native_liveness', r['unavailable_facts'])
                self.assertTrue(r['observation_issues'])
                with self.assertRaises(self.m.LaunchError):
                    self.m.require_observation(r)
                self.assertEqual(self.record(), original)
                f['save_state'].assert_not_called()

    def test_complete_coverage_late_bracket_contract_unchanged(self):
        for boundary in ('manager', 'identity', 'record'):
            with self.subTest(boundary=boundary), self.facts() as f, self.late_boundary(f, boundary):
                r = self.m.observe(self.record()['unit'])
                self.assertEqual(r['observed_liveness'], 'transitional')
                self.assertTrue(r['discovery_coverage']['complete'])
                self.m.require_observation(r)
                f['save_state'].assert_not_called()

    def test_late_bracket_actual_status_and_start_consumers_refuse(self):
        for boundary in ('manager', 'identity', 'record'):
            for action in ('status', 'already-running', 'final-start'):
                for verbose in (False, True):
                    scanned = self.m.ProcessDiscovery()
                    scanned.coverage = {'complete': False, 'limitations': [{'pid': 777, 'reason': 'unrelated exe PermissionError'}]}
                    with self.subTest(boundary=boundary, action=action, verbose=verbose), self.facts(scanned=scanned) as f, self.linux_main_context(), contextlib.redirect_stdout(io.StringIO()) as out, contextlib.redirect_stderr(io.StringIO()) as err, patch.object(self.m, 'reconcile_record', return_value=self.record()), patch.object(self.m, 'reject_unmanaged'), patch.object(self.m, 'command') as controls, patch.object(self.m, 'make_spec', return_value=self.record()['spec']):
                        native_observe = self.m.observe
                        observations = []
                        def inspect(unit):
                            with self.late_boundary(f, boundary):
                                r = native_observe(unit)
                            observations.append(r)
                            return r
                        if action == 'final-start':
                            f['unit_state'].side_effect = [{'LoadState': 'not-found', 'ActiveState': 'inactive', 'MainPID': '0'}, self.owned_state(), self.owned_state()]
                            f['cgroup_pids'].side_effect = [[], [123]]
                            f['mapped_windows'].return_value = ['0xfixture']
                        with patch.object(self.m, 'observe', side_effect=inspect):
                            argv = ['status' if action == 'status' else 'start'] + (['--verbose'] if verbose and action != 'status' else [])
                            self.assertEqual(self.m.main(argv), 1)
                        self.assertEqual(len(observations), 1)
                        r = observations[0]
                        self.assertEqual(r['observed_liveness'], 'unknown')
                        self.assertEqual(r['ownership'], 'unverified')
                        self.assertIsNone(r['owned_pids'])
                        self.assertFalse(r['discovery_coverage']['complete'])
                        self.assertTrue(err.getvalue())
                        self.assertNotIn('Already running;', out.getvalue())
                        self.assertNotIn('Started independently', out.getvalue())
                        if action == 'status':
                            self.assertEqual(json.loads(out.getvalue())['observed_liveness'], 'unknown')
                        else:
                            self.assertEqual(out.getvalue(), '')
                        self.assertEqual(controls.call_count, 1 if action == 'final-start' else 0)
                        if action != 'final-start':
                            f['save_state'].assert_not_called()




class BoundedObservationTests(unittest.TestCase):
    """Real v2 validator/observer; native acquisition seams remain modeled.

    Immutable provenance bytes are authentic accepted PiTouch build evidence,
    not target sidecars. Proof envelopes below are in-memory test fixtures only.
    """
    @classmethod
    def setUpClass(cls):
        test_launcher.LauncherTests.setUpClass.__func__(cls)
        fixture = Path(__file__).resolve().parent / 'fixtures' / 'pitouch-build'
        cls.selection_raw = (fixture / 'BUILD-SELECTION.json').read_bytes()
        cls.receipt_raw = (fixture / 'native-build-success.json').read_bytes()
        root = cls.m.BOUNDED_RELEASE_ROOT
        cls.accepted = cls.m.bounded_validate_build(cls.selection_raw, cls.receipt_raw) + (
            str(root / 'bin/link-pi-display'), str(root), str(root / 'config/profile.conf'))

    def setUp(self):
        test_launcher.LauncherTests.setUp(self)
        def evidence(deadline=None):
            if deadline is not None:
                self.m.bounded_remaining(deadline)
            root = self.m.BOUNDED_RELEASE_ROOT
            return self.m.bounded_validate_build(self.selection_raw, self.receipt_raw) + (
                str(root / 'bin/link-pi-display'), str(root), str(root / 'config/profile.conf'))
        # Replace unavailable native-path acquisition, not hash/schema/selection validation.
        acquisition = patch.object(self.m, 'bounded_build_evidence', side_effect=evidence)
        acquisition.start()
        self.addCleanup(acquisition.stop)
    record = test_launcher.LauncherTests.record
    owned_state = test_launcher.LauncherTests.owned_state
    linux_main_context = test_launcher.LauncherTests.linux_main_context
    facts = ObservationTests.facts

    def proof(self, record=None):
        r = record or self.record()
        body = {'version':2,'profile':self.m.BOUNDED_PROFILE,'elf_sha256':self.m.BOUNDED_ELF,
                'boot':'12345678-1234-1234-1234-123456789abc','uid':1000,'record_binding':self.m.bounded_binding(r),
                'build_binding':self.m.bounded_build_binding(self.accepted)}
        return self.m.bounded_canonical({'body':body,'checksum':self.m.bounded_digest(body)})

    @contextlib.contextmanager
    def bounded_facts(self, boundary=None, coverage=False):
        import copy
        record = self.record()
        state = self.owned_state(ActiveState='failed',MainPID='0',Result='exit-code',ExecMainStatus='7')
        scanned = self.m.ProcessDiscovery()
        scanned.coverage = {'complete':coverage,'limitations':[] if coverage else [{'pid':777,'reason':'unrelated protected exe; hidden legacy conflict not excluded'}]}
        raw = self.proof(record)
        context = ('12345678-1234-1234-1234-123456789abc',1000,('ns',),('mount',),1)
        reads = [(None,None),(None,None)]
        proofs = [(raw,('safe-metadata',)),(raw,('safe-metadata',))]
        contexts = [context]*4
        closing_record = copy.deepcopy(record)
        closing_state = dict(state)
        if boundary == 'record': closing_record['phase'] = 'stopped'
        if boundary == 'manager': closing_state['MainPID'] = '123'
        if boundary == 'proof': proofs[-1] = (raw+b' ',('safe-metadata',))
        if boundary == 'boot': contexts[-1] = ('other-boot',)+context[1:]
        if boundary == 'absence': reads[-1] = ('999','999')
        with self.facts(state=state,record=record,pids=[],scanned=scanned) as f, patch.object(self.m,'bounded_proof_read',side_effect=proofs), patch.object(self.m,'bounded_context',side_effect=contexts), patch.object(self.m,'bounded_absence',side_effect=reads), patch.object(self.m.os,'getuid',return_value=1000,create=True):
            f['unit_state'].side_effect = [state,closing_state]
            f['read_state'].side_effect = [record,closing_record]
            yield f

    def test_bound_stopped_preserves_global_uncertainty_and_failure_result(self):
        with self.bounded_facts() as f:
            r = self.m.status(self.record()['unit'],emit=False)
            self.assertEqual(r['observed_liveness'],'stopped')
            self.assertEqual(r['claim_scope'],'recorded exact-app invocation only')
            self.assertEqual(r['invocation_proof'],'verified-bounded-diagnostic')
            self.assertEqual(r['ownership'],'unverified')
            self.assertEqual((r['result'],r['exit_status']),('exit-code','7'))
            self.assertFalse(r['discovery_coverage']['complete'])
            self.assertIn('conflict_discovery',r['unavailable_facts'])
            f['save_state'].assert_not_called()

    def test_all_closing_boundaries_unknown_even_complete_discovery(self):
        for boundary in ('record','manager','proof','boot','absence'):
            for coverage in (False,True):
                with self.subTest(boundary=boundary,coverage=coverage), self.bounded_facts(boundary,coverage):
                    r = self.m.observe(self.record()['unit'])
                    self.assertEqual(r['observed_liveness'],'unknown')
                    self.assertIsNone(r['owned_pids'])
                    self.assertIn('native_liveness',r['unavailable_facts'])

    def test_actual_consumers_no_false_success_at_each_late_boundary(self):
        # The observer is real; lifecycle/native acquisitions are seam fixtures.
        for boundary in ('record','manager','proof','boot','absence'):
            for action in ('status','doctor','already-running','final-start'):
                for verbose in (False,True):
                    with contextlib.ExitStack() as stack:
                        stack.enter_context(self.subTest(boundary=boundary,action=action,verbose=verbose))
                        stack.enter_context(self.linux_main_context())
                        stack.enter_context(patch.object(self.m,'reconcile_record',return_value=self.record()))
                        stack.enter_context(patch.object(self.m,'make_spec',return_value=self.record()['spec']))
                        stack.enter_context(patch.object(self.m,'reject_unmanaged'))
                        stack.enter_context(patch.object(self.m,'unmanaged_instances',return_value=[]))
                        producer = stack.enter_context(patch.object(self.m,'capture_bounded_proof',return_value=False))
                        stack.enter_context(patch.object(self.m,'save_state'))
                        stack.enter_context(patch.object(self.m,'state_path',return_value=Path('unused')))
                        stack.enter_context(patch.object(self.m,'read_state',return_value=self.record()))
                        stack.enter_context(patch.object(self.m,'process_identity',return_value=self.record()['identity']))
                        stack.enter_context(patch.object(self.m,'mapped_windows',return_value=['0xfixture']))
                        command = stack.enter_context(patch.object(self.m,'command'))
                        out = stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
                        err = stack.enter_context(contextlib.redirect_stderr(io.StringIO()))
                        native_observe = self.m.observe
                        seen = []
                        def inspect(unit):
                            with self.bounded_facts(boundary):
                                r = native_observe(unit)
                            seen.append(r)
                            return r
                        states = [self.owned_state()]
                        groups = [[123]]
                        if action == 'final-start':
                            states = [{'LoadState':'not-found','ActiveState':'inactive','MainPID':'0'},self.owned_state(),self.owned_state()]
                            groups = [[]]
                        with patch.object(self.m,'unit_state',side_effect=states), patch.object(self.m,'cgroup_pids',side_effect=groups), patch.object(self.m,'observe',side_effect=inspect):
                            argv = [action if action in ('status','doctor') else 'start']
                            if verbose and action not in ('status','doctor'): argv.append('--verbose')
                            self.assertEqual(self.m.main(argv),1)
                        self.assertEqual(len(seen),1)
                        self.assertEqual(seen[0]['observed_liveness'],'unknown')
                        self.assertTrue(err.getvalue())
                        self.assertNotIn('Already running;',out.getvalue())
                        self.assertNotIn('Started independently',out.getvalue())
                        if action in ('already-running','final-start'): self.assertEqual(out.getvalue(),'')
                        if action in ('status','doctor'): producer.assert_not_called()
                        self.assertEqual(command.call_count,1 if action == 'final-start' else 0)

    def test_observers_never_capture_reconcile_lock_or_write(self):
        with self.bounded_facts(), patch.object(self.m,'capture_bounded_proof') as producer, patch.object(self.m,'reconcile_record') as reconcile, patch.object(self.m,'lifecycle_lock') as lock, patch.object(self.m,'bounded_publish') as publish:
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'],'stopped')
            for mock in (producer,reconcile,lock,publish): mock.assert_not_called()

    def test_missing_proof_custom_legacy_no_record_never_stopped(self):
        for coverage in (True,False):
            for kind in ('custom','legacy','no-record'):
                r = self.record()
                if kind == 'legacy': r['provenance'] = {'kind':'verified-legacy','invocation_id':r['invocation_id'],'identity':r['identity'],'control_group':'/user.slice/test.service'}
                scanned = self.m.ProcessDiscovery()
                scanned.coverage = {'complete':coverage,'limitations':[] if coverage else [{'reason':'denied'}]}
                with self.subTest(kind=kind,coverage=coverage), self.facts(state={'Id':r['unit'],'LoadState':'not-found','MainPID':'0','ActiveState':'inactive'},record=r,pids=[],scanned=scanned) as f, patch.object(self.m,'bounded_proof_read',side_effect=FileNotFoundError):
                    if kind == 'no-record': f['read_state'].return_value = None
                    self.assertEqual(self.m.observe(r['unit'])['observed_liveness'],'unknown')

    def test_wire_strict_schema_types_integrity_and_ranges(self):
        import copy
        raw = self.proof(); body = self.m.bounded_decode(raw)
        cases = [b'[]',b'{}',raw+b'{}',b'x'*16385,b'\xff',b'{"body":{},"body":{}}']
        for k,v in [('uid',True),('uid',1000.0),('uid',2**32),('version',True),('version',1),('version',1.0),('build_binding','0'*64),('profile','custom'),('elf_sha256','0'*64),('boot','wrong'),('extra',1),('record_binding','bad')]:
            b = copy.deepcopy(body); b[k] = v
            cases.append(self.m.bounded_canonical({'body':b,'checksum':self.m.bounded_digest(b)}))
        for case in cases:
            with self.subTest(case=case[:40]), self.assertRaises((ValueError,TypeError)):
                self.m.bounded_decode(case)

    def test_phase_only_historical_binding_and_every_identity_component(self):
        import copy
        r = self.record(); digest = self.m.bounded_binding(r)
        self.assertEqual(digest,self.m.bounded_binding(dict(r,phase='stopped')))
        for field,value in [('invocation_id','d'*32),('session','B'),('phase','pending')]:
            changed = dict(r,**{field:value})
            if field != 'phase': self.assertNotEqual(digest,self.m.bounded_binding(changed))
        for key,value in [('pid',2**31),('exe_device',2**64),('exe_inode',True),('start_ticks',str(2**64))]:
            changed = copy.deepcopy(r); changed['identity'][key] = value
            with self.assertRaises(ValueError): self.m.bounded_binding(changed)

    def test_environment_complete_allowlist_and_override_policy(self):
        r = self.record()
        values = dict(r['desktop'],SDL_VIDEODRIVER='x11',**{self.m.MARKER_KEY:r['provenance']['marker']})
        def wire(d): return b'\0'.join((k+'='+v).encode() for k,v in d.items())+b'\0'
        self.m.bounded_environment(wire(values),r)
        for prefix in ('LD_','SDL_','LIBGL_','MESA_','GALLIUM_','EGL_','GBM_','__GL_','__EGL_'):
            with self.subTest(prefix=prefix), self.assertRaises(ValueError): self.m.bounded_environment(wire(dict(values,**{prefix+'TEST':'1'})),r)
        for key in self.m.ENV_KEYS | {self.m.MARKER_KEY,'SDL_VIDEODRIVER'}:
            changed = dict(values); changed[key] = 'mismatch'
            with self.subTest(key=key), self.assertRaises(ValueError): self.m.bounded_environment(wire(changed),r)
        for bad in (wire(values)[:-1],wire(values)+b'DISPLAY=:0\0',wire(values)+b'MALFORMED\0',b'x'*65537):
            with self.assertRaises(ValueError): self.m.bounded_environment(bad,r)
        changed = dict(values); del changed['XAUTHORITY']
        with self.assertRaises(ValueError): self.m.bounded_environment(wire(changed),r)

    def test_uid_selection_status_before_directory_owner_and_fail_closed(self):
        # A root-owned proc directory may expose four same-user status UIDs.
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)/'777'; p.mkdir()
            (p/'status').write_text('Uid: 1000 1000 1000 1000\n')
            (p/'stat').write_text('777 (other) S '+' '.join(['0']*18+['99']))
            (p/'cgroup').write_text('0::/other\n'); (p/'cmdline').write_bytes(b'/other\0')
            identity = dict(self.record()['identity'],pid=777,start_ticks='99',executable='/other')
            with patch.object(self.m,'PROC_ROOT',Path(d)), patch.object(self.m.os,'getuid',return_value=1000,create=True), patch.object(self.m,'process_identity',return_value=identity):
                self.assertEqual(self.m.observation_processes(self.record()['unit'],self.record(),set()),[])
                for status in ('Uid: 0 1000 0 0\n','Uid: 1000 1000 1000\n',''):
                    (p/'status').write_text(status)
                    with self.assertRaises(self.m.LaunchError): self.m.observation_processes(self.record()['unit'],self.record(),set())
                (p/'status').write_text('Uid: 0 0 0 0\n')
                self.assertEqual(self.m.observation_processes(self.record()['unit'],self.record(),set()),[])

    def test_known_conflict_sticky_after_later_native_failure(self):
        with self.facts(state=self.owned_state(Restart='always')) as f:
            f['cgroup_pids'].side_effect = self.m.LaunchError('later denial')
            self.assertEqual(self.m.observe(self.record()['unit'])['observed_liveness'],'foreign/conflicting')

    def test_manager_subprocess_shared_remaining_deadline(self):
        import subprocess
        state = self.owned_state(); del state['ExecStop']; del state['ExecStopPost']
        calls = []
        def command(argv,**kw):
            calls.append(kw['timeout'])
            return subprocess.CompletedProcess(argv,0,'\n'.join(k+'='+v for k,v in state.items()) if argv[0]=='systemctl' else 'a(sasb) 0','')
        with patch.object(self.m,'command',side_effect=command):
            self.m.unit_state(self.record()['unit'],deadline=self.m.time.monotonic()+0.5)
        self.assertEqual(len(calls),3)
        self.assertTrue(all(0 < t <= .5 for t in calls))
        self.assertEqual(calls,sorted(calls,reverse=True))

    def test_genuine_start_capture_runs_under_existing_lock_before_final_report(self):
        for verbose in (False, True):
            for refusal in (False, True):
                with self.subTest(verbose=verbose,refusal=refusal), contextlib.ExitStack() as stack:
                    stack.enter_context(self.linux_main_context())
                    held, order = [], []
                    @contextlib.contextmanager
                    def lock(unit):
                        held.append(True)
                        try: yield
                        finally: held.pop()
                    stack.enter_context(patch.object(self.m, 'lifecycle_lock', side_effect=lock))
                    stack.enter_context(patch.object(self.m, 'unit_state', side_effect=[{'LoadState':'not-found','ActiveState':'inactive','MainPID':'0'},self.owned_state(),self.owned_state()]))
                    stack.enter_context(patch.object(self.m, 'state_path', return_value=Path('unused')))
                    stack.enter_context(patch.object(self.m, 'read_state', return_value=self.record()))
                    stack.enter_context(patch.object(self.m, 'reconcile_record', return_value=self.record()))
                    stack.enter_context(patch.object(self.m, 'cgroup_pids', return_value=[]))
                    stack.enter_context(patch.object(self.m, 'make_spec', return_value=self.record()['spec']))
                    stack.enter_context(patch.object(self.m, 'reject_unmanaged'))
                    stack.enter_context(patch.object(self.m, 'save_state'))
                    stack.enter_context(patch.object(self.m, 'command'))
                    stack.enter_context(patch.object(self.m, 'mapped_windows', return_value=['0xfixture']))
                    def capture(unit, record, path):
                        self.assertTrue(held)
                        self.assertEqual(record, self.record())
                        order.append('capture')
                        return not refusal
                    stack.enter_context(patch.object(self.m, 'capture_bounded_proof', side_effect=capture))
                    def final(unit, emit=True):
                        order.append('status')
                        return {'observed_liveness':'running'}
                    stack.enter_context(patch.object(self.m, 'status', side_effect=final))
                    out = stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
                    self.assertEqual(self.m.main(['start']+(['--verbose'] if verbose else [])),0)
                    self.assertEqual(order,['capture','status'])
                    self.assertFalse(held)
                    if not verbose: self.assertEqual(out.getvalue(),'')

    def test_actual_snapshot_rejects_copied_bindings_without_upgrade(self):
        import copy
        original = self.record()
        raw = self.proof(original)
        context = ('12345678-1234-1234-1234-123456789abc',1000,(),(),1)
        for boundary in ('unit','marker','invocation','spec','desktop','uid','boot'):
            record = copy.deepcopy(original); current = context
            if boundary == 'unit': record['unit'] = 'raspi-link-display-other.service'
            if boundary == 'marker': record['provenance']['marker'] = 'c'*32
            if boundary == 'invocation': record['invocation_id'] = 'c'*32
            if boundary == 'spec': record['spec']['windowed'] = True
            if boundary == 'desktop': record['desktop']['DISPLAY'] = ':9'
            if boundary == 'uid': current = (context[0],1001)+context[2:]
            if boundary == 'boot': current = ('87654321-1234-1234-1234-123456789abc',)+context[1:]
            with self.subTest(boundary=boundary), patch.object(self.m,'state_path',return_value=Path('unused')), patch.object(self.m,'bounded_proof_read',return_value=(raw,('safe',))), patch.object(self.m,'bounded_context',return_value=current), patch.object(self.m,'bounded_absence',return_value=(None,None)), patch.object(self.m,'save_state') as save:
                self.assertIsNone(self.m.bounded_snapshot(record['unit'],record))
                save.assert_not_called()

    def test_custom_filter_no_manager_calls_or_sidecar(self):
        with tempfile.TemporaryDirectory() as d:
            binary = Path(d)/'script'; binary.write_bytes(b'#!/bin/sh\n')
            r = self.record(); r['spec']['binary'] = str(binary); r['identity']['executable'] = str(binary)
            with patch.object(self.m,'unit_state') as manager, patch.object(self.m,'bounded_publish') as publish:
                self.assertFalse(self.m.capture_bounded_proof(r['unit'],r,Path(d)/'state'))
                manager.assert_not_called(); publish.assert_not_called()



class Correction02StatusTests(unittest.TestCase):
    setUpClass = classmethod(test_launcher.LauncherTests.setUpClass.__func__)
    setUp = test_launcher.LauncherTests.setUp
    record = test_launcher.LauncherTests.record

    def test_utf8_name_same_user_and_uniform_foreign_selection(self):
        with tempfile.TemporaryDirectory() as d:
            proc = Path(d); p = proc/'987'; p.mkdir()
            (p/'stat').write_text('987 (caf\u00e9) S '+' '.join(['0']*18+['456']), encoding='utf-8')
            (p/'cgroup').write_text('0::/unrelated\n')
            (p/'cmdline').write_bytes(b'/unrelated\0')
            identity = dict(self.record()['identity'], pid=987, executable='/unrelated')
            for uid in (1000,1001):
                # Foreign UID replacement is explicitly synthetic, not setuid.
                (p/'status').write_bytes(b'Name:\tcaf\xc3\xa9\nUid:\t'+(' '.join([str(uid)]*4)).encode()+b'\n')
                with self.subTest(uid=uid), patch.object(self.m,'PROC_ROOT',proc), patch.object(self.m.os,'getuid',return_value=1000,create=True), patch.object(self.m,'process_identity',return_value=identity) as inspect:
                    result = self.m.observation_processes('raspi-link-display.service',None,set())
                    self.assertEqual(list(result),[])
                    self.assertTrue(result.coverage['complete'])
                    if uid == 1001: inspect.assert_not_called()

    def test_status_uid_row_strict_bounded_independent_bytes(self):
        valid = b'Name:\tcaf\xc3\xa9\nUid:\t1000\t1000\t1000\t1000\n'
        self.assertEqual(self.m.bounded_uids(valid),(1000,)*4)
        for raw in (b'Name: no uid\n', valid+b'Uid: 1000 1000 1000 1000\n',
                    valid.replace(b'1000',b'\xd9\xa1',1), valid.replace(b'1000',b'+1000',1),
                    valid.replace(b'1000',b'4294967296',1), valid.replace(b'1000',b'1001',1),
                    valid.replace(b'Uid:',b'Uid: 1'), valid+b'x'*65536):
            with self.subTest(raw=raw[:80]), self.assertRaises(ValueError): self.m.bounded_uids(raw)

    def test_utf8_status_relevant_foreign_and_changing_credentials_refuse(self):
        with tempfile.TemporaryDirectory() as d:
            proc=Path(d); p=proc/'123'; p.mkdir()
            raw=b'Name:\tcaf\xc3\xa9\nUid: 1001 1001 1001 1001\n'
            (p/'status').write_bytes(raw)
            with patch.object(self.m,'PROC_ROOT',proc), patch.object(self.m.os,'getuid',return_value=1000,create=True):
                with self.assertRaises(self.m.LaunchError): self.m.observation_processes(self.record()['unit'],self.record(),set())
                with self.assertRaises(self.m.LaunchError): self.m.observation_processes(self.record()['unit'],None,{123})
                original=Path.open; calls=[]
                def change(path,*a,**kw):
                    if path == p/'status' and a == ('rb',):
                        calls.append(1)
                        if len(calls)==2: path.write_bytes(raw.replace(b'1001',b'1002'))
                    return original(path,*a,**kw)
                with patch.object(Path,'open',change), self.assertRaises(self.m.LaunchError):
                    self.m.observation_processes(self.record()['unit'],None,set())
                self.assertEqual(len(calls),2)


@unittest.skipUnless(sys.platform.startswith('linux'), 'Real Linux syscall/filesystem primitive tests; not Windows or reviewed-app acceptance')
class NativeBoundedPrimitiveTests(unittest.TestCase):
    """Actual Linux FD/filesystem/process primitives, TEST ELF hash override.

    Manager/unit/cgroup provenance is MOCKED. This is never genuine reviewed-app
    live binding or stock Bullseye/X11 acceptance. Children self-expire in 2s.
    """
    @classmethod
    def setUpClass(cls):
        import subprocess, shutil, hashlib
        test_launcher.LauncherTests.setUpClass()
        cls.m = test_launcher.LauncherTests.m
        cls.fixture = test_launcher.LauncherTests(); cls.fixture.m = cls.m
        root = Path(os.environ['P5_NATIVE_ROOT'])
        cls.work = root/'native-cases'
        cls.work.mkdir(mode=0o700)  # exclusive, no predecessor reuse
        cls.binary = cls.work/'self-expiring-fixture'
        source = cls.work/'fixture.c'
        source.write_text('#include <unistd.h>\n#include <sys/prctl.h>\n#include <stdio.h>\nint main(void) { if(prctl(PR_SET_NAME,"caf\\303\\251",0,0,0)) return 1; puts("ready"); fflush(stdout); sleep(2); return 0; }\n')
        result = subprocess.run([shutil.which('cc') or 'cc','-O0',str(source),'-o',str(cls.binary)],capture_output=True,text=True,timeout=20)
        (cls.work/'compile.stdout').write_text(result.stdout); (cls.work/'compile.stderr').write_text(result.stderr)
        if result.returncode: raise RuntimeError('Private fixture compile failed: '+str(result.returncode))
        cls.sha = hashlib.sha256(cls.binary.read_bytes()).hexdigest()

    def setUp(self):
        self.r = self.fixture.record()
        self.actual_platform = self.m.bounded_platform
        self.actual_read_at = self.m.bounded_read_at
        profile = patch.object(self.m, 'bounded_platform')  # Alternate OS, not Bullseye acceptance.
        profile.start(); self.addCleanup(profile.stop)
        def primitive_view(fd, name, *args):
            raw = self.actual_read_at(fd, name, *args)
            if name.endswith('/mountinfo'):
                # WSL binfmt-status-lock is an unsupported overmount. For the
                # FD/filesystem seam ONLY, supply a labelled ordinary proc mount
                # table. Actual fstatfs/self/UID/ns/boot/process reads still run.
                rows = raw.splitlines()
                raw = b'\n'.join(row for row in rows if not row.split(b' - ',1)[0].split()[4].startswith(b'/proc/'))+b'\n'
            return raw
        view = patch.object(self.m, 'bounded_read_at', side_effect=primitive_view)
        view.start(); self.addCleanup(view.stop)
        self.path = self.work/self._testMethodName
        self.path.mkdir(mode=0o700)
        self.statepath = self.path/(self.r['unit']+'.json')

    @contextlib.contextmanager
    def child(self, extra=None):
        import subprocess
        r = self.r
        r['spec'].update(binary=str(self.binary),config=str(self.path/'config'))
        env = dict(r['desktop'],SDL_VIDEODRIVER='x11',**{self.m.MARKER_KEY:r['provenance']['marker']})
        env.update(extra or {})
        child = subprocess.Popen([str(self.binary),'--config',r['spec']['config']],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        named = False
        try:
            self.assertEqual(child.stdout.readline(),b'ready\n')
            self.assertIn(b'Name:\tcaf\xc3\xa9\n',(Path('/proc')/str(child.pid)/'status').read_bytes())
            named = True
            r['identity'] = self.m.process_identity(child.pid)
            if r['identity'] is None: raise RuntimeError('Native child identity missing')
            self.m.save_state(self.statepath,r)
            state = self.fixture.owned_state(r,MainPID=str(child.pid))
            with patch.object(self.m,'unit_state',return_value=state), patch.object(self.m,'cgroup_pids',return_value=[child.pid]), patch.object(self.m,'BOUNDED_ELF',self.sha):
                yield child
        finally:
            # No signal, broad kill or lifecycle: the private child expires itself.
            stdout,stderr = child.communicate(timeout=4)
            code = child.returncode
            with (self.work/'children.jsonl').open('a',encoding='utf-8') as receipt:
                receipt.write(json.dumps({'test':self._testMethodName,'pid':child.pid,'exit':code,'reaped':not (Path('/proc')/str(child.pid)).exists(),'stdout':os.fsdecode(stdout),'stderr':os.fsdecode(stderr),'utf8_name_verified_after_readiness':named,'signals_sent':False})+'\n')
            self.assertEqual(code,0)
            self.assertFalse((Path('/proc')/str(child.pid)).exists())

    def capture(self):
        return self.m.capture_bounded_proof(self.r['unit'],self.r,self.statepath)

    def test_native_context_and_actual_live_fd_capture_publication(self):
        with self.child():
            self.assertTrue(self.capture())
            raw,md = self.m.bounded_proof_read(self.statepath,self.r['unit'],self.m.time.monotonic()+1)
            proof = self.m.bounded_decode(raw)
            self.assertEqual(proof['record_binding'],self.m.bounded_binding(self.r))
            self.assertEqual(proof['elf_sha256'],self.sha)
            self.assertEqual(md[3],os.getuid())
        with patch.object(self.m,'BOUNDED_ELF',self.sha), patch.object(self.m,'state_path',return_value=self.statepath):
            self.assertIsNotNone(self.m.bounded_snapshot(self.r['unit'],self.r))

    def test_native_unadapted_alternate_os_or_proc_view_refused(self):
        if 'microsoft' not in os.uname().release.lower():
            self.skipTest('Alternate WSL context rejecting control only')
        with patch.object(self.m, 'bounded_platform', self.actual_platform), patch.object(self.m, 'bounded_read_at', self.actual_read_at):
            with self.assertRaises(ValueError):
                self.m.bounded_context(self.m.time.monotonic()+1)
        # Independently show the actual WSL overmount refuses even if OS is
        # assumed supported; native test adaptation does not change product.
        with patch.object(self.m, 'bounded_read_at', self.actual_read_at):
            with self.assertRaisesRegex(ValueError, 'Overmounted proc view'):
                self.m.bounded_context(self.m.time.monotonic()+1)

    def test_native_stock_pi_exact_auxiliary_mount_rows_and_forms(self):
        # Exact full rows from the fresh pi-final-context02 read-only receipt;
        # same decisive metadata as the preserved cycle1 failed Pi receipt.
        root = b'20 18 0:17 / /proc rw,relatime shared:5 - proc proc rw\n'
        auto = b'30 20 0:26 / /proc/sys/fs/binfmt_misc rw,relatime shared:13 - autofs systemd-1 rw,fd=29,pgrp=1,timeout=0,minproto=5,maxproto=5,direct\n'
        mounted = b'12 11 0:3 / /proc/sys/fs/binfmt_misc rw,relatime - binfmt_misc binfmt_misc rw\n'
        original = self.m.bounded_read_at
        for rows in (root+auto,root+mounted,root+auto+mounted):
            def read(fd,name,*a):
                return rows if name.endswith('/mountinfo') else original(fd,name,*a)
            with self.subTest(rows=rows), patch.object(self.m,'bounded_read_at',side_effect=read):
                self.assertEqual(self.m.bounded_context(self.m.time.monotonic()+1)[1],os.getuid())

    def test_native_auxiliary_mount_critical_paths_and_malformed_refuse(self):
        root = b'20 18 0:17 / /proc rw,relatime shared:5 - proc proc rw\n'
        auto = b'30 20 0:26 / /proc/sys/fs/binfmt_misc rw,relatime shared:13 - autofs systemd-1 rw,fd=29,pgrp=1,timeout=0,minproto=5,maxproto=5,direct\n'
        paths = ('/proc','/proc/123','/proc/123/status','/proc/self','/proc/self/mountinfo',
                 '/proc/self/ns','/proc/self/ns/mnt','/proc/123/ns/pid','/proc/sys','/proc/sys/fs',
                 '/proc/sys/kernel','/proc/sys/kernel/random','/proc/sys/kernel/random/boot_id',
                 '/proc/sys/fs/binfmt_misc/child','/proc/sys/fs/binfmt_misc-other')
        rows = [root+auto.replace(b'/proc/sys/fs/binfmt_misc',p.encode()) for p in paths]
        rows += [root+auto.replace(b'/ /proc',b'/subtree /proc'),
                 root+auto.replace(b' - autofs ',b' - tmpfs '),
                 root+auto.replace(b'/proc/sys/fs/binfmt_misc',b'/proc/sys/fs/binfmt\\137misc'),
                 root+auto.replace(b'/proc/sys/fs/binfmt_misc',b'/proc/sys/../fs/binfmt_misc'),
                 root+auto.replace(b' - autofs ',b' - proc '),root+auto+auto,
                 root+auto.replace(b'0:2',b'bad'),root+auto.replace(b'rw,relatime',b'rw,hidepid=2'),
                 root.replace(b'rw,relatime',b'rw,hidepid=2'),root.replace(b'proc rw',b'proc rw,subset=pid'),
                 root.replace(b'/ /proc',b'/subtree /proc'),root+root,b'',b'malformed\n',
                 root+auto.replace(b' - autofs systemd-1 rw',b' - autofs systemd-1')]
        original = self.m.bounded_read_at
        for table in rows:
            def read(fd,name,*a): return table if name.endswith('/mountinfo') else original(fd,name,*a)
            with self.subTest(table=table), patch.object(self.m,'bounded_read_at',side_effect=read), self.assertRaises(ValueError):
                self.m.bounded_context(self.m.time.monotonic()+1)

    def test_native_utf8_helper_name_context_and_strict_nspid_row(self):
        import ctypes
        libc=ctypes.CDLL(None,use_errno=True)
        old=Path('/proc/self/comm').read_bytes().rstrip(b'\n')
        if libc.prctl(15,ctypes.c_char_p(b'caf\xc3\xa9'),0,0,0): raise RuntimeError('Private name fixture failed')
        try:
            self.assertIn(b'Name:\tcaf\xc3\xa9\n',Path('/proc/self/status').read_bytes())
            self.assertEqual(self.m.bounded_context(self.m.time.monotonic()+1)[1],os.getuid())
            original=self.m.bounded_read_at
            pid=str(os.getpid()).encode()
            for field in (b'',b'NSpid: '+pid+b'\nNSpid: '+pid,b'NSpid: 1 2',b'NSpid: \xd9\xa1',b'NSpid: +'+pid):
                def read(fd,name,*a):
                    raw=original(fd,name,*a)
                    return re.sub(rb'NSpid:[^\n]*',lambda _:field,raw) if name.endswith('/status') else raw
                with self.subTest(field=field), patch.object(self.m,'bounded_read_at',side_effect=read), self.assertRaises(ValueError):
                    self.m.bounded_context(self.m.time.monotonic()+1)
        finally:
            if libc.prctl(15,ctypes.c_char_p(old),0,0,0): raise RuntimeError('Private name restoration failed')

    def test_native_wrong_hash_refuses_without_fd_leak(self):
        with self.child(), patch.object(self.m,'BOUNDED_ELF','0'*64):
            before = len(list(Path('/proc/self/fd').iterdir()))
            self.assertFalse(self.capture())
            self.assertEqual(before,len(list(Path('/proc/self/fd').iterdir())))
            self.assertFalse(self.statepath.with_name(self.r['unit']+'.bounded-proof.json').exists())

    def test_native_injected_environment_refuses(self):
        with self.child({'SDL_RENDER_DRIVER':'software'}): self.assertFalse(self.capture())

    def test_native_desktop_mismatch_refuses(self):
        with self.child():
            self.r['desktop']['DISPLAY'] = ':999'
            self.m.save_state(self.statepath,self.r)
            self.assertFalse(self.capture())

    def test_native_capture_acquisition_failures_close_descriptors(self):
        with self.child():
            original = self.m.bounded_read_at
            for name in ('stat','status','cmdline','environ'):
                def fail(directory,selected,*args):
                    if selected == name: raise PermissionError('injected native read boundary')
                    return original(directory,selected,*args)
                count = len(list(Path('/proc/self/fd').iterdir()))
                with self.subTest(name=name), patch.object(self.m,'bounded_read_at',side_effect=fail):
                    self.assertFalse(self.capture())
                self.assertEqual(count,len(list(Path('/proc/self/fd').iterdir())))

    def test_native_capture_reopen_failure_closes_descriptors(self):
        with self.child():
            original = self.m.os.open; calls = []
            def fail(path,*a,**kw):
                if path == 'exe':
                    calls.append(path)
                    if len(calls) == 2: raise PermissionError('injected reopen failure')
                return original(path,*a,**kw)
            count = len(list(Path('/proc/self/fd').iterdir()))
            with patch.object(self.m.os,'open',side_effect=fail): self.assertFalse(self.capture())
            self.assertEqual(len(calls),2)
            self.assertEqual(count,len(list(Path('/proc/self/fd').iterdir())))

    def test_native_capture_late_manager_record_context_and_deadline_refuse(self):
        with self.child():
            original = self.m.bounded_process
            for boundary in ('manager','record','context','deadline'):
                with contextlib.ExitStack() as stack:
                    if boundary == 'manager': stack.enter_context(patch.object(self.m,'unit_state',side_effect=[self.fixture.owned_state(self.r,MainPID=str(self.r['identity']['pid'])),self.fixture.owned_state(self.r,MainPID='0')]))
                    if boundary == 'record': stack.enter_context(patch.object(self.m,'read_state',side_effect=[self.r,dict(self.r,phase='stopped')]))
                    if boundary == 'context': stack.enter_context(patch.object(self.m,'bounded_context',side_effect=[self.m.bounded_context(self.m.time.monotonic()+1),None]))
                    if boundary == 'deadline': stack.enter_context(patch.object(self.m,'bounded_remaining',side_effect=self.m.LaunchError('deadline')))
                    with self.subTest(boundary=boundary): self.assertFalse(self.capture())

    def test_native_absence_directory_enoent_and_inner_missing_distinct(self):
        with self.child() as child:
            identity = dict(self.r['identity'])
            with self.assertRaises(ValueError): self.m.bounded_absence(identity,self.m.time.monotonic()+1)
            # Real directory exists; only inner stat acquisition returns ENOENT.
            with patch.object(self.m,'bounded_read_at',side_effect=FileNotFoundError('inner stat')):
                with self.assertRaises(FileNotFoundError): self.m.bounded_absence(identity,self.m.time.monotonic()+1)
        self.assertEqual(self.m.bounded_absence(identity,self.m.time.monotonic()+1),(None,None))

    def test_native_absence_stable_reuse_zombie_denial_and_races(self):
        with self.child():
            identity = dict(self.r['identity'],start_ticks='1')
            ticks = self.r['identity']['start_ticks']
            self.assertEqual(self.m.bounded_absence(identity,self.m.time.monotonic()+1),(ticks,ticks))
            original = self.m.bounded_read_at
            def stat(t,state='S'): return (str(identity['pid'])+' (fixture) '+state+' '+' '.join(['0']*18+[t])).encode()
            for values in ([stat(ticks,'Z')]*4,[stat(ticks),stat('999')],None):
                with self.subTest(values='denial' if values is None else 'race/zombie'), patch.object(self.m,'bounded_read_at',side_effect=PermissionError('denied') if values is None else values):
                    with self.assertRaises((ValueError,PermissionError)): self.m.bounded_absence(identity,self.m.time.monotonic()+1)

    def test_native_safe_file_collisions_no_repair(self):
        raw = BoundedObservationTests.proof(self,self.r)
        proof = self.path/(self.r['unit']+'.bounded-proof.json')
        for kind in ('symlink','hardlink','unsafe-mode','fifo'):
            target = self.path/'target'
            target.write_bytes(b'protected'); target.chmod(0o600)
            if kind == 'symlink': proof.symlink_to(target)
            elif kind == 'hardlink': os.link(target,proof)
            elif kind == 'fifo': os.mkfifo(proof,0o600)
            else: proof.write_bytes(b'protected'); proof.chmod(0o644)
            with self.subTest(kind=kind), self.assertRaises((ValueError,OSError)):
                self.m.bounded_publish(self.statepath,self.r['unit'],raw,self.m.time.monotonic()+1)
            self.assertEqual(target.read_bytes(),b'protected')
            proof.unlink(); target.unlink()
        self.assertEqual(list(self.path.glob('.bounded-*')),[])

    def test_native_atomic_replacement_and_private_directory(self):
        raw = BoundedObservationTests.proof(self,self.r)
        self.m.bounded_publish(self.statepath,self.r['unit'],raw,self.m.time.monotonic()+1)
        proof = self.path/(self.r['unit']+'.bounded-proof.json')
        inode = proof.stat().st_ino
        self.m.bounded_publish(self.statepath,self.r['unit'],raw,self.m.time.monotonic()+1)
        self.assertNotEqual(inode,proof.stat().st_ino)
        self.assertEqual(proof.stat().st_mode & 0o777,0o600)
        self.path.chmod(0o750)
        with self.assertRaises(ValueError): self.m.bounded_proof_read(self.statepath,self.r['unit'],self.m.time.monotonic()+1)
        self.path.chmod(0o700)

    def test_native_safe_read_strict_wire_and_size(self):
        proof = self.path/(self.r['unit']+'.bounded-proof.json')
        for raw in (b'{}',b'[]',b'x'*16385,b'\xff'):
            proof.write_bytes(raw); proof.chmod(0o600)
            with self.assertRaises(ValueError): self.m.bounded_proof_read(self.statepath,self.r['unit'],self.m.time.monotonic()+1)

    def test_native_unsupported_proc_contexts_refuse_actual_checks(self):
        original = self.m.bounded_read_at
        for kind in ('uidmap','mount','nspid','boot'):
            def mutate(fd,name,*a):
                raw = original(fd,name,*a)
                if kind == 'uidmap' and name.endswith('/uid_map'): return b'0 1000 1\n'
                if kind == 'mount' and name.endswith('/mountinfo'): return raw.replace(b' - proc proc rw',b' - proc proc rw,hidepid=2')
                if kind == 'nspid' and name.endswith('/status'): return re.sub(rb'NSpid:[^\n]*',b'NSpid: 1 2',raw)
                if kind == 'boot' and name.endswith('/boot_id'): return b'invalid\n'
                return raw
            with self.subTest(kind=kind), patch.object(self.m,'bounded_read_at',side_effect=mutate), self.assertRaises(ValueError):
                self.m.bounded_context(self.m.time.monotonic()+1)

    def test_native_symlinked_ancestor_refuses_without_repair(self):
        raw = BoundedObservationTests.proof(self,self.r)
        link = self.path/'alias'
        target = self.path/'private'
        target.mkdir(mode=0o700); link.symlink_to(target, target_is_directory=True)
        with self.assertRaises(OSError):
            self.m.bounded_publish(link/'state',self.r['unit'],raw,self.m.time.monotonic()+1)
        self.assertEqual(list(target.iterdir()),[])

    def test_native_failed_postrename_readback_omits_new_stamp(self):
        raw = BoundedObservationTests.proof(self,self.r)
        before = len(list(Path('/proc/self/fd').iterdir()))
        with patch.object(self.m,'bounded_file_read',side_effect=ValueError('injected readback')), self.assertRaises(ValueError):
            self.m.bounded_publish(self.statepath,self.r['unit'],raw,self.m.time.monotonic()+1)
        self.assertFalse((self.path/(self.r['unit']+'.bounded-proof.json')).exists())
        self.assertEqual(list(self.path.glob('.bounded-*')),[])
        self.assertEqual(before,len(list(Path('/proc/self/fd').iterdir())))

    def test_native_capture_closing_argv_uid_environment_and_ticks_refuse(self):
        with self.child():
            original = self.m.bounded_read_at
            for boundary in ('stat','status','cmdline','environ'):
                calls = []
                def mutate(fd,name,*a):
                    raw = original(fd,name,*a)
                    if name == boundary:
                        calls.append(name)
                        if len(calls) == 2:
                            if name == 'status': return re.sub(rb'Uid:[^\n]*',b'Uid: 0 0 0 0',raw)
                            if name == 'stat':
                                pid = self.r['identity']['pid']
                                return (str(pid)+' (fixture) S '+' '.join(['0']*18+['1'])).encode()
                            if name == 'environ': return raw.replace(b'SDL_VIDEODRIVER=x11',b'SDL_VIDEODRIVER=bad')
                            return raw+b'changed\0'
                    return raw
                before = len(list(Path('/proc/self/fd').iterdir()))
                with self.subTest(boundary=boundary), patch.object(self.m,'bounded_read_at',side_effect=mutate):
                    self.assertFalse(self.capture())
                self.assertEqual(len(calls),2)  # Reached actual closing acquisition.
                self.assertEqual(before,len(list(Path('/proc/self/fd').iterdir())))
                self.assertFalse((self.path/(self.r['unit']+'.bounded-proof.json')).exists())

    def test_native_environment_overflow_refuses_without_dump(self):
        with self.child({'P5_SYNTHETIC_PADDING':'x'*65536}):
            self.assertFalse(self.capture())
            self.assertFalse((self.path/(self.r['unit']+'.bounded-proof.json')).exists())

    def test_native_publish_failure_closes_and_removes_exclusive_temp(self):
        raw = BoundedObservationTests.proof(self,self.r)
        before = len(list(Path('/proc/self/fd').iterdir()))
        with patch.object(self.m.os,'fsync',side_effect=OSError('injected fsync failure')), self.assertRaises(OSError):
            self.m.bounded_publish(self.statepath,self.r['unit'],raw,self.m.time.monotonic()+1)
        self.assertEqual(before,len(list(Path('/proc/self/fd').iterdir())))
        self.assertEqual(list(self.path.glob('.bounded-*')),[])

if __name__ == '__main__':
    unittest.main(verbosity=2)
