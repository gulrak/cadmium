//---------------------------------------------------------------------------------------
// src/emulation/m6800.hpp
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
// Notes: * While this emulator core is written with Cadmium as the using
//          application in mind, care was taken to keep the dependencies
//          to a minimum. Actually besides a decent C++17 compiler, and
//          std::format or fmtlib fmt::format nothing is needed to use this
//          core, and I try to keep it clean, to allow reuse.
//
//        * The use of template parameter for the elementary data types
//          of the CPU is based on the reuse of this code with special
//          integer/bitfield types to support speculative execution for
//          use in the heuristic disassembler that is also been worked on,
//          and a price for not implementing two cores.
//
//        * The core emulates all real and passive (VMA=0) bus cycles of
//          opcodes, thus accessing memory more often than needed for
//          minimal functionality. These accesses are done at cycle counts
//          that would match a real CPU, to allow external hardware to
//          emulate the matching time difference where needed.
//---------------------------------------------------------------------------------------
#pragma once

#define CADMIUM_M6800_CORE_VERSION 0x1001

#ifdef CADMIUM_WITH_GENERIC_CPU
#include <emulation/hardware/genericcpu.hpp>
#define M6800_WITH_TIME
#endif

#include <cstdint>
#include <string>

#ifdef USE_STD_FORMAT
#include <format>
namespace emu {
using std::format;
}
#elif __has_include(<fmt/format.h>)
#include <fmt/format.h>
namespace emu {
using fmt::format;
}
#else
#error "Need __has_include support and either <format> or <fmt/format.h> in include path!"
#endif

#ifndef M6800_STATE_BUS_ONLY

#ifdef M6800_SPECULATIVE_SUPPORT
    #include <emulation/heuristicint.hpp>
#endif
#ifdef M6800_WITH_TIME
    #include <emulation/time.hpp>
#endif


namespace emu {

#ifndef M6800_SPECULATIVE_SUPPORT
// clang-format off
// Fallback implementation of Bitfield when speculative support is not available
template<typename ValueType = uint8_t>
class Bitfield {
public:
    Bitfield() = delete;
    Bitfield(std::string names) : _names(std::move(names)) {}
    Bitfield(std::string names, ValueType positions, ValueType values) : _names(std::move(names)) { setFromVal(positions, values); }
    void setFromVal(ValueType positions, ValueType values) { _val = (_val & ~positions) | (values & positions); }
    void setFromBool(ValueType positions, bool asOnes) { if(asOnes) set(positions); else clear(positions); }
    void set(ValueType positions) { _val |= positions; }
    void clear(ValueType positions) { _val &= ~positions; }
    bool isValue(ValueType positions, ValueType values) const { return (_val & positions) == (values & positions); }
    bool isSet(ValueType positions) const { return (_val & positions) == positions; }
    bool isUnset(ValueType positions) const { return (_val & positions) == 0; }
    bool isValid(ValueType positions) const { return true; }
    ValueType asNumber() const { return _val; }
    ValueType validity() const { return ~static_cast<ValueType>(0); }
    std::string asString() const {
        std::string result;
        result.reserve(_names.size());
        ValueType bit = 0x80;
        for(auto c : _names) {
            if(_val & bit) { result.push_back(std::toupper(c)); } else { result.push_back('-'); }
            bit >>= 1;
        }
        return result;
    }
private:
    std::string _names;
    ValueType _val{};
};
using flags8_t = Bitfield<uint8_t>;
template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
constexpr bool isValidInt(const T& t) { return true; }
template<typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
constexpr T asNativeInt(const T& t) { return t; }
// clang-format on
#endif

}

#endif // M6800_STATE_BUS_ONLY

namespace emu {

template<typename byte_t = uint8_t, typename word_t = uint16_t>
class M6800Bus
{
public:
    using ByteType = byte_t;
    using WordType = word_t;
    virtual ~M6800Bus() = default;
    virtual byte_t readByte(word_t addr) const = 0;
    virtual void dummyRead(word_t addr) const {}
    virtual byte_t readDebugByte(word_t addr) const { return readByte(addr); }
    virtual void writeByte(word_t addr, byte_t val) = 0;
};

struct M6800State
{
    uint8_t a{};
    uint8_t b{};
    uint16_t ix{};
    uint16_t pc{};
    uint16_t sp{};
    uint8_t cc{};
    int64_t cycles{};
    int64_t instruction{};
    std::string toString(bool shortCycles = false) const
    {
        auto flags = emu::format("{:c}{:c}{:c}{:c}{:c}{:c}", ((cc & 32) ? 'H' : '-'), ((cc & 16) ? 'I' : '-'), ((cc & 8) ? 'N' : '-'), ((cc & 4) ? 'Z' : '-'),
                                 ((cc & 2) ? 'V' : '-'), ((cc & 1) ? 'C' : '-'));
        if(shortCycles)
            return emu::format("[{:02}/{:02}] A:{:02X} B:{:02X} X:{:04X} SP:{:04X} PC:{:04X} {}", cycles, instruction, a, b, ix, sp, pc, flags);
        return emu::format("[{:08}/{:07}] A:{:02X} B:{:02X} X:{:04X} SP:{:04X} PC:{:04X} {}", cycles, instruction, a, b, ix, sp, pc, flags);
    }
};

inline bool operator==(const M6800State& s1, const M6800State& s2)
{
    return s1.pc == s2.pc && s1.cycles == s2.cycles && s1.a == s2.a && s1.b == s2.b && s1.ix == s2.ix && s1.cc == s2.cc && s1.instruction == s2.instruction;
}

#ifndef M6800_STATE_BUS_ONLY

template<typename byte_t = uint8_t, typename word_t = uint16_t, typename long_t = uint32_t, typename ccflags_t = flags8_t>
#ifdef CADMIUM_WITH_GENERIC_CPU
#define GENERIC_OVERRIDE override
class M6800 : public GenericCpu
#else
#define GENERIC_OVERRIDE
class M6800
#endif
{
public:
    using Bus = M6800Bus<byte_t, word_t>;
    using ByteType = byte_t;
    using WordType = word_t;
    using LongType = long_t;
    using FlagsType = ccflags_t;
    enum Flags { C = 1, V = 2, Z = 4, N = 8, I = 16, H = 32 };
    enum CpuState { eNORMAL, eWAIT, eHALT, eERROR };
    struct Disassembled {
        int size;
        std::string text;
    };
    struct State
    {
        ByteType a{};
        ByteType b{};
        WordType ix{};
        WordType pc{};
        WordType sp{};
        FlagsType cc{};
        int64_t cycles{};
        int64_t instruction{};
        std::string toString(bool shortCycles = false) const
        {
            auto flags = emu::format("{:c}{:c}{:c}{:c}{:c}{:c}", ((cc & 32) ? 'H' : '-'), ((cc & 16) ? 'I' : '-'), ((cc & 8) ? 'N' : '-'), ((cc & 4) ? 'Z' : '-'),
                                     ((cc & 2) ? 'V' : '-'), ((cc & 1) ? 'C' : '-'));
            if(shortCycles)
                return emu::format("[{:02}/{:02}] A:{:02X} B:{:02X} X:{:04X} SP:{:04X} PC:{:04X} {}", cycles, instruction, a, b, ix, sp, pc, flags);
            return emu::format("[{:08}/{:07}] A:{:02X} B:{:02X} X:{:04X} SP:{:04X} PC:{:04X} {}", cycles, instruction, a, b, ix, sp, pc, flags);
        }
        inline bool operator==(const State& rhs)
        {
            return pc == rhs.pc && cycles == rhs.cycles && a == rhs.a && b == rhs.b && ix == rhs.ix && cc == rhs.cc && instruction == rhs.instruction;
        }
    };

#ifdef M6800_WITH_TIME
    M6800(M6800Bus<byte_t, word_t>& bus, Time::ticks_t clockSpeed = 1000000)
        : _bus(bus)
        , _clockSpeed(clockSpeed)
        , _systemTime(clockSpeed)
#else
    M6800(M6800Bus<byte_t, word_t>& bus)
        : _bus(bus)
#endif
    {
        M6800::reset();
    }

    void reset() override
    {
        _rA = 0;
        _rB = 0;
        _rIX = 0;
        _rSP = 0;
        _rCC.setFromVal(0xFF, 0xC0);
        _rCC.set(I);
        auto t = readByte(0xFFFE);
        _rPC = (t << 8) | readByte(0xFFFF);
        _cycles = 0;
        _instructions = 0;
        _cpuState = eNORMAL;
#ifdef M6800_WITH_TIME
        _systemTime.reset();
#endif
    }

    void irq()
    {
        _irq = true;
    }

    void nmi()
    {
        _nmi = true;
    }

    void halt(bool state)
    {
        _halt = state;
        _cpuState = _halt ? eHALT : eNORMAL;
    }

#ifdef M6800_WITH_TIME
    const ClockedTime& time() const override
    {
        return _systemTime;
    }
#endif

    int64_t cycles() const GENERIC_OVERRIDE
    {
        return _cycles;
    }

    CpuState getCpuState() const
    {
        return _cpuState;
    }

    uint32_t getPC() const GENERIC_OVERRIDE
    {
        return _rPC;
    }

    uint32_t getSP() const GENERIC_OVERRIDE
    {
        return _rSP;
    }

    void getState(M6800State& state) const
    {
        state.a = asNativeInt(_rA);
        state.b = asNativeInt(_rB);
        state.ix = asNativeInt(_rIX);
        state.sp = asNativeInt(_rSP);
        state.pc = asNativeInt(_rPC);
        state.cc = _rCC.asNumber();
        state.cycles = _cycles;
        state.instruction = _instructions;
    }

    void getState(State& state) const
    {
        state.a = _rA;
        state.b = _rB;
        state.ix = _rIX;
        state.sp = _rSP;
        state.pc = _rPC;
        state.cc = _rCC;
        state.cycles = _cycles;
        state.instruction = _instructions;
    }

    void setState(const M6800State& state)
    {
        _rA = state.a;
        _rB = state.b;
        _rIX = state.ix;
        _rSP = state.sp;
        _rPC = state.pc;
        _rCC.setFromVal(H|I|N|Z|V|C, state.cc);
        _cycles = state.cycles;
        _instructions = state.instruction;
    }

    void setState(const State& state)
    {
        _rA = state.a;
        _rB = state.b;
        _rIX = state.ix;
        _rSP = state.sp;
        _rPC = state.pc;
        _rCC = state.cc;
        _cycles = state.cycles;
        _instructions = state.instruction;
    }

    byte_t readByte(word_t addr) { auto val = _bus.readByte(addr); addCycles(1); return val; }
    word_t readWord(word_t addr) { auto t = readByte(addr); return (t << 8) | readByte(addr + 1); }
    void writeByte(word_t addr, byte_t val) { _bus.writeByte(addr, val); addCycles(1); }
    void writeWord(word_t addr, word_t val) { writeByte(addr, val>>8); writeByte(addr + 1, val & 0xff); }
    void dummyReadByte(word_t addr) { _bus.dummyRead(addr); addCycles(1); }
    void dummyReadWord(word_t addr) { addCycles(2); }
    void dummyWriteByte(word_t addr, word_t val) { addCycles(1); }
    void addCycles(int cycles)
    {
        _cycles += cycles;
#ifdef M6800_WITH_TIME
        _systemTime.addCycles(cycles);
#endif
    }

#ifdef CADMIUM_WITH_GENERIC_CPU
    std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const override
#else
    std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const
#endif
    {
        decltype(_rPC) addr{};
        if(pc < 0)
            addr = _rPC;
        else
            addr = pc;
        if(isValidInt(addr)) {
            auto address = asNativeInt(addr);
            byte_t data[3];
            data[0] = _bus.readDebugByte(address);
            data[1] = _bus.readDebugByte(address + 1);
            data[2] = _bus.readDebugByte(address + 2);
            auto [size, text] = disassembleInstruction(data, data + 3, address);
            if(bytes) *bytes = size;
            switch (size) {
                case 2:
                    return emu::format("{:04X}: {:02X} {:02X}     {}", address, data[0], data[1], text);
                case 3:
                    return emu::format("{:04X}: {:02X} {:02X} {:02X}  {}", address, data[0], data[1], data[2], text);
                default:
                    return emu::format("{:04X}: {:02X}        {}", address, data[0], text);
            }
        }
        else {
            if(bytes) *bytes = 1;
            return "????:  ???";
        }
    }

#ifdef M6800_SPECULATIVE_SUPPORT
    template<typename T, typename = typename std::enable_if<std::is_same<T,uint8_t>::value||std::is_same<T,h_uint8_t>::value, T>::type>
    static Disassembled disassembleInstruction(const T* code, const T* end, word_t addr)
#else
    static Disassembled disassembleInstruction(const uint8_t* code, const uint8_t* end, word_t addr)
#endif
    {
        auto opcode = *code++;
        if(!isValidInt(opcode)) {
            return {1, "???"};
        }
        const auto& info = _opcodes[asNativeInt(opcode)];
        char accuSym = ' ';
        if(info.addrMode & ACCUA)
            accuSym = 'A';
        else if(info.addrMode & ACCUB)
            accuSym = 'B';
        switch(info.addrMode & 7) {
            case INHERENT:
                return {1, emu::format("{}{}", info.mnemonic, accuSym)};
            case IMMEDIATE:
                return {2, emu::format("{}{} #${:02X}", info.mnemonic, accuSym, *code)};
            case IMMEDIATE16:
                return {3, emu::format("{}{} #${:04X}", info.mnemonic, accuSym, (*code << 8) | *(code + 1))};
            case DIRECT:
                return {2, emu::format("{}{} ${:02X}", info.mnemonic, accuSym, *code)};
            case EXTENDED:
                return {3, emu::format("{}{} ${:04X}", info.mnemonic, accuSym, (*code << 8) | *(code + 1))};
            case RELATIVE:
                if(isValidInt(*code)) {
                    return {2, emu::format("{}  ${:02X}", info.mnemonic, (addr + 2 + int8_t(asNativeInt(*code))) & 0xFFFF)};
                }
                return {2, emu::format("{}  ???", info.mnemonic)};
            case INDEXED:
                return {2, emu::format("{}{} ${:02X},X", info.mnemonic, accuSym, *code)};
            default:
                return {info.bytes, "???"};
        }
    }

    std::string dumpStateLine()
    {
        std::string eaStr;
        return emu::format("[{:08}/{:07}] {:<28} A:{:02X} B:{:02X} X:{:04X} SP:{:04X} {} {}", _cycles, _instructions, disassembleInstructionWithBytes(), _rA, _rB, _rIX, _rSP, _rCC.asString().substr(2), eaStr);
    }
    std::string dumpRegisterState()
    {
        return emu::format("A:{:02X} B:{:02X} X:{:04X} SP:{:04X} PC:{:04X} SR:{}", _rA, _rB, _rIX, _rSP, _rPC, _rCC.asString().substr(2));
    }

#ifdef CADMIUM_WITH_GENERIC_CPU
    int64_t executeFor(int64_t microseconds) override
    {
        if(_execMode != GenericCpu::ePAUSED) {
            auto startTime = _systemTime;
            auto endTime = _systemTime + Time::fromMicroseconds(microseconds);
            while (_execMode != GenericCpu::ePAUSED && _systemTime < endTime) {
                executeInstruction();
            }
            return startTime.excessTime_us(_systemTime, microseconds);
        }
        return 0;
    }
#endif

    int executeInstruction() override
    {
#ifdef CADMIUM_WITH_GENERIC_CPU
        if(_execMode == ePAUSED || _cpuState == eERROR)
            return 0;
#else
        if(_cpuState == eERROR)
            return;
#endif
        if(_halt) {
            handleHALT();
            return 1;
        }
        const auto startCycles = _cycles;
        if(_nmi)
            handleNMI();
        else if(_irq && _rCC.isUnset(I))
            handleIRQ();
        if(_cpuState == eNORMAL) {
            _opcode = readByte(_rPC++);
            _info = &_opcodes[_opcode];
            (this->*_info->handler)();
            ++_instructions;
        }
#ifdef CADMIUM_WITH_GENERIC_CPU
        if (_execMode == eSTEP || (_execMode == eSTEPOVER && _rSP >= _stepOverSP)) {
            _execMode = ePAUSED;
        }
        if(hasBreakPoint(getPC())) {
            if(findBreakpoint(getPC())) {
                _execMode = ePAUSED;
                _breakpointTriggered = true;
            }
        }
#endif
        return static_cast<int>(_cycles - startCycles);
    }

    std::string executeInstructionTraced()
    {
        M6800State state;
        std::string eaStr;
        auto dis = disassembleInstructionWithBytes();
        getState(state);
        executeInstruction();
        return emu::format("[{:08}/{:07}] {:<28} A:{:02X} B:{:02X} X:{:04X} SP:{:04X} {} {}", state.cycles, state.instruction, dis, _rA, _rB, _rIX, _rSP, _rCC.asString().substr(2), eaStr);
    }

    static bool isValidOpcode(uint8_t opcode)
    {
        return (_opcodes[opcode].addrMode & 7) != INVALID && !(_opcodes[opcode].type & UNDOC) && opcode != 0x3E;
    }

#ifdef CADMIUM_WITH_GENERIC_CPU
    ~M6800() override = default;
    uint8_t readMemoryByte(uint32_t addr) const override
    {
        return _bus.readDebugByte(addr);
    }
    unsigned stackSize() const override { return 0; }
    StackContent stack() const override
    {
        return {};
    }
    bool inErrorState() const override { return _cpuState == eERROR; }
    uint32_t cpuID() const override { return 6800; }
    std::string name() const override { static const std::string name = "M6800"; return name; }
    const std::vector<std::string>& registerNames() const override
    {
        static const std::vector<std::string> registerNames = {
            "A", "B", "IX", "SP", "PC", "SR"
        };
        return registerNames;
    }
    size_t numRegisters() const override { return 6; }
    RegisterValue registerbyIndex(size_t index) const override
    {
        switch(index) {
            case 0: return {_rA, 8};
            case 1: return {_rB, 8};
            case 2: return {_rIX, 16};
            case 3: return {_rSP, 16};
            case 4: return {_rPC, 16};
            case 5: return {_rCC.asNumber(), 8};
            default: return {0,0};
        }
    }
    void setRegister(size_t index, uint32_t value) override
    {
        switch(index) {
            case 0: _rA = static_cast<uint8_t>(value); break;
            case 1: _rB = static_cast<uint8_t>(value); break;
            case 2: _rIX = static_cast<uint16_t>(value); break;
            case 3: _rSP = static_cast<uint16_t>(value); break;
            case 4: _rPC = static_cast<uint16_t>(value); break;
            case 5: _rCC.setFromVal(H|I|N|Z|V|C, value); break;
            default: break;
        }

    }
#endif


protected:
    enum AddressingMode { INVALID, INHERENT, IMMEDIATE, IMMEDIATE16, DIRECT, EXTENDED, RELATIVE, INDEXED, ACCUA = 8, ACCUB = 16, UNDOC = 32};
    enum InstructionType { NORMAL, READ, WRITE, STACK, JUMP, CCJUMP, CALL, CCCALL, RETURN, HALT};
    using OpcodeHandler = void (M6800::*)();
    struct OpcodeInfo {
        uint8_t bytes;
        uint8_t cycles;
        int addrMode;
        int type;
        OpcodeHandler handler;
        char mnemonic[4];
    };
    static const OpcodeInfo* opcodeIntoTable()
    {
        return _opcodes;
    }

private:
    void pushByte(byte_t data)
    {
        writeByte(_rSP--, data);
    }

    byte_t pullByte()
    {
        return readByte(++_rSP);
    }

    void pushWord(word_t data)
    {
        pushByte(data & 0xFF);
        pushByte(data >> 8);
    }

    word_t pullWord()
    {
        word_t w = pullByte();
        w = (w << 8) + pullByte();
        return w;
    }

    void ccSetC(word_t val) { _rCC.setFromBool(C, val & 0x100); }
    void ccSetC(long_t val) { _rCC.setFromBool(C, val & 0x10000); }
    void ccSetV(byte_t v1, byte_t v2, word_t res) { _rCC.setFromBool(V, (v1 ^ v2 ^ res ^ (res >> 1)) & 0x80); }
    void ccSetV(word_t v1, word_t v2, long_t res) { _rCC.setFromBool(V, (v1 ^ v2 ^ res ^ (res >> 1)) & 0x8000); }
    void ccSetZ(byte_t val) { _rCC.setFromBool(Z, val == 0); }
    void ccSetZ(word_t val) { _rCC.setFromBool(Z, val == 0); }
    void ccSetN(byte_t val) { _rCC.setFromBool(N, val & 0x80); }
    void ccSetN(word_t val) { _rCC.setFromBool(N, val & 0x8000); }
    void ccSetNZ(byte_t val) { ccSetZ(val); ccSetN(val); }
    void ccSetNZ(word_t val) { ccSetZ(val); ccSetN(val); }
    void ccSetNZv(byte_t val) { _rCC.clear(V); ccSetZ(val); ccSetN(val); }
    void ccSetNZv(word_t val) { _rCC.clear(V); ccSetZ(val); ccSetN(val); }
    void ccSetH(byte_t v1, byte_t v2, word_t res) { _rCC.setFromBool(H, (((res) ^ (v1) ^ (v2)) & 0x10)); }
    //void ccSetH(byte_t v1, byte_t v2, word_t res) { _rCC.setFromBool(H, (v1 ^ v2 ^ res) & 0x10); }
    //((((f) ^ (a) ^ (b)) >> 4) & 1)
    void ccSetFlagsCNZV(byte_t v1, byte_t v2, word_t res) { auto r8 = static_cast<byte_t>(res); ccSetC(res); ccSetN(r8); ccSetZ(r8); ccSetV(v1, v2, res); }
    void ccSetFlagsCNZV(word_t v1, word_t v2, long_t res) { auto r16 = static_cast<word_t>(res); ccSetC(res); ccSetN(r16); ccSetZ(r16); ccSetV(v1, v2, res); }

    word_t getEA(int mode)
    {
        uint8_t t;
        switch(mode & 7) {
            case IMMEDIATE:
                _rPC++;
                return _rPC - 1;
            case IMMEDIATE16:
                _rPC += 2;
                return _rPC - 2;
            case DIRECT:
                return readByte(_rPC++);
            case EXTENDED:
                t = readByte(_rPC++);
                return (t<<8) | readByte(_rPC++);
            case RELATIVE:
                t = readByte(_rPC++);
                return isValidInt(t) ? _rPC + (int8_t)asNativeInt(t) : word_t();
            case INDEXED:
                t = readByte(_rPC++);
                _rIXwoc = (_rIX & 0xFF00) | (static_cast<byte_t>(_rIX) + t);
                return _rIX + t;
            default:
                return 0;
        }
    }


    void handleIRQ()
    {
        if(_cpuState == eWAIT)
            _cpuState = eNORMAL;
        pushWord(_rPC);
        pushWord(_rIX);
        pushByte(_rA);
        pushByte(_rB);
        pushByte(_rCC.isValid(0xFF) ? _rCC.asNumber() : byte_t());
        _rCC.set(I);
        dummyReadByte(_rSP);
        _rPC = readWord(0xFFF8);
        _irq = false;
    }
    void handleNMI()
    {
        if(_cpuState == eWAIT)
            _cpuState = eNORMAL;
        pushWord(_rPC);
        pushWord(_rIX);
        pushByte(_rA);
        pushByte(_rB);
        pushByte(_rCC.isValid(0xFF) ? _rCC.asNumber() : byte_t());
        _rCC.set(I);
        dummyReadByte(_rSP);
        _rPC = readWord(0xFFFC);
        _nmi = false;
    }
    void handleHALT()
    {
        // TODO: Check if HALT really has no bus activity
        addCycles(1);
    }

    void opINVALID()
    {
        // TODO: Currently NOP, but shouldn't
    }
    void opABA()
    {
        word_t sum = _rA + _rB;
        readByte(_rPC);
        ccSetH(_rA, _rB, sum);
        ccSetFlagsCNZV(_rA, _rB, sum);
        _rA = sum;
    }
    void opADC()
    {
        auto ea = getEA(_info->addrMode);
        auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        auto val = readByte(ea);
        word_t sum = accu + val + (_rCC.isSet(C) ? 1 : 0);
        ccSetH(accu, val, sum);
        ccSetFlagsCNZV(accu, val, sum);
        accu = sum;
    }
    void opADD()
    {
        auto ea = getEA(_info->addrMode);
        auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        auto val = readByte(ea);
        word_t sum = accu + val;
        ccSetH(accu, val, sum);
        ccSetFlagsCNZV(accu, val, sum);
        accu = sum;
    }
    void opAND()
    {
        auto ea = getEA(_info->addrMode);
        auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        auto val = readByte(ea);
        accu &= val;
        ccSetNZv(accu);
    }
    void opASL()
    {
        if((_info->addrMode & 7) == INHERENT) {
            auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
            _rCC.setFromBool(C, accu & 0x80);
            accu <<= 1;
            ccSetNZ(accu);
            _rCC.setFromBool(V, _rCC.isValue(N|C, N) || _rCC.isValue(N|C, C));
            readByte(_rPC);
        }
        else {
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            auto val = readByte(ea);
            dummyReadByte(ea);
            _rCC.setFromBool(C, val & 0x80);
            val <<= 1;
            ccSetNZ(val);
            _rCC.setFromBool(V, _rCC.isValue(N|C, N) || _rCC.isValue(N|C, C));
            writeByte(ea, val);
        }
    }
    void opASR()
    {
        if((_info->addrMode & 7) == INHERENT) {
            auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
            _rCC.setFromBool(C, accu & 1);
            accu = (accu >> 1) | (accu & 0x80);
            ccSetNZ(accu);
            _rCC.setFromBool(V, _rCC.isValue(N|C, N) || _rCC.isValue(N|C, C));
            readByte(_rPC);
        }
        else {
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            auto val = readByte(ea);
            dummyReadByte(ea);
            _rCC.setFromBool(C, val & 1);
            val = (val >> 1) | (val & 0x80);
            ccSetNZ(val);
            _rCC.setFromBool(V, _rCC.isValue(N|C, N) || _rCC.isValue(N|C, C));
            writeByte(ea, val);
        }
    }
    void opBCC()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isUnset(C))
            _rPC = ea;
    }
    void opBCS()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isSet(C))
            _rPC = ea;
    }
    void opBEQ()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isSet(Z))
            _rPC = ea;
    }
    void opBGE()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isValue(N|V,N|V) || _rCC.isValue(N|V,0))
            _rPC = ea;
    }
    void opBGT()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isUnset(Z) && (_rCC.isValue(N|V,N|V) || _rCC.isValue(N|V,0)))
            _rPC = ea;
    }
    void opBHI()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isUnset(C|Z))
            _rPC = ea;
    }
    void opBIT()
    {
        auto ea = getEA(_info->addrMode);
        auto accu = _info->addrMode & ACCUA ? _rA : _rB;
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        auto val = readByte(ea);
        accu &= val;
        ccSetNZv(accu);
    }
    void opBLE()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isSet(Z) || _rCC.isValue(N|V,N) || _rCC.isValue(N|V,V))
            _rPC = ea;
    }
    void opBLS()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isSet(C) || _rCC.isSet(Z))
            _rPC = ea;
    }
    void opBLT()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isValue(N|V,N) || _rCC.isValue(N|V,V))
            _rPC = ea;
    }
    void opBMI()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isSet(N))
            _rPC = ea;
    }
    void opBNE()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isUnset(Z))
            _rPC = ea;
    }
    void opBPL()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isUnset(N))
            _rPC = ea;
    }
    void opBRA()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        _rPC = ea;
    }
    void opBSR()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        pushWord(_rPC);
        dummyReadByte(_rSP);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        _rPC = ea;
    }
    void opBVC()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isUnset(V))
            _rPC = ea;
    }
    void opBVS()
    {
        auto ea = getEA(RELATIVE);
        dummyReadByte(_rPC);
        dummyReadByte(ea);
        if(_rCC.isSet(V))
            _rPC = ea;
    }
    void opCBA()
    {
        readByte(_rPC);
        word_t res = _rA - _rB;
        ccSetFlagsCNZV(_rA, _rB, res);
    }
    void opCLC()
    {
        _rCC.clear(C);
        readByte(_rPC);
    }
    void opCLI()
    {
        _rCC.clear(I);
        readByte(_rPC);
    }
    void opCLR()
    {
        if((_info->addrMode & 7) == INHERENT) {
            auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
            readByte(_rPC);
            accu = 0;
        }
        else{
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            readByte(ea);
            dummyReadByte(ea);
            writeByte(ea, 0);
        }
        _rCC.setFromVal(N|Z|C|V,Z);
    }
    void opCLV()
    {
        _rCC.clear(V);
        readByte(_rPC);
    }
    void opCMP()
    {
        auto accu = _info->addrMode & ACCUA ? _rA : _rB;
        auto ea = getEA(_info->addrMode);
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        auto val = readByte(ea);
        word_t res = accu - val;
        ccSetFlagsCNZV(accu, val, res);
    }
    void opCOM()
    {
        if((_info->addrMode & 7) == INHERENT) {
            auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
            auto old = accu;
            accu = 0xFF - accu;
            ccSetNZ(accu);
            _rCC.setFromVal(C|V, C);
            dummyReadByte(_rPC);
        }
        else {
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            auto val = readByte(ea);
            dummyReadByte(ea);
            auto old = val;
            val = 0xFF -val;
            ccSetNZ(val);
            _rCC.setFromVal(C|V, C);
            writeByte(ea, val);
        }
    }
    void opCPX()
    {
        auto ea = getEA(_info->addrMode);
        if(_info->addrMode == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        auto val = readWord(ea);
        long_t res = _rIX - val;
        auto resw = static_cast<word_t>(res);
        ccSetN(resw);
        ccSetZ(resw);
        _rCC.setFromBool(V, ((_rIX & 0x8000) && !(val & 0x8000) && !(resw & 0x8000)) || (!(_rIX & 0x8000) && (val & 0x8000) && (resw & 0x8000)));
    }
    void opDAA()
    {
        readByte(_rPC);
        auto low = _rA & 0xF;
        auto high = _rA & 0xF0;
        if (low >= 0x0A || _rCC.isSet(H)) {
            _rA += 0x06;
        }
        if (high >= 0xA0 || _rCC.isSet(C) || (high == 0x90 && low >= 0x0A)) {
            _rA += 0x60;
            _rCC.set(C);
        }
        ccSetNZ(_rA);
    }
    void opDEC()
    {
        if((_info->addrMode & 7) == INHERENT) {
            auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
            auto old = accu--;
            ccSetNZ(accu);
            _rCC.setFromBool(V, old == 0x80);
            readByte(_rPC);
        }
        else {
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            auto val = readByte(ea);
            dummyReadByte(ea);
            auto old = val--;
            ccSetNZ(val);
            _rCC.setFromBool(V, old == 0x80);
            writeByte(ea, val);
        }
    }
    void opDES()
    {
        readByte(_rPC);
        dummyReadByte(_rSP);
        --_rSP;
        dummyReadByte(_rSP);
    }
    void opDEX()
    {
        readByte(_rPC);
        dummyReadByte(_rIX);
        --_rIX;
        ccSetZ(_rIX);
        dummyReadByte(_rIX);
    }
    void opEOR()
    {
        auto ea = getEA(_info->addrMode);
        auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        auto val = readByte(ea);
        accu ^= val;
        ccSetNZv(accu);
    }
    void opINC()
    {
        if((_info->addrMode & 7) == INHERENT) {
            auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
            auto old = accu++;
            ccSetNZ(accu);
            _rCC.setFromBool(V, old == 0x7F);
            readByte(_rPC);
        }
        else {
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            auto val = readByte(ea);
            dummyReadByte(ea);
            auto old = val++;
            ccSetNZ(val);
            _rCC.setFromBool(V, old == 0x7F);
            writeByte(ea, val);
        }
    }
    void opINS()
    {
        readByte(_rPC);
        dummyReadByte(_rSP);
        ++_rSP;
        dummyReadByte(_rSP);
    }
    void opINX()
    {
        readByte(_rPC);
        dummyReadByte(_rIX);
        ++_rIX;
        ccSetZ(_rIX);
        dummyReadByte(_rIX);
    }
    void opJMP()
    {
        auto ea = getEA(_info->addrMode);
        if(_info->addrMode == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        _rPC = ea;
    }
    void opJSR()
    {
        auto ea = getEA(_info->addrMode);
        if(_info->addrMode == EXTENDED) {
            readByte(ea);
            pushWord(_rPC);
            dummyReadByte(_rSP);
            dummyReadByte(_rPC - 1);
            readByte(_rPC - 1);
        }
        else {
            dummyReadByte(_rIX);
            pushWord(_rPC);
            dummyReadByte(_rSP);
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        _rPC = ea;
    }
    void opLDA()
    {
        auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
        auto ea = getEA(_info->addrMode);
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(ea);
        }
        accu = readByte(ea);
        ccSetNZv(accu);
    }
    void opLDS()
    {
        auto ea = getEA(_info->addrMode);
        if(_info->addrMode == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        _rSP = readWord(ea);
        ccSetNZv(_rSP);
    }
    void opLDX()
    {
        auto ea = getEA(_info->addrMode);
        if(_info->addrMode == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        _rIX = readWord(ea);
        ccSetNZv(_rIX);
    }
    void opLSR()
    {
        if((_info->addrMode & 7) == INHERENT) {
            auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
            _rCC.setFromBool(C, accu & 1);
            accu >>= 1;
            ccSetNZ(accu);
            _rCC.setFromBool(V, _rCC.isValue(N|C, N) || _rCC.isValue(N|C, C));
            readByte(_rPC);
        }
        else {
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            auto val = readByte(ea);
            dummyReadByte(ea);
            _rCC.setFromBool(C, val & 1);
            val >>= 1;
            ccSetNZ(val);
            _rCC.setFromBool(V, _rCC.isValue(N|C, N) || _rCC.isValue(N|C, C));
            writeByte(ea, val);
        }
    }
    void opNBA() {}
    void opNEG()
    {
        if((_info->addrMode & 7) == INHERENT) {
            auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
            auto old = accu;
            accu = -accu;
            ccSetNZ(accu);
            _rCC.setFromBool(V, old == 0x80);
            _rCC.setFromBool(C, old != 0);
            dummyReadByte(_rPC);
        }
        else {
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            auto val = readByte(ea);
            dummyReadByte(ea);
            auto old = val;
            val = -val;
            ccSetNZ(val);
            _rCC.setFromBool(V, old == 0x80);
            _rCC.setFromBool(C, old != 0);
            writeByte(ea, val);
        }
    }
    void opNOP()
    {
        readByte(_rPC);
    }
    void opORA()
    {
        auto ea = getEA(_info->addrMode);
        auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        auto val = readByte(ea);
        accu |= val;
        ccSetNZv(accu);
    }
    void opPSH()
    {
        auto accu = _info->addrMode & ACCUA ? _rA : _rB;
        readByte(_rPC);
        pushByte(accu);
        dummyReadByte(_rSP);
    }
    void opPUL()
    {
        auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
        readByte(_rPC);
        accu = pullByte();
        readByte(_rSP);
    }
    void opROL()
    {
        if((_info->addrMode & 7) == INHERENT) {
            auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
            auto old = accu;
            accu = (accu << 1) | (_rCC.isSet(C) ? 1 : 0);
            _rCC.setFromBool(C, old & 0x80);
            ccSetNZ(accu);
            readByte(_rPC);
        }
        else {
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            auto val = readByte(ea);
            dummyReadByte(ea);
            auto old = val;
            val = (val << 1) | (_rCC.isSet(C) ? 1 : 0);
            _rCC.setFromBool(C, old & 0x80);
            ccSetNZ(val);
            writeByte(ea, val);
        }
        _rCC.setFromBool(V, _rCC.isValue(N|C, N) || _rCC.isValue(N|C, C));
    }
    void opROR()
    {
        if((_info->addrMode & 7) == INHERENT) {
            auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
            auto old = accu;
            accu = (accu >> 1) | (_rCC.isSet(C) ? 0x80 : 0);
            _rCC.setFromBool(C, old & 1);
            ccSetNZ(accu);
            readByte(_rPC);
        }
        else {
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            auto val = readByte(ea);
            dummyReadByte(ea);
            auto old = val;
            val = (val >> 1) | (_rCC.isSet(C) ? 0x80 : 0);
            _rCC.setFromBool(C, old & 1);
            ccSetNZ(val);
            writeByte(ea, val);
        }
        _rCC.setFromBool(V, _rCC.isValue(N|C, N) || _rCC.isValue(N|C, C));
    }
    void opRTI()
    {
        readByte(_rPC);
        dummyReadByte(_rSP);
        _rCC.setFromVal(N|Z|V|C|I|H, pullByte());
        _rB = pullByte();
        _rA = pullByte();
        _rIX = pullWord();
        _rPC = pullWord();
    }
    void opRTS()
    {
        readByte(_rPC);
        dummyReadByte(_rSP);
        _rPC = pullWord();
    }
    void opSBA()
    {
        word_t res = word_t(_rA) - _rB;
        readByte(_rPC);
        ccSetFlagsCNZV(_rA, _rB, res);
        _rA = res;
    }
    void opSBC()
    {
        auto ea = getEA(_info->addrMode);
        auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        auto val = readByte(ea);
        word_t res = accu - val - (_rCC.isSet(C) ? 1 : 0);
        ccSetFlagsCNZV(accu, val, res);
        accu = res;
    }
    void opSEC()
    {
        _rCC.set(C);
        readByte(_rPC);
    }
    void opSEI()
    {
        _rCC.set(I);
        readByte(_rPC);
    }
    void opSEV()
    {
        _rCC.set(V);
        readByte(_rPC);
    }
    void opSTA()
    {
        auto accu = _info->addrMode & ACCUA ? _rA : _rB;
        auto ea = getEA(_info->addrMode);
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
            dummyReadByte(ea);
        }
        else {
            dummyReadByte(ea);
        }
        ccSetNZv(accu);
        writeByte(ea, accu);
    }
    void opSTS()
    {
        ccSetNZv(_rSP);
        auto ea = getEA(_info->addrMode);
        if(_info->addrMode == DIRECT || _info->addrMode == EXTENDED)
            dummyReadByte(ea);
        else if(_info->addrMode == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
            dummyReadByte(ea);
        }
        writeWord(ea, _rSP);
    }
    void opSTX()
    {
        ccSetNZv(_rIX);
        auto ea = getEA(_info->addrMode);
        if(_info->addrMode == DIRECT | _info->addrMode == EXTENDED)
            dummyReadByte(ea);
        else if(_info->addrMode == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
            readByte(ea);
        }
        writeWord(ea, _rIX);
    }
    void opSUB()
    {
        auto ea = getEA(_info->addrMode);
        auto& accu = _info->addrMode & ACCUA ? _rA : _rB;
        if((_info->addrMode & 7) == INDEXED) {
            dummyReadByte(_rIX);
            dummyReadByte(_rIXwoc);
        }
        auto val = readByte(ea);
        word_t res = accu - val;
        ccSetFlagsCNZV(accu, val, res);
        accu = res;
    }
    void opSWI()
    {
        readByte(_rPC);
        pushWord(_rPC);
        pushWord(_rIX);
        pushByte(_rA);
        pushByte(_rB);
        pushByte(_rCC.isValid(0xFF) ? _rCC.asNumber() : byte_t());
        _rCC.set(I);
        dummyReadByte(_rSP);
        _rPC = readWord(0xFFFA);
    }
    void opTAB()
    {
        _rB = _rA;
        ccSetNZv(_rB);
        readByte(_rPC);
    }
    void opTAP()
    {
        _rCC.setFromVal(N|Z|V|C|I|H, _rA & 0x3F);
        readByte(_rPC);
    }
    void opTBA()
    {
        _rA = _rB;
        ccSetNZv(_rA);
        readByte(_rPC);
    }
    void opTPA()
    {
        if(_rCC.isValid(N|Z|V|C|H))
            _rA = _rCC.asNumber();
        readByte(_rPC);
    }
    void opTST()
    {
        if((_info->addrMode & 7) == INHERENT) {
            ccSetNZ(_info->addrMode & ACCUA ? _rA : _rB);
            _rCC.clear(C|V);
            readByte(_rPC);
        }
        else {
            auto ea = getEA(_info->addrMode);
            if(_info->addrMode == INDEXED) {
                dummyReadByte(_rIX);
                dummyReadByte(_rIXwoc);
            }
            auto t = readByte(ea);
            ccSetNZ(t);
            dummyReadByte(ea);
            _rCC.clear(C|V);
            writeByte(ea, t);
        }
    }
    void opTSX()
    {
        readByte(_rPC);
        dummyReadByte(_rSP);
        _rIX = _rSP + 1;
        dummyReadByte(_rIX);
    }
    void opTXS()
    {
        readByte(_rPC);
        dummyReadByte(_rIX);
        _rSP = _rIX - 1;
        dummyReadByte(_rSP);
    }
    void opWAI()
    {
        readByte(_rPC);
        pushWord(_rPC);
        pushWord(_rIX);
        pushByte(_rA);
        pushByte(_rB);
        _cpuState = eWAIT;
    }

    M6800Bus<byte_t, word_t>& _bus;
    byte_t _opcode{};
    const OpcodeInfo* _info;
    byte_t _rA{};
    byte_t _rB{};
    word_t _rIX{};
    word_t _rIXwoc{};
    word_t _rPC{};
    word_t _rSP{};
    ccflags_t _rCC{"11hinzvc", ByteType(0xC0), ByteType(0xFF)};
    int64_t _cycles{};
    int64_t _instructions{};
    CpuState _cpuState{eNORMAL};
    bool _irq{false};
    bool _nmi{false};
    bool _halt{false};
#ifdef M6800_WITH_TIME
    Time::ticks_t _clockSpeed{};
    ClockedTime _systemTime;
#endif
#if defined(OC) || defined(OC_ILL)
#error "Conflicting symbols defined!"
#endif
#define OC(x) &M6800::op##x, #x
#define OC_ILL &M6800::opINVALID, "???"
    static inline OpcodeInfo _opcodes[] = {
        // 00-07
        {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT, NORMAL, OC(NOP)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 0, INVALID, HALT, OC_ILL},
        {1, 0, INVALID, HALT, OC_ILL}, {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT, NORMAL, OC(TAP)}, {1, 2, INHERENT, NORMAL, OC(TPA)},
        // 08-0F
        {1, 4, INHERENT, NORMAL, OC(INX)}, {1, 4, INHERENT, NORMAL, OC(DEX)}, {1, 2, INHERENT, NORMAL, OC(CLV)}, {1, 2, INHERENT, NORMAL, OC(SEV)},
        {1, 2, INHERENT, NORMAL, OC(CLC)}, {1, 2, INHERENT, NORMAL, OC(SEC)}, {1, 2, INHERENT, NORMAL, OC(CLI)}, {1, 2, INHERENT, NORMAL, OC(SEI)},
        // 10-17
        {1, 2, INHERENT, NORMAL, OC(SBA)}, {1, 2, INHERENT, NORMAL, OC(CBA)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 0, INVALID, HALT, OC_ILL},
        {1, 2, INHERENT, NORMAL|UNDOC, OC(NBA)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT, NORMAL, OC(TAB)}, {1, 2, INHERENT, NORMAL, OC(TBA)},
        // 18-1F
        {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT, NORMAL, OC(DAA)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT, NORMAL, OC(ABA)},
        {1, 0, INVALID, HALT, OC_ILL}, {1, 0, INVALID, HALT, OC_ILL}, {1, 0, INVALID, HALT, OC_ILL}, {1, 0, INVALID, HALT, OC_ILL},
        // 20-27
        {2, 4, RELATIVE, JUMP, OC(BRA)}, {1, 0, INVALID, HALT, OC_ILL}, {2, 4, RELATIVE, CCJUMP, OC(BHI)}, {2, 4, RELATIVE, CCJUMP, OC(BLS)},
        {2, 4, RELATIVE, CCJUMP, OC(BCC)}, {2, 4, RELATIVE, CCJUMP, OC(BCS)}, {2, 4, RELATIVE, CCJUMP, OC(BNE)}, {2, 4, RELATIVE, CCJUMP, OC(BEQ)},
        // 28-2F
        {2, 4, RELATIVE, CCJUMP, OC(BVC)}, {2, 4, RELATIVE, CCJUMP, OC(BVS)}, {2, 4, RELATIVE, CCJUMP, OC(BPL)}, {2, 4, RELATIVE, CCJUMP, OC(BMI)},
        {2, 4, RELATIVE, CCJUMP, OC(BGE)}, {2, 4, RELATIVE, CCJUMP, OC(BLT)}, {2, 4, RELATIVE, CCJUMP, OC(BGT)}, {2, 4, RELATIVE, CCJUMP, OC(BLE)},
        // 30-37
        {1, 4, INHERENT, STACK, OC(TSX)}, {1, 4, INHERENT, STACK, OC(INS)}, {1, 4, INHERENT|ACCUA, STACK, OC(PUL)}, {1, 4, INHERENT|ACCUB, STACK, OC(PUL)},
        {1, 4, INHERENT, STACK, OC(DES)}, {1, 4, INHERENT, STACK, OC(TXS)}, {1, 4, INHERENT|ACCUA, STACK, OC(PSH)}, {1, 4, INHERENT|ACCUB, STACK, OC(PSH)},
        // 38-3F
        {1, 0, INVALID, HALT, OC_ILL}, {1, 5, INHERENT, RETURN, OC(RTS)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 10, INHERENT, RETURN, OC(RTI)},
        {1, 0, INVALID, HALT, OC_ILL}, {1, 0, INVALID, HALT, OC_ILL}, {1, 9, INHERENT, STACK, OC(WAI)}, {1, 12, INHERENT, CALL, OC(SWI)},
        // 40-47
        {1, 2, INHERENT|ACCUA, NORMAL, OC(NEG)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT|ACCUA, NORMAL, OC(COM)},
        {1, 2, INHERENT|ACCUA, NORMAL, OC(LSR)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT|ACCUA, NORMAL, OC(ROR)}, {1, 2, INHERENT|ACCUA, NORMAL, OC(ASR)},
        // 48-4F
        {1, 2, INHERENT|ACCUA, NORMAL, OC(ASL)}, {1, 2, INHERENT|ACCUA, NORMAL, OC(ROL)}, {1, 2, INHERENT|ACCUA, NORMAL, OC(DEC)}, {1, 0, INVALID, HALT, OC_ILL},
        {1, 2, INHERENT|ACCUA, NORMAL, OC(INC)}, {1, 2, INHERENT|ACCUA, NORMAL, OC(TST)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT|ACCUA, NORMAL, OC(CLR)},
        // 50-57
        {1, 2, INHERENT|ACCUB, NORMAL, OC(NEG)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT|ACCUB, NORMAL, OC(COM)},
        {1, 2, INHERENT|ACCUB, NORMAL, OC(LSR)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT|ACCUB, NORMAL, OC(ROR)}, {1, 2, INHERENT|ACCUB, NORMAL, OC(ASR)},
        // 58-5F
        {1, 2, INHERENT|ACCUB, NORMAL, OC(ASL)}, {1, 2, INHERENT|ACCUB, NORMAL, OC(ROL)}, {1, 2, INHERENT|ACCUB, NORMAL, OC(DEC)}, {1, 0, INVALID, HALT, OC_ILL},
        {1, 2, INHERENT|ACCUB, NORMAL, OC(INC)}, {1, 2, INHERENT|ACCUB, NORMAL, OC(TST)}, {1, 0, INVALID, HALT, OC_ILL}, {1, 2, INHERENT|ACCUB, NORMAL, OC(CLR)},
        // 60-67
        {2, 7, INDEXED, NORMAL, OC(NEG)}, {2, 0, INVALID, HALT, OC_ILL}, {2, 0, INVALID, HALT, OC_ILL}, {2, 7, INDEXED, NORMAL, OC(COM)},
        {2, 7, INDEXED, NORMAL, OC(LSR)}, {2, 0, INVALID, HALT, OC_ILL}, {2, 7, INDEXED, NORMAL, OC(ROR)}, {2, 7, INDEXED, NORMAL, OC(ASR)},
        // 68-6F
        {2, 7, INDEXED, NORMAL, OC(ASL)}, {2, 7, INDEXED, NORMAL, OC(ROL)}, {2, 7, INDEXED, NORMAL, OC(DEC)}, {2, 0, INVALID, HALT, OC_ILL},
        {2, 7, INDEXED, NORMAL, OC(INC)}, {2, 7, INDEXED, NORMAL, OC(TST)}, {2, 4, INDEXED, JUMP, OC(JMP)}, {2, 7, INDEXED, NORMAL, OC(CLR)},
        // 70-77
        {3, 6, EXTENDED, NORMAL, OC(NEG)}, {3, 0, INVALID, HALT, OC_ILL}, {3, 0, INVALID, HALT, OC_ILL}, {3, 6, EXTENDED, NORMAL, OC(COM)},
        {3, 6, EXTENDED, NORMAL, OC(LSR)}, {3, 0, INVALID, HALT, OC_ILL}, {3, 6, EXTENDED, NORMAL, OC(ROR)}, {3, 6, EXTENDED, NORMAL, OC(ASR)},
        // 78-7F
        {3, 6, EXTENDED, NORMAL, OC(ASL)}, {3, 6, EXTENDED, NORMAL, OC(ROL)}, {3, 6, EXTENDED, NORMAL, OC(DEC)}, {3, 0, INVALID, HALT, OC_ILL},
        {3, 6, EXTENDED, NORMAL, OC(INC)}, {3, 6, EXTENDED, NORMAL, OC(TST)}, {3, 3, EXTENDED, JUMP, OC(JMP)}, {3, 6, EXTENDED, NORMAL, OC(CLR)},
        // 80-87
        {2, 2, IMMEDIATE|ACCUA, NORMAL, OC(SUB)}, {2, 2, IMMEDIATE|ACCUA, NORMAL, OC(CMP)}, {2, 2, IMMEDIATE|ACCUA, NORMAL, OC(SBC)}, {2, 0, INVALID, HALT, OC_ILL},
        {2, 2, IMMEDIATE|ACCUA, NORMAL, OC(AND)}, {2, 2, IMMEDIATE|ACCUA, NORMAL, OC(BIT)}, {2, 2, IMMEDIATE|ACCUA, NORMAL, OC(LDA)}, {2, 2, IMMEDIATE|ACCUA, NORMAL|UNDOC, OC(STA)},
        // 88-8F
        {2, 2, IMMEDIATE|ACCUA, NORMAL, OC(EOR)}, {2, 2, IMMEDIATE|ACCUA, NORMAL, OC(ADC)}, {2, 2, IMMEDIATE|ACCUA, NORMAL, OC(ORA)}, {2, 2, IMMEDIATE|ACCUA, NORMAL, OC(ADD)},
        {3, 3, IMMEDIATE16, NORMAL, OC(CPX)}, {2, 8, RELATIVE, CCCALL, OC(BSR)}, {2, 3, IMMEDIATE16, STACK, OC(LDS)}, {2, 0, IMMEDIATE16, STACK|UNDOC, OC(STS)},
        // 90-97
        {2, 3, DIRECT|ACCUA, NORMAL, OC(SUB)}, {2, 3, DIRECT|ACCUA, NORMAL, OC(CMP)}, {2, 3, DIRECT|ACCUA, NORMAL, OC(SBC)}, {2, 0, INVALID, HALT, OC_ILL},
        {2, 3, DIRECT|ACCUA, NORMAL, OC(AND)}, {2, 3, DIRECT|ACCUA, NORMAL, OC(BIT)}, {2, 3, DIRECT|ACCUA, NORMAL, OC(LDA)}, {2, 4, DIRECT|ACCUA, NORMAL, OC(STA)},
        // 98-9F
        {2, 3, DIRECT|ACCUA, NORMAL, OC(EOR)}, {2, 3, DIRECT|ACCUA, NORMAL, OC(ADC)}, {2, 3, DIRECT|ACCUA, NORMAL, OC(ORA)}, {2, 3, DIRECT|ACCUA, NORMAL, OC(ADD)},
        {2, 4, DIRECT, NORMAL, OC(CPX)}, {1, 0, INVALID, HALT, OC_ILL/*HCF*/}, {2, 4, DIRECT, STACK, OC(LDS)}, {2, 5, DIRECT, STACK, OC(STS)},
        // A0-A7
        {2, 5, INDEXED|ACCUA, NORMAL, OC(SUB)}, {2, 5, INDEXED|ACCUA, NORMAL, OC(CMP)}, {2, 5, INDEXED|ACCUA, NORMAL, OC(SBC)}, {2, 0, INVALID, HALT, OC_ILL},
        {2, 5, INDEXED|ACCUA, NORMAL, OC(AND)}, {2, 5, INDEXED|ACCUA, NORMAL, OC(BIT)}, {2, 5, INDEXED|ACCUA, NORMAL, OC(LDA)}, {2, 6, INDEXED|ACCUA, NORMAL, OC(STA)},
        // A8-AF
        {2, 5, INDEXED|ACCUA, NORMAL, OC(EOR)}, {2, 5, INDEXED|ACCUA, NORMAL, OC(ADC)}, {2, 5, INDEXED|ACCUA, NORMAL, OC(ORA)}, {2, 5, INDEXED|ACCUA, NORMAL, OC(ADD)},
        {2, 6, INDEXED, NORMAL, OC(CPX)}, {2, 8, INDEXED, CALL, OC(JSR)}, {2, 6, INDEXED, STACK, OC(LDS)}, {2, 7, INDEXED, STACK, OC(STS)},
        // B0-B7
        {3, 4, EXTENDED|ACCUA, NORMAL, OC(SUB)}, {3, 4, EXTENDED|ACCUA, NORMAL, OC(CMP)}, {3, 4, EXTENDED|ACCUA, NORMAL, OC(SBC)}, {3, 0, INVALID, HALT, OC_ILL},
        {3, 4, EXTENDED|ACCUA, NORMAL, OC(AND)}, {3, 4, EXTENDED|ACCUA, NORMAL, OC(BIT)}, {3, 4, EXTENDED|ACCUA, NORMAL, OC(LDA)}, {3, 5, EXTENDED|ACCUA, NORMAL, OC(STA)},
        // B8-BF
        {3, 4, EXTENDED|ACCUA, NORMAL, OC(EOR)}, {3, 4, EXTENDED|ACCUA, NORMAL, OC(ADC)}, {3, 4, EXTENDED|ACCUA, NORMAL, OC(ORA)}, {3, 4, EXTENDED|ACCUA, NORMAL, OC(ADD)},
        {3, 5, EXTENDED, NORMAL, OC(CPX)}, {3, 9, EXTENDED, CALL, OC(JSR)}, {3, 5, EXTENDED, STACK, OC(LDS)}, {3, 6, EXTENDED, STACK, OC(STS)},
        // C0-C7
        {2, 2, IMMEDIATE|ACCUB, NORMAL, OC(SUB)}, {2, 2, IMMEDIATE|ACCUB, NORMAL, OC(CMP)}, {2, 2, IMMEDIATE|ACCUB, NORMAL, OC(SBC)}, {2, 0, INVALID, HALT, OC_ILL},
        {2, 2, IMMEDIATE|ACCUB, NORMAL, OC(AND)}, {2, 2, IMMEDIATE|ACCUB, NORMAL, OC(BIT)}, {2, 2, IMMEDIATE|ACCUB, NORMAL, OC(LDA)}, {2, 2, IMMEDIATE|ACCUB, NORMAL|UNDOC, OC(STA)},
        // C8-CF
        {2, 2, IMMEDIATE|ACCUB, NORMAL, OC(EOR)}, {2, 2, IMMEDIATE|ACCUB, NORMAL, OC(ADC)}, {2, 2, IMMEDIATE|ACCUB, NORMAL, OC(ORA)}, {2, 2, IMMEDIATE|ACCUB, NORMAL, OC(ADD)},
        {2, 0, INVALID, HALT, OC_ILL}, {2, 0, INVALID, HALT, OC_ILL}, {2, 3, IMMEDIATE16, NORMAL, OC(LDX)}, {2, 0, IMMEDIATE16, STACK|UNDOC, OC(STX)},
        // D0-D7
        {2, 3, DIRECT|ACCUB, NORMAL, OC(SUB)}, {2, 3, DIRECT|ACCUB, NORMAL, OC(CMP)}, {2, 3, DIRECT|ACCUB, NORMAL, OC(SBC)}, {2, 0, INVALID, HALT, OC_ILL},
        {2, 3, DIRECT|ACCUB, NORMAL, OC(AND)}, {2, 3, DIRECT|ACCUB, NORMAL, OC(BIT)}, {2, 3, DIRECT|ACCUB, NORMAL, OC(LDA)}, {2, 4, DIRECT|ACCUB, NORMAL, OC(STA)},
        // D8-DF
        {2, 3, DIRECT|ACCUB, NORMAL, OC(EOR)}, {2, 3, DIRECT|ACCUB, NORMAL, OC(ADC)}, {2, 3, DIRECT|ACCUB, NORMAL, OC(ORA)}, {2, 3, DIRECT|ACCUB, NORMAL, OC(ADD)},
        {2, 0, INVALID, HALT, OC_ILL}, {2, 0, INVALID, HALT, OC_ILL/*HCF*/}, {2, 4, DIRECT, NORMAL, OC(LDX)}, {2, 5, DIRECT, STACK, OC(STX)},
        // E0-E7
        {2, 5, INDEXED|ACCUB, NORMAL, OC(SUB)}, {2, 5, INDEXED|ACCUB, NORMAL, OC(CMP)}, {2, 5, INDEXED|ACCUB, NORMAL, OC(SBC)}, {2, 0, INVALID, HALT, OC_ILL},
        {2, 5, INDEXED|ACCUB, NORMAL, OC(AND)}, {2, 5, INDEXED|ACCUB, NORMAL, OC(BIT)}, {2, 5, INDEXED|ACCUB, NORMAL, OC(LDA)}, {2, 6, INDEXED|ACCUB, NORMAL, OC(STA)},
        // E8-EF
        {2, 5, INDEXED|ACCUB, NORMAL, OC(EOR)}, {2, 5, INDEXED|ACCUB, NORMAL, OC(ADC)}, {2, 5, INDEXED|ACCUB, NORMAL, OC(ORA)}, {2, 5, INDEXED|ACCUB, NORMAL, OC(ADD)},
        {2, 0, INVALID, HALT, OC_ILL}, {2, 0, INVALID, HALT, OC_ILL}, {2, 6, INDEXED, NORMAL, OC(LDX)}, {2, 7, INDEXED, STACK, OC(STX)},
        // F0-F7
        {3, 4, EXTENDED|ACCUB, NORMAL, OC(SUB)}, {3, 4, EXTENDED|ACCUB, NORMAL, OC(CMP)}, {3, 4, EXTENDED|ACCUB, NORMAL, OC(SBC)}, {3, 0, INVALID, HALT, OC_ILL},
        {3, 4, EXTENDED|ACCUB, NORMAL, OC(AND)}, {3, 4, EXTENDED|ACCUB, NORMAL, OC(BIT)}, {3, 4, EXTENDED|ACCUB, NORMAL, OC(LDA)}, {3, 5, EXTENDED|ACCUB, NORMAL, OC(STA)},
        // F8-FF
        {3, 4, EXTENDED|ACCUB, NORMAL, OC(EOR)}, {3, 4, EXTENDED|ACCUB, NORMAL, OC(ADC)}, {3, 4, EXTENDED|ACCUB, NORMAL, OC(ORA)}, {3, 4, EXTENDED|ACCUB, NORMAL, OC(ADD)},
        {3, 0, INVALID, HALT, OC_ILL}, {3, 0, INVALID, HALT, OC_ILL}, {3, 5, EXTENDED, NORMAL, OC(LDX)}, {3, 6, EXTENDED, STACK, OC(STX)},
    };
#undef OC
#undef OC_ILL
};

using CadmiumM6800 = M6800<>;
#ifdef M6800_SPECULATIVE_SUPPORT
    using SpeculativeM6800 = M6800<h_uint8_t, h_uint16_t, h_uint32_t, HeuristicBitfield<>>;
#endif

#endif // M6800_STATE_BUS_ONLY

}  // namespace emu

