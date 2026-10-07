# Phase 10 Implementation Notes

## Scope

Phase 10 delivers the local-first AI orchestrator for Veyra.

This phase completes:

- Standard-library-only Python AI sidecar (`agents/ai-orchestrator/`)
- Local Ollama integration with offline graceful degradation
- Page summarization, phishing detection (heuristic + model), and script explanation
- Per-persona AI policy (`ai_policy_id`) controlling what AI may do
- C++ `AiOrchestratorClient` over JSONL stdio
- Async page-text extraction from the active tab
- React `Cortex AI` panel wired to the real orchestrator
- Privacy-first defaults: nothing leaves the tab unless the AI policy allows it

## Architecture

```
React AiPanel (Cortex AI tab)
  │  postMessage({action:"ai_summarize" | "ai_phishing_check" | "ai_explain_script"})
  ▼
C++ on_dashboard_action (main.cc)
  ├─ ai_phishing_check  → engine->GetActiveTabUrl()/GetActiveTabTitle()  (sync)
  ├─ ai_summarize       → engine->RequestActiveTabText(cb)               (async JS eval)
  └─ ai_explain_script  → action.payload (operator-provided)             (sync)
  ▼
AiOrchestratorClient  (enforces ai_policy: enabled / allow_page_content / allow_script_analysis)
  │  fork + execlp(python3, veyra_ai_orchestrator.py, --stdio)
  │  write: {"id","method","params"}\n  +  shutdown\n
  │  read:  {"id","ok","result"}\n
  ▼
veyra_ai_orchestrator.py
  ├─ phishing_check → offline heuristics (+ optional model note)
  ├─ summarize      → Ollama /api/generate (or offline fallback)
  └─ explain_script → Ollama /api/generate (or offline fallback)
  ▼
AiResultSnapshot → SerializeDashboardState(..., ai_result)
  ▼
engine->PushDashboardState(json) → window.__VEYRA_UPDATE__ → React re-render
```

## AI Policy Model

Each persona references an `ai_policy_id`. Policies live in
`schemas/ai/default-ai-policies.json`:

| policy | enabled | page_content | script | phishing | memory | personas |
|---|---|---|---|---|---|---|
| `ai_full` | yes | yes | yes | yes | retained | real_identity |
| `ai_scoped` | yes | yes | yes | yes | ephemeral | anonymous, i2p_research |
| `ai_restricted` | yes | **no** | yes | yes | ephemeral | red_team |
| `ai_disabled` | no | no | no | no | — | (airgap / future) |

Enforcement happens in two layers:

1. **C++ `AiOrchestratorClient`** rejects `Summarize`/`ExplainScript`/`PhishingCheck`
   before any content is sent if the policy forbids it (`policy_denied`).
2. **React `AiPanel`** disables the corresponding buttons so denied actions are
   never offered.

`ai_restricted` (red team) is the key privacy posture: `allow_page_content=false`
means page text is **never** sent to the model — only operator-pasted scripts and
URL-based phishing heuristics are processed.

## Orchestrator Protocol (JSONL over stdio)

```
→ {"id":"sum","method":"summarize","params":{"text":"...","model":"llama3.2","endpoint":"http://127.0.0.1:11434","max_input_chars":16000}}
← {"id":"sum","ok":true,"result":{"summary":"...","source":"model"}}
```

| method | params | result |
|---|---|---|
| `ping` | endpoint, model | model_online, endpoint, model |
| `summarize` | text, model, endpoint, max_input_chars | summary, source |
| `phishing_check` | url, title, model, endpoint, allow_model | risk_level, score, signals[], explanation, source |
| `explain_script` | code, model, endpoint, max_input_chars | explanation, source |
| `shutdown` | — | bye |

The C++ client writes the request line followed by a `shutdown` line, then reads
the single response line — a per-call child process so a hung model never leaves
a zombie attached to the shell.

## Phishing Heuristics (offline)

`phishing_check` works without any model. Rule-based signals:

- IP-literal host, punycode/IDN (`xn--`), `@` in URL
- Deep subdomain nesting, many hyphens, suspicious TLD
- Brand keyword in subdomain (`paypal.evil.com`)
- Login form over plain HTTP, unusually long host

The heuristic score (0–100 → clean/low/medium/high) is authoritative; the model
may add a note but never lowers a high score.

## Building & Running

```bash
# React app
cd apps/shell-ui && npm install && npm run build

# Optional model (phishing works without it)
ollama pull llama3.2

# Launch with dashboard + AI
./build/.../veyra_shell \
  --persona=real_identity \
  --shell-ui-path=apps/shell-ui/dist \
  --ai-orchestrator-script=build/.../veyra_ai_orchestrator.py
```

CMake stages `veyra_ai_orchestrator.py` next to the shell binary automatically.

## Verification performed

- Full C++ shell compiles and links against WebKitGTK 2.50.6
- React app builds clean (`tsc && vite build`, 0 errors)
- Orchestrator phishing heuristic flags `http://paypal.secure-login.tk/verify` as
  high (70/100) with 3 signals; clean URL scores 0
- End-to-end summarize against a real local model returns a model-sourced summary
- Offline path verified: missing model → `[Model error] 404` with `source:offline`,
  no crash

## Known Limits

- The synchronous AI call blocks the GTK main loop during inference. For fast
  phishing heuristics this is imperceptible; for a large summarization on a slow
  model the dashboard can freeze briefly. A future revision should run the AI
  call on a worker thread and post the result back to the main loop.
- `webkit_web_view_evaluate_javascript` / `..._finish` require WebKitGTK ≥ 2.40.
- AI "memory" (`retain_memory`) is modelled in policy but not yet persisted; no
  conversation store is implemented in this phase.
- Default model id is `llama3.2`; if not pulled, AI degrades to offline mode.
