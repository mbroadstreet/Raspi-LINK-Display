# UI/Display Contract (v0.2 baseline)

This document describes the **display/GUI contract** separately from the runtime/API contract.

## Separation of Concerns

- **Core/Runtime Contract** (unchanged by GUI work):
  - Ableton Link integration via `LinkEngine`
  - `LinkDisplayState` snapshot (tempo, beat, phase, remotePeers, linkEnabled)
  - Peer count = remote peers only (`link.numPeers()`)
  - Tempo shown with exactly two decimal places
  - Beat/phase calculations use default quantum = 4.0
  - Manual start only — no systemd service

- **Display/GUI Contract** (this document):
  - Screen layout and visual presentation only
  - Text formatting and rendering
  - Band sizing and positioning
  - Colors, shapes, and visual elements
  - SDL2_ttf usage

## Current v0.2 Display Layout (480×320)

Three horizontal bands:

### Top band (status)
- Height: ~58 px
- Background: dark gray (#121212)
- Text: status line
  - `LINK Inactive`
  - `LINK Active · No Peers`
  - `LINK Network Devices: N`
- Color: soft white (#D2D2D2)
- Font: 26 px

### Center band (tempo)
- Height: remaining space minus top/bottom
- Background: black
- Large centered tempo value (exactly two decimals, no "BPM" suffix)
- Color: white (#F5F5F5)
- Font: 122 px

### Bottom band (beat + phase)
- Height: ~58 px
- Background: dark gray (#121212)
- Left: beat text (`Beat X.X`)
- Right: phase visualization (current v0.2 uses text `Phase Y.Y / 4`)
- Color: soft white (#D2D2D2)
- Font: 26 px

## Rendering Rules

- All text is centered within its band.
- No font files are bundled.
- Fullscreen uses `SDL_WINDOW_FULLSCREEN_DESKTOP`.
- Explicit ~30 FPS frame pacing (no vsync + delay combination).
- Quit via window close, Escape, or `q`.

## Scope for Future GUI Work

GUI-only changes are restricted to:
- `DisplayText.*`
- `SdlRenderer.*`
- Rendering helpers
- `docs/UI-SPEC.md`
- `README.md` (display-related sections)
- `docs/TEST-PLAN.md` (UI test sections)

Runtime behavior, LinkEngine, peer-count semantics, and tempo semantics must remain untouched unless explicitly authorized.