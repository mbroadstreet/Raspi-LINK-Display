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

## Planned Runtime Visual Config Testing (v0.6+)

The following areas are identified for future testing once the corresponding features are implemented. These tests do not exist yet.

### Color Preset Cycling (P key)

- Define multiple color presets in a config file.
- Verify `P` cycles through them in the defined order.
- Verify only colors change (fonts, layout, and window mode are unaffected).
- Verify `P` is a no-op (or reports) when no color presets are defined.

### Runtime Config Reload (R key)

- Start with a config file.
- Modify the config file on disk.
- Press `R` and verify visual updates occur without restarting the application or Ableton Link.
- Verify that width/height/fullscreen changes are not applied live (warning or deferral).
- Verify transactional font reload behavior (all fonts must succeed or none are swapped).

### Screen Preset Configs

- Launch with different `--config` files from a future `config/presets/` directory.
- Verify correct resolution and visual settings are applied at startup.

See `docs/RUNTIME-CONFIG.md` and `docs/SCREEN-PRESETS.md` for the design these tests will validate.

