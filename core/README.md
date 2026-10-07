# Core Foundation

This directory is the starting point for Veyra's real browser foundation.

Current contents:

- `browser-core/`: notes for the future engine integration track
- `veyra-shell/`: initial native C++ shell scaffold

The shell bootstrap now validates the shipped persona, route, and security-mode seed data
against the repository schemas before runtime profile construction begins.

It also derives an effective runtime security policy per persona, validates policy-to-route
consistency, and exposes an initial permission-broker model for the native shell.

The core foundation now also includes a Rust route-engine service that binds personas to
route runtime states during startup, exposes health and leak-prevention reporting, and
surfaces startup proxy endpoints back into the native shell.

Route control now also includes:

- persisted route override replay via `route-overrides.json`
- route switch audit logging via `route-events.jsonl`
- CLI route listing and startup route override controls

Artifact control now also includes:

- a Rust artifact-scan child service
- quarantined download routing under each session partition
- controlled release review through the native shell

The native runtime now includes Linux-first WebKitGTK integration with:

- real native window bootstrap
- real page rendering
- tab creation and activation
- navigation and back-forward support
- startup persona selection and session partition lifecycle bootstrap
- route-engine IPC bootstrap and route-state report generation
- ephemeral partition cleanup on shutdown

The intent is to move the project from a prototype-first structure to a foundation-first structure.

Track status:

- `core/` is the production implementation track.
- `src/` is the prototype-only Electron track and is not the production engine path.
