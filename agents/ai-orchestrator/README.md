# Veyra AI Orchestrator (Phase 10)

Local-first AI sidecar for the Veyra browser shell.

## Design

- **Standard library only.** No pip dependencies — minimal supply-chain surface.
- **Local-first.** Talks to a local [Ollama](https://ollama.com) server on
  `http://127.0.0.1:11434`. Never reaches the public internet on its own.
- **Graceful degradation.** If Ollama is offline, phishing checks still run via
  rule-based heuristics; summarize/explain return a clear "model offline" result.
- **Privacy.** Request content is never logged to disk or stderr.

## Protocol

Newline-delimited JSON over stdio (one object per line).

```
Request:  {"id": "req-1", "method": "summarize", "params": {"text": "...", "model": "llama3.2", "endpoint": "http://127.0.0.1:11434", "max_input_chars": 16000}}
Response: {"id": "req-1", "ok": true, "result": {"summary": "...", "source": "model"}}
```

### Methods

| method | params | result |
|---|---|---|
| `ping` | `{endpoint, model}` | `{model_online, endpoint, model}` |
| `summarize` | `{text, model, endpoint, max_input_chars}` | `{summary, source}` |
| `phishing_check` | `{url, title, model, endpoint, allow_model}` | `{risk_level, score, signals[], explanation, source}` |
| `explain_script` | `{code, model, endpoint, max_input_chars}` | `{explanation, source}` |
| `shutdown` | — | `{bye: true}` |

## Phishing heuristics (offline)

The `phishing_check` method runs a rule-based scan that works without any model:

- IP-literal host
- Punycode / IDN homograph (`xn--`)
- `@` symbol in URL (credential trick)
- Deep subdomain nesting (≥5 labels)
- Many hyphens in host (≥4)
- Suspicious TLD (`.zip`, `.xyz`, `.tk`, …)
- Brand keyword in subdomain (e.g. `paypal.evil.com`)
- Login form over plain HTTP
- Unusually long host

The heuristic score is authoritative; a model opinion can add a note but never
lowers a high heuristic score.

## Prerequisites

Optional — only needed for summarization and script explanation:

```bash
# Install Ollama, then pull a small model
ollama pull llama3.2
```

Phishing checks work without Ollama.

## Manual test

```bash
echo '{"id":"1","method":"ping","params":{"endpoint":"http://127.0.0.1:11434","model":"llama3.2"}}' \
  | python3 veyra_ai_orchestrator.py --stdio

echo '{"id":"2","method":"phishing_check","params":{"url":"http://paypal.secure-login.tk/verify","title":"Sign in"}}' \
  | python3 veyra_ai_orchestrator.py --stdio
```
