# PiTouch Desktop canonical release 0.4.0-desktop.1

Status: Owner-accepted installed-route migration. This source closeout records the already reviewed/installed helper delta and its focused tests; it does not rebuild or relabel the accepted ELF. Do not push, merge or tag without separate Owner approval.

## Source and artifact identity

- Reviewed source base: `64a8cd3911a33fc65658c1fa38cbc4c5cca88f8a`.
- This commit's production delta changes only `make_spec`, `bounded_validate_build`, `bounded_build_evidence` and their root constants in `tools/link-displayctl`. Observation acquisition fixtures and 11 focused relocation tests accompany that delta.
- Accepted unchanged native ELF: `b8d48bc0a4609fa0ca77313094ccd3e45d1f661faefecc5edc8c3550490bc048`.
- Installed helper: `f9a879bb0bb3ea12d9b28893d55b86824b30bfd918b18400ca023dd0d72d9637`.
- Runtime package: `08f97c827c7e9efa3f633c0fc77ef898e82efe407698ce077328799ef5de86ce`.
- Original as-built source47: `b2d3efdcc18d9e8414561e6f6fd4352b876764904b0ef47648f6e7118564f1c0`; baseline shipping source52: `ed56c8bcfb7ddb328c519d326ea786dbe8095c34f41a548e1a8bcaa2424a6abe`. These are distinct historical input-set identities, not the new commit's identity.

[The exact installed release manifest](releases/0.4.0-desktop.1.json) is retained byte-for-byte (SHA256 `c1d67435fb9aff4994c2bc17f2be948d299f78b267328e4348b8703b94502c95`). Its source-base plus exact production-delta fields describe the authentic package assembly; do not rewrite them to a later commit. Commit-only source/document closeout does not change native package bytes.

## Minimal maintained installation

```text
/home/pi/.local/opt/raspi-link-display/
  current -> releases/0.4.0-desktop.1
  releases/0.4.0-desktop.1/
    bin/link-pi-display
    bin/link-displayctl
    config/profile.conf
    provenance/BUILD-SELECTION.json
    provenance/native-build-success.json
    share/applications/raspi-link-display.desktop.in
    share/icons/raspi-link-display.svg
    share/licenses/raspi-link-display-MIT.txt
    share/licenses/ableton-link-LICENSE.md
    share/licenses/ableton-link-GPL-v2.md
    share/licenses/asio-BOOST-1.0.txt
    tools/link-display-user-install
    release.json
```

Public CLI is `~/.local/bin/link-displayctl`, symlinked through `current/bin/link-displayctl`. Launcher JSON contains stable `current` project/binary/config paths; each invocation resolves one concrete version. Existing XDG desktop entry/icon/installer ownership metadata remain. Keep operational locks/records/proofs under `/run/user/1000/raspi-link-display`; do not package, delete or restore them as live ownership authority.

Version/provenance directories are private0700, executable payloads0700 and other payloads0600, target UID/GID1000. Retain host-managed Python3.9/standard library, systemd user manager/D-Bus/logind/cgroup2, X11/SDL2/SDL2_ttf/ELF loader closure and stock query/presenter tools. Current profile uses system DejaVuSans-Bold fallback. Source/.git, compiler/CMake caches, headers, tests, old binaries and private QA controllers are not canonical runtime dependencies. The menu installer is retained maintenance tooling; native dry-run has no changes. Ordinary source-checkout defaults are not the installed layout: development selections must be explicit and isolated.

## Provenance and lifecycle invariants

Authenticate the original receipts against trusted immutable pins and original build associations. Historical root strings remain authentic metadata, never active filesystem lookup roots. Read provenance from the resolved helper's version-local directory under unchanged no-follow/private/single-link/bracket/deadline checks. Cache only immutable identity; attach runtime paths afterward. Canonical proof-v2 binding includes concrete runtime paths, so old-location sidecars are rejected, not translated.

Ownership, InvocationID/marker/process/cgroup/lease, lifecycle locking, safe stop and at-most-once control policies remain unchanged. Diagnostic proof never grants stop/adoption authority. `current` or JSON changes do not override remembered restart selection: promotion/rollback requires explicit settings, fresh ordinary ownership checks and genuine new-location proof. Never replay historical runtime records.

## Verification and recovery

Focused helper plus bounded-observation checks: 25 unique cases passed normally and the same25 with `-O`; unavailable Linux acquisition is modeled on Windows. Independent source/package review passed. Actual PiTouch staging verified all13 payload files, real custody/provenance acquisition, loader/font/Gio/parser/installer prerequisites and historical-proof rejection. Native installed explicit selection and restart (PIDs3697→3820), status/doctor/stop, real Gio launch (PID3983), genuine canonical proofs and final stopped/lock/focus/preservation checks passed;60strict0600receipts harvested and independently read back. Existing GUI/Link/error acceptance was reused, not rerun or newly inferred from mapping.

The paired off-device release evidence kit preserves the exact package/payload, `BUILD-PACKAGE.py`, authentication manifest, full source/Git archive, original build inputs and failed/accepted evidence. Canonical uncompressed tar identity is reproducible; byte-identical gzip reproduction uses the recorded Python3.14.9/zlib-ng1.3.1 packager. A different tested compressor produced identical tar contents but different gzip bytes. No bit-reproducible compiler-build claim is made.

For updates, privately assemble a new never-overwritten version using reviewed source, authentic matching build receipts and complete payload; verify actual target commands/dependencies/paths/custody before selection or application control. Preserve a complete verified prior installation and exact selectors/settings. Apply only separately authorized changes in a coordinated window, explicitly establish remembered selection and fresh proof, then verify installed routes/final required state. Future changed compiled inputs require a genuine new build/receipt/pin acceptance; do not edit this release's original receipts to impersonate it.

Rollback requires explicit authorization, fresh stop/ownership checks and a compatible full prior layout/controller/provenance. OS library copies are preservation/compatibility evidence, not permission to overwrite system packages. No automatic repair, rollback or version pruning. Retirement of legacy trees/backups is separately manifest-authorized after independent archival/reference/process checks; source closeout and accepted migration grant no deletion authority.

Exceptional START-recovery, full-driver/exact55 remain deferred unresolved milestones, not waived or passed. Stopped proof is bounded to the recorded exact invocation under the supported ordinary no-delegation/Owner-exclusive model, not global hidden-image absence. Physical GUI/Link/error observation remains the earlier actual Owner acceptance; no new physical acknowledgment is invented.
