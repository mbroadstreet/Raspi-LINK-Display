#!/usr/bin/env python3
"""Opt-in real Linux/systemd/X11 regressions. Uses only fresh fixture units.

Run AFTER integration_fixture.py. No production-service control or GUI events.
A fresh evidence directory and noncolliding prefix are required.
"""
import sys
if __name__ == "__main__" and sys.flags.optimize:
    raise RuntimeError("Optimized live tests are refused before any effects")

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import time

from integration_fixture import FixtureCleanup, require


NATIVE_SOURCE = r'''#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static volatile sig_atomic_t stopping = 0;
static void stop_handler(int sig) { (void)sig; stopping = 1; }
int main(int argc, char **argv) {
    const char *config = NULL;
    int preflight = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--config") && i + 1 < argc) config = argv[++i];
        else if (!strcmp(argv[i], "--print-config")) preflight = 1;
        else if (strcmp(argv[i], "--windowed")) return 2;
    }
    if (!config) return 2;
    FILE *stream = fopen(config, "r");
    if (!stream) return 3;
    char mode[32];
    if (!fgets(mode, sizeof mode, stream)) { fclose(stream); return 3; }
    fclose(stream);
    if (preflight) { puts("native fixture: preflight"); return 0; }
    if (!strncmp(mode, "ignore", 6)) signal(SIGTERM, SIG_IGN);
    else signal(SIGTERM, stop_handler);
    puts("native fixture: ready"); fflush(stdout);
    while (!stopping) sleep(1);
    puts("native fixture: normal shutdown"); fflush(stdout);
    return 0;
}
'''


def verify_native_residual(ctl, unit, pinned, reported):
    """Manager MainPID may be zero after timeout; verify the original native pin."""
    identity = pinned["identity"]
    require(identity is not None, "Native force test lacks an earlier pinned identity")
    require(reported["state"] == "failed" and reported["result"] == "timeout",
            "Native ordinary stop did not establish the expected timeout failure")
    pid = identity["pid"]
    state = ctl.unit_state(unit)
    require(ctl.require_owned(state, pinned), "Native residual unit disappeared")
    require(pid in reported["owned_pids"] and pid in ctl.cgroup_pids(state),
            "Previously pinned native PID is no longer in the owned cgroup")
    observed = ctl.process_identity(pid)
    require(ctl.same_executable_identity(identity, observed) and ctl.matches_process_identity(pinned, observed),
            "Native residual PID/startticks/device/inode/path identity changed or is unreadable")
    require(observed["exe_unlinked"] and observed["executable"] == identity["executable"] + " (deleted)",
            "Native replacement lacks verified unlink evidence")
    return observed


def wait_for_empty_group(pgid, timeout=40):
    """Bounded read-only native proof; never terminate an unpinned group member."""
    deadline = time.monotonic() + timeout
    while True:
        try:
            os.killpg(pgid, 0)
        except ProcessLookupError:
            return
        require(time.monotonic() < deadline, "Launcher group did not quiesce: " + str(pgid))
        time.sleep(0.02)


def exited_leader_regression(root, prefix, options, work):
    """Real Linux race: leader exits, closed outer pipes, late service creation.

    The survivor is deliberately bounded (20s gate + 15s systemd-run timeout).
    Cleanup must report unsafe membership, not reconcile early or kill a stale
    group. After native quiescence, the same nonce-owned attempt is reconciled.
    """
    require(sys.platform == "linux", "Exited-leader regression requires native Linux")
    race = FixtureCleanup(root, prefix, "exited-leader-cleanup")
    sentinel_cleanup = FixtureCleanup(root, prefix, "exited-leader-sentinel-cleanup")
    suffix = "leader-exit"
    race.fresh(suffix)
    notice = work / "exited-leader-launcher.json"
    release = Path(str(notice) + ".release")
    result_path = Path(str(release) + ".result")
    evidence = {"completed": False, "first_cleanup_errors": None}
    try:
        sentinel = sentinel_cleanup.spawn(["/bin/sleep", "90"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        leader = race.begin("start", suffix, options, exited_notice=notice,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 20
        while not notice.exists() and leader.poll() is None and time.monotonic() < deadline:
            time.sleep(0.02)
        require(notice.exists(), "Exited-helper seam did not produce native launcher evidence")
        launcher = json.loads(notice.read_text())
        stdout, stderr = leader.communicate(timeout=10)
        evidence.update(leader_pid=leader.pid, leader_exit=leader.returncode,
                        launcher=launcher, helper_stdout=stdout, helper_stderr=stderr)
        require(leader.returncode == 73, "Helper did not deliberately exit at the launcher seam")
        require(race.ctl.same_executable_identity(launcher, race.ctl.process_identity(launcher["pid"]))
                and os.getpgid(launcher["pid"]) == leader.pid, "Native surviving launcher identity/group differs")
        require(race.ctl.unit_state(race.unit(suffix))["LoadState"] == "not-found", "Launcher gate created a unit early")
        first = race.cleanup()
        evidence["first_cleanup_errors"] = first
        require(len(first) == 1 and "quiescence unproven" in first[0], "Cleanup accepted an exited leader with a native survivor")
        require(suffix not in race.records, "Blocked cleanup reconciled the unit before launcher quiescence")
        require(race.ctl.same_executable_identity(launcher, race.ctl.process_identity(launcher["pid"]))
                and sentinel.poll() is None, "Blocked cleanup signalled a surviving or foreign process")
        release.touch(exist_ok=False)
        wait_for_empty_group(leader.pid)
        require(result_path.exists() and result_path.read_text() == "0", "Delayed real systemd-run did not complete successfully")
        state = race.ctl.unit_state(race.unit(suffix))
        record = race.ctl.read_state(race.ctl.state_path(race.unit(suffix)))
        require(record is not None and race.ctl.require_owned(state, record)
                and race.ctl.cgroup_pids(state), "No live nonce-owned service from the late launcher")
        evidence["late_invocation_id"] = state["InvocationID"]
        require(not race.cleanup(), "Quiescent late-creation cleanup failed")
        final = race.ctl.unit_state(race.unit(suffix))
        require(not race.ctl.cgroup_pids(final) and sentinel.poll() is None,
                "Late service residue or foreign sentinel changed")
        require(race.ctl.process_identity(launcher["pid"]) is None, "Launcher residue after cleanup")
        evidence["completed"] = True
    finally:
        # Always release/wait the bounded descendant, even when an assertion
        # fails, then retry exact-provenance cleanup. No descendant/group signals.
        errors = []
        try:
            release.touch(exist_ok=True)
        except Exception as exc:
            errors.append("bounded launcher release: " + str(exc))
        for child in race.children:
            try:
                if child.poll() is None:
                    child.terminate()
                try:
                    child.communicate(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.communicate(timeout=10)
                wait_for_empty_group(child.pid)
            except Exception as exc:
                errors.append("bounded launcher wait: " + str(exc))
        errors.extend(race.cleanup())
        errors.extend(sentinel_cleanup.cleanup())
        evidence["cleanup_errors"] = errors
        evidence["completed"] = evidence["completed"] and not errors
        with (work / "exited-leader-results.json").open("x") as stream:
            json.dump(evidence, stream, indent=2)
            stream.write("\n")
        require(not errors, "Exited-leader final cleanup errors: " + "; ".join(errors))
    return evidence


def main():
    require(not sys.flags.optimize, "Optimized regression tests are refused before any effects")
    p = argparse.ArgumentParser()
    p.add_argument("candidate", type=Path)
    p.add_argument("--prefix", required=True)
    p.add_argument("--native-checks", action="store_true", help="Compile isolated native replacement fixtures (requires cc)")
    p.add_argument("--exited-leader-check", action="store_true", help="Run the bounded real Linux exited-helper/surviving-launcher regression")
    a = p.parse_args()
    if not re.fullmatch(r"test-[A-Za-z0-9_-]+", a.prefix):
        p.error("Use a safe unique test- prefix")
    root = a.candidate.resolve()
    helper = str(root / "tools/link-displayctl")
    fixture = str(root / "tests/launcher/fixture.py")
    normal = str(root / "tests/launcher/fixture-normal.conf")
    common = ["--project", str(root), "--binary", fixture, "--config", normal]
    work = root / "evidence/regression-inputs"
    work.mkdir(mode=0o700)
    checks = []
    native_evidence = None
    exited_leader_evidence = None
    cleanup = FixtureCleanup(root, a.prefix, "regression-cleanup")


    def unit(suffix):
        return "raspi-link-display-" + a.prefix + suffix + ".service"


    def native(suffix):
        r = subprocess.run(["systemctl", "--user", "show", unit(suffix), "-p", "LoadState", "-p", "InvocationID", "-p", "MainPID", "-p", "ActiveState"], check=True, text=True, stdout=subprocess.PIPE)
        return dict(line.split("=", 1) for line in r.stdout.splitlines() if "=" in line)


    def call(action, suffix, options=(), expected=0):
        argv = [helper, action, "--unit", unit(suffix)] + list(options)
        if action in ("start", "restart"):
            process = cleanup.begin(action, suffix, options, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                stdout, stderr = process.communicate(timeout=40)
            finally:
                cleanup.reconcile(suffix)
            r = subprocess.CompletedProcess(argv, process.returncode, stdout, stderr)
        else:
            r = subprocess.run(argv, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=40)
        print(json.dumps({"action": action, "unit": unit(suffix), "exit": r.returncode, "stdout": r.stdout, "stderr": r.stderr}), flush=True)
        assert r.returncode == expected, (action, suffix, r.stdout, r.stderr)
        return r


    def status(suffix):
        return json.loads(call("status", suffix).stdout)


    def track(suffix):
        cleanup.reconcile(suffix)
        s = status(suffix)
        assert s["state"] == "active" and s["identity"]
        return s


    def start(suffix, options=None):
        call("start", suffix, common if options is None else options)
        return track(suffix)


    def passed(text):
        checks.append(text)
        print("PASS " + text, flush=True)


    def write_json(path, obj):
        with path.open("x") as f:
            json.dump(obj, f)
            f.write("\n")


    suffixes = ("bindings", "literal", "pending", "forged", "race-a", "race-b", "deleted")
    if a.native_checks:
        suffixes += ("native-normal", "native-force")
    for suffix in suffixes:
        cleanup.fresh(suffix)
    production_before = subprocess.run(["systemctl", "--user", "show", "raspi-link-display.service", "-p", "MainPID", "-p", "InvocationID"], check=True, text=True, stdout=subprocess.PIPE).stdout

    try:
        config_b = work / "normal-B.conf"
        with config_b.open("x") as f:
            f.write("normal\n")
        settings_b = work / "B.json"
        write_json(settings_b, {"project": str(root), "binary": fixture, "config": str(config_b)})
        old = start("bindings")
        call("start", "bindings", ["--settings", str(settings_b)], expected=1)
        assert status("bindings")["identity"] == old["identity"]
        passed("explicit different settings are rejected without changing the active instance")
        call("restart", "bindings", ["--settings", str(settings_b)])
        new = track("bindings")
        assert new["identity"] != old["identity"] and new["launch"]["config"] == str(config_b)
        argv = (Path("/proc") / str(new["identity"]["pid"]) / "cmdline").read_bytes().split(b"\0")
        assert os.fsencode(config_b) in argv
        passed("restart explicitly selects B in both record and actual argv")
        for action in ("start", "restart"):
            call(action, "bindings", ["--settings", str(work / "missing.json")], expected=1)
            assert status("bindings")["identity"] == new["identity"]
            call(action, "bindings", ["--session", "999999"], expected=1)
            assert status("bindings")["identity"] == new["identity"]
        passed("missing explicit bindings and session mismatches leave a healthy instance unchanged")
        duplicate_log = (work / "duplicate.log").open("x")
        duplicate = cleanup.spawn([fixture, "--config", normal], stdout=duplicate_log, stderr=duplicate_log)
        try:
            call("restart", "bindings", expected=1)
            assert status("bindings")["identity"] == new["identity"] and duplicate.poll() is None
            passed("restart rejects an unmanaged conflict before stopping its managed instance")
        finally:
            duplicate.send_signal(signal.SIGTERM)
            duplicate.wait(timeout=10)
            duplicate_log.close()
        malformed = work / "malformed.json"
        with malformed.open("x") as f:
            f.write("{broken\n")
        call("stop", "bindings", ["--settings", str(malformed)])
        assert not status("bindings")["owned_pids"]
        start("bindings")
        call("stop", "bindings", ["--settings", str(malformed), "--force"])
        assert not status("bindings")["owned_pids"]
        passed("stop and explicit force remain usable despite malformed unrelated settings")

        literal = work / "literal-${PROFILE}-$HOME"
        literal.mkdir(mode=0o700)
        literal_binary = literal / "${PROFILE}-$HOME-app"
        shutil.copyfile(fixture, literal_binary)
        literal_binary.chmod(0o700)
        literal_config = literal / "${PROFILE}-$HOME.conf"
        with literal_config.open("x") as f:
            f.write("normal\n")
        s = start("literal", ["--project", str(literal), "--binary", str(literal_binary), "--config", str(literal_config)])
        argv = (Path("/proc") / str(s["identity"]["pid"]) / "cmdline").read_bytes().split(b"\0")
        assert os.fsencode(literal_binary) in argv and os.fsencode(literal_config) in argv
        assert os.readlink(str(Path("/proc") / str(s["identity"]["pid"]) / "cwd")) == str(literal)
        passed("systemd247 preserves literal dollar executable/config paths and working directory")
        call("stop", "literal")

        notice = work / "creation-complete"
        with (work / "interrupted-helper.log").open("x") as log:
            interrupted = cleanup.begin("start", "pending", common, pause_notice=notice, stdout=log, stderr=log)
            deadline = time.monotonic() + 20
            while not notice.exists() and interrupted.poll() is None and time.monotonic() < deadline:
                time.sleep(0.05)
            assert notice.exists(), "Service creation did not reach the test seam"
            record_path = Path("/run/user") / str(os.getuid()) / "raspi-link-display" / (unit("pending") + ".json")
            record = json.loads(record_path.read_text())
            assert record["phase"] == "pending" and record["identity"] is None
            cleanup.reconcile("pending")
            interrupted.send_signal(signal.SIGTERM)
            interrupted.wait(timeout=10)
        recovered = track("pending")
        assert json.loads(record_path.read_text())["phase"] == "running" and recovered["owned_pids"]
        passed("interrupted helper after service creation recovers a genuine pending record")
        call("stop", "pending")

        start("forged")
        call("stop", "forged")
        foreign_options = ["--property=" + prop for prop in ("Restart=no", "KillMode=control-group", "KillSignal=SIGTERM", "SendSIGKILL=no", "SendSIGHUP=no", "TimeoutStopSec=3s", "TimeoutStopFailureMode=terminate", "FailureAction=none", "SuccessAction=none")]
        cleanup.create_foreign("forged", cleanup.ctl.DESCRIPTION, foreign_options)
        forged_before = native("forged")
        for force in ([], ["--force"]):
            call("stop", "forged", force, expected=1)
            assert native("forged") == forged_before
        passed("ordinary and force stop reject a recreated same-description transient unit")
        cleanup.stop_foreign("forged")

        procs = []
        for suffix in ("race-a", "race-b"):
            procs.append((suffix, cleanup.begin("start", suffix, common, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)))
        outcomes = []
        for suffix, process in procs:
            try:
                stdout, stderr = process.communicate(timeout=35)
            finally:
                cleanup.reconcile(suffix)
            print(json.dumps({"race": suffix, "exit": process.returncode, "stdout": stdout, "stderr": stderr}), flush=True)
            outcomes.append(process.returncode)
            if process.returncode == 0:
                track(suffix)
        assert sorted(outcomes) == [0, 1]
        passed("concurrent matching cross-unit launches create exactly one instance")
        for suffix in ("race-a", "race-b"):
            if cleanup.reconcile(suffix) is not None:
                call("stop", suffix)

        copied = work / "fixture.py"
        saved_copy = work / "fixture-binary-before-delete"
        shutil.copyfile("/bin/sleep", copied)
        copied.chmod(0o700)
        shutil.copyfile(copied, saved_copy)
        require(hashlib.sha256(copied.read_bytes()).digest() == hashlib.sha256(saved_copy.read_bytes()).digest(), "Test copy recovery bytes differ; refusing unlink")
        deleted = cleanup.spawn([str(copied), "90"])
        try:
            copied.unlink()  # Only the new test copy; verified recovery bytes retained.
            assert os.readlink(str(Path("/proc") / str(deleted.pid) / "exe")).endswith(" (deleted)")
            call("start", "deleted", common, expected=1)
            assert deleted.poll() is None and native("deleted")["LoadState"] == "not-found"
            passed("a running deleted/replaced matching executable blocks duplicate creation")
        finally:
            deleted.send_signal(signal.SIGTERM)
            deleted.wait(timeout=10)

        if a.exited_leader_check:
            exited_leader_evidence = exited_leader_regression(root, a.prefix, common, work)
            passed("native exited helper with closed outer pipes blocks cleanup until its surviving launcher exits; late nonce-owned service and helpers leave zero residue")

        if a.native_checks:
            compiler = shutil.which("cc")
            require(compiler is not None, "Optional native checks require cc")
            source = work / "native-fixture.c"
            with source.open("x") as stream:
                stream.write(NATIVE_SOURCE)
            binary = work / "native-fixture (deleted)"
            subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary)], check=True, timeout=60)
            saved = work / "native-fixture-original-bytes"
            shutil.copyfile(binary, saved)
            original_hash = hashlib.sha256(saved.read_bytes()).hexdigest()
            native_evidence = {"source": str(source), "binary": str(binary), "original_bytes": str(saved), "sha256": original_hash}
            def replace_native(label):
                replacement = work / ("native-replacement-" + label)
                shutil.copyfile(saved, replacement)
                replacement.chmod(0o700)
                os.replace(str(replacement), str(binary))
                assert hashlib.sha256(binary.read_bytes()).hexdigest() == original_hash
                assert hashlib.sha256(saved.read_bytes()).hexdigest() == original_hash
            native_normal = ["--project", str(work), "--binary", str(binary), "--config", normal]
            first = start("native-normal", native_normal)
            assert "native fixture: ready" in call("logs", "native-normal").stdout
            replace_native("graceful")
            after = status("native-normal")
            assert after["identity"]["executable"] == str(binary) + " (deleted)"
            assert after["identity"]["exe_unlinked"]
            assert all(after["identity"][k] == first["identity"][k] for k in ("pid", "start_ticks", "exe_device", "exe_inode"))
            call("start", "native-normal", native_normal)
            call("stop", "native-normal")
            assert not status("native-normal")["owned_pids"]
            assert "native fixture: normal shutdown" in call("logs", "native-normal").stdout
            passed("owned native status/idempotent start/graceful stop survive executable replacement, including a literal deleted suffix")
            before_restart = start("native-normal", native_normal)
            replace_native("restart")
            call("restart", "native-normal", native_normal)
            restarted = track("native-normal")
            assert (restarted["identity"]["pid"], restarted["identity"]["start_ticks"]) != (before_restart["identity"]["pid"], before_restart["identity"]["start_ticks"])
            assert restarted["identity"]["exe_inode"] != before_restart["identity"]["exe_inode"]
            call("stop", "native-normal")
            passed("explicit restart replaces an owned unlinked native image with the new pinned image")
            native_ignore = list(native_normal)
            native_ignore_config = work / "native-ignore.conf"
            with native_ignore_config.open("x") as stream:
                stream.write("ignore\n")
            native_ignore[-1] = str(native_ignore_config)
            unresponsive = start("native-force", native_ignore)
            pinned = cleanup.ctl.read_state(cleanup.ctl.state_path(unit("native-force")))
            require(pinned is not None and pinned["identity"] == unresponsive["identity"], "Native force test record differs from its earlier live pin")
            replace_native("force")
            call("stop", "native-force", expected=1)
            alive = status("native-force")
            residual = verify_native_residual(cleanup.ctl, unit("native-force"), pinned, alive)
            native_evidence["force_residual"] = {"status": alive, "verified_identity": residual}
            call("stop", "native-force", ["--force"])
            assert not status("native-force")["owned_pids"]
            assert cleanup.ctl.process_identity(pinned["identity"]["pid"]) is None
            assert hashlib.sha256(saved.read_bytes()).hexdigest() == original_hash
            passed("owned nonresponsive replaced native fixture requires explicit force; original compiled bytes retained")

        production_after = subprocess.run(["systemctl", "--user", "show", "raspi-link-display.service", "-p", "MainPID", "-p", "InvocationID"], check=True, text=True, stdout=subprocess.PIPE).stdout
        assert production_after == production_before
        passed("production display invocation and PID remain unchanged throughout fixture regressions")
    finally:
        errors = cleanup.cleanup()
        with (root / "evidence/regression-results.json").open("x") as f:
            json.dump({"passed": checks, "count": len(checks), "production_before": production_before, "cleanup_errors": errors, "native_checks": a.native_checks, "native_evidence": native_evidence, "exited_leader_check": a.exited_leader_check, "exited_leader_evidence": exited_leader_evidence, "limits": ["fixture tests, not GUI acceptance", "unified cgroup2 only on this target; unsupported topology rejection covered by unit mocks"]}, f, indent=2)
            f.write("\n")
        require(not errors, "Cleanup errors: " + "; ".join(errors))
    print("Regression checks passed:", len(checks))


if __name__ == "__main__":
    main()
