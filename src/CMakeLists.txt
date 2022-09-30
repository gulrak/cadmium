add_subdirectory(emulation)

add_executable(cadmium
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

target_compile_definitions(cadmium PUBLIC CADMIUM_VERSION="${PROJECT_VERSION}")
if(WEB_WITH_CLIPBOARD)
    target_compile_definitions(cadmium PUBLIC WEB_WITH_CLIPBOARD)
endif()
set_source_files_properties(src/emulation/chip8compiler.cpp PROPERTIES COMPILE_FLAGS "-fpermissive -Wno-write-strings")
target_link_libraries(cadmium PRIVATE emulation rlguipp resourcedata)
if (${PLATFORM} MATCHES "Web")
    if(WEB_WITH_CLIPBOARD)
        if(USE_WEBGL)
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 --no-heap-copy --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/minshell.html -sASYNCIFY -sASYNCIFY_IMPORTS=[copyClip,pasteClip]")
        else()
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=0 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 --no-heap-copy --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/minshell.html -sASYNCIFY -sASYNCIFY_IMPORTS=[copyClip,pasteClip]")
        endif()
    else()
        if(USE_WEBGL)
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=1 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 --no-heap-copy --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/minshell.html")
        else()
            set_target_properties(cadmium PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=0 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 --no-heap-copy --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/src/minshell.html")
        endif()
    endif()
    set_target_properties(cadmium PROPERTIES OUTPUT_NAME "index")
    set_target_properties(cadmium PROPERTIES SUFFIX ".html")
endif ()
