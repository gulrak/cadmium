//---------------------------------------------------------------------------------------
// src/emulation/cdp186x.hpp
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

#include <emulation/config.hpp>
#include <emulation/chip8options.hpp>

#include <array>

namespace emu {

#define VIDEO_FIRST_VISIBLE_LINE 80
#define VIDEO_FIRST_INVISIBLE_LINE  208

class Cdp1802;

class Cdp186x
{
public:
    enum Type { eCDP1861, eCDP1861_C10, eCDP1861_62, eCDP1864 };
    Cdp186x(Type type, Cdp1802& cpu, const Chip8EmulatorOptions& options);
    void reset();
    bool getNEFX() const;
    int executeStep();
    void enableDisplay();
    void disableDisplay();
    bool isDisplayEnabled() const { return _displayEnabled; }
    int frames() const { return _frameCounter; }
    const uint8_t* getScreenBuffer() const;

    static int64_t machineCycle(cycles_t cycles)
    {
        return cycles >> 3;
    }

    static int frameCycle(cycles_t cycles)
    {
        return machineCycle(cycles) % 3668;
    }

    static int videoLine(cycles_t cycles)
    {
        return frameCycle(cycles) / 14;
    }

    static cycles_t nextFrame(cycles_t cycles)
    {
        return ((cycles + 8*3668) / (8*3668)) * (8*3668);
    }

private:
    Cdp1802& _cpu;
    Type _type{eCDP1861};
    const Chip8EmulatorOptions& _options;
    std::array<uint8_t,256*192> _screenBuffer;
    int _frameCycle{0};
    int _frameCounter{0};
    bool _displayEnabled{false};
};

}
