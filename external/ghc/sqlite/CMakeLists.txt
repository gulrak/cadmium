cmake_minimum_required(VERSION 3.21)
if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()
project(ghc-sqlite VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

include(CheckIncludeFileCXX)
include(FetchContent)
include(CTest)
enable_testing()

if(NOT PROJECT_IS_TOP_LEVEL AND NOT TARGET fmt::fmt AND NOT USE_FMTLIB_POLYFILL)
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -std=c++20")
    set(_fmt_test_source "
    #include <format>
    int main() {
      auto s = std::format(\"{} {}\", 1, 2);
      (void)s;
      return 0;
    }")
    check_cxx_source_compiles("${_fmt_test_source}" HAVE_WORKING_STD_FORMAT)

    if(NOT HAVE_WORKING_STD_FORMAT)
    message(STATUS "std::format not usable; enabling polyfill (e.g. fmtlib).")
    SET(USE_FMTLIB_POLYFILL ON)
    endif()

    # check_include_file_cxx("format" CXX_FORMAT_SUPPORT)
    if(USE_FMTLIB_POLYFILL)
    message(STATUS "Can't use std::format, enabling fmtlib polyfill")
    set(FMT_TEST FF CACHE INTERNAL "disabling fmt tests")
    FetchContent_Declare(
            fmt
            GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
            GIT_TAG         11.2.0
            GIT_SHALLOW     TRUE
    )
    FetchContent_MakeAvailable(fmt)
    endif()
else()
    set(USE_FMTLIB_POLYFILL ON)
endif()

add_subdirectory(test)

add_library(ghc-sqlite-static STATIC src/sqlite.cpp include/ghc/sqlite.hpp src/sqlite3/sqlite3.c src/sqlite3/sqlite3.h src/sqlite3/sqlite3ext.h include/ghc/logger.hpp)
target_include_directories(ghc-sqlite-static PUBLIC include)
if(USE_FMTLIB_POLYFILL)
    target_link_libraries(ghc-sqlite-static PUBLIC fmt::fmt)
    target_compile_definitions(ghc-sqlite-static PUBLIC USE_FMTLIB_POLYFILL)
endif()
if(GHC_CUSTOM_LOGGER_PROVIDED)
    target_compile_definitions(ghc-sqlite-static PUBLIC GHC_CUSTOM_LOGGER_PROVIDED=\"${GHC_CUSTOM_LOGGER_PROVIDED}\")
endif()
add_library(ghc::sqlite ALIAS ghc-sqlite-static)
