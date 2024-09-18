//---------------------------------------------------------------------------------------
// src/emulation/ichip8.hpp
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
#include <emulation/hardware/genericcpu.hpp>
#include <emulation/videoscreen.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "iemulationcore.hpp"

namespace emu
{

struct Chip8State
{
    cycles_t cycles{};
    int frameCycle{};
    std::array<uint8_t,16> v{};
    std::array<uint16_t,16> s{};
    int i{};
    int pc{};
    int sp{};
    int dt{};
    int st{};
};

class IChip8Emulator : public GenericCpu
{
public:
    ~IChip8Emulator() override = default;
    virtual void executeInstructions(int numInstructions) = 0;

    virtual uint8_t getV(uint8_t index) const = 0;
    virtual uint32_t getI() const = 0;
    //virtual uint8_t getSP() const = 0;
    virtual uint8_t stackSize() const = 0;
    virtual const uint16_t* stackElements() const = 0;

    virtual uint8_t* memory() = 0;
    virtual int memSize() const = 0;

    virtual int64_t frames() const = 0;

    virtual uint8_t delayTimer() const = 0;
    virtual uint8_t soundTimer() const = 0;

    //---------------------------------------------------------
    // Additional interfaces have default implementations that
    // allow using the unit tests without much overhead
    //---------------------------------------------------------
    virtual std::tuple<uint16_t, uint16_t, std::string> disassembleInstruction(const uint8_t* code, const uint8_t* end) const = 0;
    virtual std::string dumpStateLine() const = 0;

    virtual bool isGenericEmulation() const { return true; }

    // defaults for unused debugger support
    virtual uint16_t opcode() {
        return (memory()[getPC()] << 8) | memory()[getPC() + 1];
    }
    virtual int64_t machineCycles() const { return cycles(); }

    // functions with default handling to get started with tests
    virtual void handleTimer() {}

    // optional interfaces for audio and/or modern CHIP-8 variant properties
    virtual const uint8_t* getXOAudioPattern() const { return nullptr; }
    virtual uint8_t getXOPitch() const { return 0; }
};


} // namespace emu
