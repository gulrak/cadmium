add_executable(resourcer resourcer.cpp)

add_executable(soundfreq soundfreq.cpp)

add_executable(decompile decompiler.cpp ../src/emulation/chip8decompiler.cpp)
target_link_libraries(decompile fmt::fmt)

add_executable(fontgenerator fontgenerator.cpp)

add_executable(colorsort colorsort.cpp)
target_link_libraries(colorsort PUBLIC raylib)
