# cmake/toolchains/llvm-homebrew-21.cmake
# Use Homebrew-installed LLVM/Clang (llvm-21) on macOS.

cmake_minimum_required(VERSION 3.20)

# Tell CMake we're building for macOS (Darwin).
set(CMAKE_SYSTEM_NAME Darwin)

# --- Locate Homebrew LLVM root ------------------------------------------------
# You can override by passing -DLLVM_ROOT=/custom/path when configuring.
set(_llvm_candidates
    "$ENV{LLVM_ROOT}"
    "/opt/homebrew/opt/llvm@21"
    "/opt/homebrew/opt/llvm"
    "/usr/local/opt/llvm@21"
    "/usr/local/opt/llvm"
)

foreach(_cand IN LISTS _llvm_candidates)
    if(_cand AND EXISTS "${_cand}")
        set(LLVM_ROOT "${_cand}")
        break()
    endif()
endforeach()

if(NOT LLVM_ROOT)
    message(FATAL_ERROR
        "Homebrew LLVM not found. Set LLVM_ROOT or install with:\n"
        "  brew install llvm\n"
        "  (formula may be llvm or llvm@21)")
endif()

set(LLVM_BIN "${LLVM_ROOT}/bin")
set(LLVM_LIB "${LLVM_ROOT}/lib")
set(LLVM_CMAKE "${LLVM_LIB}/cmake/llvm")

# --- Compilers & binutils -----------------------------------------------------
# Force CMake to use Homebrew's clang tools.
set(CMAKE_C_COMPILER   "${LLVM_BIN}/clang"   CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${LLVM_BIN}/clang++" CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER "${LLVM_BIN}/clang"   CACHE FILEPATH "" FORCE)

# (Optional) Use LLD for faster linking. Comment these out to use Apple's ld.
# set(CMAKE_EXE_LINKER_FLAGS_INIT    "-fuse-ld=lld")
# set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")
# set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld")

# If you want to be explicit about the tools (usually not required):
find_program(CMAKE_AR      NAMES llvm-ar      PATHS "${LLVM_BIN}" NO_DEFAULT_PATH)
find_program(CMAKE_RANLIB  NAMES llvm-ranlib  PATHS "${LLVM_BIN}" NO_DEFAULT_PATH)
find_program(CMAKE_NM      NAMES llvm-nm      PATHS "${LLVM_BIN}" NO_DEFAULT_PATH)
find_program(CMAKE_STRIP   NAMES llvm-strip   PATHS "${LLVM_BIN}" NO_DEFAULT_PATH)
find_program(CMAKE_OBJDUMP NAMES llvm-objdump PATHS "${LLVM_BIN}" NO_DEFAULT_PATH)
find_program(CMAKE_OBJCOPY NAMES llvm-objcopy PATHS "${LLVM_BIN}" NO_DEFAULT_PATH)

# --- macOS SDK & deployment target -------------------------------------------
# Ensure the macOS SDK is used when compiling with non-Apple clang.
# If you already pass -DCMAKE_OSX_SYSROOT, this won't run.
if(NOT DEFINED CMAKE_OSX_SYSROOT)
    execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE _sdk
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_sdk AND EXISTS "${_sdk}")
        set(CMAKE_OSX_SYSROOT "${_sdk}" CACHE PATH "macOS SDK path" FORCE)
    endif()
endif()

# Set a sensible deployment target if the env var isn't present.
# You can override with -DCMAKE_OSX_DEPLOYMENT_TARGET=XX.Y
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET AND NOT DEFINED ENV{MACOSX_DEPLOYMENT_TARGET})
    # Choose something modern but safe; change to suit your needs.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

# --- Search paths so find_package(LLVM) etc. work -----------------------------
# Helps CMake locate LLVMConfig.cmake and friends.
set(CMAKE_PREFIX_PATH
    "${LLVM_ROOT}"
    "${CMAKE_PREFIX_PATH}"
)

# Also expose LLVM_DIR explicitly for find_package(LLVM CONFIG)
if(EXISTS "${LLVM_CMAKE}")
    set(LLVM_DIR "${LLVM_CMAKE}" CACHE PATH "Path to LLVMConfig.cmake" FORCE)
endif()

# --- C/C++ flags niceties (optional) ------------------------------------------
# Use libc++ (default on macOS) and enable color diagnostics.
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++ -fcolor-diagnostics")
set(CMAKE_C_FLAGS_INIT   "-fcolor-diagnostics")

# rpath: keep loader_path for installed/shared libs; append LLVM lib just in case.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "@loader_path;@loader_path/..;${LLVM_LIB}")

# --- Status -------------------------------------------------------------------
message(STATUS "Using Homebrew LLVM at: ${LLVM_ROOT}")
message(STATUS "Clang: ${CMAKE_C_COMPILER}")
message(STATUS "Clang++: ${CMAKE_CXX_COMPILER}")
message(STATUS "macOS SDK: ${CMAKE_OSX_SYSROOT}")
message(STATUS "Deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
