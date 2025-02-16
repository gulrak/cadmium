#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#define ENABLE_CONSOLE_LOGGER
#include <emulation/logger.hpp>

#ifndef WITHOUT_STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#include <chiplet/stb_image.h>
#endif

int main(int argc, char** argv) {
    doctest::Context context;
    context.applyCommandLine(argc, argv);

    int res = 0;
    {
        emu::ConsoleLogger log(std::clog);
        res = context.run(); // run
    }
    return res;
}
