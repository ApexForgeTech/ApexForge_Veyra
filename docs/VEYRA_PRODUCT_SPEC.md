# ApexForge Veyra Product Specification

## Vision

ApexForge Veyra is intended to be more than a browser. The product direction is a compartmentalized cyber workspace for daily browsing, privacy-focused research, developer workflows, OSINT, and AI-assisted analysis.

The goal is to serve users who need more than mainstream browsing:

- privacy-focused users
- developers
- researchers
- journalists
- OSINT analysts
- incident responders
- malware analysts
- reverse engineers
- security teams
- red teams
- blue teams
- enterprises with stricter browser controls

## Product Positioning

Most modern browsers optimize only a subset of the problem space:

- `Chrome`: excellent compatibility, weak privacy posture by default
- `Firefox`: flexible, but less consistent across ecosystems and workflows
- `Tor Browser`: strong anonymity, but too heavy for many daily-use cases
- `Brave`: privacy-oriented, but not built as a deep compartment platform
- `Opera GX`: tuned for gamer UX rather than security architecture
- `Arc`: strong visual design, weaker isolation story

Veyra should aim to combine:

- mainstream web compatibility
- credible compartment boundaries
- route-aware privacy controls
- built-in AI assistance
- artifact quarantine and analysis
- workspace-style operational tooling

## Product Thesis

Veyra should be:

- practical for everyday browsing
- strong enough for privacy-sensitive work
- useful for research and documentation
- extensible into AI-assisted workflows
- designed around compartmentalization instead of treating it as an add-on

## Goals

The product goals are:

- build an independent browser product rather than a branded wrapper
- make compartmentalization a first-class feature
- support real operator workflows for red team, blue team, OSINT, and incident response
- make routing, permissions, artifact handling, and persona state policy-driven
- provide a tool-integration model that is secure, auditable, and persona-aware
- support AI-assisted workflows without turning the browser into a chat-first product

## Non-Goals

The near-term non-goals are:

- building a completely new browser engine from scratch
- shipping microVM-backed tabs in the first release
- pretending UI mockups are equivalent to real route isolation
- exposing unrestricted local-tool execution without policy controls
- mixing personas, evidence, and downloads in a shared unsafe workspace

## Realistic Architecture

The intended long-term architecture is:

`UI Layer -> AI Orchestration -> Security and Isolation -> Networking -> Veyra Browser Core -> OS Sandbox / Optional MicroVM`

### Browser Core Strategy

The product should be presented as its own browser, with its own shell, identity, and platform model.

That means:

- Veyra should not be described as another browser's skin
- the engine backend is a technical choice, not the product definition
- the browser shell, policy model, routing model, and persona model belong to Veyra itself

The most important requirement for the backend is:

- strong modern web compatibility
- mature extension ecosystem
- existing support for WebGPU, WebRTC, and WebAssembly
- realistic path to daily usability

### Reality Check

This repository is currently an Electron prototype, not a real browser-engine implementation.

That is acceptable for validating ideas, but it must not be confused with the long-term architecture.

A realistic progression is:

1. use Electron for concept exploration
2. extract routing and security logic into native services
3. move to a native Veyra browser shell
4. add deeper hardening and optional microVM-backed research tracks

## Language and Framework Strategy

The stack should be chosen by responsibility, not by variety.

### Recommended Stack

- `C++`
  - browser shell
  - browser-core integration
  - sandbox and renderer policy
  - WebRTC and permission control
  - fingerprint surface handling

- `Rust`
  - routing controller
  - secure storage helpers
  - quarantine and artifact inspection services
  - policy sidecars
  - native service boundaries

- `TypeScript + React`
  - desktop control-plane UI
  - settings
  - persona management
  - dashboards
  - route and vault views
  - AI and OSINT workspace surfaces

- `Python`
  - AI orchestration
  - local LLM workflows
  - Ollama integration
  - OSINT automation modules
  - reporting pipelines

- `Go`
  - enterprise control plane
  - fleet management
  - secure sync and service APIs
  - SOC integrations

### Limited or Optional Use

- `JavaScript`
  - acceptable for the current prototype
  - should shrink as TypeScript replaces it

- `C`
  - reasonable only for small low-level compatibility helpers where needed

- `Java` or `Kotlin`
  - only if there is a future Android companion effort

- `Angular`
  - not recommended alongside React
  - if a modern UI framework is needed here, React is the better fit

## Recommended Repository Shape

For a serious implementation, the repository can evolve toward:

```text
apps/
  desktop-shell/         # TypeScript + React desktop UI
  control-plane/         # Go enterprise/fleet backend

services/
  route-controller/      # Rust
  vault-service/         # Rust
  artifact-scan/         # Rust + Python integrations
  tool-bridge/           # Rust tool execution and exchange boundary
  ai-orchestrator/       # Python

core/
  browser-core/          # C++
  sandbox-bridge/        # C++ / Rust / small C helpers

packages/
  ui-kit/
  integration-schema/
  persona-schema/
  policy-schema/
  shared-protocols/
```

## Core Product Features

### Digital Personas

Digital Personas should be Veyra's primary differentiator.

Target personas:

- Real Identity
- Anonymous
- Work
- Research
- Red Team
- Blue Team
- Incident Response
- Malware Analysis
- Banking
- Social Media
- Disposable

Each persona should control its own:

- cookies
- storage partition
- AI memory scope
- route profile
- DNS policy
- permission defaults
- timezone profile
- fingerprint policy
- extension allowlist
- history retention behavior
- download policy
- evidence handling behavior

The guiding rule is simple:

- one persona should not meaningfully leak into another

### Security Modes

**Implemented modes** (in `schemas/policy/security-mode.schema.json`):

- `Casual` — balanced defaults for daily browsing
- `Hardened` — stricter tracking resistance and permission behavior
- `Ghost` — RAM-only, low-retention, auto-cleanup session behavior
- `Red Team` — higher scrutiny, stronger route discipline, stronger leak prevention; JavaScript disabled
- `Airgap Transfer` — controlled file handoff and analysis-first workflow

**Planned modes** (not yet in schema; require Phase 3 expansion):

- `Blue Team` — evidence-safe investigation with stricter logging and artifact handling
- `Investigation` — research-oriented evidence collection and pivot workflow
- `Malware Analysis` — stronger download isolation and hostile-content handling

When adding planned modes, extend the `id` enum in `schemas/policy/security-mode.schema.json` and add seed entries in `schemas/policy/default-security-modes.json` before using them in personas.

### Built-in AI

AI should be local-first where practical.

High-value AI functions:

- summarize active pages
- explain code or scripts
- flag phishing and scam indicators
- warn about suspicious JavaScript patterns
- assist with OSINT collection workflows
- generate research notes and reports
- automate repetitive browser workflows
- explain suspicious infrastructure indicators
- assist with documentation, triage, and evidence summarization

### Download Security

Downloads should not be treated like normal browser downloads.

Preferred flow:

1. intercept the file
2. place it into an isolated staging area
3. run malware and YARA checks
4. perform entropy and macro analysis
5. strip metadata where appropriate
6. release only through a controlled handoff

### Networking

Routing should be a first-class product feature.

Required route options:

- direct ISP
- VPN
- Tor
- I2P (optional, garlic-routing overlay; requires a local I2P router)
- residential proxy
- chained route

Important implementation note:

True per-tab route isolation is significantly harder than UI mockups suggest. For MVP, per-persona route control is the more realistic target.

I2P notes:

- I2P requires a separately installed and running I2P router (`i2pd` or the Java I2P client).
- Veyra routes I2P-persona traffic through the local I2P HTTP proxy on `127.0.0.1:4444`.
- `.i2p` eepsite navigation is gated: only allowed when the active persona uses an I2P route.
- HTTPS to `.i2p` domains is blocked (eepsites are HTTP-only by design; transport security is at the I2P layer).
- Clearnet navigation through I2P outproxies is supported but intentionally slower.

### OSINT Workspace

Useful built-ins include:

- WHOIS
- DNS mapping
- archive lookup
- subdomain graphing
- metadata review
- social footprint hints
- timeline building

This feature set should remain clearly legal and user-directed.

### Tool Integration Layer

Veyra should be designed to integrate with external tools used by:

- red teams
- blue teams
- OSINT analysts
- incident responders
- malware analysts
- developers

The integration model should support:

- plugin-style adapters or service connectors
- persona-aware execution context
- route-aware execution context
- controlled artifact exchange
- permissioned command execution
- local service bridges instead of unrestricted process spawning
- audit logging for tool actions
- secure import and export of evidence and reports

The browser should not treat tool integration as an afterthought. It should be part of the platform design.

### Evidence and Artifact Handling

Evidence-safe handling is especially important for blue-team, incident-response, and malware-analysis workflows.

The platform should support:

- quarantined artifact intake
- metadata preservation where required for evidence workflows
- metadata stripping where required for operational privacy workflows
- explicit release paths instead of silent filesystem drops
- auditability for acquisition, inspection, and export actions
- persona-aware evidence storage boundaries

### Blue Team and SOC Workflows

Veyra should support workflows such as:

- IOC enrichment
- alert pivoting
- threat-intelligence browsing
- safe investigation sessions
- evidence review and export
- isolated review of suspicious URLs and documents
- integration with future SOC and case-management systems

### Developer Workspace

Veyra is not only a browser; it is also intended to become a security-oriented workspace.

The developer and operator workspace should eventually support:

- terminal views
- git context
- SSH session management
- API testing
- container-aware workflows
- documentation and code review surfaces
- AI-assisted coding and explanation

## MVP Scope

**The core browser MVP is complete** (foundation Phases 0–5 delivered).

What is delivered:

- native desktop browser shell (WebKitGTK, Linux-first) `[DONE]`
- persona-based storage partitions (persistent and ephemeral) `[DONE]`
- security mode switching (Casual, Hardened, Ghost, Red Team, Airgap) `[DONE]`
- route presets (direct, VPN, Tor, I2P, chained) `[DONE]`
- artifact quarantine (BlackVault with static heuristics and controlled release) `[DONE]`
- permission broker for all sensitive permissions `[DONE]`

What still belongs to the post-MVP track:

- phishing and download warnings with AI-powered analysis
- basic AI assistance (local LLM)
- basic OSINT workspace
- initial tool-integration boundaries
- React control-plane UI

## Post-MVP Phases

### Immediate Next: Hardening Track

- anti-fingerprint engine (canvas, audio, GPU, font, screen, timezone) — **Phase 6 NEXT**
- deeper download scanning (YARA, entropy, macro, EXIF) — Phase 5 follow-up
- persona-specific extension policies — Phase 8
- deeper permission mediation and centralized prompt management

### UI and Intelligence Track

- React control-plane UI (command palette, persona manager, route manager, vault monitor) — Phase 9
- local LLM orchestration with Ollama integration and persona-scoped memory — Phase 10
- OSINT workspace (WHOIS, DNS map, archive lookup, timeline builder) — Phase 11

### Operational Track

- Blue Team workspace: IOC enrichment, alert pivoting, evidence review — Phase 12
- Developer workspace: terminal, git, SSH, API testing — Phase 13
- Enterprise: Go-based control plane, fleet management, SOC connectors

### Research Track

- browser-core hardening layer and deeper network stack control
- enterprise policy distribution and secure sync
- microVM-backed browsing for selected workflows
- stronger hardware-backed secrets
- post-quantum experiments
- decentralized identity support

## Platform Considerations

Linux is the most realistic first platform for deep isolation research because it provides better access to:

- namespaces
- seccomp
- KVM
- container primitives

Windows and macOS can still matter commercially, but the deepest isolation experiments will be more complex there.

## UI Direction

The visual identity can absolutely lean into:

- dark cyberpunk
- glassmorphism
- neon minimalism
- floating panels
- command palette workflows
- modular dashboards

But the UI should still read as a credible professional security product, not only as a style exercise.

## Messaging

Strong candidate slogans:

- Not Just A Browser - A Cyber Operating System.
- Your Identity. Your Network. Your Control.
- The Browser Built For The AI Era.

## Final Recommendation

If Veyra is going to feel real rather than theatrical, the strongest combination is:

- `C++` for the browser root
- `Rust` for security and routing services
- `TypeScript + React` for the control-plane UI
- `Python` for AI and OSINT orchestration
- `Go` later for enterprise infrastructure

That stack is credible, scalable, and aligned with the feature set Veyra is aiming for.
