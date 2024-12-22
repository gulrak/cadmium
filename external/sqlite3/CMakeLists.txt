cmake_minimum_required(VERSION 3.16)

set(SQLITE3_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.h
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3ext.h
)

add_library(sqlite3-lib STATIC ${SQLITE3_SOURCES})
target_include_directories(sqlite3-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_library(SQLite::SQLite3 ALIAS sqlite3-lib)

