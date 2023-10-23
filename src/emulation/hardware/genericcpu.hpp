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
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <variant>
#include <emulation/time.hpp>

namespace emu
{

class GenericCpu
{
public:
    enum ExecMode { ePAUSED, eRUNNING, eSTEP, eSTEPOVER, eSTEPOUT };
    struct BreakpointInfo {
        enum Type { eTRANSIENT, eCODED };
        std::string label;
        Type type{eTRANSIENT};
        bool isEnabled{true};
    };
    struct RegisterValue {
        uint32_t value{};
        uint32_t size{};
    };
    using RegisterPack = std::vector<RegisterValue>;
    virtual ~GenericCpu() = default;

    virtual int64_t executeFor(int64_t microseconds) = 0;
    virtual ExecMode getExecMode() const { return _execMode; }
    virtual void setExecMode(ExecMode mode) { _execMode = mode; if(mode == eSTEPOVER) _stepOverSP = getSP(); }
    virtual bool inErrorState() const = 0;

    virtual uint32_t getCpuID() const = 0;
    virtual const std::string& getName() const = 0;
    virtual const std::vector<std::string>& getRegisterNames() const = 0;
    virtual size_t getNumRegisters() const = 0;
    virtual RegisterValue getRegister(size_t index) const = 0;
    virtual void fetchAllRegisters(RegisterPack& registers) const
    {
        registers.resize(getNumRegisters());
        for(size_t i = 0; i < getNumRegisters(); ++i) {
            registers[i] = getRegister(i);
        }
    }
    virtual void setRegister(size_t index, uint32_t value) = 0;
    virtual uint32_t getSP() const = 0;
    virtual uint32_t getPC() const = 0;
    virtual int64_t getCycles() const = 0;
    virtual const ClockedTime& getTime() const = 0;
    virtual uint8_t getMemoryByte(uint32_t addr) const = 0;

    virtual std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const = 0;

    virtual void setBreakpoint(uint32_t address, const BreakpointInfo& bpi)
    {
        _breakpoints[address] = bpi;
        _breakMap[address & 0xFFF] = 1;
    }
    virtual void removeBreakpoint(uint32_t address)
    {
        _breakpoints.erase(address);
        size_t count = 0;
        uint32_t masked = address & 0xFFF;
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
    virtual std::pair<uint32_t, BreakpointInfo*> getNthBreakpoint(size_t index)
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
    RegisterValue getRegisterByName(const std::string& name) const
    {
        static const auto& regNames = getRegisterNames();
        auto iter = std::find(regNames.begin(), regNames.end(), name);
        if(iter != regNames.end()) {
            return getRegister(iter - regNames.begin());
        }
        return {0,0};
    }
protected:
    mutable ExecMode _execMode{eRUNNING};
    uint32_t _stepOverSP{};
    std::array<uint8_t,4096> _breakMap;
    std::map<uint32_t,BreakpointInfo> _breakpoints;
};

}