# Veyra Core Foundation Plan

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

### Phase 0: Direction Freeze

Decisions:

- stop treating Electron as the long-term core
- keep the prototype, but label it clearly as prototype-only
- create a core-first implementation track
- select the engine integration strategy

Deliverables:

- technical decision record
- repository split plan
- milestone map

### Phase 1: Browser Core Foundation

Tasks:

1. choose the engine backend and integration workflow
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

Primary language:

- `C++`

### Phase 2: Process and Profile Architecture

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

Primary language:

- `C++`

### Phase 3: Security Policy Engine

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

Primary language:

- `C++`

### Phase 4: Routing Engine

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
- chained

Deliverable:

- real per-persona route control foundation

Primary language:

- `Rust`

### Phase 5: Download Quarantine

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

Primary languages:

- `Rust`
- `Python`

### Phase 6: Fingerprint Layer

Tasks:

- stable persona fingerprint profiles
- cross-surface consistency checks
- profile-to-policy mapping
- anti-anomaly validation

Deliverable:

- first usable fingerprint-obfuscation policy engine

Primary language:

- `C++`

### Phase 7: Tool Integration Boundary

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

### Phase 8: Extension Isolation

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

### Phase 9: React UI Layer

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

### Phase 10: AI Orchestrator

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

### Phase 11: OSINT Workspace

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

### Phase 12: Blue Team and Incident Response Workspace

This track should support:

- IOC enrichment
- alert pivoting
- evidence review
- suspicious URL and file investigation
- case-linked browsing sessions
- future SOC connectors

This is also where evidence-preserving workflows must become first-class rather than incidental.

### Phase 13: Developer Workspace

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

### Phase 14: MicroVM Research Track

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

1. browser shell
2. process and profile model
3. persona partitions
4. security modes
5. routing
6. quarantine

Only after that:

7. UI panels
8. AI assistant
9. OSINT workspace
10. deep tool integrations

## Real MVP Definition

A credible browser MVP for Veyra is:

1. native custom shell
2. multi-persona profile partitions
3. persistent and ephemeral session handling
4. security mode engine
5. permission broker
6. download quarantine
7. route profile switching

Only after those foundations:

8. React dashboard
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

## First 90-Day Execution Map

### Days 1-10

- freeze architecture direction
- restructure the repository
- choose engine integration workflow
- prepare the build environment

### Days 11-25

- bootstrap the custom shell
- implement basic window and tab lifecycle
- support navigation

### Days 26-40

- build the profile system
- add persona partitions
- add ephemeral session behavior

### Days 41-55

- build the security mode registry
- add permission brokering
- add WebRTC policy
- add cookie policy

### Days 56-70

- ship the Rust routing service
- bind personas to routes
- add route and leak status reporting

### Days 71-85

- implement quarantine pipeline
- add controlled release flow
- add risk summary output

### Days 86-90

- run the first internal MVP review
- perform threat-model review
- correct architectural weaknesses

## First Coding Sprint

If the team follows this plan, the first sprint should focus on:

1. separating the Electron prototype as explicitly prototype-only
2. creating the new `core/` structure
3. writing the browser-shell bootstrap plan and build files
4. formalizing the persona schema
5. formalizing the security mode schema
6. formalizing the route profile schema
7. scaffolding the C++ shell entrypoint

Sprint goal:

- move the repository from prototype thinking to real browser-foundation thinking

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

The next technically correct step is:

1. split the repository into prototype and core-foundation tracks
2. create the `core/` area for the C++ browser shell
3. move persona, mode, and route models into formal schemas
4. define Rust service boundaries before implementation begins

The principle is:

- foundation first
- compartments second
- security third
- routing fourth
- vault fifth
- AI and workspace last
