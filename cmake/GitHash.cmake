# Writes the header with the current git hash, run as a script on every build:
#   cmake -DGIT_EXECUTABLE=<git> -DSOURCE_DIR=<repository> -DOUTPUT_FILE=<header> -P GitHash.cmake
# The file is only touched when the hash changed, so an unchanged hash recompiles nothing.
set(GIT_COMMIT_HASH "")
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()

set(HEADER_CONTENT "#pragma once\n#define CADMIUM_GIT_HASH \"${GIT_COMMIT_HASH}\"\n")
set(CURRENT_CONTENT "")
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" CURRENT_CONTENT)
endif()
if(NOT "${HEADER_CONTENT}" STREQUAL "${CURRENT_CONTENT}")
    file(WRITE "${OUTPUT_FILE}" "${HEADER_CONTENT}")
endif()
