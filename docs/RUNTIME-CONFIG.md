# Runtime Visual Config

This document describes the runtime visual configuration system for Raspi-LINK-Display.

Status (v0.6 path):

- **Color presets / P key (Ticket 2):** implemented and merged on the accepted integration baseline.
- **Runtime config reload / R key (Ticket 3):** implemented on branch `v0.6-runtime-config-reload` with safe transactional semantics described below. Not Pi-validated or accepted until owner/Pi gates pass.
- **Screen preset configs (Ticket 4):** not implemented; still future work. Do not confuse with R reload.

## Conceptual Hierarchy

- **Screen preset** — A complete configuration file selected at launch time (Ticket 4). It can define window size, fonts, phase bar dimensions, band colors, and one or more color presets.
- **Color preset** — A named collection of color values defined *inside* a config file. A color preset overrides only color-related settings. Cycled with **P**.
- **R key** — Reloads the original startup config source at runtime (visual settings).
- **P key** — Cycles through named color presets in the currently loaded configuration.

The design separates launch-time decisions (screen presets / window creation) from runtime adjustments (color preset cycling and config reload).

## R Key — Config Reload (Implemented on Ticket 3 branch)

Pressing `R` in GUI mode reloads the configuration source while the application is running.

### Reload source rules

- Explicit startup `--config PATH`: every R reloads that same PATH.
- Auto-discovered `config/link-pi-display.conf` loaded at startup: every R reloads that same discovered path.
- No config file loaded at startup: R rebuilds built-in defaults and re-applies original CLI overrides. It does not discover a new file later.
- Reload never writes a config file.

### Behavior

- Rebuild/validate a complete **candidate** without modifying active state until validation succeeds.
- Re-apply original CLI overrides on every successful reload (including repeated reloads):
  `--windowed`, `--no-gui`, `--width`, `--height`, `--font`, `--tempo`, `--quantum`.
- Skip `--config` as a visual override while retaining `startupConfigPath` as the reload source.
- On success, apply safe live visual updates (colors, preset definitions/order/initial, phase-bar layout values that remain usable on the live window, help-overlay settings, hide-cursor preference, and successfully reopened fonts).
- On failure (missing/invalid/unparsable/unusable layout/font open failure): keep the complete previous working Config and fonts; print a nonfatal diagnostic; leave the app running.

### Strict startup is preserved

- Invalid explicitly requested startup config still exits nonzero.
- Invalid values in auto-discovered startup config still fail fatally via the same shared validators (process exit).
- Runtime reload uses the **same** validation implementation with a recoverable failure policy (throw + rollback) so invalid existing files cannot terminate the process via `std::exit`.

### Window settings (launch-time)

R does **not**:

- Restart the process
- Restart Ableton Link
- Recreate, resize, or toggle the SDL window
- Apply candidate `width`, `height`, or `fullscreen` live

If the candidate differs in width/height/fullscreen from the live window, the live values are kept and a restart-required/deferred warning is printed. Other valid visual settings from the same candidate may still commit. After deferral, layout constraints such as `phase_bar_margin` are validated against the **live** width; a larger unapplied candidate width cannot make an unusable margin appear valid.

### Font reload (transactional)

1. Parse and validate the candidate configuration (Config-level).
2. Attempt to open all four required fonts (status, tempo, bottom, help) into temporary `TTF_Font` objects.
3. Only if **all** opens succeed: swap them into use, close the old fonts, keep the new Config.
4. If any font fails: close every temporary font opened during the attempt, restore the previous Config, keep existing fonts, report nonfatal error.

### Missing / invalid config at reload

- Explicit or auto-discovered path missing at reload time → nonfatal; keep previous state.
- Existing file with invalid numeric/Boolean/RGBA/margin/preset data → nonfatal; keep previous state.
- This differs from startup, where invalid explicit config (and invalid content through shared fatal validators) still fails the process.

## P Key — Color Preset Cycling (Implemented, Ticket 2)

- Only colors change.
- Fonts, layout dimensions, and window mode are not affected by P.
- No configuration files are written.
- Built-ins: `default` (label-only base restore) and `high_contrast`.
- File `color_presets=` list replaces built-ins; omitted list keeps built-ins; empty list disables presets (P no-op).
- P continues to work after a successful R.

### Config syntax

```ini
color_presets=default,high_contrast,warm_dim
color_preset=default

color_preset.default.name=Default
color_preset.default.tempo_color=64,79,96,255
# ... other color_preset.<id>.<color_key> entries ...
```

## Help Overlay

Help includes:

```
R Reload config
P Color preset
```

Existing controls (`F1`, `F`, `Q`/`Esc`) remain unchanged.

## `--print-config`

Includes effective values plus:

```
color_presets=...
active_color_preset=...
```

## Implementation order

1. Ticket 2 color presets / P — done on accepted integration baseline.
2. Ticket 3 runtime config reload / R — implemented on this feature branch; Pi validation and acceptance pending.
3. Ticket 4 screen preset configs/docs — not started.

## Status

Do not treat this document as proof of Pi validation, merge, release, or final `v0.6-pi-validated` acceptance. See ROADMAP and the continuation brief for process state.
