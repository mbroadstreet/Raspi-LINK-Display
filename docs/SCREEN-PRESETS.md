# Screen Preset Configurations

Ticket 4 supplies three ordinary, standalone configuration files under `config/presets/`. Each is selected at launch through the existing `--config` option; there is no screen-preset parser, include mechanism, selector, or runtime screen switch.

## Supplied files

| File | Layout | Initial color preset |
|---|---|---|
| `480x320-landscape.conf` | 480×320 fullscreen | `default` |
| `480x320-high-contrast.conf` | 480×320 fullscreen | `high_contrast` |
| `320x240-landscape.conf` | 320×240 fullscreen | `default` |

Launch examples:

```bash
./build/link-pi-display --config config/presets/480x320-landscape.conf
./build/link-pi-display --config config/presets/480x320-high-contrast.conf
./build/link-pi-display --config config/presets/320x240-landscape.conf
```

Use `--windowed` for inspection while retaining the selected file's configured dimensions:

```bash
./build/link-pi-display --config config/presets/320x240-landscape.conf --windowed
```

## Runtime controls and precedence

- **P** cycles `default` and `high_contrast` inside the loaded file and wraps in file order. The label-only `default` restores the file's base colors.
- **R** reloads the same startup config path. It does not choose another screen file, restart Link, or recreate the SDL window.
- Command-line options override file values at startup and remain authoritative across R reloads.
- To select another screen configuration or apply changed `width`, `height`, or `fullscreen`, restart with the desired `--config` path.

The files intentionally omit `tempo` and `quantum`, so choosing a screen configuration does not alter Link/runtime values.

## Font portability

The supplied files omit `font_path`. The application may discover an installed system font, or the operator may provide a portable host-specific path with `--font PATH`. No machine-specific or bundled font path is embedded in these files.

## Candidate status

The files and left-aligned F1 help are Ticket 4 candidate work. Automated parser/build checks and Raspberry Pi GUI validation remain required before Ticket 4 acceptance. This documentation does not claim a merge, push, tag, final v0.6 release, or Pi acceptance.
