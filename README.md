# Ableton Link Pi Display

A minimal C++17 Raspberry Pi display foundation for monitoring Ableton Link session data.

## v0.4 Status

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

## Quick Start

```bash
./build/link-pi-display
```

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

## Controls (GUI mode)

- `F1` — Show help overlay (auto-dismisses after ~8 seconds)
- `Q` or `Esc` — Quit
- `F` — Toggle fullscreen / windowed
- `P` — Cycle the loaded config's color presets
- `R` — Reload the original startup config path

## Configuration

Copy the example config:

```bash
cp config/link-pi-display.example.conf config/link-pi-display.conf
```

Then edit `config/link-pi-display.conf`.

The application will use built-in defaults if no config file is present.

### v0.4 Band Colors

- `top_band_color` — Background color of the top status band (default: black)
- `center_band_color` — Background color of the center tempo band (default: dark grey)
- `bottom_band_color` — Background color of the bottom phase meter band (default: black)

These defaults are intentional for better readability on small Raspberry Pi TFT displays.

### Deprecated Aliases

The older keys `background_color` and `band_color` are still accepted as compatibility aliases but are deprecated. Use the explicit `top_band_color`, `center_band_color`, and `bottom_band_color` keys going forward.

CLI options always override values from the config file.

## Inspection Commands (v0.5)

- `--print-config` — Print the effective configuration (after defaults, config file, and CLI overrides) and exit.
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
- [Screen Preset Configurations](docs/SCREEN-PRESETS.md)
- Example config: `config/link-pi-display.example.conf`

## License

MIT License — see [LICENSE](LICENSE) file.

## Runtime Configuration (v0.6)

- **P** — cycle named color presets (Ticket 2, accepted integration behavior). Only colors change; layout, fonts, and Link state are unaffected.
- **R** — reload the original startup visual-config source (Ticket 3, published and Pi-validated). A successful reload reconstructs the configured initial preset (or the first effective preset fallback), rather than persisting a preset reached only with P. A missing/invalid reload is nonfatal and keeps the previous working configuration, fonts, and active preset. Window width/height/fullscreen are not applied live; restart is required.
- **Screen preset configs** — Ticket 4 files under `config/presets/`, selected through the existing `--config` option. The implementation checkpoint `ace2a7f`—including sparse palettes and left-aligned F1 help information—passed the complete owner-reported Raspberry Pi validation. This documentation records that result; branch publication, merge, tag, and release status must be determined from repository refs and release records.

Details: `docs/RUNTIME-CONFIG.md`, `docs/SCREEN-PRESETS.md`, `docs/TEST-PLAN.md`.
