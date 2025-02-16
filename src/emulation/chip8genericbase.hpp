//---------------------------------------------------------------------------------------
// src/emulation/chip8genericbase.hpp
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
#pragma once

#include <emulation/chip8opcodedisass.hpp>
#include <emulation/ichip8.hpp>
#include <emulation/iemulationcore.hpp>

namespace emu {

class Chip8GenericBase : public IEmulationCore, public IChip8Emulator
{
public:
    enum MegaChipBlendMode { eBLEND_NORMAL = 0, eBLEND_ALPHA_25 = 1, eBLEND_ALPHA_50 = 2, eBLEND_ALPHA_75 = 3, eBLEND_ADD = 4, eBLEND_MUL = 5 };
    enum Chip8Font { C8F5_COSMAC, C8F5_ETI, C8F5_DREAM, C8F5_CHIP48, C8F5_FISHNCHIPS, C8F5_AKOUZ1 };
    enum Chip8BigFont { C8F10_NONE, C8F10_SCHIP10, C8F10_SCHIP11, C8F10_FISHNCHIPS, C8F10_MEGACHIP, C8F10_XOCHIP, C8F10_AUCHIP, C8F10_AKOUZ1 };
    explicit Chip8GenericBase(Chip8Variant variant, std::optional<uint64_t> clockRate);
    bool inErrorState() const override { return _cpuState == eERROR; }
    bool isGenericEmulation() const override { return true; }
    GenericCpu* executionUnit(size_t index) override { return this; }
    void setFocussedExecutionUnit(GenericCpu* unit) override {}
    GenericCpu* focussedExecutionUnit() override { return this; }
    IChip8Emulator* chip8Core() override { return this; }
    void setExecMode(ExecMode mode) override { GenericCpu::setExecMode(mode); }
    ExecMode execMode() const override { return GenericCpu::execMode(); }
    int64_t cycles() const override { return _cycleCounter; }
    int64_t frames() const override { return _frameCounter; }
    int frameRate() const override { return 60; };
    const ClockedTime& time() const override { return _systemTime; }
    const std::string& errorMessage() const override { return _errorMessage; }
    uint8_t getV(uint8_t index) const override { return _rV[index]; }
    uint32_t getPC() const override { return _rPC; }
    uint32_t getI() const override { return _rI; }
    uint32_t getSP() const override { return _rSP; }
    uint8_t delayTimer() const override { return _rDT; }
    uint8_t soundTimer() const override { return _rST; }
    uint8_t* memory() override { return _memory.data(); }
    int memSize() const override { return static_cast<int>(_memory.size()); }
    std::tuple<uint16_t, uint16_t, std::string> disassembleInstruction(const uint8_t* code, const uint8_t* end) const override;
    size_t disassemblyPrefixSize() const override;
    std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const override;
    std::string dumpStateLine() const override;
    bool loadData(std::span<const uint8_t> data, std::optional<uint32_t> loadAddress) override;
    const std::vector<std::string>& registerNames() const override;
    size_t numRegisters() const override { return 21; }
    RegisterValue registerbyIndex(size_t index) const override;
    void setRegister(size_t index, uint32_t value) override;
    uint8_t readMemoryByte(uint32_t addr) const override { return addr < _memory.size() ? _memory[addr] : 0xFF; }
    static std::pair<const uint8_t*, size_t> smallFontData(Chip8Font font);
    static std::pair<const uint8_t*, size_t> bigFontData(Chip8BigFont font);

protected:
    void initExpressionist();
    Chip8OpcodeDisassembler _disassembler;
    std::vector<uint8_t> _memory{};
    uint8_t* _rV{};
    uint8_t _rDT{};
    uint8_t _rST{};
    uint16_t _rSP{};
    uint32_t _rI{};
    uint32_t _rPC{};
    int64_t _cycleCounter{};
    int _frameCounter{};
    ClockedTime _systemTime;
};

//---------------------------------------------------------------------------------------
// Sprite drawing related quirk flags for templating
//---------------------------------------------------------------------------------------
enum Chip8Quirks { HiresSupport = 1, MultiColor = 2, WrapSprite = 4, SChip11Collisions = 8, SChip1xLoresDraw = 16 };

} // emu

