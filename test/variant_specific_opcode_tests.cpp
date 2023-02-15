//---------------------------------------------------------------------------------------
// tests/variant_specific_opcode_tests.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
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

#include "chip8adapter.hpp"
#include "chip8testhelper.hpp"

TEST_SUITE_BEGIN(C8CORE "VariantOpcodes");

TEST_CASE(C8CORE "8xy6 - vx >>= vy, lost bit in vF, this shift test expects vy to be used")
{
    EmuCore chip8;
    SUBCASE("CHIP8") {
        chip8 = createChip8Instance(C8TV_C8);
    }
    SUBCASE("CHIP 10") {
        chip8 = createChip8Instance(C8TV_C10);
    }
    SUBCASE("XO-CHIP") {
        chip8 = createChip8Instance(C8TV_XO);
    }
    if(chip8 && chip8->name() != "DREAM6800") {
        chip8->reset();
        write(chip8, 0x200, {0x6142, 0x6231, 0x8126, 0x6f84, 0x8f26});
        step(chip8);  // #1
        CheckState(chip8, {.i = 0, .pc = 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x42, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x42");
        step(chip8);  // #2
        CheckState(chip8, {.i = 0, .pc = 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x42, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v2 := 0x31");
        step(chip8);  // #3
        CheckState(chip8, {.i = 0, .pc = 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x18, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 >>= v2, vF set to 1");
        step(chip8);  // #4
        CheckState(chip8, {.i = 0, .pc = 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x18, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
        step(chip8);  // #5
        CheckState(chip8, {.i = 0, .pc = 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x18, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "vF >>= v2, vF set to 1");
    }
    else {
        MESSAGE("feature not supported");
    }
}

TEST_CASE(C8CORE "8xye - vx <<= vy, lost bit in vF, this shift test expects vy to be used")
{
    EmuCore chip8;
    SUBCASE("CHIP8") {
        chip8 = createChip8Instance(C8TV_C8);
    }
    SUBCASE("CHIP 10") {
        chip8 = createChip8Instance(C8TV_C10);
    }
    SUBCASE("XO-CHIP") {
        chip8 = createChip8Instance(C8TV_XO);
    }
    if(chip8 && chip8->name() != "DREAM6800") {
        chip8->reset();
        write(chip8, 0x200, {0x6142, 0x6281, 0x812e, 0x6f84, 0x8f1e});
        step(chip8);  // #1
        CheckState(chip8, {.i = 0, .pc = 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x42, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x42");
        step(chip8);  // #2
        CheckState(chip8, {.i = 0, .pc = 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x42, 0x81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v2 := 0x31");
        step(chip8);  // #3
        CheckState(chip8, {.i = 0, .pc = 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x02, 0x81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 <<= v2, vF set to 1");
        step(chip8);  // #4
        CheckState(chip8, {.i = 0, .pc = 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x02, 0x81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
        step(chip8);  // #5
        CheckState(chip8, {.i = 0, .pc = 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x02, 0x81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "vF <<= v1, vF set to 0");
    }
    else {
        MESSAGE("feature not supported");
    }
}

TEST_CASE(C8CORE "8xy6 - vx >>= vx, lost bit in vF, this shift test expects vy to be ignored")
{
    EmuCore chip8;
    SUBCASE("CHIP-48") {
        chip8 = createChip8Instance(C8TV_C48);
    }
    SUBCASE("SUPER-CHIP 1.0") {
        chip8 = createChip8Instance(C8TV_SC10);
    }
    SUBCASE("SUPER-CHIP 1.1") {
        chip8 = createChip8Instance(C8TV_SC11);
    }
    SUBCASE("MegaChip8") {
        chip8 = createChip8Instance(C8TV_MC8);
    }
    if(chip8) {
        chip8->reset();
        write(chip8, 0x200, {0x6141, 0x6231, 0x8126, 0x6f84, 0x8f26});
        step(chip8);  // #1
        CheckState(chip8, {.i = 0, .pc = 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x41, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x41");
        step(chip8);  // #2
        CheckState(chip8, {.i = 0, .pc = 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x41, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v2 := 0x31");
        step(chip8);  // #3
        CheckState(chip8, {.i = 0, .pc = 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x20, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 >>= v2, v2 ignored, vF set to 1");
        step(chip8);  // #4
        CheckState(chip8, {.i = 0, .pc = 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x20, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
        step(chip8);  // #5
        CheckState(chip8, {.i = 0, .pc = 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x20, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "vF >>= v2, v2 ignored, vF set to 0");
    }
    else {
        MESSAGE("feature not supported");
    }
}

TEST_CASE(C8CORE "8xye - vx <<= vx, lost bit in vF, this shift test expects vy to be ignored")
{
    EmuCore chip8;
    SUBCASE("CHIP-48") {
        chip8 = createChip8Instance(C8TV_C48);
    }
    SUBCASE("SUPER-CHIP 1.0") {
        chip8 = createChip8Instance(C8TV_SC10);
    }
    SUBCASE("SUPER-CHIP 1.1") {
        chip8 = createChip8Instance(C8TV_SC11);
    }
    SUBCASE("MegaChip8") {
        chip8 = createChip8Instance(C8TV_MC8);
    }
    if(chip8) {
        chip8->reset();
        write(chip8, 0x200, {0x6181, 0x6231, 0x812e, 0x6f82, 0x8f2e});
        step(chip8);  // #1
        CheckState(chip8, {.i = 0, .pc = 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x81");
        step(chip8);  // #2
        CheckState(chip8, {.i = 0, .pc = 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x81, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v2 := 0x31");
        step(chip8);  // #3
        CheckState(chip8, {.i = 0, .pc = 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x02, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 <<= v2, v2 ignored, vF set to 1");
        step(chip8);  // #4
        CheckState(chip8, {.i = 0, .pc = 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x02, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x82}, .stack = {}}, "vF := 0x82");
        step(chip8);  // #5
        CheckState(chip8, {.i = 0, .pc = 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x02, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "vF <<= v2, v2 ignored, vF set to 1");
    }
    else {
        MESSAGE("feature not supported");
    }
}

TEST_SUITE_END();
