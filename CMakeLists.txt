cmake_minimum_required(VERSION 3.16)

project(chip8rl VERSION 0.9.7 LANGUAGES C CXX)
cmake_policy(VERSION 3.16)

include(cmake/BuildSettings.cmake)
include_directories(src external)

add_subdirectory(external)
add_subdirectory(tools)
add_subdirectory(resources)


add_subdirectory(src)
add_subdirectory(test)
