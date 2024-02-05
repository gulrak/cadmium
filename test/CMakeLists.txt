
if(FALSE)
    # JamesGriffin/CHIP-8
    file(DOWNLOAD https://raw.githubusercontent.com/JamesGriffin/CHIP-8-Emulator/master/src/chip8.h ${CMAKE_CURRENT_SOURCE_DIR}/cores/jgriffin/chip8.h)
    file(DOWNLOAD https://raw.githubusercontent.com/JamesGriffin/CHIP-8-Emulator/master/src/chip8.cpp ${CMAKE_CURRENT_SOURCE_DIR}/cores/jgriffin/chip8.cpp)

    # wernsey/chip8
    file(DOWNLOAD https://raw.githubusercontent.com/wernsey/chip8/master/chip8.h ${CMAKE_CURRENT_SOURCE_DIR}/cores/wernsey/chip8.h)
    file(DOWNLOAD https://raw.githubusercontent.com/wernsey/chip8/master/chip8.c ${CMAKE_CURRENT_SOURCE_DIR}/cores/wernsey/chip8.c)
endif()

add_executable(chip8-fpcore-tests main.cpp basic_opcode_tests.cpp variant_specific_opcode_tests.cpp chip8adapter.hpp chip8adapter.cpp)
target_compile_definitions(chip8-fpcore-tests PUBLIC TEST_CHIP8EMULATOR_FP=1 C8CORE="C8FP:")
target_link_libraries(chip8-fpcore-tests PRIVATE doctest emulation)
doctest_discover_tests(chip8-fpcore-tests)

add_executable(chip8-tscore-tests main.cpp basic_opcode_tests.cpp variant_specific_opcode_tests.cpp chip8adapter.hpp chip8adapter.cpp)
target_compile_definitions(chip8-tscore-tests PUBLIC TEST_CHIP8EMULATOR_TS C8CORE="C8TS:")
target_link_libraries(chip8-tscore-tests PRIVATE doctest emulation)
doctest_discover_tests(chip8-tscore-tests)

add_executable(chip8-strictcore-tests main.cpp basic_opcode_tests.cpp variant_specific_opcode_tests.cpp chip8adapter.hpp chip8adapter.cpp)
target_compile_definitions(chip8-strictcore-tests PUBLIC TEST_CHIP8EMULATOR_STRICT C8CORE="C8ST:")
target_link_libraries(chip8-strictcore-tests PRIVATE doctest emulation)
doctest_discover_tests(chip8-strictcore-tests)

add_executable(chip8-vipcore-tests main.cpp basic_opcode_tests.cpp variant_specific_opcode_tests.cpp chip8adapter.hpp chip8adapter.cpp)
target_compile_definitions(chip8-vipcore-tests PUBLIC TEST_CHIP8VIP C8CORE="C8RV:")
target_link_libraries(chip8-vipcore-tests PRIVATE doctest emulation)
doctest_discover_tests(chip8-vipcore-tests)

add_executable(chip8-dreamcore-tests main.cpp basic_opcode_tests.cpp variant_specific_opcode_tests.cpp chip8adapter.hpp chip8adapter.cpp)
target_compile_definitions(chip8-dreamcore-tests PUBLIC TEST_CHIP8DREAM C8CORE="C8RD:")
target_link_libraries(chip8-dreamcore-tests PRIVATE doctest emulation)
doctest_discover_tests(chip8-dreamcore-tests)

add_executable(cdp1802test cdp1802test.cpp)
target_link_libraries(cdp1802test emulation)
#doctest_discover_tests(chip8-fpcore-tests)

#add_executable(heuristicint-tests main.cpp heuristicint_tests.cpp)
#doctest_discover_tests(heuristicint-tests)
#target_link_libraries(heuristicint-tests PRIVATE doctest fmt::fmt)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cores/m6800")
    add_subdirectory(cores/m6800)
    set(M6800_EXTERN_CORE "ExorSimCore")
endif()
add_executable(m6800test m6800test.cpp fuzzer.cpp)
target_compile_definitions(m6800test PUBLIC M6800TEST_VERSION="${PROJECT_VERSION}" M6800TEST_GIT_HASH="${GIT_COMMIT_HASH}" CADMIUM_WITH_GENERIC_CPU)
if(M6800_EXTERN_CORE)
    target_compile_definitions(m6800test PRIVATE M6800_EXTERN_CORE=${M6800_EXTERN_CORE})
    target_link_libraries(m6800test PRIVATE m6800verify emulation)
else()
    target_link_libraries(m6800test PRIVATE emulation)
endif()
#doctest_discover_tests(chip8-fpcore-tests)

add_executable(variantset-tests main.cpp variantset_test.cpp)
target_link_libraries(variantset-tests PUBLIC doctest emulation fmt::fmt)
doctest_discover_tests(variantset-tests)

add_executable(time-tests main.cpp time_test.cpp)
target_link_libraries(time-tests PUBLIC doctest emulation)
doctest_discover_tests(time-tests)

if (${PLATFORM} MATCHES "Web")
    add_executable(web_test web_test.cpp)
    target_link_libraries(web_test PRIVATE raylib)
    set_target_properties(web_test PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=0 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -sFETCH -s EXPORTED_RUNTIME_METHODS=[ccall] --no-heap-copy --shell-file ${PROJECT_SOURCE_DIR}/resources/minshell.html -sASYNCIFY -sASYNCIFY_IMPORTS=[copyClip,pasteClip]")
    set_target_properties(web_test PROPERTIES OUTPUT_NAME "webtest")
    set_target_properties(web_test PROPERTIES SUFFIX ".html")
endif ()