//---------------------------------------------------------------------------------------
// tests/opcode_tests.cpp
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

TEST_SUITE_BEGIN(C8CORE "BasicOpcodes");

TEST_CASE(C8CORE "reset()")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    CheckState(chip8, {.i = 0, .pc= 0x200, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}}, "state after reset");
}

TEST_CASE(C8CORE "1nnn - jump nnn")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x1204});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}}, "jump 0x204");
}

TEST_CASE(C8CORE "2nnn - call nnn")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x2204});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 1, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {0x202,0}}, "call 0x204");
}

TEST_CASE(C8CORE "00EE - return")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x2208,0x0000,0x0000,0x0000,0x00EE});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x208, .sp = 1, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {0x202,0}}, "call 0x208");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {-1,0}}, "return");
}

TEST_CASE(C8CORE "6xnn - vx := nn")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6032, 0x6314, 0x6bff});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0x32,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x32");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0x32,0,0,0x14, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x14");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0x32,0,0,0x14, 0,0,0,0, 0,0,0,0xff, 0,0,0,0}, .stack = {}}, "vB := 0xff");
}

TEST_CASE(C8CORE "3xnn - skip if vx == nn")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x3304, 0x6542, 0x3542});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 != 4, should not skip");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x42");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 == 0x42, should skip");
}

TEST_CASE(C8CORE "4xnn - skip if vx != nn")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6312, 0x4312, 0x6542, 0x4540});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 9x12");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 == 0x12, should not skip");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x42");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 != 0x40, should skip");
}

TEST_CASE(C8CORE "5xy0 - skip if vx == vy")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6312, 0x5310, 0x6512, 0x5350});
    step(chip8); // #1
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x12");
    step(chip8); // #2
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 != v1, should not skip");
    step(chip8); // #3
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0x12, 0,0x12,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x12");
    step(chip8); // #4
    CheckState(chip8, {.i = 0, .pc= 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0x12, 0,0x12,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 == v5, should skip");
}

TEST_CASE(C8CORE "7xnn - vx += nn")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6132, 0x7154, 0x717f});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x32,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x32");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x86,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 += 0x54");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x05,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 += 0x7f");
}

TEST_CASE(C8CORE "8xy0 - vx := vy")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6132, 0x8210, 0x8220});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x32,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x32");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x32,0x32,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v2 := v1");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x32,0x32,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v2 := v2");
}

TEST_CASE(C8CORE "8xy1 - vx |= vy")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6133, 0x6381, 0x8311, 0x8331});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x81");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0xb3, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 |= v1");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0xb3, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 |= v3");
}

TEST_CASE(C8CORE "8xy2 - vx &= vy")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6133, 0x6381, 0x8312, 0x8332});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x81");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 &= v1");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 &= v3");
}

TEST_CASE(C8CORE "8xy3 - vx ^= vy")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6133, 0x6381, 0x8313, 0x8333});
    step(chip8); // #1
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(chip8); // #2
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x81");
    step(chip8); // #3
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0xB2, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 ^= v1");
    step(chip8); // #4
    CheckState(chip8, {.i = 0, .pc= 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 ^= v3");
}

TEST_CASE(C8CORE "8xy4 - vx += vy, set vF to 1 on overflow, 0 if not")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6133, 0x6381, 0x8314, 0x8334, 0x6f84, 0x8f34, 0x6fda, 0x8f34});
    step(chip8); // #1
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(chip8); // #2
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x81");
    step(chip8); // #3
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0xB4, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 += v1");
    step(chip8); // #4
    CheckState(chip8, {.i = 0, .pc= 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "v3 += v3, vF should be set as this overflows");
    step(chip8); // #5
    CheckState(chip8, {.i = 0, .pc= 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,0x84}, .stack = {}}, "vF := 0x84");
    step(chip8); // #6
    CheckState(chip8, {.i = 0, .pc= 0x20c, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "vF += v3, vF is also carry flag, should be cleared");
    step(chip8); // #7
    CheckState(chip8, {.i = 0, .pc= 0x20e, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,0xDA}, .stack = {}}, "vF := 0xDA");
    step(chip8); // #8
    CheckState(chip8, {.i = 0, .pc= 0x210, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "vF += v3, vF is also carry flag, should be set");
}

TEST_CASE(C8CORE "8xy5 - vx -= vy, set vF to 0 if underflow, 1 if not")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6133, 0x6364, 0x8315, 0x8315, 0x6f84, 0x8f15, 0x6f30, 0x8f15});
    step(chip8); // #1
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(chip8); // #2
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x64, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x64");
    step(chip8); // #3
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x31, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "v3 -= v1, vF should be 1 as this does not underflow");
    step(chip8); // #4
    CheckState(chip8, {.i = 0, .pc= 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 -= v1, vF should be 0 as this time it does underflow");
    step(chip8); // #5
    CheckState(chip8, {.i = 0, .pc= 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,0x84}, .stack = {}}, "vF := 0x84");
    step(chip8); // #6
    CheckState(chip8, {.i = 0, .pc= 0x20c, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "vF -= v1, vF is also carry flag, should be set to 1 as no underflow");
    step(chip8); // #7
    CheckState(chip8, {.i = 0, .pc= 0x20e, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,0x30}, .stack = {}}, "vF := 0x30");
    step(chip8); // #8
    CheckState(chip8, {.i = 0, .pc= 0x210, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "vF -= v1, vF is also carry flag, should be set to 0, as there is underflow");
}

TEST_CASE(C8CORE "8xx6 - vx >>= vx, lost bit in vF, basic shift test is quirk agnostic")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6131, 0x8116, 0x8116, 0x6f84, 0x8ff6, 0x6f83, 0x8ff6});
    step(chip8);  // #1
    CheckState(chip8, {.i = 0, .pc = 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x31");
    step(chip8);  // #2
    CheckState(chip8, {.i = 0, .pc = 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 >>= v1, vF set to 1");
    step(chip8);  // #3
    CheckState(chip8, {.i = 0, .pc = 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 >>= v1, vF set to 0");
    step(chip8);  // #4
    CheckState(chip8, {.i = 0, .pc = 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
    step(chip8);  // #5
    CheckState(chip8, {.i = 0, .pc = 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "vF >>= vF, vF set to 0");
    step(chip8);  // #6
    CheckState(chip8, {.i = 0, .pc = 0x20c, .sp = 0, .dt = 0, .st = 0, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x83}, .stack = {}}, "vF := 0x83");
    step(chip8);  // #7
    CheckState(chip8, {.i = 0, .pc = 0x20e, .sp = 0, .dt = 0, .st = 0, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "vF >>= vF, vF set to 1");
}

TEST_CASE(C8CORE "8xy7 - vx = vy-vx, set vF to 0 if underflow, 1 if not")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6133, 0x6364, 0x8317, 0x8137, 0x6f84, 0x8f37, 0x6fA0, 0x8f17});
    step(chip8); // #1
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(chip8); // #2
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0x64, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x64");
    step(chip8); // #3
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x33,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 = v1-v3, vF should be 0 as this does underflow");
    step(chip8); // #4
    CheckState(chip8, {.i = 0, .pc= 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "v1 = v3-v1, vF should be 1 as this time it not underflow");
    step(chip8); // #5
    CheckState(chip8, {.i = 0, .pc= 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,0x84}, .stack = {}}, "vF := 0x84");
    step(chip8); // #6
    CheckState(chip8, {.i = 0, .pc= 0x20c, .sp = 0, .dt = 0, .st = 0, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "vF = v3-vF, vF is also carry flag, should be set to 1 as no underflow");
    step(chip8); // #7
    CheckState(chip8, {.i = 0, .pc= 0x20e, .sp = 0, .dt = 0, .st = 0, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,0xA0}, .stack = {}}, "vF := 0xA0");
    step(chip8); // #8
    CheckState(chip8, {.i = 0, .pc= 0x210, .sp = 0, .dt = 0, .st = 0, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "vF = v1-vF, vF is also carry flag, should be set to 0, as there is underflow");
}

TEST_CASE(C8CORE "8xxE - vx <<= vx, lost bit in vF, basic shift test is quirk agnostic")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6162, 0x811E, 0x811E, 0x6f84, 0x8ffE, 0x6f43, 0x8ffE});
    step(chip8);  // #1
    CheckState(chip8, {.i = 0, .pc = 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x62");
    step(chip8);  // #2
    CheckState(chip8, {.i = 0, .pc = 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0, 0xC4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 <<= v1, vF set to 0");
    step(chip8);  // #3
    CheckState(chip8, {.i = 0, .pc = 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 <<= v1, vF set to 1");
    step(chip8);  // #4
    CheckState(chip8, {.i = 0, .pc = 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
    step(chip8);  // #5
    CheckState(chip8, {.i = 0, .pc = 0x20a, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "vF <<= vF, vF set to 1");
    step(chip8);  // #6
    CheckState(chip8, {.i = 0, .pc = 0x20c, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x43}, .stack = {}}, "vF := 0x43");
    step(chip8);  // #7
    CheckState(chip8, {.i = 0, .pc = 0x20e, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "vF <<= vF, vF set to 0");
}

TEST_CASE(C8CORE "9xy0 - skip if vx != vy")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6112, 0x6312, 0x9310, 0x6542, 0x9350});
    step(chip8); // #1
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x12,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x12");
    step(chip8); // #2
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x12,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x12");
    step(chip8); // #3
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x12,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 == v1, should not skip");
    step(chip8); // #4
    CheckState(chip8, {.i = 0, .pc= 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0,0x12,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x42");
    step(chip8); // #5
    CheckState(chip8, {.i = 0, .pc= 0x20c, .sp = 0, .dt = 0, .st = 0, .v = {0,0x12,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 != v5, should skip");
}

TEST_CASE(C8CORE "Annn - i := nnn")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0xA032, 0xA314, 0xABFF});
    step(chip8);
    CheckState(chip8, {.i = 0x32, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x32");
    step(chip8);
    CheckState(chip8, {.i = 0x314, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x314");
    step(chip8);
    CheckState(chip8, {.i = 0xBFF, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0xBFF");
}

TEST_CASE(C8CORE "Bnnn - jump nnn+v0")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x60FF, 0xB0FF});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0xFF,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0xFF");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x1FE, .sp = 0, .dt = 0, .st = 0, .v = {0xFF,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "jump0 0x0FF + v0");
}

TEST_CASE(C8CORE "Fx15 - DT = vx")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6125, 0xF115});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x25");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0x25, .st = 0, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "dt := v1");
}


TEST_CASE(C8CORE "Fx18 - ST = vx")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6125, 0xF118});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x25");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0, .st = 0x25, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "st := v1");
}

TEST_CASE(C8CORE "Fx07 - vx = DT")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6125, 0xF115, 0xF207});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x25");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x204, .sp = 0, .dt = 0x25, .st = 0, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "dt := v1");
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x206, .sp = 0, .dt = 0x25, .st = 0, .v = {0,0x25,0x25,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v2 := dt");
}


TEST_CASE(C8CORE "Fx1E - i += vx")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x617f, 0xF11E, 0x6189, 0xF11E});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x7F,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x7F");
    step(chip8);
    CheckState(chip8, {.i = 0x7F, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x7F,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i += v1");
    step(chip8);
    CheckState(chip8, {.i = 0x7F, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x89,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x89");
    step(chip8);
    CheckState(chip8, {.i = 0x108, .pc= 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0,0x89,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i += v1");
}

TEST_CASE(C8CORE "Fx29 - i := hex vx")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0x6109, 0xF129});
    step(chip8);
    CheckState(chip8, {.i = 0, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0x09,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x09");
    step(chip8);
    CHECK(chip8->getI() != 0);
}

TEST_CASE(C8CORE "Fx33 - bcd vx")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0xA400, 0x6189, 0xF133});
    step(chip8);
    CheckState(chip8, {.i = 0x400, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x400");
    step(chip8);
    CheckState(chip8, {.i = 0x400, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0,0x89,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 137");
    step(chip8);
    CheckState(chip8, {.i = 0x400, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0,0x89,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "bcd v1");
    CHECK(chip8->memory()[0x400] == 1);
    CHECK(chip8->memory()[0x401] == 3);
    CHECK(chip8->memory()[0x402] == 7);
}

TEST_CASE(C8CORE "Fx55 - save vx")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    write(chip8, 0x200, {0xA400, 0x6042, 0x6107, 0xF155});
    step(chip8);
    CheckState(chip8, {.i = 0x400, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x400");
    step(chip8);
    CheckState(chip8, {.i = 0x400, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0x42,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x42");
    step(chip8);
    CheckState(chip8, {.i = 0x400, .pc= 0x206, .sp = 0, .dt = 0, .st = 0, .v = {0x42,0x07,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x07");
    step(chip8);
    CheckState(chip8, {.i = -1, .pc= 0x208, .sp = 0, .dt = 0, .st = 0, .v = {0x42,0x07,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "save v1");
    CHECK(chip8->memory()[0x400] == 0x42);
    CHECK(chip8->memory()[0x401] == 0x07);
    CHECK(chip8->memory()[0x402] == 0);
}

TEST_CASE(C8CORE "Fx65 - load vx")
{
    auto chip8 = createChip8Instance();
    chip8->reset();
    chip8->memory()[0x400] = 0x33;
    chip8->memory()[0x401] = 0x99;
    chip8->memory()[0x402] = 0xFF;
    write(chip8, 0x200, {0xA400, 0xF165});
    step(chip8);
    CheckState(chip8, {.i = 0x400, .pc= 0x202, .sp = 0, .dt = 0, .st = 0, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x400");
    step(chip8);
    CheckState(chip8, {.i = -1, .pc= 0x204, .sp = 0, .dt = 0, .st = 0, .v = {0x33,0x99,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "load v1");
}

TEST_SUITE_END();
