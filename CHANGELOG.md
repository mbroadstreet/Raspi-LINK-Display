# Changelog

## v0.4 — Pi-validated config/runtime visual polish

Tag: v0.4-pi-validated
Commit: a79db44

### Added
- Config file support with strict validation (including whole-string numeric parsing).
- Explicit `top_band_color`, `center_band_color`, `bottom_band_color`.
- F1 help overlay.
- F fullscreen/windowed toggle.
- Fullscreen mouse cursor hiding (`hide_mouse_cursor`).
- Configurable phase bar height, segment gap, and margin.
- Strict CLI and boolean parsing.

### Changed
- Bottom GUI display is now strictly phase-only.
- Default visual bands: black top, dark grey center (18,18,18), black bottom.
- Config precedence: built-in defaults → config file → CLI overrides.
- Deprecated `background_color` / `band_color` aliases now only apply if explicit new keys are absent.

### Fixed
- Config/CLI precedence bugs.
- Deprecated alias fallback logic (no longer clobbers intentional black defaults).
- Unknown key warnings vs errors.
- Invalid RGBA and numeric value handling (clear errors with key/value/file:line).
- Help overlay blending and size calculation.
- Center band rendering to use explicit center color.

## v0.3 — Pi-validated bottom phase bar

Tag: v0.3-pi-validated
Commit: 9fb81d6b86bee97c13338b5a0ca542842cd0116c

## v0.2 — Corrected skeleton and Link submodule baseline

## v0.1 — Initial foundation
