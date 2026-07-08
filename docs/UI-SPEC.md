# UI/Display Contract (v0.3.6 baseline)

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
  - Colors, fonts, and layout values (many now configurable)
  - Help overlay behavior

## Current Display Layout (480×320)

### Top band
- Link status line (enlarged, configurable color)
- `LINK Inactive`
- `LINK Active · No Peers`
- `LINK Network Devices: N`

### Center band
- Large tempo (exactly two decimals, no BPM suffix)

### Bottom band
- Phase-only 4-segment blue phase bar with white marker
- No beat count
- No numeric phase text

## Help Overlay (F1)

- Press **F1** in GUI mode to show a brief help overlay.
- The overlay auto-dismisses after the configured number of seconds (`help_overlay_seconds`, default 8).
- Overlay text:
  ```
  F1 Help
  Q / Esc  Quit
  F        Toggle Fullscreen
  ```

## Configuration

Many visual and layout values can now be changed via `config/link-pi-display.conf` without recompiling:

- Font sizes
- Colors (status, tempo, phase bar, help overlay, etc.)
- Phase bar dimensions
- Help overlay timeout

See `config/link-pi-display.example.conf` for the full list of supported keys.

## Future Visual/Theming Work (Planned)

The following areas are expected to be explored in future tickets but are **not** implemented in v0.3.6:

- Warm grey tempo color
- Additional top-line color options
- Phase meter shade variations
- Alternate font support
- Centralized theme/constants in the renderer

These remain future work.