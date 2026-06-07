# Phase 6 Implementation Notes

## Scope

Phase 6 delivers the first real fingerprint-obfuscation policy engine for Veyra.

This phase completes:

- fingerprint profile schema and seed data
- per-persona fingerprint profile resolution during startup bootstrap
- `FingerprintEngine` C++ layer that translates profiles into deterministic injection policies
- per-session canvas noise injection via seeded PRNG
- WebGL vendor/renderer string override
- AudioContext frequency-data noise injection
- navigator property override (hardwareConcurrency, deviceMemory, platform)
- screen metrics override (width, height, availHeight, colorDepth, pixelDepth, devicePixelRatio)
- JavaScript injection at document-start in all frames via WebKit UserContentManager
- fingerprint script update on hot-route policy reconfiguration
- fingerprint profile reference validation in the startup foundation registry
- `fingerprint_profile_id` and `fingerprint_script_injected` fields in per-persona security-policy reports

## Architecture

The fingerprint layer sits between the persona profile system and the WebKit engine.

```
PersonaDefinition.fingerprint_profile_id
        ↓
FingerprintProfileDefinition  (loaded from schemas/fingerprint/)
        ↓
BuildFingerprintPolicy()       (fingerprint_engine.cc)
        ↓
BuildFingerprintScript()       (fingerprint_engine.cc — generates JS)
        ↓
EngineSecurityPolicy.fingerprint_script
        ↓
InjectFingerprintScript()      (webkitgtk_browser_engine.cc)
        ↓
WebKitUserContentManager → WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START
                         → WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES
```

## Implemented Fingerprint Profiles

### `native_stable`
No fingerprint shaping. Uses actual hardware values. No script is injected.
Assigned to: `real_identity` persona.

### `balanced_stealth`
- Canvas noise: subtle (1% pixel probability, ±1 value noise)
- WebGL: Intel Iris OpenGL Engine / Intel Inc.
- Audio: subtle noise on getFloatFrequencyData / getByteFrequencyData
- Navigator: hardwareConcurrency=4, deviceMemory=8, platform=Linux x86_64
- Screen: 1920×1080, colorDepth=24, devicePixelRatio=1.0

Assigned to: `anonymous` and `i2p_research` personas.

### `low_noise_obfuscated`
- Canvas noise: aggressive (3% pixel probability)
- WebGL: Mesa / Mesa Software Rasterizer (reduces GPU leakage surface)
- Audio: subtle noise
- Navigator: hardwareConcurrency=2, deviceMemory=4, platform=Linux x86_64
- Screen: 1280×800

Assigned to: `red_team` persona.

### `minimal_surface`
Same as low_noise_obfuscated but with 1024×768 screen dimensions for maximum commonality.
Reserved for future maximum-isolation personas.

## Canvas Noise Design

### Why per-session determinism matters

A naive random-per-call approach can itself be detected: if `toDataURL()` returns a different value on each call for the same canvas content, JavaScript on the page can detect the instability by calling it twice and comparing. Deterministic per-session noise is undetectable by this test.

### Seed derivation

```cpp
unsigned int seed = std::hash<std::string>{}(session_id + "|" + profile_id);
```

- `session_id` changes each browser launch (contains millisecond timestamp)
- Same content drawn on the same `(sx, sy)` coordinates always gets the same noise within one session
- Different sites cannot correlate noise values because they render different content at different coordinates

### PRNG: xorshift32

Fast, low-quality but sufficient. Not cryptographic — only needs to resist fingerprint correlation, not adversarial brute force.

### Noise magnitude

- `subtle`: ~1% of pixels get ±1 value change in the red channel only
- `aggressive`: ~3% of pixels, same ±1 magnitude

Magnitude is deliberately small. Large noise would be visible to users and detectable via statistical tests. The goal is to break hash-based canvas fingerprinting, not to produce visually degraded images.

## WebGL Override

WebGL exposes `RENDERER` and `VENDOR` strings plus the WEBGL_debug_renderer_info extension strings `UNMASKED_RENDERER_WEBGL` (0x9246) and `UNMASKED_VENDOR_WEBGL` (0x9245).

The script patches both `WebGLRenderingContext.prototype.getParameter` and `WebGL2RenderingContext.prototype.getParameter` to intercept all four parameter IDs.

`Mesa / Mesa Software Rasterizer` was chosen for `low_noise_obfuscated` because it is a real, common Linux string that does not reveal hardware GPU type.

## AudioContext Noise

`getFloatFrequencyData` is the primary AudioContext fingerprinting vector. The OfflineAudioContext technique renders a specific tone and measures the output buffer hash.

The injected noise adds `(rng() - 0.5) * AUDIO_MAG` to each float in the output. At `AUDIO_MAG = 0.0001`, the noise is below the threshold of human hearing and below the precision typically used in fingerprint hash comparisons.

`getByteFrequencyData` is also patched (±0 or ±1 rounding noise) since some fingerprinters use the integer API.

## Script Injection Timing

`WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START` fires before the HTML parser creates the body element and before any page script runs. This ensures that overrides cannot be undone by early-running page scripts.

`WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES` extends protection to iframes, which are a common fingerprinting bypass: the top frame could be clean while a tracking iframe fingerprints via a cross-origin `<canvas>`.

## Hot Route Reconfigure

When `ApplySecurityPolicy` is called (hot-route change), the fingerprint script for each active tab is cleared via `webkit_user_content_manager_remove_all_scripts` and the new script is injected before the forced cache-bypass reload. This ensures that the fingerprint policy is always consistent with the active security posture.

## Known Limits

Phase 6 is the first usable fingerprint layer, not the final hardening architecture.

Current limits:

- Font enumeration is not yet addressed; `measureText` timing attacks can still enumerate installed fonts
- Timezone offset override is not implemented; `Date.prototype.getTimezoneOffset` still returns the system value
- `navigator.languages` is not overridden; the accept-language HTTP header may still leak locale
- WebRTC ICE candidate leakage is handled at the route/WebKit layer, not the fingerprint layer
- The canvas seed does not incorporate origin; a site that controls multiple subdomains could potentially correlate seeds. This is an acceptable tradeoff at this phase.
- The PRNG seed depends on `std::hash<std::string>` which may vary across platforms and standard library versions

## Validation

Validated in this repository with:

- schema validation against `fingerprint-profile.schema.json` on startup
- fingerprint profile reference checking in `profile_registry.cc` for all personas
- smoke runs confirm `fingerprint_profile_id` and `fingerprint_script_injected` in security-policy reports
- manual inspection of generated JS for `balanced_stealth` and `low_noise_obfuscated` profiles
