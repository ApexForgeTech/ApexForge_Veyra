# Veyra Browser Control Extension (v2.0)

A built-in automation / remote-control surface for the Veyra shell. It lets an
operator — or an automated agent — **drive the browser from outside**: navigate,
wait, fill forms, type, click, read the DOM, capture screenshots, run scripts,
and inspect a page the way a computer-use AI does.

It is **not** a visual theme. It changes the browser's *behaviour*, not the
dashboard's look (that is the React app in `apps/shell-ui/`).

---

## 1. What it is

```
extensions/browser-control/
├── manifest.json   → metadata; default_enabled: true
├── control.js      → content script; injected into every page at document-start
└── README.md
```

`control.js` defines `window.__VEYRA_CONTROL__` in every tab — a frozen object of
DOM helpers. The native C++ shell calls these via WebKit's `evaluate_javascript`
when it receives a command on the **control channel**. The extension auto-loads
because its manifest sets `"default_enabled": true`.

---

## 2. The control channel

Launch with `--control-fifo=<path>` to open a command FIFO:

```bash
build/manual/veyra_shell --persona=real_identity \
  --route-engine-bin=build/manual/route_engine \
  --artifact-scan-bin=build/manual/artifact_scan \
  --shell-ui-path=apps/shell-ui/dist \
  --control-fifo=/tmp/veyra.ctl
```

Send **one command per line**:

```bash
echo "navigate https://example.org" > /tmp/veyra.ctl
```

### Results

Commands that return a value write to **two** places:

1. **stdout**, prefixed `[control] result …`
2. a file **`<fifo>.out`** (overwritten each call), for scripted reads.

```bash
echo "gettext h1" > /tmp/veyra.ctl
sleep 0.3
cat /tmp/veyra.ctl.out        # {"ok":true,"value":"Example Domain"}
```

### Correlated requests (`@id`)

Prefix a command with `@<id>` to tag its result. The reply echoes `@<id>` on
stdout and is written to **`<fifo>.out.<id>`** instead of `<fifo>.out`:

```bash
echo "@login click #submit" > /tmp/veyra.ctl
cat /tmp/veyra.ctl.out.login
```

---

## 3. Selectors

Every selector argument accepts three forms:

| form | example |
|---|---|
| CSS | `#username`, `.btn.primary`, `form input[name=q]` |
| text match | `text=Sign In` (exact, else first containing) |
| XPath | `xpath=//button[@id="login"]` |

---

## 4. Command reference

### Navigation & lifecycle
| Command | Effect |
|---|---|
| `navigate <url>` | Load URL in the active tab |
| `back` / `forward` / `reload` | History / reload |
| `url` / `title` | Active tab URL / `document.title` |
| `tab-new [url]` | Open a new tab |
| `tab-close [id]` | Close a tab (active tab when no id) |
| `tabs` | List open tabs `{id,url,active}` |
| `tab <id>` | Activate a tab |
| `help` | List all control commands |
| `quit` | Close the shell |

### Waits (no more manual `sleep`)
| Command | Effect |
|---|---|
| `wait-load [ms]` | Block until the active tab finishes loading (default 15 s) |
| `wait-for <selector>[\|ms]` | Wait until the element exists (default 10 s) |
| `wait-text <selector>\|<text>` | Wait until the element contains text |

### DOM actions
| Command | Effect |
|---|---|
| `click <selector>` | Click |
| `dblclick` / `rightclick <selector>` | Double / context-menu click |
| `hover <selector>` | Mouse-over events |
| `focus` / `blur <selector>` | Focus / blur |
| `fill <selector>\|<value>` | Set an input value (fires input/change) |
| `type <selector>\|<text>` | Type character-by-character with key events |
| `clear <selector>` | Empty an input |
| `keypress <key>` | Send a key to the focused element (`Enter`, `Tab`, …) |
| `check <selector>\|<0\|1>` | Set a checkbox |
| `select <selector>\|<value>` | Choose a `<select>` option |
| `submit <selector>` | Submit the enclosing form |
| `scroll <selector>` | Scroll element into view |
| `scroll-by <x>\|<y>` | Scroll the window |

### Queries / introspection
| Command | Result |
|---|---|
| `gettext` / `getvalue <selector>` | Text / input value |
| `exists` / `visible` / `count <selector>` | Presence / visibility / match count |
| `attrs <selector>` | All attributes as an object |
| `html <selector>` | `outerHTML` (truncated) |
| `bounds <selector>` | `{x,y,w,h}` bounding rect |
| `query <selector>` | Up to 50 matches `{tag,text,selector,visible}` |
| `pageinfo` | URL, title, readyState, forms, links, scroll |
| `links` | Up to 50 `{text,href}` |

### Computer-use surface (for vision/LLM drivers)
| Command | Effect |
|---|---|
| `clickable` | List interactive elements `{index,tag,text,selector,rect}` |
| `textdump [chars]` | Visible/accessibility text of the page |
| `annotate` | Draw numbered red badges over clickable elements |
| `annotate-clear` | Remove the overlay |
| `click-index <n>` | Click the n-th element from `clickable`/`annotate` |

> Typical agent loop: `annotate` → `snapshot view.png` → look at the numbered
> overlay → `click-index 4`.

### Logs / storage
| Command | Effect |
|---|---|
| `console` | Last 100 captured `console.*` messages |
| `errors` | Last 100 JS errors / unhandled rejections |
| `clear-logs` | Reset the buffers |
| `cookies` | `document.cookie` |
| `storage-get <key>` / `storage-set <k>\|<v>` / `storage-clear` | localStorage |

### Assertions (for tests)
| Command | Result |
|---|---|
| `assert-text <selector>\|<expected>` | `{pass:bool,actual}` |
| `assert-exists <selector>` | `{pass:bool}` |
| `assert-url <substring>` | `{pass:bool,actual}` |

### Settings / routing
| Command | Effect |
|---|---|
| `route <profile_id>` | Switch the active route (e.g. `route tor_bridge`) |
| `action <json>` | Raw dashboard action (`switch_route`, `open_tab`, `ai_*`, `osint_*`) |
| `panel <on\|off\|toggle>` | Show/hide the Veyra dashboard side panel |
| `theme [id]` | List themes, or switch (`veyra-dark`, `veyra-light`, `midnight-purple`, `ghost-green`) |
| `search-engine [id]` | List/set the address-bar search engine (`duckduckgo`, `google`, `bing`, `brave`, `startpage`) |

Panel visibility, theme and search engine persist in `~/.config/veyra/ui.conf`.

### Capture & scripting
| Command | Effect |
|---|---|
| `snapshot <path.png>` | Render the **active page** to PNG (WebKit snapshot) |
| `snapshot-dash <path.png>` | Render the **dashboard** panel to PNG |
| `snapshot-window <path.png>` | Render the **whole GTK window** (toolbar, tabs, paned chrome) to PNG — for chrome/theme inspection |
| `eval <js>` | Run arbitrary JS, return its value |
| `script <file>` | Run a sequence of commands from a file |

---

## 5. Scripts

A `.veyractl` file is one command per line; blank lines and `#` comments are
skipped. The runner **auto-paces through page loads** — it won't run the next
line while the tab is still loading, so `navigate` is naturally followed by its
load. Add explicit `wait-*` lines for JS-driven changes.

```
# auto-login.veyractl
navigate http://127.0.0.1:8765/login.html
wait-for #username
fill #username|scripted_user
fill #password|pw12345
select #role|operator
check #remember|1
click #login
wait-text #result|Signed in
```

```bash
echo "script /tmp/auto-login.veyractl" > /tmp/veyra.ctl
```

---

## 6. The injected API (`window.__VEYRA_CONTROL__`)

Every method returns a JSON string. Highlights: `exists`, `count`, `visible`,
`getText`, `getValue`, `getAttribute`, `attrs`, `html`, `bounds`, `query`,
`pageInfo`, `links`, `clickable`, `textDump`, `annotate`, `clearAnnotations`,
`clickIndex`, `click`, `dblclick`, `rightclick`, `hover`, `focus`, `blur`,
`clear`, `fill`, `type`, `keypress`, `setChecked`, `selectOption`, `submit`,
`scrollTo`, `scrollBy`, `scrollIntoView`, `cookies`, `storageGet/Set/Clear`,
`getLogs`, `getErrors`, `clearLogs`, `assertText`, `assertExists`,
`predExists`/`predText` (used by native `wait-*` polling).

The object is `Object.freeze`d, non-writable/non-configurable, and idempotent
across re-injections. `console.*` and `window.onerror` are captured into
in-page ring buffers at document-start.

---

## 7. Architecture

```
 driver ──@id cmd──▶ /tmp/veyra.ctl (FIFO)
                          │  g_io_add_watch on the GTK main loop
                          ▼
              HandleControlLine(cmd)
       ┌──────────────────┼─────────────────────────────┐
       ▼                  ▼                              ▼
  load/reload/      evaluate_javascript(            wait-* :
  back/forward       "__VEYRA_CONTROL__.click(…)")   g_timeout polls
  tabs/route                │                         predExists / is_loading
                            ▼                              │
              control.js runs in page → DOM changes        ▼
                            │                         get_snapshot → PNG
                            ▼
            result → stdout + <fifo>.out[.id]
```

---

## 8. Security boundaries

The control channel respects the browser's own policy; it does **not** bypass it:

- **`file://` navigation is blocked** for browsing tabs. Serve local test pages
  over `http://127.0.0.1:<port>` (allowed for `routing_requirement: none`
  personas like `real_identity`).
- `navigate` still passes `ShouldAllowNavigation` (route / leak / extension policy).
- The injected API only touches its own page's DOM — no shell, filesystem, or
  cross-tab access.
- The FIFO is for **local automation/self-test**. Anyone who can write to it can
  drive the browser; don't expose the path to untrusted processes.

---

## 9. Manifest

```json
{
  "id": "veyra.browser-control",
  "name": "Veyra Browser Control",
  "version": "2.0.0",
  "default_enabled": true,
  "content_scripts": [
    { "matches": ["<all_urls>"], "js": ["control.js"], "run_at": "document_start", "all_frames": false }
  ],
  "permissions": ["dom", "forms", "navigation", "automation"]
}
```

Set `"default_enabled": false` to ship dormant, or pass `--no-extensions` to the
shell to disable the whole `extensions/` directory.

---

## 10. Verified

Built against WebKitGTK 2.50 and driven end-to-end (real, not mocked):

- `wait-load`, `wait-for`, `wait-text` resolve correctly (no manual sleeps)
- `clickable` / `annotate` / `click-index` — numbered overlay captured via snapshot
- `type` (char-by-char), `text=` and `xpath=` selectors
- `assert-text` / `assert-url`, `console` / `errors` capture
- `@id` correlation → `<fifo>.out.<id>`
- `script` runner auto-paces a full login flow to completion
