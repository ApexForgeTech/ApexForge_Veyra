# Veyra Core Foundation Plan

## Current Status (as of 2026-06-03)

Foundation phases 0–5 are complete. The browser MVP is delivered.

| Phase | Title | Status |
|---|---|---|
| 0 | Direction Freeze | COMPLETE |
| 1 | Browser Core Foundation (WebKitGTK) | COMPLETE |
| 2 | Process and Profile Architecture | COMPLETE |
| 3 | Security Policy Engine | COMPLETE |
| 4 | Routing Engine (direct / VPN / Tor / I2P / chained) | COMPLETE |
| 5 | Download Quarantine (BlackVault) | COMPLETE |
| 6 | Fingerprint Layer | COMPLETE |
| 7 | Tool Integration Boundary | COMPLETE |
| 8 | Extension Isolation | COMPLETE |
| 9 | React UI Layer | COMPLETE |
| 10 | AI Orchestrator | COMPLETE |
| 11 | OSINT Workspace | COMPLETE |
| 12 | Blue Team / Incident Response Workspace | **NEXT** |
| 8 | Extension Isolation | pending |
| 9 | React UI Layer | pending |
| 10 | AI Orchestrator | pending |
| 11 | OSINT Workspace | pending |
| 12 | Blue Team / Incident Response Workspace | pending |
| 13 | Developer Workspace | pending |
| 14 | MicroVM Research Track | pending |

## Purpose

This document defines the correct starting point for Veyra if the goal is to build a real browser platform rather than a UI-first prototype.

The key decision is simple:

- Veyra should not begin as an Electron-first product
- Veyra should not treat JavaScript as the root implementation language
- Veyra should begin with a low-level browser foundation
- Real isolation, routing, policy enforcement, and persona separation must be designed into the architecture from the start

In short, this plan is for building Veyra as a serious browser system, not as a themed desktop wrapper.

## Core Decision

There are two possible directions:

1. Build a visual prototype first
2. Build a real browser foundation first

For Veyra's long-term goals, the second path is the correct one.

That means:

- the browser core must be low-level
- process boundaries must be real
- profile and persona isolation must be real
- security policy must be enforced below the UI layer
- networking and download controls must not be treated as cosmetic features

## Primary Language Choice

The first foundational language should be `C++`.

### Why C++ first

- browser process control, renderer policy, permission flow, and sandbox integration all live naturally at that level
- the browser shell, security hooks, and policy plumbing belong in the same environment as the engine backend
- a serious browser product still needs a low-level core even if its public identity is fully independent

### Why not Rust first

Rust is still a major part of the architecture, but not the first layer.

Rust is an excellent fit for:

- routing services
- secure storage helpers
- download quarantine services
- artifact scanning pipelines
- policy sidecars
- IPC-safe native services

But the root browser layer still belongs in C++ if the product is going to integrate deeply with an engine backend.

## Technology Ownership

The language split should be responsibility-driven:

- `C++`: browser shell, browser-core integration, sandbox and renderer policy, permission mediation
- `Rust`: routing, quarantine pipeline, secure storage, artifact inspection, policy services
- `TypeScript + React`: control-plane UI, dashboards, persona editor, route manager, vault views
- `Python`: AI orchestration, local model workflows, OSINT automation, reporting
- `Go`: future enterprise services, fleet management, sync backends, SOC integrations

## Fundamental Technical Principle

Veyra should not attempt to build a browser engine from scratch.

That would require rebuilding:

- HTML and CSS parsing
- a JavaScript runtime
- layout and rendering pipelines
- GPU integration
- networking stacks
- compatibility layers for the modern web

That is not a realistic product path.

The correct approach is:

- `native Veyra browser shell + dedicated engine-integration layer`

This keeps Veyra's product identity independent while still leaving room for compatibility, performance, and deep security customization.

## High-Level Architecture

The intended architecture is:

`UI Layer -> Persona and Policy Layer -> Security Layer -> Routing Layer -> Veyra Browser Core -> OS Sandbox`

More concretely, the platform can be broken down into:

1. `Veyra Browser Core`
2. `Veyra Browser Shell`
3. `Persona Manager`
4. `Security Policy Engine`
5. `Routing and Isolation Engine`
6. `Download Vault and Artifact Scan Pipeline`
7. `Tool Integration Layer`
8. `AI Orchestrator`
9. `Workspace Modules`

## Core Browser Modules

The following modules should be treated as the minimum viable foundation.

### Browser Process

Responsibilities:

- browser lifecycle
- window lifecycle
- tab creation
- profile loading
- persona loading
- permission mediation
- download interception

Primary language:

- `C++`

### Renderer Policy Layer

Responsibilities:

- JavaScript policy
- WebRTC policy
- permission requests
- fingerprint surfaces
- storage access policy
- site-level restrictions

Primary language:

- `C++`

### Persona Manager

This is Veyra's most important differentiator.

Each persona should own its own:

- cookie store
- local storage space
- cache space
- extension allowlist
- timezone profile
- locale profile
- DNS policy
- route profile
- AI memory namespace
- download policy
- evidence handling policy
- permission baseline
- history retention policy

Minimum persona model:

```text
Persona
- id
- display_name
- storage_partition
- route_profile_id
- fingerprint_profile_id
- timezone_profile
- locale_profile
- extension_policy_id
- ai_memory_scope
- security_mode
- ephemeral_flag
```

Implementation ownership:

- `C++` for core policy wiring
- `Rust` later for supporting profile-state services

### Security Mode Engine

Modes:

- Casual
- Hardened
- Ghost
- Red Team
- Blue Team
- Investigation
- Malware Analysis
- Airgap Transfer

Each mode should define:

- JavaScript policy
- cookie policy
- persistence policy
- WebRTC policy
- permission policy
- download policy
- routing requirements
- history retention policy
- evidence capture behavior
- audit behavior

Primary language:

- `C++`

### Routing Engine

Required route types:

- direct
- VPN
- Tor
- residential proxy
- chained route

Important constraint:

True per-tab routing in an MVP is significantly harder than it appears. The first realistic milestone is:

- `per-persona route isolation`

Later milestone:

- `per-tab route isolation`

Primary language:

- `Rust`

Reason:

- strong concurrency model
- better memory safety for network-facing code
- a good fit for long-running native services

### Download Vault

Downloads should not land directly on the host filesystem.

Instead, the flow should be:

1. intercept the download
2. write it to quarantine
3. extract metadata
4. compute hashes
5. infer file type
6. assign a risk score
7. release or block the file

Planned checks:

- static heuristics
- YARA rules
- entropy analysis
- macro analysis
- EXIF and metadata stripping where appropriate
- evidence-preserving export where appropriate

Primary languages:

- `Rust`
- `Python` for analysis integrations

### Fingerprint Defense Layer

The goal is not randomization for its own sake.

The real goal is:

- `consistent, believable, policy-controlled fingerprint shaping`

Target surfaces:

- canvas
- audio
- fonts
- GPU information
- WebGL
- timezone
- locale
- hardware concurrency
- device memory
- screen metrics

Version 1 should use:

- profile-based stable fingerprint sets

Later versions can explore:

- adaptive balancing and realism tuning

Primary language:

- `C++`

### Permission Broker

All sensitive permissions should flow through a central broker:

- camera
- microphone
- notifications
- clipboard
- geolocation
- fullscreen
- USB
- filesystem access

Rule:

- Veyra should not rely entirely on default browser permission behavior
- permission outcomes should pass through Veyra policy enforcement

Primary language:

- `C++`

### Tool Integration Layer

Veyra must be able to integrate with external tools used in:

- red-team operations
- blue-team investigations
- OSINT workflows
- incident response
- malware analysis
- developer and DevSecOps tasks

This layer should provide:

- plugin or adapter boundaries
- persona-aware execution context
- route-aware execution context
- controlled artifact import and export
- audit logging
- permissioned command execution
- secure local service bridges

Primary languages:

- `Rust` for the execution and exchange boundary
- `Python` for higher-level workflow adapters where appropriate

## Recommended Repository Shape

The current repository is a prototype. A real foundation should evolve toward something like this:

```text
apps/
  shell-ui/                  # React + TypeScript control-plane interface

core/
  browser-core/              # engine integration and browser-core notes
  veyra-shell/               # C++ browser shell
  policy-engine/             # C++ policy enforcement
  fingerprint-layer/         # C++ anti-fingerprint logic

services/
  route-engine/              # Rust
  vault-service/             # Rust
  artifact-scan/             # Rust + Python
  tool-bridge/               # Rust tool execution boundary
  persona-store/             # Rust

agents/
  ai-orchestrator/           # Python
  osint-worker/              # Python

schemas/
  integration/
  persona/
  policy/
  route/
```

## Phased Delivery Plan

### Phase 0: Direction Freeze `[COMPLETE]`

Decisions:

- stop treating Electron as the long-term core
- keep the prototype, but label it clearly as prototype-only
- create a core-first implementation track
- lock engine integration strategy for Phase 1:
  - Chromium-based backends are out of scope
  - Gecko/Firefox-based backends are out of scope
  - Linux-first backend for Phase 1 is WebKitGTK

Deliverables:

- technical decision record
- repository artifact: `docs/TDR-0001-engine-strategy.md`
- repository split plan
- milestone map
- repository artifact: `docs/PHASE0_PHASE1_MILESTONE_MAP.md`

Exit criteria:

- TDR merged and referenced by repo docs.
- `src/` labeled as prototype-only/non-production.
- `core/` identified as production implementation track.

### Phase 1: Browser Core Foundation `[COMPLETE]`

Tasks:

1. implement the locked backend and integration workflow (WebKitGTK, Linux-first)
2. establish the build environment
3. bootstrap the Veyra browser shell
4. run a basic single-window shell
5. support tab creation, navigation, and back-forward behavior

Goal:

- move from themed prototype to a real browser shell

Deliverables:

- minimal Veyra browser shell
- custom Veyra browser shell branding and runtime boundary
- working main browser process

Exit criteria:

- native window renders real web content
- tab creation works
- navigation works
- back-forward works

Primary language:

- `C++`

### Phase 2: Process and Profile Architecture `[COMPLETE]`

Tasks:

1. build a profile abstraction
2. layer persona abstraction on top of profiles
3. implement persistent and ephemeral partition behavior
4. add startup persona loading
5. add shutdown cleanup rules

Deliverables:

- persona-backed browsing sessions
- separate storage partitions
- ephemeral session teardown
- repository artifact: `docs/PHASE2_IMPLEMENTATION_NOTES.md`

Primary language:

- `C++`

### Phase 3: Security Policy Engine `[COMPLETE]`

Tasks:

- mode policy registry
- permission broker
- JavaScript restriction controls
- cookie strictness rules
- WebRTC kill switch
- DNS-handling hooks
- history retention policy

Deliverables:

- real Casual, Hardened, Ghost, Red Team, and Airgap behavior
- repository artifact: `docs/PHASE3_IMPLEMENTATION_NOTES.md`

Primary language:

- `C++`

### Phase 4: Routing Engine `[COMPLETE]`

Tasks:

1. define the route profile schema
2. implement the Rust routing service
3. build IPC between the browser shell and route engine
4. bind personas to route profiles
5. surface leak-prevention and route-health status

Initial target:

- direct
- VPN
- Tor
- I2P (optional; garlic-routing overlay via local I2P router HTTP proxy)
- chained

Deliverable:

- real per-persona route control foundation
- repository artifact: `docs/PHASE4_IMPLEMENTATION_NOTES.md`

Primary language:

- `Rust`

### Phase 5: Download Quarantine `[COMPLETE]`

Tasks:

1. download interception
2. quarantine path handling
3. metadata extraction
4. hashing pipeline
5. static heuristics
6. controlled release UI

Follow-up work:

- YARA scanning
- macro inspection
- entropy engine
- EXIF stripping

Deliverable:

- files do not flow directly to the host by default
- repository artifact: `docs/PHASE5_IMPLEMENTATION_NOTES.md`

Primary language:

- `Rust`

Note: `Python` was listed here for analysis integrations (YARA, entropy) but was not used in the Phase 5 implementation. Python remains the planned language for follow-up scanning integrations in a later sub-phase.

### Phase 6: Fingerprint Layer `[COMPLETE]`

Tasks:

- stable persona fingerprint profiles
- cross-surface consistency checks
- profile-to-policy mapping
- anti-anomaly validation

Deliverable:

- first usable fingerprint-obfuscation policy engine

Primary language:

- `C++`

### Phase 7: Tool Integration Boundary `[COMPLETE]`

Tasks:

- define integration schemas
- define command and service adapter boundaries
- define persona-aware execution contracts
- define artifact exchange contracts
- define audit logging requirements
- prevent unrestricted tool spawning outside policy

Deliverable:

- a secure integration foundation for red-team, blue-team, OSINT, and evidence workflows

Primary languages:

- `Rust`
- `Python`

### Phase 8: Extension Isolation `[COMPLETE]`

Tasks:

- allowlist model
- denylist model
- extension enablement by persona
- locked-down banking persona defaults
- stricter red-team persona handling

Deliverable:

- persona-specific extension policy

Primary language:

- `C++`

### Phase 9: React UI Layer `[COMPLETE]`

The UI should arrive after the core foundation exists.

Responsibilities:

- command palette
- persona manager
- route manager
- security dashboard
- vault monitor
- AI panel

Primary language and framework:

- `TypeScript + React`

Design principle:

- the UI should control the core
- the core should not exist as a mock created for the UI

### Phase 10: AI Orchestrator `[COMPLETE]`

AI belongs after the browser core, not before it.

Otherwise, the product risks becoming a chat UI wrapped around a weak browser shell.

Responsibilities:

- page summarization
- phishing hints
- script explanation
- OSINT task orchestration
- report generation
- persona-scoped memory

Primary language:

- `Python`

Integrations:

- local models
- Ollama
- optional remote model gateway

### Phase 11: OSINT Workspace `[COMPLETE]`

This should follow the AI orchestration layer.

Modules:

- WHOIS
- DNS map
- archive lookup
- metadata analysis
- username correlation
- timeline builder
- evidence packaging
- report export

Rule:

- legal, user-directed workflows first

Primary languages:

- `Python`
- `TypeScript + React` for the UI surface

### Phase 12: Blue Team and Incident Response Workspace `[pending]`

This track should support:

- IOC enrichment
- alert pivoting
- evidence review
- suspicious URL and file investigation
- case-linked browsing sessions
- future SOC connectors

This is also where evidence-preserving workflows must become first-class rather than incidental.

### Phase 13: Developer Workspace `[pending]`

This is where Veyra begins to feel like a cyber operating system.

Possible modules:

- embedded terminal
- git panel
- SSH manager
- API testing tools
- container hooks
- AI coding assistant

This should not ship before the core security model is credible.

Otherwise, it becomes a fancy dashboard rather than a serious platform.

### Phase 14: MicroVM Research Track `[pending]`

This should not be part of the MVP.

It should exist as a separate research track because it is:

- platform-specific
- expensive to build
- deeply dependent on OS primitives

Research directions:

- KVM-backed tab isolation
- Firecracker-style experiments
- seccomp and namespace hardening
- gated file transfer flows

## What Should Not Be Built First

The following should not lead the implementation:

- polished glass UI
- AI chat panels
- OSINT dashboards
- marketing visuals
- animated effects
- enterprise sync
- monetization

The correct build order is:

1. browser shell `[DONE]`
2. process and profile model `[DONE]`
3. persona partitions `[DONE]`
4. security modes `[DONE]`
5. routing `[DONE]`
6. quarantine `[DONE]`

Only after that:

7. UI panels `[NEXT]`
8. AI assistant
9. OSINT workspace
10. deep tool integrations

## Real MVP Definition

A credible browser MVP for Veyra is:

1. native custom shell `[DONE]`
2. multi-persona profile partitions `[DONE]`
3. persistent and ephemeral session handling `[DONE]`
4. security mode engine `[DONE]`
5. permission broker `[DONE]`
6. download quarantine `[DONE]`
7. route profile switching `[DONE]`

**The browser MVP core is complete as of Phase 5.**

Post-MVP track:

8. React dashboard `[NEXT after Phase 6]`
9. AI assistant
10. OSINT workspace
11. advanced tool integrations

## Recommended Language Order

If the work is sequenced realistically, the language order is:

1. `C++`
2. `Rust`
3. `TypeScript`
4. `Python`
5. `Go`

Why:

- `C++` builds the browser root
- `Rust` strengthens native security and routing services
- `TypeScript` powers the control-plane UI
- `Python` enables AI and OSINT workflows
- `Go` fits later enterprise infrastructure

Languages and frameworks that should not be primary choices here:

- `Angular`
- `Java`
- `C#`

Reason:

- they do not fit the browser-core problem as directly
- they add unnecessary architectural fragmentation

## Completed Execution Map (Phase 0–5)

All foundation phases are delivered. The following work is done:

- architecture direction frozen (Phase 0)
- repository restructured into prototype and core-foundation tracks (Phase 0)
- WebKitGTK engine integration operational (Phase 1)
- native window, tab creation, navigation, back-forward working (Phase 1)
- persona schema, security mode schema, route profile schema formalized (Phase 2)
- profile manager and persona partition lifecycle implemented (Phase 2)
- security mode registry and permission broker implemented (Phase 3)
- JavaScript, cookie, WebRTC, TLS, and navigation policies enforced (Phase 3)
- Rust route engine IPC operational (Phase 4)
- VPN, Tor, chained, and I2P route types supported (Phase 4)
- route override persistence and audit logging working (Phase 4)
- BlackVault download quarantine with static heuristic scanning (Phase 5)
- SHA-256 / SHA-1 hashing, MIME detection, and risk scoring (Phase 5)
- controlled release review before artifacts leave quarantine (Phase 5)

## Completed Execution Sprint (Phase 6: Fingerprint Layer)

Phase 6 is implemented. The following work is done:

- fingerprint profile schema (`schemas/fingerprint/fingerprint-profile.schema.json`)
- four shipped fingerprint profiles: `native_stable`, `balanced_stealth`, `low_noise_obfuscated`, `minimal_surface`
- `FingerprintProfileDefinition` added to `FoundationState` and loaded at startup
- `fingerprint_profile_id` reference validation in `profile_registry.cc`
- `FingerprintPolicy` and `FingerprintEngine` in `core/veyra-shell/src/runtime/fingerprint_engine.cc`
- per-session canvas noise via seeded xorshift32 PRNG (deterministic, stable within a session)
- WebGL `getParameter` override for VENDOR, RENDERER, UNMASKED_VENDOR_WEBGL, UNMASKED_RENDERER_WEBGL
- AudioContext `getFloatFrequencyData` and `getByteFrequencyData` noise injection
- navigator property overrides: hardwareConcurrency, deviceMemory, platform
- screen metrics overrides: width, height, availHeight, colorDepth, pixelDepth, devicePixelRatio
- JavaScript injected at `WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START` in all frames
- fingerprint script rebuilt on hot-route reconfigure via `ApplySecurityPolicy`
- `fingerprint_profile_id` and `fingerprint_script_injected` in security-policy reports
- `docs/PHASE6_IMPLEMENTATION_NOTES.md` published

## Completed Execution Sprint (Phase 7: Tool Integration Boundary)

Phase 7 is implemented. The following work is done:

- tool definition schema (`schemas/integration/tool.schema.json`)
- 7 pre-defined tools: `whois`, `dig`, `nmap-quick`, `curl-fetch`, `file-inspect`, `strings-extract`, `openssl-x509`
- `ToolDefinition` model loaded into `FoundationState` and `ProfileManager`
- unique ID validation in `profile_registry.cc`
- Rust `tool-bridge` service (`core/tool-bridge/src/main.rs`) with stdio IPC: INVOKE, STATUS, CANCEL, SHUTDOWN
- per-tool timeout enforcement (configurable `max_runtime_seconds`)
- stdout/stderr capture and per-line OUTPUT streaming back to C++ shell
- `ToolBridgeClient` C++ class with `CheckPolicy`, `Invoke`, audit logging, quarantine routing
- 5-rule policy evaluation: tool registry check, allowed personas, denied personas, denied security modes, network + isolation mode check
- per-invocation JSONL audit events appended to session `events.jsonl`
- tool output quarantine routing for `output_to_quarantine=true` tools
- CLI flags `--invoke-tool=<tool_id>:<args>` and `--list-tools`
- `tool_count` in `RegistrySummary` and bootstrap output
- `docs/PHASE7_IMPLEMENTATION_NOTES.md` published

## Completed Execution Sprint (Phase 8: Extension Isolation)

Phase 8 is implemented. The following work is done:

- extension policy schema (`schemas/extension/extension-policy.schema.json`)
- 3 extension policies: `trusted_daily`, `minimal_locked`, `strict_redteam`
- `ExtensionPolicyDefinition` loaded into `FoundationState`; `extension_policy_id` validated in registry
- `ExtensionPolicy` runtime struct with 5 policy fields and pre-built eval-block script
- `extension_engine.cc`: `BuildExtensionPolicy`, `BuildEvalBlockScript`, `IsBlockedDomain` (subdomain-aware)
- eval/Function/setTimeout/setInterval blocking via document-start IIFE injected in ALL frames
- domain navigation blocking in `ShouldAllowNavigation` using `IsBlockedDomain`
- mixed content enforcement via `webkit_settings_set_allow_running_insecure_content` and `webkit_settings_set_allow_displaying_insecure_content`
- extension policy script injected per-tab at `CreateTab`; cleared and rebuilt on hot-route reconfigure
- 11 domains blocked for `minimal_locked`; 26 domains blocked for `strict_redteam`
- security-policy JSON report includes 7 extension policy fields
- `extension_policy_count` in `RegistrySummary` and bootstrap output
- `docs/PHASE8_IMPLEMENTATION_NOTES.md` published

## Completed Execution Sprint (Phase 9: React UI Layer)

Phase 9 is implemented. The following work is done:

- `apps/shell-ui/` TypeScript + React + Vite app scaffolded and fully written
- 5 panels: Personas, GhostNet Route, Sentinel Guard, BlackVault, Tool Bridge
- Command palette (Ctrl+K) with fuzzy search across route switch, tool invoke, browser actions
- Dark cyberpunk CSS theme matching Veyra branding (`#070c14` base, `#2aa6ff` accent)
- `VeyraState` TypeScript types with full persona, route, permission, vault, and tool data
- `window.__VEYRA_STATE__` injection at document-start before React boots
- `window.webkit.messageHandlers.veyra.postMessage(...)` for C++-bound actions
- `window.__VEYRA_UPDATE__(state)` function for C++ to push refreshed state
- `shell_ui_bridge.h/.cc`: `SerializeDashboardState`, `BuildStateInjectionScript`, `ParseDashboardAction`
- `WebKitGtkBrowserEngine`: GtkPaned sidebar (300px), ephemeral dashboard WebView, `veyra` message handler
- `PushDashboardState` — calls `window.__VEYRA_UPDATE__` after route switch or tool invocation
- `on_dashboard_action` callback in `main.cc`: switch_route, invoke_tool, open_tab, request_state_refresh
- `--shell-ui-path=<dist/>` CLI flag to activate the dashboard panel
- CMake npm build step (`find_program(NPM_EXECUTABLE npm)`)
- `docs/PHASE9_IMPLEMENTATION_NOTES.md` published

## Completed Execution Sprint (Phase 10: AI Orchestrator)

Phase 10 is implemented. The following work is done:

- `agents/ai-orchestrator/veyra_ai_orchestrator.py` — stdlib-only Python sidecar, JSONL over stdio
- Local-first Ollama integration via `urllib` (no pip dependencies)
- `summarize`, `phishing_check`, `explain_script`, `ping` methods
- Offline phishing heuristics (IP host, punycode, brand-in-subdomain, suspicious TLD, login-over-HTTP, …) that work with no model
- Graceful degradation when Ollama is offline (verified: 404/offline → fallback, never crashes)
- `schemas/ai/ai-policy.schema.json` + `default-ai-policies.json` (ai_full, ai_scoped, ai_restricted, ai_disabled)
- Per-persona `ai_policy_id` added to persona schema + all 4 seed personas
- `AiPolicyDefinition` threaded through foundation_models → loader → registry validation → profile_manager → RuntimeProfile
- `AiOrchestratorClient` (C++): forks Python per-call, JSONL stdio, enforces policy (allow_page_content, allow_script_analysis, allow_phishing_check) before sending content
- `RequestActiveTabText` async JS extraction (`document.body.innerText`) for summarization
- `GetActiveTabUrl` / `GetActiveTabTitle` for phishing checks
- Dashboard action handlers: `ai_summarize`, `ai_phishing_check`, `ai_explain_script` in main.cc
- `AiResultSnapshot` serialized into dashboard state; results pushed back via `PushDashboardState`
- React `AiPanel.tsx` — Cortex AI tab: policy posture, Summarize/Phishing buttons, script textarea, result card with risk badges
- Command palette AI commands (summarize / phishing) gated on policy
- `--ai-orchestrator-script=` and `--ai-python=` CLI flags
- CMake stages the Python script next to the shell binary
- Full C++ build verified (compiles + links against WebKitGTK 2.50); React app builds (tsc + vite); end-to-end summarize verified against a real local model
- `docs/PHASE10_IMPLEMENTATION_NOTES.md` published

## Completed Execution Sprint (Phase 11: OSINT Workspace)

Phase 11 is implemented. The following work is done:

- `agents/osint-workspace/veyra_osint_workspace.py` — stdlib-only Python sidecar, JSONL over stdio
- Route-honouring network layer: in-file SOCKS5 (Tor, remote DNS via ATYP=domain), HTTP proxy CONNECT (I2P), direct
- Leak prevention: `ensure_egress_allowed` refuses lookups when `require_route` and no proxy is active
- DNS via DoH (Cloudflare JSON) through the route — no system-resolver leak
- Modules: `whois_lookup` (IANA → registry refer), `dns_lookup` (A/AAAA/MX/NS/TXT), `archive_lookup` (Wayback CDX), `username_search` (curated ~15 sites)
- `schemas/osint/osint-policy.schema.json` + `default-osint-policies.json` (osint_full, osint_passive, osint_operational, osint_disabled)
- Per-persona `osint_policy_id` added to persona schema + all 4 seed personas
- `OsintPolicyDefinition` threaded through foundation_models → loader → registry validation → profile_manager → RuntimeProfile
- `OsintWorkspaceClient` (C++): forks Python per-call, JSONL stdio, SIGPIPE-safe + poll-bounded IO, client-side module policy gate
- Investigation case model (`OsintCaseSnapshot`): entities deduped by type+value, relationships by from+to+kind, timeline appended; owned by the shell
- Dashboard actions: `osint_whois`, `osint_dns`, `osint_archive`, `osint_username`, `osint_clear`; live route resolved per action
- React `OsintPanel.tsx` — Recon tab: target input, module buttons, egress posture badge, entity list, timeline
- Command palette `osint_clear` command gated on an active case
- `--osint-workspace-script=` CLI flag; CMake stages the Python script
- Full C++ build verified (WebKitGTK 2.50); React builds (41 modules); leak guard + policy gate + ping verified end-to-end through C++; SIGPIPE-safe
- `docs/PHASE11_IMPLEMENTATION_NOTES.md` published

## Next Execution Sprint (Phase 12: Blue Team / Incident Response Workspace)

The next sprint should focus on:

1. scaffold `agents/blueteam-workspace/` Python project
2. IOC enrichment (hashes, IPs, domains) reusing the routed network layer from Phase 11
3. a local indicator store (sightings, verdicts) scoped per persona
4. log/artefact triage helpers that route through BlackVault (Phase 5) and Tool Bridge (Phase 7)
5. a React `Blue Team` panel surfacing indicators, verdicts, and a triage queue
6. all egress route-locked exactly as OSINT (no leaks via direct ISP)

Primary language: `Python` (workspace), `TypeScript` (React panel), `C++` (bridge)

Sprint goal:

- deliver a defensive triage workspace — persona-scoped IOC enrichment and artefact analysis that never leaks outside the active route

## Final Recommendation

The strategic conclusion is:

- start with the browser root
- choose the low-level foundation first
- build persona separation, security, and routing into the core
- add UI and AI only after the foundation is credible

The clearest summary is:

- `C++` should be the root language of Veyra
- `Rust` should be the second major systems language
- `TypeScript + React` should sit above the core as the control-plane UI
- `Python` should power AI and OSINT workflows

## Next Practical Step

The next technically correct step is Phase 6: Fingerprint Layer.

The foundation is complete. What is missing before a credible security product can be claimed is:

1. anti-fingerprinting policy engine (Phase 6) — without this, persona separation leaks through canvas/GPU/audio surfaces
2. React control-plane UI (Phase 9) — without this, operators cannot manage personas, routes, or vault state through a real UI
3. AI orchestrator (Phase 10) — post-UI, local model integration for page analysis and phishing hints

The principle remains:

- fingerprint hardening before UI
- UI before AI
- AI before OSINT workspace
- OSINT workspace before Blue Team workspace
- Blue Team workspace before Developer workspace
- MicroVM research is always a separate optional track
