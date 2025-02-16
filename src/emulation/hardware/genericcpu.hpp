//---------------------------------------------------------------------------------------
// src/emulation/mc682x.hpp
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

#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <span>
#include <ranges>
#include <string>
#include <vector>

#include <emulation/time.hpp>
#include <emulation/expressionist.hpp>

namespace emu
{

/// Abstract CPU base class that allows generic control and register retrieval
class GenericCpu
{
public:
    enum ExecMode { ePAUSED, eRUNNING, eSTEP, eSTEPOVER, eSTEPOUT };
    enum CpuState { eNORMAL, eIDLE, eWAIT = eIDLE, eHALT, eERROR };
    enum StackDirection { eDOWNWARDS, eUPWARDS };
    enum Endianness { eNATIVE, eLITTLE, eBIG };
    struct BreakpointInfo {
        enum Type { eTRANSIENT, eCODED };
        std::string label;
        std::string condition;
        Type type{eTRANSIENT};
        bool isEnabled{true};
        Expressionist::CompiledExpression conditionExpr;
    };
    struct RegisterValue {
        uint32_t value{};
        uint32_t size{};
    };
    struct StackContent
    {
        int entrySize{2};
        Endianness endianness{eLITTLE};
        StackDirection stackDirection{eDOWNWARDS};
        std::span<const uint8_t> content{};
    };
    using RegisterPack = std::vector<RegisterValue>;
    virtual ~GenericCpu() = default;
    virtual void reset() = 0;
    virtual int executeInstruction() = 0;
    virtual int64_t executeFor(int64_t microseconds) = 0;
    virtual ExecMode execMode() const { return _execMode; }
    virtual void setExecMode(ExecMode mode) { _execMode = mode; if(mode == eSTEPOVER) _stepOverSP = getSP(); }
    virtual bool inErrorState() const = 0;
    virtual const std::string& errorMessage() const { return _errorMessage; }
    virtual uint32_t cpuID() const = 0;
    virtual std::string name() const = 0;
    virtual CpuState cpuState() const { return _cpuState; }
    virtual const std::vector<std::string>& registerNames() const = 0;
    virtual size_t numRegisters() const = 0;
    virtual RegisterValue registerbyIndex(size_t index) const = 0;
    virtual void fetchAllRegisters(RegisterPack& registers) const
    {
        registers.resize(numRegisters());
        for(size_t i = 0; i < numRegisters(); ++i) {
            registers[i] = registerbyIndex(i);
        }
    }
    virtual void setRegister(size_t index, uint32_t value) = 0;
    virtual uint32_t getSP() const = 0;
    virtual uint32_t getPC() const = 0;
    virtual int64_t cycles() const = 0;
    virtual const ClockedTime& time() const = 0;
    virtual uint8_t readMemoryByte(uint32_t addr) const = 0;
    virtual unsigned stackSize() const = 0;
    virtual StackContent stack() const = 0;
    uint32_t stackElement(size_t index) const
    {
        auto s = stack();
        auto offset = s.stackDirection == emu::GenericCpu::eUPWARDS ? index * s.entrySize : s.content.size() - (index + 1) * s.entrySize;
        switch(s.entrySize) {
            case 1: return readStackEntry<uint8_t>(&s.content[offset], s.endianness);
            case 2: return readStackEntry<uint16_t>(&s.content[offset], s.endianness);
            case 4: return readStackEntry<uint32_t>(&s.content[offset], s.endianness);
            default: return 0;
        }
    }

    virtual size_t disassemblyPrefixSize() const = 0;
    virtual std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const = 0;

    virtual void setBreakpoint(uint32_t address, BreakpointInfo&& bpi)
    {
        _breakpoints[address] = std::move(bpi);
        _breakMap[address & 0xFFF] = 1;
    }
    virtual void removeBreakpoint(uint32_t address)
    {
        _breakpoints.erase(address);
        const uint32_t masked = address & 0xFFF;
        for(const auto& [addr, bpi] : _breakpoints) {
            if((addr & 0xFFF) == masked) {
                _breakMap[masked] = 1;
                return;
            }
        }
        _breakMap[masked] = 0;
    }
    virtual BreakpointInfo* findBreakpoint(uint32_t address)
    {
        if(_breakMap[address & 0xFFF]) {
            auto iter = _breakpoints.find(address);
            if(iter != _breakpoints.end())
                return &iter->second;
        }
        return nullptr;
    }
    virtual size_t numBreakpoints() const {
        return _breakpoints.size();
    }
    virtual std::pair<uint32_t, BreakpointInfo*> nthBreakpoint(size_t index)
    {
        size_t count = 0;
        for(auto& [addr, bpi] : _breakpoints) {
            if(count++ == index)
                return {addr, &bpi};
        }
        return {0, nullptr};
    }
    virtual void removeAllBreakpoints()
    {
        std::memset(_breakMap.data(), 0, 4096);
        _breakpoints.clear();
    }
    virtual bool hasBreakPoint(uint32_t address) const
    {
        return _breakMap[address&0xfff] != 0;
    }
    RegisterValue registerByName(const std::string& name) const
    {
        static const auto& regNames = registerNames();
        if (const auto iter = std::ranges::find(regNames, name); iter != regNames.end()) {
            return registerbyIndex(iter - regNames.begin());
        }
        return {0,0};
    }
    virtual bool isBreakpointTriggered() { auto result = _breakpointTriggered; _breakpointTriggered = false; return result; }
protected:
    template<typename T>
    T readStackEntry(const uint8_t* address, Endianness endianness) const
    {
        switch(endianness) {
            case eNATIVE:
                return *reinterpret_cast<const T*>(address);
            case eBIG: {
                T result = 0;
                for (int i = 0; i < sizeof(T); ++i) {
                    result |= static_cast<T>(address[i]) << (8 * (sizeof(T) - i - 1));
                }
                return result;
            }
            case eLITTLE: {
                T result = 0;
                for (int i = 0; i < sizeof(T); ++i) {
                    result |= static_cast<T>(address[i]) << (8 * i);
                }
                return result;
            }
        }
        return 0;
    }
    mutable ExecMode _execMode{ePAUSED};
    CpuState _cpuState{eNORMAL};
    uint32_t _stepOverSP{};
    std::array<uint8_t,4096> _breakMap{};
    std::map<uint32_t,BreakpointInfo> _breakpoints{};
    std::string _errorMessage{};
    bool _breakpointTriggered{false};
    Expressionist _expressionist;
};

}