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
- On success, apply safe live visual updates (colors, preset definitions/order/initial, phase-bar layout values that remain usable on the live window, help-overlay settings, hide-cursor preference, and successfully reopened fonts). Successful R resets the active preset from the file’s `color_preset=<id>` (or first listed / built-in default rules), not from an in-session P position that is not the file start preset.
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

Manual **F** fullscreen toggle updates the tracked live `fullscreen` value only after a successful `SDL_SetWindowFullscreen` call. A later **R** therefore preserves the actual on-screen fullscreen/windowed selection (it must not recreate or re-toggle the SDL window). Original CLI args such as `--windowed` remain stored and may appear in a deferred-difference warning if they disagree with the current live state after F.

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

### Base / Default Colors vs presets

- **Base / Default Colors** are the top-level color keys in the config (or built-in defaults when no file supplies them). They are the required base layer for normal appearance.
- A **label-only** preset (for example `color_preset.default.name=Default` with **no** `color_preset.default.*_color` keys) does not force color overrides. Activating it restores the captured base colors.
- A **partial** preset may define only some `color_preset.<id>.*_color` keys. Unspecified colors **inherit from the base layer**, not from the previously active preset.
- Repeating every base color under `color_preset.default.*` is redundant when `default` is intended as base restore; keep `.name` only unless you deliberately want `default` to force overrides.

### File-defined preset list behavior

| `color_presets=` in file | Effect |
|---|---|
| Present with one or more IDs | Replaces the built-in preset list with that order |
| Omitted entirely | Built-in presets remain available |
| Present but empty (`color_presets=`) | Presets disabled; P is a no-op |

`color_preset=<id>` selects the initial active preset at startup and after a **successful R** (reload re-applies the file’s initial `color_preset` and does not keep a mid-session P selection unless that id is still the file’s start preset).

### Config syntax

```ini
# Base / Default color
tempo_color=64,79,96,255

color_presets=default,high_contrast
color_preset=default

# Label-only default: restores Base / Default Colors
color_preset.default.name=Default

# Alternate preset overrides only what differs
color_preset.high_contrast.name=High Contrast
color_preset.high_contrast.tempo_color=255,255,255,255
```

See `config/link-pi-display.example.conf` for a Pi-terminal-oriented layout: short R/P guidance and Base colors near the top, `high_contrast` overrides later.

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
