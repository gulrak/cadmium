file(GLOB RESOURCE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/files/*.png ${CMAKE_SOURCE_DIR}/files/*.ttf)
add_custom_command(
        OUTPUT resourcedata.c
        COMMAND resourcer ${CMAKE_CURRENT_SOURCE_DIR}/files/ ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c
        DEPENDS resourcer ${RESOURCE_FILES}
)

add_library(resourcedata STATIC ${CMAKE_CURRENT_BINARY_DIR}/resourcedata.c)
