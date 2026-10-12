"""Dependency-free launcher tests. Run with python3 tests/launcher/test_launcher.py."""
import contextlib
import copy
import importlib.machinery
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

SOURCE = Path(__file__).resolve().parents[2] / "tools" / "link-displayctl"


class LauncherTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        loader = importlib.machinery.SourceFileLoader("link_displayctl", str(SOURCE))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        cls.m = importlib.util.module_from_spec(spec)
        loader.exec_module(cls.m)

    def setUp(self):
        if not hasattr(os, "O_NOFOLLOW"):
            # Linux-only flag: state I/O tests on Windows do not claim OS-level protection.
            flag = patch.object(os, "O_NOFOLLOW", 0, create=True)
            flag.start()
            self.addCleanup(flag.stop)

    def record(self, phase="running"):
        root = SOURCE.parent.parent
        identity = {"pid": 123, "start_ticks": "456", "executable": str(root / "build/link-pi-display"),
                    "exe_device": 2049, "exe_inode": 100, "exe_unlinked": False}
        return {"version": 3, "unit": "raspi-link-display.service", "phase": phase,
                "spec": {"project": str(root), "binary": identity["executable"], "config": str(root / "config/a.conf"), "windowed": False},
                "desktop": {"DISPLAY": ":0", "XAUTHORITY": "/home/pi/.Xauthority", "XDG_RUNTIME_DIR": "/run/user/1000", "XDG_SESSION_TYPE": "x11"},
                "session": "A", "provenance": {"kind": "launch", "marker": "a" * 32},
                "invocation_id": None if phase == "pending" else "b" * 32,
                "identity": None if phase == "pending" else identity}

    def owned_state(self, record=None, **changes):
        record = record or self.record()
        value = {"Id": record["unit"], "LoadState": "loaded", "Description": self.m.DESCRIPTION, "Transient": "yes",
                 "ActiveState": "active", "MainPID": "123", "InvocationID": "b" * 32,
                 "ControlGroup": "/user.slice/test.service", "Environment": self.m.MARKER_KEY + "=" + "a" * 32,
                 "KillMode": "control-group", "KillSignal": "15", "SendSIGKILL": "no", "SendSIGHUP": "no",
                 "Restart": "no", "ExecStop": "", "ExecStopPost": "", "TimeoutStopFailureMode": "terminate",
                 "FailureAction": "none", "SuccessAction": "none", "OnFailure": ""}
        value.update(changes)
        return value

    @contextlib.contextmanager
    def owned_context(self, record=None, state=None, pids=None):
        record = record or self.record()
        with contextlib.ExitStack() as stack:
            values = {"unit_state": state or self.owned_state(record), "state_path": Path("unused"),
                      "read_state": record, "process_identity": self.record()["identity"],
                      "cgroup_pids": [123] if pids is None else pids}
            mocks = {k: stack.enter_context(patch.object(self.m, k, return_value=v)) for k, v in values.items()}
            mocks["save_state"] = stack.enter_context(patch.object(self.m, "save_state"))
            yield mocks

    @contextlib.contextmanager
    def linux_main_context(self):
        with contextlib.ExitStack() as stack:
            stack.enter_context(patch.object(self.m.sys, "platform", "linux"))
            stack.enter_context(patch.object(self.m.os, "getuid", return_value=1000, create=True))
            stack.enter_context(patch.object(self.m, "lifecycle_lock", side_effect=lambda unit: contextlib.nullcontext()))
            stack.enter_context(patch.object(self.m, "load_settings", return_value={}))
            stack.enter_context(patch.object(self.m, "validate_spec"))
            stack.enter_context(patch.object(self.m, "desktop_environment", return_value=("A", self.record()["desktop"])))
            stack.enter_context(patch.object(self.m, "unified_cgroup_mount", return_value=SOURCE.parent))
            yield

    def mount_evidence(self, directory, membership="0::/user.slice/test\n", extra="", root="/"):
        proc = Path(directory) / "proc"
        mount = Path(directory) / "unified mount"
        (proc / "self").mkdir(parents=True)
        mount.mkdir()
        path = str(mount).replace("\\", "/").replace(" ", "\\040")
        (proc / "self/cgroup").write_text(membership)
        (proc / "self/mountinfo").write_text("29 23 0:26 {} {} rw - cgroup2 cgroup rw\n".format(root, path) + extra)
        return proc, mount

    def test_unit_validation(self):
        for unit in ("raspi-link-display.service", "raspi-link-display-test_1.service"):
            self.assertEqual(self.m.validate_unit(unit), unit)
        for unit in ("sshd.service", "../../x", "raspi-link-display;id.service", "raspi-link-display.socket"):
            with self.subTest(unit=unit), self.assertRaises(self.m.LaunchError):
                self.m.validate_unit(unit)

    def test_property_parser_preserves_equals(self):
        self.assertEqual(self.m.properties("A=x=y\nB=\n"), {"A": "x=y", "B": ""})

    def test_missing_unit_is_stopped(self):
        self.assertFalse(self.m.require_owned({"LoadState": "not-found"}))

    def test_foreign_unit_rejected(self):
        for props in ({"LoadState": "loaded", "Description": "other", "Transient": "yes"},
                      {"LoadState": "loaded", "Description": self.m.DESCRIPTION, "Transient": "no"}):
            with self.assertRaises(self.m.LaunchError):
                self.m.require_owned(props)

    def test_same_description_without_provenance_is_rejected(self):
        with self.assertRaises(self.m.LaunchError):
            self.m.require_owned({"LoadState": "loaded", "Description": self.m.DESCRIPTION, "Transient": "yes"})

    def test_explicit_settings_override_prior_selection_then_cli(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d).resolve()
            prior = {"project": str(root), "binary": str(root / "old"), "config": str(root / "a"), "windowed": True}
            settings = {"project": str(root), "binary": str(root / "new"), "config": str(root / "b")}
            args = self.m.parser().parse_args(["restart", "--settings", "B.json"])
            spec = self.m.make_spec(args, settings, prior)
            self.assertEqual(spec["config"], settings["config"])
            self.assertEqual(spec["binary"], settings["binary"])
            args.config = str(root / "c")
            self.assertEqual(self.m.make_spec(args, settings, prior)["config"], args.config)

    def test_relative_settings_binding_is_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "settings.json"
            p.write_text('{"config": "relative.conf"}')
            with self.assertRaises(self.m.LaunchError):
                self.m.load_settings(p)

    def test_systemd_argv_escapes_literal_dollars(self):
        spec = {"project": "/project", "binary": "/project/app", "config": "/project/${PROFILE}/$HOME.conf", "windowed": False}
        argv = self.m.service_arguments("raspi-link-display.service", spec, {})
        self.assertEqual(argv[-1], "/project/$${PROFILE}/$$HOME.conf")

    def test_no_shell_and_useful_error(self):
        result = subprocess.CompletedProcess(["tool"], 3, "", "denied")
        with patch.object(self.m.subprocess, "run", return_value=result) as run:
            with self.assertRaisesRegex(self.m.LaunchError, "denied"):
                self.m.command(["tool", "a; touch forbidden"])
            self.assertNotIn("shell", run.call_args.kwargs)
            self.assertEqual(run.call_args.args[0][1], "a; touch forbidden")

    def test_timeout_becomes_launch_error(self):
        with patch.object(self.m.subprocess, "run", side_effect=subprocess.TimeoutExpired("tool", 2)):
            with self.assertRaisesRegex(self.m.LaunchError, "timed out"):
                self.m.command(["tool"])

    def test_environment_allowlist(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "env"
            p.write_bytes(b"DISPLAY=:0\0SECRET=do-not-return\0XAUTHORITY=/x\0")
            env = self.m.read_environment(p)
            self.assertEqual(env, {"DISPLAY": ":0", "XAUTHORITY": "/x"})

    def test_settings_strict_keys(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "settings.json"
            p.write_text('{"unexpected": "value"}')
            with self.assertRaises(self.m.LaunchError):
                self.m.load_settings(p)

    def test_bad_json_settings(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "settings.json"
            p.write_text('{')
            with self.assertRaises(self.m.LaunchError):
                self.m.load_settings(p)

    def test_settings_non_string_paths_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "settings.json"
            p.write_text('{"project": 42}')
            with self.assertRaises(self.m.LaunchError):
                self.m.load_settings(p)

    def test_spec_absolute_paths_and_spaces(self):
        with tempfile.TemporaryDirectory(prefix="link space ") as d:
            root = Path(d)
            (root / "build").mkdir()
            (root / "config" / "presets").mkdir(parents=True)
            args = self.m.parser().parse_args(["start", "--project", str(root)])
            spec = self.m.make_spec(args, {})
            self.assertEqual(spec["project"], str(root.resolve()))
            self.assertEqual(spec["binary"], str(root / "build" / "link-pi-display"))
            self.assertTrue(Path(spec["config"]).is_absolute())

    def test_service_arguments_have_safe_policy_and_quoting(self):
        spec = {"project": "/a space", "binary": "/a space/build/link-pi-display", "config": "/a space/config/a.conf", "windowed": True}
        env = {"DISPLAY": ":0", "XAUTHORITY": "/home/pi/.Xauthority", "XDG_RUNTIME_DIR": "/run/user/1000"}
        argv = self.m.service_arguments("raspi-link-display.service", spec, env)
        for item in ("--service-type=exec", "--property=Restart=no", "--property=SendSIGKILL=no", "--property=StandardInput=null", "--property=StandardOutput=journal", "--property=KillMode=control-group", "--setenv=SDL_VIDEODRIVER=x11"):
            self.assertIn(item, argv)
        self.assertIn("--working-directory=/a space", argv)
        self.assertEqual(argv[-4:], ["/a space/build/link-pi-display", "--config", "/a space/config/a.conf", "--windowed"])
        for prohibited in ("--scope", "--pty", "--pipe", "--collect", "sudo"):
            self.assertNotIn(prohibited, argv)

    def test_unsupported_backend_is_clear(self):
        with self.assertRaisesRegex(self.m.LaunchError, "X11"):
            self.m.validate_desktop_values({"XDG_SESSION_TYPE": "wayland"})

    def test_missing_authority_rejected(self):
        with self.assertRaises(self.m.LaunchError):
            self.m.validate_desktop_values({"DISPLAY": ":0", "XDG_SESSION_TYPE": "x11", "XAUTHORITY": "/path/does/not/exist"})

    def test_duplicate_same_spec_is_idempotent(self):
        current = {"config": "a", "windowed": False}
        self.assertFalse(self.m.require_matching_spec(current, dict(current)))

    def test_duplicate_changed_spec_rejected(self):
        with self.assertRaisesRegex(self.m.LaunchError, "restart"):
            self.m.require_matching_spec({"config": "a"}, {"config": "b"})

    def test_process_start_field_handles_spaces(self):
        text = "42 (some (name) here) S " + " ".join(str(i) for i in range(4, 23))
        self.assertEqual(self.m.start_ticks(text), "22")

    def test_export_does_not_overwrite(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "log.txt"
            self.m.export_log(p, "first")
            with self.assertRaises(self.m.LaunchError):
                self.m.export_log(p, "second")
            self.assertEqual(p.read_text(), "first")

    def test_empty_cgroup_is_empty(self):
        with tempfile.TemporaryDirectory() as d:
            proc, _ = self.mount_evidence(d)
            with patch.object(self.m, "PROC_ROOT", proc):
                self.assertEqual(self.m.cgroup_pids({"ControlGroup": ""}), [])

    def test_unsafe_cgroup_path_rejected(self):
        with patch.object(self.m, "unified_cgroup_mount", return_value=SOURCE.parent), self.assertRaises(self.m.LaunchError):
            self.m.cgroup_pids({"ControlGroup": "/../../etc"})

    def test_read_state_rejects_symlink(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "source"
            p.write_text('{}')
            q = Path(d) / "state"
            try:
                q.symlink_to(p)
            except OSError:
                self.skipTest("Symlink creation unavailable on this test host")
            with self.assertRaises(self.m.LaunchError):
                self.m.read_state(q)

    def test_malformed_state_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "state.json"
            p.write_text('{"spec": 42}')
            with self.assertRaisesRegex(self.m.LaunchError, "schema"):
                self.m.read_state(p)

    def test_stop_missing_unit_is_idempotent(self):
        with patch.object(self.m, "unit_state", return_value={"LoadState": "not-found"}), patch.object(self.m, "command") as run:
            self.m.stop("raspi-link-display.service")
            run.assert_not_called()

    def test_stop_timeout_never_silently_force_kills(self):
        failure = subprocess.CompletedProcess([], 1, "", "timed out")
        with self.owned_context(), patch.object(self.m, "command", return_value=failure) as run:
            with self.assertRaisesRegex(self.m.LaunchError, "no automatic force kill"):
                self.m.stop("raspi-link-display.service")
            self.assertNotIn("kill", run.call_args.args[0])

    def test_stop_foreign_unit_never_signals(self):
        with self.owned_context(state=self.owned_state(Description="other")), patch.object(self.m, "command") as run:
            with self.assertRaises(self.m.LaunchError):
                self.m.stop("raspi-link-display.service", True)
            run.assert_not_called()

    def test_missing_desktop_session_fails(self):
        result = subprocess.CompletedProcess([], 0, "", "")
        with patch.object(self.m, "command", return_value=result):
            with self.assertRaisesRegex(self.m.LaunchError, "exactly one"):
                self.m.desktop_environment()

    def test_restart_invalid_inputs_do_not_stop_live_app(self):
        import contextlib
        with patch.object(self.m.sys, "platform", "linux"), patch.object(self.m.os, "getuid", return_value=1000, create=True), patch.object(self.m, "load_settings", return_value={}), patch.object(self.m, "lifecycle_lock", return_value=contextlib.nullcontext()), patch.object(self.m, "state_path", return_value=Path("unused")), patch.object(self.m, "read_state", return_value=None), patch.object(self.m, "validate_spec", side_effect=self.m.LaunchError("bad replacement")), patch.object(self.m, "stop") as stop:
            self.assertEqual(self.m.main(["restart", "--config", "bad"]), 1)
            stop.assert_not_called()

    def test_owned_service_requires_marker_invocation_and_process_identity(self):
        record = self.record()
        with patch.object(self.m, "process_identity", return_value=record["identity"]):
            self.assertTrue(self.m.require_owned(self.owned_state(), record))
            for changes in ({"Environment": ""}, {"Environment": self.m.MARKER_KEY + "=" + "c" * 32},
                            {"InvocationID": "c" * 32}, {"Id": "raspi-link-display-other.service"}):
                with self.subTest(changes=changes), self.assertRaises(self.m.LaunchError):
                    self.m.require_owned(self.owned_state(**changes), record)
        changed = dict(record["identity"], start_ticks="457")
        with patch.object(self.m, "process_identity", return_value=changed), self.assertRaisesRegex(self.m.LaunchError, "process identity"):
            self.m.require_owned(self.owned_state(), record)

    def test_stop_and_force_refuse_recreated_same_description_unit(self):
        for force in (False, True):
            for changes in ({"InvocationID": "c" * 32}, {"Environment": self.m.MARKER_KEY + "=" + "c" * 32}):
                with self.subTest(force=force, changes=changes), self.owned_context(state=self.owned_state(**changes)), patch.object(self.m, "command") as run:
                    with self.assertRaises(self.m.LaunchError):
                        self.m.stop(self.record()["unit"], force)
                    run.assert_not_called()

    def test_stop_rechecks_invocation_immediately_before_signalling(self):
        for force in (False, True):
            with self.subTest(force=force), self.owned_context() as mocks, patch.object(self.m, "command") as run:
                mocks["unit_state"].side_effect = [self.owned_state(), self.owned_state(InvocationID="c" * 32)]
                with self.assertRaisesRegex(self.m.LaunchError, "InvocationID"):
                    self.m.stop(self.record()["unit"], force)
                run.assert_not_called()

    def test_stop_refuses_unsafe_shutdown_policy(self):
        for changes in ({"SendSIGKILL": "yes"}, {"KillMode": "process"}, {"KillSignal": "9"},
                        {"Restart": "always"}, {"SendSIGHUP": "yes"}, {"ExecStop": "foreign command"}, {"ExecStopPost": "foreign command"},
                        {"TimeoutStopFailureMode": "kill"}, {"FailureAction": "exit"}, {"SuccessAction": "exit"}, {"OnFailure": "foreign.service"}):
            with self.subTest(changes=changes), self.owned_context(state=self.owned_state(**changes)), patch.object(self.m, "command") as run:
                with self.assertRaisesRegex(self.m.LaunchError, "shutdown"):
                    self.m.stop(self.record()["unit"], True)
                run.assert_not_called()

    def test_explicit_verified_legacy_migration_is_pinned(self):
        record = self.record()
        record["provenance"] = {"kind": "verified-legacy", "invocation_id": record["invocation_id"],
                                "identity": copy.deepcopy(record["identity"]), "control_group": "/user.slice/test.service"}
        self.m.validate_record(record)
        with patch.object(self.m, "process_identity", return_value=record["identity"]):
            self.assertTrue(self.m.require_owned(self.owned_state(Environment=""), record))
            for changes in ({"InvocationID": "c" * 32}, {"ControlGroup": "/other/service"}):
                with self.subTest(changes=changes), self.assertRaises(self.m.LaunchError):
                    self.m.require_owned(self.owned_state(Environment="", **changes), record)
        record["provenance"]["invocation_id"] = "c" * 32
        with self.assertRaises(ValueError):
            self.m.validate_record(record)

    def test_state_schema_and_identity_validation(self):
        mutations = [lambda r: r.update(version=1), lambda r: r.update(unit="foreign.service"),
                     lambda r: r.update(phase="unknown"), lambda r: r.update(invocation_id="bad"),
                     lambda r: r.update(identity=None), lambda r: r["identity"].update(pid=True),
                     lambda r: r["identity"].update(start_ticks="not-ticks"), lambda r: r["identity"].update(executable="relative"),
                     lambda r: r["identity"].update(unexpected="x"), lambda r: r.update(session=42),
                     lambda r: r["provenance"].update(marker="bad"), lambda r: r.update(unexpected="x")]
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / (self.record()["unit"] + ".json")
            for mutate in mutations:
                record = self.record()
                mutate(record)
                path.write_text(json.dumps(record))
                with self.subTest(record=record), self.assertRaises((self.m.LaunchError, ValueError)):
                    self.m.read_state(path)
            record = self.record("pending")
            self.m.save_state(path, record)
            self.assertEqual(self.m.read_state(path), record)
            record["phase"] = "failed"
            self.m.save_state(path, record)
            self.assertEqual(self.m.read_state(path), record)

    def test_legacy_schema_is_not_automatically_adopted(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / (self.record()["unit"] + ".json")
            record = self.record()
            path.write_text(json.dumps({k: record[k] for k in ("spec", "desktop", "session", "invocation_id", "identity")}))
            with self.assertRaisesRegex(self.m.LaunchError, "migration"):
                self.m.read_state(path)

    def test_pending_state_reconciles_only_matching_launch_marker(self):
        record = self.record("pending")
        with tempfile.TemporaryDirectory() as d, patch.object(self.m, "process_identity", return_value=self.record()["identity"]):
            path = Path(d) / (record["unit"] + ".json")
            self.m.save_state(path, record)
            with self.assertRaisesRegex(self.m.LaunchError, "marker"):
                self.m.reconcile_record(self.owned_state(Environment=""), record, path)
            self.assertEqual(self.m.read_state(path), record)
            reconciled = self.m.reconcile_record(self.owned_state(), record, path)
            self.assertEqual(reconciled["phase"], "running")
            self.assertEqual(reconciled["invocation_id"], "b" * 32)
            self.assertEqual(reconciled["identity"], self.record()["identity"])
            self.assertEqual(self.m.read_state(path), reconciled)

    def test_attempt_is_saved_before_creation_and_identity_before_window_error(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / (self.record()["unit"] + ".json")
            self.m.save_state(path, self.record())
            selected = str(Path(d) / "selected-B.conf")
            args = self.m.parser().parse_args(["start", "--config", selected])
            created = []
            def state(unit):
                if not created:
                    return {"LoadState": "not-found", "ActiveState": "inactive"}
                record = self.m.read_state(path)
                return self.owned_state(Environment=self.m.MARKER_KEY + "=" + record["provenance"]["marker"])
            def create(argv, **kwargs):
                record = self.m.read_state(path)
                self.assertEqual(record["phase"], "pending")
                self.assertIsNone(record["invocation_id"])
                self.assertEqual(record["spec"]["config"], selected)
                self.assertIn("--setenv=" + self.m.MARKER_KEY + "=" + record["provenance"]["marker"], argv)
                created.append(True)
                return subprocess.CompletedProcess(argv, 0, "", "")
            def observe(pid, env):
                record = self.m.read_state(path)
                self.assertEqual(record["phase"], "running")
                self.assertEqual(record["identity"], self.record()["identity"])
                raise self.m.LaunchError("window inspection failed")
            with patch.object(self.m, "state_path", return_value=path), patch.object(self.m, "unit_state", side_effect=state), patch.object(self.m, "validate_spec"), patch.object(self.m, "desktop_environment", return_value=("A", self.record()["desktop"])), patch.object(self.m, "unified_cgroup_mount", return_value=SOURCE.parent), patch.object(self.m, "unmanaged_instances", return_value=[]), patch.object(self.m, "cgroup_pids", return_value=[]), patch.object(self.m, "process_identity", return_value=self.record()["identity"]), patch.object(self.m, "command", side_effect=create), patch.object(self.m, "mapped_windows", side_effect=observe):
                with self.assertRaisesRegex(self.m.LaunchError, "window inspection"):
                    self.m.start(args, {})
            record = self.m.read_state(path)
            self.assertEqual(record["phase"], "failed")
            self.assertEqual(record["spec"]["config"], selected)
            self.assertEqual(record["invocation_id"], "b" * 32)

    def test_interruption_after_service_creation_leaves_reconcilable_intent(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / (self.record()["unit"] + ".json")
            args = self.m.parser().parse_args(["start"])
            created = []
            def state(unit):
                if not created:
                    return {"LoadState": "not-found", "ActiveState": "inactive"}
                record = self.m.read_state(path)
                return self.owned_state(Environment=self.m.MARKER_KEY + "=" + record["provenance"]["marker"])
            def create(argv, **kwargs):
                self.assertEqual(self.m.read_state(path)["phase"], "pending")
                created.append(True)
                raise KeyboardInterrupt()
            with patch.object(self.m, "state_path", return_value=path), patch.object(self.m, "unit_state", side_effect=state), patch.object(self.m, "validate_spec"), patch.object(self.m, "desktop_environment", return_value=("A", self.record()["desktop"])), patch.object(self.m, "unified_cgroup_mount", return_value=SOURCE.parent), patch.object(self.m, "unmanaged_instances", return_value=[]), patch.object(self.m, "cgroup_pids", side_effect=lambda state: [123] if created else []), patch.object(self.m, "process_identity", return_value=self.record()["identity"]), patch.object(self.m, "command", side_effect=create), patch.object(self.m, "mapped_windows", return_value=[]):
                with self.assertRaises(KeyboardInterrupt):
                    self.m.start(args, {})
                self.assertEqual(self.m.read_state(path)["phase"], "pending")
                result = self.m.status(args.unit)
                self.assertEqual(result["phase"], "running")
                self.assertEqual(self.m.read_state(path)["invocation_id"], "b" * 32)

    def test_immediate_failed_launch_retains_selection_and_nullable_identity(self):
        with self.owned_context(record=self.record("pending")) as mocks, patch.object(self.m, "validate_spec"), patch.object(self.m, "desktop_environment", return_value=("A", self.record()["desktop"])), patch.object(self.m, "unified_cgroup_mount"), patch.object(self.m, "unmanaged_instances", return_value=[]), patch.object(self.m, "command"):
            mocks["unit_state"].side_effect = [{"LoadState": "not-found", "ActiveState": "inactive"}, self.owned_state(ActiveState="failed", MainPID="0"), self.owned_state(ActiveState="failed", MainPID="0")]
            mocks["cgroup_pids"].return_value = []
            with patch.object(self.m.secrets, "token_hex", return_value="a" * 32):
                with self.assertRaisesRegex(self.m.LaunchError, "failed after launch"):
                    self.m.start(self.m.parser().parse_args(["start"]), {})
            saved = mocks["save_state"].call_args.args[1]
            self.assertEqual(saved["phase"], "failed")
            self.assertEqual(saved["invocation_id"], "b" * 32)
            self.assertIsNone(saved["identity"])
            self.m.validate_record(saved)

    def test_failed_state_with_residual_processes_blocks_new_start(self):
        record = self.record("failed")
        with self.owned_context(record=record, state=self.owned_state(ActiveState="failed", MainPID="0")), patch.object(self.m, "command") as run:
            with self.assertRaisesRegex(self.m.LaunchError, "residual"):
                self.m.start(self.m.parser().parse_args(["start"]), {})
            run.assert_not_called()

    def test_failed_state_residual_processes_can_be_explicitly_force_stopped(self):
        record = self.record("failed")
        record["identity"] = None
        failed = self.owned_state(ActiveState="failed", MainPID="0")
        with self.owned_context(record=record, state=failed) as mocks, patch.object(self.m, "command", return_value=subprocess.CompletedProcess([], 0, "", "")) as run:
            mocks["unit_state"].side_effect = [failed, failed, failed, {"LoadState": "not-found"}]
            mocks["cgroup_pids"].side_effect = [[456], []]
            self.m.stop(record["unit"], True)
            self.assertEqual(run.call_args_list[0].args[0], ["systemctl", "--user", "kill", "--kill-who=all", "--signal=SIGKILL", record["unit"]])
            self.assertIn("stop", run.call_args_list[1].args[0])
            self.assertEqual(mocks["save_state"].call_args.args[1]["phase"], "stopped")

    def test_command_timeout_stop_message_is_actionable_without_kill(self):
        with self.owned_context(), patch.object(self.m, "command", side_effect=self.m.LaunchError("Command timed out")) as run:
            with self.assertRaisesRegex(self.m.LaunchError, "stop --unit raspi-link-display.service --force"):
                self.m.stop(self.record()["unit"])
            self.assertEqual(run.call_count, 1)
            self.assertNotIn("kill", run.call_args.args[0])

    def test_unified_mount_discovery_counts_descendant_processes(self):
        with tempfile.TemporaryDirectory() as d:
            proc, mount = self.mount_evidence(d)
            group = mount / "user.slice/test.service"
            (group / "child").mkdir(parents=True)
            (group / "cgroup.procs").write_text("12\n13\n")
            (group / "child/cgroup.procs").write_text("13\n14\n")
            with patch.object(self.m, "PROC_ROOT", proc):
                self.assertEqual(self.m.unified_cgroup_mount(), mount)
                self.assertEqual(self.m.cgroup_pids({"ControlGroup": "/user.slice/test.service"}), [12, 13, 14])
                self.assertEqual(self.m.cgroup_pids({"ControlGroup": "/missing/service"}), [])

    def test_v1_hybrid_and_subtree_hierarchies_are_not_reported_empty(self):
        cases = [("1:name=systemd:/user.slice\n", "", "/"),
                 ("0::/user.slice\n1:name=systemd:/user.slice\n", "", "/"),
                 ("0::/user.slice\n", "30 23 0:27 / /sys/fs/cgroup/systemd rw - cgroup cgroup rw\n", "/"),
                 ("0::/user.slice\n", "", "/user.slice")]
        for membership, extra, root in cases:
            with self.subTest(membership=membership, extra=extra, root=root), tempfile.TemporaryDirectory() as d:
                proc, _ = self.mount_evidence(d, membership, extra, root)
                with patch.object(self.m, "PROC_ROOT", proc), self.assertRaisesRegex(self.m.LaunchError, "Unsupported"):
                    self.m.cgroup_pids({"ControlGroup": "/missing/service"})

    def test_missing_mount_evidence_is_an_error_not_empty(self):
        with tempfile.TemporaryDirectory() as d, patch.object(self.m, "PROC_ROOT", Path(d)):
            with self.assertRaisesRegex(self.m.LaunchError, "mount"):
                self.m.cgroup_pids({"ControlGroup": ""})

    def test_live_service_missing_cgroup_is_an_error(self):
        with patch.object(self.m, "unified_cgroup_mount", return_value=SOURCE.parent), self.assertRaisesRegex(self.m.LaunchError, "no inspectable cgroup"):
            self.m.cgroup_pids({"ControlGroup": "", "ActiveState": "active", "MainPID": "123"})

    def test_unreadable_cgroup_is_an_error_not_empty(self):
        with tempfile.TemporaryDirectory() as d:
            proc, mount = self.mount_evidence(d)
            (mount / "service").mkdir()
            with patch.object(self.m, "PROC_ROOT", proc), self.assertRaisesRegex(self.m.LaunchError, "Cannot inspect"):
                self.m.cgroup_pids({"ControlGroup": "/service"})

    def test_unmanaged_cmdline_is_inspected_without_resolvable_exe(self):
        with tempfile.TemporaryDirectory() as d:
            proc = Path(d)
            (proc / "123").mkdir()
            binary = str(proc / "link-pi-display")
            (proc / "123/cmdline").write_bytes(binary.encode() + b"\0--config\0")
            with patch.object(self.m, "PROC_ROOT", proc), patch.object(self.m.os, "getuid", return_value=(proc / "123").stat().st_uid, create=True), patch.object(self.m.os, "readlink", side_effect=OSError("deleted/unreadable")):
                self.assertEqual(self.m.unmanaged_instances({"binary": binary}, set()), [123])
                self.assertEqual(self.m.unmanaged_instances({"binary": binary}, {123}), [])

    def test_unmanaged_deleted_binary_detected_even_without_cmdline(self):
        with tempfile.TemporaryDirectory() as d:
            proc = Path(d)
            (proc / "123").mkdir()
            binary = str(proc / "link-pi-display")
            native_stat = Path.stat
            def stat(path, *args, **kwargs):
                if path == proc / "123/exe":
                    return SimpleNamespace(st_dev=2049, st_ino=100, st_nlink=0)
                return native_stat(path, *args, **kwargs)
            with patch.object(self.m, "PROC_ROOT", proc), patch.object(self.m.os, "getuid", return_value=(proc / "123").stat().st_uid, create=True), patch.object(self.m.os, "readlink", return_value=binary + " (deleted)"), patch.object(Path, "stat", stat):
                self.assertEqual(self.m.unmanaged_instances({"binary": binary}, set()), [123])

    def test_unmanaged_literal_deleted_suffix_uses_raw_path_before_stat(self):
        for denied in (False, True):
            with self.subTest(denied=denied), tempfile.TemporaryDirectory() as d:
                proc = Path(d)
                p = proc / "123"
                p.mkdir()
                binary = str(proc / "literal-app (deleted)")
                (p / "cmdline").write_bytes(b"overridden-name\0--config\0")
                native_stat, native_bytes = Path.stat, Path.read_bytes
                def cmdline(path):
                    if path == p / "cmdline" and denied:
                        raise PermissionError("cmdline denied")
                    return native_bytes(path)
                def stat(path, *args, **kwargs):
                    if path == p / "exe":
                        raise PermissionError("exe stat denied")
                    return native_stat(path, *args, **kwargs)
                with patch.object(self.m, "PROC_ROOT", proc), patch.object(self.m.os, "getuid", return_value=p.stat().st_uid, create=True), patch.object(self.m.os, "readlink", return_value=binary), patch.object(Path, "stat", stat), patch.object(Path, "read_bytes", cmdline):
                    self.assertEqual(self.m.unmanaged_instances({"binary": binary}, set()), [123])

    def test_unmanaged_unlinked_literal_suffix_needs_native_unlink_evidence(self):
        for denied in (False, True):
            for links in (0, 1, None):
                with self.subTest(denied=denied, links=links), tempfile.TemporaryDirectory() as d:
                    proc = Path(d)
                    p = proc / "123"
                    p.mkdir()
                    binary = str(proc / "literal-app (deleted)")
                    (p / "cmdline").write_bytes(b"overridden-name\0--config\0")
                    native_stat, native_bytes = Path.stat, Path.read_bytes
                    def cmdline(path):
                        if path == p / "cmdline" and denied:
                            raise PermissionError("cmdline denied")
                        return native_bytes(path)
                    def stat(path, *args, **kwargs):
                        if path == p / "exe":
                            if links is None:
                                raise PermissionError("exe stat denied")
                            return SimpleNamespace(st_dev=2049, st_ino=100, st_nlink=links)
                        return native_stat(path, *args, **kwargs)
                    with patch.object(self.m, "PROC_ROOT", proc), patch.object(self.m.os, "getuid", return_value=p.stat().st_uid, create=True), patch.object(self.m.os, "readlink", return_value=binary + " (deleted)"), patch.object(Path, "stat", stat), patch.object(Path, "read_bytes", cmdline):
                        if links is None:
                            with self.assertRaisesRegex(self.m.LaunchError, "unlink evidence"):
                                self.m.unmanaged_instances({"binary": binary}, set())
                        else:
                            self.assertEqual(self.m.unmanaged_instances({"binary": binary}, set()), [123] if links == 0 else [])

    def test_unmanaged_unlink_evidence_changes_fail_closed(self):
        for change in ("path", "device", "inode", "links"):
            with self.subTest(change=change), tempfile.TemporaryDirectory() as d:
                proc = Path(d)
                p = proc / "123"
                p.mkdir()
                (p / "cmdline").write_bytes(b"overridden-name\0")
                binary = str(proc / "literal-app (deleted)")
                native_stat = Path.stat
                states = iter([SimpleNamespace(st_dev=2049, st_ino=100, st_nlink=0),
                               SimpleNamespace(st_dev=2050 if change == "device" else 2049,
                                               st_ino=101 if change == "inode" else 100,
                                               st_nlink=1 if change == "links" else 0)])
                def stat(path, *args, **kwargs):
                    return next(states) if path == p / "exe" else native_stat(path, *args, **kwargs)
                raw = binary + " (deleted)"
                with patch.object(self.m, "PROC_ROOT", proc), patch.object(self.m.os, "getuid", return_value=p.stat().st_uid, create=True), patch.object(self.m.os, "readlink", side_effect=[raw, "changed" if change == "path" else raw]), patch.object(Path, "stat", stat):
                    with self.assertRaisesRegex(self.m.LaunchError, "unlink evidence changed"):
                        self.m.unmanaged_instances({"binary": binary}, set())

    def test_restart_unmanaged_conflict_does_not_stop_healthy_instance(self):
        with self.linux_main_context(), self.owned_context(), patch.object(self.m, "unmanaged_instances", return_value=[456]) as inspect, patch.object(self.m, "stop") as stop, patch.object(self.m, "start") as start:
            self.assertEqual(self.m.main(["restart"]), 1)
            self.assertEqual(inspect.call_args.args[1], {123})
            stop.assert_not_called()
            start.assert_not_called()

    def test_restart_foreign_unit_cgroup_is_never_excluded_as_owned(self):
        with self.linux_main_context(), self.owned_context(state=self.owned_state(Environment="")), patch.object(self.m, "unmanaged_instances") as inspect, patch.object(self.m, "stop") as stop:
            self.assertEqual(self.m.main(["restart"]), 1)
            inspect.assert_not_called()
            stop.assert_not_called()

    def test_start_session_mismatch_never_moves_or_stops_instance(self):
        with self.owned_context(), patch.object(self.m, "command") as run, patch.object(self.m, "status") as status:
            with self.assertRaisesRegex(self.m.LaunchError, "session A"):
                self.m.start(self.m.parser().parse_args(["start", "--session", "B"]), {})
            run.assert_not_called()
            status.assert_not_called()

    def test_restart_session_mismatch_does_not_stop_instance(self):
        with self.linux_main_context(), self.owned_context(), patch.object(self.m, "stop") as stop:
            self.assertEqual(self.m.main(["restart", "--session", "B"]), 1)
            stop.assert_not_called()

    def test_implicit_settings_remain_fallback_to_previous_selection(self):
        previous = self.record()["spec"]
        settings = dict(previous, config=str(SOURCE.parent / "binding-B.conf"))
        for action in ("start", "restart"):
            args = self.m.parser().parse_args([action])
            self.assertEqual(self.m.make_spec(args, settings, previous), previous)

    def test_duplicate_start_explicit_settings_change_is_rejected(self):
        with self.owned_context(), patch.object(self.m, "command") as run:
            args = self.m.parser().parse_args(["start", "--settings", "B.json"])
            settings = dict(self.record()["spec"], config=str(SOURCE.parent / "B.conf"))
            settings.pop("windowed")
            with self.assertRaisesRegex(self.m.LaunchError, "different settings"):
                self.m.start(args, settings)
            run.assert_not_called()

    def test_explicit_missing_settings_fails_but_default_is_optional(self):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "missing.json"
            self.assertEqual(self.m.load_settings(path), {})
            with self.assertRaisesRegex(self.m.LaunchError, "Explicit settings"):
                self.m.load_settings(path, required=True)
            with patch.object(self.m.sys, "platform", "linux"), patch.object(self.m.os, "getuid", return_value=1000, create=True), patch.object(self.m, "start") as start:
                self.assertEqual(self.m.main(["start", "--settings", str(path)]), 1)
                start.assert_not_called()

    def test_stop_and_force_never_parse_malformed_bindings(self):
        for force in (False, True):
            with self.subTest(force=force), self.linux_main_context(), patch.object(self.m, "load_settings", side_effect=self.m.LaunchError("bad JSON")) as settings, patch.object(self.m, "stop") as stop:
                argv = ["stop", "--settings", "malformed.json"] + (["--force"] if force else [])
                self.assertEqual(self.m.main(argv), 0)
                settings.assert_not_called()
                stop.assert_called_once_with("raspi-link-display.service", force)

    def test_lifecycle_lock_is_shared_across_supported_unit_names(self):
        from unittest.mock import Mock
        fcntl = SimpleNamespace(flock=Mock(), LOCK_EX=2)
        with patch.dict(self.m.sys.modules, {"fcntl": fcntl}), patch.object(self.m, "state_path", side_effect=lambda unit, create=False: SOURCE.parent / (unit + ".json")), patch.object(self.m.os, "O_NOFOLLOW", 0, create=True), patch.object(self.m.os, "open", return_value=42) as opened, patch.object(self.m.os, "close"):
            for unit in ("raspi-link-display.service", "raspi-link-display-other.service"):
                with self.m.lifecycle_lock(unit):
                    pass
            self.assertEqual(opened.call_args_list[0].args[0], opened.call_args_list[1].args[0])
            self.assertTrue(opened.call_args.args[0].endswith("lifecycle.lock"))
            self.assertEqual(fcntl.flock.call_count, 2)

    def test_literal_dollars_in_executable_and_config_are_escaped_as_arguments(self):
        spec = {"project": "/project", "binary": "/project/$HOME/${PROFILE}/app", "config": "/project/$$/${PROFILE}/$HOME.conf", "windowed": False}
        argv = self.m.service_arguments(self.record()["unit"], spec, {}, "a" * 32)
        self.assertEqual(argv[-5:], ["/usr/bin/env", "--", "/project/$$HOME/$${PROFILE}/app", "--config", "/project/$$$$/$${PROFILE}/$$HOME.conf"])
        self.assertNotIn("--expand-environment=no", argv)

    def test_native_empty_stop_arrays_are_verified_when_systemctl_omits_them(self):
        shown = subprocess.CompletedProcess([], 0, "LoadState=loaded\n", "")
        empty = subprocess.CompletedProcess([], 0, "a(sasbttttuii) 0\n", "")
        with patch.object(self.m, "command", side_effect=[shown, empty, empty]) as run:
            state = self.m.unit_state("raspi-link-display-test-native.service")
            self.assertEqual(state["ExecStop"], "")
            self.assertEqual(state["ExecStopPost"], "")
            self.assertEqual(run.call_args_list[1].args[0][0], "busctl")
            self.assertIn("/org/freedesktop/systemd1/unit/raspi_2dlink_2ddisplay_2dtest_2dnative_2eservice", run.call_args_list[1].args[0])

    def test_native_nonempty_stop_array_is_not_treated_as_safe(self):
        shown = subprocess.CompletedProcess([], 0, "LoadState=loaded\nExecStopPost=\n", "")
        nonempty = subprocess.CompletedProcess([], 0, 'a(sasbttttuii) 1 "/bin/kill" 0\n', "")
        with patch.object(self.m, "command", side_effect=[shown, nonempty]):
            self.assertNotEqual(self.m.unit_state("raspi-link-display-test-native.service")["ExecStop"], "")

    def test_native_invalid_stop_array_evidence_fails_closed(self):
        shown = subprocess.CompletedProcess([], 0, "LoadState=loaded\n", "")
        malformed = subprocess.CompletedProcess([], 0, "not an array\n", "")
        with patch.object(self.m, "command", side_effect=[shown, malformed]):
            with self.assertRaises(self.m.LaunchError):
                self.m.unit_state("raspi-link-display-test-native.service")

    def test_unit_query_includes_empty_shutdown_properties(self):
        result = subprocess.CompletedProcess([], 0, "LoadState=loaded\nExecStop=\nExecStopPost=\n", "")
        with patch.object(self.m, "command", return_value=result) as run:
            state = self.m.unit_state(self.record()["unit"])
            self.assertIn("--all", run.call_args.args[0])
            self.assertEqual(state["ExecStop"], "")
            self.assertEqual(state["ExecStopPost"], "")

    def test_pending_exec_wrapper_transition_pins_pid_and_start_ticks(self):
        record = self.record("pending")
        wrapper = dict(self.record()["identity"], executable=str(Path("/usr/bin/env").resolve()))
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / (record["unit"] + ".json")
            with patch.object(self.m, "process_identity", return_value=wrapper):
                record = self.m.reconcile_record(self.owned_state(), record, path)
            self.assertEqual(record["phase"], "pending")
            self.assertEqual(record["identity"], wrapper)
            for identity in (dict(self.record()["identity"], pid=124), dict(self.record()["identity"], start_ticks="457"), dict(self.record()["identity"], executable=str(SOURCE.parent / "foreign"))):
                with self.subTest(identity=identity), patch.object(self.m, "process_identity", return_value=identity), self.assertRaises(self.m.LaunchError):
                    self.m.reconcile_record(self.owned_state(), record, path)
            with patch.object(self.m, "process_identity", return_value=self.record()["identity"]):
                record = self.m.reconcile_record(self.owned_state(), record, path)
            self.assertEqual(record["phase"], "running")
            self.assertEqual(record["identity"], self.record()["identity"])

    def test_stop_and_force_reject_unsupported_cgroup_before_signalling(self):
        for force in (False, True):
            with self.subTest(force=force), self.owned_context(), patch.object(self.m, "cgroup_pids", side_effect=self.m.LaunchError("Unsupported hierarchy")), patch.object(self.m, "command") as run:
                with self.assertRaisesRegex(self.m.LaunchError, "Unsupported"):
                    self.m.stop(self.record()["unit"], force)
                run.assert_not_called()

    def test_force_stop_does_not_signal_a_unit_that_disappeared(self):
        with self.owned_context() as mocks, patch.object(self.m, "command") as run:
            mocks["unit_state"].side_effect = [self.owned_state(), {"LoadState": "not-found"}]
            self.m.stop(self.record()["unit"], True)
            run.assert_not_called()

    def test_unmanaged_relative_cmdline_is_detected_without_exe(self):
        with tempfile.TemporaryDirectory() as d:
            proc = Path(d)
            (proc / "123").mkdir()
            (proc / "123/cmdline").write_bytes(b"./build/link-pi-display\0--config\0")
            with patch.object(self.m, "PROC_ROOT", proc), patch.object(self.m.os, "getuid", return_value=(proc / "123").stat().st_uid, create=True), patch.object(self.m.os, "readlink", side_effect=OSError("unreadable")):
                self.assertEqual(self.m.unmanaged_instances({"binary": str(proc / "link-pi-display")}, set()), [123])

    def test_create_command_error_retains_new_attempt_not_previous_selection(self):
        args = self.m.parser().parse_args(["start", "--config", str(SOURCE.parent / "B.conf")])
        with self.owned_context() as mocks, patch.object(self.m, "validate_spec"), patch.object(self.m, "desktop_environment", return_value=("A", self.record()["desktop"])), patch.object(self.m, "unified_cgroup_mount"), patch.object(self.m, "unmanaged_instances", return_value=[]), patch.object(self.m, "command", side_effect=self.m.LaunchError("creation failed")):
            mocks["unit_state"].return_value = {"LoadState": "not-found", "ActiveState": "inactive"}
            mocks["cgroup_pids"].return_value = []
            with self.assertRaisesRegex(self.m.LaunchError, "creation failed"):
                self.m.start(args, {})
            saved = mocks["save_state"].call_args.args[1]
            self.assertEqual(saved["spec"]["config"], args.config)
            self.assertEqual(saved["phase"], "failed")
            self.assertIsNone(saved["invocation_id"])
            self.assertIsNone(saved["identity"])
            self.assertNotEqual(saved["provenance"], self.record()["provenance"])
            self.m.validate_record(saved)

    def test_duplicate_start_same_owned_spec_is_unchanged(self):
        with self.owned_context(), patch.object(self.m, "unmanaged_instances", return_value=[]), patch.object(self.m, "command") as run, patch.object(self.m, "status", return_value={"unchanged": True}) as status:
            result = self.m.start(self.m.parser().parse_args(["start", "--session", "A"]), {})
            self.assertEqual(result, {"unchanged": True})
            status.assert_called_once_with(self.record()["unit"])
            run.assert_not_called()

    def test_cross_unit_matching_instance_blocks_creation(self):
        args = self.m.parser().parse_args(["start", "--unit", "raspi-link-display-other.service"])
        with self.owned_context() as mocks, patch.object(self.m, "validate_spec"), patch.object(self.m, "desktop_environment", return_value=("A", self.record()["desktop"])), patch.object(self.m, "unified_cgroup_mount"), patch.object(self.m, "unmanaged_instances", return_value=[123]) as inspect, patch.object(self.m, "command") as run:
            mocks["unit_state"].return_value = {"LoadState": "not-found", "ActiveState": "inactive"}
            mocks["cgroup_pids"].return_value = []
            with self.assertRaisesRegex(self.m.LaunchError, "Unmanaged"):
                self.m.start(args, {})
            self.assertEqual(inspect.call_args.args[1], set())
            run.assert_not_called()
            mocks["save_state"].assert_not_called()

    def test_pending_shebang_fixture_records_actual_interpreter_identity(self):
        record = self.record("pending")
        record["spec"]["binary"] = str(SOURCE.parent.parent / "tests/launcher/fixture.py")
        interpreter = dict(self.record()["identity"], executable=str(SOURCE.parent / "python3"))
        with tempfile.TemporaryDirectory() as d:
            proc = Path(d) / "proc"
            (proc / "123").mkdir(parents=True)
            (proc / "123/cmdline").write_bytes(b"python3\0" + os.fsencode(record["spec"]["binary"]) + b"\0--config\0")
            path = Path(d) / (record["unit"] + ".json")
            with patch.object(self.m, "PROC_ROOT", proc), patch.object(self.m, "process_identity", return_value=interpreter):
                saved = self.m.reconcile_record(self.owned_state(), record, path)
            self.assertEqual(saved["identity"], interpreter)
            self.assertEqual(saved["phase"], "running")
            wrapper = dict(interpreter, executable=str(Path("/usr/bin/env").resolve()))
            record.update(invocation_id="b" * 32, identity=wrapper)
            with patch.object(self.m, "PROC_ROOT", proc), patch.object(self.m, "process_identity", return_value=interpreter):
                saved = self.m.reconcile_record(self.owned_state(), record, path)
            self.assertEqual(saved["identity"], interpreter)
            self.assertEqual(saved["phase"], "running")

    def test_absent_unit_does_not_hide_unsupported_mount_evidence(self):
        with patch.object(self.m, "unified_cgroup_mount", side_effect=self.m.LaunchError("Unsupported hierarchy")):
            with self.assertRaisesRegex(self.m.LaunchError, "Unsupported"):
                self.m.cgroup_pids({"LoadState": "not-found"})

    def test_managed_native_deleted_executable_retains_pinned_identity(self):
        record = self.record()
        record["version"] = 3
        record["identity"].update(exe_device=2049, exe_inode=100, exe_unlinked=False)
        deleted = dict(record["identity"], executable=record["identity"]["executable"] + " (deleted)", exe_unlinked=True)
        self.assertTrue(self.m.matches_process_identity(record, deleted))
        for change in ({"exe_inode": 101}, {"exe_device": 2050}, {"pid": 124},
                       {"start_ticks": "457"}, {"exe_unlinked": False},
                       {"executable": record["identity"]["executable"] + " (deleted) (deleted)"}):
            with self.subTest(change=change):
                self.assertFalse(self.m.matches_process_identity(record, dict(deleted, **change)))

    def test_version_two_requires_explicit_native_identity_migration(self):
        record = self.record()
        record["version"] = 2
        for key in ("exe_device", "exe_inode", "exe_unlinked"):
            record["identity"].pop(key)
        with self.assertRaisesRegex(ValueError, "migration"):
            self.m.validate_record(record)

    def test_literal_deleted_filename_is_not_normalized(self):
        record = self.record()
        record["identity"]["executable"] += " (deleted)"
        literal = copy.deepcopy(record["identity"])
        self.assertTrue(self.m.matches_process_identity(record, literal))
        self.assertFalse(self.m.matches_process_identity(record, dict(literal, executable=literal["executable"].removesuffix(" (deleted)"))))
        deleted = dict(literal, executable=literal["executable"] + " (deleted)", exe_unlinked=True)
        self.assertTrue(self.m.matches_process_identity(record, deleted))
        self.assertFalse(self.m.matches_process_identity(record, dict(deleted, exe_unlinked=False)))

    def test_reconcile_deleted_image_does_not_rewrite_original_pin(self):
        for legacy in (False, True):
            record = self.record()
            if legacy:
                record["provenance"] = {"kind": "verified-legacy", "invocation_id": record["invocation_id"],
                                        "identity": copy.deepcopy(record["identity"]), "control_group": "/user.slice/test.service"}
            observed = dict(record["identity"], executable=record["identity"]["executable"] + " (deleted)", exe_unlinked=True)
            with self.subTest(legacy=legacy), patch.object(self.m, "process_identity", return_value=observed), patch.object(self.m, "save_state") as save:
                reconciled = self.m.reconcile_record(self.owned_state(), record, Path("unused"))
                self.assertEqual(reconciled, record)
                self.m.validate_record(reconciled)
                save.assert_not_called()

    def test_managed_deleted_native_status_and_both_stop_modes(self):
        record = self.record()
        observed = dict(record["identity"], executable=record["identity"]["executable"] + " (deleted)", exe_unlinked=True)
        with self.owned_context() as mocks, patch.object(self.m, "mapped_windows", return_value=[]):
            mocks["process_identity"].return_value = observed
            result = self.m.status(record["unit"])
            self.assertEqual(result["identity"], observed)
            self.assertEqual(result["desktop"], record["desktop"])
        for force in (False, True):
            with self.subTest(force=force), self.owned_context() as mocks, patch.object(self.m, "command", return_value=subprocess.CompletedProcess([], 0, "", "")) as run:
                mocks["process_identity"].return_value = observed
                mocks["unit_state"].side_effect = [self.owned_state()] * (3 if force else 2) + [{"LoadState": "not-found"}]
                mocks["cgroup_pids"].side_effect = [[123], []]
                self.m.stop(record["unit"], force)
                self.assertEqual(run.call_count, 2 if force else 1)
                self.assertEqual(mocks["save_state"].call_args.args[1]["identity"], record["identity"])

    def test_restart_accepts_owned_replaced_native_before_stop(self):
        with self.linux_main_context(), self.owned_context() as mocks, patch.object(self.m, "unmanaged_instances", return_value=[]), patch.object(self.m, "stop") as stop, patch.object(self.m, "start") as start:
            known = self.record()["identity"]
            mocks["process_identity"].return_value = dict(known, executable=known["executable"] + " (deleted)", exe_unlinked=True)
            self.assertEqual(self.m.main(["restart"]), 0)
            stop.assert_called_once_with(self.record()["unit"])
            start.assert_called_once()

    def test_process_identity_pins_proc_exe_stat_and_rejects_exec_race(self):
        before = SimpleNamespace(st_dev=2049, st_ino=100, st_nlink=0)
        after = SimpleNamespace(st_dev=2049, st_ino=101, st_nlink=0)
        stat_text = "123 (native fixture) S " + " ".join(str(i) for i in range(4, 23))
        with patch.object(Path, "read_text", return_value=stat_text), patch.object(self.m.os, "readlink", return_value=str(SOURCE) + " (deleted)"), patch.object(Path, "stat", return_value=before) as stat:
            identity = self.m.process_identity(123)
            self.assertEqual(identity["exe_inode"], 100)
            self.assertEqual(identity["exe_device"], 2049)
            self.assertTrue(identity["exe_unlinked"])
            stat.side_effect = [before, after]
            self.assertIsNone(self.m.process_identity(123))
        with patch.object(Path, "stat", side_effect=PermissionError("denied")):
            self.assertIsNone(self.m.process_identity(123))

    @contextlib.contextmanager
    def discovered_desktop(self, sessions=("A",), leader=None, manager=None):
        leader = self.record()["desktop"] if leader is None else leader
        manager = {} if manager is None else manager
        def command(argv, **kwargs):
            if argv[:2] == ["loginctl", "list-sessions"]:
                text = "".join(sid + " 1000 pi seat0\n" for sid in sessions)
            elif argv[:2] == ["loginctl", "show-session"]:
                text = "User=1000\nRemote=no\nType=x11\nState=active\nDisplay=:0\nLeader=123\n"
            elif argv[0] == "systemctl":
                text = "\n".join(k + "=" + v for k, v in manager.items())
            else:
                text = ""
            return subprocess.CompletedProcess(argv, 0, text, "")
        with patch.object(self.m, "command", side_effect=command) as run, patch.object(self.m.os, "getuid", return_value=1000, create=True), patch.object(Path, "iterdir", return_value=iter(())), patch.object(self.m, "read_environment", return_value=leader), patch.object(self.m, "runtime_root", return_value=Path("/run/user/1000")), patch.object(self.m, "validate_desktop_values"):
            yield run

    def test_successful_desktop_discovery_binds_xset_to_saved_allowlist(self):
        desktop = dict(self.record()["desktop"], DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/1000/bus")
        with self.discovered_desktop(leader=desktop) as run, patch.dict(os.environ, {"DISPLAY": ":999", "XAUTHORITY": "/wrong"}):
            sid, values = self.m.desktop_environment()
            self.assertEqual(sid, "A")
            self.assertEqual(values, dict(desktop, XDG_RUNTIME_DIR=str(Path("/run/user/1000"))))
            self.assertEqual(run.call_args.args[0], ["xset", "q"])
            self.assertEqual(run.call_args.kwargs["env"]["DISPLAY"], ":0")
            self.assertEqual(run.call_args.kwargs["env"]["XAUTHORITY"], desktop["XAUTHORITY"])

    def test_desktop_session_ambiguity_requires_explicit_selection(self):
        with self.discovered_desktop(sessions=("A", "B")):
            with self.assertRaisesRegex(self.m.LaunchError, "found 2"):
                self.m.desktop_environment()
        with self.discovered_desktop(sessions=("A", "B")):
            self.assertEqual(self.m.desktop_environment("B")[0], "B")

    def test_readable_authority_is_accepted_and_unreadable_authority_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            authority = Path(d) / "authority"
            authority.write_bytes(b"test authority contents are never returned")
            env = dict(self.record()["desktop"], XAUTHORITY=str(authority))
            self.m.validate_desktop_values(env)
            with patch.object(self.m.os, "access", return_value=False):
                with self.assertRaisesRegex(self.m.LaunchError, "not readable"):
                    self.m.validate_desktop_values(env)

    def test_desktop_authority_absence_and_conflict_fail_closed(self):
        for leader, manager in (({}, {}), (self.record()["desktop"], {"DISPLAY": ":0", "XAUTHORITY": "/another"})):
            with self.subTest(manager=manager), self.discovered_desktop(leader=leader, manager=manager):
                with self.assertRaisesRegex(self.m.LaunchError, "absent or ambiguous"):
                    self.m.desktop_environment()

    def test_cli_forbidden_flag_combinations(self):
        with self.assertRaises(self.m.LaunchError):
            self.m.validate_options(self.m.parser().parse_args(["logs", "--follow", "--export", "x"]))
        with self.assertRaises(self.m.LaunchError):
            self.m.validate_options(self.m.parser().parse_args(["start", "--force"]))
        with self.assertRaises(self.m.LaunchError):
            self.m.validate_options(self.m.parser().parse_args(["logs", "--lines", "0"]))


class FixtureSafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import integration_fixture
        cls.fixture = integration_fixture
        loader = importlib.machinery.SourceFileLoader("safety_ctl", str(SOURCE))
        spec = importlib.util.spec_from_loader(loader.name, loader)
        cls.m = importlib.util.module_from_spec(spec)
        loader.exec_module(cls.m)

    def tracker(self, directory, module=None):
        tracker = (module or self.fixture).FixtureCleanup.__new__((module or self.fixture).FixtureCleanup)
        tracker.root = SOURCE.parent.parent
        tracker.prefix = "test-safety-"
        tracker.helper = str(SOURCE)
        tracker.ctl = self.m
        tracker.directory = Path(directory)
        tracker.records = {}
        tracker.attempts = []
        tracker.foreign = {}
        tracker.children = []
        return tracker

    def record(self, tracker, suffix, phase="running", marker="a", invocation="b"):
        record = LauncherTests.record(self, phase)
        record["unit"] = tracker.unit(suffix)
        record["provenance"]["marker"] = marker * 32
        if record["invocation_id"] is not None:
            record["invocation_id"] = invocation * 32
        return record

    def state(self, record):
        return LauncherTests.owned_state(self, record, InvocationID=record["invocation_id"] or "b" * 32,
                                         Environment=self.m.MARKER_KEY + "=" + record["provenance"]["marker"])

    def evidence(self, tracker, suffix, records):
        path = tracker.directory / ("evidence-{}.jsonl".format(len(tracker.attempts)))
        path.write_text("".join(json.dumps(record) + "\n" for record in records))
        tracker.attempts.append({"suffix": suffix, "evidence": path})
        return path

    @contextlib.contextmanager
    def native_context(self, state, record):
        with patch.object(self.m, "unit_state", side_effect=state if callable(state) else None, return_value=state if not callable(state) else None), patch.object(self.m, "process_identity", return_value=record["identity"]), patch.object(self.m, "read_state", return_value=record), patch.object(self.m, "state_path", return_value=Path("unused")), patch.object(self.m, "lifecycle_lock", side_effect=lambda unit: contextlib.nullcontext()):
            yield

    def test_collision_and_foreign_cleanup_guards_survive_optimized_compile(self):
        import types
        module = types.ModuleType("optimized_fixture")
        source = (SOURCE.parent.parent / "tests/launcher/integration_fixture.py").read_text()
        exec(compile(source, "integration_fixture.py", "exec", optimize=2), module.__dict__)
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d, module)
            record = self.record(tracker, "foreign")
            with self.native_context(self.state(record), record), patch.object(self.fixture.subprocess, "run") as run:
                with self.assertRaisesRegex(RuntimeError, "Existing test unit"):
                    tracker.fresh("foreign")
                tracker.foreign["foreign"] = {"marker": "a" * 32, "description": self.m.DESCRIPTION, "invocation_id": "c" * 32}
                with self.assertRaisesRegex(RuntimeError, "invocation mismatch"):
                    tracker.stop_foreign("foreign")
                run.assert_not_called()

    def test_existing_record_collision_is_unconditionally_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            path = Path(d) / "existing.json"
            path.write_text("{}")
            with patch.object(self.m, "unit_state", return_value={"LoadState": "not-found"}), patch.object(self.m, "state_path", return_value=path):
                with self.assertRaisesRegex(RuntimeError, "Existing record"):
                    tracker.fresh("collision")

    def test_foreign_success_path_uses_exact_invocation_guard(self):
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            record = self.record(tracker, "foreign")
            tracker.foreign["foreign"] = {"marker": "a" * 32, "description": self.m.DESCRIPTION, "invocation_id": "b" * 32}
            with self.native_context(self.state(record), record), patch.object(self.m, "unit_state", side_effect=[self.state(record), {"LoadState": "not-found"}]), patch.object(self.fixture.subprocess, "run") as run:
                tracker.stop_foreign("foreign")
                run.assert_called_once()
            changed = self.state(record)
            changed["InvocationID"] = "c" * 32
            with self.native_context(changed, record), patch.object(self.fixture.subprocess, "run") as run:
                with self.assertRaisesRegex(RuntimeError, "invocation mismatch"):
                    tracker.stop_foreign("foreign")
                run.assert_not_called()

    def test_creation_then_error_reconciles_without_status_or_window_observation(self):
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            intent = self.record(tracker, "partial", "pending")
            running = self.record(tracker, "partial")
            self.evidence(tracker, "partial", [intent])
            with self.native_context(self.state(running), running), patch.object(self.m, "mapped_windows", side_effect=RuntimeError("observation failed")) as windows, patch.object(self.m, "stop") as stop:
                errors = tracker.cleanup()
                self.assertEqual(errors, [])
                pinned = tracker.records["partial"][0]
                self.assertEqual(pinned["invocation_id"], running["invocation_id"])
                self.assertEqual(pinned["identity"], running["identity"])
                stop.assert_called_once_with(tracker.unit("partial"), force=True)
                windows.assert_not_called()

    def test_failed_restart_preserves_predecessor_and_pins_changed_invocation(self):
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            old = self.record(tracker, "restart")
            replacement = self.record(tracker, "restart", "failed", marker="c", invocation="d")
            replacement["identity"]["pid"] = 124
            replacement["identity"]["start_ticks"] = "457"
            tracker.records["restart"] = [copy.deepcopy(old)]
            intent = copy.deepcopy(replacement)
            intent.update(phase="pending", invocation_id=None, identity=None)
            self.evidence(tracker, "restart", [intent, replacement])
            state = self.state(replacement)
            state["MainPID"] = "124"
            with self.native_context(state, replacement), patch.object(self.m, "stop") as stop:
                self.assertEqual(tracker.cleanup(), [])
                self.assertEqual(len(tracker.records["restart"]), 2)
                self.assertEqual(tracker.records["restart"][0], old)
                self.assertEqual(tracker.records["restart"][1]["invocation_id"], "d" * 32)
                stop.assert_called_once_with(tracker.unit("restart"), force=True)

    def test_timeout_registers_attempt_child_and_quiesces_before_service_cleanup(self):
        from unittest.mock import Mock
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            running = self.record(tracker, "timeout")
            intent = self.record(tracker, "timeout", "pending")
            events = []
            child = Mock(pid=1234, returncode=None)
            child.poll.return_value = None
            child.communicate.side_effect = [subprocess.TimeoutExpired("helper", 1), ("", "")]
            def spawn(*args, **kwargs):
                self.assertEqual(len(tracker.attempts), 1)
                tracker.attempts[0]["evidence"].write_text(json.dumps(intent) + "\n")
                return child
            with patch.object(self.fixture.subprocess, "Popen", side_effect=spawn):
                process = tracker.begin("start", "timeout", text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertIn(process, tracker.children)
            with self.assertRaises(subprocess.TimeoutExpired):
                process.communicate(timeout=1)
            child.terminate.side_effect = lambda: events.append("quiesce")
            with self.native_context(self.state(running), running), patch.object(self.fixture.os, "killpg", side_effect=ProcessLookupError(), create=True) as probe, patch.object(self.m, "stop", side_effect=lambda *args, **kwargs: events.append("stop")):
                self.assertEqual(tracker.cleanup(), [])
            probe.assert_called_once_with(1234, 0)
            self.assertEqual(events, ["quiesce", "stop"])
            self.assertEqual(tracker.records["timeout"][0]["invocation_id"], "b" * 32)

    def test_recreated_unit_is_untouched_and_other_cleanup_entries_continue(self):
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            bad = self.record(tracker, "a-bad")
            good = self.record(tracker, "b-good")
            tracker.records = {"a-bad": [bad], "b-good": [good]}
            def state(name):
                value = self.state(bad if name == bad["unit"] else good)
                if name == bad["unit"]:
                    value["InvocationID"] = "c" * 32
                return value
            with self.native_context(state, good), patch.object(self.m, "stop") as stop:
                errors = tracker.cleanup()
            self.assertEqual(len(errors), 1)
            self.assertIn("left untouched", errors[0])
            self.assertEqual(tracker.records["a-bad"], [bad])
            stop.assert_called_once_with(good["unit"], force=True)

    def test_unproven_attempt_does_not_adopt_same_description(self):
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            record = self.record(tracker, "unproven")
            self.evidence(tracker, "unproven", [])
            with self.native_context(self.state(record), record), patch.object(self.m, "stop") as stop:
                self.assertEqual(len(tracker.cleanup()), 1)
                stop.assert_not_called()

    def test_runner_journals_intent_before_creation_and_observation_failure(self):
        import sys
        record = LauncherTests.record(self, "pending")
        with tempfile.TemporaryDirectory() as d:
            evidence = Path(d) / "attempt.jsonl"
            evidence.touch()
            def save(path, value):
                pass  # State I/O is covered separately; inspect the runner journal.
            def command(argv):
                captured = json.loads(evidence.read_text().splitlines()[0])
                self.assertEqual(captured, record)
                return subprocess.CompletedProcess(argv, 0, "", "")
            def main(argv):
                module.save_state(Path(d) / "state.json", record)
                module.command(["systemd-run"])
                raise RuntimeError("creation succeeded, observation failed")
            def load(target):
                target.save_state = save
                target.command = command
                target.main = main
            module = None
            original_from_spec = importlib.util.module_from_spec
            def make_module(spec):
                nonlocal module
                module = original_from_spec(spec)
                return module
            with patch.object(importlib.machinery.SourceFileLoader, "exec_module", side_effect=load), patch.object(importlib.util, "module_from_spec", side_effect=make_module), patch.object(sys, "argv", ["runner", str(SOURCE), str(evidence), "no", "", "start"]):
                with self.assertRaisesRegex(RuntimeError, "observation failed"):
                    exec(compile(self.fixture.HELPER_RUNNER, "helper_runner", "exec"), {"__name__": "runner"})
            self.assertEqual(json.loads(evidence.read_text()), record)

    def test_foreign_creation_timeout_is_registered_and_cleanup_pins_partial_success(self):
        from unittest.mock import Mock
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            record = self.record(tracker, "foreign")
            state = self.state(record)
            state["Description"] = "foreign fixture"
            child = Mock(pid=1234, returncode=None)
            child.poll.return_value = None
            child.communicate.side_effect = [subprocess.TimeoutExpired("systemd-run", 1), ("", "")]
            def spawn(*args, **kwargs):
                self.assertIn("foreign", tracker.foreign)
                self.assertIsNone(tracker.foreign["foreign"]["invocation_id"])
                return child
            with patch.object(self.m.secrets, "token_hex", return_value="a" * 32), patch.object(self.fixture.subprocess, "Popen", side_effect=spawn):
                with self.assertRaises(subprocess.TimeoutExpired):
                    tracker.create_foreign("foreign", "foreign fixture")
            self.assertIn(child, tracker.children)
            with self.native_context(state, record), patch.object(self.m, "unit_state", side_effect=[state, {"LoadState": "not-found"}]), patch.object(self.fixture.os, "killpg", side_effect=ProcessLookupError(), create=True), patch.object(self.fixture.subprocess, "run") as run:
                self.assertEqual(tracker.cleanup(), [])
                self.assertEqual(tracker.foreign["foreign"]["invocation_id"], "b" * 32)
                run.assert_called_once()

    def test_failed_helper_quiescence_blocks_its_unit_but_not_independent_cleanup(self):
        from unittest.mock import Mock
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            child = Mock(pid=1234)
            child.poll.return_value = None
            child.communicate.side_effect = subprocess.TimeoutExpired("helper", 10)
            tracker.children = [child]
            tracker.attempts = [{"suffix": "blocked", "child": child}]
            tracker.records["independent"] = []
            with patch.object(self.fixture.os, "killpg", create=True), patch.object(tracker, "stop_managed") as stop:
                errors = tracker.cleanup()
            self.assertEqual(len(errors), 1)
            self.assertIn("child 1234", errors[0])
            stop.assert_called_once_with("independent")

    def test_live_helper_communication_timeout_escalates_only_owned_leader_then_probes(self):
        from unittest.mock import Mock
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            child = Mock(pid=1234)
            child.poll.return_value = None
            child.communicate.side_effect = [subprocess.TimeoutExpired("helper", 10), ("", "")]
            tracker.children = [child]
            tracker.attempts = [{"suffix": "verified", "child": child}]
            with patch.object(self.fixture.os, "killpg", side_effect=ProcessLookupError(), create=True) as probe, patch.object(tracker, "stop_managed") as stop:
                self.assertEqual(tracker.cleanup(), [])
            child.terminate.assert_called_once_with()
            child.kill.assert_called_once_with()
            probe.assert_called_once_with(1234, 0)
            stop.assert_called_once_with("verified")

    def test_exited_leader_runner_records_intent_before_bounded_inner_pipe_launcher(self):
        import sys
        from unittest.mock import Mock
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            record = self.record(tracker, "seam", "pending")
            identity = LauncherTests.record(self)["identity"]
            evidence = Path(d) / "attempt.jsonl"
            notice = Path(d) / "launcher.json"
            evidence.touch()
            def popen(argv, **kwargs):
                self.assertEqual(json.loads(evidence.read_text()), record)
                self.assertEqual(kwargs["stdout"], subprocess.DEVNULL)
                self.assertEqual(kwargs["stderr"], subprocess.DEVNULL)
                self.assertNotIn("start_new_session", kwargs)
                self.assertIn("timeout=15", argv[3])
                self.assertIn("time.monotonic()+20", argv[3])
                return SimpleNamespace(pid=identity["pid"])
            def load(module):
                module.save_state = lambda path, value: None
                module.command = Mock(side_effect=AssertionError("Original creation bypassed the bounded seam"))
                module.subprocess = SimpleNamespace(Popen=popen, DEVNULL=subprocess.DEVNULL)
                module.process_identity = lambda pid: identity
                def main(argv):
                    module.save_state(Path(d) / "state", record)
                    module.command(["systemd-run", "--user", "/bin/true"])
                module.main = main
            with patch.object(importlib.machinery.SourceFileLoader, "exec_module", side_effect=load), patch.object(sys, "argv", ["runner", str(SOURCE), str(evidence), "exit", str(notice), "start"]), patch.object(os, "_exit", side_effect=RuntimeError("leader exited 73")) as exited:
                with self.assertRaisesRegex(RuntimeError, "leader exited 73"):
                    exec(compile(self.fixture.HELPER_RUNNER, "helper_runner", "exec"), {"__name__": "runner"})
            exited.assert_called_once_with(73)
            self.assertEqual(json.loads(notice.read_text()), identity)

    def test_exited_helper_survivor_blocks_unit_even_with_closed_outer_pipes(self):
        from unittest.mock import Mock
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            child = Mock(pid=1234, returncode=73)
            child.poll.return_value = 73
            child.communicate.return_value = ("", "")
            tracker.children = [child]
            tracker.attempts = [{"suffix": "blocked", "child": child}]
            tracker.records["independent"] = []
            with patch.object(self.fixture.os, "killpg", create=True) as probe, patch.object(tracker, "stop_managed") as stop:
                errors = tracker.cleanup()
            self.assertEqual(len(errors), 1)
            self.assertIn("quiescence unproven", errors[0])
            probe.assert_called_once_with(1234, 0)
            child.terminate.assert_not_called()
            child.kill.assert_not_called()
            stop.assert_called_once_with("independent")

    def test_exited_helper_group_probe_failure_blocks_without_any_signal(self):
        from unittest.mock import Mock
        for failure in (PermissionError("denied"), OSError("unavailable")):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as d:
                tracker = self.tracker(d)
                child = Mock(pid=1234, returncode=0)
                child.poll.return_value = 0
                child.communicate.return_value = ("", "")
                tracker.children = [child]
                tracker.foreign["blocked"] = {"child": child}
                with patch.object(self.fixture.os, "killpg", side_effect=failure, create=True) as probe, patch.object(tracker, "stop_foreign") as stop:
                    self.assertEqual(len(tracker.cleanup()), 1)
                probe.assert_called_once_with(1234, 0)
                child.terminate.assert_not_called()
                child.kill.assert_not_called()
                stop.assert_not_called()

    def test_native_force_residual_verification_accepts_cleared_manager_mainpid(self):
        import integration_regressions
        with tempfile.TemporaryDirectory() as d:
            tracker = self.tracker(d)
            pinned = self.record(tracker, "native-force")
            pinned["identity"]["executable"] += " (deleted)"
            pinned["spec"]["binary"] = pinned["identity"]["executable"]
            reported = {"state": "failed", "result": "timeout", "identity": None, "owned_pids": [123]}
            state = self.state(pinned)
            state.update(ActiveState="failed", MainPID="0")
            observed = dict(pinned["identity"], executable=pinned["identity"]["executable"] + " (deleted)", exe_unlinked=True)
            with patch.object(self.m, "unit_state", return_value=state), patch.object(self.m, "cgroup_pids", return_value=[123]), patch.object(self.m, "process_identity", return_value=observed):
                self.assertEqual(integration_regressions.verify_native_residual(self.m, tracker.unit("native-force"), pinned, reported), observed)
                for change in ({"pid": 124}, {"start_ticks": "457"}, {"exe_device": 2050}, {"exe_inode": 101}, {"exe_unlinked": False}, {"executable": "foreign"}):
                    with self.subTest(change=change), patch.object(self.m, "process_identity", return_value=dict(observed, **change)), self.assertRaises(RuntimeError):
                        integration_regressions.verify_native_residual(self.m, tracker.unit("native-force"), pinned, reported)
                with self.assertRaises(RuntimeError):
                    integration_regressions.verify_native_residual(self.m, tracker.unit("native-force"), pinned, dict(reported, owned_pids=[]))
                with patch.object(self.m, "cgroup_pids", return_value=[]), self.assertRaises(RuntimeError):
                    integration_regressions.verify_native_residual(self.m, tracker.unit("native-force"), pinned, reported)
                with patch.object(self.m, "unit_state", return_value=dict(state, InvocationID="c" * 32)), self.assertRaises(self.m.LaunchError):
                    integration_regressions.verify_native_residual(self.m, tracker.unit("native-force"), pinned, reported)

    def test_optimized_test_entrypoints_refuse_before_side_effects(self):
        import sys
        for name in ("integration_fixture.py", "integration_regressions.py"):
            result = subprocess.run([sys.executable, "-B", "-O", str(SOURCE.parent.parent / "tests/launcher" / name)], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Optimized", result.stderr)
            self.assertNotIn("PASS", result.stdout)

    def test_gui_event_uses_verified_desktop_not_caller_environment(self):
        import runpy
        from unittest.mock import Mock
        desktop = LauncherTests.record(self)["desktop"]
        owned = {"state": "active", "identity": {"pid": 123}, "mapped_windows": ["0x1"], "desktop": desktop}
        shown = "MainPID=123\nDescription=Raspi Link Display (link-displayctl)\nTransient=yes\n"
        responses = [subprocess.CompletedProcess([], 0, json.dumps(owned)), subprocess.CompletedProcess([], 0, shown), subprocess.CompletedProcess([], 0, "_NET_WM_PID(CARDINAL) = 123\n")]
        x = Mock()
        x.XOpenDisplay.return_value = 1
        x.XDefaultRootWindow.return_value = 1
        x.XKeysymToKeycode.return_value = 1
        x.XSendEvent.return_value = 1
        def open_display(display):
            self.assertEqual(display, b":0")
            self.assertEqual(os.environ["XAUTHORITY"], desktop["XAUTHORITY"])
            return 1
        x.XOpenDisplay.side_effect = open_display
        with patch.dict(os.environ, {"DISPLAY": ":999", "XAUTHORITY": "/wrong"}), patch("sys.argv", ["gui_key_smoke.py", "--helper", str(SOURCE), "--window", "0x1", "--pid", "123", "--key", "F1"]), patch("subprocess.run", side_effect=responses) as run, patch("ctypes.CDLL", return_value=x), patch("ctypes.util.find_library", return_value="X11"):
            runpy.run_path(str(SOURCE.parent.parent / "tests/launcher/gui_key_smoke.py"), run_name="smoke_test")
            env = run.call_args.kwargs["env"]
            self.assertEqual(env["DISPLAY"], ":0")
            self.assertEqual(env["XAUTHORITY"], desktop["XAUTHORITY"])
            x.XOpenDisplay.assert_called_once_with(b":0")


if __name__ == "__main__":
    unittest.main(verbosity=2)
