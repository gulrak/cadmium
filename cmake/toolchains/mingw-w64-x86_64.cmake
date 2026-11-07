# Cross-compile macOS (Apple Silicon) -> Windows x86_64 using Homebrew mingw-w64

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Homebrew prefix
set(_brew "/opt/homebrew")
set(_triplet "x86_64-w64-mingw32")

# Compilers / binutils
find_program(CMAKE_C_COMPILER   NAMES ${_triplet}-gcc     HINTS "${_brew}/bin" REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES ${_triplet}-g++     HINTS "${_brew}/bin" REQUIRED)
find_program(CMAKE_RC_COMPILER  NAMES ${_triplet}-windres HINTS "${_brew}/bin")
find_program(CMAKE_AR           NAMES ${_triplet}-ar      HINTS "${_brew}/bin")
find_program(CMAKE_RANLIB       NAMES ${_triplet}-ranlib  HINTS "${_brew}/bin")
find_program(CMAKE_STRIP        NAMES ${_triplet}-strip   HINTS "${_brew}/bin")

# Helps CMake find the right headers/libs under the MinGW sysroot
execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -print-sysroot
        OUTPUT_VARIABLE _sysroot
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)

set(CMAKE_FIND_ROOT_PATH "")
if(_sysroot AND EXISTS "${_sysroot}")
    list(APPEND CMAKE_FIND_ROOT_PATH "${_sysroot}")
endif()
if(EXISTS "${_brew}/opt/mingw-w64")
    list(APPEND CMAKE_FIND_ROOT_PATH "${_brew}/opt/mingw-w64")
endif()

# Prevent accidental pickup of macOS host deps
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
