//---------------------------------------------------------------------------------------
// src/emulation/chip8realcorebase.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2023, Steffen Schümann <s.schuemann@pobox.com>
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

#include <emulation/emulatorhost.hpp>
#include <emulation/chip8opcodedisass.hpp>
#include <emulation/hardware/genericcpu.hpp>
#include <emulation/properties.hpp>

#include <fmt/format.h>

namespace emu {

struct RealCoreSetupInfo {
    const char* name;
    const char* propertiesJsonString;
};

class Chip8RealCoreBase : public IEmulationCore, public IChip8Emulator, public Chip8OpcodeDisassembler
{
public:
    explicit Chip8RealCoreBase(EmulatorHost& host)
        : Chip8OpcodeDisassembler(Chip8Variant::CHIP_8)
        , _host(host)
    {
    }

    bool hasBackendStopped() { auto result = _backendStopped; _backendStopped = false; return result; }
    bool inErrorState() const override { return _cpuState == eERROR; }
    CpuState cpuState() const override { return _cpuState; }
    bool hybridChipMode() const { return _isHybridChipMode; }
    int64_t cycles() const override { return _cycles; }
    int64_t frames() const override { return _frames; }
    const ClockedTime& time() const override { return getBackendCpu().time(); }

    virtual GenericCpu& getBackendCpu() = 0;
    const GenericCpu& getBackendCpu() const
    {
        return const_cast<Chip8RealCoreBase*>(this)->getBackendCpu();
    }
    IChip8Emulator* chip8Core() override { return this; }

    virtual Properties& getProperties() = 0;

    std::string dumpStateLine() const override {
        uint16_t op = (readMemoryByte(getPC())<<8)|readMemoryByte(getPC() + 1);
        return fmt::format("V0:{:02x} V1:{:02x} V2:{:02x} V3:{:02x} V4:{:02x} V5:{:02x} V6:{:02x} V7:{:02x} V8:{:02x} V9:{:02x} VA:{:02x} VB:{:02x} VC:{:02x} VD:{:02x} VE:{:02x} VF:{:02x} I:{:04x} SP:{:1x} PC:{:04x} O:{:04x}", getV(0), getV(1), getV(2),
                           getV(3), getV(4), getV(5), getV(6), getV(7), getV(8), getV(9), getV(10), getV(11), getV(12), getV(13), getV(14), getV(15), getI(), getSP(), getPC(), op);
    }

    uint8_t getV(uint8_t index) const override { return _state.v[index & 0xF]; }
    uint32_t getPC() const override { auto pcstr = std::to_string(_state.pc); return _state.pc; }
    uint32_t getI() const override { return _state.i; }
    uint32_t getSP() const override { return _state.sp; }
    uint8_t delayTimer() const override { return _state.dt; }
    uint8_t soundTimer() const override { return _state.st; }

    virtual bool isDisplayEnabled() const = 0;

    uint32_t cpuID() const override { return 0xC8; }

    std::string name() const override
    {
        static const std::string name = "SystemChip8";
        return name;
    }

    const std::vector<std::string>& registerNames() const override
    {
        static const std::vector<std::string> registerNames = {
            "V0", "V1", "V2", "V3", "V4", "V5", "V6", "V7",
            "V8", "V9", "VA", "VB", "VC", "VD", "VE", "VF",
            "I", "DT", "ST", "PC", "SP"
        };
        return registerNames;
    }

    size_t numRegisters() const override
    {
        return 21;
    }

    RegisterValue registerbyIndex(size_t index) const override
    {
        if(index < 16)
            return {_state.v[index], 8};
        if(index == 16)
            return {(uint32_t)_state.i, 16};
        if(index == 17)
            return {(uint32_t)_state.dt, 8};
        if(index == 18)
            return {(uint32_t)_state.st, 8};
        if(index == 19)
            return {(uint32_t)_state.pc, 16};
        return {(uint32_t)_state.sp, 8};
    }

    void setRegister(size_t index, uint32_t value) override
    {
        // TODO
    }

    std::tuple<uint16_t, uint16_t, std::string> disassembleInstruction(const uint8_t* code, const uint8_t* end) const override
    {
        return Chip8OpcodeDisassembler::disassembleInstruction(code, end);
    }

    size_t disassemblyPrefixSize() const override
    {
        return 12;
    }

    std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const override
    {
        if(pc < 0) pc = getPC();
        uint8_t code[4];
        for(size_t i = 0; i < 4; ++i) {
            code[i] = readMemoryByte(pc + i);
        }
        auto [size, opcode, instruction] = Chip8OpcodeDisassembler::disassembleInstruction(code, code + 4);
        if(bytes)
            *bytes = size;
        if (size == 2) {
            // return fmt::format("{:04X}: {:04X}       {}", pc, (code[0] << 8)|code[1], instruction);
            return fmt::format("{:04X}: {:04X}  {}", pc, (code[0] << 8) | code[1], instruction);
        }
        return fmt::format("{:04X}: {:04X} {:04X}  {}", pc, (code[0] << 8)|code[1], (code[2] << 8)|code[3], instruction);
    }
    const std::string& errorMessage() const override { return _errorMessage; }
    bool isBreakpointTriggered() override { return GenericCpu::isBreakpointTriggered() || getBackendCpu().isBreakpointTriggered(); }
protected:
    EmulatorHost& _host;
    Chip8State _state{};
    int64_t _cycles{0};
    int _frames{0};
    bool _backendStopped{false};
    bool _isHybridChipMode{true};
    bool _execChip8{true};
    mutable CpuState _cpuState{eNORMAL};
    std::array<uint8_t,4096> _breakMap{};
    std::map<uint32_t,BreakpointInfo> _breakpoints;
    std::string _errorMessage;
};

}  // namespace emu