# Raspi-LINK-Display

Ableton Link Pi Display Foundation — v0.2 skeleton (display-only).

See Ableton Link Pi Display Foundation — Continuation Brief v0.2.md for the authoritative specification.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Run

```bash
./build/ableton-link-pi-display          # GUI (if SDL available)
./build/ableton-link-pi-display --no-gui # console mode
```

## Options

`--windowed`, `--width`, `--height`, `--font`, `--tempo`, `--quantum`, `--no-gui`.
