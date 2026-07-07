# Ableton Link Pi Display

A minimal C++17 Raspberry Pi display foundation for monitoring Ableton Link session data.

This repository is the v0.2-compliant skeleton for the display-only v0.1 appliance target.

## Target

- Raspberry Pi 3 B+
- Raspberry Pi OS with GUI
- 3.5" 480x320 landscape display
- Official `Ableton/link`
- SDL2 + SDL2_ttf
- Read-only Link monitor

## Display contract

Top band:

```text
LINK Active · No Peers
```

or:

```text
LINK Network Devices: N
```

or:

```text
LINK Inactive
```

Center:

```text
128.37
```

Bottom band:

```text
Beat 241.3 · Phase 1.3 / 4
```

`N` is the remote Link peer count from `link.numPeers()`. Do not add this display to the count. Peer count is rendered as a plain integer, not zero-padded.

## Non-goals for this foundation

This skeleton intentionally excludes audio, MIDI, GPIO, Link Audio, tempo control, start/stop control, web UI, touch controls, musical key, device names, and DAW-specific metadata.

## Dependencies

```bash
sudo apt update
sudo apt install -y git cmake build-essential pkg-config libsdl2-dev libsdl2-ttf-dev
```

Add Ableton Link from the official checkout:

```bash
git submodule add https://github.com/Ableton/link third_party/link
git submodule update --init --recursive
```

The project integrates Link through:

```text
third_party/link/AbletonLinkConfig.cmake
Ableton::Link
```

## Build

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

## Run

Windowed test:

```bash
./build/link-pi-display --windowed
```

Fullscreen on the Pi display:

```bash
./build/link-pi-display
```

Console mode over SSH:

```bash
./build/link-pi-display --no-gui
```

Custom font:

```bash
./build/link-pi-display --font /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf
```

Optional configuration:

```bash
./build/link-pi-display --windowed --width 480 --height 320
./build/link-pi-display --tempo 120 --quantum 4
```

## Notes

- `SessionState` snapshots are reacquired during polling and are not stored long-term.
- Fullscreen mode uses `SDL_WINDOW_FULLSCREEN_DESKTOP`, so the app is designed for a 480x320 display but does not force a hardware mode to 480x320 in every environment.
- The renderer does not request vsync; the main loop uses explicit frame pacing for roughly 30 FPS.
- No font files are bundled. Install a system font or pass `--font PATH`.
