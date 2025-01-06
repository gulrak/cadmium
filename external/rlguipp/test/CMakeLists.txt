if(NOT EMSCRIPTEN)
    add_executable(rlguipp_test rlguipp_test.cpp stylemanager.cpp stylemanager.hpp)
    target_link_libraries(rlguipp_test PUBLIC rlguipp fmt::fmt)
endif()
