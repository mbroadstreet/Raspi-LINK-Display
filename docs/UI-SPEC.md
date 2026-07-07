# UI/Display Contract (v0.3.4 baseline)

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

## Current v0.3.4 Display Layout (480×320)

Three horizontal bands with improved visual balance:

### Top band (status)
- Enlarged centered Link status line
- Slightly lower vertical position than v0.3.3
- Status strings:
  - `LINK Inactive`
  - `LINK Active · No Peers`
  - `LINK Network Devices: N`
- Peer count remains remote-peers-only

### Center band (tempo)
- Large tempo with side padding
- Exactly two decimal places
- No BPM suffix
- Remains the dominant visual element

### Bottom band (phase meter)
- Phase-only 4-segment blue phase bar
- No beat count
- No numeric phase text
- No bullet separator
- Slightly narrower than v0.3.3 version
- Visible white phase position marker
- Marker moves smoothly through the 4-beat cycle

## Future Visual/Theming Tickets (Documented but Not Implemented)

The following areas are expected to be explored in future tickets. They are **not** part of the current v0.3.4 implementation:

- Warm grey tempo color
- Top-line color choices
- Adjusted lower phase meter shade
- Alternate font exploration
- Possible renderer cleanup to centralize theme values (colors, font sizes, padding, bar dimensions)

These items should be treated as future work and should not be implemented unless a later ticket explicitly authorizes them.

## Scope for GUI Work

GUI-only changes are restricted to:
- `DisplayText.*`
- `SdlRenderer.*`
- Rendering helpers
- `docs/UI-SPEC.md`
- `README.md` (display-related sections)
- `docs/TEST-PLAN.md` (UI test sections)

Runtime behavior, LinkEngine, peer-count semantics, and tempo semantics must remain untouched unless explicitly authorized.