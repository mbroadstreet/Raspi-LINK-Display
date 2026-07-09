# Integration Notes

This document provides guidance for using Raspi-LINK-Display as a reference template and for future integration work.

## Use as a Template

When creating a new standalone module (e.g. an OSC input module or MIDI control module), consider copying and adapting:

- README structure and quick-start section
- Config loading + strict validation approach
- `docs/TEST-PLAN.md` style
- `module.json` metadata format
- Manual-start assumptions
- Clear separation of concerns (core vs display vs config)
- Explicit "non-goals" and "deferred" sections

## Future Container Integration Principles

A future container application should **only** integrate modules after they have proven they work well standalone.

Recommended high-level structure for a future container:

```
container-app/
  modules/
    raspi-link-display/
    osc-control-input/
    midi-control-input/
    dmx-control-input/
  shared/
    config/
    logging/
    event-router/
    display-manager/
```

## Normalized Event Ideas (Future)

A container may eventually route normalized events between modules. Examples of events that could be useful (not implemented here):

- `next_view`
- `previous_view`
- `show_help`
- `toggle_fullscreen`
- `set_view`
- `set_brightness`
- `quit`

These events are documented here only as a hint for future design. Do not implement them inside this module.

## Key Assumptions Modules Should Preserve

- Manual start is the primary supported mode.
- Each module owns its own config validation.
- External control should be a separate module, not bolted onto a display module.
- The display module should remain runnable and testable without the container.

## Anti-Patterns to Avoid

- Adding container-level concerns (window management, module switching) directly into this repo.
- Hard-coding assumptions about other modules being present.
- Implementing external control protocols inside a display-only module.

See also:
- `ROADMAP.md`
- `docs/MODULE-CONTRACT.md`
- `module.json`
## Module Inspection (v0.5)

Future container tooling may use `module.json` and the `--module-info` command to discover and inspect modules.

`--print-config` can be used by tooling to inspect effective configuration without starting the module runtime.
