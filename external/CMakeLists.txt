
add_subdirectory(fmt)

add_library(c_octo INTERFACE)
target_include_directories(c_octo INTERFACE c-octo/src)

add_library(rlguipp INTERFACE)
target_include_directories(rlguipp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(rlguipp INTERFACE raylib)
