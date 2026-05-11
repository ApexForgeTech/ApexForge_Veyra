add_executable(veyra_shell
  src/config/foundation_loader.cc
  src/config/startup_config.cc
  src/main.cc
  src/runtime/browser_window.cc
  src/runtime/profile_manager.cc
  src/runtime/profile_registry.cc
  src/runtime/session_partition.cc
  src/runtime/tab_model.cc
  src/serialization/json.cc
)

target_include_directories(veyra_shell
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)
