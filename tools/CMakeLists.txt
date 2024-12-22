if (NOT (${PLATFORM} MATCHES "Web"))
add_executable(resourcer resourcer.cpp)
target_compile_definitions(resourcer PUBLIC CADMIUM_VERSION="${PROJECT_VERSION}")
target_link_libraries(resourcer PUBLIC ghc_filesystem)
add_executable(icongenerator icongenerator.cpp)
target_compile_definitions(icongenerator PUBLIC CADMIUM_VERSION="${PROJECT_VERSION}")
target_link_libraries(icongenerator PUBLIC raylib)
endif()

add_executable(soundfreq soundfreq.cpp)

add_executable(fontgenerator fontgenerator.cpp)

add_executable(colorsort colorsort.cpp)
target_link_libraries(colorsort PUBLIC raylib)

add_executable(imageconv imageconv.cpp)
target_link_libraries(imageconv PUBLIC raylib fmt::fmt)

add_executable(rpgt rpgt.cpp)
target_link_libraries(rpgt PUBLIC emulation ghc_filesystem)

add_executable(c8db c8db.cpp ../src/librarian.cpp ../src/configuration.cpp ../src/emuhostex.cpp ../src/systemtools.cpp)
target_compile_definitions(c8db PUBLIC CADMIUM_VERSION="${PROJECT_VERSION}")
target_link_libraries(c8db PUBLIC emulation ghc_filesystem raylib)
target_code_coverage(c8db)

add_executable(hp48prgtest hp48prgtest.cpp hpsaturnbase.cpp)
target_include_directories(hp48prgtest PUBLIC ../../chiplet/include/ ../external)
target_link_libraries(hp48prgtest PUBLIC emulation fmt::fmt)