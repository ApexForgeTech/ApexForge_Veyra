# Phase 7 Implementation Notes

## Scope

Phase 7 delivers the tool integration boundary for Veyra.

This phase completes:

- tool definition schema and seed data (7 pre-defined tools across 4 categories)
- `ToolDefinition` model loaded as part of `FoundationState` at startup
- unique ID validation for tool definitions in the startup registry
- a Rust tool-bridge child service with stdio IPC (same pattern as route-engine and artifact-scan)
- a C++ `ToolBridgeClient` with policy evaluation, invocation, quarantine routing, and audit logging
- persona + security mode policy evaluation before any tool is allowed to execute
- per-invocation audit logging to the session's `events.jsonl`
- tool output quarantine routing for high-risk tools
- CLI flag `--invoke-tool=<tool_id>:<arg0>:<arg1>...` for operator-driven tool invocation
- CLI flag `--list-tools` to enumerate available tools
- `RegistrySummary.tool_count` and bootstrap output line for tool definitions loaded

## Architecture

```
CLI --invoke-tool=whois:example.com
         ↓
main.cc → ToolBridgeClient
         ↓
CheckPolicy()       — persona + security mode evaluation
         ↓
WriteAuditEvent()   — "invoked" log entry
         ↓
Start() → fork/execl → tool_bridge --stdio
         ↓
SendLine("INVOKE\t...") → Rust tool-bridge
         ↓
Rust: spawn binary, capture stdout/stderr, enforce timeout
         ↓
C++: receive STARTED / OUTPUT / COMPLETED lines
         ↓
WriteOutputToQuarantine()  — if output_to_quarantine=true
         ↓
WriteAuditEvent()   — "completed" or "failed" log entry
         ↓
Result returned to caller
```

## Tool Definitions

### `whois` (OSINT)
- Binary: `/usr/bin/whois`
- Allowed personas: `anonymous`, `red_team`, `i2p_research`
- Denied personas: `real_identity`
- Denied modes: `casual`
- Output: not quarantined (plain text lookup result)
- Audit: standard

### `dig` (OSINT)
- Binary: `/usr/bin/dig`
- Allowed personas: any
- Denied modes: `casual`
- Output: not quarantined
- Audit: standard

### `nmap-quick` (Network Scan)
- Binary: `/usr/bin/nmap`
- Allowed personas: `red_team` only
- Denied modes: `casual`, `airgap`
- Output: quarantined (scan results go to BlackVault)
- Audit: full
- Max runtime: 120 seconds

### `curl-fetch` (Web)
- Binary: `/usr/bin/curl`
- Allowed personas: `anonymous`, `red_team`, `i2p_research`
- Denied personas: `real_identity`
- Output: quarantined
- Audit: full
- Max runtime: 60 seconds

### `file-inspect` (Forensics)
- Binary: `/usr/bin/file`
- Allowed personas: any
- Denied modes: none
- Output: not quarantined
- Audit: minimal

### `strings-extract` (Forensics)
- Binary: `/usr/bin/strings`
- Allowed personas: any
- Denied modes: none
- Output: not quarantined
- Audit: standard

### `openssl-x509` (Crypto)
- Binary: `/usr/bin/openssl`
- Allowed personas: `anonymous`, `red_team`
- Denied modes: `casual`
- Output: not quarantined
- Audit: standard

## Policy Evaluation

The policy checks fire in this order:

1. Tool ID must exist in the registry. Otherwise: `denied`.
2. If `allowed_personas` is non-empty: persona must be in the list. Otherwise: `denied`.
3. If persona is in `denied_personas`: `denied`.
4. If current security mode is in `denied_security_modes`: `denied`.
5. If `requires_network=true` and `route_type=direct` and security mode is `ghost` or `redteam`: `denied`.

All denials are audit-logged with the reason.

## IPC Protocol

### Shell → Rust

```
INVOKE\t<invocation_id>\t<binary>\t<max_seconds>\t<capture:1/0>\t[arg0]\t[arg1]...
STATUS\t<invocation_id>
CANCEL\t<invocation_id>
SHUTDOWN
```

### Rust → Shell

```
STARTED\t<invocation_id>\t<pid>
OUTPUT\t<invocation_id>\t<stdout|stderr>\t<line_content>
COMPLETED\t<invocation_id>\t<exit_code>
FAILED\t<invocation_id>\t<reason>\t<detail>
RUNNING\t<invocation_id>\t<elapsed_seconds>
OK\t<count>
ERROR\t<message>
```

## Audit Log Format

Each tool lifecycle event produces one JSONL line in the session's `events.jsonl`:

```json
{"event_type":"tool:invoked","invocation_id":"inv-1-whois","tool_id":"whois","persona_id":"anonymous","security_mode":"ghost","route_type":"tor","timestamp_ms":1717430000000,"args":["example.com"]}
{"event_type":"tool:completed","invocation_id":"inv-1-whois","tool_id":"whois","persona_id":"anonymous","security_mode":"ghost","route_type":"tor","timestamp_ms":1717430001823,"exit_code":0,"timed_out":false,"output_lines":12}
```

Policy denials:
```json
{"event_type":"tool:denied","invocation_id":"inv-2-nmap-quick","tool_id":"nmap-quick","persona_id":"real_identity","security_mode":"casual","route_type":"direct","timestamp_ms":...,"reason":"Tool 'nmap-quick' is not allowed for persona 'real_identity'."}
```

## Output Quarantine

When `output_to_quarantine: true`, tool stdout/stderr is written to:

```
<session_root>/downloads/quarantine/tool-<invocation_id>/output.txt
```

This path is returned in `ToolInvocationResult.quarantine_path`. The Phase 5 BlackVault release flow can optionally be applied to this output file in a future sub-phase.

## CLI Usage

```bash
# List available tools
./build/core/veyra_shell --persona=anonymous --list-tools

# Invoke a tool
./build/core/veyra_shell --persona=anonymous --invoke-tool=whois:example.com

# Invoke with multiple args (colon-separated)
./build/core/veyra_shell --persona=red_team --invoke-tool=dig:example.com:+short:A

# Tool invocation then exit (no browser window)
./build/core/veyra_shell --smoke --persona=anonymous --invoke-tool=whois:example.com
```

## Known Limits

Phase 7 is the first usable tool boundary, not the final tool execution architecture.

Current limits:

- tool invocation is CLI-driven; there is no GUI tool launcher yet (that belongs to Phase 9 React UI)
- `output_to_quarantine` routes to a directory but does not trigger the BlackVault scan + review dialog automatically (manual integration point for a future sub-phase)
- the Rust tool-bridge captures stdout/stderr line-by-line; very large outputs (multi-MB) may be slow to pass through the stdio pipe
- `--invoke-tool` exits after execution; it does not open a browser window alongside tool invocation (concurrent tool + browse is a Phase 9 workflow)
- no sandboxing of the child tool process (no seccomp, no namespace isolation); this is a Phase 14 concern
- tool arguments are colon-separated in CLI spec, which prevents using `:` in argument values; the `ToolInvocationRequest.args` vector supports arbitrary strings at the API level
