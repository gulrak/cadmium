cmake_minimum_required(VERSION 3.16)

project(cadmium VERSION 1.0.5 LANGUAGES C CXX)
cmake_policy(VERSION 3.16)

include(cmake/BuildSettings.cmake)
include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
enable_testing()
include_directories(src external)

add_subdirectory(external)
add_subdirectory(tools)
add_subdirectory(resources)

add_subdirectory(src)
add_subdirectory(test)
