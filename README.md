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
  - UI: TypeScript + React
  - Security and networking modules: Rust
  - AI layer: TypeScript and/or Python
  - Long-term engine path: Chromium-based hardened layer

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
