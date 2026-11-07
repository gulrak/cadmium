
#add_subdirectory(fmt)

add_subdirectory(rlguipp)
add_subdirectory(ghc)

add_library(c_octo INTERFACE)
target_include_directories(c_octo INTERFACE c-octo/src)
