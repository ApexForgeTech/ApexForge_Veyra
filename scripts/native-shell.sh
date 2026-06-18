#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/manual"
OUT_BIN="$OUT_DIR/veyra_shell"
ROUTE_OUT_DIR="$ROOT_DIR/build/cargo-route"
ROUTE_BIN="$OUT_DIR/route_engine"
ARTIFACT_OUT_DIR="$ROOT_DIR/build/cargo-artifact-scan"
ARTIFACT_BIN="$OUT_DIR/artifact_scan"

MODE="run"
if [[ "${1:-}" == "build" || "${1:-}" == "run" || "${1:-}" == "smoke" ]]; then
  MODE="$1"
  shift
fi

if pkg-config --exists webkit2gtk-4.1; then
  WEBKIT_PKG="webkit2gtk-4.1"
elif pkg-config --exists webkit2gtk-4.0; then
  WEBKIT_PKG="webkit2gtk-4.0"
else
  echo "[ERROR] WebKitGTK dev package tapilmadi." >&2
  echo "Install edin: webkit2gtk-4.1 (preferred) veya webkit2gtk-4.0" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"

echo "[INFO] Building route engine -> $ROUTE_BIN"
cargo build \
  --manifest-path "$ROOT_DIR/core/route-engine/Cargo.toml" \
  --target-dir "$ROUTE_OUT_DIR"
cp "$ROUTE_OUT_DIR/debug/route_engine" "$ROUTE_BIN"
chmod +x "$ROUTE_BIN"

echo "[INFO] Building artifact scan service -> $ARTIFACT_BIN"
cargo build \
  --manifest-path "$ROOT_DIR/core/artifact-scan/Cargo.toml" \
  --target-dir "$ARTIFACT_OUT_DIR"
cp "$ARTIFACT_OUT_DIR/debug/artifact_scan" "$ARTIFACT_BIN"
chmod +x "$ARTIFACT_BIN"

COMMON_SRC=(
  "$ROOT_DIR/core/veyra-shell/src/engine/browser_engine_factory.cc"
  "$ROOT_DIR/core/veyra-shell/src/engine/webkitgtk_browser_engine.cc"
  "$ROOT_DIR/core/veyra-shell/src/config/foundation_loader.cc"
  "$ROOT_DIR/core/veyra-shell/src/config/schema_validator.cc"
  "$ROOT_DIR/core/veyra-shell/src/config/startup_config.cc"
  "$ROOT_DIR/core/veyra-shell/src/main.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/ai_orchestrator_client.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/osint_workspace_client.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/artifact_scan_service.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/browser_window.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/extension_engine.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/fingerprint_engine.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/shell_ui_bridge.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/permission_broker.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/tool_bridge_client.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/profile_manager.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/profile_registry.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/route_service.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/router_supervisor.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/runtime_policy.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/security_policy_engine.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/session_partition.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/session_lifecycle.cc"
  "$ROOT_DIR/core/veyra-shell/src/runtime/tab_model.cc"
  "$ROOT_DIR/core/veyra-shell/src/serialization/json.cc"
)

echo "[INFO] Building native shell -> $OUT_BIN"
g++ -std=c++17 \
  -I"$ROOT_DIR/core/veyra-shell/include" \
  -I"$ROOT_DIR" \
  $(pkg-config --cflags "$WEBKIT_PKG") \
  "${COMMON_SRC[@]}" \
  -o "$OUT_BIN" \
  $(pkg-config --libs "$WEBKIT_PKG")

echo "[INFO] Build complete"

if [[ "$MODE" == "build" ]]; then
  exit 0
fi

if [[ "$MODE" == "smoke" ]]; then
  exec "$OUT_BIN" --smoke "$@"
fi

exec "$OUT_BIN" "$@"
