add_subdirectory(emulation)

set(CADMIUM_SOURCE
    cadmium.cpp
    editor.cpp
    editor.hpp
    librarian.cpp
    librarian.hpp
    systemtools.cpp
    systemtools.hpp
    resourcemanager.cpp
    resourcemanager.hpp
)

if(WIN32)
    set(ICO_FILE_NAME "${PROJECT_BINARY_DIR}/resources/cadmium.ico")
    configure_file(${PROJECT_SOURCE_DIR}/cmake/cadmium.rc.in cadmium.rc)
    add_executable(cadmium WIN32 ${CADMIUM_SOURCE} cadmium.rc)
elseif(APPLE)
    set(CADMIUM_ICON "${PROJECT_BINARY_DIR}/resources/cadmium.icns")
    set_source_files_properties(${CADMIUM_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources" GENERATED 1)
    add_executable(cadmium MACOSX_BUNDLE ${CADMIUM_SOURCE} ${CADMIUM_ICON})
    set_target_properties(cadmium PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE_GUI_IDENTIFIER "net.gulrak.cadmium"
        MACOSX_BUNDLE_BUNDLE_NAME "cadmium"
        MACOSX_BUNDLE_INFO_STRING "Cadmium CHIP-8 Environment"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
        MACOSX_BUNDLE_ICON_FILE "cadmium.icns"
        MACOSX_BUNDLE_COPYRIGHT "Copyright (C) 2022 by Steffen Schümann"
        MACOSX_BUNDLE_INFO_PLIST ${PROJECT_SOURCE_DIR}/cmake/MacOSXBundleInfo.plist.in
    )
else()
    add_executable(cadmium ${CADMIUM_SOURCE})
endif()
if (NOT ${PLATFORM} MATCHES "Web")
    add_dependencies(cadmium cadmium_icon)
endif()
target_compile_definitions(cadmium PUBLIC CADMIUM_VERSION="${PROJECT_VERSION}" CADMIUM_GIT_HASH="${GIT_COMMIT_HASH}")
if(WEB_WITH_CLIPBOARD)
    target_compile_definitions(cadmium PUBLIC WEB_WITH_CLIPBOARD)
endif()
set_source_files_properties(src/editor.cpp PROPERTIES COMPILE_FLAGS "-Wno-unused-value")
target_link_libraries(cadmium PRIVATE emulation rlguipp resourcedata)
if (${PLATFORM} MATCHES "Web")
    if(WEB_WITH_CLIPBOARD)
        if(USE_WEBGL)
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=[ccall] --no-heap-copy --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/minshell.html -sASYNCIFY -sASYNCIFY_IMPORTS=[copyClip,pasteClip]")
        else()
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=0 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=[ccall] --no-heap-copy --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/minshell.html -sASYNCIFY -sASYNCIFY_IMPORTS=[copyClip,pasteClip]")
        endif()
    else()
        if(USE_WEBGL)
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=[ccall] --no-heap-copy --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/minshell.html")
        else()
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=0 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_RUNTIME_METHODS=[ccall] --no-heap-copy --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/minshell.html")
        endif()
    endif()
    set_target_properties(cadmium PROPERTIES OUTPUT_NAME "index")
    set_target_properties(cadmium PROPERTIES SUFFIX ".html")
endif ()
