# Phase 8 Implementation Notes

## Scope

Phase 8 delivers the extension isolation layer for Veyra.

This phase completes:

- extension policy schema and seed data (3 policies across the isolation spectrum)
- `ExtensionPolicyDefinition` model loaded as part of `FoundationState` at startup
- `extension_policy_id` reference validation in the startup foundation registry
- `ExtensionEngine` C++ layer: `BuildExtensionPolicy` + `BuildEvalBlockScript` + `IsBlockedDomain`
- eval/Function/setTimeout/setInterval blocking via document-start JS injection for locked personas
- mixed content enforcement via `WebKitSettings` (`allow_running_insecure_content`, `allow_displaying_insecure_content`)
- domain-level navigation blocking in `ShouldAllowNavigation` with subdomain matching
- extension policy script injection cleared and rebuilt on hot-route reconfigure
- `extension_policy_id`, `allow_eval`, `allow_third_party_frames`, `block_mixed_content`, `allow_external_fonts`, `blocked_domains_count`, `eval_block_script_injected` in per-persona security-policy reports
- `extension_policy_count` in `RegistrySummary` and bootstrap output

## Architecture

```
PersonaDefinition.extension_policy_id
        ↓
ExtensionPolicyDefinition  (loaded from schemas/extension/)
        ↓
BuildExtensionPolicy()           (extension_engine.cc)
        ↓
ExtensionPolicy {
  allow_eval, allow_third_party_frames,
  block_mixed_content, allow_external_fonts,
  blocked_domains, eval_block_script
}
        ↓
EngineSecurityPolicy.* fields set in BuildEngineSecurityPolicy()
        ↓
Three enforcement points in webkitgtk_browser_engine.cc:
  1. InjectExtensionPolicyScript() → WebKitUserContentManager (eval blocking)
  2. ApplyViewSecurityPolicy()     → WebKitSettings (mixed content)
  3. ShouldAllowNavigation()       → IsBlockedDomain() (domain blocking)
```

## Implemented Extension Policies

### `trusted_daily`
For `real_identity` persona (daily browsing).

- `allow_eval: true` — eval, Function constructor, string setTimeout/setInterval allowed
- `allow_third_party_frames: true` — iframes from any origin allowed
- `block_mixed_content: false` — HTTP resources on HTTPS pages allowed
- `allow_external_fonts: true` — external font loading allowed
- `blocked_domains: []` — no domain blocking
- Eval-block script: **not injected** (no overhead for trusted persona)

### `minimal_locked`
For `anonymous` and `i2p_research` personas.

- `allow_eval: false` — eval and dynamic code execution blocked
- `allow_third_party_frames: false` — third-party iframes blocked at navigation policy
- `block_mixed_content: true` — HTTP resources blocked on HTTPS pages
- `allow_external_fonts: false` — external font loading blocked
- `blocked_domains: [doubleclick.net, googlesyndication.com, google-analytics.com, ...]` — 11 known tracker/analytics domains
- Eval-block script: **injected** at document-start in all frames

### `strict_redteam`
For `red_team` persona.

- All locked settings from `minimal_locked` apply
- `blocked_domains`: 26 domains including Google services, Facebook, Twitter, major ad networks (doubleclick, adsrvr, pubmatic, rubiconproject, openx, amazon-adsystem)
- Provides the strongest domain isolation of the three policies

## Eval Blocking Design

When `allow_eval: false`, the following script is injected at `WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START` in ALL frames:

```javascript
(function() {
  'use strict';
  // Block eval()
  Object.defineProperty(window, 'eval', { value: function() { throw new EvalError(...); }, ... });
  // Block new Function("code")
  // (preserves new Function() with non-string arguments for normal use)
  Object.defineProperty(window, 'Function', { value: blocked wrapper, ... });
  // Block setTimeout("string") and setInterval("string")
  Object.defineProperty(window, 'setTimeout', { value: guarded wrapper, ... });
  Object.defineProperty(window, 'setInterval', { value: guarded wrapper, ... });
})();
```

The `Function` override preserves non-string constructor calls (e.g., `new Function(function() { ... })`) since these are not dynamic code execution. Only string body arguments are blocked.

## Domain Blocking Design

`IsBlockedDomain` checks exact match and subdomain match:
- `"doubleclick.net"` blocks `doubleclick.net` and `ads.doubleclick.net` and `ad.doubleclick.net`
- Matching is case-insensitive
- The check runs in `ShouldAllowNavigation` which fires on every navigation decision (including iframes via `OnDecidePolicy`)

Domain blocks apply to ALL navigation types: top-level, iframe, redirect, form submission.

## Mixed Content Enforcement

Applied via `WebKitSettings`:

```cpp
webkit_settings_set_allow_running_insecure_content(settings, !block_mixed_content);
webkit_settings_set_allow_displaying_insecure_content(settings, !block_mixed_content);
```

`allow_running_insecure_content: false` blocks active mixed content (scripts, plugins, forms from HTTP on HTTPS pages).
`allow_displaying_insecure_content: false` blocks passive mixed content (images, audio, video from HTTP on HTTPS pages).

## Hot-Route Reconfigure

When `ApplySecurityPolicy` is called (hot-route change), the `WebKitUserContentManager` for each active tab is cleared via `webkit_user_content_manager_remove_all_scripts` and the new fingerprint and extension policy scripts are re-injected before the forced reload. This ensures the extension policy is always consistent with the active persona posture.

## Known Limits

Phase 8 is the first usable extension isolation layer, not the final security architecture.

Current limits:

- `allow_third_party_frames: false` is declared in the policy struct and enforced by domain blocking (a third-party domain blocked by the domain list won't load in frames). However, a general "block ALL third-party iframes regardless of domain" control is not yet implemented at the WebKit layer — this requires distinguishing frame navigations from top-level navigations in `OnDecidePolicy`, which requires inspecting `WebKitNavigationAction` navigation type.
- `allow_external_fonts: false` is declared in the policy and stored in `EngineSecurityPolicy` but no WebKit API directly blocks font loading. Full font blocking would require a content blocker rule or CSP `font-src 'none'` injection. This is deferred to Phase 9 (React UI can surface a CSP header injection mechanism).
- The eval-block script can be bypassed by sites that cache a reference to `eval` before the script fires. This is unavoidable with JS injection — complete eval blocking requires engine-level changes (Phase 14 scope).
- Domain blocking in `ShouldAllowNavigation` fires for navigation decisions but NOT for sub-resource loads (images, scripts, stylesheets loaded by a page). Full sub-resource blocking requires a `WebKitWebView::resource-load-started` handler or a WebKit content blocker filter (Phase 9 scope).
