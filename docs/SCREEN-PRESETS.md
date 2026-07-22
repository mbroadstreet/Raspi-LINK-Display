# Screen Preset Configurations

Ticket 4 supplies three ordinary, sparse configuration files under `config/presets/`. Each is selected at launch through the existing `--config` option; there is no screen-preset parser, include mechanism, selector, or runtime screen switch.

## Supplied files and explicit screen contracts

| File | Size/mode | Fonts: status/tempo/bottom/help | Phase: height/gap/margin | Initial color preset |
|---|---|---|---|---|
| `480x320-landscape.conf` | 480×320 fullscreen | 30/110/25/20 | 22/6/24 | built-in `default` (implicit) |
| `480x320-high-contrast.conf` | 480×320 fullscreen | 30/110/25/20 | 22/6/24 | built-in `high_contrast` (explicit) |
| `320x240-landscape.conf` | 320×240 fullscreen | 22/80/18/16 | 16/4/16 | built-in `default` (implicit) |

Each file keeps those window, font-size, and phase-layout values active because they define the named screen profile. Unrelated settings—including band/help colors, help timeout, cursor preference, tempo, quantum, and `font_path`—are omitted and inherited from compiled defaults.

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

## Sparse file-only palette

Starting without a config retains the six dim compiled colors:

| Color | No-config compiled value | File-loaded default value |
|---|---|---|
| Inactive status | `40,44,48,255` | `100,44,48,255` |
| No-peers status | `40,44,48,255` | `40,54,88,255` |
| Connected status | `75,85,95,255` | `105,105,95,255` |
| Tempo | `64,79,96,255` | `84,89,166,255` |
| Phase bar | `83,114,151,255` | `73,164,121,255` |
| Phase marker | `255,255,255,255` | `125,205,25,255` |

All three files actively set exactly the file-loaded values above. They omit `color_presets=` and all dot definitions, so the built-in order `default,high_contrast`, labels, label-only default semantics, and unchanged high-contrast overrides remain available.

The high-contrast launch file carries the same six base overrides before selecting `color_preset=high_contrast`. Pressing P therefore returns to built-in `default` and reveals the same file-loaded palette as the other two files; another P wraps to unchanged built-in high contrast.

## Runtime controls and precedence

- **P** cycles the inherited built-in `default` and `high_contrast` presets and wraps in built-in order. Label-only `default` restores the selected file's six-color base palette.
- **R** reloads the same startup config path and rebuilds compiled defaults → sparse file overrides → initial preset → original CLI overrides. Complete profile state must remain identical after an unchanged-file reload.
- Command-line options override file values at startup and remain authoritative across R reloads.
- To select another screen configuration or apply changed `width`, `height`, or `fullscreen`, restart with the desired `--config` path.

An active `color_presets=` in a custom file replaces the built-in list and requires a definition for every listed ID. Omitting it, as these files do, retains the built-ins.

## Font portability

The supplied files omit `font_path`. The application may discover an installed system font, or the operator may provide a host-specific path with `--font PATH`. Font specification work is reserved for Ticket 5; no font feature is part of this follow-up.

## Candidate status

This sparse-palette follow-up is an unaccepted Ticket 4 replacement candidate. The first candidate passed owner Pi checks, but replacement-archive review and targeted Pi validation of no-config/file-default/high-contrast/P/R behavior remain required. This document does not claim a merge, push, tag, final v0.6 release, or Ticket 4 acceptance.
