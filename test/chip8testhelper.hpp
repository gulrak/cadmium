//---------------------------------------------------------------------------------------
// tests/chip8testhelper.hpp
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
#pragma once

struct Chip8State
{
    int i{};
    int pc{};
    int sp{};
    int dt{};
    int st{};
    std::array<int,16> v{};
    std::array<int,16> stack{};
    inline static int stepCount = 0;
    inline static std::string pre;
    inline static std::string post;
};

inline void CheckState(const std::unique_ptr<emu::IChip8Emulator>& chip8, const Chip8State& expected, std::string comment = "")
{
    std::string message = (comment.empty()?"":"\nAfter step #" + std::to_string(Chip8State::stepCount) + "\nCOMMENT: " + comment) + "\nPRE:  " + Chip8State::pre + "\nPOST: " + Chip8State::post;
    INFO(message);
    if(expected.i >= 0) CHECK(expected.i == chip8->getI());
    if(expected.pc >= 0) CHECK(expected.pc == chip8->getPC());
    if(expected.sp >= 0) CHECK(expected.sp == chip8->getSP());
    if(expected.dt >= 0) CHECK(expected.dt == chip8->delayTimer());
    if(expected.st >= 0) CHECK(expected.st == chip8->soundTimer());

    if(expected.v[0] >= 0) CHECK(expected.v[0] == chip8->getV(0));
    if(expected.v[1] >= 0) CHECK(expected.v[1] == chip8->getV(1));
    if(expected.v[2] >= 0) CHECK(expected.v[2] == chip8->getV(2));
    if(expected.v[3] >= 0) CHECK(expected.v[3] == chip8->getV(3));
    if(expected.v[4] >= 0) CHECK(expected.v[4] == chip8->getV(4));
    if(expected.v[5] >= 0) CHECK(expected.v[5] == chip8->getV(5));
    if(expected.v[6] >= 0) CHECK(expected.v[6] == chip8->getV(6));
    if(expected.v[7] >= 0) CHECK(expected.v[7] == chip8->getV(7));
    if(expected.v[8] >= 0) CHECK(expected.v[8] == chip8->getV(8));
    if(expected.v[9] >= 0) CHECK(expected.v[9] == chip8->getV(9));
    if(expected.v[10] >= 0) CHECK(expected.v[0xA] == chip8->getV(0xA));
    if(expected.v[11] >= 0) CHECK(expected.v[0xB] == chip8->getV(0xB));
    if(expected.v[12] >= 0) CHECK(expected.v[0xC] == chip8->getV(0xC));
    if(expected.v[13] >= 0) CHECK(expected.v[0xD] == chip8->getV(0xD));
    if(expected.v[14] >= 0) CHECK(expected.v[0xE] == chip8->getV(0xE));
    if(expected.v[15] >= 0) CHECK(expected.v[0xF] == chip8->getV(0xF));

    for (int i = 0; i <= expected.sp; ++i) {
        if(expected.stack[i] >= 0) CHECK(expected.stack[i] == chip8->getStackElements()[i]);
    }
}

inline void write(const std::unique_ptr<emu::IChip8Emulator>& chip8, uint32_t address, std::initializer_list<uint16_t> values)
{
    size_t offset = 0;
    for(auto val : values) {
        chip8->memory()[address + offset++] = val>>8;
        chip8->memory()[address + offset++] = val & 0xFF;
    }
    Chip8State::stepCount = 0;
}

inline void step(const std::unique_ptr<emu::IChip8Emulator>& chip8)
{
    Chip8State::pre = chip8->dumStateLine();
    chip8->executeInstruction();
    Chip8State::stepCount++;
    Chip8State::post = chip8->dumStateLine();
}

