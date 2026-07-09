# Module Contract

This document defines the contract for Raspi-LINK-Display as a standalone module and as a reference template for future modules.

## What This Module Owns

- Ableton Link session state acquisition and snapshotting via `LinkEngine`
- Its own configuration loading and strict validation (`Config`)
- Its own display rendering and layout (`SdlRenderer`, `DisplayText`)
- Its own standalone runtime modes (GUI + `--no-gui`)
- Its own manual start behavior
- Its own config file format and defaults

## What a Future Container Would Own

If multiple modules are later combined:

- Global window management and layout (when multiple views share a screen)
- Module switching / view selection
- External control routing (MIDI, OSC, DMX, web, etc.)
- Shared logging infrastructure
- Shared configuration orchestration across modules

## Module Invariants

This module **must** remain:

- Standalone buildable (`cmake` + native dependencies only)
- Manually startable without any container
- Testable in isolation
- Clear about its inputs and outputs
- Independent of any future container

## Integration Rules

- External control protocols (MIDI, OSC, DMX, web) must be developed as **separate modules** first.
- This module should not grow direct support for container-level concerns.
- Derivative modules should copy/adapt the structure, config validation approach, test plan style, and metadata conventions from this repository.

## Current Boundaries (v0.4 / v0.5)

- No external control implemented.
- `manual_start: true`
- `systemd_enabled: false` (systemd unit is optional convenience only)
- `container_integration: future`

See also: `ROADMAP.md` and `docs/INTEGRATION-NOTES.md`.