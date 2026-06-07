# Phase 2 Implementation Notes

## Scope Covered

Phase 2 targets from `VEYRA_CORE_FOUNDATION_PLAN.md`:

1. build a profile abstraction
2. layer persona abstraction on top of profiles
3. implement persistent and ephemeral partition behavior
4. add startup persona loading
5. add shutdown cleanup rules

## Implemented

- Profile abstraction is active through `ProfileManager` + `RuntimeProfile` composition.
- Persona-to-profile startup resolution is implemented via `ResolveStartupProfile(...)` and CLI `--persona=<id>`.
- Session partition lifecycle bootstrap is implemented via `SessionLifecycleManager`:
  - runtime roots
  - per-persona partition directories
  - persistent partition retention
  - ephemeral partition staging
- Each allocation writes a `session.json` manifest with persona/security/route/session metadata.
- Stale ephemeral directories under the runtime memory root are cleaned on the next bootstrap by default.
- Shutdown cleanup rules are implemented for ephemeral partitions by default.
- Cleanup can be bypassed intentionally with `--keep-ephemeral` for debugging.
- Runtime root can be overridden with `--runtime-root=<path>`.
- Persona inventory can be printed with `--list-personas`.

## Current Limitations

- Session lifecycle currently manages filesystem partition paths, not full browser-engine storage wiring.
- Per-tab storage partitioning is not yet implemented; partitioning is persona/profile scoped.
- Routing and vault services are still future phases.
