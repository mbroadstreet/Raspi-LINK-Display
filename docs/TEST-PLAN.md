# Test Plan (v0.4)

**v0.4 is Pi-validated at commit a79db44 (tag v0.4-pi-validated).**

## Build & Basic Runtime

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

## Core Smoke Tests

```bash
./build/link-pi-display --help
timeout 10 ./build/link-pi-display --no-gui
./build/link-pi-display --windowed
./build/link-pi-display
```

## Configuration Tests

```bash
# Test with example config (no active font_path)
./build/link-pi-display --config config/link-pi-display.example.conf --windowed

# Test missing config (should use built-in defaults)
./build/link-pi-display --windowed

# Test with a copied editable config
cp config/link-pi-display.example.conf config/link-pi-display.conf
# Edit one band color, then run:
./build/link-pi-display --config config/link-pi-display.conf --windowed
```

## CLI Override Tests

```bash
# Config file says fullscreen=true, but --windowed should win
./build/link-pi-display --config config/link-pi-display.example.conf --windowed
```

## Invalid Config Handling

- Provide an invalid color component (e.g. 999) in a config file and confirm a clear error mentioning the key/value.
- Provide an invalid number or boolean and confirm useful error output.

## F1 Help Overlay (GUI only)

- Press **F1** → help overlay appears
- Wait ~8 seconds → overlay disappears automatically
- Press **Q** or **Esc** → application quits
- Press **F** → toggles fullscreen/windowed

## Cursor Hiding Behavior

- Start in fullscreen with `hide_mouse_cursor=true` → cursor should be hidden
- Press **F** to go to windowed → cursor should appear
- Press **F** again to return to fullscreen → cursor should hide again
- Set `hide_mouse_cursor=false` and confirm cursor remains visible in fullscreen

## Visual / Layout Acceptance (v0.4)

- Top status band uses `top_band_color`
- Center tempo band uses `center_band_color`
- Bottom phase band uses `bottom_band_color`
- Default v0.4 colors: black top, dark grey center, black bottom
- Top status line uses configured color and size
- Tempo remains two decimals, no BPM suffix
- Bottom phase meter remains phase-only (no beat count or numeric phase text)
- Help overlay is readable and times out correctly

## Remote Peer Test

- No peers → `LINK Active · No Peers`
- One remote peer → `LINK Network Devices: 1`
- Status colors match the displayed state:
  - `LINK Inactive` → `status_inactive_color`
  - `LINK Active · No Peers` → `status_no_peers_color`
  - `LINK Network Devices: N` → `status_connected_color`

## No-Regression Checks

- Link peer display and status text behavior unchanged from v0.3.x
- Bottom meter remains strictly phase-only
- Manual-start operation unchanged
- No changes to LinkEngine, LinkDisplayState, or Ableton link logic

## v0.5 Inspection and Test Commands

### Config Parser Tests

```bash
cmake --build build --target config_parser_tests -j"$(nproc)"
./build/config_parser_tests
```

### --module-info

```bash
./build/link-pi-display --module-info
```

Should print module metadata and exit without starting SDL or Link.

### --print-config

```bash
./build/link-pi-display --print-config
./build/link-pi-display --config config/link-pi-display.example.conf --windowed --print-config
```

Should print effective config (key=value) and exit without SDL or Link.

### Negative Validation Tests (with --print-config)

These should fail with clear error and non-zero exit, without starting SDL or Link:

```bash
./build/link-pi-display --width 480abc --print-config
./build/link-pi-display --tempo -1 --print-config
./build/link-pi-display --config /tmp/does-not-exist.conf --print-config
```

## No-Regression for v0.5

- Normal runtime behavior unchanged (GUI and --no-gui still work)
- --help includes the new options
- Config defaults unchanged
- Existing v0.4 tests still valid

## Runtime Visual Config Testing (v0.6)

### Color Preset Cycling (P key) — Ticket 2 (accepted on integration baseline)

- Built-in default/high_contrast cycle without a config file.
- File-defined `color_presets=` replaces built-ins; empty list disables P.
- Only colors change; fonts/layout/window/Link unaffected.
- Base colors restore when returning to the label-only `default` preset.

### Runtime Config Reload (R key) — Ticket 3 (published and Pi-validated)

Automated (config_parser_tests / direct g++ of Config.cpp):

- Correct fixture under `// config-defined presets still override after reload` uses escaped `\n`.
- Missing explicit reload source returns false; prior state intact; process continues.
- Existing file overwritten with invalid RGBA or invalid positive integer returns false in-process (no termination); prior Config intact.
- Invalid preset content (duplicate IDs / undefined listed preset) returns false with full rollback.
- At least two successful reloads preserve `--windowed`, `--no-gui`, `--width`, `--height`, `--font`, `--tempo`, `--quantum`.
- P after R changes active preset name to the next preset and wraps to the first.
- Ticket 2 built-in / empty-disable / base-restore behaviors still hold after reload.
- Live window width/height/fullscreen remain after reload when candidate differs; safe colors may still apply.
- `phase_bar_margin` is validated against live width after window deferral.

Manual Pi GUI matrix completed by the owner for the accepted Ticket 3 checkpoint:

- No-config `--windowed`: F1 lists P and R; P cycles; R succeeds; P still cycles; no crash/Link restart/window recreate.
- Explicit example config: R reloads same path; safe edits apply; P cycles after R; repeated R stable.
- Invalid existing config while running: nonfatal, previous appearance/fonts retained, P still works; repair file and R succeeds.
- Font path/size success and nonexistent font failure with full rollback.
- Width/height/fullscreen file changes warn/defer without window recreate; other safe visuals apply.
- CLI overrides remain authoritative across repeated R; tempo/quantum do not restart Link.
- **F then R:** start `--windowed`, press F to fullscreen, press R — window stays fullscreen (no recreate); deferred difference vs startup/`--windowed` may be reported only; F again still returns to windowed. Opposite direction from initial fullscreen when practical.

### Screen Preset Configs and F1 Alignment — Ticket 4 candidate

Automated/source checks:

- Parse each real file under `config/presets/` from the repository root.
- Assert exact dimensions, fullscreen state, font sizes, phase height/gap/margin, help timeout, and cursor preference.
- Assert preset order `default,high_contrast`, expected initial preset, exact base and high-contrast colors, and label-only default definition.
- Assert P advances and wraps, with default restoring base colors.
- Assert `startupConfigPath`/`configPath` retain the selected real file and unchanged-file R succeeds.
- Run direct `g++` parser tests, clean CMake build, CMake parser tests, inspection commands, and `--print-config` for all three files where tools are available.

Required Raspberry Pi GUI matrix before Ticket 4 acceptance:

- Launch all three files in fullscreen; also launch `320x240-landscape.conf` with `--windowed` and confirm actual 320×240 dimensions.
- Confirm the high-contrast file starts in high contrast; P cycles and wraps in every file; R reloads the same selected file.
- Confirm invalid R reload keeps the working display.
- Confirm F1 title, keys, and actions are left-aligned, use distinct columns, and remain readable/unclipped at 480×320 and 320×240 in fullscreen and windowed modes.
- Confirm F toggle and cursor behavior remain correct.
- Confirm status, centered tempo, phase rendering, Link state, and window creation are unchanged; P/R do not restart Link or recreate the SDL window.

See `docs/SCREEN-PRESETS.md`. Ticket 4 remains unaccepted until the source/build and Pi GUI gates pass.

See `docs/RUNTIME-CONFIG.md` for implemented R/P semantics versus Ticket 4.
