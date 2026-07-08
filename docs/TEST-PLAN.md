# Test Plan (v0.3.6)

## Build & Basic Runtime

```bash
cmake -S . -B build
cmake --build build -j"$(nproc)"
```

## Core Smoke Tests

```bash
./build/link-pi-display --help
timeout 10 ./build/link-pi-display --no-gui
./build/link-pi-display --windowed
./build/link-pi-display
```

## Configuration Tests

```bash
# Test with example config
./build/link-pi-display --config config/link-pi-display.example.conf --windowed

# Test missing config (should use defaults)
./build/link-pi-display --windowed
```

## F1 Help Overlay (GUI only)

- Press **F1** → help overlay appears
- Wait ~8 seconds → overlay disappears automatically
- Press **Q** or **Esc** → application quits
- Press **F** → toggles fullscreen/windowed (if supported)

## Visual Acceptance (v0.3.6)

- Top status line uses configured color and size
- Tempo remains two decimals, no BPM suffix
- Bottom phase meter remains phase-only
- No beat count or numeric phase text appears
- Help overlay is readable and times out correctly

## Remote Peer Test

- No peers → `LINK Active · No Peers`
- One remote peer → `LINK Network Devices: 1`

## Invalid Config Handling (if implemented)

- Invalid color or number should produce a clear warning/error mentioning the key.