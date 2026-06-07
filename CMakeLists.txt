cmake_minimum_required(VERSION 3.20)

project(Veyra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "Veyra Phase 1 native shell is Linux-first. Build is currently supported only on Linux.")
endif()

add_subdirectory(core/veyra-shell)
