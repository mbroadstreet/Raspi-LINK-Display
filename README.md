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

## Controls (GUI mode)

- `F1` — Show help overlay (auto-dismisses after ~8 seconds)
- `Q` or `Esc` — Quit
- `F` — Toggle fullscreen / windowed

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
n## Inspection Commands (v0.5)

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
- Example config: `config/link-pi-display.example.conf`

## License

MIT License — see [LICENSE](LICENSE) file.

## Planned Runtime Configuration (v0.6+)

The following capabilities are planned for future releases:

- `R` key — reload the current config source at runtime (visual settings only in first implementation)
- `P` key — cycle through named color presets defined in the loaded config
- Screen preset configs — separate launch-time config files for different displays (see `config/presets/` in the future)

For details see:
- `docs/RUNTIME-CONFIG.md`
- `docs/SCREEN-PRESETS.md`

These features are not yet implemented.

## v0.6 Color Presets (P key)

Press **P** to cycle through named color presets defined in the config using dot-prefixed syntax (see config/link-pi-display.example.conf for examples).

Only colors change. Layout, fonts, and Link state are unaffected.
