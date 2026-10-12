# Launching on an existing Raspberry Pi desktop

`tools/link-displayctl` is an optional Python 3 standard-library helper. It starts the existing C++/SDL2 executable in a **manual transient systemd user service**, not a root service or boot-starting kiosk. The display user must already have an active, accessible **local X11 desktop**, user manager, `systemctl`/`systemd-run`/`busctl`, `loginctl`, `xset`, and **unified cgroup2** hierarchy. `xprop` and `xwininfo` are optional mapped-window diagnostics; without them the helper reports process state only. V1/hybrid/subtree-only cgroup mounts and Wayland are rejected rather than treated as empty/supported.

## Run from the source tree

```bash
python3 tools/link-displayctl doctor
python3 tools/link-displayctl start
python3 tools/link-displayctl status
python3 tools/link-displayctl logs
python3 tools/link-displayctl stop
```

Defaults: this source tree's `build/link-pi-display` and `config/presets/480x320-landscape.conf`. Explicit `--project`, `--binary`, and `--config` paths are supported; paths containing spaces and literal dollar signs are accepted without shell evaluation. The service uses `/usr/bin/env --` to exec the selected binary and systemd-247-compatible `$$` escaping for command arguments; it does not rely on newer `--expand-environment` options. Inspection/config validation runs from the selected project directory. Start uses explicit absolute paths and the same working directory, regardless of the SSH caller's directory.

An installed executable helper can instead use `~/.config/raspi-link-display/launcher.json`:

```json
{
  "project": "/absolute/path/to/Raspi-LINK-Display",
  "binary": "/absolute/path/to/Raspi-LINK-Display/build/link-pi-display",
  "config": "/absolute/path/to/Raspi-LINK-Display/config/presets/480x320-landscape.conf"
}
```

Settings accept only string `project`, `binary`, and `config` bindings with absolute paths (or `~` expansion); relative bindings and unknown keys are rejected. Explicit `--settings FILE` replaces the prior selection as the binding source, then CLI launch options override it. On restart/duplicate checks, implicit default bindings fall back to the last recorded selection. A missing optional default file is allowed; an explicitly selected missing file fails. `stop`, including `--force`, and `status` do not parse launcher bindings.

Install only after checking for existing helper/settings and preserving any predecessor. One reversible option is to retain the helper in a versioned candidate directory and create `~/.local/bin/link-displayctl` as a symlink to it. Give the helper executable permissions. If `~/.local/bin` is not in the SSH shell's PATH, invoke `~/.local/bin/link-displayctl` explicitly; changing shell startup files is not required.

```bash
ssh pi@PI_ADDRESS '~/.local/bin/link-displayctl start'
ssh pi@PI_ADDRESS '~/.local/bin/link-displayctl status'
ssh pi@PI_ADDRESS '~/.local/bin/link-displayctl logs --lines 100'
ssh pi@PI_ADDRESS '~/.local/bin/link-displayctl stop'
```

The helper discovers the desktop's display/authority values; do not use root, `sudo`, or `xhost +`. Multiple active local graphical sessions require an explicit `--session ID`. The helper never prints authority cookie contents.

## Lifetime and existing controls

- The application survives the initiating SSH disconnect **while its desktop remains active**.
- No boot autostart, user lingering, automatic restart, direct DRM/KMS fallback, or desktop reconfiguration is enabled.
- Q/Esc leave it stopped. F1 help, F fullscreen, P preset cycling, and R reload remain the application's existing SDL controls; the application window must receive the input.
- `stop` requests SIGTERM and checks the owned service processes. After a stop timeout it reports failure instead of secretly force-killing. `stop --force` is an explicit SIGKILL recovery operation confined to the owned unit/cgroup.
- `restart` prevalidates replacement config, desktop, and unmanaged matching conflicts before stopping the current instance, excluding only a validated owned cgroup. An explicit restart necessarily loses ephemeral application state. It reuses the preceding recorded selection unless overridden by explicit settings/CLI options. Display/font failures that arise only after SDL starts can still make the replacement fail.
- `start` is idempotent for the same managed selection; requesting different settings requires `restart`. An explicit session different from an existing instance's recorded session is rejected without moving or stopping it (also on restart). It reports independent/unmanaged matching instances, including deleted/replaced binaries, without adopting or stopping them. Lifecycle changes across all supported unit names share one user-runtime lock.
- A process/window observation does not certify correct pixels, local input, Link peer state, or visual acceptance.

## Logs and diagnostics

```bash
~/.local/bin/link-displayctl doctor
~/.local/bin/link-displayctl logs --lines 200
~/.local/bin/link-displayctl logs --follow
~/.local/bin/link-displayctl logs --export /chosen/new-file.log
```

Output is bounded by default; export refuses an existing file. The application has no SSH stdin/stdout pipes. Journal retention and reboot durability follow the machine's existing policy; this helper does not alter hostwide settings. Follow attaches only the log viewer, not the application. Some existing application stdout may buffer without a terminal.

The user unit is `raspi-link-display.service`. `status` includes process identity, allowlisted verified desktop values (not cookie contents), mapped X11 windows if observed, and exit/failure data when available. After a stop timeout, the manager may clear MainPID and status identity may be null even while `owned_pids` still contains the earlier pinned process; residual verification must inspect that PID's live native identity independently. Normally stopped transient units may be unloaded, while their journal messages remain queryable. Failed starts retain manager diagnostics and the attempted launch selection. A version-3 pending record with a random launch marker is saved before service creation; InvocationID and process identity are saved before extended window observation. An interrupted helper can reconcile only the matching service marker. Failed units with residual processes block a new start until explicitly stopped. The helper checks shutdown policy (`KillMode=control-group`, `KillSignal=SIGTERM`, no automatic SIGKILL/SIGHUP/restart, `TimeoutStopFailureMode=terminate`, no stop commands or failure/success actions/dependencies), launch provenance, InvocationID and process identity; a fixed description/transient flag alone never authorizes control of a same-name unit. Executable identity pins the native `/proc/PID/exe` device/inode, PID and start ticks. A later rebuild/unlink remains controllable only when those pins still match; one additional kernel ` (deleted)` annotation requires an unlinked inode. Literal filenames containing that suffix are never stripped. Unmanaged detection compares the untouched executable pathname first and classifies one extra deletion annotation only with stable native unlinked-inode evidence; ambiguous matching-image stat failures block launch. The persisted original identity is retained rather than rewritten to the annotated pathname.

**Existing older service migration, including version 2, is manual and must be independently verified before the new helper controls it.** Preserve the old record as evidence. Under the shared lifecycle lock, the parent/operator must check it against freshly observed live same-user service evidence (transient description, safe shutdown properties, InvocationID, actual executable/cmdline/config, PID/start ticks, cgroup, selected session and desktop environment), then repeat those checks immediately before the atomic record replacement. Only after that verification may `/run/user/UID/raspi-link-display/UNIT.service.json` be rewritten with the exact top-level fields `version: 3`, `unit`, `phase: "running"`, verified `spec`, `desktop`, `session`, `invocation_id`, `identity`, and `provenance`. Identity is `{ "pid": INTEGER, "start_ticks": "DECIMAL", "executable": "ABSOLUTE_RAW_PROC_EXE_PATH", "exe_device": NONNEGATIVE_INTEGER, "exe_inode": POSITIVE_INTEGER, "exe_unlinked": BOOLEAN }`; get device/inode/link count from `stat('/proc/PID/exe')`, following the symlink, not from `stat(binary_path)`. `exe_unlinked` is true only when that stat's link count is zero. Read `/proc/PID/stat` and `/proc/PID/exe` stat before/after the path observation and reject changed evidence. InvocationID is 32 lowercase hex characters. An already proven version-2 random-marker invocation retains its exact `{ "kind": "launch", "marker": "OBSERVED_32_HEX" }` provenance only after the live service's marker, invocation, original executable path, PID and start ticks all agree; do not infer deleted-image ownership from a version-2 pathname alone. For a separately verified legacy invocation without a marker, provenance is `{ "kind": "verified-legacy", "invocation_id": "OBSERVED_ID", "identity": OBSERVED_IDENTITY, "control_group": "OBSERVED_ABSOLUTE_CGROUP" }`; its ID/identity must exactly match the top-level values. Do not invent a marker or automatically convert an unverified old record. Subsequent launches use a new random marker and the same marker in the service's `LINK_DISPLAYCTL_LAUNCH_ID` environment. No migration is performed automatically. On systemd247, omitted empty stop-command arrays are verified with `busctl` native D-Bus properties, not assumed empty.

## Tests and boundaries

```bash
python3 tests/launcher/test_launcher.py
# or:
sh tests/launcher/test-launcher.sh
```

Tests require no third-party Python packages. `fixture.py` is for separately isolated Linux user-service tests and deliberately does not open SDL or join Link. Use distinct `raspi-link-display-test*.service` names and never substitute fixture commands for real GUI evidence.

The opt-in live harnesses refuse Python `-O`/`PYTHONOPTIMIZE` before effects and use unconditional mutation/cleanup guards. They journal each launch attempt before service creation, retain predecessor evidence, and require native isolated-process-group absence before final cleanup reconciliation. Only the directly owned helper leader is terminated; an exited leader, completed outer pipes, or a successful communication alone does not prove descendant quiescence. A surviving, reused, or uninspectable group blocks that entry and reports unsafe cleanup without any descendant/group termination signal. They reconcile only captured random-marker provenance with exact InvocationID/process checks. Recreated or unproven units are left untouched; cleanup continues with other independently verified entries and reports aggregate errors. `integration_regressions.py CANDIDATE --prefix UNIQUE_TEST_PREFIX --native-checks` additionally requires `cc` and compiles a tiny isolated native fixture only inside the fresh task-local evidence/input directory. It opens no SDL window or Link session, preserves the original compiled bytes, and checks status, graceful stop, restart and explicit force after executable replacement. Compiler availability is not required for the stdlib tests or non-native live checks. Add `--exited-leader-check` for the bounded real Linux race: an exited helper leaves a surviving delayed launcher with closed outer pipes; cleanup must report blocked, then native group absence permits reconciliation and removal of its late-created nonce-owned service. The regression uses a foreign-process sentinel and always releases/waits the bounded launcher and verifies final cleanup; it needs no compiler. The opt-in `gui_key_smoke.py` binds both X11 inspection and event delivery to the helper's verified saved desktop, not the caller's display/authority; event behavior still needs independent assertions.

This helper adds OS process lifecycle control, **not** application UI IPC or a network control API. It leaves GUI source, font work, and the optional existing root/system unit template unchanged. Boot/session-recovery and live UI command routing remain separate work.

Do not push, merge, tag, or publish without explicit Owner approval.
