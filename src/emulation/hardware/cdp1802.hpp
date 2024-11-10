//---------------------------------------------------------------------------------------
// src/emulation/cdp1802.hpp
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

#ifdef CADMIUM_WITH_GENERIC_CPU
#include <emulation/hardware/genericcpu.hpp>
#else
#include <emulation/time.hpp>
#endif

#include <fmt/format.h>

#include <cstdint>
#include <functional>
#include <utility>
#include <iostream>

#ifndef NDEBUG
//#define DIFFERENTIATE_CYCLES
#endif

#define CASE_7(base) case base: case base+1: case base+2: case base+3: case base+4: case base+5: case base+6
#define CASE_15(base) case base: case base+1: case base+2: case base+3: case base+4: case base+5: case base+6: case base+7:\
                     case base+8: case base+9: case base+10: case base+11: case base+12: case base+13: case base+14
#define CASE_16(base) case base: case base+1: case base+2: case base+3: case base+4: case base+5: case base+6: case base+7:\
                     case base+8: case base+9: case base+10: case base+11: case base+12: case base+13: case base+14: case base+15

namespace emu {

class Cdp1802Bus
{
public:
    virtual ~Cdp1802Bus() = default;
    virtual uint8_t readByte(uint16_t addr) const = 0;
    virtual uint8_t readByteDMA(uint16_t addr) const = 0;
    virtual void writeByte(uint16_t addr, uint8_t val) = 0;
};

struct Cdp1802State
{
    uint16_t r[16]{};
    uint16_t p:4;
    uint16_t x:4;
    uint16_t n:4;
    uint16_t i:4;
    uint8_t t{};
    uint8_t d{};
    bool df{};
    bool ie{};
    bool q{};
    int64_t cycles{};
};

#ifdef CADMIUM_WITH_GENERIC_CPU
#define GENERIC_OVERRIDE override
#define GENERIC_BASE : public GenericCpu
#else
#define GENERIC_OVERRIDE
#define GENERIC_BASE
#endif

/// Implementation of a CDP1802 CPU
class Cdp1802 : public GenericCpu
{
public:
    struct Disassembled {
        int size;
        std::string text;
    };
    using OutputHandler = std::function<void(uint8_t, uint8_t)>;
    using InputHandler = std::function<uint8_t (uint8_t)>;
    using NEFInputHandler = std::function<bool (uint8_t)>;
    explicit Cdp1802(Cdp1802Bus& bus, Time::ticks_t clockFreq = 3200000)
        : _bus(bus)
        , _systemTime(clockFreq)
    {
        _output = [](uint8_t, uint8_t){};
        _input = [](uint8_t){ return 0; };
        _inputNEF = [](uint8_t) { return true; };
        Cdp1802::reset();
    }

    void reset() override
    {
        _rI = 0;
        _rN = 0;
        _rP = 0;
        _rQ = false;
        _rX = 0;
        _rR[0] = 0;
        _rR[1] = 0;
        _rIE = true;
        _cycles = 0;
#ifdef DIFFERENTIATE_CYCLES
        _idleCycles = 0;
        _irqCycles = 0;
#endif
        _systemTime.reset();
        _execMode = eRUNNING;
        _cpuState = eNORMAL;
        _errorMessage.clear();
    }

    void setOutputHandler(OutputHandler handler)
    {
        _output = std::move(handler);
    }

    void setInputHandler(InputHandler handler)
    {
        _input = std::move(handler);
    }

    void setNEFInputHandler(NEFInputHandler handler)
    {
        _inputNEF = handler;
    }

    //void triggerIrq() { _irq = true; }
    uint16_t getR(uint8_t index) const { return _rR[index & 0xf]; }
    void setR(uint8_t index, uint16_t value) { _rR[index & 0xf] = value; }
    bool getIE() const { return _rIE; }
    const ClockedTime& time() const override { return _systemTime; }
    int64_t cycles() const GENERIC_OVERRIDE { return _cycles; }
#ifdef DIFFERENTIATE_CYCLES
    int64_t getIdleCycles() const { return _idleCycles; }
    int64_t getIrqCycles() const { return _irqCycles; }
#endif
    CpuState getCpuState() const { return _cpuState; }
    uint16_t& PC() { return _rR[_rP]; }
    uint8_t getN() const { return _rN; }
    uint8_t getP() const { return _rP; }
    uint8_t getX() const { return _rX; }
    uint8_t getD() const { return _rD; }
    uint8_t getDF() const { return _rDF ? 1 : 0; }
    uint8_t getT() const { return _rT; }
    bool getQ() const { return _rQ; }
    uint16_t& RN() { return _rR[_rN]; }
    uint16_t& RX() { return _rR[_rX]; }
    void getState(Cdp1802State& state) const
    {
        std::memcpy(state.r, _rR, sizeof(_rR));
        state.p = _rP;
        state.x = _rX;
        state.n = _rN;
        state.i = _rI;
        state.t = _rT;
        state.d = _rD;
        state.df = _rDF;
        state.ie = _rIE;
        state.q = _rQ;
        state.cycles = _cycles;
    }
    void setState(const Cdp1802State& state)
    {
        std::memcpy(_rR, state.r, sizeof(_rR));
        _rP = state.p;
        _rX = state.x;
        _rN = state.n;
        _rI = state.i;
        _rT = state.t;
        _rD = state.d;
        _rDF = state.df;
        _rIE = state.ie;
        _rQ = state.q;
        _cycles = state.cycles;
    }
    uint8_t readByte(uint16_t addr) { return _bus.readByte(addr); }
    uint8_t readByteDMA(uint16_t addr) { return _bus.readByteDMA(addr); }
    void writeByte(uint16_t addr, uint8_t val) { _bus.writeByte(addr, val); }
    void branchShort(bool condition)
    {
        if(condition) {
            PC() = (PC() & 0xFF00) | readByte(PC());
        }
        else {
            ++PC();
        }
    }
    void addCycles(cycles_t cycles)
    {
        _cycles += cycles;
#ifdef DIFFERENTIATE_CYCLES
        if(_cpuState == eIDLE) {
            _idleCycles += cycles;
        }
        else if(!_rIE) {
            _irqCycles += cycles;
        }
#endif
        _systemTime.addCycles(cycles);
    }
    void branchLong(bool condition)
    {
        if(condition) {
            PC() = (readByte(PC()) << 8) | readByte(PC()+1);
        }
        else {
            PC() += 2;
        }
        addCycles(8);
    }
    void skipLong(bool condition)
    {
        if(condition) {
            PC() += 2;
        }
        addCycles(8);
    }

#ifdef CADMIUM_WITH_GENERIC_CPU
    std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const override
#else
    std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const
#endif
    {
        auto addr = pc >= 0 ? pc : _rR[_rP];
        uint8_t data[3];
        data[0] = _bus.readByte(addr);
        data[1] = _bus.readByte(addr+1);
        data[2] = _bus.readByte(addr+2);
        auto [size, text] = Cdp1802::disassembleInstruction(data, data+3);
        if(bytes) *bytes = size;
        switch(size) {
           case 2:  return fmt::format("{:04x}: {:02x} {:02x}  {}", addr, data[0], data[1], text);
           case 3:  return fmt::format("{:04x}: {:02x} {:02x} {:02x}  {}", addr, data[0], data[1], data[2], text);
           default: return fmt::format("{:04x}: {:02x}     {}", addr, data[0], text);
        }
    }

    std::string dumpStateLine() const
    {
        return fmt::format("R0:{:04x} R1:{:04x} R2:{:04x} R3:{:04x} R4:{:04x} R5:{:04x} R6:{:04x} R7:{:04x} R8:{:04x} R9:{:04x} RA:{:04x} RB:{:04x} RC:{:04x} RD:{:04x} RE:{:04x} RF:{:04x} D:{:02x} DF:{} P:{:1x} X:{:1x} N:{:1x} I:{:1x} T:{:02x} PC:{:04x} O:{:02x} EF:{}{}{}{} Q:{}", getR(0), getR(1), getR(2),
                           getR(3), getR(4), getR(5), getR(6), getR(7), getR(8), getR(9), getR(10), getR(11), getR(12), getR(13), getR(14), getR(15), _rD, _rDF?1:0, _rP, _rX, _rN, _rI, _rT, _rR[_rP], _bus.readByte(_rR[_rP]), _inputNEF(0)?0:1, _inputNEF(1)?0:1, _inputNEF(2)?0:1, _inputNEF(3)?0:1, _rQ?1:0);
    }

    static Disassembled disassembleInstruction(const uint8_t* code, const uint8_t* end)
    {
        auto opcode = *code++;
        auto n = opcode & 0xF;
        switch(opcode) {
            case 0x00: return {1, "IDL"};
            CASE_15(0x01): return {1, fmt::format("LDN R{:X}", n)};
            CASE_16(0x10): return {1, fmt::format("INC R{:X}", n)};
            CASE_16(0x20): return {1, fmt::format("DEC R{:X}", n)};
            case 0x30: return {2, fmt::format("BR 0x{:02X}", *code)};
            case 0x31: return {2, fmt::format("BQ 0x{:02X}", *code)};
            case 0x32: return {2, fmt::format("BZ 0x{:02X}", *code)};
            case 0x33: return {2, fmt::format("BDF 0x{:02X}", *code)};
            case 0x34: return {2, fmt::format("B1 0x{:02X}", *code)};
            case 0x35: return {2, fmt::format("B2 0x{:02X}", *code)};
            case 0x36: return {2, fmt::format("B3 0x{:02X}", *code)};
            case 0x37: return {2, fmt::format("B4 0x{:02X}", *code)};
            case 0x38: return {1, "SKP"};
            case 0x39: return {2, fmt::format("BNQ 0x{:02X}", *code)};
            case 0x3A: return {2, fmt::format("BNZ 0x{:02X}", *code)};
            case 0x3B: return {2, fmt::format("BNF 0x{:02X}", *code)};
            case 0x3C: return {2, fmt::format("BN1 0x{:02X}", *code)};
            case 0x3D: return {2, fmt::format("BN2 0x{:02X}", *code)};
            case 0x3e: return {2, fmt::format("BN3 0x{:02X}", *code)};
            case 0x3f: return {2, fmt::format("BN4 0x{:02X}", *code)};
            CASE_16(0x40): return {1, fmt::format("LDA R{:X}", n)};
            CASE_16(0x50): return {1, fmt::format("STR R{:X}", n)};
            case 0x60: return {1, "IRX"};
            CASE_7(0x61): return {1, fmt::format("OUT {:X}", n)};
            CASE_7(0x69): return {1, fmt::format("INP {:X}", n&7)};
            case 0x70: return {1, "RET"};
            case 0x71: return {1, "DIS"};
            case 0x72: return {1, "LDXA"};
            case 0x73: return {1, "STXD"};
            case 0x74: return {1, "ADC"};
            case 0x75: return {1, "SDB"};
            case 0x76: return {1, "SHRC"};
            case 0x77: return {1, "SMB"};
            case 0x78: return {1, "SAV"};
            case 0x79: return {1, "MARK"};
            case 0x7A: return {1, "REQ"};
            case 0x7B: return {1, "SEQ"};
            case 0x7C: return {2, fmt::format("ADCI #0x{:02X}", *code)};
            case 0x7D: return {2, fmt::format("SDBI #0x{:02X}", *code)};
            case 0x7E: return {1, "SHLC"};
            case 0x7F: return {2, fmt::format("SMBI #0x{:02X}", *code)};
            CASE_16(0x80): return {1, fmt::format("GLO R{:X}", n)};
            CASE_16(0x90): return {1, fmt::format("GHI R{:X}", n)};
            CASE_16(0xA0): return {1, fmt::format("PLO R{:X}", n)};
            CASE_16(0xB0): return {1, fmt::format("PHI R{:X}", n)};
            case 0xC0: return {3, fmt::format("LBR 0x{:04X}", (*code<<8)|*(code+1))};
            case 0xC1: return {3, fmt::format("LBQ 0x{:04X}", (*code<<8)|*(code+1))};
            case 0xC2: return {3, fmt::format("LBZ 0x{:04X}", (*code<<8)|*(code+1))};
            case 0xC3: return {3, fmt::format("LBDF 0x{:04X}", (*code<<8)|*(code+1))};
            case 0xC4: return {1, "NOP"};
            case 0xC5: return {1, "LSNQ"};
            case 0xC6: return {1, "LSNZ"};
            case 0xC7: return {1, "LSNF"};
            case 0xC8: return {1, "LSKP"};
            case 0xC9: return {3, fmt::format("LBNQ 0x{:04X}", (*code<<8)|*(code+1))};
            case 0xCA: return {3, fmt::format("LBNZ 0x{:04X}", (*code<<8)|*(code+1))};
            case 0xCB: return {3, fmt::format("LBNF 0x{:04X}", (*code<<8)|*(code+1))};
            case 0xCC: return {1, "LSIE"};
            case 0xCD: return {1, "LSQ"};
            case 0xCE: return {1, "LSZ"};
            case 0xCF: return {1, "LSDF"};
            CASE_16(0xD0): return {1, fmt::format("SEP R{:X}", n)};
            CASE_16(0xE0): return {1, fmt::format("SEX R{:X}", n)};
            case 0xF0: return {1, "LDX"};
            case 0xF1: return {1, "OR"};
            case 0xF2: return {1, "AND"};
            case 0xF3: return {1, "XOR"};
            case 0xF4: return {1, "ADD"};
            case 0xF5: return {1, "SD"};
            case 0xF6: return {1, "SHR"};
            case 0xF7: return {1, "SM"};
            case 0xF8: return {2, fmt::format("LDI #0x{:02X}", *code)};
            case 0xF9: return {2, fmt::format("ORI #0x{:02X}", *code)};
            case 0xFA: return {2, fmt::format("ANI #0x{:02X}", *code)};
            case 0xFB: return {2, fmt::format("XRI #0x{:02X}", *code)};
            case 0xFC: return {2, fmt::format("ADI #0x{:02X}", *code)};
            case 0xFD: return {2, fmt::format("SDI #0x{:02X}", *code)};
            case 0xFE: return {1, "SHL"};
            case 0xFF: return {2, fmt::format("SMI #0x{:02X}", *code)};
            default:
                return {1, "ILLEGAL"};
        }
    }

    void triggerInterrupt()
    {
        if(_rIE) {
            //std::clog << fmt::format("CDP1802: [{:9}]  ", _cycles>>3) << "--- IRQ ---" << std::endl;
            _rIE = false;
            addCycles(8);
            _rT = (_rX << 4) | _rP;
            _rP = 1;
            _rX = 2;
            if(_cpuState == eIDLE)
                _cpuState = eNORMAL;
        }
    }

    int64_t executeFor(int64_t microseconds) override
    {
        if(_execMode != GenericCpu::ePAUSED) {
            auto startTime = _systemTime;
            auto endTime = startTime + Time::fromMicroseconds(microseconds);
            while (_execMode != GenericCpu::ePAUSED && _systemTime < endTime) {
                executeInstruction();
            }
            return startTime.excessTime_us(_systemTime, microseconds);
        }
        return 0;
    }

    void executeDMAIn(uint8_t data)
    {
        if(_cpuState == eIDLE)
            _cpuState = eNORMAL;
        addCycles(8);
        writeByte(_rR[0]++, data);
    }

    std::pair<uint8_t,uint16_t> executeDMAOut()
    {
        if(_cpuState == eIDLE)
            _cpuState = eNORMAL;
        addCycles(8);
        auto addr = _rR[0]++;
        return {readByteDMA(addr), addr};
    }

    int executeInstruction() override
    {
        const auto startCycles = _cycles;
        if (_execMode == GenericCpu::ePAUSED || _cpuState == eERROR)
            return 0;
        //if(!_rIE)
        //    std::clog << fmt::format("CDP1802: [{:9}] {:<24} | ", _cycles, disassembleCurrentStatement()) << dumpStateLine() << std::endl;
        if(_cpuState == eIDLE) {
            addCycles(8);
            return 8;
        }
        uint8_t opcode = readByte(PC()++);
        addCycles(16);
        _rN = opcode & 0xF;
        switch (opcode) {
            case 0x00: // IDL ; WAIT FOR DMA OR INTERRUPT; M(R(0)) → BUS
                _cpuState = eIDLE;
                break;
            CASE_15(0x01): // LDN Rn ; M(R(N)) → D; FOR N not 0
                _rD = readByte(RN());
                break;
            CASE_16(0x10): // INC Rn ; R(N) + 1 → R(N)
                RN()++;
                break;
            CASE_16(0x20): // DEC Rn ; R(N) - 1 → R(N)
                RN()--;
                break;
            case 0x30: // BR
                branchShort(true);
                break;
            case 0x31: // BQ
                branchShort(_rQ);
                break;
            case 0x32: // BZ
                branchShort(!_rD);
                break;
            case 0x33: // BDF
                branchShort(_rDF);
                break;
            case 0x34: // B1
                branchShort(_inputNEF(0));
                break;
            case 0x35: // B2
                branchShort(_inputNEF(1));
                break;
            case 0x36: // B3
                branchShort(_inputNEF(2));
                break;
            case 0x37: // B4
                branchShort(_inputNEF(3));
                break;
            case 0x38: // SKP
                PC()++;
                break;
            case 0x39: // BNQ
                branchShort(!_rQ);
                break;
            case 0x3A: // BNZ
                branchShort(_rD != 0);
                break;
            case 0x3B: // BNF
                branchShort(!_rDF);
                break;
            case 0x3c: // BN1
                branchShort(!_inputNEF(0));
                break;
            case 0x3d: // BN2
                branchShort(!_inputNEF(1));
                break;
            case 0x3e: // BN3
                branchShort(!_inputNEF(2));
                break;
            case 0x3f: // BN4
                branchShort(!_inputNEF(3));
                break;
            CASE_16(0x40): // LDA Rn ; M(R(N)) → D; R(N) + 1 → R(N)
                _rD = readByte(RN()++);
                break;
            CASE_16(0x50): // STR Rn ; D → M(R(N))
                writeByte(RN(), _rD);
                break;
            case 0x60: // IRX ; R(X) + 1 → R(X)
                RX()++;
                break;
            CASE_7(0x61): { // OUT 1/7 ; M(R(X)) → BUS; R(X) + 1 → R(X); N LINES = N
                _output(_rN, readByte(RX()++));
                break;
            }
            case 0x68: _cpuState = eERROR; _errorMessage = "Illegal opcode 0x68!"; PC()--; break; // ILLEGAL (still behaving as NOP on the original CDP1802)
            CASE_7(0x69): { // INP 1/7 ; BUS → M(R(X)); BUS → D; N LINES = N
                _rD = _input(_rN&7);
                writeByte(RX(), _rD);
                break;
            }
            case 0x70: { // RET ; M(R(X)) → (X, P); R(X) + 1 → R(X), 1 → lE
                auto t = readByte(RX()++);
                _rP = t & 0xF;
                _rX = t >> 4;
                _rIE = true;
                break;
            }
            case 0x71: { // DIS ; M(R(X)) → (X, P); R(X) + 1 → R(X), 0 → lE
                auto t = readByte(RX()++);
                _rP = t & 0xF;
                _rX = t >> 4;
                _rIE = false;
                break;
            }
            case 0x72: // LDXA ; M(R(X)) → D; R(X) + 1 → R(X)
                _rD = readByte(RX()++);
                break;
            case 0x73: // STXD ; D → M(R(X)); R(X) - 1 → R(X)
                writeByte(RX()--, _rD);
                break;
            case 0x74: { // ADC ; M(R(X)) + D + DF → DF, D
                auto t = uint16_t(readByte(RX())) + _rD + _rDF;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            case 0x75: { // SDB ; M(R(X)) - D - (NOT DF) → DF, D
                auto t = uint16_t(readByte(RX())) + (_rD ^ 0xFF) + _rDF;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            case 0x76: { // SHRC ; SHIFT D RIGHT, LSB(D) → DF, DF → MSB(D)
                auto t = _rDF << 7;
                _rDF = _rD & 1;
                _rD = (_rD >> 1) | t;
                break;
            }
            case 0x77: { // SMB ; D-M(R(X))-(NOT DF) → DF, D
                auto t = uint16_t(readByte(RX()) ^ 0xFF) + _rD + _rDF;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            case 0x78: // SAV ; T → M(R(X))
                writeByte(RX(), _rT);
                break;
            case 0x79: // MARK ; (X, P) → T; (X, P) → M(R(2)), THEN P → X; R(2) - 1 → R(2)
                _rT = _rX << 4 | _rP;
                writeByte(_rR[2], _rT);
                _rX = _rP;
                _rR[2]--;
                break;
            case 0x7A: // REQ ; 0 → Q
                _rQ = false;
                break;
            case 0x7B: // SEQ ; 1 → Q
                _rQ = true;
                break;
            case 0x7C: { // ADCI ; M(R(P)) + D + DF → DF, D; R(P) + 1 → R(P)
                auto t = uint16_t(readByte(PC()++)) + _rD + _rDF;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            case 0x7D: { // SDBI ; M(R(P)) - D - (Not DF) → DF, D; R(P) + 1 → R(P)
                auto t = uint16_t(readByte(PC()++)) + (_rD ^ 0xFF) + _rDF;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            case 0x7E: { // SHLC ; SHIFT D LEFT, MSB(D) → DF, DF → LSB(D)
                auto t = _rDF;
                _rDF = _rD >> 7;
                _rD = (_rD << 1) | t;
                break;
            }
            case 0x7f: { // SMBI ; D-M(R(P))-(NOT DF) → DF, D; R(P) + 1 → R(P)
                auto t = uint16_t(readByte(PC()++) ^ 0xff) + _rD + _rDF;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            CASE_16(0x80): // GLO Rn ; R(N).0 → D
                _rD = RN() & 0xFF;
                break;
            CASE_16(0x90): // GHI Rn ; R(N).1 → D
                _rD = RN() >> 8;
                break;
            CASE_16(0xA0): // PLO Rn
                RN() = (RN() & 0xff00) | _rD;
                break;
            CASE_16(0xB0): // PHI Rn ; D → R(N).1
                RN() = (RN() & 0xFF) | (_rD << 8);
                break;
            case 0xC0: // LBR ; M(R(P)) → R(P). 1, M(R(P) + 1) → R(P).0
                branchLong(true);
                break;
            case 0xC1: // LBQ ; IF Q = 1, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(_rQ);
                break;
            case 0xC2: // LBZ ; IF D = 0, M(R(P)) → R(P).1, M(R(P) +1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(_rD == 0);
                break;
            case 0xC3: // LBDF ; IF DF = 1, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(_rDF);
                break;
            case 0xC4: // NOP ; CONTINUE
                addCycles(8);
                break;
            case 0xC5: // LSNQ ; IF Q = 0, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(!_rQ);
                break;
            case 0xC6: // LSNZ ; IF D Not 0, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(_rD != 0);
                break;
            case 0xC7: // LSNF ; IF DF = 0, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(_rDF == 0);
                break;
            case 0xC8: // LSKP ; R(P) + 2 → R(P)
                skipLong(true);
                break;
            case 0xC9: // LBNQ ; IF Q = 0, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0 EISE R(P) + 2 → R(P)
                branchLong(!_rQ);
                break;
            case 0xCA: // LBNZ ; IF D Not 0, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(_rD != 0);
                break;
            case 0xCB: // LBNF ; IF DF = 0, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(_rDF == 0);
                break;
            case 0xCC: // LSIE ; IF IE = 1, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(_rIE);
                break;
            case 0xCD: // LSQ ; IF Q = 1, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(_rQ);
                break;
            case 0xCE: // LSZ ; IF D = 0, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(_rD == 0);
                break;
            case 0xCF: // LSDF ; IF DF = 1, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(_rDF);
                break;
            CASE_16(0xD0): // SEP Rx
                _rP = _rN;
                break;
            CASE_16(0xE0): // SEX Rx
                _rX = _rN;
                break;
            case 0xF0: // LDX ; M(R(X)) → D
                _rD = readByte(RX());
                break;
            case 0xF1: // OR ; M(R(X)) OR D → D
                _rD |= readByte(RX());
                break;
            case 0xF2: // AND ; M(R(X)) AND D → D
                _rD &= readByte(RX());
                break;
            case 0xF3: // XOR ; M(R(X)) XOR D → D
                _rD ^= readByte(RX());
                break;
            case 0xF4: { // ADD ; M(R(X)) + D → DF, D
                auto t = uint16_t(readByte(RX())) + _rD;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            case 0xF5: { // SD ; M(R(X)) - D → DF, D
                auto t = uint16_t(readByte(RX())) + (_rD ^ 0xFF) + 1;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            case 0xF6: // SHR ; SHIFT D RIGHT, LSB(D) → DF, 0 → MSB(D)
                _rDF = _rD & 1;
                _rD >>= 1;
                break;
            case 0xF7: { // SM ; D-M(R(X)) → DF, D
                auto t = uint16_t(readByte(RX()) ^ 0xFF) + _rD + 1;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            case 0xF8: // LDI ; M(R(P)) → D; R(P) + 1 → R(P)
                _rD = readByte(PC()++);
                break;
            case 0xF9: // ORI ; M(R(P)) OR D → D; R(P) + 1 → R(P)
                _rD |= readByte(PC()++);
                break;
            case 0xFA: // ANI ; M(R(P)) AND D → D; R(P) + 1 → R(P)
                _rD &= readByte(PC()++);
                break;
            case 0xFB: // XRI ; M(R(P)) XOR D → D; R(P) + 1 → R(P)
                _rD ^= readByte(PC()++);
                break;
            case 0xFC: { // ADI ; M(R(P)) + D → DF, D; R(P) + 1 → R(P)
                auto t = readByte(PC()++) + _rD;
                _rDF = t >> 8;
                _rD = t;
                break;
            }
            case 0xFD: { // SDI ; M(R(P)) - D → DF, D; R(P) + 1 → R(P)
                auto t = uint16_t(readByte(PC()++)) + (_rD ^ 0xFF) + 1;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            case 0xFE: // SHL ; SHIFT D LEFT, MSB(D) → DF, 0 → LSB(D)
                _rDF = (_rD >> 7) & 1;
                _rD <<= 1;
                break;
            case 0xFF: { // SMI ; D-M(R(P)) → DF, D; R(P) + 1 → R(P)
                auto t = uint16_t(readByte(PC()++) ^ 0xFF) + _rD + 1;
                _rDF = (t >> 8) & 1;
                _rD = t;
                break;
            }
            default:
                _cpuState = eERROR;
                _errorMessage = "Internal error, default opcode case should not happen!";
                --PC();
                break;
        }
        if (_execMode == eSTEP || (_execMode == eSTEPOVER && _rR[_rP] >= _stepOverSP)) {
            _execMode = ePAUSED;
        }
        if(hasBreakPoint(getPC())) {
            if(findBreakpoint(getPC())) {
                _execMode = ePAUSED;
                _breakpointTriggered = true;
            }
        }
        return static_cast<int>(_cycles - startCycles);
    }
#ifdef CADMIUM_WITH_GENERIC_CPU
    ~Cdp1802() override = default;
    uint8_t readMemoryByte(uint32_t addr) const override { return _bus.readByteDMA(addr);
    }
    unsigned stackSize() const override { return 0; }
    StackContent stack() const override { return {}; }
    bool inErrorState() const override { return _cpuState == eERROR; }
    uint32_t cpuID() const override { return 1802; }
    std::string name() const override { static const std::string name = "CDP1802"; return name; }
    const std::vector<std::string>& registerNames() const override
    {
        static const std::vector<std::string> registerNames = {
            "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7",
            "R8", "R9", "RA", "RB", "RC", "RD", "RE", "RF",
            "I", "N", "P", "X", "D", "DF", "T", "IE", "Q"
        };
        return registerNames;
    }
    size_t numRegisters() const override { return 25; }
    RegisterValue registerbyIndex(size_t index) const override
    {
        switch(index) {
            case 0: return {_rR[0], 16};
            case 1: return {_rR[1], 16};
            case 2: return {_rR[2], 16};
            case 3: return {_rR[3], 16};
            case 4: return {_rR[4], 16};
            case 5: return {_rR[5], 16};
            case 6: return {_rR[6], 16};
            case 7: return {_rR[7], 16};
            case 8: return {_rR[8], 16};
            case 9: return {_rR[9], 16};
            case 10: return {_rR[10], 16};
            case 11: return {_rR[11], 16};
            case 12: return {_rR[12], 16};
            case 13: return {_rR[13], 16};
            case 14: return {_rR[14], 16};
            case 15: return {_rR[15], 16};
            case 16: return {_rI, 4};
            case 17: return {_rN, 4};
            case 18: return {_rP, 4};
            case 19: return {_rX, 4};
            case 20: return {_rD, 8};
            case 21: return {_rDF, 1};
            case 22: return {_rT, 8};
            case 23: return {_rIE, 1};
            case 24: return {_rQ, 1};
            default: return {0,0};
        }
    }
    void setRegister(size_t index, uint32_t value) override
    {
        switch(index) {
            case 0: _rR[0] = static_cast<uint16_t>(value); break;
            case 1: _rR[1] = static_cast<uint16_t>(value); break;
            case 2: _rR[2] = static_cast<uint16_t>(value); break;
            case 3: _rR[3] = static_cast<uint16_t>(value); break;
            case 4: _rR[4] = static_cast<uint16_t>(value); break;
            case 5: _rR[5] = static_cast<uint16_t>(value); break;
            case 6: _rR[6] = static_cast<uint16_t>(value); break;
            case 7: _rR[7] = static_cast<uint16_t>(value); break;
            case 8: _rR[8] = static_cast<uint16_t>(value); break;
            case 9: _rR[9] = static_cast<uint16_t>(value); break;
            case 10: _rR[12] = static_cast<uint16_t>(value); break;
            case 11: _rR[11] = static_cast<uint16_t>(value); break;
            case 12: _rR[12] = static_cast<uint16_t>(value); break;
            case 13: _rR[13] = static_cast<uint16_t>(value); break;
            case 14: _rR[14] = static_cast<uint16_t>(value); break;
            case 15: _rR[15] = static_cast<uint16_t>(value); break;
            case 16: _rI = value & 0xF; break;
            case 17: _rN = value & 0xF; break;
            case 18: _rP = value & 0xF; break;
            case 19: _rX = value & 0xF; break;
            case 20: _rD = static_cast<uint8_t>(value); break;
            case 21: _rDF = value != 0; break;
            case 22: _rT = static_cast<uint8_t>(value); break;
            case 23: _rIE = value != 0; break;
            case 24: _rQ = value != 0; break;
            default: break;
        }
    }
    uint32_t getPC() const override
    {
        return _rR[_rP];
    }

    uint32_t getSP() const override
    {
        return _rR[2];
    }
#endif
private:
    Cdp1802Bus& _bus;
    OutputHandler _output;
    InputHandler _input;
    NEFInputHandler _inputNEF;
    CpuState _cpuState{eNORMAL};
    uint8_t _rD{};
    bool _rDF{};
    uint16_t _rR[16]{};
    uint16_t _rP:4{};
    uint16_t _rX:4{};
    uint16_t _rN:4{};
    uint16_t _rI:4{};
    uint8_t _rT{};
    bool _rIE{false};
    bool _rQ{false};
    bool _irq{false};
    int64_t _cycles{};
    int64_t _idleCycles{};
    int64_t _irqCycles{};
    ClockedTime _systemTime;
};

}
