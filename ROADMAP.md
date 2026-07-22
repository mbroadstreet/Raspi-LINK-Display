# Roadmap

## Current Status
- v0.4 is Pi-validated and released.
- Validated at commit `a79db44` (tag/branch `v0.4-pi-validated`).
- The repository is a clean, standalone Ableton Link display module.

## Guiding Principles
- This repository must remain a standalone, buildable, manually-startable module.
- It serves as a reference template for future similar modules.
- A future container application may later combine multiple modules.
- External control and integration logic should live in separate modules, not here.
- No new runtime features in hygiene-focused releases.

## v0.5 Planned Work
- Release hygiene and module contract documentation (Batch 1 - complete).
- Config parser test harness (Batch 2 - implemented).
- `--print-config` CLI flag (Batch 2 - implemented).
- `--module-info` CLI flag (Batch 2 - implemented).
- Additional screen preset / example configs for common displays.

## Future Module / Container Direction
Raspi-LINK-Display should remain a standalone Link display module and a reference template for other modules.

External control (MIDI, OSC, DMX, web, etc.) should be developed as separate standalone modules first.

A future container app may later own:
- Shared windowing / layout when multiple modules run together
- Module switching / view management
- External control routing
- Shared logging and configuration orchestration

This repo must stay independently testable and runnable without the container.

## Deferred Ideas
- DMX input/output
- MIDI input/output (including MIDI clock)
- OSC / UDP control
- Web dashboard or remote UI
- Play/stop state display or control
- Tap tempo
- Physical button support beyond basic keyboard

## Non-goals
- This module will not become the container application.
- Direct addition of external control protocols into this module is discouraged.
- Runtime behavior changes are out of scope for documentation/hygiene releases.

## v0.6 Planned Work

Runtime visual configuration sequence:

- Color preset parsing and `P` key cycling (`v0.6-color-presets-cycle-key`) — complete.
- Runtime config reload via `R` key with safe visual updates (`v0.6-runtime-config-reload`) — Ticket 3 published, technically complete, and Pi-validated at `334116f`.
- Standalone screen configuration files plus left-aligned F1 help (`v0.6-screen-preset-configs`) — Ticket 4 implementation checkpoint `ace2a7f` passed the complete owner/Raspberry Pi validation. Its local documentation closeout requires supervisor archive review before publication.

See:
- `docs/RUNTIME-CONFIG.md`
- `docs/SCREEN-PRESETS.md`

These features are intended to be additive and should not affect launch-time behavior or existing `--print-config` / `--module-info` output beyond planned extensions.

## v0.6 Status
- Ticket 2 color presets + P cycle key are complete.
- Ticket 3 R reload is complete and Pi-validated.
- Ticket 4 runtime/config/test implementation at `ace2a7f` is owner/Pi-validated. The documentation-only closeout above that checkpoint is not yet pushed, merged, tagged, or released and still requires supervisor archive review.
- Ticket 5 font specification remains separate and has not started.
