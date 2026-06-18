# Veyra Extensions

Built-in browser extensions for the Veyra shell. Each subdirectory is one
extension: a `manifest.json` plus one or more content-script JS files.

## Loading model

At startup the shell scans this directory (overridable with `--extensions-dir=`,
or disabled entirely with `--no-extensions`). For every extension whose manifest
has `"default_enabled": true` (the default when the key is absent), it reads each
file listed in `content_scripts[].js` and injects it into **every browsing tab**
at `document_start`, top frame.

```
extensions/
└── browser-control/        ← automation / remote-control surface (default on)
    ├── manifest.json
    ├── control.js
    └── README.md
```

## Manifest fields honoured today

| field | meaning |
|---|---|
| `id`, `name`, `version`, `description`, `author` | metadata |
| `default_enabled` | inject automatically at startup (default `true`) |
| `content_scripts[].js` | JS files injected at document-start |
| `permissions` | declarative only (not yet enforced) |

`matches`, `run_at`, and `all_frames` are recorded for forward-compatibility but
the current loader injects at document-start / top-frame / all URLs.

## Bundled extensions

| extension | purpose | default |
|---|---|---|
| [`browser-control`](browser-control/README.md) | Drive the browser (navigate, click, fill, snapshot) over the shell control channel | enabled |

See [`browser-control/README.md`](browser-control/README.md) for the full command
reference and worked examples.

## Writing a new extension

1. Create `extensions/<your-extension>/manifest.json`.
2. Add your content script(s) and list them under `content_scripts[].js`.
3. Set `"default_enabled": true` to auto-load, or `false` to ship it dormant.
4. Rebuild is **not** required — extensions are read from disk at shell startup;
   just relaunch the shell.

Content scripts run in the page's JavaScript context at document-start. They are
subject to the active persona's extension policy (e.g. `eval` blocking) just like
any page script.
