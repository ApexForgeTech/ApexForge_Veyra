find_package(PkgConfig REQUIRED)
find_program(CARGO_EXECUTABLE cargo REQUIRED)
pkg_check_modules(WEBKITGTK41 QUIET IMPORTED_TARGET webkit2gtk-4.1)
pkg_check_modules(WEBKITGTK40 QUIET IMPORTED_TARGET webkit2gtk-4.0)

if(WEBKITGTK41_FOUND)
  set(VEYRA_WEBKITGTK_TARGET PkgConfig::WEBKITGTK41)
elseif(WEBKITGTK40_FOUND)
  set(VEYRA_WEBKITGTK_TARGET PkgConfig::WEBKITGTK40)
else()
  message(FATAL_ERROR
    "WebKitGTK development package is required for Phase 1. Install webkit2gtk-4.1 (preferred) or webkit2gtk-4.0.")
endif()

set(VEYRA_ROUTE_ENGINE_TARGET_DIR ${CMAKE_BINARY_DIR}/cargo-route)
set(VEYRA_ROUTE_ENGINE_BINARY ${CMAKE_BINARY_DIR}/route_engine)
set(VEYRA_ARTIFACT_SCAN_TARGET_DIR ${CMAKE_BINARY_DIR}/cargo-artifact-scan)
set(VEYRA_ARTIFACT_SCAN_BINARY ${CMAKE_BINARY_DIR}/artifact_scan)
set(VEYRA_TOOL_BRIDGE_TARGET_DIR ${CMAKE_BINARY_DIR}/cargo-tool-bridge)
set(VEYRA_TOOL_BRIDGE_BINARY ${CMAKE_BINARY_DIR}/tool_bridge)
set(VEYRA_SHELL_UI_DIST ${CMAKE_BINARY_DIR}/shell-ui-dist)
find_program(NPM_EXECUTABLE npm)

add_custom_command(
  OUTPUT ${VEYRA_ROUTE_ENGINE_BINARY}
  COMMAND ${CARGO_EXECUTABLE} build
          --manifest-path ${CMAKE_SOURCE_DIR}/core/route-engine/Cargo.toml
          --target-dir ${VEYRA_ROUTE_ENGINE_TARGET_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy
          ${VEYRA_ROUTE_ENGINE_TARGET_DIR}/debug/route_engine
          ${VEYRA_ROUTE_ENGINE_BINARY}
  DEPENDS
    ${CMAKE_SOURCE_DIR}/core/route-engine/Cargo.toml
    ${CMAKE_SOURCE_DIR}/core/route-engine/src/main.rs
  COMMENT "Building Phase 4 Rust route engine"
)

add_custom_target(veyra_route_engine ALL DEPENDS ${VEYRA_ROUTE_ENGINE_BINARY})

add_custom_command(
  OUTPUT ${VEYRA_ARTIFACT_SCAN_BINARY}
  COMMAND ${CARGO_EXECUTABLE} build
          --manifest-path ${CMAKE_SOURCE_DIR}/core/artifact-scan/Cargo.toml
          --target-dir ${VEYRA_ARTIFACT_SCAN_TARGET_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy
          ${VEYRA_ARTIFACT_SCAN_TARGET_DIR}/debug/artifact_scan
          ${VEYRA_ARTIFACT_SCAN_BINARY}
  DEPENDS
    ${CMAKE_SOURCE_DIR}/core/artifact-scan/Cargo.toml
    ${CMAKE_SOURCE_DIR}/core/artifact-scan/src/main.rs
  COMMENT "Building Phase 5 Rust artifact scan service"
)

add_custom_target(veyra_artifact_scan ALL DEPENDS ${VEYRA_ARTIFACT_SCAN_BINARY})

add_custom_command(
  OUTPUT ${VEYRA_TOOL_BRIDGE_BINARY}
  COMMAND ${CARGO_EXECUTABLE} build
          --manifest-path ${CMAKE_SOURCE_DIR}/core/tool-bridge/Cargo.toml
          --target-dir ${VEYRA_TOOL_BRIDGE_TARGET_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy
          ${VEYRA_TOOL_BRIDGE_TARGET_DIR}/debug/tool_bridge
          ${VEYRA_TOOL_BRIDGE_BINARY}
  DEPENDS
    ${CMAKE_SOURCE_DIR}/core/tool-bridge/Cargo.toml
    ${CMAKE_SOURCE_DIR}/core/tool-bridge/src/main.rs
  COMMENT "Building Phase 7 Rust tool bridge service"
)

add_custom_target(veyra_tool_bridge ALL DEPENDS ${VEYRA_TOOL_BRIDGE_BINARY})

if(NPM_EXECUTABLE)
  add_custom_command(
    OUTPUT ${VEYRA_SHELL_UI_DIST}/index.html
    COMMAND ${NPM_EXECUTABLE} install --prefer-offline
    COMMAND ${NPM_EXECUTABLE} run build
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/apps/shell-ui/dist
            ${VEYRA_SHELL_UI_DIST}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/apps/shell-ui
    DEPENDS
      ${CMAKE_SOURCE_DIR}/apps/shell-ui/package.json
      ${CMAKE_SOURCE_DIR}/apps/shell-ui/src/main.tsx
      ${CMAKE_SOURCE_DIR}/apps/shell-ui/src/App.tsx
    COMMENT "Building Phase 9 React shell UI"
  )
  add_custom_target(veyra_shell_ui ALL DEPENDS ${VEYRA_SHELL_UI_DIST}/index.html)
else()
  message(WARNING "npm not found. Shell UI (Phase 9) will not be built automatically. "
                  "Run 'npm install && npm run build' in apps/shell-ui/ manually.")
endif()

add_executable(veyra_shell
  src/engine/browser_engine_factory.cc
  src/engine/webkitgtk_browser_engine.cc
  src/config/foundation_loader.cc
  src/config/schema_validator.cc
  src/config/startup_config.cc
  src/main.cc
  src/runtime/artifact_scan_service.cc
  src/runtime/browser_window.cc
  src/runtime/ai_orchestrator_client.cc
  src/runtime/osint_workspace_client.cc
  src/runtime/extension_engine.cc
  src/runtime/fingerprint_engine.cc
  src/runtime/shell_ui_bridge.cc
  src/runtime/permission_broker.cc
  src/runtime/tool_bridge_client.cc
  src/runtime/profile_manager.cc
  src/runtime/profile_registry.cc
  src/runtime/route_service.cc
  src/runtime/runtime_policy.cc
  src/runtime/security_policy_engine.cc
  src/runtime/session_lifecycle.cc
  src/runtime/session_partition.cc
  src/runtime/tab_model.cc
  src/serialization/json.cc
)

target_include_directories(veyra_shell
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(veyra_shell
  PRIVATE
    ${VEYRA_WEBKITGTK_TARGET}
)

add_dependencies(veyra_shell veyra_route_engine veyra_artifact_scan veyra_tool_bridge)

if(NPM_EXECUTABLE)
  add_dependencies(veyra_shell veyra_shell_ui)
endif()

# Phase 10: stage the Python AI orchestrator next to the shell binary so it can
# be launched with --ai-orchestrator-script=<build>/veyra_ai_orchestrator.py
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/veyra_ai_orchestrator.py
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_SOURCE_DIR}/agents/ai-orchestrator/veyra_ai_orchestrator.py
          ${CMAKE_BINARY_DIR}/veyra_ai_orchestrator.py
  DEPENDS ${CMAKE_SOURCE_DIR}/agents/ai-orchestrator/veyra_ai_orchestrator.py
  COMMENT "Staging Phase 10 AI orchestrator"
)
add_custom_target(veyra_ai_orchestrator ALL
  DEPENDS ${CMAKE_BINARY_DIR}/veyra_ai_orchestrator.py)
add_dependencies(veyra_shell veyra_ai_orchestrator)

# Phase 11: stage the Python OSINT workspace next to the shell binary so it can
# be launched with --osint-workspace-script=<build>/veyra_osint_workspace.py
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/veyra_osint_workspace.py
  COMMAND ${CMAKE_COMMAND} -E copy
          ${CMAKE_SOURCE_DIR}/agents/osint-workspace/veyra_osint_workspace.py
          ${CMAKE_BINARY_DIR}/veyra_osint_workspace.py
  DEPENDS ${CMAKE_SOURCE_DIR}/agents/osint-workspace/veyra_osint_workspace.py
  COMMENT "Staging Phase 11 OSINT workspace"
)
add_custom_target(veyra_osint_workspace ALL
  DEPENDS ${CMAKE_BINARY_DIR}/veyra_osint_workspace.py)
add_dependencies(veyra_shell veyra_osint_workspace)
