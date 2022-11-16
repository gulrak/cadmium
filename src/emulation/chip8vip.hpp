//---------------------------------------------------------------------------------------
// src/emulation/chip8vip.hpp
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

#include <emulation/ichip8.hpp>
#include <emulation/cdp1802.hpp>
#include <emulation/chip8emulatorhost.hpp>

#include <memory>

namespace emu {

class Chip8VIP : public IChip8Emulator, public Cdp1802Bus
{
public:
    constexpr static uint32_t MAX_ADDRESS_MASK = 5095;
    constexpr static uint32_t MAX_MEMORY_SIZE = 4096;

    Chip8VIP(Chip8EmulatorHost& host);
    ~Chip8VIP() override;

    void reset() override;
    std::string name() const override;
    void executeInstruction() override;
    void executeInstructions(int numInstructions) override;
    void tick(int instructionsPerFrame) override;

    uint8_t getV(uint8_t index) const override;
    uint32_t getPC() const override;
    uint32_t getI() const override;
    uint8_t getSP() const override;
    uint8_t stackSize() const override;
    const uint16_t* getStackElements() const override;

    uint8_t* memory() override;
    uint8_t* memoryCopy() override;
    int memSize() const override;

    int64_t cycles() const override;
    int64_t frames() const override;

    uint8_t delayTimer() const override;
    uint8_t soundTimer() const override;

    std::pair<uint16_t, std::string> disassembleInstruction(const uint8_t* code, const uint8_t* end) override;
    std::string dumStateLine() const override;

    void setExecMode(ExecMode mode) override;
    ExecMode execMode() const override;
    CpuState cpuState() const override;

    const uint8_t* getScreenBuffer() const override;

    // CDP1802-Bus
    uint8_t readByte(uint16_t addr) const override;
    void writeByte(uint16_t addr, uint8_t val) override;

private:
    class Private;
    std::unique_ptr<Private> _impl;
};

}
