add_executable(rlguipp_showcase
    showcase.cpp
    ../test/stylemanager.cpp
    ../test/stylemanager.hpp
    ../test/fs_impl.cpp)
target_link_libraries(rlguipp_showcase PUBLIC rlguipp fmt::fmt)
target_include_directories(rlguipp_showcase PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../test)
