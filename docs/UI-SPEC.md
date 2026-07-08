# UI/Display Contract (v0.4 baseline)

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
- Overlay text:
  ```
  F1 Help
  Q / Esc  Quit
  F        Toggle Fullscreen
  ```

## Configuration

Many visual and layout values can be changed via `config/link-pi-display.conf` without recompiling:

- Font sizes
- Colors (status, tempo, phase bar, help overlay, band backgrounds)
- Phase bar dimensions
- Help overlay timeout
- `hide_mouse_cursor` (hides cursor when in fullscreen mode)

See `config/link-pi-display.example.conf` for the full list of supported keys.

CLI options override config-file values.

## Fullscreen Cursor Behavior (v0.4)

- When `hide_mouse_cursor=true` (default) and the app is in fullscreen, the mouse cursor is hidden.
- Toggling to windowed mode with `F` restores the cursor.
- Cursor state is not written back to the config file.
- The cursor is restored on application exit.