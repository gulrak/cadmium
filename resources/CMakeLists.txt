file(GLOB
        RESOURCE_FILES
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/files/*.png
        ${CMAKE_CURRENT_SOURCE_DIR}/files/*.ttf
        ${CMAKE_CURRENT_SOURCE_DIR}/files/*.vs
        ${CMAKE_CURRENT_SOURCE_DIR}/files/*.fs
        ${CMAKE_CURRENT_SOURCE_DIR}/files/sxrom-rev-e
        ${CMAKE_CURRENT_SOURCE_DIR}/files/hp-chip8/*.asap
        ${CMAKE_CURRENT_SOURCE_DIR}/files/library/*.cd48db
)

# ------------------------------------------------------------
# Detect when build-time tools must be HOST binaries
# ------------------------------------------------------------
set(NEED_HOST_TOOLS FALSE)

# Web / emscripten
if (${PLATFORM} MATCHES "Web")
    set(NEED_HOST_TOOLS TRUE)
endif()

# macOS host -> mingw-w64 (Windows) cross compile (matches your toolchain file)
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin"
        AND CMAKE_CROSSCOMPILING
        AND CMAKE_SYSTEM_NAME STREQUAL "Windows"
        AND (CMAKE_CXX_COMPILER MATCHES "w64-mingw32"
        OR CMAKE_C_COMPILER  MATCHES "w64-mingw32"))
    set(NEED_HOST_TOOLS TRUE)
endif()

# Host compiler to build host tools
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    set(HOST_CC gcc)
    set(HOST_CXX g++)
else()
    # Prefer system clang++ on macOS to avoid PATH toolchain surprises
    if (EXISTS "/usr/bin/clang++")
        set(HOST_CC /usr/bin/clang)
        set(HOST_CXX /usr/bin/clang++)
    else()
        set(HOST_CC clang)
        set(HOST_CXX clang++)
    endif()
endif()

# Which binaries to run during the build, and what to depend on
if (NEED_HOST_TOOLS)
    set(RESOURCER_RUNNER     ${CMAKE_CURRENT_BINARY_DIR}/resourcer_host)
    set(ICONGENERATOR_RUNNER ${CMAKE_CURRENT_BINARY_DIR}/icongenerator_host)

    # Build host resourcer, as a small project of its own, as it needs zstd to pack resources
    add_custom_command(
            OUTPUT resourcer_host
            COMMAND ${CMAKE_COMMAND}
            -S ${PROJECT_SOURCE_DIR}/cmake/host-resourcer
            -B ${CMAKE_CURRENT_BINARY_DIR}/host-resourcer-build
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_C_COMPILER=${HOST_CC}
            -DCMAKE_CXX_COMPILER=${HOST_CXX}
            -DCADMIUM_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -DCADMIUM_GHC_FILESYSTEM_INCLUDE_DIR=${PROJECT_BINARY_DIR}/_deps/ghcfilesystem-src/include
            -DCADMIUM_ZSTD_SOURCE_DIR=${zstd_SOURCE_DIR}
            -DCADMIUM_HOST_RESOURCER_OUTPUT=${RESOURCER_RUNNER}
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_CURRENT_BINARY_DIR}/host-resourcer-build --target resourcer --config Release
            DEPENDS ${PROJECT_SOURCE_DIR}/tools/resourcer.cpp ${PROJECT_SOURCE_DIR}/cmake/host-resourcer/CMakeLists.txt
            VERBATIM
    )

    # Build host icongenerator  (adjust source(s) if needed)
    add_custom_command(
            OUTPUT icongenerator_host
            COMMAND ${HOST_CXX} -o icongenerator_host -std=c++17
            ${PROJECT_SOURCE_DIR}/tools/icongenerator.cpp -I${PROJECT_SOURCE_DIR}/external -DCADMIUM_VERSION="${PROJECT_VERSION}"
            DEPENDS ${PROJECT_SOURCE_DIR}/tools/icongenerator.cpp
            VERBATIM
    )

    # Convenient phony targets for dependencies
    add_custom_target(resourcer_tool     DEPENDS resourcer_host)
    set(RESOURCER_DEPENDS ${RESOURCER_RUNNER})
    add_custom_target(icongenerator_tool DEPENDS icongenerator_host)
else()
    # Native case: use the normal targets you already have elsewhere
    set(RESOURCER_RUNNER     resourcer)
    set(ICONGENERATOR_RUNNER icongenerator)

    add_custom_target(resourcer_tool     DEPENDS resourcer)
    set(RESOURCER_DEPENDS resourcer)
    add_custom_target(icongenerator_tool DEPENDS icongenerator)
endif()

# ------------------------------------------------------------
# Icon generation (now works for native, Web, and mingw cross)
# ------------------------------------------------------------
if(APPLE)
    add_custom_command(
            OUTPUT cadmium-icon.png cadmium.icns
            COMMAND ${ICONGENERATOR_RUNNER}
            ${CMAKE_CURRENT_SOURCE_DIR}/files/cadmium-title.png
            ${CMAKE_CURRENT_SOURCE_DIR}/files/micro-font.png
            cadmium-icon.png $<$<BOOL:${NEED_HOST_TOOLS}>:--win32>
            COMMAND iconutil -c icns ${CMAKE_CURRENT_BINARY_DIR}/cadmium.iconset
            DEPENDS icongenerator_tool ${RESOURCE_FILES}
            VERBATIM
    )
    add_custom_target(cadmium_icon ALL DEPENDS cadmium.icns)
else()
    add_custom_command(
            OUTPUT cadmium-icon.png
            COMMAND ${ICONGENERATOR_RUNNER}
            ${CMAKE_CURRENT_SOURCE_DIR}/files/cadmium-title.png
            ${CMAKE_CURRENT_SOURCE_DIR}/files/micro-font.png
            cadmium-icon.png $<$<BOOL:${NEED_HOST_TOOLS}>:--win32>
            DEPENDS icongenerator_tool ${RESOURCE_FILES}
            VERBATIM
    )
    add_custom_target(cadmium_icon DEPENDS cadmium-icon.png)
endif()

# ------------------------------------------------------------
# Resource data generation (now works for native, Web, and mingw cross)
# ------------------------------------------------------------
# The HP-48SX ROM may lie in files/ while the core is disabled, then neither it nor the interpreter
# sources are compiled in.
set(RESOURCE_EXCLUDES)
if(NOT CADMIUM_WITH_HP48)
    set(RESOURCE_EXCLUDES --exclude sxrom-rev-e --exclude hp-chip8)
endif()
# The starter metadata of the library is only of use with the database (not on the web).
if(NOT CADMIUM_WITH_DATABASE)
    list(APPEND RESOURCE_EXCLUDES --exclude library)
endif()
add_custom_command(
        OUTPUT resourcedata.c
        COMMAND ${RESOURCER_RUNNER}
        ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c
        ${CMAKE_CURRENT_SOURCE_DIR}/files/
        ${RESOURCE_EXCLUDES}
        # the resourcer itself, not only its target, so a changed resource format regenerates the data
        DEPENDS resourcer_tool ${RESOURCER_DEPENDS} ${RESOURCE_FILES}
        VERBATIM
)

add_library(resourcedata STATIC ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c)
