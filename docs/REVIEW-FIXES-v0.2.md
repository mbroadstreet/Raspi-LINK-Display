# v0.2 Skeleton Review Fixes

Review branch: `v0.2-skeleton-review-fixes`

Original branch/commit supplied for review:

- Branch: `v0.2-skeleton`
- Commit: `f892cd0`

## Summary

The supplied skeleton was a useful starting commit, but it did not yet satisfy the v0.2 display contract. This review pass updates the repository so the implementation, README, test plan, and runtime display behavior agree with the v0.2 continuation brief.

## Corrected items

- Replaced the class-style `DisplayText` interface with a state-based formatter namespace.
- Added `LinkDisplayState` so status, tempo, beat, phase, and quantum travel together as one display snapshot.
- Changed tempo display from `128.37 BPM` to exactly `128.37`.
- Kept peer count as a plain integer, e.g. `LINK Network Devices: 1`, not `01`.
- Added a single `LinkEngine::snapshot()` path so tempo, beat, phase, and peers are captured together during polling.
- Enabled Link in the `LinkEngine` constructor.
- Kept `SessionState` short-lived and reacquired during polling.
- Implemented actual SDL2_ttf text rendering with `TTF_RenderUTF8_Blended`.
- Implemented the three-band display: top status, large center tempo, bottom beat/phase.
- Implemented event polling and quit behavior using window close, Escape, or `q`.
- Implemented fullscreen/windowed behavior from `Config`.
- Removed renderer vsync usage and kept explicit approximately-30-FPS frame pacing in `main`.
- Added system font discovery and no-bundled-font documentation.
- Added a clearer CMake integration path for official `Ableton/link` through `AbletonLinkConfig.cmake` and `Ableton::Link`.
- Added a test plan and service template.

## Verification performed in this environment

A full CMake build could not be completed in the sandbox because the required local dependencies are not present. The configure step was intentionally run and verified to fail clearly when the official Ableton/link checkout is absent:

```text
Ableton/link was not found at .../third_party/link.
Add it with:
  git submodule add https://github.com/Ableton/link third_party/link
  git submodule update --init --recursive
```

A standalone `DisplayText` compile/test was run successfully without requiring Link or SDL2:

```text
DisplayText checks OK
```

Static contract checks:

```text
SDL_RENDERER_PRESENTVSYNC occurrences: 0
twoDigits occurrences: 0
setfill occurrences: 0
TTF_RenderUTF8_Blended occurrences: 2
BPM suffix in DisplayText.cpp: 0
```

## Remaining local verification after syncing

On the Raspberry Pi or development machine with dependencies installed:

```bash
sudo apt update
sudo apt install -y git cmake build-essential pkg-config libsdl2-dev libsdl2-ttf-dev
git submodule add https://github.com/Ableton/link third_party/link
git submodule update --init --recursive
cmake -S . -B build
cmake --build build -j"$(nproc)"
./build/link-pi-display --no-gui
./build/link-pi-display --windowed --width 480 --height 320
```
