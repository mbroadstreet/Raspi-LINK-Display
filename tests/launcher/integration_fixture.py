#!/usr/bin/env python3
"""Opt-in live Linux fixture tests. Requires an accessible X11 desktop/user manager."""
import sys
if __name__ == "__main__" and sys.flags.optimize:
    raise RuntimeError("Optimized live tests are refused before any effects")

import argparse
import json
import os
import re
from pathlib import Path
import signal
import subprocess

import copy
import importlib.machinery
import importlib.util
import shlex


def require(condition, message):
    """Authorization is never an assert: these checks survive Python -O."""
    if not condition:
        raise RuntimeError(message)


# The child records durable launch intent BEFORE systemd-run, then records each
# identity update. An observation failure cannot hide a successful creation.
HELPER_RUNNER = r'''import importlib.machinery,importlib.util,json,os,pathlib,sys,time
loader=importlib.machinery.SourceFileLoader("fixture_ctl",sys.argv[1]);spec=importlib.util.spec_from_loader(loader.name,loader);m=importlib.util.module_from_spec(spec);loader.exec_module(m)
evidence=pathlib.Path(sys.argv[2]);pause=sys.argv[3];notice=sys.argv[4]
original_save=m.save_state
original_command=m.command
def save(path,record):
 original_save(path,record)
 with evidence.open("a") as stream:
  stream.write(json.dumps(record)+"\n");stream.flush();os.fsync(stream.fileno())
def command(argv,*args,**kwargs):
 if argv[0]=="systemd-run" and pause=="exit":
  # A real, bounded launcher keeps only INNER stdio, not the outer helper pipes.
  # Its inherited process group survives the leader's deliberate exit.
  code = """import pathlib,subprocess,sys,time
release=pathlib.Path(sys.argv[1]);deadline=time.monotonic()+20
while not release.exists() and time.monotonic()<deadline: time.sleep(0.02)
if not release.exists(): sys.exit(74)
result=subprocess.run(sys.argv[2:],timeout=15,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
pathlib.Path(str(release)+'.result').write_text(str(result.returncode))
sys.exit(result.returncode)
"""
  child=m.subprocess.Popen([sys.executable,"-B","-c",code,notice+".release"]+argv,stdout=m.subprocess.DEVNULL,stderr=m.subprocess.DEVNULL,env=kwargs.get("env"),cwd=kwargs.get("cwd"))
  identity=m.process_identity(child.pid)
  if identity is None: raise RuntimeError("Cannot pin bounded launcher descendant")
  pathlib.Path(notice).write_text(json.dumps(identity))
  os._exit(73)
 result=original_command(argv,*args,**kwargs)
 if argv[0]=="systemd-run" and pause=="yes":
  pathlib.Path(notice).write_text("created\n");time.sleep(60)
 return result
m.save_state=save;m.command=command
sys.exit(m.main(sys.argv[5:]))
'''


class FixtureCleanup:
    """Track attempts/children, not just successful test observations.

    Preserve predecessor records and pin each captured random-marker invocation.
    Cleanup never adopts the current name/description or a changed invocation.
    """
    def __init__(self, root, prefix, evidence_name):
        self.root = root
        self.prefix = prefix
        self.helper = str(root / "tools/link-displayctl")
        loader = importlib.machinery.SourceFileLoader("cleanup_ctl", self.helper)
        spec = importlib.util.spec_from_loader(loader.name, loader)
        self.ctl = importlib.util.module_from_spec(spec)
        loader.exec_module(self.ctl)
        self.directory = root / "evidence" / evidence_name
        self.directory.mkdir(mode=0o700)
        self.attempts = []
        self.records = {}
        self.foreign = {}
        self.children = []

    def unit(self, suffix):
        return self.ctl.validate_unit("raspi-link-display-" + self.prefix + suffix + ".service")

    def fresh(self, suffix):
        name = self.unit(suffix)
        require(self.ctl.unit_state(name).get("LoadState") == "not-found",
                "Existing test unit; no adoption/cleanup: " + name)
        path = self.ctl.state_path(name)
        require(not path.exists() and not path.is_symlink(), "Existing record; choose another prefix: " + name)

    def spawn(self, argv, **kwargs):
        # Popen ownership is registered immediately, before any wait/observation.
        child = subprocess.Popen(argv, start_new_session=True, **kwargs)
        self.children.append(child)
        return child

    def begin(self, action, suffix, options=(), pause_notice=None, exited_notice=None, **kwargs):
        require(action in ("start", "restart"), "Not a launch attempt")
        require(pause_notice is None or exited_notice is None, "Choose one helper interruption seam")
        evidence = self.directory / ("attempt-{}.jsonl".format(len(self.attempts)))
        evidence.touch(exist_ok=False)
        attempt = {"suffix": suffix, "evidence": evidence}
        self.attempts.append(attempt)
        argv = [sys.executable, "-B", "-c", HELPER_RUNNER, self.helper, str(evidence),
                "exit" if exited_notice is not None else ("yes" if pause_notice is not None else "no"),
                str(exited_notice if exited_notice is not None else (pause_notice or "")),
                action, "--unit", self.unit(suffix)] + list(options)
        attempt["child"] = self.spawn(argv, **kwargs)
        return attempt["child"]

    def _pin(self, suffix, state, record):
        self.ctl.validate_record(record)
        require(record["unit"] == self.unit(suffix) and record["provenance"]["kind"] == "launch",
                "Attempt lacks exact launch provenance")
        require(self.ctl.require_owned(state, record), "Attempt unit disappeared")
        pinned = copy.deepcopy(record)
        pinned["invocation_id"] = state["InvocationID"]
        pid = int(state.get("MainPID") or 0)
        if pid:
            observed = self.ctl.process_identity(pid)
            require(self.ctl.matches_process_identity(record, observed), "Attempt process changed while pinning")
            if not self.ctl.same_executable_identity(record["identity"], observed):
                pinned["identity"] = observed
        return pinned

    def reconcile(self, suffix):
        name = self.unit(suffix)
        candidates = self.records.setdefault(suffix, [])
        for attempt in self.attempts:
            if attempt["suffix"] != suffix:
                continue
            # A killed writer can leave one partial final line; it cannot authorize
            # mutation. Earlier complete, fsynced intent remains available.
            for line in attempt["evidence"].read_text().splitlines():
                try:
                    record = json.loads(line)
                except ValueError:
                    continue
                self.ctl.validate_record(record)
                require(record["unit"] == name and record["provenance"]["kind"] == "launch", "Mismatched attempt evidence")
                marker = record["provenance"]["marker"]
                prior = next((r for r in candidates if r["provenance"]["marker"] == marker), None)
                if prior is None:
                    candidates.append(copy.deepcopy(record))
                elif prior["invocation_id"] is None and record["invocation_id"] is not None:
                    # Only fill a previously unpinned invocation; never erase a pin.
                    candidates[candidates.index(prior)] = copy.deepcopy(record)
        state = self.ctl.unit_state(name)
        if state.get("LoadState") == "not-found":
            return None
        matches = []
        for record in candidates:
            try:
                pinned = self._pin(suffix, state, record)
            except (self.ctl.LaunchError, RuntimeError):
                continue
            candidates[candidates.index(record)] = pinned
            matches.append(pinned)
        require(len(matches) == 1, "Cleanup identity mismatch/unproven attempt; left untouched: " + name)
        return matches[0]

    def authorize_existing(self, suffix, pid, ticks, binary):
        # Explicit fresh-SSH fixture handoff: require the supplied PID/start time
        # and validated random-marker record, not a status/result assertion.
        state = self.ctl.unit_state(self.unit(suffix))
        record = self.ctl.read_state(self.ctl.state_path(self.unit(suffix)))
        require(record is not None and record["spec"]["binary"] == binary, "Unexpected fixture launch")
        pinned = self._pin(suffix, state, record)
        require(pinned["identity"] is not None and pinned["identity"]["pid"] == pid
                and pinned["identity"]["start_ticks"] == ticks, "Fresh-SSH fixture identity mismatch")
        self.records.setdefault(suffix, []).append(pinned)

    def create_foreign(self, suffix, description, options=()):
        marker = self.ctl.secrets.token_hex(16)
        entry = {"marker": marker, "description": description, "invocation_id": None}
        require(suffix not in self.foreign, "Foreign attempt already registered")
        self.foreign[suffix] = entry  # Intent precedes creation/wait, including errors.
        argv = ["systemd-run", "--user", "--unit=" + self.unit(suffix), "--description=" + description,
                "--service-type=exec", "--setenv=" + self.ctl.MARKER_KEY + "=" + marker] + list(options) + ["/bin/sleep", "300"]
        child = self.spawn(argv, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        entry["child"] = child
        stdout, stderr = child.communicate(timeout=20)
        self.pin_foreign(suffix)
        require(child.returncode == 0, "Foreign fixture creation failed: " + stdout + stderr)

    def pin_foreign(self, suffix):
        entry = self.foreign[suffix]
        state = self.ctl.unit_state(self.unit(suffix))
        if state.get("LoadState") == "not-found":
            return None
        markers = [v for v in shlex.split(state.get("Environment", "")) if v.startswith(self.ctl.MARKER_KEY + "=")]
        require(state.get("Id") == self.unit(suffix) and state.get("Transient") == "yes"
                and state.get("Description") == entry["description"]
                and markers == [self.ctl.MARKER_KEY + "=" + entry["marker"]], "Foreign-fixture provenance mismatch; left untouched")
        invocation = state.get("InvocationID", "")
        require(re.fullmatch(r"[0-9a-f]{32}", invocation), "Foreign fixture has no valid invocation")
        require(entry["invocation_id"] in (None, invocation), "Foreign-fixture invocation mismatch; left untouched")
        entry["invocation_id"] = invocation
        return state

    def stop_foreign(self, suffix):
        # Same unconditional exact-invocation guard on success AND finally paths.
        with self.ctl.lifecycle_lock(self.unit(suffix)):
            if self.pin_foreign(suffix) is None:
                return
            subprocess.run(["systemctl", "--user", "stop", self.unit(suffix)], check=True, timeout=20)
            final = self.pin_foreign(suffix)
            require(final is None or final.get("ActiveState") in ("inactive", "failed"), "Foreign fixture did not stop")

    def stop_managed(self, suffix):
        with self.ctl.lifecycle_lock(self.unit(suffix)):
            pinned = self.reconcile(suffix)
            if pinned is None:
                return
            actual = self.ctl.read_state(self.ctl.state_path(self.unit(suffix)))
            require(actual is not None and actual["provenance"] == pinned["provenance"], "Cleanup record replaced; left untouched")
            self.ctl.require_owned(self.ctl.unit_state(self.unit(suffix)), pinned)
            self.ctl.require_owned(self.ctl.unit_state(self.unit(suffix)), actual)
            self.ctl.stop(self.unit(suffix), force=True)

    def cleanup(self):
        errors = []
        failed_children = set()
        # First quiesce every launch helper (including concurrent/paused ones),
        # so no service can be created after the final provenance reconciliation.
        for child in self.children:
            try:
                if child.poll() is None:
                    child.terminate()  # Popen owns this direct child; never signal a stale group.
                try:
                    child.communicate(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.communicate(timeout=10)
                # Helpers/commands/fixtures do not detach from the isolated group
                # created by spawn(). Completed outer pipes or an exited leader
                # alone cannot prove that a launcher descendant has also exited.
                # Signal 0 is a native membership probe, NOT a termination signal:
                # even a reused PGID is left untouched and blocks this entry.
                try:
                    os.killpg(child.pid, 0)
                except ProcessLookupError:
                    pass  # Kernel proves there are no remaining group members.
                except OSError as exc:
                    raise RuntimeError("native membership probe failed; descendant quiescence unproven; unit cleanup blocked: " + str(exc)) from exc
                else:
                    raise RuntimeError("process group still has members; descendant quiescence unproven; unit cleanup blocked")
            except Exception as exc:
                failed_children.add(id(child))
                errors.append("child {}: {}".format(child.pid, exc))
        blocked = {attempt["suffix"] for attempt in self.attempts if id(attempt.get("child")) in failed_children}
        blocked.update(suffix for suffix, entry in self.foreign.items() if id(entry.get("child")) in failed_children)
        suffixes = set(self.records) | {attempt["suffix"] for attempt in self.attempts}
        for suffix in sorted(suffixes - set(self.foreign) - blocked):
            try:
                self.stop_managed(suffix)
            except Exception as exc:
                errors.append(self.unit(suffix) + ": " + str(exc))
        for suffix in sorted(set(self.foreign) - blocked):
            try:
                self.stop_foreign(suffix)
            except Exception as exc:
                errors.append(self.unit(suffix) + ": " + str(exc))
        return errors


def main():
    require(not sys.flags.optimize, "Optimized fixture tests are refused before any effects")
    p = argparse.ArgumentParser()
    p.add_argument("candidate", type=Path)
    p.add_argument("--surviving-pid", type=int, required=True)
    p.add_argument("--surviving-start-ticks", required=True)
    p.add_argument("--prefix", default="test-", help="Unique test-unit suffix prefix")
    a = p.parse_args()
    if not re.fullmatch(r"test-[A-Za-z0-9_-]*", a.prefix):
        p.error("Prefix must begin test- and contain only safe unit characters")
    root = a.candidate.resolve()
    helper = str(root / "tools/link-displayctl")
    fixture = str(root / "tests/launcher/fixture.py")
    common = ["--project", str(root), "--binary", fixture]
    results = []
    cleanup = FixtureCleanup(root, a.prefix, "fixture-cleanup")


    def invoke(action, suffix, options=(), expected=0):
        cmd = [helper, action, "--unit", cleanup.unit(suffix)] + list(options)
        if action in ("start", "restart"):
            child = cleanup.begin(action, suffix, options, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            try:
                stdout, stderr = child.communicate(timeout=35)
            finally:
                cleanup.reconcile(suffix)
            r = subprocess.CompletedProcess(cmd, child.returncode, stdout, stderr)
        else:
            r = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=35)
        print(json.dumps({"command": cmd, "exit": r.returncode, "stdout": r.stdout, "stderr": r.stderr}), flush=True)
        assert r.returncode == expected, (cmd, r.returncode, r.stderr)
        return r


    def state(suffix):
        r = invoke("status", suffix)
        return json.loads(r.stdout[r.stdout.index("{"):])


    def passed(name):
        results.append(name)
        print("PASS " + name, flush=True)


    for suffix in ("fail", "ignore", "foreign", "space", "unmanaged"):
        cleanup.fresh(suffix)

    try:
        cleanup.authorize_existing("normal", a.surviving_pid, a.surviving_start_ticks, fixture)
        s = state("normal")
        assert s["state"] == "active" and s["identity"]["pid"] == a.surviving_pid and s["identity"]["start_ticks"] == a.surviving_start_ticks
        passed("fresh SSH retains exact fixture process identity")
        invoke("start", "normal", common + ["--config", str(root / "tests/launcher/fixture-normal.conf")])
        assert state("normal")["identity"] == s["identity"]
        passed("duplicate start is idempotent")
        invoke("start", "normal", common + ["--config", str(root / "tests/launcher/fixture-fail.conf")], expected=1)
        assert state("normal")["identity"] == s["identity"]
        passed("changed start settings leave running instance unchanged")
        invoke("restart", "normal", ["--config", str(root / "does-not-exist.conf")], expected=1)
        assert state("normal")["identity"] == s["identity"]
        passed("invalid replacement is rejected before stopping")
        r = invoke("logs", "normal")
        assert "fixture: ready" in r.stdout
        passed("unit journal contains stdout")
        export = root / "evidence/fixture-journal.log"
        invoke("logs", "normal", ["--export", str(export)])
        before = export.read_bytes()
        invoke("logs", "normal", ["--export", str(export)], expected=1)
        assert export.read_bytes() == before
        passed("log export refuses overwrite")
        invoke("stop", "normal")
        assert not state("normal")["owned_pids"]
        invoke("stop", "normal")
        passed("graceful targeted stop and repeated stop")
        assert "fixture: normal shutdown" in invoke("logs", "normal").stdout
        passed("graceful shutdown message is logged")
        invoke("start", "fail", common + ["--config", str(root / "tests/launcher/fixture-fail.conf")], expected=1)
        f = state("fail")
        assert f["state"] == "failed" and f["exit_status"] == "7" and not f["owned_pids"]
        assert "deliberate startup failure" in invoke("logs", "fail").stdout
        passed("startup failure exit and stderr retained without respawn")
        invoke("start", "fail", common + ["--config", str(root / "tests/launcher/fixture-normal.conf")])
        assert state("fail")["state"] == "active"
        invoke("stop", "fail")
        passed("explicit relaunch after failure")
        invoke("start", "ignore", common + ["--config", str(root / "tests/launcher/fixture-ignore.conf")])
        original = state("ignore")["identity"]
        invoke("stop", "ignore", expected=1)
        i = state("ignore")
        assert original["pid"] in i["owned_pids"]
        passed("stop timeout does not force kill")
        invoke("start", "ignore", common + ["--config", str(root / "tests/launcher/fixture-normal.conf")], expected=1)
        assert original["pid"] in state("ignore")["owned_pids"]
        passed("failed stop residue blocks duplicate start")
        invoke("stop", "ignore", ["--force"])
        assert not state("ignore")["owned_pids"]
        passed("explicit force recovery is confined to owned fixture")
        invoke("doctor", "normal", common + ["--config", str(root / "tests/launcher/fixture-normal.conf"), "--session", "999999"], expected=1)
        passed("absent graphical session fails clearly")
        foreign = "raspi-link-display-" + a.prefix + "foreign.service"
        cleanup.create_foreign("foreign", "foreign fixture")
        invoke("stop", "foreign", expected=1)
        r = subprocess.run(["systemctl", "--user", "is-active", foreign], text=True, stdout=subprocess.PIPE)
        assert r.stdout.strip() == "active"
        cleanup.stop_foreign("foreign")
        passed("foreign unit rejected and left untouched by helper")
        config = root / "tests/launcher/config with spaces;literal.conf"
        with config.open("x") as stream:
            stream.write("normal\n")
        invoke("start", "space", common + ["--config", str(config)])
        assert state("space")["launch"]["config"] == str(config)
        invoke("stop", "space")
        passed("space and shell-metacharacter path remains a literal argument")
        with (root / "evidence/unmanaged.log").open("x") as log:
            child = cleanup.spawn([fixture, "--config", str(root / "tests/launcher/fixture-normal.conf")], stdout=log, stderr=log)
            try:
                invoke("start", "unmanaged", common + ["--config", str(root / "tests/launcher/fixture-normal.conf")], expected=1)
                assert child.poll() is None
                passed("unmanaged matching instance detected and not stopped")
            finally:
                child.send_signal(signal.SIGTERM)
                child.wait(timeout=10)
    finally:
        errors = cleanup.cleanup()
        with (root / "evidence/fixture-results.json").open("x") as stream:
            json.dump({"passed": results, "count": len(results), "cleanup_errors": errors}, stream, indent=2)
            stream.write("\n")
        require(not errors, "Cleanup errors: " + "; ".join(errors))
    print("Fixture checks passed:", len(results))


if __name__ == "__main__":
    main()
