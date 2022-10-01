if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS ON)
endif()

set(DEPENDENCY_FOLDER "external")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER ${DEPENDENCY_FOLDER})
option(CADMIUM_FLAT_OUTPUT "Combine outputs (binaries, libs and archives) in top level bin and lib directories." ON)
if(CADMIUM_FLAT_OUTPUT)
    link_directories(${CMAKE_BINARY_DIR}/lib)
    set(BINARY_OUT_DIR ${CMAKE_BINARY_DIR}/bin)
    set(LIB_OUT_DIR ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_OUT_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LIB_OUT_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUT_DIR})
    foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${BINARY_OUT_DIR})
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${LIB_OUT_DIR})
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${LIB_OUT_DIR})
    endforeach()
    message("using combined output directories ${BINARY_OUT_DIR} and ${LIB_OUT_DIR}")
else()
    message("not combining output")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type, available options: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif()

if(EMSCRIPTEN)
    set(PLATFORM "Web")
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -gsource-map")
    #set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -g3 -gsource-map")
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

find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()
