#!/usr/bin/env python3
"""
Veyra OSINT Workspace (Phase 11)

A route-aware OSINT sidecar for the Veyra browser shell. Runs as a stdio child
process and speaks newline-delimited JSON (JSONL): one request object per line
in, one response object per line out.

Design constraints:
  - Standard library only. No pip dependencies (minimal supply-chain surface).
  - Route-honouring: every network egress traverses the active persona's proxy.
      * Tor (socks5://) via a minimal in-file SOCKS5 client with REMOTE DNS
        (ATYP=domain) so hostnames are resolved by Tor, never leaking locally.
      * I2P / HTTP proxy (http://) via absolute-form GET and CONNECT tunnels.
      * direct only when the policy permits it.
  - Leak prevention: when require_route is set and no usable proxy is active,
    every network lookup is REFUSED rather than silently going out the ISP.
  - DNS is resolved via DNS-over-HTTPS (Cloudflare JSON API) THROUGH the route,
    so DNS questions are never exposed to the system resolver.
  - Privacy: request content and results are never logged to disk or stderr.

Protocol:
  Request:  {"id": "<str>", "method": "<str>", "params": { ... , "route": {...}, "policy": {...} }}
  Response: {"id": "<str>", "ok": true,  "result": { ... }}
            {"id": "<str>", "ok": false, "error": "<str>"}

  route  = {"proxy_uri": "socks5://127.0.0.1:9050", "route_type": "tor"}
  policy = {"require_route": true, "allow_whois": true, ...}

Lookup results share a structured shape so the C++ side can merge them into a case:
  {"entities": [{"type","value","source","attributes":{}}],
   "relationships": [{"from","to","kind"}],
   "timeline": [{"ts_ms","event","detail"}],
   "summary": "<str>", "raw": "<str>"}

Methods:
  ping             -> {"ok_route": bool, "route_type": str, "egress": "proxy"|"direct"|"blocked"}
  whois_lookup     params: {target}   -> lookup result
  dns_lookup       params: {target}   -> lookup result (A/AAAA/MX/NS/TXT via DoH)
  archive_lookup   params: {target}   -> lookup result (Wayback CDX)
  username_search  params: {target}   -> lookup result (curated site set)
"""

import sys
import json
import time
import socket
import ssl
import struct
from urllib.parse import urlparse, quote


# ── Route / egress layer ──────────────────────────────────────────────

class EgressBlocked(Exception):
    """Raised when a lookup would leak outside the required route."""


def now_ms():
    return int(time.time() * 1000)


def parse_proxy(route):
    """Return (kind, host, port) where kind is 'socks5' | 'http' | 'direct'."""
    proxy_uri = (route or {}).get("proxy_uri", "") or ""
    route_type = (route or {}).get("route_type", "") or ""
    if not proxy_uri or route_type == "direct":
        return ("direct", None, None)
    parsed = urlparse(proxy_uri)
    scheme = (parsed.scheme or "").lower()
    host = parsed.hostname or "127.0.0.1"
    port = parsed.port or (9050 if scheme.startswith("socks") else 4444)
    if scheme.startswith("socks"):
        return ("socks5", host, port)
    if scheme in ("http", "https"):
        return ("http", host, port)
    return ("direct", None, None)


def ensure_egress_allowed(route, policy):
    """Enforce leak prevention. Returns the egress kind or raises EgressBlocked."""
    kind, _, _ = parse_proxy(route)
    require_route = bool((policy or {}).get("require_route", False))
    if require_route and kind == "direct":
        raise EgressBlocked(
            "Leak prevention: this persona requires a proxy route, but none is "
            "active. Refusing to send OSINT traffic over the direct ISP.")
    return kind


def socks5_connect(proxy_host, proxy_port, dest_host, dest_port, timeout=30):
    """Open a TCP connection to dest via a SOCKS5 proxy, with REMOTE DNS.

    The destination hostname is sent to the proxy (ATYP=0x03) so it is resolved
    by the proxy (e.g. Tor), preventing local DNS leaks.
    """
    sock = socket.create_connection((proxy_host, proxy_port), timeout=timeout)
    try:
        # Greeting: VER=5, one method, 0x00 = no auth.
        sock.sendall(b"\x05\x01\x00")
        resp = _recv_exact(sock, 2)
        if resp[0:1] != b"\x05" or resp[1:2] != b"\x00":
            raise OSError("SOCKS5 proxy rejected no-auth method")

        host_bytes = dest_host.encode("idna") if _is_hostname(dest_host) else dest_host.encode("ascii")
        if len(host_bytes) > 255:
            raise OSError("destination host too long for SOCKS5")
        req = b"\x05\x01\x00\x03" + bytes([len(host_bytes)]) + host_bytes + struct.pack(">H", dest_port)
        sock.sendall(req)

        reply = _recv_exact(sock, 4)
        if reply[1:2] != b"\x00":
            raise OSError("SOCKS5 connect failed (code %d)" % reply[1])
        atyp = reply[3:4]
        if atyp == b"\x01":
            _recv_exact(sock, 4)
        elif atyp == b"\x04":
            _recv_exact(sock, 16)
        elif atyp == b"\x03":
            ln = _recv_exact(sock, 1)[0]
            _recv_exact(sock, ln)
        _recv_exact(sock, 2)  # bound port
        return sock
    except Exception:
        sock.close()
        raise


def http_connect_tunnel(proxy_host, proxy_port, dest_host, dest_port, timeout=30):
    """Open a TCP tunnel to dest via an HTTP proxy CONNECT (for TLS)."""
    sock = socket.create_connection((proxy_host, proxy_port), timeout=timeout)
    try:
        req = ("CONNECT %s:%d HTTP/1.1\r\nHost: %s:%d\r\n\r\n"
               % (dest_host, dest_port, dest_host, dest_port)).encode("ascii")
        sock.sendall(req)
        head = _read_http_head(sock)
        status_line = head.split("\r\n", 1)[0]
        parts = status_line.split(" ", 2)
        if len(parts) < 2 or not parts[1].startswith("2"):
            raise OSError("HTTP proxy CONNECT failed: " + status_line)
        return sock
    except Exception:
        sock.close()
        raise


def open_tcp(dest_host, dest_port, route, timeout=30):
    """Open a raw TCP socket to dest, honouring the route. No TLS."""
    kind, phost, pport = parse_proxy(route)
    if kind == "socks5":
        return socks5_connect(phost, pport, dest_host, dest_port, timeout)
    if kind == "http":
        # Plain TCP (e.g. WHOIS :43) over an HTTP proxy requires CONNECT.
        return http_connect_tunnel(phost, pport, dest_host, dest_port, timeout)
    return socket.create_connection((dest_host, dest_port), timeout=timeout)


def https_request(method, url, route, headers=None, body=None, timeout=30, max_bytes=1048576):
    """Perform an HTTP/HTTPS request honouring the route. Returns (status, body_text)."""
    parsed = urlparse(url)
    scheme = (parsed.scheme or "https").lower()
    host = parsed.hostname
    port = parsed.port or (443 if scheme == "https" else 80)
    path = parsed.path or "/"
    if parsed.query:
        path += "?" + parsed.query

    kind, phost, pport = parse_proxy(route)
    headers = dict(headers or {})
    headers.setdefault("Host", host)
    headers.setdefault("User-Agent", "VeyraOSINT/0.11")
    headers.setdefault("Accept", "*/*")
    headers.setdefault("Connection", "close")

    if scheme == "http" and kind == "http":
        # Absolute-form request straight to the HTTP proxy.
        sock = socket.create_connection((phost, pport), timeout=timeout)
        request_target = url
    else:
        sock = open_tcp(host, port, route, timeout=timeout)
        request_target = path
        if scheme == "https":
            ctx = ssl.create_default_context()
            sock = ctx.wrap_socket(sock, server_hostname=host)

    try:
        lines = ["%s %s HTTP/1.1" % (method, request_target)]
        for k, v in headers.items():
            lines.append("%s: %s" % (k, v))
        raw = ("\r\n".join(lines) + "\r\n\r\n").encode("utf-8")
        if body is not None:
            raw += body if isinstance(body, bytes) else body.encode("utf-8")
        sock.sendall(raw)

        head = _read_http_head(sock)
        status_line = head.split("\r\n", 1)[0]
        status = int(status_line.split(" ", 2)[1]) if len(status_line.split(" ")) > 1 else 0

        # Read the remaining body (Connection: close → read to EOF).
        chunks = []
        # Anything already buffered past the header was returned by _read_http_head.
        leftover = getattr(_read_http_head, "_leftover", b"")
        if leftover:
            chunks.append(leftover)
        total = len(leftover)
        while total < max_bytes:
            data = sock.recv(65536)
            if not data:
                break
            chunks.append(data)
            total += len(data)
        body_bytes = b"".join(chunks)
        text = _decode_http_body(head, body_bytes)
        return (status, text)
    finally:
        try:
            sock.close()
        except Exception:
            pass


# ── low-level helpers ─────────────────────────────────────────────────

def _recv_exact(sock, n):
    buf = b""
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise OSError("unexpected EOF from socket")
        buf += chunk
    return buf


def _read_http_head(sock):
    """Read until end of HTTP headers; stash any trailing body bytes."""
    data = b""
    while b"\r\n\r\n" not in data:
        chunk = sock.recv(4096)
        if not chunk:
            break
        data += chunk
        if len(data) > 262144:
            break
    head, _, rest = data.partition(b"\r\n\r\n")
    _read_http_head._leftover = rest
    return head.decode("iso-8859-1", "replace")


def _decode_http_body(head, body_bytes):
    """Decode the body, handling chunked transfer-encoding."""
    lowered = head.lower()
    if "transfer-encoding: chunked" in lowered:
        decoded = b""
        rest = body_bytes
        while rest:
            line, _, rest = rest.partition(b"\r\n")
            if not line:
                break
            try:
                size = int(line.strip(), 16)
            except ValueError:
                break
            if size == 0:
                break
            decoded += rest[:size]
            rest = rest[size + 2:]  # skip trailing CRLF
        body_bytes = decoded
    return body_bytes.decode("utf-8", "replace")


def _is_hostname(value):
    try:
        socket.inet_aton(value)
        return False
    except OSError:
        return ":" not in value  # not an IPv6 literal either


# ── OSINT modules ─────────────────────────────────────────────────────

WHOIS_IANA = "whois.iana.org"


def module_whois(target, route):
    target = target.strip().lower()
    host = urlparse("http://" + target).hostname or target

    def query(server, q):
        sock = open_tcp(server, 43, route, timeout=30)
        try:
            sock.sendall((q + "\r\n").encode("idna", "replace") if _is_hostname(q)
                         else (q + "\r\n").encode("ascii", "replace"))
            data = b""
            while len(data) < 65536:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                data += chunk
            return data.decode("utf-8", "replace")
        finally:
            sock.close()

    iana_text = query(WHOIS_IANA, host)
    refer = None
    for line in iana_text.splitlines():
        if line.lower().startswith("refer:"):
            refer = line.split(":", 1)[1].strip()
            break

    registry_text = ""
    if refer:
        try:
            registry_text = query(refer, host)
        except Exception as exc:
            registry_text = "[registry whois failed: %s]" % exc

    combined = registry_text if registry_text.strip() else iana_text
    attributes = {}
    for key in ("Registrar", "Creation Date", "Registry Expiry Date",
                "Updated Date", "Registrant Organization", "Name Server"):
        for line in combined.splitlines():
            if line.strip().lower().startswith(key.lower() + ":"):
                attributes.setdefault(key, line.split(":", 1)[1].strip())
                break

    entities = [{"type": "domain", "value": host, "source": "whois", "attributes": attributes}]
    relationships = []
    timeline = []
    if "Registrar" in attributes:
        entities.append({"type": "org", "value": attributes["Registrar"],
                         "source": "whois", "attributes": {}})
        relationships.append({"from": host, "to": attributes["Registrar"], "kind": "registered_by"})
    if "Creation Date" in attributes:
        timeline.append({"ts_ms": now_ms(), "event": "domain_created",
                         "detail": host + " created " + attributes["Creation Date"]})

    summary = "WHOIS for %s via %s." % (host, refer or WHOIS_IANA)
    return {"entities": entities, "relationships": relationships,
            "timeline": timeline, "summary": summary, "raw": combined[:8000]}


DOH_URL = "https://cloudflare-dns.com/dns-query"


def module_dns(target, route):
    host = urlparse("http://" + target).hostname or target.strip()
    record_types = ["A", "AAAA", "MX", "NS", "TXT"]
    entities = [{"type": "domain", "value": host, "source": "dns", "attributes": {}}]
    relationships = []
    timeline = [{"ts_ms": now_ms(), "event": "dns_query", "detail": "DoH lookup for " + host}]
    found = {}

    for rtype in record_types:
        url = "%s?name=%s&type=%s" % (DOH_URL, quote(host), rtype)
        try:
            status, text = https_request("GET", url, route,
                                         headers={"Accept": "application/dns-json"})
            if status != 200:
                continue
            data = json.loads(text)
        except Exception:
            continue
        answers = data.get("Answer", []) or []
        values = [a.get("data", "") for a in answers if a.get("data")]
        if values:
            found[rtype] = values
            for v in values:
                etype = "ip" if rtype in ("A", "AAAA") else "dns_record"
                entities.append({"type": etype, "value": v, "source": "dns",
                                 "attributes": {"record": rtype}})
                relationships.append({"from": host, "to": v,
                                      "kind": rtype.lower() + "_record"})

    summary = "DNS (DoH) for %s: %s" % (
        host, ", ".join("%s=%d" % (k, len(v)) for k, v in found.items()) or "no records")
    return {"entities": entities, "relationships": relationships,
            "timeline": timeline, "summary": summary, "raw": json.dumps(found)[:8000]}


def module_archive(target, route):
    host = urlparse("http://" + target).hostname or target.strip()
    url = ("https://web.archive.org/cdx/search/cdx?url=%s&output=json&limit=25"
           "&fl=timestamp,original,statuscode&collapse=timestamp:8" % quote(host))
    status, text = https_request("GET", url, route)
    rows = []
    try:
        parsed = json.loads(text)
        rows = parsed[1:] if parsed and isinstance(parsed, list) else []
    except Exception:
        rows = []

    entities = [{"type": "domain", "value": host, "source": "archive", "attributes": {}}]
    timeline = []
    for row in rows[:25]:
        if len(row) >= 2:
            ts, original = row[0], row[1]
            timeline.append({"ts_ms": _wayback_ts_to_ms(ts), "event": "archived_snapshot",
                             "detail": "%s @ %s" % (original, ts)})
    summary = "Wayback Machine: %d snapshots for %s." % (len(timeline), host)
    return {"entities": entities, "relationships": [],
            "timeline": timeline, "summary": summary, "raw": text[:8000]}


USERNAME_SITES = {
    "GitHub":    "https://github.com/{}",
    "GitLab":    "https://gitlab.com/{}",
    "Reddit":    "https://www.reddit.com/user/{}",
    "Twitter/X": "https://x.com/{}",
    "Instagram": "https://www.instagram.com/{}/",
    "Telegram":  "https://t.me/{}",
    "Keybase":   "https://keybase.io/{}",
    "HackerNews":"https://news.ycombinator.com/user?id={}",
    "Medium":    "https://medium.com/@{}",
    "Mastodon":  "https://mastodon.social/@{}",
    "TikTok":    "https://www.tiktok.com/@{}",
    "Pinterest": "https://www.pinterest.com/{}/",
    "Twitch":    "https://www.twitch.tv/{}",
    "DevTo":     "https://dev.to/{}",
    "Gravatar":  "https://gravatar.com/{}",
}


def module_username(username, route, max_sites):
    username = username.strip().lstrip("@")
    entities = [{"type": "username", "value": username, "source": "username_search", "attributes": {}}]
    relationships = []
    timeline = [{"ts_ms": now_ms(), "event": "username_search",
                 "detail": "probing %d sites for '%s'" % (max_sites, username)}]
    hits = []
    checked = 0
    for site, template in USERNAME_SITES.items():
        if checked >= max_sites:
            break
        checked += 1
        profile_url = template.format(quote(username))
        try:
            status, _ = https_request("GET", profile_url, route, timeout=20, max_bytes=4096)
            present = status == 200
        except Exception:
            status, present = 0, False
        if present:
            hits.append(site)
            entities.append({"type": "url", "value": profile_url, "source": "username_search",
                             "attributes": {"site": site, "status": str(status)}})
            relationships.append({"from": username, "to": profile_url, "kind": "profile_on"})
    summary = "Username '%s': found on %d/%d sites: %s" % (
        username, len(hits), checked, ", ".join(hits) or "none")
    return {"entities": entities, "relationships": relationships,
            "timeline": timeline, "summary": summary, "raw": ""}


def _wayback_ts_to_ms(ts):
    try:
        t = time.strptime(ts[:14], "%Y%m%d%H%M%S")
        return int(time.mktime(t) * 1000)
    except Exception:
        return now_ms()


# ── dispatch ──────────────────────────────────────────────────────────

def guarded(method_name, policy_flag, params, fn):
    route = params.get("route", {})
    policy = params.get("policy", {})
    if not bool(policy.get("enabled", True)):
        raise EgressBlocked("OSINT is disabled for this persona's policy.")
    if not bool(policy.get(policy_flag, False)):
        raise EgressBlocked("Module '%s' is not permitted by this persona's OSINT policy." % method_name)
    ensure_egress_allowed(route, policy)
    return fn(route)


def handle_ping(params):
    route = params.get("route", {})
    policy = params.get("policy", {})
    kind, _, _ = parse_proxy(route)
    require_route = bool(policy.get("require_route", False))
    if require_route and kind == "direct":
        egress = "blocked"
    elif kind == "direct":
        egress = "direct"
    else:
        egress = "proxy"
    return {"ok_route": egress != "blocked", "route_type": route.get("route_type", "unknown"),
            "egress": egress}


def handle_whois(params):
    return guarded("whois", "allow_whois", params,
                   lambda route: module_whois(params.get("target", ""), route))


def handle_dns(params):
    return guarded("dns", "allow_dns", params,
                   lambda route: module_dns(params.get("target", ""), route))


def handle_archive(params):
    return guarded("archive", "allow_archive", params,
                   lambda route: module_archive(params.get("target", ""), route))


def handle_username(params):
    policy = params.get("policy", {})
    max_sites = int(policy.get("max_username_sites", 0) or 0)
    if max_sites <= 0:
        raise EgressBlocked("Username search is not permitted by this persona's OSINT policy.")
    return guarded("username", "allow_username_search", params,
                   lambda route: module_username(params.get("target", ""), route, max_sites))


HANDLERS = {
    "ping": handle_ping,
    "whois_lookup": handle_whois,
    "dns_lookup": handle_dns,
    "archive_lookup": handle_archive,
    "username_search": handle_username,
}


def dispatch(request):
    request_id = request.get("id", "")
    method = request.get("method", "")
    params = request.get("params", {}) or {}
    handler = HANDLERS.get(method)
    if handler is None:
        return {"id": request_id, "ok": False, "error": "unknown method: " + method}
    try:
        return {"id": request_id, "ok": True, "result": handler(params)}
    except EgressBlocked as exc:
        return {"id": request_id, "ok": False, "error": str(exc)}
    except Exception as exc:
        return {"id": request_id, "ok": False, "error": "%s: %s" % (type(exc).__name__, exc)}


def emit(obj):
    try:
        sys.stdout.write(json.dumps(obj) + "\n")
        sys.stdout.flush()
        return True
    except BrokenPipeError:
        return False


def main():
    if len(sys.argv) < 2 or sys.argv[1] != "--stdio":
        sys.stderr.write("Veyra OSINT workspace requires --stdio mode.\n")
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
        try:
            sys.stdout.close()
        except Exception:
            pass


if __name__ == "__main__":
    main()
