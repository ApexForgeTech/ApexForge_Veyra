# Phase 5 Implementation Notes

## Scope

Phase 5 introduces the first real BlackVault download quarantine pipeline for Veyra.

This phase completes:

- download interception inside the native WebKit shell
- quarantine path handling under each session partition
- a Rust artifact-scan child service
- metadata extraction and hashing
- static heuristic scoring
- branded controlled release review before an artifact leaves quarantine

## Implemented Artifacts

### 1. Quarantine Roots

Each session now receives:

- `downloads/quarantine/`
- `downloads/released/`
- `downloads/reports/`
- `downloads/events.jsonl`

These paths are recorded in `security-policy.json`.

### 2. Rust Artifact Scan Service

The artifact scan service now lives under:

- `core/artifact-scan/`

It runs as a sibling binary in `--stdio` mode and computes:

- SHA-256
- SHA-1
- file size
- MIME guess
- heuristic list
- risk score
- risk level

### 3. Download Interception

The WebKit layer now intercepts downloads using `download-started` and routes them into the session quarantine directory before bytes land in the default host downloads folder.

### 4. Static Heuristics

Initial heuristics include:

- executable extensions
- executable magic bytes
- macro-enabled Office suffixes
- archive containers
- double extensions
- insecure source transport
- active-content document extensions
- empty files

### 5. Controlled Release

When scanning completes, Veyra presents a BlackVault review sheet that shows:

- source URL
- MIME
- hashes
- risk score
- heuristic list
- controlled release destination

The operator can:

- keep the file in quarantine
- release it into the controlled `downloads/released/` directory

Artifacts do not flow directly to the host by default.

## Runtime Behavior

The Phase 5 runtime now produces:

- per-artifact JSON report files in `downloads/reports/`
- `downloads/events.jsonl` audit events
- quarantine retention when release is denied or blocked
- controlled export path moves when release is approved

## Validation

Validated in this repository with:

- native build including both `route_engine` and `artifact_scan`
- smoke runs with active-session hot route reconfigure
- direct stdio scan execution against a test executable-like sample
- policy report checks for download pipeline directories
- filesystem checks for quarantine/released/report directory bootstrap

## Known Limits

Phase 5 is a real quarantine foundation, not the final malware-analysis stack.

Current limits:

- download release is controlled to the session export directory, not an arbitrary operator-selected host path
- the BlackVault review is synchronous and local to the GTK shell
- the artifact scan service currently uses static heuristics only
- YARA, macro analysis, entropy scoring, EXIF stripping, and deep archive inspection are still future work
