# Ableton Link Pi Display

A minimal C++17 Raspberry Pi display foundation for monitoring Ableton Link session data.

## Features (v0.4)

- Real-time Link tempo, beat, and phase display
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

## Documentation

- [UI Specification](docs/UI-SPEC.md)
- [Test Plan](docs/TEST-PLAN.md)
- Example config: `config/link-pi-display.example.conf`

## License

MIT License — see [LICENSE](LICENSE) file.
