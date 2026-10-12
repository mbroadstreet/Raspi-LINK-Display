# Desktop launcher validation — initiated 2026-10-11, closed 2026-10-12

**Status: bounded Owner-authorized R5 correction-and-revalidation COMPLETE; independent review PASSED with no findings. Final fresh-SSH verification confirms original and test app units remain stopped, with no app/test residue. No installation, record migration, commit or push.**

This is a task-local evidence/checkpoint record, not release approval or a new authority source. Owner authorized execution of the desktop-launch proposal in the current session. Do not push, merge, tag, or publish. Independent R5 code review is complete and passed. Stable command installation and default-record migration remain outside the current authorization. Rejected predecessor snapshots remain historical evidence; functional PASS counts do not override their rejection.

## Target and preserved baseline

- Verified target: `pi@192.168.0.47`, hostname **PiTouch**, Raspberry Pi 3 Model B Plus Rev 1.3, Bullseye/X11/LightDM desktop, 480×320.
- Existing remote project: `/home/pi/projects/v0.5-test/Raspi-LINK-Display`.
- Remote clean source checkpoint: `733572fe69759fbde2bd274d1d527ce0d7e827c4`.
- Local starting checkpoint: `7b68de610db820f945acbf519adf2d3b21d87f61`, branch `v0.6-screen-preset-configs`. Diff between these checkpoints for `src`, `include`, `CMakeLists.txt`, and `config` was empty.
- Existing binary SHA-256, verified unchanged after testing: `97294629bf3d9e562c9aca515b57e1a52781e03e061c5dc5a1606e6f1458f6c5`.
- Original `480x320-landscape.conf` SHA-256, verified unchanged after testing: `b2eb6d0febd4215253abe97169cf2343d8e1405dbda3f17be3fdaa05c812ad56`.
- No C++ GUI, font implementation, boot startup, lingering, root service, host journal settings, or desktop configuration changes were made. Test reload mutations were confined to a newly created configuration copy.

## First implementation evidence — historical after correction

Deployed candidate helper SHA-256: `15085be384b318c74a1271c40c8efabb0cc39057e5b6799c226ef371af4d05c8`.

Candidate staging/evidence root: `/home/pi/projects/link-display-launcher-20261011`.

| Check | Actual result | Limit |
| --- | --- | --- |
| Existing native config parser suite | Passed from its required project working directory | Initial invocation from SSH home failed on relative paths; corrected invocation passed |
| Python launcher unit tests | 29 passed locally and on Pi Python 3.9 | Does not establish completeness; independent review found missing cases |
| Isolated live user-service fixture checks | 17 passed | Fixture opens no GUI/Link session |
| Real SDL application runtime checks | 14 passed | Programmatic targeted X11 input, not physical keyboard/touch acceptance |
| X11 screenshots | Normal Link status/tempo/phase, F1 help, P high-contrast palette, and R original palette observed | Initial screenshot before the first rendered frame was black; later screenshot rendered normally. Framebuffer evidence is not physical-panel acceptance |
| SSH detachment | Same app PID and process start ticks verified on fresh SSH connection | Desktop remained active; logout/reboot survival not promised |
| Onscreen exits | Q and Esc stopped the app without respawn | No automatic restart enabled |
| Reload behavior | Successful reload, invalid-value rollback, and invalid-font rollback exercised | Original configuration/fonts were retained |
| Remote lifecycle | Explicit restart and graceful SIGTERM stop exercised | Final safety review requires strengthened ownership handling |
| Other desktop roles | Original LXSession, Openbox and VNC process identities remained alive | Not an exhaustive machine-health audit |

Detailed remote receipts: `evidence/fixture-results.json` and `evidence/gui-results.json`. Exact real-GUI test source: `evidence/gui-runtime-checks.py`. Fixture/test sources remain in the candidate `tests/launcher/` tree. Logs and screenshots remain under `evidence/`.

Short, sequential ten-second comparison samples used the same existing binary/configuration and X11 backend:

| Launch method | Process CPU | RSS bytes | Sample duration |
| --- | ---: | ---: | ---: |
| Managed user service | 137.25% | 64,917,504 | 10.011 s |
| Direct detached process | 135.45% | 65,437,696 | 10.011 s |

These are short process samples, not a thermal/power benchmark or proof of negligible difference. CPU above 100% reflects multicore accounting. Observed application threads included llvmpipe; launcher adoption does not optimize the existing renderer. Rendering/power work remains separate.

## Independent review gate

First review returned `passed=false`. It was a read-only review of the source snapshot, not independent live-Pi testing. Blocking findings cover:

- Per-launch provenance and invocation/process identity before stop/force.
- Cgroup hierarchy handling and failed-stop residual processes.
- Deleted/replaced unmanaged executables and cross-unit duplicate races.
- Restart conflict detection before stopping a working instance.
- Explicit settings/session precedence and missing settings errors.
- Pending/failed launch records and recovery after SSH interruption.
- Stop independence from unrelated settings parsing.
- Literal dollar signs in systemd service arguments.

A separate fix context was assigned only the helper, unit tests and necessary guide corrections and returned its changes. Parent retained deployment/verification custody. No reviewed FAIL has been converted into PASS by functional tests; fresh independent review is pending.

## Pre-migration legacy-service checkpoint — historical

At the pre-migration readback, the existing app was running normally with:

- Unit: `raspi-link-display.service`.
- InvocationID: `6bb9fcc7b8c4485898753de0b448c854`.
- PID: `6249`; start ticks: `5055726`.
- Executable: preserved original binary at the remote project path above.
- `Transient=yes`, expected description, `Restart=no`, `SendSIGKILL=no`, `KillMode=control-group`, SIGTERM.
- Runtime record: `/run/user/1000/raspi-link-display/raspi-link-display.service.json`.

This old record lacks the stronger per-launch provenance expected by the corrected helper. Do not silently adopt it. Before migration, independently reverify the then-current invocation/process/start time/executable and service properties; stop only this proven task-created instance through a bounded parent-controlled operation; preserve its old record as evidence; then start a fresh provenance-bound instance using the corrected helper. Stop on any mismatch. These observations are a dated checkpoint, not permission to signal a later reused PID or replaced unit.

## R3 runtime checkpoint — functional evidence, not review approval

- Corrected candidate: `/home/pi/projects/link-display-launcher-20261011-r3`.
- Helper SHA-256: `f5d91534d1ff1e34943b021b1e4d572086d5ea389c17ad8a8e1e44eecdb07fb1`.
- **76** unit tests passed both locally and on Pi Python 3.9; syntax/whitespace checks passed.
- **17** isolated live fixture checks and **11** targeted live regression checks passed; receipt counts were parsed and reconciled to their enumerated checks.
- Target regressions exercised explicit B settings selection in actual argv, missing settings/session rejection, conflict preflight preservation, malformed-settings-independent stop/force, literal `${PROFILE}`/`$HOME` executable/config/cwd paths, genuine interrupted pending-record recovery, same-description recreated-unit refusal, concurrent cross-unit launches, and a running deleted executable. The production PID/invocation stayed unchanged throughout the fixture regressions.
- Bullseye compatibility discovery: `systemctl show --all` still omits empty `ExecStop`/`ExecStopPost` arrays. The helper now obtains native D-Bus evidence through `busctl` rather than assuming missing properties are empty; three regression tests cover empty/nonempty/invalid evidence. The first corrected start refused on missing evidence; its live fixture was then safely reconciled and stopped by the corrected helper.
- The exact legacy production invocation above was independently checked by the parent against PID/start ticks, InvocationID, executable/argv, original-file hashes, cgroup membership, session/environment and native shutdown properties. Its record was preserved byte-for-byte at `evidence/legacy-launch-record-v1.json`, explicitly migrated with verified-legacy provenance, read back, and only then stopped/restarted. A fresh normal launch now uses random-marker provenance. Migration source/receipt: `evidence/legacy-migration.py`, `evidence/legacy-migration.json`.
- **15** post-correction real-app checks passed, including fresh-SSH identity, F1 screenshot, F/P/R, invalid-config and invalid-font transactional rollback, Q/Esc no-respawn exits, explicit restart/SIGTERM stop, and original desktop/VNC process retention. Only a task-created reload config copy was mutated.
- GUI repeat source/receipt: `evidence/gui-runtime-checks-attempt2.py`, `evidence/gui-results.json`. The first rerun stopped at a harness journal-prefix assertion; the application remained healthy and logged a successful reload. The accepted rerun uses native journal cursors for new-event assertions instead of relying on a stable formatted journal header. Failed-attempt source/log evidence is retained separately.
- Post-correction screenshots under `evidence/gui-pass2/` visibly show F1 help with Q/Esc, F, P, R; high-contrast P palette; original palette after R; and normal final `LINK Active · No Peers`, `120.00` BPM and phase segments. These are framebuffer observations, not physical-panel or keyboard/touch acceptance.
- Latest verified running checkpoint after a fresh SSH connection: PID `9764`, start ticks `5410210`, original binary/config, session `1`, one mapped X11 window, `Restart=no`, `SendSIGKILL=no`; original-file hashes remain equal to the preserved baseline and remote source remains clean. These are dated observations, not later signalling identifiers.
- Short corrected ten-second process samples: managed `135.66%` CPU / `65,732,608` RSS bytes; direct `133.45%` / `66,146,304` RSS bytes. Multicore accounting, short sequential sampling and no thermal/power measurements still apply; no renderer optimization is claimed.
- Fresh read-only code-review batch `deleg_54f4d594` returned `passed=false`. The reviewer reported reading all eight supplied files, checking frozen/workspace hashes and passing 16 selected mocked tests; no SSH/live-Pi verification or mutations were performed. Its blockers are recorded below. The frozen R3 diff/manifest remains review provenance; local assigned sources have returned from the final fix context as R4 and must not be mistaken for the unchanged deployed R3 helper.
- Stable entry point/settings targets remain absent after post-verdict SSH readback. The production R3 invocation is still active with PID `9764`, start ticks `5410210`, InvocationID `d1f883dcc18b4eeda994dc3d131d0fc1`, `Restart=no`, `SendSIGKILL=no`. Installation remains held pending correction/re-review. No Git staging, commit, push, merge, tag or publication.

## Second review blockers and bounded correction

All four material findings from the delivered R3 verdict are retained:

1. **P1, `integration_regressions.py:83–85,240–244`:** destructive-operation collision/identity checks use `assert` and disappear under optimization. Require unconditional authorization guards; prevent optimized execution from silently bypassing test-result criteria.
2. **P2, `integration_fixture.py:118–124`:** successful-path direct foreign-fixture stop lacks the remembered InvocationID comparison. Apply the same checked cleanup on every path; leave mismatches untouched.
3. **P2, helper `:324–336,460–465,620–631`:** ordinary replacement/unlink of a managed native executable adds the kernel ` (deleted)` annotation and defeats raw executable equality, blocking status/stop/force/restart for the still-owned process. Pin stable native executable identity while preserving invocation, provenance, PID/start-time and literal-filename integrity. Cover managed-native replacement independently of the existing unmanaged test.
4. **P2, fixture/regression cleanup registration:** creation followed by observation failure, failed restart with a new invocation, or concurrent-helper wait timeout can leave units/helpers unregistered. Register attempts/children before waits, reconcile genuine launch provenance on all exits, continue other independently authorized cleanup after a failure and report aggregates; never adopt mismatches.

The reviewer also suggested binding GUI test events to the verified saved desktop environment, adding successful/ambiguous desktop discovery mocks, and distinguishing mandatory `loginctl`/`xset` from optional window-observation tools.

Completed final fix batch `deleg_4f3c58ba` was assigned only `tools/link-displayctl`, the existing `test_launcher.py`, `integration_fixture.py`, `integration_regressions.py` and `gui_key_smoke.py` launcher tests, and necessary guide lines; no SSH, runtime, C++/configuration, other-file or Git changes. Parent retains deployment custody. This is the **second and final automatic fix pass**. Managed-native live tests must use an isolated compiled fixture, never a copied second GUI/Link application. Any schema migration must be explicit and independently reverified against the unchanged live app before control.

## R4 final correction return and live-test blocker

- Distinct staged candidate: `/home/pi/projects/link-display-launcher-20261011-r4`; all eight staged source hashes match the frozen local manifest. Helper SHA-256: `2ba6bbfc3ce778b4198535306cee01ce14d745b23d4e2faf82736637ce22d075`.
- Parent independently reran **100 stdlib tests locally and 100 on Pi Python 3.9**, with Python 3.9 syntax checks, and verified whitespace. Both optimized live entry points were actually invoked and rejected before effects.
- Base fixture suite completed **17 checks**, `cleanup_errors=[]`; receipt count equals the enumerated list. The optional native regression run completed **12 checks**, then exited **1**. Its receipt is partial, not a PASS verdict.
- Actual compiled isolated native fixture opened no SDL/Link session. It proved status/idempotent start/graceful stop and explicit restart after executable replacement, including a literal filename ending ` (deleted)`. Original compiled bytes and SHA-256 remain at `evidence/regression-inputs/native-fixture-original-bytes`.
- **Blocking live-test defect:** `tests/launcher/integration_regressions.py:308` dereferences `alive["identity"]["pid"]` after the nonresponsive native fixture's stop timeout. systemd247 had cleared `MainPID`; helper status correctly reported a null main identity and a residual owned PID. This raises `TypeError` before the explicit-force test and final production-preservation assertion. No automatic third correction was attempted.
- Registered cleanup reported no errors and exercised exact-owned force recovery. Parent separately read the three retained failed test units' cgroups: all have **no residual PIDs**. No matching test helper/fixture processes remain. Failed-unit diagnostics and predecessor evidence were retained, not reset globally.
- Separate fresh SSH readback confirmed production PID `9764`, start ticks `5410210`, InvocationID `d1f883dcc18b4eeda994dc3d131d0fc1`, original binary/config hashes and clean remote Git source unchanged. `Restart=no`, `SendSIGKILL=no`, `Linger=no`; one original mapped window remains. Production record is still **version 2**. **No V3 migration, R4 production restart, R4 GUI rerun or stable helper/settings installation occurred.**
- Final independent review batch `deleg_49088b33` returned **`passed=false`**, with two medium logic findings and no separately categorized security concerns. It reports verifying all eight hashes and frozen-diff reconstruction, independently passing 100 Windows-local stdlib tests normally and under `-O`, and passing AST/shell syntax checks. No SSH or live-Linux/physical-input verification was performed. **The parent live-failure steer did not land before completion**, so that failure remains separate, not independently assessed by this verdict. Complete delivered findings are retained at scratch `raspi-launcher-r4-final-review-verdict.json`; source locations below were checked against the unchanged candidate.
- Exact target receipts: `evidence/fixture-results.json`, `evidence/regression-results.json`; bootstrap/handoff source and evidence are also retained. Local copies/logs: scratch `raspi-r4-{local-tests,pi-unit-tests,bootstrap,live-tests}.log`, `raspi-r4-{fixture,regression}-results.json`, `raspi-launcher-r4-{review.diff,manifest.json}`. Terminal stdout transport omitted some middle lines; complete parsed JSON receipts, not shortened stdout, establish check counts.

## R4 rejection disposition — historical boundary superseded only by the bounded authorization below

1. **Review logic blocker, unmanaged duplicate detection — `tools/link-displayctl:493–516`, especially `:511`.** The raw executable pathname always loses a trailing ` (deleted)`, including a linked executable's literal filename suffix. With overridden or unreadable cmdline this can miss an exact unmanaged conflict and permit a duplicate start/restart. Reviewer reproduced the omission with a local stdlib mock, not live process control. Compare untouched raw paths first and distinguish an additional kernel annotation using unlink evidence; add linked/unlinked literal-suffix regressions for both cmdline conditions.
2. **Review logic blocker, exited helper/remaining descendants — `integration_fixture.py:29–44,79–83,207–225`.** Cleanup signals the registered group only while its leader is alive. An exited leader's completed outer `communicate()` does not prove its inner `systemd-run` descendant terminated; cleanup can reconcile too early and miss later service creation. Reviewer reproduced the unchecked branch with a mock, not a live Linux race. Establish verified descendant quiescence before reconciliation, or report blocked cleanup; never blindly signal a stale/reused group ID. Cover the exited-leader/surviving-launcher case.
3. **Separate parent live-test blocker — `integration_regressions.py:308`.** The native force test dereferences a null main identity after systemd clears `MainPID` on stop timeout, although the pinned process remains in the owned cgroup. Correct the assertion around residual membership/live identity and rerun the still-uncompleted explicit-force and final-preservation checks. Do not count cleanup force recovery as completion of the interrupted test.

Optimization/authorization guards, exact foreign InvocationID checks and owned deleted-image pins passed the final review, but that is not overall acceptance. Actual R4 cleanup readback establishes no remaining test processes in this run; it does not refute the untested descendant race. Frozen review sources remain unchanged. No helper/test corrections, third fix agent, migration, R4 production control or installation were performed after the final verdict.

## Owner-authorized three-defect pass and stopped-state requirement

Owner explicitly authorized a narrowly scoped correction-and-revalidation pass for the three findings above, **not an installation waiver**, and required the app to be remotely stoppable and returned to its prior **not-running** state. This authorizes only one bounded pass, not an unlimited repair loop. Default command/settings installation remains excluded, even if review passes. No commit, push, merge or tag.

- Parent freshly verified the active original invocation and immutable R3 helper, then invoked its ownership-checked **graceful remote stop**. It returned `Stopped owned application; desktop and unrelated applications untouched.` No force or broad process signal was used.
- A separate fresh SSH connection read back `load_state=not-found`, `state=inactive`, `phase=stopped`, `identity=null`, `owned_pids=[]`, `mapped_windows=[]`. A same-user process check found **no `link-pi-display` process**. The old running PID is now historical, not a later control target.
- Exact stop/readback receipts: scratch `raspi-owner-pass-initial-remote-stop.log` and `raspi-owner-pass-stopped-readback.log`. The complete prior validation record was preserved as `raspi-r4-validation-before-owner-pass.md`.
- Completed fix batch `deleg_7ce6d213` was authorized only the helper, unit tests, two existing integration harnesses and necessary factual guide lines. It has **no SSH, runtime, Git index/history or installation authority**. Parent retains live revalidation and final-stop custody; no concurrent edits to assigned sources.

## R5 executed revalidation — passed, app left stopped

- Separate candidate: `/home/pi/projects/link-display-launcher-20261011-r5`. Helper SHA-256: `3a23ce947b9cd1abc63c82ef0d576304e77f6d648d54b5882473707a80c4cb86`. Parent verified the exact five allowed source changes and all eight staged hashes; out-of-scope frozen inputs, C++/binary/config and Git HEAD/index remain unchanged.
- **108 stdlib tests passed normally and under `-O` on Windows and Pi Python 3.9**; Python 3.9 AST checks cover helper/test sources and embedded runners. Both optimized live entry points reject before effects. Whitespace checks passed. The unchanged native `config_parser_tests` suite also passed from its required working directory.
- **17 base fixture checks and 15 extended live regressions passed**, including `--native-checks --exited-leader-check`. All receipt counts equal enumerated checks, both native options are true and final cleanup errors are empty.
- The actual exited-leader test produced a living delayed launcher with closed outer pipes. First cleanup correctly **blocked** reconciliation on native group membership and left the launcher and separate sentinel untouched. After bounded launcher quiescence, the late-created random-marker service was reconciled and stopped; final launcher/service cleanup left no residue. This is real Linux execution, not only the earlier mock.
- The nonresponsive replaced native image reproduced `MainPID=0`, null status identity and an owned residual PID. The corrected test proved original PID/start ticks/device/inode/path/unlink evidence and cgroup membership, then actually invoked explicit force and verified zero residue. This force operation used only the isolated native fixture, never the real SDL app.
- **Two additional actual Linux unmanaged conflicts passed**: linked and unlinked literal-` (deleted)` filenames with overridden argv0 were rejected without adoption, signals or duplicate-unit creation. Unreadable cmdline/stat-failure coverage remains mocked. Original isolated native compiled bytes are preserved.
- **16 real SDL GUI/lifecycle checks passed**, using isolated unit `raspi-link-display-test-r5-real-gui.service`: doctor/launch, fresh-SSH identity, F1 help, F fullscreen/windowed, P palette, cursor-scoped fresh R success and transactional invalid-config/font retention, Q/Esc stopped/no-respawn exits, explicit restart, remote graceful SIGTERM stop, desktop/VNC identity and original-file preservation. The two unit bindings in a task-local event-sender copy were explicitly adapted; all saved-desktop/PID/window checks remain. Events succeeded even with intentionally wrong caller DISPLAY/XAUTHORITY.
- Real-app cleanup explicitly uses **`force=False`** instead of fixture force cleanup. The real app was gracefully stopped during the GUI run. A subsequent fresh SSH call returned `Already stopped.`; both the original default and isolated test units read back inactive/not-found, stopped, no identity, owned PIDs or mapped windows. No matching app, native fixture or test-launcher processes remain.
- X11 screenshots visibly show legible F1 quit/fullscreen/palette/reload help, high-contrast P display, restored blue/purple/green R palette and normal `LINK Active · No Peers` / `120.00` rendering. `desktop-after-stop.png` shows no Link window, a desktop panel and black workspace; no clock/weather browser was restarted. These are framebuffer observations, not physical keyboard/touch/panel acceptance.
- Original default service's stopped **version-2** record remains intact: it was not adopted, restarted or migrated by R5. No stable helper/settings entry point was installed. The explicitly tested R5 staged stop command is:

```bash
ssh pi@192.168.0.47 '/home/pi/projects/link-display-launcher-20261011-r5/tools/link-displayctl stop --unit raspi-link-display-test-r5-real-gui.service'
```

- Actual remote receipts/source/logs remain under candidate `evidence/`: `fixture-results.json`, `regression-results.json`, `real-gui-results.json`, `unmanaged-literal-results.json`, complete unit/base/native/GUI logs, real bootstrap/GUI sources and task-local event sender. Local copies and frozen source/parent-test manifests are in scratch with the `raspi-r5-` prefix. Final fresh-SSH receipt: `raspi-r5-final-fresh-ssh-stop.log`.
- Focused independent review **`deleg_ef0c9c2e` returned `passed=true`**, with empty security concerns, logic errors and suggestions. All three authorized findings are explicitly resolved. Reviewer verified all eight repository sources, six parent execution sources and exact frozen reconstruction, and independently passed 108 Windows-local tests normally and under `-O`, plus AST/shell checks. It reviewed complete live receipts as attributed parent evidence; no independent SSH/live-Pi/framebuffer/physical-input verification was performed. No source, frozen inputs, Git state or runtime changes were made by review.
- Frozen review snapshot SHA-256: `e48ba48c21d0bf0be0fb54a202b21c25b8d0b7bd5c23f69a3c176f7a6a83b2e4`. Parent independently rechecked all eight source and six parent-test hashes after the verdict; all remain unchanged. Complete delivered verdict is preserved locally as scratch `raspi-launcher-r5-final-review-verdict.json` and remotely at candidate `evidence/final-independent-review.json`; exact-target readback and matching SHA-256 `90c3397772f2bc547ab29a1236d22582416836daa9f95e3d63011362ec221757` were verified.
- Final fresh-SSH readback at **`2026-10-12T01:16:22-07:00`** confirmed both original and test app units inactive/not-found, stopped, no identity, no owned PIDs or mapped windows. A separate process-only probe found no matching app/test fixture/launcher processes. The original remote Git source remains clean at its preserved HEAD, executable/config hashes remain equal to baseline, `Linger=no`, and stable helper/settings targets remain absent. Local HEAD remains `7b68de610db820f945acbf519adf2d3b21d87f61`, Git index unchanged/empty and whitespace checks passed; task edits remain uncommitted.

## Closed scope and separately unauthorized work

The three-defect correction, affected mocked/native/Linux/GUI revalidation, independent review and final stopped-state checks are complete. No material blockers remain within this bounded pass. **The real app is left stopped.**

Stable helper/settings installation, default V2 record transition, commit/push/merge/tag and physical keyboard/touch/panel acceptance remain separate and **not authorized or performed**. Passing this review does not grant those permissions. No further automatic repair or validation run is pending. Original and predecessor evidence is retained.

No Git commit or publication is authorized by the verification gate.
