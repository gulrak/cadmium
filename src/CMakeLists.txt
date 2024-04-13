add_subdirectory(emulation)

set(CADMIUM_SOURCE
    cadmium.cpp
    debugger.cpp
    debugger.hpp
    editor.cpp
    editor.hpp
    logview.cpp
    logview.hpp
    librarian.cpp
    librarian.hpp
    systemtools.cpp
    systemtools.hpp
    resourcemanager.cpp
    resourcemanager.hpp
    configuration.cpp
    configuration.hpp
    circularbuffer.cpp
    circularbuffer.hpp
    chip8emuhostex.cpp
    chip8emuhostex.hpp
    c8capturehost.cpp
    c8capturehost.hpp
    stylemanager.cpp
    stylemanager.hpp
    rlguippimpl.cpp
    c8db/database.hpp
)

if(WIN32)
    set(ICO_FILE_NAME "${PROJECT_BINARY_DIR}/resources/cadmium.ico")
    configure_file(${PROJECT_SOURCE_DIR}/cmake/cadmium.rc.in cadmium.rc)
    if(CMAKE_BUILD_TYPE EQUAL "Debug")
        add_executable(cadmium ${CADMIUM_SOURCE} cadmium.rc)
    else()
        add_executable(cadmium WIN32 ${CADMIUM_SOURCE} cadmium.rc)
    endif()
elseif(APPLE)
    set(CADMIUM_ICON "${PROJECT_BINARY_DIR}/resources/cadmium.icns")
    set_source_files_properties(${CADMIUM_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources" GENERATED 1)
    add_executable(cadmium MACOSX_BUNDLE ${CADMIUM_SOURCE} ${CADMIUM_ICON})
    if(WITH_FLAG_COCOA_GRAPHICS_SWITCHING)
        target_compile_definitions(cadmium PRIVATE WITH_FLAG_COCOA_GRAPHICS_SWITCHING)
    endif()
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
target_compile_definitions(cadmium PUBLIC CADMIUM_VERSION="${PROJECT_VERSION}" CADMIUM_VERSION_DECIMAL=${PROJECT_VERSION_DECIMAL} CADMIUM_GIT_HASH="${GIT_COMMIT_HASH}")
if(WEB_WITH_CLIPBOARD)
    target_compile_definitions(cadmium PUBLIC WEB_WITH_CLIPBOARD)
endif()
if(WEB_WITH_FETCHING)
    target_compile_definitions(cadmium PUBLIC WEB_WITH_FETCHING)
endif()
set_source_files_properties(src/editor.cpp PROPERTIES COMPILE_FLAGS "-Wno-unused-value")
target_link_libraries(cadmium PRIVATE emulation rlguipp resourcedata jsct ghc_filesystem)
target_code_coverage(cadmium)
if (${PLATFORM} MATCHES "Web")
    if(WEB_WITH_CLIPBOARD)
        if(USE_WEBGL)
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -sUSE_GLFW=3 -sASSERTIONS -sUSE_WEBGL2=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -sFETCH -s EXPORTED_RUNTIME_METHODS=[ccall] --no-heap-copy --shell-file ${PROJECT_SOURCE_DIR}/resources/minshell.html -sASYNCIFY -sASYNCIFY_IMPORTS=[copyClip,pasteClip]")
        else()
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -sASSERTIONS=1 -sUSE_GLFW=3 -sUSE_WEBGL2=0 -sWASM=1 -sALLOW_MEMORY_GROWTH=1 -sFETCH -sEXPORTED_RUNTIME_METHODS=[ccall,stackAlloc,stackSave,stackRestore] --no-heap-copy --shell-file ${PROJECT_SOURCE_DIR}/resources/minshell.html -sASYNCIFY -sASYNCIFY_IMPORTS=[copyClip,pasteClip]")
        endif()
    else()
        if(USE_WEBGL)
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -sFETCH -s EXPORTED_RUNTIME_METHODS=[ccall] --no-heap-copy --shell-file ${PROJECT_SOURCE_DIR}/resources/minshell.html")
        else()
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=0 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -sFETCH -s EXPORTED_RUNTIME_METHODS=[ccall] --no-heap-copy --shell-file ${PROJECT_SOURCE_DIR}/resources/minshell.html")
        endif()
    endif()
    set_target_properties(cadmium PROPERTIES OUTPUT_NAME "index")
    set_target_properties(cadmium PROPERTIES SUFFIX ".html")
endif ()

if(APPLE)
    install(TARGETS cadmium DESTINATION "." COMPONENT cadmium_app)
    install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION "." COMPONENT cadmium_app)
    install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION "." COMPONENT cadmium_app)
else()
    install(TARGETS cadmium DESTINATION . COMPONENT cadmium_app)
    install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION "." COMPONENT cadmium_app)
    install(FILES ${PROJECT_SOURCE_DIR}/README.md DESTINATION "." COMPONENT cadmium_app)
    if(UNIX)
        set(CPACK_COMPONENT_CADMIUM_APP_DESCRIPTION "A CHIP-8 variant emulation and development environment")
    endif()
endif()
