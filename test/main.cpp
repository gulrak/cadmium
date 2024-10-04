#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#define ENABLE_CONSOLE_LOGGER
#include <emulation/logger.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <chiplet/stb_image.h>

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
