#!/usr/bin/env python3
"""
Veyra AI Orchestrator (Phase 10)

A local-first AI sidecar for the Veyra browser shell. Runs as a stdio child
process and speaks newline-delimited JSON (JSONL): one request object per line
in, one response object per line out.

Design constraints:
  - Standard library only. No pip dependencies (minimal supply chain surface).
  - Local-first: talks to a local Ollama server. Never reaches the public
    internet on its own.
  - Graceful degradation: if Ollama is unreachable, phishing checks still work
    via rule-based heuristics, and other methods return a clear "model offline"
    result instead of failing.
  - Privacy: request content is never logged to disk or stderr.

Protocol:
  Request:  {"id": "<str>", "method": "<str>", "params": { ... }}
  Response: {"id": "<str>", "ok": true,  "result": { ... }}
            {"id": "<str>", "ok": false, "error": "<str>"}

Methods:
  ping            -> { "model_online": bool, "endpoint": str, "model": str }
  summarize       params: {text, model, endpoint, max_input_chars}
                  -> { "summary": str, "source": "model"|"offline" }
  phishing_check  params: {url, title, model, endpoint, allow_model}
                  -> { "risk_level": str, "score": int, "signals": [str],
                       "explanation": str, "source": "heuristic"|"model" }
  explain_script  params: {code, model, endpoint, max_input_chars}
                  -> { "explanation": str, "source": "model"|"offline" }
"""

import sys
import json
import urllib.request
import urllib.error
from urllib.parse import urlparse


def ollama_generate(endpoint, model, prompt, timeout=60):
    """POST to Ollama /api/generate. Returns the generated text or raises."""
    url = endpoint.rstrip("/") + "/api/generate"
    payload = json.dumps({
        "model": model,
        "prompt": prompt,
        "stream": False,
        "options": {"temperature": 0.2},
    }).encode("utf-8")

    request = urllib.request.Request(
        url, data=payload, headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(request, timeout=timeout) as response:
        body = response.read().decode("utf-8")
    parsed = json.loads(body)
    return parsed.get("response", "").strip()


def ollama_available(endpoint, timeout=3):
    """Quick health probe against the Ollama server."""
    try:
        url = endpoint.rstrip("/") + "/api/tags"
        request = urllib.request.Request(url, method="GET")
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return response.status == 200
    except Exception:
        return False


def clamp(text, max_chars):
    if max_chars and len(text) > max_chars:
        return text[:max_chars]
    return text


# ── Phishing heuristics (work fully offline) ──────────────────────────

SUSPICIOUS_TLDS = {
    "zip", "mov", "xyz", "top", "tk", "ml", "ga", "cf", "gq", "work",
    "click", "link", "country", "kim", "loan", "men", "review",
}

BRAND_KEYWORDS = [
    "paypal", "apple", "microsoft", "google", "amazon", "facebook",
    "netflix", "bank", "secure", "login", "signin", "verify", "account",
    "wallet", "coinbase", "binance", "metamask",
]


def phishing_heuristics(url, title):
    signals = []
    score = 0

    parsed = urlparse(url if "://" in url else "http://" + url)
    host = (parsed.hostname or "").lower()
    scheme = (parsed.scheme or "").lower()

    if not host:
        return {"risk_level": "unknown", "score": 0, "signals": ["no_host"],
                "explanation": "No host could be parsed from the URL."}

    # IP-literal host
    is_ip = host.replace(".", "").isdigit() or (":" in host)
    if is_ip:
        signals.append("ip_literal_host")
        score += 25

    # Punycode / IDN homograph
    if "xn--" in host:
        signals.append("punycode_host")
        score += 30

    # @ in URL (credential-in-url trick)
    if "@" in url:
        signals.append("at_symbol_in_url")
        score += 25

    # Excessive subdomain depth
    labels = host.split(".")
    if len(labels) >= 5:
        signals.append("deep_subdomain_nesting")
        score += 15

    # Many hyphens in host
    if host.count("-") >= 4:
        signals.append("many_hyphens")
        score += 10

    # Suspicious TLD
    tld = labels[-1] if labels else ""
    if tld in SUSPICIOUS_TLDS:
        signals.append("suspicious_tld:" + tld)
        score += 20

    # Brand keyword in a subdomain but not the registrable domain
    registrable = ".".join(labels[-2:]) if len(labels) >= 2 else host
    subdomain_part = host[:-len(registrable)] if host.endswith(registrable) else host
    for brand in BRAND_KEYWORDS:
        if brand in subdomain_part:
            signals.append("brand_in_subdomain:" + brand)
            score += 20
            break

    # Login-like page over plain HTTP
    login_hint = any(k in (title or "").lower() or k in url.lower()
                     for k in ("login", "sign in", "signin", "verify", "password"))
    if login_hint and scheme == "http":
        signals.append("login_form_without_https")
        score += 30

    # Overly long host
    if len(host) > 40:
        signals.append("unusually_long_host")
        score += 10

    score = min(score, 100)
    if score >= 60:
        level = "high"
    elif score >= 30:
        level = "medium"
    elif score > 0:
        level = "low"
    else:
        level = "clean"

    explanation = (
        "Heuristic scan flagged {} signal(s): {}.".format(len(signals), ", ".join(signals))
        if signals else "No phishing heuristics triggered for this URL."
    )

    return {"risk_level": level, "score": score, "signals": signals,
            "explanation": explanation}


# ── Method handlers ───────────────────────────────────────────────────

def handle_ping(params):
    endpoint = params.get("endpoint", "http://127.0.0.1:11434")
    model = params.get("model", "llama3.2")
    return {"model_online": ollama_available(endpoint),
            "endpoint": endpoint, "model": model}


def handle_summarize(params):
    text = clamp(params.get("text", ""), params.get("max_input_chars", 16000))
    endpoint = params.get("endpoint", "http://127.0.0.1:11434")
    model = params.get("model", "llama3.2")

    if not text.strip():
        return {"summary": "The page has no extractable text content.", "source": "offline"}

    if not ollama_available(endpoint):
        # Offline fallback: naive extractive summary (first sentences).
        snippet = " ".join(text.split())[:400]
        return {"summary": "[Model offline] First content: " + snippet,
                "source": "offline"}

    prompt = (
        "You are a concise browsing assistant. Summarize the following web page "
        "content in 3-4 short bullet points. Focus on what the page is about and "
        "any actions it asks the user to take. Do not invent facts.\n\n"
        "PAGE CONTENT:\n" + text
    )
    try:
        summary = ollama_generate(endpoint, model, prompt)
        return {"summary": summary or "(empty model response)", "source": "model"}
    except Exception as exc:
        return {"summary": "[Model error] " + str(exc), "source": "offline"}


def handle_phishing_check(params):
    url = params.get("url", "")
    title = params.get("title", "")
    endpoint = params.get("endpoint", "http://127.0.0.1:11434")
    model = params.get("model", "llama3.2")
    allow_model = params.get("allow_model", True)

    base = phishing_heuristics(url, title)
    base["source"] = "heuristic"

    # Optionally enrich with a model opinion, but heuristics remain authoritative
    # for the score (model never lowers a high heuristic score).
    if allow_model and ollama_available(endpoint):
        prompt = (
            "You are a phishing detector. Given only this URL and page title, "
            "answer in one short sentence whether it looks like a phishing or "
            "scam attempt and why. URL: {}\nTITLE: {}".format(url, title)
        )
        try:
            opinion = ollama_generate(endpoint, model, prompt, timeout=30)
            if opinion:
                base["explanation"] = base["explanation"] + " Model note: " + opinion
                base["source"] = "model"
        except Exception:
            pass  # heuristics stand on their own

    return base


def handle_explain_script(params):
    code = clamp(params.get("code", ""), params.get("max_input_chars", 8000))
    endpoint = params.get("endpoint", "http://127.0.0.1:11434")
    model = params.get("model", "llama3.2")

    if not code.strip():
        return {"explanation": "No script content was provided.", "source": "offline"}

    if not ollama_available(endpoint):
        return {"explanation": "[Model offline] Script analysis requires a local Ollama model.",
                "source": "offline"}

    prompt = (
        "You are a security-focused code reviewer. Explain what the following "
        "JavaScript does in plain language. Call out anything that looks like "
        "obfuscation, data exfiltration, credential capture, crypto-mining, or "
        "tracking. Be concise.\n\nSCRIPT:\n" + code
    )
    try:
        explanation = ollama_generate(endpoint, model, prompt)
        return {"explanation": explanation or "(empty model response)", "source": "model"}
    except Exception as exc:
        return {"explanation": "[Model error] " + str(exc), "source": "offline"}


HANDLERS = {
    "ping": handle_ping,
    "summarize": handle_summarize,
    "phishing_check": handle_phishing_check,
    "explain_script": handle_explain_script,
}


def dispatch(request):
    request_id = request.get("id", "")
    method = request.get("method", "")
    params = request.get("params", {}) or {}

    handler = HANDLERS.get(method)
    if handler is None:
        return {"id": request_id, "ok": False, "error": "unknown method: " + method}

    try:
        result = handler(params)
        return {"id": request_id, "ok": True, "result": result}
    except Exception as exc:
        return {"id": request_id, "ok": False, "error": str(exc)}


def emit(obj):
    """Write one JSONL response. The shell may close our stdout after reading the
    first line (it consumes one response per call), so treat a broken pipe as a
    normal shutdown rather than dumping a traceback to stderr."""
    try:
        sys.stdout.write(json.dumps(obj) + "\n")
        sys.stdout.flush()
        return True
    except BrokenPipeError:
        return False


def main():
    if len(sys.argv) < 2 or sys.argv[1] != "--stdio":
        sys.stderr.write("Veyra AI orchestrator requires --stdio mode.\n")
        sys.exit(1)

    try:
        for line in sys.stdin:
            line = line.strip()
            if not line:
                continue
            try:
                request = json.loads(line)
            except json.JSONDecodeError:
                if not emit({"id": "", "ok": False, "error": "invalid JSON request"}):
                    break
                continue

            if request.get("method") == "shutdown":
                emit({"id": request.get("id", ""), "ok": True, "result": {"bye": True}})
                break

            if not emit(dispatch(request)):
                break
    except BrokenPipeError:
        pass
    finally:
        # Avoid a second BrokenPipeError during interpreter shutdown flush.
        try:
            sys.stdout.close()
        except Exception:
            pass


if __name__ == "__main__":
    main()
