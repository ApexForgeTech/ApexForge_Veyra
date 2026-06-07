# Phase 9 Implementation Notes

## Scope

Phase 9 delivers the React control-plane UI for Veyra.

This phase completes:

- TypeScript + React + Vite app under `apps/shell-ui/`
- 5 operational panels: Personas, GhostNet Route, Sentinel Guard, BlackVault, Tool Bridge
- Command palette (Ctrl+K) with fuzzy-search across all shell actions
- Dark cyberpunk CSS theme (no external CSS frameworks)
- Full TypeScript type definitions for `VeyraState` and all sub-types
- C++ `shell_ui_bridge.cc`: state serialization, action parsing, state injection script
- `WebKitGtkBrowserEngine`: GtkPaned sidebar, ephemeral dashboard WebView, `veyra` message handler
- `PushDashboardState` — pushes JSON state to dashboard after C++ actions complete
- `on_dashboard_action` callback for bidirectional control from React → C++
- `--shell-ui-path=<dist/>` CLI flag
- CMake npm build integration

## Architecture

```
React App (apps/shell-ui/dist/index.html)
  ↑ loaded as file:// in GtkPaned left pane
  ↓ reads window.__VEYRA_STATE__ at mount

C++ Shell → BuildStateInjectionScript()
  → webkit_user_script injected at DOCUMENT_START
  → sets window.__VEYRA_STATE__ before React boots

React → window.webkit.messageHandlers.veyra.postMessage(action)
  → WebKitUserContentManager "script-message-received::veyra"
  → OnDashboardMessage() → config_.on_dashboard_action(json)
  → main.cc: switch_route / invoke_tool / open_tab / request_state_refresh

After action:
C++ → SerializeDashboardState() → PushDashboardState(json)
  → webkit_web_view_evaluate_javascript: window.__VEYRA_UPDATE__(newState)
  → React re-renders with updated data
```

## React Panel Descriptions

### Personas Panel
Shows all loaded personas with active indicator, security mode badge, route type icon,
ephemeral status. Active persona detail card: route, fingerprint, extension, storage type.
Refresh and New Tab buttons (when Veyra-hosted).

### GhostNet Route Panel
Active route card: name, health dot, route type, leak status badge, DNS resolver, proxy URI.
Diagnostic summary. Route switch selector + Apply button (posts `switch_route` action).

### Sentinel Guard Panel
Security mode card. Full permission broker table (8 permissions, allow/prompt/deny badges).
Fingerprint profile. Extension policy: eval status, mixed content status, blocked domains count.

### BlackVault Panel
Recent quarantine events (last 20, most recent first). Event type badge + artifact ID + detail.
Empty state when no events.

### Tool Bridge Panel
All tools with allowed/denied status for the active persona. Expand to show argument input
and Invoke button. Denial reason shown for denied tools. Posts `invoke_tool` action.

### Command Palette (Ctrl+K)
Keyboard-driven. All route profiles as switch commands. All allowed tools as invoke commands.
Open new tab, refresh state. Arrow key navigation, Enter to execute, Escape to close.

## State Serialization

`SerializeDashboardState` produces a JSON object containing:

- `active_persona` — full persona fields
- `all_personas` — all loaded personas with active flag
- `active_route` — live route state from `RouteServiceClient`
- `all_route_profiles` — all route profile definitions for the switch UI
- `security_mode_id`, `security_mode_name`
- `fingerprint_profile_id`, `extension_policy_id`
- `allow_eval`, `block_mixed_content`, `blocked_domains_count`
- `permissions` — full permission broker decision table
- `vault_events` — last 30 lines from `events.jsonl` (raw JSONL embedded as array)
- `tools` — all tool definitions with per-persona policy check results
- `runtime_root`, `shell_version`

## Communication Protocol

### React → Shell

All messages are JSON objects posted via `window.webkit.messageHandlers.veyra.postMessage(msg)`.

| action | payload | effect |
|---|---|---|
| `switch_route` | `{route_profile_id: string}` | Hot-routes the active persona, pushes new state |
| `invoke_tool` | `{tool_id: string, args: string[]}` | Invokes tool via ToolBridgeClient, logs to events.jsonl |
| `open_tab` | `{url: string}` | Opens new tab in the browser content area |
| `request_state_refresh` | — | Pushes current state to dashboard without any action |

### Shell → React

C++ calls `window.__VEYRA_UPDATE__(stateObj)` via `webkit_web_view_evaluate_javascript`.
This triggers React to call `window.__veyra_react_update__(stateObj)` if registered by
the App component.

## Building

```bash
# Install React app dependencies
cd apps/shell-ui && npm install

# Build the React app
npm run build
# Output: apps/shell-ui/dist/

# Launch with dashboard
./build/core/veyra_shell \
  --persona=anonymous \
  --shell-ui-path=apps/shell-ui/dist
```

CMake builds the React app automatically if `npm` is found.

## Known Limits

- Persona switching from the dashboard is NOT implemented (requires full session recreation).
  This is Phase 10 scope.
- The vault events panel embeds raw JSONL from events.jsonl. Mixed event types (download
  events, tool events) are shown together; per-type filtering is a Phase 10 enhancement.
- The AI panel placeholder exists in the React component structure but is not wired to
  the AI orchestrator (that is Phase 10 work).
- Dashboard WebView uses an ephemeral context so React DevTools and persistent storage
  are unavailable in the panel.
- `webkit_web_view_evaluate_javascript` (WebKit2GTK 2.40+ API) is used for `PushDashboardState`.
  On older WebKit2GTK (< 2.40), fall back to `webkit_web_view_run_javascript`.
