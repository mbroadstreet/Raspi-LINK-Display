# Screen Presets (Planned)

This document describes the **planned** screen preset system.

This is a design/spec document. No screen preset functionality is implemented yet.

## What is a Screen Preset?

A screen preset is a complete, standalone configuration file chosen at launch time. It is intended to support different physical displays (different resolutions, orientations, contrast needs, etc.) without requiring code changes or recompilation.

Example future usage:

```bash
./build/link-pi-display --config config/presets/480x320-landscape.conf
./build/link-pi-display --config config/presets/480x320-high-contrast.conf
./build/link-pi-display --config config/presets/320x240-landscape.conf
```

## Location

Screen preset config files are planned to live under:

```
config/presets/
```

This directory will contain normal config files using the same syntax as `config/link-pi-display.example.conf`.

## What a Screen Preset May Define

A screen preset config file is expected to be able to contain any currently supported configuration key, plus future color preset definitions:

- `width`, `height`, `fullscreen`
- `font_path` and font sizes
- `phase_bar_*` dimensions
- Band background colors (`top_band_color`, `center_band_color`, `bottom_band_color`)
- Status, tempo, phase, and help overlay colors
- One or more named color presets (see RUNTIME-CONFIG.md)

## Launch-Time vs Runtime Distinction (Planned)

- **Launch-time only** (first implementation):
  - `width`, `height`, `fullscreen` — these determine the SDL window / renderer at startup.
  - Changing these at runtime via `R` will be deferred or reported as requiring restart.

- **Runtime reloadable** (via `R` key, planned):
  - Colors (via color presets or direct)
  - Fonts (transactionally, if practical)

## Relationship to Color Presets

A screen preset can contain multiple named color presets. The `P` key will cycle among the color presets defined inside the *currently loaded* screen preset (or base config).

Switching to a different screen preset always requires restarting the application with a different `--config` path.

## Status

Screen preset support is **planned** and not yet implemented. See ROADMAP.md for the intended implementation sequence.