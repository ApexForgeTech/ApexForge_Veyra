# Phase 3 Implementation Notes

## Scope

Phase 3 turns Veyra's security-mode model into a real runtime enforcement layer inside the native WebKitGTK shell.

This phase completes:

- security mode registry generation
- per-persona engine policy derivation
- per-session security policy reporting
- permission broker enforcement hooks
- JavaScript restriction wiring
- cookie policy wiring
- WebRTC/media-capture kill switch wiring
- DNS and isolation navigation hooks
- history-retention alignment between runtime policy and tab model

## Implemented Artifacts

### 1. Security Mode Registry

Bootstrap now writes a runtime-wide registry report:

- `runtime_root/security-mode-registry.json`

This file records the effective mode inventory derived from schema-backed foundation data.

### 2. Per-Persona Engine Policy Reports

Each prepared session partition now receives:

- `security-policy.json`

The report includes:

- persona and session identity
- route and security-mode posture
- JavaScript, cookie, WebRTC, and history settings
- navigation isolation flags
- permission broker decisions

### 3. WebKitGTK Enforcement

The native shell now applies policy in three layers:

1. `WebKitWebsiteDataManager`
2. `WebKitWebContext`
3. `WebKitWebView` settings and callbacks

Real enforcement now includes:

- JavaScript hard-disable for `high_restrict`
- media stream disable when WebRTC is disabled
- persistent vs ephemeral website data managers
- persistent cookie storage only for eligible persistent personas
- third-party cookie restriction for strict and ephemeral cookie modes
- TLS fail-on-error policy
- sandbox-enabled WebKit context
- navigation blocking for direct IP literals in isolated DNS postures
- navigation blocking for localhost and private-network targets in isolated modes
- non-web scheme blocking
- permission-request allow or deny handling for notifications, geolocation, clipboard, camera, and microphone
- interactive GTK modal prompts for permission states that resolve to `prompt`
- session-scoped caching of prompt decisions per origin and permission
- disk-backed prompt decision persistence inside the active persona partition
- a custom Veyra-branded permission sheet UI for interactive reviews
- fullscreen gate through the permission broker
- file chooser denial through the permission broker
- site notification suppression through the permission broker

### 4. History Retention

History retention remains enforced at the Veyra tab model layer:

- `standard` -> tracked with the larger bounded history buffer
- `reduced` -> bounded to the reduced entry count
- `none` -> only the current location is retained

## Mode Outcomes

### Casual

- JavaScript enabled
- persistent session storage allowed
- media capture allowed
- localhost and private-network navigation allowed
- notifications and fullscreen allowed
- prompt-based requests surface an operator decision dialog

### Ghost

- ephemeral context only
- media capture disabled
- history retention disabled
- localhost and private-network navigation blocked
- permission posture deny-by-default

### Red Team

- JavaScript disabled at engine settings level
- ephemeral context only
- media capture disabled
- localhost, private-network, and IP-literal navigation blocked
- permission posture deny-by-default

## Known Limits

Phase 3 is real enforcement, but it is not the final browser-security architecture.

Current limits:

- DNS and isolation handling is enforced through navigation policy and session posture, not through a completed Rust route engine
- per-tab isolated WebKit network stacks are not implemented yet; isolation is per launched persona/session
- download quarantine plumbing still belongs to Phase 5
- prompt decisions are persisted per partition, but are not yet managed by a central review UI or enterprise policy service

## Validation

Validated in this repository with:

- native syntax checks
- manual native build
- smoke runs for `real_identity`, `anonymous`, and `red_team`
- report generation checks for `security-mode-registry.json` and `security-policy.json`
