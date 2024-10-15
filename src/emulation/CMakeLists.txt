
set(CHIP8_EMU_SOURCE
    chip8generic.cpp
    chip8generic.hpp
    #chip8emulatorbase.cpp
    #chip8emulatorbase.hpp
    chip8genericbase.cpp
    chip8genericbase.hpp
    chip8opcodedisass.cpp
    chip8opcodedisass.hpp
    chip8options.cpp
    chip8options.hpp
    chip8strict.cpp
    chip8strict.hpp
    coreregistry.cpp
    coreregistry.hpp
    cosmacvip.cpp
    cosmacvip.hpp
    eti660.cpp
    eti660.hpp
    #dream6800.cpp
    #dream6800.hpp
    emulatorhost.hpp
    hardware/cdp186x.cpp
    hardware/cdp186x.hpp
    hardware/cdp1802.hpp
    hardware/m6800.hpp
    hardware/mc682x.cpp
    hardware/mc682x.hpp
    ichip8.hpp
    iemulationcore.hpp
    math.hpp
    properties.cpp
    properties.hpp
    time.cpp
    time.hpp
    utility.cpp
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
