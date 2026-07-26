
set(CHIP8_EMU_SOURCE
    chip8generic.cpp
    chip8generic.hpp
    chip8genericbase.cpp
    chip8genericbase.hpp
    chip8opcodedisass.cpp
    chip8opcodedisass.hpp
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
    hp48/display.cpp
    hp48/display.hpp
    hp48/font.cpp
    hp48/font.hpp
    hp48/stack.cpp
    hp48/stack.hpp
)
if(CADMIUM_WITH_HP48)
    list(APPEND CHIP8_EMU_SOURCE hp48sx.cpp hp48sx.hpp)
endif()

add_library(emulation OBJECT ${CHIP8_EMU_SOURCE})
set_source_files_properties(src/emulation/chip8compiler.cpp PROPERTIES COMPILE_FLAGS "-fpermissive -Wno-write-strings")
#target_compile_definitions(emulation PUBLIC CADMIUM_WITH_GENERIC_CPU GEN_OPCODE_STATS)
target_compile_definitions(emulation PUBLIC CADMIUM_WITH_GENERIC_CPU)
target_link_libraries(emulation PUBLIC c_octo chiplet-lib raylib_static)
if(CADMIUM_WITH_HP48)
    target_link_libraries(emulation PUBLIC hp48::core hp48::asacpp)
    target_compile_definitions(emulation PRIVATE
        CADMIUM_WITH_HP48=1
        HP48SX_SOURCE_DIR="${hp48sx_SOURCE_DIR}")
endif()
if(CODE_COVERAGE)
    target_code_coverage(emulation)
endif()
