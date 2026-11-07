cmake_minimum_required(VERSION 3.16)
project(RlGuiPp)

set(RLGUIPP_RAYLIB_TARGET raylib_static CACHE STRING "Name of the raylib target rlguipp links against")

add_library(rlguipp STATIC
    rlguipp.cpp rlguipp.hpp
    rlguipp_filedialog.cpp rlguipp_filedialog.hpp
    rlguipp_leds.cpp rlguipp_leds.hpp
    rlguiapp.cpp rlguiapp.hpp
    rlguipp_font.cpp rlguipp_font.hpp
    raygui4.5.h icons.h
    syshelper.cpp syshelper.hpp)
target_include_directories(rlguipp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(rlguipp PUBLIC ${RLGUIPP_RAYLIB_TARGET} ghc_filesystem)

add_subdirectory(test)
add_subdirectory(tools)
add_subdirectory(examples)