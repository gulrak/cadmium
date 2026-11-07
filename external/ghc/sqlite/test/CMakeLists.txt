set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
    GIT_SHALLOW TRUE
)
set(CATCH_BUILD_TESTING OFF)
set(CATCH_INSTALL_DOCS OFF)
set(CATCH_INSTALL_EXTRAS OFF)

# Prevent Catch2 from registering with CTest
set(CATCH_INSTALL_HELPERS OFF)
set(CATCH_CONFIG_DISABLE_EXCEPTIONS OFF)
set(CATCH_CONFIG_DISABLE_ASSERTION_MACROS OFF)
set(CATCH_DEVELOPMENT_BUILD OFF)

FetchContent_MakeAvailable(Catch2)
include(Catch)

add_executable(sqlite-tests sqlite-test.cpp)
target_link_libraries(sqlite-tests PRIVATE ghc::sqlite Catch2::Catch2WithMain)
catch_discover_tests(sqlite-tests)

add_executable(cadmium-tests cadmium-test.cpp)
target_link_libraries(cadmium-tests PRIVATE ghc::sqlite Catch2::Catch2WithMain)
catch_discover_tests(cadmium-tests)
