# ApexForge Veyra

**Not Just A Browser - A Cyber Operating System.**

ApexForge Veyra is a hardened browser workspace concept for daily browsing, privacy-first research, developer workflows, OSINT, and AI-assisted analysis. This repository is an Electron prototype for the product direction, not a custom browser engine.

Full written product requirements and stack decisions are tracked in [docs/VEYRA_PRODUCT_SPEC.md](/mnt/zboth/ApexForge_Veyra/docs/VEYRA_PRODUCT_SPEC.md).

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
  - Browser core: Chromium base with a hardened custom layer in C++

## Realistic Language Split

- `TypeScript + React`: dashboard UI, settings, command palette, AI workspace, persona management
- `JavaScript`: minimal prototype glue in the current Electron demo
- `Rust`: download scanning, routing control, isolation policy engine, secure storage helpers
- `Python`: local AI orchestration, OSINT tools, report generation, YARA- and model-facing workflows
- `C++`: Chromium-level hardening, sandbox hooks, WebRTC/WebGPU policy, fingerprint surfaces
- `Go`: enterprise sync, SOC connectors, fleet management, service APIs
- `C`: only for low-level helpers where platform APIs or legacy libraries require it

Using both React and Angular in the same desktop client is not recommended here. React is the better fit for this product direction because the UI will behave more like a command center than a traditional CRUD dashboard.

## Getting Started

### Prerequisites
- Node.js 18+
- npm 9+

### Installation

```bash
npm install
npm start
```

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

## License

ISC
