
set(CHIP8_EMU_SOURCE
    chip8generic.cpp
    chip8generic.hpp
    chip8genericbase.cpp
    chip8genericbase.hpp
    chip8opcodedisass.cpp
    chip8opcodedisass.hpp
    chip8rsm8k1.cpp
    chip8rsm8k1.hpp
    chip8rsm8k1detail.cpp
    chip8rsm8k1detail.hpp
    chip8rsm8k1tables.cpp
    chip8strict.cpp
    chip8strict.hpp
    coreregistry.cpp
    coreregistry.hpp
    cosmacvip.cpp
    cosmacvip.hpp
    eti660.cpp
    eti660.hpp
    datavariants.cpp
    datavariants.hpp
    dream6800.cpp
    dream6800.hpp
    dreamtape.cpp
    dreamtape.hpp
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
    expressionist.cpp
    expressionist.hpp
    videorenderer.cpp
    videorenderer.hpp
    #video/framedescriptor.hpp
    #video/iframeeffect.hpp
    #video/frameprocessor.hpp
    #video/frameprocessor.cpp
    #video/iframeeffect.cpp
    #video/plaineffect.hpp
    #video/plaineffect.cpp
    #video/scanlineeffect.hpp
    #video/scanlineeffect.cpp
    #video/lcdeffect.hpp
    #video/lcdeffect.cpp
    hardware/hp48/display.cpp
    hardware/hp48/display.hpp
    hardware/hp48/font.cpp
    hardware/hp48/font.hpp
    hardware/hp48/stack.hpp
)
if(CADMIUM_WITH_HP48)
    list(APPEND CHIP8_EMU_SOURCE
        hp48sx.cpp
        hp48sx.hpp
        filter.hpp
        hardware/hp48/asacpp/asacpp.cpp
        hardware/hp48/asacpp/asacpp.hpp
        hardware/hp48/binary.hpp
        hardware/hp48/emptyportcontroller.hpp
        hardware/hp48/g48statefile.cpp
        hardware/hp48/g48statefile.hpp
        hardware/hp48/g48sxmachine.cpp
        hardware/hp48/g48sxmachine.hpp
        hardware/hp48/hdwcontroller.hpp
        hardware/hp48/hpsaturnl2.cpp
        hardware/hp48/hpsaturnl2.hpp
        hardware/hp48/hpsaturnl2op.hpp
        hardware/hp48/imemorycontroller.hpp
        hardware/hp48/isaturn.hpp
        hardware/hp48/nce1romcontroller.cpp
        hardware/hp48/nce1romcontroller.hpp
        hardware/hp48/nce2ramcontroller.hpp
        hardware/hp48/nibblememory.hpp
        hardware/hp48/saturndecoder.cpp
        hardware/hp48/saturndecoder.hpp
        hardware/hp48/saturninstructions.cpp
        hardware/hp48/saturnreg64.hpp
        hardware/hp48/saturnstate.cpp
        hardware/hp48/saturnstate.hpp
        hardware/hp48/saturntrace.cpp
        hardware/hp48/saturntrace.hpp
        hardware/hp48/systembus.hpp
    )
endif()

add_library(emulation OBJECT ${CHIP8_EMU_SOURCE})
set_source_files_properties(src/emulation/chip8compiler.cpp PROPERTIES COMPILE_FLAGS "-fpermissive -Wno-write-strings")
#target_compile_definitions(emulation PUBLIC CADMIUM_WITH_GENERIC_CPU GEN_OPCODE_STATS)
target_compile_definitions(emulation PUBLIC CADMIUM_WITH_GENERIC_CPU)
target_link_libraries(emulation PUBLIC c_octo chiplet-lib raylib_static)
if(CADMIUM_WITH_HP48)
    target_link_libraries(emulation PUBLIC libzstd_static ghc_filesystem resourcemanager)
    target_include_directories(emulation PRIVATE "${zstd_SOURCE_DIR}/lib")
    target_compile_definitions(emulation PUBLIC HP48_ENABLE_INSTRUCTION_CACHE=1)
    target_compile_definitions(emulation PRIVATE CADMIUM_WITH_HP48=1)
endif()
if(CODE_COVERAGE)
    target_code_coverage(emulation)
endif()
