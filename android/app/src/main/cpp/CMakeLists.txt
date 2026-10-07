cmake_minimum_required(VERSION 3.22.1)

project(veyra_native)

set(VEYRA_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

add_library(veyra_native SHARED
    veyra_jni.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/engine/android_webview_engine.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/engine/browser_engine_factory.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/config/foundation_loader.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/config/schema_validator.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/config/startup_config.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/artifact_scan_service.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/browser_window.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/ai_orchestrator_client.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/osint_workspace_client.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/extension_engine.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/fingerprint_engine.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/shell_ui_bridge.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/permission_broker.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/tool_bridge_client.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/profile_manager.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/profile_registry.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/route_service.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/runtime_policy.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/security_policy_engine.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/session_lifecycle.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/session_partition.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/runtime/tab_model.cc
    ${VEYRA_ROOT}/core/veyra-shell/src/serialization/json.cc
)

target_include_directories(veyra_native PRIVATE
    ${VEYRA_ROOT}/core/veyra-shell/include
    ${VEYRA_ROOT}
)

find_library(log-lib log)

target_link_libraries(veyra_native ${log-lib})
