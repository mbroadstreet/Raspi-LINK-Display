# Test Plan

## Configure-time checks

Run without Ableton/link installed to confirm CMake fails clearly:

```bash
cmake -S . -B build
```

Expected result: CMake reports that `third_party/link/AbletonLinkConfig.cmake` is missing and prints the submodule commands.

## Build checks

After adding Ableton/link and installing SDL2/SDL2_ttf:

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

## Console smoke test

```bash
./build/link-pi-display --no-gui
```

Expected format:

```text
LINK Active · No Peers | 120.00 | Beat 0.0 · Phase 0.0 / 4
```

## GUI smoke test

```bash
./build/link-pi-display --windowed --width 480 --height 320
```

Confirm:

- Top band renders one status line.
- Center renders large tempo with two decimals and no `BPM` suffix.
- Bottom renders beat/phase.
- Escape or `q` exits.

## Link session test

Join from Ableton Live or another Link-capable peer and confirm:

- `LINK Active · No Peers` when alone.
- `LINK Network Devices: N` when remote peers are present.
- `N` is remote peers only.
- Tempo follows the Link session.
- Beat/phase move smoothly.
