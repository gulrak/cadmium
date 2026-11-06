//---------------------------------------------------------------------------------------
// test/display_wait_tests.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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

#include "chip8headless.hpp"

#include <emulation/ichip8.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

struct Variant
{
    const char* generic;
    const char* vip;
    uint16_t startAddress;
};

const Variant VARIANTS[] = {
    {"chip-8", "vip-chip-8", 0x200},
    {"chip-8-tpd", "vip-chip-8-tpd", 0x260},
    {"chip-8-e", "vip-chip-8-e", 0x200},
    {"chip-8-x", "vip-chip-8-x", 0x300},
};

struct DrawLoop
{
    int lines;
    int x;
    int filler;
};

std::vector<uint8_t> drawLoopRom(uint16_t startAddress, const DrawLoop& loop)
{
    std::vector<uint8_t> rom;
    const auto addOpcode = [&](int opcode) {
        rom.push_back(opcode >> 8);
        rom.push_back(opcode & 0xFF);
    };
    addOpcode(0x6300 | loop.x);
    addOpcode(0x6408);
    addOpcode(0xA000 | (startAddress + 0x100));
    const int loopAddress = startAddress + static_cast<int>(rom.size());
    addOpcode(0xD340 | loop.lines);
    addOpcode(0x7001);
    for (int i = 0; i < loop.filler; ++i) {
        addOpcode(0x6505);
    }
    addOpcode(0x1000 | loopAddress);
    rom.resize(0x100, 0);
    rom.resize(0x110, 0xFF);
    return rom;
}

double drawsPerFrame(const char* preset, const std::vector<uint8_t>& rom)
{
    auto instance = createChip8Instance(preset);
    auto& host = std::get<0>(instance);
    auto* chip8 = host->chip8Emulator();
    chip8->reset();
    REQUIRE(host->load(rom));
    for (int frame = 0; frame < 12; ++frame) {
        host->executeFrame();
    }
    constexpr int FRAMES = 60;
    int draws = 0;
    int last = chip8->getV(0);
    for (int frame = 0; frame < FRAMES; ++frame) {
        host->executeFrame();
        draws += (chip8->getV(0) - last) & 0xFF;
        last = chip8->getV(0);
    }
    return static_cast<double>(draws) / FRAMES;
}

}  // namespace

TEST_SUITE_BEGIN("DisplayWait");

TEST_CASE("The draw rate follows the real interpreters")
{
    // tests for a more precise heuristic than my old rule of lines > 4 && lines + (x & 7) > 9
    const DrawLoop loops[] = {{1, 8, 0}, {8, 11, 0}, {10, 8, 0}, {12, 8, 0}, {15, 8, 0}, {5, 13, 0}, {6, 12, 0}, {8, 13, 0},
                              {10, 11, 0}, {12, 10, 0}, {15, 9, 0}, {8, 9, 2}, {8, 12, 2}, {10, 9, 2}, {5, 12, 5}, {8, 8, 5}, {8, 11, 5}};
    for (const auto& variant : VARIANTS) {
        for (const auto& loop : loops) {
            const auto rom = drawLoopRom(variant.startAddress, loop);
            CAPTURE(std::string(variant.generic));
            CAPTURE(loop.lines);
            CAPTURE(loop.x);
            CAPTURE(loop.filler);
            CHECK(drawsPerFrame(variant.generic, rom) == doctest::Approx(drawsPerFrame(variant.vip, rom)).epsilon(0.2));
        }
    }
}

TEST_CASE("The timing of the disp-quirk-strict test for classic CHIP-8")
{
    CHECK(drawsPerFrame("chip-8", drawLoopRom(0x200, {1, 32, 8})) == doctest::Approx(1.0));
    CHECK(drawsPerFrame("chip-8", drawLoopRom(0x200, {8, 28, 8})) == doctest::Approx(0.5));
    CHECK(drawsPerFrame("chip-8", drawLoopRom(0x200, {8, 32, 8})) == doctest::Approx(1.0));
}

TEST_SUITE_END();
