# Phase 11 Implementation Notes

## Scope

Phase 11 delivers the route-aware OSINT investigation workspace for Veyra.

This phase completes:

- Standard-library-only Python OSINT sidecar (`agents/osint-workspace/`)
- Route-honouring network layer: SOCKS5 (Tor, remote DNS), HTTP proxy (I2P), direct
- Leak prevention: lookups refused when `require_route` and no proxy is active
- DNS via DoH (Cloudflare JSON) through the route — no system-resolver leak
- WHOIS, DNS, archive (Wayback), and username-search modules
- Per-persona `osint_policy_id` controlling modules + routing
- C++ `OsintWorkspaceClient` over JSONL stdio (SIGPIPE-safe, poll-bounded)
- Investigation case model (entities / relationships / timeline) accumulated shell-side
- React `Recon` panel wired to the live workspace

## Architecture

```
React OsintPanel (Recon tab)
  │  postMessage({action:"osint_whois"|"osint_dns"|"osint_archive"|"osint_username"|"osint_clear", target})
  ▼
C++ on_dashboard_action (main.cc)
  ├─ resolve live route → OsintRouteContext{proxy_uri, route_type}  (route_service.FindByPersonaId)
  ├─ OsintWorkspaceClient(python, script, persona.osint_policy)
  ├─ merge result into OsintCaseSnapshot (dedup entities/relationships, append timeline)
  └─ push_dashboard_with_ai(...) → SerializeDashboardState(..., osint_case)
  ▼
OsintWorkspaceClient  (client-side policy gate)
  │  fork + execlp(python3, veyra_osint_workspace.py, --stdio)
  │  write: {"id","method","params":{target, route, policy}}\n + shutdown
  │  read:  {"id","ok","result":{entities,relationships,timeline,summary}}\n
  ▼
veyra_osint_workspace.py
  ├─ ensure_egress_allowed(route, policy)   ← LEAK GUARD
  ├─ guarded(per-module policy flag)         ← MODULE GATE
  └─ module via routed network:
       SOCKS5 (tor, remote DNS) | HTTP proxy (i2p) | direct
```

## OSINT Policy Model

Each persona references an `osint_policy_id`. Policies live in
`schemas/osint/default-osint-policies.json`:

| policy | enabled | require_route | whois | dns | archive | username | persona |
|---|---|---|---|---|---|---|---|
| `osint_full` | yes | no | yes | yes | yes | yes | real_identity |
| `osint_passive` | yes | **yes** | yes | yes | yes | no | anonymous, i2p_research |
| `osint_operational` | yes | **yes** | yes | yes | yes | yes | red_team |
| `osint_disabled` | no | yes | no | no | no | no | (airgap / future) |

`require_route: true` is the leak-prevention guarantee — for anonymous, I2P, and
red-team personas every lookup is refused unless the persona's proxy is active.

## Route-honouring network layer (Python)

- **SOCKS5** (`socks5_connect`): minimal stdlib SOCKS5 CONNECT with `ATYP=0x03`
  (domain), so Tor resolves the hostname — no local DNS leak.
- **HTTP proxy** (`http_connect_tunnel` + absolute-form GET): for I2P / HTTP proxies.
- **DoH** for DNS: `https://cloudflare-dns.com/dns-query` with
  `Accept: application/dns-json`, sent through the route's HTTPS path.
- **WHOIS**: TCP :43 via the route (SOCKS5 or CONNECT), follows `refer:`.

A raw HTTP/1.1 client (`https_request`) handles chunked transfer-encoding and
TLS (SNI) over whichever socket the route produced.

## Investigation Case

The shell owns the case (`OsintCaseSnapshot` in `main.cc`, RunBootstrap scope):

- **entities** deduped by `(type, value)`
- **relationships** deduped by `(from, to, kind)`
- **timeline** appended, sorted newest-first in the UI

Serialized into dashboard state as `osint_case`; the React panel renders entities,
a timeline, the last lookup status, and the egress posture (routed / direct / blocked).

## Building & Running

```bash
cd apps/shell-ui && npm install && npm run build

# Optional: a local Tor SOCKS proxy on 127.0.0.1:9050 for the routed personas
./build/.../veyra_shell \
  --persona=anonymous \
  --shell-ui-path=apps/shell-ui/dist \
  --osint-workspace-script=build/.../veyra_osint_workspace.py
```

CMake stages `veyra_osint_workspace.py` next to the shell binary automatically.

## Verification performed

- Full C++ shell compiles and links against WebKitGTK 2.50.6
- React app builds clean (`tsc && vite build`, 41 modules, 0 errors)
- Python orchestrator: leak guard refuses `require_route + direct`; per-module
  policy flags enforced; raw HTTP client + chunked decode verified against a
  local HTTP server (200, parsed JSON)
- C++ `OsintWorkspaceClient` end-to-end: leak guard (`policy_denied`), username
  gate, and ping (`egress=direct`) all correct; process survives a child that
  exec-fails (no SIGPIPE death)
- Runtime smoke: all personas load (osint_policy_id parsed + reference-validated)

## Known Limits

- External lookups (WHOIS/DNS/archive/username) require outbound internet, which
  the dev sandbox lacks; the network code is verified against localhost and the
  leak/policy gates are verified end-to-end.
- Synchronous lookups block the GTK main loop (bounded by a 120 s poll timeout);
  username fan-out across many sites can take seconds. A future revision should
  move lookups to a worker thread.
- Report export (Markdown/evidence packaging) is not yet wired; the case model
  and serialization are in place for it.
- Username search uses a curated ~15-site list; it is not an exhaustive enumerator.
- `retain_cases` is modelled but case persistence to disk is not yet implemented.
