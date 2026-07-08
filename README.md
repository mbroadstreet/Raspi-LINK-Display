# Ableton Link Pi Display

A minimal C++17 Raspberry Pi display foundation for monitoring Ableton Link session data.

## Features (v0.3.6)

- Real-time Link tempo, beat, and phase display
- Remote peer count (does not count the display itself)
- F1 help overlay (GUI mode)
- User-editable configuration file for colors, fonts, and layout
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

Copy the example config and edit it:

```bash
cp config/link-pi-display.example.conf config/link-pi-display.conf
```

Then edit `config/link-pi-display.conf`.

The application will use built-in defaults if no config file is present.

## Documentation

- [UI Specification](docs/UI-SPEC.md)
- [Test Plan](docs/TEST-PLAN.md)
- Example config: `config/link-pi-display.example.conf`

## License

MIT License — see [LICENSE](LICENSE) file.