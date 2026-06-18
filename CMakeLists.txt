cmake_minimum_required(VERSION 3.20)

project(Veyra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
  message(WARNING "Veyra Native Shell is primarily tested on Linux. Build on ${CMAKE_SYSTEM_NAME} is experimental.")
endif()

add_subdirectory(core/veyra-shell)
