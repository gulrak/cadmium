//---------------------------------------------------------------------------------------
// tests/basic_opcode_tests.hpp
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


TEST_SUITE_BEGIN(C8CORE "-Basic-Opcodes");

TEST_CASE(C8CORE ": reset()")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    CheckState(core, {.i = 0, .pc = start, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}}, "state after reset");
}

TEST_CASE(C8CORE ": 1nnn - jump nnn")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {uint16_t(0x1004 + start)});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}}, "jump 0x204");
}

TEST_CASE(C8CORE ": 2nnn - call nnn")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {uint16_t(0x2004 + start)});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 1, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {start + 2,0}}, "call 0x204");
}

TEST_CASE(C8CORE ": 00EE - return")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {uint16_t(0x2008 + start),0x0000,0x0000,0x0000,uint16_t(std::string(C8CORE) == "vip-chip-8x-tpd" ? 0x00F0 : 0x00EE)});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 1, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {start + 2,0}}, "call 0x208");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {-1,0}}, "return");
}

TEST_CASE(C8CORE ": 6xnn - vx := nn")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6032, 0x6314, 0x6bff});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x32,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x32");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x32,0,0,0x14, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x14");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x32,0,0,0x14, 0,0,0,0, 0,0,0,0xff, 0,0,0,0}, .stack = {}}, "vB := 0xff");
}

TEST_CASE(C8CORE ": 3xnn - skip if vx == nn")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x3304, 0x6542, 0x3542});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 != 4, should not skip");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x42");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 == 0x42, should skip");
}

#if QUIRKS & QUIRK_LONG_SKIP
TEST_CASE(C8CORE ": 3xnn - skip long opcode if vx == nn")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x3304, 0x6542, 0x3542, uint16_t(std::string(C8CORE) == "xo-chip" ? 0xF000 : 0x0100)});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 != 4, should not skip");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x42");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 == 0x42, should skip");
}
#endif

#if QUIRKS & QUIRK_LONG_SKIP
TEST_CASE(C8CORE ": 4xnn - skip long opcode if vx != nn")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6312, 0x4312, 0x6542, 0x4540, uint16_t(std::string(C8CORE) == "xo-chip" ? 0xF000 : 0x0100)});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 9x12");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 == 0x12, should not skip");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x42");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 != 0x40, should skip");
}
#endif

TEST_CASE(C8CORE ": 4xnn - skip if vx != nn")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6312, 0x4312, 0x6542, 0x4540});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 9x12");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 == 0x12, should not skip");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x42");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 != 0x40, should skip");
}

TEST_CASE(C8CORE ": 5xy0 - skip if vx == vy")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6312, 0x5310, 0x6512, 0x5350});
    step(core); // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x12");
    step(core); // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 != v1, should not skip");
    step(core); // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0x12,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x12");
    step(core); // #4
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0x12,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 == v5, should skip");
}

#if QUIRKS & QUIRK_LONG_SKIP
TEST_CASE(C8CORE ": 5xy0 - skip long opcode if vx == vy")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6312, 0x5310, 0x6512, 0x5350, uint16_t(std::string(C8CORE) == "xo-chip" ? 0xF000 : 0x0100)});
    step(core); // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x12");
    step(core); // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 != v1, should not skip");
    step(core); // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0x12,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x12");
    step(core); // #4
    CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0x12, 0,0x12,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 == v5, should skip");
}
#endif

TEST_CASE(C8CORE ": 7xnn - vx += nn")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6132, 0x7154, 0x717f});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x32,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x32");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x86,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 += 0x54");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x05,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 += 0x7f");
}

TEST_CASE(C8CORE ": 8xy0 - vx := vy")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6132,0x6f42, 0x8210, 0x8220});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x32,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x32");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x32,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v1 := 0x32");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x32,0x32,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v2 := v1");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x32,0x32,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v2 := v2");
}

TEST_CASE(C8CORE ": 8xy1 - vx |= vy")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6133, 0x6381, 0x8311, 0x8331});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x81");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xb3, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 |= v1");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xb3, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 |= v3");
}

#if QUIRKS & QUIRK_VF_NOT_RESET
TEST_CASE(C8CORE ": 8xy1 - vx |= vy, checking vf is kept")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6f42, 0x6133, 0x6381, 0x8311, 0x8331});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "vf := 0x42");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v1 := 0x33");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v3 := 0x81");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xb3, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v3 |= v1");
}
#else
TEST_CASE(C8CORE ": 8xy1 - vx |= vy, checking vf is reset")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6f42, 0x6133, 0x6381, 0x8311, 0x8331});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "vf := 0x42");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v1 := 0x33");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v3 := 0x81");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xb3, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 |= v1");
}
#endif

TEST_CASE(C8CORE ": 8xy2 - vx &= vy")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6133, 0x6381, 0x8312, 0x8332});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x81");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 &= v1");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 &= v3");
}

#if QUIRKS & QUIRK_VF_NOT_RESET
TEST_CASE(C8CORE ": 8xy2 - vx &= vy, checking vf is kept")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6f42, 0x6133, 0x6381, 0x8312});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "vf := 0x42");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v1 := 0x33");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v3 := 0x81");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v3 &= v1");
}
#else
TEST_CASE(C8CORE ": 8xy2 - vx &= vy, checking vf is reset")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6f42, 0x6133, 0x6381, 0x8312});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "vf := 0x42");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v1 := 0x33");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v3 := 0x81");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,1, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 &= v1");
}
#endif

TEST_CASE(C8CORE ": 8xy3 - vx ^= vy")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6133, 0x6381, 0x8313, 0x8333});
    step(core); // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(core); // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x81");
    step(core); // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xB2, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 ^= v1");
    step(core); // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 ^= v3");
}

#if QUIRKS & QUIRK_VF_NOT_RESET
TEST_CASE(C8CORE ": 8xy3 - vx ^= vy, checking vf is kept")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6f42, 0x6133, 0x6381, 0x8313});
    step(core); // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "vf := 0x42");
    step(core); // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v1 := 0x33");
    step(core); // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v3 := 0x81");
    step(core); // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xB2, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v3 ^= v1");
}
#else
TEST_CASE(C8CORE ": 8xy3 - vx ^= vy, checking vf is reset")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6f42, 0x6133, 0x6381, 0x8313});
    step(core); // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "vf := 0x42");
    step(core); // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v1 := 0x33");
    step(core); // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "v3 := 0x81");
    step(core); // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xB2, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 ^= v1");
}
#endif

TEST_CASE(C8CORE ": 8xy4 - vx += vy, set vF to 1 on overflow, 0 if not")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6133, 0x6381, 0x8314, 0x8334, 0x6f84, 0x8f34, 0x6fda, 0x8f34});
    step(core); // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(core); // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x81, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x81");
    step(core); // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xB4, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 += v1");
    step(core); // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "v3 += v3, vF should be set as this overflows");
    step(core); // #5
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,0x84}, .stack = {}}, "vF := 0x84");
    step(core); // #6
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,0xEC}, .stack = {}}, "vF += v3, vF is also carry flag, but CHIPOS overwrites it");
    else
        CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "vF += v3, vF is also carry flag, should be cleared");
    step(core); // #7
    CheckState(core, {.i = 0, .pc = start + 14, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,0xDA}, .stack = {}}, "vF := 0xDA");
    step(core); // #8
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 16, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,0x42}, .stack = {}}, "vF += v3, vF is also carry flag, but CHIPOS overwrites it");
    else
        CheckState(core, {.i = 0, .pc = start + 16, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x68, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "vF += v3, vF is also carry flag, should be set");
}

TEST_CASE(C8CORE ": 8xy5 - vx -= vy, set vF to 0 if underflow, 1 if not")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6133, 0x6364, 0x8315, 0x8315, 0x6f84, 0x8f15, 0x6f30, 0x8f15});
    step(core); // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(core); // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x64, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x64");
    step(core); // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x31, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "v3 -= v1, vF should be 1 as this does not underflow");
    step(core); // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 -= v1, vF should be 0 as this time it does underflow");
    step(core); // #5
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,0x84}, .stack = {}}, "vF := 0x84");
    step(core); // #6
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,0x51}, .stack = {}}, "vF -= v1, vF is also carry flag, but CHIPOS ignores it");
    else
        CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "vF -= v1, vF is also carry flag, should be set to 1 as no underflow");
    step(core); // #7
    CheckState(core, {.i = 0, .pc = start + 14, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,0x30}, .stack = {}}, "vF := 0x30");
    step(core); // #8
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 16, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,0xFD}, .stack = {}}, "vF -= v1, vF is also carry flag, but CHIPOS ignores it");
    else
        CheckState(core, {.i = 0, .pc = start + 16, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xFE, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "vF -= v1, vF is also carry flag, should be set to 0, as there is underflow");
}

TEST_CASE(C8CORE ": 8xx6 - vx >>= vx, lost bit in vF, basic shift test is quirk agnostic")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6131, 0x8116, 0x8116, 0x6f84, 0x8ff6, 0x6f83, 0x8ff6});
    step(core);  // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x31");
    step(core);  // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0x18, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 >>= v1, vF set to 1");
    step(core);  // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 >>= v1, vF set to 0");
    step(core);  // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
    step(core);  // #5
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x42}, .stack = {}}, "vF >>= vF, CHIPOSLO overwrites the bit in VF");
    else
        CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "vF >>= vF, vF set to 0");
    step(core);  // #6
    CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x83}, .stack = {}}, "vF := 0x83");
    step(core);  // #7
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 14, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x41}, .stack = {}}, "vF >>= vF, CHIPOSLO overwrites the bit in VF");
    else
        CheckState(core, {.i = 0, .pc = start + 14, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0xC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "vF >>= vF, vF set to 1");
}

#if !(QUIRKS & (QUIRK_SHIFT_VX|QUIRK_NO_SHIFT))
TEST_CASE(C8CORE ": 8xy6 - vx >>= vy, lost bit in vF, this shift test expects vy to be used")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6142, 0x6231, 0x8126, 0x6f84, 0x8f26});
    step(core);  // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x42, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x42");
    step(core);  // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x42, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v2 := 0x31");
    step(core);  // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x18, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 >>= v2, vF set to 1");
    step(core);  // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x18, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
    step(core);  // #5
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x18, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "vF >>= v2, vF set to 1");
}
#endif

#if !(QUIRKS & QUIRK_NO_SHIFT) && (QUIRKS & QUIRK_SHIFT_VX)
TEST_CASE(C8CORE ": 8xy6 - vx >>= vx, lost bit in vF, this shift test expects vy to be ignored")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6141, 0x6231, 0x8126, 0x6f84, 0x8f26});
    step(core);  // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x41, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x41");
    step(core);  // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x41, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v2 := 0x31");
    step(core);  // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x20, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 >>= v2, v2 ignored, vF set to 1");
    step(core);  // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x20, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
    step(core);  // #5
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x20, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "vF >>= v2, v2 ignored, vF set to 0");
}
#endif

TEST_CASE(C8CORE ": 8xy7 - vx = vy-vx, set vF to 0 if underflow, 1 if not")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6133, 0x6364, 0x8317, 0x8137, 0x6f84, 0x8f37, 0x6fA0, 0x8f17});
    step(core); // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x33");
    step(core); // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0x64, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x64");
    step(core); // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x33,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 = v1-v3, vF should be 0 as this does underflow");
    step(core); // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "v1 = v3-v1, vF should be 1 as this time it not underflow");
    step(core); // #5
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,0x84}, .stack = {}}, "vF := 0x84");
    step(core); // #6
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,0x4B}, .stack = {}}, "vF = v3-vF, vF is also carry flag, but CHIPOSLO ignores that");
    else
        CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,1}, .stack = {}}, "vF = v3-vF, vF is also carry flag, should be set to 1 as no underflow");
    step(core); // #7
    CheckState(core, {.i = 0, .pc = start + 14, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,0xA0}, .stack = {}}, "vF := 0xA0");
    step(core); // #8
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 16, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,0xFC}, .stack = {}}, "vF = v1-vF, vF is also carry flag, but CHIPOSLO ignores that");
    else
        CheckState(core, {.i = 0, .pc = start + 16, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x9C,0,0xCF, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "vF = v1-vF, vF is also carry flag, should be set to 0, as there is underflow");
}

TEST_CASE(C8CORE ": 8xxE - vx <<= vx, lost bit in vF, basic shift test is quirk agnostic")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6162, 0x811E, 0x811E, 0x6f84, 0x8ffE, 0x6f43, 0x8ffE});
    step(core);  // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0x62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x62");
    step(core);  // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0xC4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 <<= v1, vF set to 0");
    step(core);  // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 <<= v1, vF set to 1");
    step(core);  // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
    step(core);  // #5
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8}, .stack = {}}, "vF <<= vF, CHIPOSLO overwrites the bit in VF");
    else
        CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "vF <<= vF, vF set to 1");
    step(core);  // #6
    CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x43}, .stack = {}}, "vF := 0x43");
    step(core);  // #7
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 14, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x86}, .stack = {}}, "vF <<= vF, CHIPOSLO overwrites the bit in VF");
    else
        CheckState(core, {.i = 0, .pc = start + 14, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0, 0x88, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "vF <<= vF, vF set to 0");
}

#if !(QUIRKS & (QUIRK_SHIFT_VX|QUIRK_NO_SHIFT))
TEST_CASE(C8CORE ": 8xyE - vx <<= vy, lost bit in vF, this shift test expects vy to be used")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6142, 0x6281, 0x812e, 0x6f84, 0x8f1e});
    step(core);  // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x42, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x42");
    step(core);  // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x42, 0x81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v2 := 0x31");
    step(core);  // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x02, 0x81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 <<= v2, vF set to 1");
    step(core);  // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x02, 0x81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
    step(core);  // #5
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x02, 0x81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "vF <<= v1, vF set to 0");
}
#endif

#if !(QUIRKS & QUIRK_NO_SHIFT) && (QUIRKS & QUIRK_SHIFT_VX)
TEST_CASE(C8CORE ": 8xy6 - vx >>= vx, lost bit in vF, this shift test expects vy to be ignored")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6141, 0x6231, 0x8126, 0x6f84, 0x8f26});
    step(core);  // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x41, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v1 := 0x41");
    step(core);  // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x41, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "v2 := 0x31");
    step(core);  // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x20, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, .stack = {}}, "v1 >>= v2, v2 ignored, vF set to 1");
    step(core);  // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x20, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x84}, .stack = {}}, "vF := 0x84");
    step(core);  // #5
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = 0, .st = 0, .v = {0, 0x20, 0x31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, .stack = {}}, "vF >>= v2, v2 ignored, vF set to 0");
}
#endif

TEST_CASE(C8CORE ": 9xy0 - skip if vx != vy")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6112, 0x6312, 0x9310, 0x6542, 0x9350});
    step(core); // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x12,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x12");
    step(core); // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x12,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x12");
    step(core); // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x12,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 == v1, should not skip");
    step(core); // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x12,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x42");
    step(core); // #5
    CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x12,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 != v5, should skip");
}

#if QUIRKS & QUIRK_LONG_SKIP
TEST_CASE(C8CORE ": 9xy0 - skip if vx != vy")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6112, 0x6312, 0x9310, 0x6542, 0x9350, uint16_t(std::string(C8CORE) == "xo-chip" ? 0xF000 : 0x0100)});
    step(core); // #1
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x12,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x12");
    step(core); // #2
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x12,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 := 0x12");
    step(core); // #3
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x12,0,0x12, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 == v1, should not skip");
    step(core); // #4
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x12,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v5 := 0x42");
    step(core); // #5
    CheckState(core, {.i = 0, .pc = start + 14, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x12,0,0x12, 0,0x42,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v3 != v5, should skip");
}
#endif

TEST_CASE(C8CORE ": Annn - i := nnn")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0xA032, 0xA314, 0xABFF});
    step(core);
    CheckState(core, {.i = 0x32, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x32");
    step(core);
    CheckState(core, {.i = 0x314, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x314");
    step(core);
    CheckState(core, {.i = 0xBFF, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0xBFF");
}

#if !(QUIRKS & QUIRK_NO_JUMP)
TEST_CASE(C8CORE ": Bnnn - jump nnn+v0")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x60FF, 0xB0FF});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0xFF,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0xFF");
    step(core);
    CheckState(core, {.i = 0, .pc = start - 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0xFF,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "jump0 0x0FF + v0");
}
#endif

TEST_CASE(C8CORE ": Cxnn - vx := random nn")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f, 0xC21f});
    uint8_t maskAnd = 0x1f;
    uint8_t maskOr = 0;
    for(int i = 0; i < 16; ++i) {
        step(core);
        maskAnd &= core->getV(2);
        maskOr |= core->getV(2);
    }
    CHECK(maskAnd == 0);
    CHECK(maskOr == 0x1f);
    core->reset();
    write(core, start, {0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5, 0xC2a5});
    maskAnd = 0xa5;
    maskOr = 0;
    for(int i = 0; i < 16; ++i) {
        step(core);
        maskAnd &= core->getV(2);
        maskOr |= core->getV(2);
    }
    CHECK(maskAnd == 0);
    CHECK(maskOr == 0xa5);
}

TEST_CASE(C8CORE ": Dxyn - sprite vx vy n")
{
    std::string pacImage = "..####.\n.######\n##.###.\n#####..\n#####..\n######.\n.######\n..####.\n";
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6003, 0x6104, 0xA400, 0xD018, uint16_t(0x1000 + start + 8)});
    write(core, 0x400, {0x3c7e, 0xdcf8, 0xf8fc, 0x7e3c, 0x8000});
    step(core);
    step(core);
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {3,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "set up draw");
    while(core->getPC() == start + 6) {
        step(core);
    }
    CheckState(core, {.i = 0x400, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {3,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "sprite v0 v1 8");
    host->executeFrame();
    host->executeFrame();
#if QUIRKS & QUIRK_SCALE_X2
    auto scaleX = 2;
#else
    auto scaleX = 1;
#endif
#if QUIRKS & QUIRK_SCALE_Y4
    auto scaleY = 4;
#elif QUIRKS & QUIRK_SCALE_Y2
    auto scaleY = 2;
#else
    auto scaleY = 1;
#endif
    auto [rect, content] = host->chip8UsedScreen(scaleX, scaleY);
    CHECK(3 == rect.x);
    CHECK(4 == rect.y);
    CHECK(pacImage == content);
}

TEST_CASE(C8CORE ": Ex9E - if vx -key then")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6104, 0xE19E, 0x6012, 0xE19E, 0x6013, 0x6234});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x4");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "if v1 -key then");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x12,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x12");
    host->keyDown(4);
    step(core);
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x12,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "if v1 -key then");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x12,4,0x34,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x12");
}

#if QUIRKS & QUIRK_LONG_SKIP
TEST_CASE(C8CORE ": Ex9E - skip long opcode if vx -key then")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6104, 0xE19E, 0x6012, 0xE19E, uint16_t(std::string(C8CORE) == "xo-chip" ? 0xF000 : 0x0100), 0x5555, 0x6234});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x4");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "if v1 -key then");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x12,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x12");
    host->keyDown(4);
    step(core);
    CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x12,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "if v1 -key then");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 14, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x12,4,0x34,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x12");
}
#endif

TEST_CASE(C8CORE ": ExA1 - if vx key then")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6104, 0xE1A1, 0x6012, 0xE1A1, 0x6013, 0x6234});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x4");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "if v1 key then");
    host->keyDown(4);
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "if v1 key then");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x13,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x12");
}

#if QUIRKS & QUIRK_LONG_SKIP
TEST_CASE(C8CORE ": ExA1 - skip long opcode if vx key then")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6104, 0xE1A1, uint16_t(std::string(C8CORE) == "xo-chip" ? 0xF000 : 0x0100), 0x5555, 0xE1A1, 0x6013, 0x6234});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x4");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "if v1 key then");
    host->keyDown(4);
    step(core);
    CheckState(core, {.i = 0, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "if v1 key then");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 12, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x13,4,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x12");
}
#endif

TEST_CASE(C8CORE ": Fx15 - DT = vx")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6125, 0xF115});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x25");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = 0x25, .st = TIMER_DEFAULT, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "dt := v1");
}

TEST_CASE(C8CORE ": Fx07 - vx = DT")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6125, 0xF115, 0xF207});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x25");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = 0x25, .st = TIMER_DEFAULT, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "dt := v1");
    step(core);
    CheckState(core, {.i = 0, .pc = start + 6, .sp = 0, .dt = 0x25, .st = TIMER_DEFAULT, .v = {0,0x25,0x25,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v2 := dt");
}

TEST_CASE(C8CORE ": Fx18 - ST = vx")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6125, 0xF118});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x25");
    step(core);
    if(core->name() == "DREAM6800")
        CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = 0x00, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "st := v1");
    else
        CheckState(core, {.i = 0, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = 0x25, .v = {0,0x25,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "st := v1");
}

TEST_CASE(C8CORE ": Fx1E - i += vx")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x617f, 0xF11E, 0x6189, 0xF11E});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x7F,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x7F");
    step(core);
    CheckState(core, {.i = 0x7F, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x7F,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i += v1");
    step(core);
    CheckState(core, {.i = 0x7F, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x89,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x89");
    step(core);
    CheckState(core, {.i = 0x108, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x89,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i += v1");
}

TEST_CASE(C8CORE ": Fx29 - i := hex vx")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0x6109, 0xF129});
    step(core);
    CheckState(core, {.i = 0, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x09,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x09");
    step(core);
    CHECK(core->getI() != 0);
}

TEST_CASE(C8CORE ": Fx33 - bcd vx")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0xA400, 0x6189, 0xF133});
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x400");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x89,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 137");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0x89,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "bcd v1");
    CHECK(core->memory()[0x400] == 1);
    CHECK(core->memory()[0x401] == 3);
    CHECK(core->memory()[0x402] == 7);
}

#if !(QUIRKS & (QUIRK_LOAD_X|QUIRK_LOAD_0))
TEST_CASE(C8CORE ": Fx55 - save vx, i is incremented by x+1")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0xA400, 0x6042, 0x6107, 0x6233, 0xF155});
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x400");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x42");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0x07,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x07");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0x07,0x33,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x07");
    step(core);
    CheckState(core, {.i = 0x400 + 2, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0x07,0x33,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "save v1");
    CHECK(core->memory()[0x400] == 0x42);
    CHECK(core->memory()[0x401] == 0x07);
    CHECK(core->memory()[0x402] == 0);
}
#elif QUIRKS & QUIRK_LOAD_X
TEST_CASE(C8CORE ": Fx55 - save vx, i is incremented by x")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0xA400, 0x6042, 0x6107, 0x6233, 0xF155});
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x400");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x42");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0x07,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x07");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0x07,0x33,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x07");
    step(core);
    CheckState(core, {.i = 0x400 + 1, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0x07,0x33,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "save v1");
    CHECK(core->memory()[0x400] == 0x42);
    CHECK(core->memory()[0x401] == 0x07);
    CHECK(core->memory()[0x402] == 0);
}
#elif QUIRKS & QUIRK_LOAD_0
TEST_CASE(C8CORE ": Fx55 - save vx, i is unchanged")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    write(core, start, {0xA400, 0x6042, 0x6107, 0x6233, 0xF155});
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x400");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v0 := 0x42");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 6, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0x07,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x07");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 8, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0x07,0x33,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "v1 := 0x07");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 10, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x42,0x07,0x33,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "save v1");
    CHECK(core->memory()[0x400] == 0x42);
    CHECK(core->memory()[0x401] == 0x07);
    CHECK(core->memory()[0x402] == 0);
}
#else
#error "An invalid load/save quirk combination is used!"
#endif

#if !(QUIRKS & (QUIRK_LOAD_X|QUIRK_LOAD_0))
TEST_CASE(C8CORE ": Fx65 - load vx, checking i is incremented by x+1")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    core->memory()[0x400] = 0x33;
    core->memory()[0x401] = 0x99;
    core->memory()[0x402] = 0xFF;
    write(core, start, {0xA400, 0xF165});
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x400");
    step(core);
    CheckState(core, {.i = 0x402, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x33,0x99,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "load v1");
}
#elif QUIRKS & QUIRK_LOAD_X
TEST_CASE(C8CORE ": Fx65 - load vx, checking i is incremented by x")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    core->memory()[0x400] = 0x33;
    core->memory()[0x401] = 0x99;
    core->memory()[0x402] = 0xFF;
    write(core, start, {0xA400, 0xF165});
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x400");
    step(core);
    CheckState(core, {.i = 0x401, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x33,0x99,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "load v1");
}
#elif QUIRKS & QUIRK_LOAD_0
TEST_CASE(C8CORE ": Fx65 - load vx, checking i is unchanged")
{
    auto [host, core, start] = createChip8Instance(C8CORE);
    core->reset();
    core->memory()[0x400] = 0x33;
    core->memory()[0x401] = 0x99;
    core->memory()[0x402] = 0xFF;
    write(core, start, {0xA400, 0xF165});
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 2, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "i := 0x400");
    step(core);
    CheckState(core, {.i = 0x400, .pc = start + 4, .sp = 0, .dt = TIMER_DEFAULT, .st = TIMER_DEFAULT, .v = {0x33,0x99,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .stack = {}}, "load v1");
}
#else
#error "An invalid load/save quirk combination is used!"
#endif

TEST_SUITE_END();
