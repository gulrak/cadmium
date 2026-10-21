
if(FALSE)
    # JamesGriffin/CHIP-8
    file(DOWNLOAD https://raw.githubusercontent.com/JamesGriffin/CHIP-8-Emulator/master/src/chip8.h ${CMAKE_CURRENT_SOURCE_DIR}/cores/jgriffin/chip8.h)
    file(DOWNLOAD https://raw.githubusercontent.com/JamesGriffin/CHIP-8-Emulator/master/src/chip8.cpp ${CMAKE_CURRENT_SOURCE_DIR}/cores/jgriffin/chip8.cpp)

    # wernsey/chip8
    file(DOWNLOAD https://raw.githubusercontent.com/wernsey/chip8/master/chip8.h ${CMAKE_CURRENT_SOURCE_DIR}/cores/wernsey/chip8.h)
    file(DOWNLOAD https://raw.githubusercontent.com/wernsey/chip8/master/chip8.c ${CMAKE_CURRENT_SOURCE_DIR}/cores/wernsey/chip8.c)
endif()

if(chip8testsuite_SOURCE_DIR)
    add_executable(chip8_emulation_tests main.cpp basic_opcode_tests.cpp test_suite_tests.cpp superchip_tests.cpp chip8testhelper.hpp)
    add_compile_definitions(chip8_emulation_tests PUBLIC CHIP8_TEST_SUITE="${chip8testsuite_SOURCE_DIR}" TEST_ROM_FOLDER="${PROJECT_SOURCE_DIR}/test-roms/bin")
else()
    add_executable(chip8_emulation_tests main.cpp basic_opcode_tests.cpp chip8testhelper.hpp)
endif()
target_compile_definitions(chip8_emulation_tests PRIVATE WITHOUT_STB_IMAGE)
target_link_libraries(chip8_emulation_tests PUBLIC doctest emulation)
target_code_coverage(chip8_emulation_tests AUTO ALL)
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND AND TARGET cadmium AND NOT EMSCRIPTEN)
    add_test(NAME headless-cli
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/headless_cli_test.py
            $<TARGET_FILE:cadmium>)
    set_tests_properties(headless-cli PROPERTIES TIMEOUT 60)
endif()
if(Python3_Interpreter_FOUND AND TARGET c8sst-gen)
    add_test(NAME c8sst-generator
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/c8sst_gen_test.py
            $<TARGET_FILE:c8sst-gen> $<TARGET_FILE:c8sst-run>)
    set_tests_properties(c8sst-generator PROPERTIES TIMEOUT 360)
endif()
target_include_directories(chip8_emulation_tests PUBLIC ${PROJECT_SOURCE_DIR}/external)
doctest_discover_tests(chip8_emulation_tests)

add_executable(eti660-tests main.cpp eti660_tests.cpp)
target_compile_definitions(eti660-tests PRIVATE WITHOUT_STB_IMAGE)
if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/docs/ETI-660/Chip-8 ETI660 Programs")
    target_compile_definitions(eti660-tests PRIVATE ETI660_EXAMPLES="${PROJECT_SOURCE_DIR}/docs/ETI-660")
endif()
target_link_libraries(eti660-tests PRIVATE doctest emulation)
doctest_discover_tests(eti660-tests)

if(CADMIUM_WITH_HP48)
    add_executable(hp48sx-tests main.cpp hp48sx_tests.cpp)
    target_compile_definitions(hp48sx-tests PRIVATE WITHOUT_STB_IMAGE)
    target_link_libraries(hp48sx-tests PRIVATE doctest emulation)
    doctest_discover_tests(hp48sx-tests PROPERTIES TIMEOUT 300)
endif()

add_executable(hex-rom-tests main.cpp hex_rom_tests.cpp host_lifecycle_tests.cpp property_ids_tests.cpp property_rules_tests.cpp
    ../src/emuhostex.cpp ../src/librarian.cpp ../src/configuration.cpp ../src/systemtools.cpp)
target_compile_definitions(hex-rom-tests PRIVATE WITHOUT_STB_IMAGE CADMIUM_VERSION="${PROJECT_VERSION}")
if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/docs/ETI-660/Chip-8 ETI660 Hybrids")
    target_compile_definitions(hex-rom-tests PRIVATE ETI660_EXAMPLES="${PROJECT_SOURCE_DIR}/docs/ETI-660")
endif()
target_link_libraries(hex-rom-tests PRIVATE doctest emulation Threads::Threads)
doctest_discover_tests(hex-rom-tests)
#if (OR NOT DEFINED ENV{CI})
#    add_custom_command(
#        TARGET chip8_emulation_tests
#        COMMENT "Run tests"
#        POST_BUILD
#        COMMAND chip8_emulation_tests
#    )
#    add_dependencies(cadmium chip8_emulation_tests)
#endif()

add_executable(cdp1802test cdp1802test.cpp)
target_link_libraries(cdp1802test emulation)
target_code_coverage(cdp1802test AUTO ALL)
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
target_code_coverage(m6800test AUTO ALL)
#doctest_discover_tests(chip8-fpcore-tests)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cores/cdp1802/emma")
    add_subdirectory(cores/cdp1802/emma)
    set(CDP1802_EMMA_CORE ON)
endif()
add_executable(cdp1802sst cdp1802sst.cpp fuzzer.cpp)
target_compile_definitions(cdp1802sst PUBLIC CDP1802SST_VERSION="${PROJECT_VERSION}" CDP1802SST_GIT_HASH="${GIT_COMMIT_HASH}" CADMIUM_WITH_GENERIC_CPU WITHOUT_VIDEO)
if(CDP1802_EMMA_CORE)
    target_compile_definitions(cdp1802sst PUBLIC CDP1802_EMMA_CORE=1)
    target_link_libraries(cdp1802sst PUBLIC emulation emma_lib)
else()
    target_link_libraries(cdp1802sst PUBLIC emulation)
endif()

add_executable(variantset-tests main.cpp variantset_test.cpp)
target_compile_definitions(variantset-tests PUBLIC WITHOUT_STB_IMAGE)
target_link_libraries(variantset-tests PUBLIC doctest chiplet-lib)
target_code_coverage(variantset-tests AUTO ALL)
doctest_discover_tests(variantset-tests)

add_executable(expressionist-tests main.cpp expressionist_tests.cpp)
target_compile_definitions(expressionist-tests PUBLIC WITHOUT_STB_IMAGE)
target_link_libraries(expressionist-tests PUBLIC doctest::doctest fmt::fmt)
target_include_directories(expressionist-tests PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_code_coverage(expressionist-tests AUTO ALL)
doctest_discover_tests(expressionist-tests)

add_executable(emuint-tests main.cpp emuint_tests.cpp)
target_compile_definitions(emuint-tests PUBLIC WITHOUT_STB_IMAGE)
target_link_libraries(emuint-tests PUBLIC doctest::doctest fmt::fmt)
target_code_coverage(emuint-tests AUTO ALL)
doctest_discover_tests(emuint-tests)

if(FALSE)
add_executable(time-tests main.cpp time_test.cpp)
target_include_directories(expressionist-tests PUBLIC ${PROJECT_SOURCE_DIR}/external)
target_link_libraries(time-tests PUBLIC doctest emulation)
target_code_coverage(time-tests AUTO ALL)
doctest_discover_tests(time-tests)
endif()

if (${PLATFORM} MATCHES "Web")
    add_executable(web_test web_test.cpp)
    target_link_libraries(web_test PRIVATE raylib)
    set_target_properties(web_test PROPERTIES LINK_FLAGS "-Os -s USE_GLFW=3 -s USE_WEBGL2=0 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -sFETCH -s EXPORTED_RUNTIME_METHODS=[ccall] --no-heap-copy --shell-file ${PROJECT_SOURCE_DIR}/resources/minshell.html -sASYNCIFY -sASYNCIFY_IMPORTS=[copyClip,pasteClip]")
    set_target_properties(web_test PROPERTIES OUTPUT_NAME "webtest")
    set_target_properties(web_test PROPERTIES SUFFIX ".html")
endif ()

#add_executable(frame-time-test frame_time_test.cpp)
#target_link_libraries(frame-time-test PUBLIC emulation fmt::fmt)
#target_code_coverage(frame-time-test AUTO ALL)

# Optional independent reference, used only for cross-project behavioral tests.
set(CADMIUM_RSM8K1_REFERENCE_DIR "" CACHE PATH "Path to the standalone RSM8k1 source directory")
if(CADMIUM_RSM8K1_REFERENCE_DIR)
    add_executable(chip8rsm8k1_comparison
        chip8rsm8k1_comparison.cpp
        "${CADMIUM_RSM8K1_REFERENCE_DIR}/src/Chip8Core.cpp"
        "${CADMIUM_RSM8K1_REFERENCE_DIR}/src/Chip8Tables.cpp"
        "${CADMIUM_RSM8K1_REFERENCE_DIR}/src/Platform.cpp")
    target_include_directories(chip8rsm8k1_comparison PRIVATE "${CADMIUM_RSM8K1_REFERENCE_DIR}/include")
    target_link_libraries(chip8rsm8k1_comparison PRIVATE emulation)
    add_test(NAME rs-m8k1-reference COMMAND chip8rsm8k1_comparison)
    set_tests_properties(rs-m8k1-reference PROPERTIES TIMEOUT 60)
endif()



# Core-specific tests do not depend on the external reconstruction.
add_executable(chip8rsm8k1_tests chip8rsm8k1_tests.cpp)
target_compile_definitions(chip8rsm8k1_tests PRIVATE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)
target_link_libraries(chip8rsm8k1_tests PRIVATE doctest emulation)
doctest_discover_tests(chip8rsm8k1_tests)

if(TARGET chip8rsm8k1_comparison)
    file(GLOB rsmDemoRoms "${CADMIUM_RSM8K1_REFERENCE_DIR}/../res/MegaChip8 Demos/*.ch8")
    if(rsmDemoRoms)
        add_test(NAME rs-m8k1-reference-demos COMMAND chip8rsm8k1_comparison ${rsmDemoRoms})
        set_tests_properties(rs-m8k1-reference-demos PROPERTIES TIMEOUT 60)
    endif()
endif()
