add_executable(rlguipp_showcase
    showcase.cpp
    ../test/stylemanager.cpp
    ../test/stylemanager.hpp
    ../test/fs_impl.cpp)
target_link_libraries(rlguipp_showcase PUBLIC rlguipp fmt::fmt)
target_include_directories(rlguipp_showcase PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../test)

# Golden image tests, they render the showcase in a hidden window, so they need a display
if(NOT EMSCRIPTEN)
    add_test(NAME rlguipp-golden-images
        COMMAND rlguipp_showcase --golden ${CMAKE_CURRENT_SOURCE_DIR}/golden --out ${CMAKE_CURRENT_BINARY_DIR}/golden-out)
    set_tests_properties(rlguipp-golden-images PROPERTIES LABELS "gui")
endif()
