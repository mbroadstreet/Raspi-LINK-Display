# Runtime Visual Config

This document describes the runtime visual configuration system for Raspi-LINK-Display.

Status (v0.6 path):

- **Color presets / P key (Ticket 2):** implemented and merged on the accepted integration baseline.
- **Runtime config reload / R key (Ticket 3):** published, technically complete, and owner/Pi-validated at accepted commit `334116f`.
- **Screen preset configs / F1 alignment (Ticket 4):** the runtime/config/test implementation at `ace2a7f` on `v0.6-screen-preset-configs` passed the complete owner-reported Raspberry Pi validation. This documentation records that result; branch publication, merge, tag, and release status must be determined from repository refs and release records. Do not confuse launch-time screen-file selection with R reload.

## Conceptual Hierarchy

- **Screen preset** — An ordinary sparse configuration file selected at launch time (Ticket 4). It explicitly defines its screen contract and file-only palette while inheriting unrelated compiled defaults and built-in color presets.
- **Color preset** — A built-in or file-defined named collection of color values. A color preset overrides only color-related settings. Cycled with **P**.
- **R key** — Reloads the original startup config source at runtime (visual settings).
- **P key** — Cycles through named color presets in the currently loaded configuration.

The design separates launch-time decisions (screen presets / window creation) from runtime adjustments (color preset cycling and config reload).

## R Key — Config Reload (Ticket 3 accepted and Pi-validated)

Pressing `R` in GUI mode reloads the configuration source while the application is running.

### Reload source rules

- Explicit startup `--config PATH`: every R reloads that same PATH.
- Auto-discovered `config/link-pi-display.conf` loaded at startup: every R reloads that same discovered path.
- No config file loaded at startup: R rebuilds built-in defaults and re-applies original CLI overrides. It does not discover a new file later.
- Reload never writes a config file. A preset reached only with P is runtime state and is not written to the file or persisted elsewhere.

### Behavior

- Rebuild/validate a complete **candidate** without modifying active state until validation succeeds.
- Re-apply original CLI overrides on every successful reload (including repeated reloads):
  `--windowed`, `--no-gui`, `--width`, `--height`, `--font`, `--tempo`, `--quantum`.
- Skip `--config` as a visual override while retaining `startupConfigPath` as the reload source.
- On success, apply safe live visual updates (colors, preset definitions/order/initial, phase-bar layout values that remain usable on the live window, help-overlay settings, hide-cursor preference, and successfully reopened fonts).
- A successful R reconstructs preset selection using these rules:
  1. Select a valid explicit `color_preset=<id>` from the effective configuration when one is configured.
  2. Otherwise, select the first ID in the effective preset list when that list is non-empty.
  3. With unchanged built-ins, the fallback is `default` because the list order is `default,high_contrast`.
  4. If `color_presets=` is empty, there is no active preset and P is a no-op.
- A mid-session selection reached only with P is not preserved by a successful R. For example, starting at built-in `default`, pressing P to reach `high_contrast`, and then successfully pressing R returns to `default` unless the startup configuration explicitly selects another valid initial ID.
- `config/presets/480x320-high-contrast.conf` explicitly selects `color_preset=high_contrast`, so its successful R returns to `high_contrast`, not `default`.
- On failure (missing/invalid/unparsable/unusable layout/font open failure): transactionally keep the complete previous working Config, fonts, and active preset; print a nonfatal diagnostic; leave the app running.

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
- No configuration files are written, and a runtime preset selection reached with P is not persisted elsewhere.
- Built-ins: `default` (label-only base restore) and `high_contrast`.
- File `color_presets=` list replaces built-ins; omitted list keeps built-ins; empty list disables presets (P no-op).
- With at least two effective preset IDs, P continues to cycle after a successful R. A one-ID list remains on that preset, while an empty list has no active preset; P cannot advance either case.

### Base / Default Colors vs presets

- **Compiled defaults** are always the first layer. Starting with no config keeps the diagnostic dim palette: inactive `40,44,48,255`; no-peers `40,44,48,255`; connected `75,85,95,255`; tempo `64,79,96,255`; phase bar `83,114,151,255`; marker `255,255,255,255`.
- **Sparse file overrides** replace only keys actively assigned by the selected file. The main example and supplied screen files override the same six colors with inactive `100,44,48,255`; no-peers `40,54,88,255`; connected `105,105,95,255`; tempo `84,89,166,255`; phase bar `73,164,121,255`; marker `125,205,25,255`.
- **Base / Default Colors** are captured from the effective top-level colors after compiled defaults, sparse file overrides, original CLI replay, and supported alias resolution, but before applying the selected initial preset. Current CLI options do not directly set colors. Commented or omitted file keys inherit compiled values; uncommenting a key creates an override. `--print-config` displays the final effective configuration.
- A **label-only** preset (for example `color_preset.default.name=Default` with **no** `color_preset.default.*_color` keys) does not force color overrides. Activating it restores the captured base colors.
- A **partial** preset may define only some `color_preset.<id>.*_color` keys. Unspecified colors **inherit from the base layer**, not from the previously active preset.
- Repeating every base color under `color_preset.default.*` is redundant when `default` is intended as base restore; keep `.name` only unless you deliberately want `default` to force overrides.

The construction order at startup and during a successful R is: compiled defaults and built-in presets → sparse config-file overrides → original CLI overrides/replay and supported alias resolution → capture effective top-level base colors → select and apply a valid configured initial preset, or the first effective preset fallback. Current CLI options do not directly set colors, but CLI processing still occurs before base-color capture and initial-preset application. Existing live/deferred window semantics remain unchanged.

### File-defined preset list behavior

| `color_presets=` in file | Effect |
|---|---|
| Present with one or more IDs | Replaces the built-in preset list with that order |
| Omitted entirely | Built-in presets remain available |
| Present but empty (`color_presets=`) | Presets disabled; P is a no-op |

`color_preset=<id>` selects the initial active preset at startup and after a **successful R** when the ID is valid. Without an explicit initial ID, startup and successful R select the first effective preset; with an empty effective list there is no active preset. A failed R instead retains the complete prior working state, including its active preset.

### Config syntax

```ini
# Sparse file-only base override
tempo_color=84,89,166,255

# Omit color_presets= to retain built-in default,high_contrast.
# Select a built-in initial preset only when needed:
# color_preset=high_contrast

# Optional built-in customization:
# color_preset.high_contrast.tempo_color=255,240,0,255
```

An active `color_presets=` replaces the built-in list and requires a definition for every listed ID. See `config/link-pi-display.example.conf` for commented replacement syntax and compiled-default references.

## Help Overlay

The Ticket 4 implementation keeps the panel centered but left-aligns the title and renders keys/actions as separate left-aligned columns:

```
F1 Help
Q / Esc    Quit
F          Toggle Fullscreen
P          Color preset
R          Reload config
```

The spacing above is illustrative only; the renderer does not align columns with embedded spaces. Existing controls, overlay timeout, colors, and alpha blending remain unchanged. The complete owner-reported Pi validation at `ace2a7f` included readable, unclipped 480×320 and 320×240 output.

## Ticket 4 standalone screen configurations

The Pi-validated Ticket 4 implementation adds three normal config files selected with the existing `--config` option:

- `config/presets/480x320-landscape.conf`
- `config/presets/480x320-high-contrast.conf`
- `config/presets/320x240-landscape.conf`

They keep explicit screen-contract values (window size/mode, four font sizes, and phase geometry), apply the six-color file palette, and inherit unrelated defaults plus the built-in preset list/labels/high-contrast overrides. They omit `tempo`, `quantum`, and machine-specific `font_path`. Only the high-contrast launch file actively selects `color_preset=high_contrast`. P cycles colors; R reloads the same startup path; CLI overrides remain authoritative. Selecting another screen file or applying window dimensions requires restart. See `docs/SCREEN-PRESETS.md`.

## `--print-config`

Includes effective values plus:

```
color_presets=...
active_color_preset=...
```

## Implementation order

1. Ticket 2 color presets / P — done on accepted integration baseline.
2. Ticket 3 runtime config reload / R — published, technically complete, and Pi-validated at `334116f`.
3. Ticket 4 screen configs and left-aligned F1 information — implementation checkpoint `ace2a7f` passed complete owner/Pi validation. Determine publication, merge, tag, and release status from repository refs and release records.

## Status

Ticket 3's Pi-validation statement is inherited from its accepted checkpoint. The owner reports that the complete targeted Raspberry Pi matrix passed for the Ticket 4 runtime/config/test implementation at `ace2a7f`. This documentation-only closeout records inherited validation results and does not represent a new runtime or Raspberry Pi test run. Branch publication, merge, tag, and release status must be determined from repository refs and release records. See ROADMAP and TEST-PLAN.
