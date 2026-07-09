# UI/Display Contract (v0.4 baseline)

**v0.4 is Pi-validated at commit a79db44 (tag v0.4-pi-validated).**

This document describes the **display/GUI contract** separately from the runtime/API contract.

## Separation of Concerns

- **Core/Runtime Contract**:
  - Ableton Link integration via `LinkEngine`
  - `LinkDisplayState` snapshot
  - Peer count = remote peers only
  - Manual start only

- **Display/GUI Contract** (this document):
  - Screen layout and visual presentation
  - Text formatting and rendering
  - Colors, fonts, and layout values (configurable via file)
  - Help overlay behavior
  - Band/background color handling

## Current Display Layout (480×320)

### Top band
- Link status line (enlarged, configurable color)
- `LINK Inactive`
- `LINK Active · No Peers`
- `LINK Network Devices: N`

### Center band
- Large tempo (exactly two decimals, no BPM suffix)