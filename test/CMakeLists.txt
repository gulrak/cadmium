if(FALSE)
# JamesGriffin/CHIP-8
file(DOWNLOAD https://raw.githubusercontent.com/JamesGriffin/CHIP-8-Emulator/master/src/chip8.h ${CMAKE_CURRENT_SOURCE_DIR}/cores/jgriffin/chip8.h)
file(DOWNLOAD https://raw.githubusercontent.com/JamesGriffin/CHIP-8-Emulator/master/src/chip8.cpp ${CMAKE_CURRENT_SOURCE_DIR}/cores/jgriffin/chip8.cpp)

# wernsey/chip8
file(DOWNLOAD https://raw.githubusercontent.com/wernsey/chip8/master/chip8.h ${CMAKE_CURRENT_SOURCE_DIR}/cores/wernsey/chip8.h)
file(DOWNLOAD https://raw.githubusercontent.com/wernsey/chip8/master/chip8.c ${CMAKE_CURRENT_SOURCE_DIR}/cores/wernsey/chip8.c)
endif()

add_executable(chip8-tests main.cpp basic_opcode_tests.cpp chip8adapter.hpp  chip8adapter.cpp ../src/emulation/chip8cores.hpp ../src/emulation/chip8cores.cpp)
target_link_libraries(chip8-tests PRIVATE doctest emulation)

add_executable(cdp1802test cdp1802test.cpp)
target_link_libraries(cdp1802test emulation)
