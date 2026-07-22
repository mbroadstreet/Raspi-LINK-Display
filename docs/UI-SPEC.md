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

### Bottom band
- Phase-only 4-segment phase bar with marker
- No beat count
- No numeric phase text

## v0.4 Band / Background Colors

The screen is divided into three horizontal bands with configurable background colors:

- **top_band_color** — Top status band (default: black `0,0,0,255`)
- **center_band_color** — Center tempo band (default: dark grey `18,18,18,255`)
- **bottom_band_color** — Bottom phase meter band (default: black `0,0,0,255`)

This layout (black top/bottom, dark grey center) is intentional to improve readability of the smaller status and phase elements on inexpensive small TFT Raspberry Pi displays.

The older keys `background_color` and `band_color` are supported only as deprecated compatibility aliases.

## Help Overlay (F1)

- Press **F1** in GUI mode to show a brief help overlay.
- The overlay auto-dismisses after the configured number of seconds (`help_overlay_seconds`, default 8).
- Overlay uses actual renderer output size for positioning.
- Ticket 4 candidate placement keeps the panel centered while left-aligning the title, key column, and action column. Keys and actions are rendered separately rather than aligned with embedded spaces:
  ```
  F1 Help
  Q / Esc    Quit
  F          Toggle Fullscreen
  P          Color preset
  R          Reload config
  ```
- Row order, configured colors, alpha blending, timeout, and approximate panel size are preserved.
- The candidate targets readable, unclipped output at both 480×320 and 320×240 in fullscreen and windowed modes; Pi GUI validation remains required.

## Configuration

Many visual and layout values can be changed via `config/link-pi-display.conf` without recompiling:

- Font sizes
- Colors (status, tempo, phase bar, help overlay, band backgrounds)
- Phase bar dimensions
- Help overlay timeout
- `hide_mouse_cursor` (hides cursor when in fullscreen mode)

See `config/link-pi-display.example.conf` for the full list of supported keys.

CLI options override config-file values.

## Supplied screen configurations (Ticket 4 candidate)

- `config/presets/480x320-landscape.conf` — 480×320 fullscreen, starts with `default` colors.
- `config/presets/480x320-high-contrast.conf` — 480×320 fullscreen, starts with `high_contrast` colors.
- `config/presets/320x240-landscape.conf` — 320×240 fullscreen with smaller status/tempo/bottom/help fonts and phase-bar dimensions.

These are ordinary launch-time `--config` files. Selecting another screen configuration or applying changed window dimensions requires restart. P cycles colors and R reloads the same startup file without recreating the window.

## Fullscreen Cursor Behavior (v0.4)

- When `hide_mouse_cursor=true` (default) and the app is in fullscreen, the mouse cursor is hidden.
- Toggling to windowed mode with `F` restores the cursor.
- Cursor state is not written back to the config file.
- The cursor is restored on application exit.

## Runtime visual controls (v0.6)

- `P` cycles named color presets and changes colors only.
- `R` reloads the original startup config path transactionally; window size/fullscreen remain launch-time settings and Link/window creation are unchanged.
- `F` continues to toggle the live fullscreen state.

Ticket 3 P/R behavior is accepted and Pi-validated. Ticket 4 screen-file and F1-placement changes remain candidate work pending automated Linux/Pi and visual acceptance. See `docs/RUNTIME-CONFIG.md`.
