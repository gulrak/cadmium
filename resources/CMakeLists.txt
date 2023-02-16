file(GLOB RESOURCE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/files/*.png ${CMAKE_SOURCE_DIR}/files/*.ttf)
if (${PLATFORM} MATCHES "Web")
    add_custom_command(
            OUTPUT resourcer resourcedata.c
            COMMAND clang++ -o resourcer -std=c++17 ${PROJECT_SOURCE_DIR}/tools/resourcer.cpp
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/resourcer ${CMAKE_CURRENT_SOURCE_DIR}/files/ ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c
            DEPENDS ${RESOURCE_FILES}
    )
else()
    if(APPLE)
        add_custom_command(
                OUTPUT cadmium-icon.png cadmium.icns
                COMMAND icongenerator ${CMAKE_CURRENT_SOURCE_DIR}/files/cadmium-title.png ${CMAKE_CURRENT_SOURCE_DIR}/files/micro-font.png cadmium-icon.png
                COMMAND iconutil -c icns ${CMAKE_CURRENT_BINARY_DIR}/cadmium.iconset
                DEPENDS icongenerator ${RESOURCE_FILES}
        )
        add_custom_target(cadmium_icon ALL DEPENDS cadmium.icns)
    else()
        add_custom_command(
                OUTPUT cadmium-icon.png
                COMMAND icongenerator ${CMAKE_CURRENT_SOURCE_DIR}/files/cadmium-title.png ${CMAKE_CURRENT_SOURCE_DIR}/files/micro-font.png cadmium-icon.png
                DEPENDS icongenerator ${RESOURCE_FILES}
        )
        add_custom_target(cadmium_icon DEPENDS cadmium-icon.png)
    endif()
    add_custom_command(
        OUTPUT resourcedata.c
        COMMAND resourcer ${CMAKE_CURRENT_SOURCE_DIR}/files/ ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c
        DEPENDS resourcer ${RESOURCE_FILES}
    )
endif()

add_library(resourcedata STATIC ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c)

