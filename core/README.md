# Core Foundation

This directory is the starting point for Veyra's real browser foundation.

Current contents:

- `browser-core/`: notes for the future engine integration track
- `veyra-shell/`: initial native C++ shell scaffold

The shell bootstrap now validates the shipped persona, route, and security-mode seed data
against the repository schemas before runtime profile construction begins.

It also derives an effective runtime security policy per persona, validates policy-to-route
consistency, and exposes an initial permission-broker model for the native shell.

The intent is to move the project from a prototype-first structure to a foundation-first structure.
