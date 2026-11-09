# Builds the resourcer for the build host when Cadmium is cross compiled (web, mingw), it needs zstd
# to pack resources, which a single compiler call can't build. With CADMIUM_HOST_HP48ROMASM_OUTPUT it
# also builds hp48romasm, which assembles the HP-48SX replacement ROM.
cmake_minimum_required(VERSION 3.20)
project(HostResourcer LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

foreach(required CADMIUM_SOURCE_DIR CADMIUM_GHC_FILESYSTEM_INCLUDE_DIR CADMIUM_ZSTD_SOURCE_DIR CADMIUM_HOST_RESOURCER_OUTPUT)
    if(NOT ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory("${CADMIUM_ZSTD_SOURCE_DIR}/build/cmake" zstd EXCLUDE_FROM_ALL)

add_executable(resourcer "${CADMIUM_SOURCE_DIR}/tools/resourcer.cpp")
target_include_directories(resourcer PRIVATE "${CADMIUM_GHC_FILESYSTEM_INCLUDE_DIR}" "${CADMIUM_ZSTD_SOURCE_DIR}/lib")
target_link_libraries(resourcer PRIVATE libzstd_static)
add_custom_command(
        TARGET resourcer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:resourcer>" "${CADMIUM_HOST_RESOURCER_OUTPUT}"
        VERBATIM
)

if(CADMIUM_HOST_HP48ROMASM_OUTPUT)
    foreach(required CADMIUM_FMT_INCLUDE_DIR CADMIUM_CHIPLET_INCLUDE_DIR)
        if(NOT ${required})
            message(FATAL_ERROR "${required} is required for hp48romasm")
        endif()
    endforeach()
    set(HP48_DIR "${CADMIUM_SOURCE_DIR}/src/emulation/hardware/hp48")
    add_executable(hp48romasm "${CADMIUM_SOURCE_DIR}/tools/hp48romasm.cpp" "${HP48_DIR}/asacpp/asacpp.cpp" "${HP48_DIR}/saturndecoder.cpp" "${HP48_DIR}/saturninstructions.cpp")
    target_include_directories(hp48romasm PRIVATE "${CADMIUM_SOURCE_DIR}/src" "${CADMIUM_GHC_FILESYSTEM_INCLUDE_DIR}" "${CADMIUM_FMT_INCLUDE_DIR}" "${CADMIUM_CHIPLET_INCLUDE_DIR}")
    target_compile_definitions(hp48romasm PRIVATE FMT_HEADER_ONLY)
    add_custom_command(
            TARGET hp48romasm POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:hp48romasm>" "${CADMIUM_HOST_HP48ROMASM_OUTPUT}"
            VERBATIM
    )
endif()
