//---------------------------------------------------------------------------------------
// tests/superchip_tests.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//---------------------------------------------------------------------------------------

#include <doctest/doctest.h>
#include "chip8testhelper.hpp"

TEST_SUITE_BEGIN("SUPER-CHIP-Tests");

static std::string screens[] = {
    R"(################
################
##..########..##
##..########..##
################
################
######....######
######....######
################
################
##..##....##..##
##..##....##..##
####..####..####
####..####..####
##..##....##..##
##..##....##..##
####..####..####
####..####..####
##..##....##..##
##..##....##..##
####..####....##
####..####....##
################
################
################
################
################
################
################
################
)",
    R"(..##..#.########
.......#########
......#.####..##
..##...#####..##
..##..#.########
.......#########
..##...#..######
......#...######
..##..#.########
.......#########
.......#..##..##
..##..#...##..##
..#####.##..####
....##.###..####
.......#..##..##
##..##....##..##
####..####..####
####..####..####
##..##....##..##
##..##....##..##
####..####....##
####..####....##
################
################
################
################
################
################
################
################
)",
    R"(##..#...##..#.
.....#.......#
....#...#####.
##...#....##.#
##..#...##..#.
.....#.......#
##...#####..#.
....#.##.....#
##..#...##..#.
.....#.......#
.....########.
##..#.##..##.#
#####.......#.
..##.#..##...#
.....########.
.....########.
..##.#..##...#
#####.......#.
##..#.##..##.#
.....########.
..##.#..####.#
#####.....###.
.....#.......#
##..#...##..#.
.....#.......#
##..#...##..#.
.....#.......#
##..#...##..#.
.....#.......#
##..#...##..#.
)",
    R"(##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
##..##.###..##.#
#######.#######.
#######.#######.
#######.#######.
#######.#######.
#######.#######.
#######.#######.
#######.#######.
#######.#######.
#######.#######.
#######.#######.
#######.#######.
#######.#######.
#######.#######.
#######.#######.
)",
    R"()"
};

void runSchip1xDrawTest(std::string preset)
{
    auto [host, core, start] = createChip8Instance(preset);
    auto rom = loadFile(fs::path(TEST_ROM_FOLDER) / "chromatophore-hp48-draw.ch8");
    struct Info { uint16_t idx, pc, x; };
    Info info[] = {
        {0, 0x20A, 20}, {0, 0x20E, 20}, {1, 0x214, 20},{2, 0x21C, 22},
        {2, 0x220, 22}, {3, 0x226, 20}, {4, 0x22A, 20}
    };
    core->reset();
    host->load(rom);
    HeadlessTestHost::Rect rect;
    std::string content;
    int key = 1;
    for(auto& step : info) {
        INFO(fmt::format("Step-PC: {}", step.pc));
        std::tie(rect, content) = host->screenUsedOnNextKeyWait();
        CHECK_EQ(step.pc, core->getPC());
        if(screens[step.idx].empty()) {
            CHECK_EQ(0, rect.x); CHECK_EQ(0, rect.y);
        }
        else {
            CHECK_EQ(step.x, rect.x); CHECK_EQ(20, rect.y);
        }
        CHECK_EQ(screens[step.idx], content);
        host->selectKey(key++);
    }
}

TEST_CASE("SCHIP-1.0 Chromatophore Draw Test")
{
    runSchip1xDrawTest("schip-1.0");
}

TEST_CASE("SCHIP-1.1 Chromatophore Draw Test")
{
    runSchip1xDrawTest("schip-1.1");
}

TEST_SUITE_END();