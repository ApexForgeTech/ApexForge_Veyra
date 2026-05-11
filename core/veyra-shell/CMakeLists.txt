add_executable(veyra_shell
  src/main.cc
  src/startup_config.cc
)

target_include_directories(veyra_shell
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
)
