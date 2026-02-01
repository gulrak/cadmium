if(NOT EMSCRIPTEN)
    add_executable(rlguipp_test rlguipp_test.cpp stylemanager.cpp stylemanager.hpp fs_impl.cpp)
    target_link_libraries(rlguipp_test PUBLIC rlguipp fmt::fmt)
endif()

#add_executable(rlguipp_menu rlguipp_menu.cpp stylemanager.cpp stylemanager.hpp fs_impl.cpp)
#target_link_libraries(rlguipp_menu PUBLIC rlguipp fmt::fmt)
