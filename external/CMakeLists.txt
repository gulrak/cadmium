
#add_subdirectory(fmt)

add_subdirectory(miniz)
add_subdirectory(sqlite3)
add_subdirectory(rlguipp)

add_library(c_octo INTERFACE)
target_include_directories(c_octo INTERFACE c-octo/src)

add_library(jsct STATIC jsct/JsClipboardTricks.cpp jsct/JsClipboardTricks.h)
target_include_directories(jsct PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
