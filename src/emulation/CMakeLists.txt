
set(CHIP8_EMU_SOURCE
    time.cpp
    time.hpp
    math.hpp
    chip8cores.hpp
    chip8cores.cpp
    chip8strict.hpp
    chip8opcodedisass.cpp
    chip8opcodedisass.hpp
    chip8emulatorbase.cpp
    chip8emulatorbase.hpp
    chip8options.cpp
    chip8options.hpp
    hardware/cdp1802.hpp
    hardware/cdp186x.cpp
    hardware/cdp186x.hpp
    hardware/m6800.hpp
    hardware/mc682x.cpp
    hardware/mc682x.hpp
    chip8vip.cpp
    chip8vip.hpp
    chip8dream.cpp
    chip8dream.hpp
    utility.cpp
    properties.cpp
    properties.hpp
    coreregistry.cpp
    coreregistry.hpp
        #octocartridge.cpp
    #octocartridge.hpp
)

add_library(emulation OBJECT ${CHIP8_EMU_SOURCE})
set_source_files_properties(src/emulation/chip8compiler.cpp PROPERTIES COMPILE_FLAGS "-fpermissive -Wno-write-strings")
#target_compile_definitions(emulation PUBLIC CADMIUM_WITH_GENERIC_CPU GEN_OPCODE_STATS)
target_compile_definitions(emulation PUBLIC CADMIUM_WITH_GENERIC_CPU)
target_link_libraries(emulation PUBLIC c_octo chiplet-lib)
if(CODE_COVERAGE)
    target_code_coverage(emulation)
endif()
