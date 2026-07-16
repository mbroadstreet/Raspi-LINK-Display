# Runtime Visual Config (Planned)

This document describes the **planned** runtime visual configuration system for Raspi-LINK-Display.

This is a design/spec document. None of the runtime features described here are implemented yet.

## Conceptual Hierarchy

- **Screen preset** — A complete configuration file selected at launch time. It can define window size, fonts, phase bar dimensions, band colors, and one or more color presets.
- **Color preset** — A named collection of color values defined *inside* a config file. A color preset overrides only color-related settings.
- **R key** — Reloads the current config source at runtime.
- **P key** — Cycles through named color presets defined in the currently loaded config.

The design intentionally separates launch-time decisions (screen presets) from runtime adjustments (color preset cycling and config reload).

## R Key — Config Reload (Planned)

Pressing `R` in GUI mode is intended to reload the configuration source while the application is running.

### Intended Behavior

- Reload built-in defaults.
- If a config file was provided at startup and still exists, reload it.
- Re-apply the original CLI overrides on top of the reloaded config.
- Validate the resulting configuration.
- Apply safe, live visual updates (colors, fonts where possible).
- Preserve the currently active color preset by name when practical.

### Constraints for First Implementation

The initial R implementation should **not**:

- Restart the process
- Restart Ableton Link
- Recreate the SDL window
- Change `width`, `height`, or `fullscreen` at runtime
- Write any configuration files

Values that require a restart (window size, fullscreen mode) should be detected and reported/warned rather than applied live.

### Missing Config File Behavior (Planned)

- If an explicit `--config` path was provided at startup and the file is now missing → non-fatal error. Keep the current in-memory configuration and report the problem.
- If no config file was ever loaded (defaults + CLI only) → reload built-in defaults + re-apply original CLI overrides.
- This differs from startup behavior, where a missing explicit config is fatal.

### Font Reload (Planned)

A transactional font reload is desirable:

1. Parse and validate the candidate configuration.
2. Attempt to open all required fonts from the candidate configuration into temporary font objects.
3. Only if **all** candidate fonts open successfully:
   - Swap the new fonts into use.
   - Close the old fonts.
   - Apply the new visual configuration.
4. If any font fails to load:
   - Discard the candidate configuration.
   - Keep the existing fonts and configuration.
   - Report the error.

This approach avoids leaving the application in a partially configured visual state.

## P Key — Color Preset Cycling (Planned)

Pressing `P` is intended to cycle through named color presets defined in the currently loaded configuration.

### Planned Behavior

- Only colors are changed.
- Fonts, layout dimensions (`phase_bar_height`, `phase_bar_segment_gap`, `phase_bar_margin`), and window mode are **not** affected.
- No configuration files are written.
- If the current config defines no color presets, `P` should be a no-op or report "no color presets defined".

### Proposed Config Syntax

```ini
color_presets=default,high_contrast,warm_dim
color_preset=default

color_preset.default.name=Default
color_preset.default.status_inactive_color=40,44,48,255
color_preset.default.status_no_peers_color=40,44,48,255
color_preset.default.status_connected_color=75,85,95,255
color_preset.default.tempo_color=64,79,96,255
color_preset.default.phase_bar_color=83,114,151,255
color_preset.default.phase_marker_color=255,255,255,255
color_preset.default.top_band_color=0,0,0,255
color_preset.default.center_band_color=18,18,18,255
color_preset.default.bottom_band_color=0,0,0,255
color_preset.default.help_overlay_background_color=0,0,0,220
color_preset.default.help_overlay_text_color=210,210,210,255

color_preset.high_contrast.name=High Contrast
color_preset.high_contrast.status_inactive_color=220,220,220,255
color_preset.high_contrast.status_no_peers_color=220,220,220,255
color_preset.high_contrast.status_connected_color=255,255,255,255
color_preset.high_contrast.tempo_color=255,255,255,255
color_preset.high_contrast.phase_bar_color=255,255,255,255
color_preset.high_contrast.phase_marker_color=0,0,0,255
color_preset.high_contrast.top_band_color=0,0,0,255
color_preset.high_contrast.center_band_color=0,0,0,255
color_preset.high_contrast.bottom_band_color=0,0,0,255
color_preset.high_contrast.help_overlay_background_color=0,0,0,230
color_preset.high_contrast.help_overlay_text_color=255,255,255,255
```

### Rules

- `color_presets` defines the cycle order.
- `color_preset` defines the initial active preset by name.
- `color_preset.<name>.<key>` entries define named preset values.
- Top-level color keys continue to act as base/default values.
- A color preset may override any subset of color keys. Missing keys inherit from the base effective configuration.
- `--print-config` should report the active color preset and the list of available presets.

## Help Overlay (Planned Additions)

Future help overlay content is expected to include:

```
R Reload config
P Color preset
```

Existing controls (`F1`, `F`, `Q`/`Esc`) remain unchanged.

## `--print-config` (Planned Additions)

In addition to current output, `--print-config` is planned to include:

```
active_color_preset=default
color_presets=default,high_contrast,warm_dim
```

The command will continue to print the final effective color values after any active preset is applied.

## Implementation Order (Recommended)

1. `v0.6-color-presets-cycle-key` — Add color preset parsing and the `P` key.
2. `v0.6-runtime-config-reload` — Add the `R` key with safe reload semantics.
3. `v0.6-screen-preset-configs` — Add example screen preset configs and supporting documentation.

This ordering allows color preset infrastructure to exist before the more complex reload logic is added.

## Status

All features described in this document are **planned** and not yet implemented. See the linked tickets and ROADMAP.md for current status.
