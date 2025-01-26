if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS ON)
    add_compile_definitions(bit_CONFIG_SELECT_BIT=bit_BIT_NONSTD)
endif()

# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

math(EXPR PROJECT_VERSION_DECIMAL "${PROJECT_VERSION_MAJOR} * 10000 + (${PROJECT_VERSION_MINOR} * 100) + ${PROJECT_VERSION_PATCH}")
math(EXPR PROJECT_IS_WIP "${PROJECT_VERSION_PATCH} & 1")

find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
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

if(APPLE AND CMAKE_BUILD_TYPE EQUAL "Debug")
    execute_process(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE)
    set(CMAKE_OSX_ARCHITECTURES "${ARCHITECTURE}" CACHE STRING "Force universal binary 2 builds" FORCE)
endif()

if(EMSCRIPTEN)
    message(STATUS "Using emscripten, building for web...")
    set(PLATFORM "Web")
    set(USE_WEBGL2 "no" CACHE STRING "Ensure compatibility with older systems" FORCE)
    set(GRAPHICS GRAPHICS_API_OPENGL_21 CACHE BOOL "" FORCE)
    #set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -gsource-map")
    #set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -g3 -gsource-map")
    add_compile_options(-fexceptions)
    add_compile_definitions(WEB_WITH_FETCHING)
    add_link_options(-fexceptions)
    set(DATABASE_DEFAULT OFF)
else()
    set(PLATFORM "Desktop")
    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static"  CACHE STRING "Linker Flags for MinGW Builds" FORCE)
    endif()
    set(DATABASE_DEFAULT ON)
endif()

option(CADMIUM_WITH_DATABASE "Compile Cadmium with a database functionality, this links sqlite into Cadmium" ${DATABASE_DEFAULT})

include(CheckIncludeFile)
check_include_file(bcm_host.h HAS_BCMHOST)
if(HAS_BCMHOST AND NOT GRAPHICS)
    set(GRAPHICS GRAPHICS_API_OPENGL_21 CACHE BOOL "" FORCE)
endif()

option(WEB_WITH_CLIPBOARD "Build emscripten version supporting real clipboard (experimental)" OFF)

include(FetchContent)

find_package(Threads REQUIRED)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAYLIB_BRANCH_NAME "5.5")
if(RAYLIB_BRANCH_NAME STREQUAL HEAD)
    set(RAYLIB_BRANCH_NAME raylib-cadmium-1.0.6)
endif()
set(raylib_patch git apply ${PROJECT_SOURCE_DIR}/cmake/raylib.patch)
FetchContent_Declare(
    raylib
    #URL https://github.com/gulrak/raylib/archive/refs/heads/master.zip
    GIT_REPOSITORY https://github.com/gulrak/raylib.git
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG ${RAYLIB_BRANCH_NAME}
    GIT_SHALLOW TRUE
    PATCH_COMMAND ${raylib_patch}
    UPDATE_DISCONNECTED TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(raylib)

if (CADMIUM_WITH_DATABASE)
    add_subdirectory(${PROJECT_SOURCE_DIR}/external/sqlite3)
    set(SQLite3_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/external/sqlite3")
    set(SQLite3_LIBRARY SQLite::SQLite3)
    set(zxorm_patch git apply ${PROJECT_SOURCE_DIR}/cmake/zxorm.patch)
    FetchContent_Declare(
        ZxOrm
        GIT_REPOSITORY "https://github.com/crabmandable/zxorm.git"
        GIT_TAG "92cd52ec0530fb3e66061058548916fb879879e7"
        GIT_SHALLOW TRUE
        PATCH_COMMAND ${zxorm_patch}
        UPDATE_DISCONNECTED TRUE
    )
    FetchContent_MakeAvailable(ZxOrm)
endif()

if(PLATFORM STREQUAL "Desktop")
    set(LIBRESSL_TESTS OFF CACHE BOOL "" FORCE)
    set(LIBRESSL_APPS OFF CACHE BOOL "" FORCE)
    if(APPLE)
      set(ENABLE_ASM OFF CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(
        libressl
        URL "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-4.0.0.tar.gz"
        URL_MD5 4775b6b187a93c527eeb95a13e6ebd64
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(libressl)
    #target_compile_definitions(crypto PUBLIC LIBRESSL_TESTS=OFF LIBRESSL_APPS=OFF)
    #target_compile_definitions(ssl PUBLIC LIBRESSL_TESTS=OFF LIBRESSL_APPS=OFF)
    #target_compile_definitions(tls PUBLIC LIBRESSL_TESTS=OFF LIBRESSL_APPS=OFF)

    set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOl "" FORCE)
    FetchContent_Declare(
        CppHttplib
        GIT_REPOSITORY "https://github.com/yhirose/cpp-httplib.git"
        GIT_TAG "v0.18.3"
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(CppHttplib)
    include_directories(${cpphttplib_INCLUDE_DIR})
endif()

FetchContent_Declare(
        DocTest
        GIT_REPOSITORY "https://github.com/doctest/doctest.git"
        GIT_TAG "v2.4.9"
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(DocTest)
include_directories(${DOCTEST_INCLUDE_DIR})

FetchContent_Declare(
        Chiplet
        GIT_REPOSITORY "https://github.com/gulrak/chiplet.git"
        GIT_TAG "dev-road-to-2.0"
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Chiplet)
include_directories(${chiplet_SOURCE_DIR}/external)
include(${chiplet_SOURCE_DIR}/cmake/code-coverage.cmake)

FetchContent_Declare(
    Chip8TestSuite
    GIT_REPOSITORY "https://github.com/Timendus/chip8-test-suite.git"
    GIT_TAG "v4.2"
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Chip8TestSuite)

option(BUILD_DOC "Build documentation" ON)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    add_custom_target(doc_doxygen ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM )
else()
    message("Doxygen need to be installed to generate the doxygen documentation")
endif()
