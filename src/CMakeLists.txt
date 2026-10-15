add_subdirectory(emulation)

set(CADMIUM_SOURCE
    c8capturehost.cpp
    c8capturehost.hpp
    c8db/database.hpp
    cadmium.cpp
    circularbuffer.cpp
    circularbuffer.hpp
    configuration.cpp
    configuration.hpp
    debugger.cpp
    debugger.hpp
    editor.cpp
    editor.hpp
    emuhostex.cpp
    emuhostex.hpp
    highlighter.cpp
    highlighter.hpp
    headlessrun.cpp
    headlessrun.hpp
    librarian.cpp
    librarian.hpp
    logview.cpp
    logview.hpp
    resourcemanager.cpp
    resourcemanager.hpp
    stylemanager.cpp
    stylemanager.hpp
    systemtools.cpp
    systemtools.hpp
    texturescaler.hpp
)

if(CADMIUM_WITH_DATABASE)
    list(APPEND CADMIUM_SOURCE database.cpp database.hpp)
endif()

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
target_compile_definitions(cadmium PUBLIC
        CADMIUM_VERSION="${PROJECT_VERSION}"
        CADMIUM_VERSION_DECIMAL=${PROJECT_VERSION_DECIMAL}
        CADMIUM_GIT_HASH="${GIT_COMMIT_HASH}")
if(NOT EMSCRIPTEN)
    target_compile_definitions(cadmium PUBLIC CADMIUM_WITH_BACKGROUND_EMULATION)
    target_include_directories(cadmium PRIVATE
        "${raylib_SOURCE_DIR}/src/external/glfw/include")
endif()
if(CADMIUM_WITH_DATABASE)
    target_compile_definitions(cadmium PUBLIC CADMIUM_WITH_DATABASE)
    target_link_libraries(cadmium PUBLIC ghc::sqlite ssl httplib::httplib)
endif()
if(WEB_WITH_FETCHING)
    target_compile_definitions(cadmium PUBLIC WEB_WITH_FETCHING)
endif()
set_source_files_properties(src/editor.cpp PROPERTIES COMPILE_FLAGS "-Wno-unused-value")
target_link_libraries(cadmium PRIVATE emulation rlguipp resourcedata ghc_filesystem Threads::Threads)
target_code_coverage(cadmium)
if (${PLATFORM} MATCHES "Web")
    if(CADMIUM_WITH_HP48)
        # The adapter assembles the selected interpreter at runtime. Embed only
        # its sources and the default ROM, using the same virtual asset root.
        set(hp48WebAssets
            resources/sxrom-rev-e
            resources/research/chip48.asap
            resources/research/schip10.asap
            resources/research/schip11.asap
            resources/research/schpc.asap)
        foreach(asset IN LISTS hp48WebAssets)
            target_link_options(cadmium PRIVATE
                "SHELL:--embed-file \"${hp48sx_SOURCE_DIR}/${asset}@/hp48/${asset}\"")
            set_property(TARGET cadmium APPEND PROPERTY LINK_DEPENDS "${hp48sx_SOURCE_DIR}/${asset}")
        endforeach()
    endif()
    target_link_options(cadmium PRIVATE
        -sUSE_GLFW=3
        -sUSE_WEBGL2=0
        -sALLOW_MEMORY_GROWTH=1
        -sFETCH
        -sEXPORTED_RUNTIME_METHODS=['HEAPF32']
        --shell-file=${PROJECT_SOURCE_DIR}/resources/minshell.html)
    set_target_properties(cadmium PROPERTIES OUTPUT_NAME "index" SUFFIX ".html")
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
