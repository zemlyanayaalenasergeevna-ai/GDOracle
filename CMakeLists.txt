cmake_minimum_required(VERSION 3.21)
project(GDOracle VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(GDOracle SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
  message(FATAL_ERROR "GEODE_SDK is not set.")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(GDOracle)
