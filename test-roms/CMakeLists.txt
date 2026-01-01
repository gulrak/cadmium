
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/bin/xochip-sound-test.xo8
    COMMAND chiplet ARGS xochip-sound-test.8o -o bin/xochip-sound-test.xo8
    DEPENDS chiplet
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/bin/variant-detection.ch8
    COMMAND chiplet ARGS ${CMAKE_CURRENT_SOURCE_DIR}/variant-detection/variant-detection.8o -o bin/variant-detection.ch8
    DEPENDS chiplet
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(test_roms ALL
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/bin/xochip-sound-test.xo8 ${CMAKE_CURRENT_SOURCE_DIR}/bin/variant-detection.ch8
)
