if(NOT EMSCRIPTEN)
    add_executable(rlguipp_test rlguipp_test.cpp stylemanager.cpp stylemanager.hpp fs_impl.cpp)
    target_link_libraries(rlguipp_test PUBLIC rlguipp fmt::fmt)

    add_executable(rlguipp_textedit_test textedit_test.cpp fs_impl.cpp)
    target_link_libraries(rlguipp_textedit_test PUBLIC rlguipp)
    add_test(NAME rlguipp-textedit COMMAND rlguipp_textedit_test)
    set_tests_properties(rlguipp-textedit PROPERTIES LABELS "gui")

    add_executable(rlguipp_codeeditor_test codeeditor_test.cpp fs_impl.cpp)
    target_link_libraries(rlguipp_codeeditor_test PUBLIC rlguipp)
    add_test(NAME rlguipp-codeeditor COMMAND rlguipp_codeeditor_test)
    set_tests_properties(rlguipp-codeeditor PROPERTIES LABELS "gui")

    add_executable(rlguipp_filetree_test filetree_test.cpp fs_impl.cpp)
    target_link_libraries(rlguipp_filetree_test PUBLIC rlguipp)
    add_test(NAME rlguipp-filetree COMMAND rlguipp_filetree_test)
    set_tests_properties(rlguipp-filetree PROPERTIES LABELS "gui")
endif()

add_executable(rlguipp_menu rlguipp_menu.cpp stylemanager.cpp stylemanager.hpp fs_impl.cpp)
target_link_libraries(rlguipp_menu PUBLIC rlguipp fmt::fmt)
