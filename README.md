# Ableton Link Pi Display

## Controlled PiTouch-bound source integration

This Windows integration retains the **exact proof-v2 runtime helper** exercised by the accepted ordinary C/D session, with native ELF SHA-256 `b8d48bc0a4609fa0ca77313094ccd3e45d1f661faefecc5edc8c3550490bc048`. It is not an installation, promotion or whole-release acceptance. The as-built source set, later integrated source and runtime helper are distinct identities.

PiTouch is the provisional first deployment target. The proof profile deliberately retains its fixed existing native build/provenance layout; do not redirect launchers, overwrite the as-built source or infer general relocation support. See the current addendum in [Desktop Launch](docs/DESKTOP-LAUNCH.md). Earlier candidate/native-blocker paragraphs below are preserved history, not a reversal of accepted C/D evidence. CLI restart, the exceptional recovery gates and final release approval remain separate. **Do not push, merge or tag.**

A minimal C++17 Raspberry Pi display foundation for monitoring Ableton Link session data.

## Unified-launch/native-state candidate — not deployed

This next-round candidate adds read-only `observed_liveness` diagnostics, a source-private cooperative runtime lease, and staged per-user desktop-entry tooling. It preserves the accepted palette/fonts and quiet/default + explicit `--verbose` behavior. **Pi build/GUI/lifecycle/performance acceptance is currently blocked; native Wayland is unverified and remains refused.** Local fixtures and alternate-Linux admission checks do not establish Pi application acceptance. The accepted installed helper/binary/settings have not been replaced. CMake/module metadata remains `0.4.0`; this is not a release.

Use [Desktop Launch](docs/DESKTOP-LAUNCH.md#next-round-candidate-boundaries-and-diagnostics) for historical phase versus observed state, the unmanaged/direct-runtime boundary, staged desktop generation/recovery and legacy-service distinction. Do not deploy source examples against an Owner's default unit as a candidate test. **Do not push, merge or tag without applicable sign-off.**

## Current work candidate

`v0.7-defaults-fonts-desktop-launch` adds the example palette as real compiled defaults and optional per-role font paths, with local/SSH launch documentation. It is a work candidate, not a new validated release. Build, font-transaction, GUI and installed-launch evidence must be recorded separately; the historical results below do not validate these changes. CMake/module metadata remains `0.4`.

## v0.4 Status (historical release)

**Pi-validated release.**

- Validated commit: `a79db44`
- Validated tag: `v0.4-pi-validated`
- Validated branch: `v0.4-pi-validated`
- Original development branch: `v0.4-config-runtime-visual-polish`
- Commit message: `v0.4: Require whole-string numeric parsing`

**Note:** The name `v0.4-pi-validated` exists as both a branch and a tag. When ambiguity matters, use the exact commit `a79db44` or fully qualified refs (e.g. `refs/tags/v0.4-pi-validated`).

## Features (v0.4)

- Real-time Link tempo, peer count, and phase display
- Remote peer count (does not count the display itself)
- F1 help overlay (GUI mode)
- User-editable configuration file for colors, fonts, and layout
- Configurable top/center/bottom band background colors
- Optional mouse cursor hiding in fullscreen
- Manual-start operation

## Quick Start (Pi desktop terminal)

From an already built source tree, as the desktop user:

```bash
PROJECT=/absolute/path/to/Raspi-LINK-Display
cd "$PROJECT"
./build/link-pi-display
```

Without `--config`, the application checks **CWD-relative** `config/link-pi-display.conf`; only when that file is absent does it use no-file compiled defaults. The main example and normal 480×320 profile explicitly assign the same six base colors as the new defaults. Custom file values still override them.

Windowed mode:

```bash
./build/link-pi-display --windowed
```

Using a custom config:

```bash
./build/link-pi-display --config config/link-pi-display.example.conf
```

Ticket 4 screen configurations (owner/Pi-validated implementation at `ace2a7f`):

```bash
./build/link-pi-display --config config/presets/480x320-landscape.conf
./build/link-pi-display --config config/presets/480x320-high-contrast.conf
./build/link-pi-display --config config/presets/320x240-landscape.conf
```

For a managed launch that survives terminal/SSH disconnect while the existing Pi-local X11 desktop stays active:

```bash
python3 tools/link-displayctl doctor
python3 tools/link-displayctl start
python3 tools/link-displayctl status
python3 tools/link-displayctl stop
```

The helper always passes `--config` (normally the 480×320 landscape profile); it is **not** a no-file launch. Saved selections can affect later launches. Full local/source/installed SSH commands, logs, restart, settings-reset and recovery instructions have one home: [Desktop Launch](docs/DESKTOP-LAUNCH.md). Stable installation is a separate reviewed step, not implied by these commands.

Successful managed `start` and `restart` are silent by default, including an already-running identical start and restart's internal stop. For QA, use `python3 tools/link-displayctl start --verbose` or `python3 tools/link-displayctl --verbose restart` to retain success explanations and status JSON. The flag is output-only, accepted before/after these two actions, and rejected with other actions. Errors remain actionable on stderr with nonzero exits; explicit `status`, `doctor`, `logs`, and standalone `stop` keep their output. Silence does not bypass lifecycle/ownership/window checks or prove visual correctness.

## Controls (GUI mode)

- `F1` — Show help overlay (auto-dismisses after ~8 seconds)
- `Q` or `Esc` — Quit
- `F` — Toggle fullscreen / windowed
- `P` — Cycle the loaded config's color presets
- `R` — Reload the original startup source + CLI (or rebuild defaults + CLI if no file was loaded)

The SDL window must have keyboard focus. Foreground output stays in the launching terminal; Ctrl-C stops that foreground process. Typing in an SSH terminal is not SDL key input.

## Configuration

Copy the example config:

```bash
cp -i config/link-pi-display.example.conf config/link-pi-display.conf
```

Then edit `config/link-pi-display.conf`.

This optional copy enables implicit file lookup when launched from the project root; do not overwrite an existing operator file. No `--config` means implicit lookup, not necessarily no loaded file. An explicitly selected missing/invalid config is fatal at startup. R keeps the original startup source; a no-file run does not discover a file created later.

Optional file keys `status_font_path`, `tempo_font_path`, `bottom_font_path`, and `help_font_path` select role fonts. For every role: global `--font PATH` → role key → general `font_path` → existing discovery. Paths are literal, CWD-relative when not absolute; explicit empty/space-tab-only values are invalid even if another override would mask them. See [Runtime Configuration](docs/RUNTIME-CONFIG.md#font-paths-and-portability) for reload, ownership and portability limits.

### v0.4 Band Colors

- `top_band_color` — Background color of the top status band (default: black)
- `center_band_color` — Background color of the center tempo band (default: dark grey)
- `bottom_band_color` — Background color of the bottom phase meter band (default: black)

These defaults are intentional for better readability on small Raspberry Pi TFT displays.

### Deprecated Aliases

The older keys `background_color` and `band_color` are still accepted as compatibility aliases but are deprecated. Use the explicit `top_band_color`, `center_band_color`, and `bottom_band_color` keys going forward.

CLI options always override values from the config file.

## Inspection Commands (v0.5)

- `--print-config` — Print effective configuration, including general and four resolved role font paths, and exit before SDL/Link. It does not prove fonts can open or render glyphs.
- `--module-info` — Print static module metadata and exit.

Example:

```bash
./build/link-pi-display --print-config
./build/link-pi-display --module-info
./build/link-pi-display --config config/link-pi-display.example.conf --windowed --print-config
```

Config parser tests:

```bash
cmake --build build --target config_parser_tests -j"$(nproc)"
./build/config_parser_tests
```

## Documentation

- [UI Specification](docs/UI-SPEC.md)
- [Test Plan](docs/TEST-PLAN.md)
- [Changelog](CHANGELOG.md)
- [Roadmap](ROADMAP.md)
- [Module Contract](docs/MODULE-CONTRACT.md)
- [Integration Notes](docs/INTEGRATION-NOTES.md)
- [Runtime Configuration](docs/RUNTIME-CONFIG.md)
- [Screen Preset Configurations](docs/SCREEN-PRESETS.md)
- [SSH / Existing-Desktop Launcher](docs/DESKTOP-LAUNCH.md)
- [Accepted PiTouch canonical release 0.4.0-desktop.1](docs/CANONICAL-RELEASE-0.4.0-desktop.1.md)
- Example config: `config/link-pi-display.example.conf`

## License

MIT License — see [LICENSE](LICENSE) file.

## Runtime Configuration (v0.6)

- **P** — cycle named color presets (Ticket 2, accepted integration behavior). Only colors change; layout, fonts, and Link state are unaffected.
- **R** — reload the original startup visual-config source (Ticket 3, published and Pi-validated). A successful reload reconstructs the configured initial preset (or the first effective preset fallback), rather than persisting a preset reached only with P. A missing/invalid reload is nonfatal and keeps the previous working configuration, fonts, and active preset. Window width/height/fullscreen are not applied live; restart is required.
- **Screen preset configs** — Ticket 4 files under `config/presets/`, selected through the existing `--config` option. The implementation checkpoint `ace2a7f`—including sparse palettes and left-aligned F1 help information—passed the complete owner-reported Raspberry Pi validation. This documentation records that result; branch publication, merge, tag, and release status must be determined from repository refs and release records.

Details: `docs/RUNTIME-CONFIG.md`, `docs/SCREEN-PRESETS.md`, `docs/TEST-PLAN.md`.
