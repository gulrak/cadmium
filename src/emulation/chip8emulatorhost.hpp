//---------------------------------------------------------------------------------------
// src/emulation/chip8emulatorhost.hpp
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

#include <array>
#include <cstdint>
#include <vector>

namespace emu {

class Chip8EmulatorHost
{
public:
    virtual ~Chip8EmulatorHost() = default;
    virtual bool isHeadless() const = 0;
    virtual uint8_t getKeyPressed() = 0;
    virtual bool isKeyDown(uint8_t key) = 0;
    virtual bool isKeyUp(uint8_t key) { return !isKeyDown(key); }
    virtual void updateScreen() = 0;
    virtual void updatePalette(const std::array<uint8_t, 16>& palette) = 0;
    virtual void updatePalette(const std::vector<uint32_t>& palette, size_t offset) = 0;
};

}
