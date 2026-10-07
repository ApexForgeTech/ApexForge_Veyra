# Veyra OSINT Workspace (Phase 11)

Route-aware, legal-by-default OSINT sidecar for the Veyra browser shell.

## Design

- **Standard library only.** No pip dependencies — minimal supply-chain surface.
- **Route-honouring.** Every network egress traverses the active persona's proxy:
  - **Tor** (`socks5://`) via a minimal in-file SOCKS5 client with **remote DNS**
    (`ATYP=domain`) so hostnames are resolved by Tor, never leaking locally.
  - **I2P / HTTP proxy** (`http://`) via absolute-form GET and `CONNECT` tunnels.
  - **direct** only when the persona's policy explicitly permits it.
- **Leak prevention.** When `require_route` is set and no usable proxy is active,
  every lookup is **refused** rather than silently going out the ISP.
- **DNS via DoH.** DNS is resolved through DNS-over-HTTPS (Cloudflare JSON API)
  *through the route*, so DNS questions never hit the system resolver.
- **Privacy.** Request content and results are never logged to disk or stderr.

## Protocol

Newline-delimited JSON over stdio (one object per line).

```
→ {"id":"1","method":"dns_lookup","params":{"target":"example.com","route":{"proxy_uri":"socks5://127.0.0.1:9050","route_type":"tor"},"policy":{"enabled":true,"require_route":true,"allow_dns":true}}}
← {"id":"1","ok":true,"result":{"entities":[...],"relationships":[...],"timeline":[...],"summary":"...","raw":"..."}}
```

### Methods

| method | params | result |
|---|---|---|
| `ping` | route, policy | `{ok_route, route_type, egress}` |
| `whois_lookup` | target, route, policy | lookup result |
| `dns_lookup` | target, route, policy | lookup result (A/AAAA/MX/NS/TXT via DoH) |
| `archive_lookup` | target, route, policy | lookup result (Wayback CDX) |
| `username_search` | target, route, policy | lookup result (curated site set) |
| `shutdown` | — | `{bye:true}` |

### Lookup result shape

```json
{
  "entities":      [{"type":"domain|ip|email|username|url|org|dns_record", "value":"...", "source":"...", "attributes":{}}],
  "relationships": [{"from":"...", "to":"...", "kind":"..."}],
  "timeline":      [{"ts_ms":0, "event":"...", "detail":"..."}],
  "summary":       "...",
  "raw":           "..."
}
```

The C++ side merges these into an investigation **case** (entities deduped by
type+value, relationships by from+to+kind, timeline appended).

## Modules

- **WHOIS** — TCP :43 to `whois.iana.org`, follows the `refer:` line to the
  registry whois. Through SOCKS5 / CONNECT per route.
- **DNS** — A/AAAA/MX/NS/TXT via Cloudflare DoH JSON API through the route.
- **Archive** — Wayback Machine CDX API (`web.archive.org/cdx/search/cdx`).
- **Username search** — probes a curated set of ~15 sites (GitHub, GitLab,
  Reddit, Mastodon, Keybase, …); records 200-OK profiles. Capped by
  `max_username_sites`.

## Security model

Two enforcement layers, both must pass:

1. **C++ `OsintWorkspaceClient`** rejects disallowed modules before spawning.
2. **Python `ensure_egress_allowed`** refuses any lookup that would leave the
   required route, and `guarded()` re-checks the per-module policy flag.

## Manual test

```bash
# Leak guard (require_route + direct => refused)
echo '{"id":"1","method":"dns_lookup","params":{"target":"example.com","route":{"proxy_uri":"","route_type":"direct"},"policy":{"enabled":true,"require_route":true,"allow_dns":true}}}' \
  | python3 veyra_osint_workspace.py --stdio

# WHOIS over Tor (requires a local Tor SOCKS proxy on 9050)
echo '{"id":"2","method":"whois_lookup","params":{"target":"example.com","route":{"proxy_uri":"socks5://127.0.0.1:9050","route_type":"tor"},"policy":{"enabled":true,"require_route":true,"allow_whois":true}}}' \
  | python3 veyra_osint_workspace.py --stdio
```

## Legal / scope note

OSINT modules are **legal, user-directed** lookups against public data sources
(WHOIS, DNS, public archives, public profile pages). No authentication bypass,
no scraping behind logins, no active exploitation.
