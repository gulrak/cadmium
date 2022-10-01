file(GLOB RESOURCE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/files/*.png ${CMAKE_SOURCE_DIR}/files/*.ttf)
if (${PLATFORM} MATCHES "Web")
add_custom_command(
        OUTPUT resourcer resourcedata.c
        COMMAND clang++ -o resourcer -std=c++17 ${PROJECT_SOURCE_DIR}/tools/resourcer.cpp
        COMMAND ${CMAKE_CURRENT_BINARY_DIR}/resourcer ${CMAKE_CURRENT_SOURCE_DIR}/files/ ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c
        DEPENDS ${RESOURCE_FILES}
)
else()
    add_custom_command(
        OUTPUT resourcer resourcedata.c
        COMMAND clang++ -o resourcer -std=c++17 ${PROJECT_SOURCE_DIR}/tools/resourcer.cpp
        COMMAND resourcer ${CMAKE_CURRENT_SOURCE_DIR}/files/ ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c
        DEPENDS resourcer ${RESOURCE_FILES}
    )
endif()

add_library(resourcedata STATIC ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c)
