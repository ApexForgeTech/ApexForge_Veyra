# Phase 4 Implementation Notes

## Scope

Phase 4 introduces the first real routing control foundation for Veyra.

This phase completes:

- a Rust route-engine service
- stdio IPC between the native shell and the route engine
- per-persona route binding during startup bootstrap
- persisted route override and route switch orchestration
- route-health and leak-status reporting
- route-state reporting to disk
- route override and route event reporting to disk
- startup route state propagation into the WebKit network configuration layer

## Implemented Artifacts

### 1. Rust Route Engine

The route engine now lives under:

- `core/route-engine/`

It runs as a Linux child process in `--stdio` mode and accepts bootstrap commands from the native shell.

### 2. Shell-to-Route IPC

The C++ shell now launches the route engine as a sibling binary and communicates through line-based stdio IPC.

Current command flow:

1. `BOOTSTRAP`
2. one `PROFILE` record per persona
3. `END`
4. `ROUTE` responses returned per persona
5. `OK`

The service now also supports:

1. `SWITCH` for per-persona route profile changes after bootstrap
2. `STATUS` for refreshed route runtime state queries

### 3. Route Runtime State

Each persona now resolves to a route runtime state that includes:

- route type
- dns policy
- route health
- proxy endpoint
- leak-prevention status
- DNS resolver endpoint
- diagnostic summary

### 4. Route State Report

Bootstrap now writes:

- `runtime_root/route-state.json`
- `runtime_root/route-overrides.json`
- `runtime_root/route-events.jsonl`

This is the Phase 4 artifact for operator and future control-plane visibility.

### 5. Engine Integration

The startup persona route state now feeds the active browser engine policy:

- direct routes keep system-default proxy behavior
- routed personas receive custom proxy endpoints
- route DNS and leak status are recorded in `security-policy.json`
- active tabs now also carry resolved route metadata in the runtime model

## Route Override Model

Phase 4 now supports route profile switching without editing seed data.

Supported CLI controls:

- `--startup-route=<route_profile_id>`
- `--route-override=<persona_id:route_profile_id>`
- `--list-route-profiles`
- `--list-routes`

Overrides are persisted into `route-overrides.json`, replayed at the next startup, and written to `route-events.jsonl` as audit events after the route-engine applies them.

## Current Route Model

Supported route types:

- `direct`
- `vpn`
- `tor`
- `chained`
- `i2p` (optional; garlic-routing overlay)

Current proxy endpoint staging:

- `vpn` -> `socks5://127.0.0.1:19090`
- `tor` -> `socks5://127.0.0.1:19050`
- `chained` -> `socks5://127.0.0.1:19110`
- `i2p` -> `http://127.0.0.1:4444` (local I2P router HTTP proxy)

These are control endpoints. `i2p` requires a separately running I2P router (`i2pd` or Java I2P).

## I2P Route Notes

The I2P route type uses garlic routing, which differs from Tor's onion routing.

Key properties of the I2P route:

- Proxy: `http://127.0.0.1:4444` (I2P HTTP proxy; handles both `.i2p` eepsite and clearnet via outproxy)
- DNS resolver: `i2p-proxy-bound` (hostname resolution handled entirely by the I2P router)
- WebRTC: always disabled for I2P personas to prevent real IP exposure
- `.i2p` eepsite navigation: enabled only when the active persona uses the `i2p` route type
- HTTPS to `.i2p`: blocked at the navigation policy layer (eepsites are HTTP-only; security is at the I2P transport layer)
- Leak prevention: `maximum` (same posture as Tor)

The `i2p_research` persona ships as a default seed persona using the `i2p_network` route profile with Ghost mode and ephemeral storage.

## Validation

Validated in this repository with:

- native build including the Rust route engine
- smoke runs for `real_identity`, `anonymous`, and `red_team`
- smoke runs with persisted route overrides and route listing flags
- route-state report generation checks
- route override and route event report generation checks
- security-policy report checks for route metadata propagation
- session manifest checks for overridden route-profile alignment

## Known Limits

Phase 4 provides control-plane routing foundation, not full packet-path enforcement.

Current limits:

- the Rust route engine is a staged control service and does not yet own a real tunnel implementation
- health status is policy-derived and route-structure-derived, not from live tunnel probes
- route switching is control-plane level and is not yet a hot in-place proxy swap for already opened WebKit tabs
- per-tab route isolation is still future work
- centralized route telemetry and UI management are not implemented yet
