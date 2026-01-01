file(GLOB
        RESOURCE_FILES
        CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/files/*.png
        ${CMAKE_SOURCE_DIR}/files/*.ttf
        ${CMAKE_SOURCE_DIR}/files/*.vs
        ${CMAKE_SOURCE_DIR}/files/*.fs
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
    set(HOST_CXX g++)
else()
    # Prefer system clang++ on macOS to avoid PATH toolchain surprises
    if (EXISTS "/usr/bin/clang++")
        set(HOST_CXX /usr/bin/clang++)
    else()
        set(HOST_CXX clang++)
    endif()
endif()

# Which binaries to run during the build, and what to depend on
if (NEED_HOST_TOOLS)
    set(RESOURCER_RUNNER     ${CMAKE_CURRENT_BINARY_DIR}/resourcer_host)
    set(ICONGENERATOR_RUNNER ${CMAKE_CURRENT_BINARY_DIR}/icongenerator_host)

    # Build host resourcer
    add_custom_command(
            OUTPUT resourcer_host
            COMMAND ${HOST_CXX} -o resourcer_host -std=c++17
            ${PROJECT_SOURCE_DIR}/tools/resourcer.cpp
            -I${PROJECT_BINARY_DIR}/_deps/ghcfilesystem-src/include
            DEPENDS ${PROJECT_SOURCE_DIR}/tools/resourcer.cpp
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
    add_custom_target(icongenerator_tool DEPENDS icongenerator_host)
else()
    # Native case: use the normal targets you already have elsewhere
    set(RESOURCER_RUNNER     resourcer)
    set(ICONGENERATOR_RUNNER icongenerator)

    add_custom_target(resourcer_tool     DEPENDS resourcer)
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
add_custom_command(
        OUTPUT resourcedata.c
        COMMAND ${RESOURCER_RUNNER}
        ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c
        ${CMAKE_CURRENT_SOURCE_DIR}/files/
        DEPENDS resourcer_tool ${RESOURCE_FILES}
        VERBATIM
)

add_library(resourcedata STATIC ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c)
