cmake_minimum_required(VERSION 3.16)

project(chip8rl VERSION 0.9.4 LANGUAGES C CXX)
cmake_policy(VERSION 3.16)

if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS ON)
endif()

#add_compile_options(-fsanitize=address)
#add_link_options(-fsanitize=address)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type, available options: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif()

if(EMSCRIPTEN)
    set(PLATFORM "Web")
else()
    set(PLATFORM "Desktop")
endif()

include(CheckIncludeFile)
check_include_file(bcm_host.h HAS_BCMHOST)
if(HAS_BCMHOST AND NOT GRAPHICS)
    set(GRAPHICS GRAPHICS_API_OPENGL_21 CACHE BOOL "" FORCE)
endif()

option(WEB_WITH_CLIPBOARD "Build emscripten version supporting real clipboard (experimental)" OFF)

include(FetchContent)

if(RAYLIB_BRANCH_NAME STREQUAL HEAD)
    set(RAYLIB_BRANCH_NAME master)
endif()
FetchContent_Declare(
    raylib
    #URL https://github.com/gulrak/raylib/archive/refs/heads/master.zip
    GIT_REPOSITORY https://github.com/gulrak/raylib.git
    #GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG ${RAYLIB_BRANCH_NAME}
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(raylib)
if (NOT raylib_POPULATED)
    set(FETCHCONTENT_QUIET NO)
    FetchContent_Populate(raylib)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})
endif()

FetchContent_Declare(
    DocTest
    GIT_REPOSITORY "https://github.com/doctest/doctest.git"
    GIT_TAG "v2.4.9"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(DocTest)
include_directories(${DOCTEST_INCLUDE_DIR})

include_directories(src external)
add_subdirectory(external)

add_subdirectory(tools)
add_subdirectory(resources)

add_library(c_octo INTERFACE)
target_include_directories(c_octo INTERFACE external/c-octo/src)

add_library(rlguipp INTERFACE)
target_include_directories(rlguipp INTERFACE src)
target_link_libraries(rlguipp INTERFACE raylib)

add_subdirectory(src)
add_subdirectory(test)
