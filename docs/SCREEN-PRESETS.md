# Screen Preset Configurations

Ticket 4 supplies three ordinary, sparse configuration files under `config/presets/`. Each is selected at launch through the existing `--config` option; there is no screen-preset parser, include mechanism, selector, or runtime screen switch.

## Supplied files and explicit screen contracts

| File | Size/mode | Fonts: status/tempo/bottom/help | Phase: height/gap/margin | Initial color preset |
|---|---|---|---|---|
| `480x320-landscape.conf` | 480×320 fullscreen | 30/110/25/20 | 22/6/24 | built-in `default` (implicit) |
| `480x320-high-contrast.conf` | 480×320 fullscreen | 30/110/25/20 | 22/6/24 | built-in `high_contrast` (explicit) |
| `320x240-landscape.conf` | 320×240 fullscreen | 22/80/18/16 | 16/4/16 | built-in `default` (implicit) |

Each file keeps those window, font-size, and phase-layout values active because they define the named screen profile. Unrelated settings—including band/help colors, help timeout, cursor preference, tempo, quantum, general and role font paths—are omitted and inherited from compiled defaults/discovery.

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

## Sparse base palette matching compiled defaults

The current candidate promotes the example palette to genuine no-file compiled defaults:

| Color | Compiled and explicit file base value |
|---|---|
| Inactive status | `100,44,48,255` |
| No-peers status | `40,54,88,255` |
| Connected status | `105,105,95,255` |
| Tempo | `84,89,166,255` |
| Phase bar | `73,164,121,255` |
| Phase marker | `125,205,25,255` |

No `--config` still checks CWD-relative `config/link-pi-display.conf`; a true no-file check uses an isolated CWD with the absolute binary. The managed helper always passes an explicit profile, normally `480x320-landscape.conf`, even though its base colors match compiled defaults.

All three files retain exactly these six active base assignments. They omit `color_presets=` and all dot definitions, so the built-in order `default,high_contrast`, labels, label-only default semantics, and unchanged high-contrast overrides remain available.

The high-contrast launch file carries the same six base overrides before selecting `color_preset=high_contrast`. Pressing P therefore returns to built-in `default` and reveals the same file-loaded palette as the other two files; another P wraps to unchanged built-in high contrast.

## Runtime controls and precedence

- **P** cycles the inherited built-in `default` and `high_contrast` presets and wraps in built-in order. Label-only `default` restores the selected file's six-color base palette.
- **R** reloads the same startup config path and reconstructs compiled defaults and built-in presets → sparse file overrides → original CLI overrides/replay and supported alias resolution → captured effective top-level base colors → configured initial preset, or the first effective preset fallback.
- Command-line options override file values at startup and remain authoritative across R reloads.
- To select another screen configuration or apply changed `width`, `height`, or `fullscreen`, restart with the desired `--config` path.

A successful R does not preserve a mid-session preset reached only with P, because that runtime selection is not written to the config file or persisted elsewhere. For the implicit-default files, default → P to high contrast → successful R returns to built-in `default`, the first effective preset. The high-contrast launch file explicitly sets `color_preset=high_contrast`, so its successful R returns to `high_contrast`, not `default`. If `color_presets=` is empty, no preset is active and P is a no-op. A failed R remains transactional and retains the complete previous working configuration, including the active preset.

An active `color_presets=` in a custom file replaces the built-in list and requires a definition for every listed ID. Omitting it, as these files do, retains the built-ins.

## Font portability

The supplied files have no active general or role font path. Commented examples are operator-provided literal paths, not bundled assets or machine-specific defaults. Global `--font PATH` overrides every role; otherwise each role inherits its file override, general `font_path`, or existing discovery. The bottom font is opened/owned but has no visible text in the phase-only band. Empty supplied paths, CWD-relative semantics, all-four R rollback and glyph/readability limits are specified in [Runtime Config](RUNTIME-CONFIG.md#font-paths-and-portability).

## Validation and closeout status

The current defaults/role-font candidate needs new execution evidence; the following is historical Ticket 4 evidence, including its then-different no-file palette. It does not certify current fonts or installed launch commands.

The owner reports that the complete targeted Raspberry Pi validation passed for the Ticket 4 runtime/config/test implementation at `ace2a7f`, including no-config/file-default/high-contrast behavior, P/R behavior, invalid-R rollback, F1 alignment, fullscreen/windowed and cursor behavior, and 320×240 rendering. This documentation-only closeout records inherited validation results and does not represent a new runtime or Raspberry Pi test run. Branch publication, merge, tag, and release status must be determined from repository refs and release records.
