
set(CHIP8_EMU_SOURCE
    time.hpp
    math.hpp
    chip8cores.hpp
    chip8cores.cpp
    chip8emulatorbase.cpp
    chip8emulatorbase.hpp
    chip8options.cpp
    chip8options.hpp
    chip8compiler.cpp
    chip8decompiler.cpp
    octocompiler.cpp
    cdp1802.hpp
)

add_library(emulation ${CHIP8_EMU_SOURCE})
set_source_files_properties(src/emulation/chip8compiler.cpp PROPERTIES COMPILE_FLAGS "-fpermissive -Wno-write-strings")
target_link_libraries(emulation PUBLIC fmt::fmt c_octo)
