# Phase 0 / Phase 1 Milestone Map

**Status: Both phases complete.**

## Purpose

This map defines measurable, date-free exit criteria for the first two delivery phases.

## Phase 0 Exit (Direction Freeze) `[COMPLETE]`

All criteria must be true:

- TDR-0001 is merged and accepted.
- Engine strategy is locked to Linux-first WebKitGTK.
- Chromium and Gecko/Firefox are explicitly out of scope for Phase 1.
- Repository tracks are explicitly split:
  - `src/` => prototype-only and non-production.
  - `core/` => production implementation track.
- Core plan references the locked strategy (not an open decision).

## Phase 1 Exit (Browser Core Foundation) `[COMPLETE]`

All criteria must be true:

- Native window is created from the C++ shell runtime.
- Default tab is created and loads a real web URL.
- Tab creation API works for additional tabs.
- Navigation API works (`Navigate`).
- History APIs work (`GoBack`, `GoForward`).
- Runtime synchronization updates `TabModel` from engine navigation callbacks.
- Startup foundation validation remains fail-fast and blocks engine launch on invalid seed data.
- Linux build configuration fails early with clear dependency errors when WebKitGTK dev packages are missing.

## Smoke and Regression Checks

- `veyra_shell --smoke` passes foundation bootstrap checks.
- Existing schema validation and profile bootstrap flows remain intact.
- Permission broker preview remains available in bootstrap output.
