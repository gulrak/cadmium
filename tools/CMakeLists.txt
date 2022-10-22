if (NOT (${PLATFORM} MATCHES "Web"))
add_executable(resourcer resourcer.cpp)
add_executable(icongenerator icongenerator.cpp)
target_compile_definitions(icongenerator PUBLIC CADMIUM_VERSION="${PROJECT_VERSION}")
target_link_libraries(icongenerator PUBLIC raylib)
endif()

add_executable(soundfreq soundfreq.cpp)

add_executable(decompile decompiler.cpp)
target_link_libraries(decompile emulation)

add_executable(fontgenerator fontgenerator.cpp)

add_executable(colorsort colorsort.cpp)
target_link_libraries(colorsort PUBLIC raylib)
