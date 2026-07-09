# Test Plan (v0.4)

**v0.4 is Pi-validated at commit a79db44 (tag v0.4-pi-validated).**

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
# Test with example config (no active font_path)
./build/link-pi-display --config config/link-pi-display.example.conf --windowed

# Test missing config (should use built-in defaults)
./build/link-pi-display --windowed

# Test with a copied editable config
cp config/link-pi-display.example.conf config/link-pi-display.conf
# Edit one band color, then run:
./build/link-pi-display --config config/link-pi-display.conf --windowed
```