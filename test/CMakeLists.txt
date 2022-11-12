
if(FALSE)
# JamesGriffin/CHIP-8
file(DOWNLOAD https://raw.githubusercontent.com/JamesGriffin/CHIP-8-Emulator/master/src/chip8.h ${CMAKE_CURRENT_SOURCE_DIR}/cores/jgriffin/chip8.h)
file(DOWNLOAD https://raw.githubusercontent.com/JamesGriffin/CHIP-8-Emulator/master/src/chip8.cpp ${CMAKE_CURRENT_SOURCE_DIR}/cores/jgriffin/chip8.cpp)

# wernsey/chip8
file(DOWNLOAD https://raw.githubusercontent.com/wernsey/chip8/master/chip8.h ${CMAKE_CURRENT_SOURCE_DIR}/cores/wernsey/chip8.h)
file(DOWNLOAD https://raw.githubusercontent.com/wernsey/chip8/master/chip8.c ${CMAKE_CURRENT_SOURCE_DIR}/cores/wernsey/chip8.c)
endif()

add_executable(chip8-fpcore-tests main.cpp basic_opcode_tests.cpp variant_specific_opcode_tests.cpp chip8adapter.hpp  chip8adapter.cpp ../src/emulation/chip8cores.hpp ../src/emulation/chip8cores.cpp)
target_compile_definitions(chip8-fpcore-tests PUBLIC TEST_CHIP8EMULATOR_FP=1 C8CORE="C8FP:")
target_link_libraries(chip8-fpcore-tests PRIVATE doctest emulation)
doctest_discover_tests(chip8-fpcore-tests)

add_executable(chip8-tscore-tests main.cpp basic_opcode_tests.cpp variant_specific_opcode_tests.cpp chip8adapter.hpp  chip8adapter.cpp ../src/emulation/chip8cores.hpp ../src/emulation/chip8cores.cpp)
target_compile_definitions(chip8-tscore-tests PUBLIC TEST_CHIP8EMULATOR_TS C8CORE="C8TS:")
target_link_libraries(chip8-tscore-tests PRIVATE doctest emulation)
doctest_discover_tests(chip8-tscore-tests)

add_executable(cdp1802test cdp1802test.cpp)
target_link_libraries(cdp1802test emulation)
#doctest_discover_tests(chip8-fpcore-tests)
