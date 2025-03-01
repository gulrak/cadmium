cmake_minimum_required(VERSION 3.16)
project(RlGuiPp)

add_library(rlguipp STATIC rlguipp.cpp rlguipp.hpp raygui4.h rlunicode.h dm_property_list.h)
target_include_directories(rlguipp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(rlguipp PUBLIC raylib_static ghc_filesystem fmt::fmt)

add_subdirectory(test)