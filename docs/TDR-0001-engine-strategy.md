# TDR-0001: Phase 0 Engine Strategy Freeze

## Status

Accepted — Phase 0 and Phase 1 complete.

## Date

2026-05-22

## Context

Veyra's direction is a native browser shell with independent product identity.
The existing Electron app remains useful for UI exploration but is not the long-term production core.

Phase 1 requires a concrete backend decision so the team can deliver a real rendered native shell,
window lifecycle, tab lifecycle, and navigation behavior without ambiguity.

## Decision

For Phase 1, Veyra uses a Linux-first WebKitGTK integration strategy.

Hard rules for Phase 1:

- Chromium-based backends are not used.
- Gecko/Firefox-based backends are not used.
- The production implementation track is `core/`.
- The Electron track in `src/` remains prototype-only and non-production.

## Rationale

- Keeps product identity independent from upstream browser branding.
- Delivers real rendering and tab behavior without building a browser engine from scratch.
- Aligns with Linux-first deep-isolation research posture in the broader roadmap.
- Reduces decision churn so implementation can proceed immediately.
- WebKitGTK's network proxy API supports the full range of required routing backends including SOCKS5 (Tor, VPN, chained) and HTTP proxy (I2P) without external patches.

## Consequences

Positive:

- Phase 1 scope is decision-complete and implementation-ready.
- Build and runtime boundaries can be standardized around one backend.
- The team can focus on shell, policy, and runtime model integration.

Tradeoffs:

- Platform support is intentionally narrowed to Linux-first in this phase.
- Future backend expansion requires explicit follow-up TDRs and migration planning.

## Acceptance Criteria

Phase 0 is complete when all of the following are true:

- This TDR is merged. `[DONE]`
- Core plan documents reflect the locked strategy. `[DONE]`
- Repository tracks are clearly labeled as prototype vs production. `[DONE]`

Phase 1 is complete when all of the following are true:

- Native window renders real web content. `[DONE]`
- Tab creation works. `[DONE]`
- Navigation works. `[DONE]`
- Back/forward works. `[DONE]`
- Startup validation still gates engine launch (fail-fast on invalid foundation state). `[DONE]`
