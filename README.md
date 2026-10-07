<p align="center">
  <img src="assets/branding/logo.png" alt="Veyra Logo" width="300"/>
</p>

```text
 █████╗ ██████╗ ███████╗██╗  ██╗███████╗ ██████╗ ██████╗  ██████╗ ███████╗
██╔══██╗██╔══██╗██╔════╝╚██╗██╔╝██╔════╝██╔═══██╗██╔══██╗██╔════╝ ██╔════╝
███████║██████╔╝█████╗   ╚███╔╝ █████╗  ██║   ██║██████╔╝██║  ███╗█████╗
██╔══██║██╔═══╝ ██╔══╝   ██╔██╗ ██╔══╝  ██║   ██║██╔══██╗██║   ██║██╔══╝
██║  ██║██║     ███████╗██╔╝ ██╗██║     ╚██████╔╝██║  ██║╚██████╔╝███████╗
╚═╝  ╚═╝╚═╝     ╚══════╝╚═╝  ╚═╝╚═╝      ╚═════╝ ╚═╝  ╚═╝ ╚═════╝ ╚══════╝

██╗   ██╗███████╗██╗   ██╗██████╗  █████╗ 
██║   ██║██╔════╝╚██╗ ██╔╝██╔══██╗██╔══██╗
██║   ██║█████╗   ╚████╔╝ ██████╔╝███████║
╚██╗ ██╔╝██╔══╝    ╚██╔╝  ██╔══██╗██╔══██║
 ╚████╔╝ ███████╗   ██║   ██║  ██║██║  ██║
  ╚═══╝  ╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝

Compartmentalized · Secure · Persona-Driven · Cyber OS
```

<p align="center">
  <img src="https://img.shields.io/badge/Stack-C%2B%2B%20%7C%20Rust%20%7C%20TS%20%7C%20Python-007ACC?style=for-the-badge">
  <img src="https://img.shields.io/badge/Architecture-Native%20Shell%20%7C%20Engine%20Integration-0d0d0d?style=for-the-badge">
  <img src="https://img.shields.io/badge/Isolation-Multi--Persona%20%7C%20Hardened%20Partitions-7000FF?style=for-the-badge">
  <img src="https://img.shields.io/badge/Networking-GhostNet%20%7C%20Tor%20%7C%20I2P%20%7C%20Proxy%20Chaining-00F2FF?style=for-the-badge">
  <img src="https://img.shields.io/badge/Security-Sentinel%20%7C%20BlackVault%20%7C%20Anti--Fingerprint-FF8A00?style=for-the-badge">
  <img src="https://img.shields.io/badge/Intelligence-ForgeAI%20%7C%20OSINT%20Workspace-059669?style=for-the-badge">
</p>

**ApexForge Veyra** is more than just a browser—it's a next-generation **Cyber Operating System** designed to bridge the gap between traditional browsing and high-stakes operational workflows. Built on a low-level native foundation of **C++** and **Rust**, Veyra provides deep, policy-driven compartmentalization that treats every browsing session as a distinct, isolated operational entity.

### 🛡️ Core Pillars of Veyra:

*   **Digital Personas**: Go beyond profiles. Veyra offers true isolation with dedicated partitions for *Work, Research, Red Team, Banking, and Disposable* sessions. Each persona maintains its own independent storage, cookie jars, and fingerprint policy.
*   **Sentinel Guard**: A proactive security layer that monitors your privacy posture, flags phishing indicators, and enforces strict permission brokering.
*   **GhostNet**: Advanced, route-aware networking with native support for *VPN, Tor, I2P, and residential proxy chaining*—visualized and controlled at the persona level.
*   **BlackVault**: A secure, quarantined staging area for intercepted downloads, providing metadata analysis and risk scoring before files ever touch your host system.
*   **ForgeAI**: A local-first AI orchestrator that assists with page summarization, script explanation, and OSINT collection without leaking your data to the cloud.

Veyra is engineered for security researchers, OSINT analysts, and developers who require a professional-grade command center for their digital identity and cyber operations.

# ApexForge Veyra

**Not Just A Browser - A Cyber Operating System.**

ApexForge Veyra is evolving along two tracks:

- a current Electron prototype for product exploration
- a new core-foundation track for a real browser architecture

This repository is not yet a full native browser implementation, but it now includes the first foundation scaffolding for that direction.

Full written product requirements and stack decisions are tracked in [docs/VEYRA_PRODUCT_SPEC.md](/mnt/zboth/ApexForge_Veyra/docs/VEYRA_PRODUCT_SPEC.md).
The core-first implementation plan is tracked in [docs/VEYRA_CORE_FOUNDATION_PLAN.md](/mnt/zboth/ApexForge_Veyra/docs/VEYRA_CORE_FOUNDATION_PLAN.md).
Phase 0 engine strategy freeze is tracked in [docs/TDR-0001-engine-strategy.md](/mnt/zboth/ApexForge_Veyra/docs/TDR-0001-engine-strategy.md).
Phase 0/1 milestone exits are tracked in [docs/PHASE0_PHASE1_MILESTONE_MAP.md](/mnt/zboth/ApexForge_Veyra/docs/PHASE0_PHASE1_MILESTONE_MAP.md).
Phase 2 implementation notes are tracked in [docs/PHASE2_IMPLEMENTATION_NOTES.md](/mnt/zboth/ApexForge_Veyra/docs/PHASE2_IMPLEMENTATION_NOTES.md).
Phase 3 implementation notes are tracked in [docs/PHASE3_IMPLEMENTATION_NOTES.md](/mnt/zboth/ApexForge_Veyra/docs/PHASE3_IMPLEMENTATION_NOTES.md).
Phase 4 implementation notes are tracked in [docs/PHASE4_IMPLEMENTATION_NOTES.md](/mnt/zboth/ApexForge_Veyra/docs/PHASE4_IMPLEMENTATION_NOTES.md).
Phase 5 implementation notes are tracked in [docs/PHASE5_IMPLEMENTATION_NOTES.md](/mnt/zboth/ApexForge_Veyra/docs/PHASE5_IMPLEMENTATION_NOTES.md).

## Repository Tracks

### 1. Prototype Track

The existing Electron app under [`src/`](/mnt/zboth/ApexForge_Veyra/src) remains useful for:

- UI exploration
- workflow demos
- persona and dashboard mockups
- product concept validation

Status:

- prototype-only
- non-production
- not the long-term engine path

### 2. Core Foundation Track

The new low-level foundation work starts under:

- [`core/`](/mnt/zboth/ApexForge_Veyra/core)
- [`schemas/`](/mnt/zboth/ApexForge_Veyra/schemas)

This track is where the real browser architecture begins:

- C++ browser shell scaffold
- formal persona, route, and security-mode schemas
- build-system setup for native work
- Linux-first native shell integration using WebKitGTK for Phase 1

## Product Direction

### 1. Digital Personas
- Real Identity, Anonymous, Work, Research, Banking, Red Team, and Disposable sessions
- Isolated cookie and storage partitions per persona
- Ghost-style ephemeral sessions for throwaway browsing

### 2. Security Modes
- Casual for everyday browsing
- Hardened for stricter privacy controls
- Ghost for in-memory sessions
- Red Team for isolated routing and higher scrutiny

### 3. Built-in Modules
- `Sentinel Guard`: privacy and browsing posture
- `GhostNet`: route and tunnel visualization
- `ForgeAI`: AI assistant workspace
- `BlackVault`: quarantined artifact destination
- `ShadowTabs`: compartment-focused persona model
- `SpecterScan`: download inspection concept
- `NebulaSync`: future encrypted sync concept

### 4. AI + Workspace
- Built-in assistant panel for page analysis and workflow guidance
- OSINT workspace for domain lookups and metadata-oriented flows
- A stepping stone toward autonomous browsing and agent orchestration

## Architecture

- Current prototype: Electron + HTML + CSS + JavaScript
- Future target:
  - UI and control plane: TypeScript + React
  - Security, routing, and artifact handling services: Rust
  - AI and automation: Python plus TypeScript integrations
  - Browser core: native C++ shell with a dedicated Veyra engine-integration layer

## Realistic Language Split

- `TypeScript + React`: dashboard UI, settings, command palette, AI workspace, persona management
- `JavaScript`: minimal prototype glue in the current Electron demo
- `Rust`: download scanning, routing control, isolation policy engine, secure storage helpers
- `Python`: local AI orchestration, OSINT tools, report generation, YARA- and model-facing workflows
- `C++`: browser shell, engine integration, sandbox hooks, WebRTC/WebGPU policy, fingerprint surfaces
- `Go`: enterprise sync, SOC connectors, fleet management, service APIs
- `C`: only for low-level helpers where platform APIs or legacy libraries require it

Using both React and Angular in the same desktop client is not recommended here. React is the better fit for this product direction because the UI will behave more like a command center than a traditional CRUD dashboard.

## Getting Started

### Prototype Prerequisites
- Node.js 18+
- npm 9+

### Run Prototype

```bash
npm install
npm start
```

### Core Foundation Prerequisites

- CMake 3.20+
- A C++17 compiler

### Build Core Scaffold

```bash
cmake -S . -B build/core
cmake --build build/core
./build/core/veyra_shell
```

Alternative (no CMake) native build/run:

```bash
npm run native:build
npm run native:run
```

Core smoke check:

```bash
./build/core/veyra_shell --smoke
npm run native:smoke
```

Phase 2 startup/session examples:

```bash
./build/core/veyra_shell --persona=anonymous --url=https://example.org --open-tab=https://kernel.org
./build/core/veyra_shell --smoke --persona=real_identity
```

Notes:

- Linux-first only in Phase 1.
- WebKitGTK development packages are required (`webkit2gtk-4.1` preferred, `webkit2gtk-4.0` fallback).
- Native shell flags:
  - `--smoke`
  - `--list-personas`
  - `--list-route-profiles`
  - `--list-routes`
  - `--persona=<persona_id>`
  - `--startup-route=<route_profile_id>`
  - `--hot-route-after-start=<route_profile_id>`
  - `--url=<startup_url>`
  - `--runtime-root=<path>`
  - `--open-tab=<url>` (repeatable)
  - `--route-override=<persona_id:route_profile_id>` (repeatable)
  - `--route-engine-bin=<path>`
  - `--artifact-scan-bin=<path>`
  - `--keep-ephemeral`
- `--smoke` runs foundation bootstrap checks and exits without keeping the native window open.

## Current Prototype Scope

This version demonstrates:
- branded browser shell
- persona switching
- security mode switching
- AI panel mock interactions
- OSINT workspace mock outputs
- routing and guard dashboards

It does not yet implement:
- real proxy or Tor routing
- microVM tab isolation
- full anti-fingerprinting
- malware scanning
- local LLM orchestration

## Current Core Foundation Scope

The native foundation currently includes:

- repository split toward a core-first architecture
- formal JSON schemas for personas, routes, and security modes
- schema-backed startup validation for seed foundation data
- an initial C++ runtime policy engine and permission-broker layer
- a Linux-first WebKitGTK-backed native shell runtime
- real native window creation and real page rendering
- native tab creation, activation, navigation, and back-forward flow
- startup persona loading via CLI (`--persona=...`)
- persistent and ephemeral session partition path lifecycle bootstrap
- per-session manifest generation inside partition roots
- runtime-wide `security-mode-registry.json` generation
- per-persona `security-policy.json` generation
- runtime-wide `route-state.json` generation
- runtime-wide `route-overrides.json` persistence
- runtime-wide `route-events.jsonl` audit logging
- hot route reconfiguration for the active WebKit session via CLI-driven runtime policy apply
- real policy enforcement for JavaScript, cookies, WebRTC/media capture, TLS strictness, and navigation isolation
- permission broker wiring for notifications, geolocation, clipboard, camera, microphone, fullscreen, and file chooser flows
- interactive GTK permission prompts for `prompt` decisions in the native shell
- session-scoped prompt decision caching per origin and permission
- disk-backed prompt decision persistence inside each session partition
- a branded Veyra permission sheet instead of the default GTK message box
- a Rust route-engine child service with startup IPC and per-persona route runtime states
- route-health, leak-status, DNS-resolver, and proxy endpoint reporting during startup
- persisted per-persona route overrides and startup route switching
- route status refresh for the startup persona and route listing output via CLI
- route-aware tab metadata in the runtime window model
- a Rust artifact-scan child service for hashing, metadata extraction, and static heuristics
- quarantined download interception inside the active WebKit shell
- per-session `downloads/quarantine`, `downloads/released`, and `downloads/reports` roots
- branded BlackVault release review before artifacts leave quarantine
- per-download JSON reporting and `downloads/events.jsonl` audit logging
- stale ephemeral session cleanup during next bootstrap
- ephemeral session cleanup on shutdown (configurable with `--keep-ephemeral`)
- **I2P garlic-routing network support** as an optional route type alongside Tor, VPN, and chained routes
- I2P eepsite (`.i2p` TLD) navigation gated behind an active I2P route persona
- I2P-specific proxy configuration via local I2P router HTTP proxy on `127.0.0.1:4444`
- `i2p_research` persona with ephemeral Ghost-mode session and proxy-bound DNS for I2P workflows
- **Fingerprint obfuscation engine (Phase 6)** — per-persona JavaScript injection at document-start
- Canvas pixel noise (subtle / aggressive) using seeded per-session deterministic PRNG
- WebGL vendor and renderer string override per persona profile
- AudioContext frequency-data noise injection
- Navigator property override: hardwareConcurrency, deviceMemory, platform
- Screen metrics normalization: width, height, colorDepth, devicePixelRatio
- Four shipped fingerprint profiles: `native_stable`, `balanced_stealth`, `low_noise_obfuscated`, `minimal_surface`
- Fingerprint script cleared and rebuilt on hot-route policy reconfigure
- Full validation of `fingerprint_profile_id` at startup with schema-backed seed data
- **Tool Integration Boundary (Phase 7)** — policy-controlled, audited external tool execution
- 7 pre-defined tools: WHOIS, dig, Nmap, curl, file, strings, openssl
- Persona + security mode policy evaluation before any tool executes (5-rule check)
- Rust `tool-bridge` child service with stdio IPC: timeout enforcement, output capture
- Tool output quarantine routing through BlackVault for high-risk tools (nmap, curl)
- Per-invocation JSONL audit logging to session events log
- CLI: `--invoke-tool=<tool_id>:<args>` and `--list-tools`
- **Extension Isolation (Phase 8)** — per-persona eval blocking, domain blocking, mixed content enforcement
- `trusted_daily`, `minimal_locked`, `strict_redteam` extension policies
- `eval`/`Function`/`setTimeout`/`setInterval` (string form) blocked at document-start for locked personas
- 26-domain block list for red_team persona (Google, Facebook, Twitter, ad networks)
- Mixed content blocked via WebKit settings for anonymous and red_team personas
- Domain blocking with subdomain-aware matching in navigation policy
- Third-party (cross-registrable-domain) iframe blocking for locked personas
- Extension policy script rebuilt on hot-route reconfigure
- **React UI Layer (Phase 9)** — `apps/shell-ui/` Vite+React+TypeScript control plane
- 6 panels: Personas, GhostNet Route, Sentinel Guard, BlackVault, Tool Bridge, Cortex AI
- Command palette (Ctrl+K) with policy-gated route/tool/AI commands
- Dashboard runs in an ephemeral WebKitWebView in a GtkPaned sidebar (`--shell-ui-path=`)
- Bidirectional C++↔React state bridge (`window.__VEYRA_STATE__` / `messageHandlers.veyra`)
- **AI Orchestrator (Phase 10)** — local-first AI sidecar (`agents/ai-orchestrator/`)
- Stdlib-only Python, JSONL over stdio, local Ollama via urllib (no pip deps)
- Page summarization, script explanation, and phishing detection (heuristic + model)
- Offline phishing heuristics work with no model; graceful degradation when Ollama is down
- Per-persona `ai_policy_id` (ai_full / ai_scoped / ai_restricted / ai_disabled) gates capabilities
- `ai_restricted` (red_team) never sends page content to the model
- CLI: `--ai-orchestrator-script=<script.py>` and `--ai-python=<interpreter>`
- **OSINT Workspace (Phase 11)** — route-aware investigation sidecar (`agents/osint-workspace/`)
- Stdlib-only Python with in-file SOCKS5 (Tor, remote DNS), HTTP-proxy CONNECT (I2P), and direct egress
- WHOIS, DNS (via DoH — no resolver leak), Wayback archive, and username-search modules
- Leak prevention: routed personas refuse every lookup unless their proxy is active
- Per-persona `osint_policy_id` (osint_full / osint_passive / osint_operational / osint_disabled)
- Investigation case (entities / relationships / timeline) accumulated and shown in the Recon panel
- CLI: `--osint-workspace-script=<script.py>`

It does not yet include:

- vault and scanning services
- centralized enterprise policy sync for prompt decisions
- live tunnel backends
- per-tab live route switching inside the active WebKit session
- YARA, macro, entropy, and EXIF follow-up scanners

## License

ISC
