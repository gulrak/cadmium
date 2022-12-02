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

#include <emulation/time.hpp>

#include <fmt/format.h>

#include <cstdint>
#include <functional>
#include <utility>
#include <iostream>

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
    virtual void writeByte(uint16_t addr, uint8_t val) = 0;
};

class Cdp1802
{
public:
    enum ExecMode { eNORMAL, eIDLE, eHALT };
    struct Disassembled {
        int size;
        std::string text;
    };
    using OutputHandler = std::function<void(uint8_t, uint8_t)>;
    using InputHandler = std::function<uint8_t (uint8_t)>;
    using NEFInputHandler = std::function<bool (uint8_t)>;
    Cdp1802(Cdp1802Bus& bus, Time::ticks_t clockFreq = 1760900)
        : _bus(bus)
        , _clockSpeed(clockFreq)
    {
        _output = [](uint8_t, uint8_t){};
        _input = [](uint8_t){ return 0; };
        _inputNEF = [](uint8_t) { return true; };
        reset();
    }
    ~Cdp1802() {}

    void reset()
    {
        _rI = 0;
        _rN = 0;
        _rP = 0;
        _rQ = false;
        _rX = 0;
        _rR[0] = 0;
        _rR[1] = 0xfff;
        _rIE = true;
        _cycles = 0;
        _systemTime = Time::zero;
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
    bool getIE() const { return _rIE; }
    int64_t getCycles() const { return _cycles; }
    uint16_t& PC() { return _rR[_rP]; }
    uint8_t getN() const { return _rN; }
    uint8_t getP() const { return _rP; }
    uint8_t getX() const { return _rX; }
    uint8_t getD() const { return _rD; }
    uint8_t getDF() const { return _rDF ? 1 : 0; }
    uint8_t getT() const { return _rT; }
    uint16_t& RN() { return _rR[_rN]; }
    uint16_t& RX() { return _rR[_rX]; }
    uint8_t readByte(uint16_t addr) { return _bus.readByte(addr); }
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
        _systemTime.addCycles(cycles, _clockSpeed);
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

    std::string disassembleCurrentStatement() const
    {
       uint8_t data[3];
       data[0] = _bus.readByte(_rR[_rP]);
       data[1] = _bus.readByte(_rR[_rP]+1);
       data[2] = _bus.readByte(_rR[_rP]+2);
       auto [size, text] = Cdp1802::disassembleInstruction(data, data+3);
       return fmt::format("{:04x}:  {:02x}    {}", _rR[_rP], data[0], text);
    }

    std::string dumpStateLine() const
    {
        return fmt::format("R0:{:04x} R1:{:04x} R2:{:04x} R3:{:04x} R4:{:04x} R5:{:04x} R6:{:04x} R7:{:04x} R8:{:04x} R9:{:04x} RA:{:04x} RB:{:04x} RC:{:04x} RD:{:04x} RE:{:04x} RF:{:04x} D:{:02x} P:{:1x} X:{:1x} N:{:1x} I:{:1x} T:{:02x} PC:{:04x} O:{:02x}", getR(0), getR(1), getR(2),
                           getR(3), getR(4), getR(5), getR(6), getR(7), getR(8), getR(9), getR(10), getR(11), getR(12), getR(13), getR(14), getR(15), _rD, _rP, _rX, _rN, _rI, _rT, _rR[_rP], _bus.readByte(_rR[_rP]));
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
            case 0x7C: return {1, "ADCI"};
            case 0x7D: return {1, "SDBI"};
            case 0x7E: return {1, "SHLC"};
            case 0x7f: return {1, "SMBI"};
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
        std::clog << fmt::format("CDP1802: [{:9}]  ", _cycles) << "--- IRQ ---" << std::endl;
        addCycles(8);
        _rIE = false;
        _rT = (_rX<<4)|_rP;
        _rP = 1;
        _rX = 2;
    }

    void executeDMAIn(uint8_t data)
    {
        addCycles(8);
        writeByte(_rR[0]++, data);
    }

    uint8_t executeDMAOut()
    {
        addCycles(8);
        return readByte(_rR[0]++);
    }

    void executeInstruction()
    {
        if(!_rIE)
            std::clog << fmt::format("CDP1802: [{:9}] {:<24} | ", _cycles, disassembleCurrentStatement()) << dumpStateLine() << std::endl;
        uint8_t opcode = readByte(PC()++);
        addCycles(16);
        _rN = opcode & 0xF;
        switch (opcode) {
            case 0x00: // IDL ; WAIT FOR DMA OR INTERRUPT; M(R(0)) → BUS
                _execMode = eIDLE;
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
            case 0x68: break; // ILLEGAL
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
                branchLong(!_rD);
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
                skipLong(_rD == 0);
                break;
            case 0xC7: // LSNF ; IF DF = 0, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(_rDF == 0);
                break;
            case 0xC8: // LSKP ; R(P) + 2 → R(P)
                skipLong(true);
                break;
            case 0xC9: // LBNQ ; IF Q = 0, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0 EISE R(P) + 2 → R(P)
                branchLong(_rQ);
                break;
            case 0xCA: // LBNZ ; IF D Not 0, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(_rD);
                break;
            case 0xCB: // LBNF ; IF DF = 0, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(_rDF);
                break;
            case 0xCC: // LSIE ; IF IE = 1, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(_rIE);
                break;
            case 0xCD: // LSQ ; IF Q = 1, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(_rQ);
                break;
            case 0xCE: // LSZ ; IF D = 0, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(_rD);
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
        }
        //if(!_rIE)
        //    std::clog << "CDP1802: " << dumpStateLine() << std::endl;
    }
private:
    Cdp1802Bus& _bus;
    OutputHandler _output;
    InputHandler _input;
    NEFInputHandler _inputNEF;
    ExecMode _execMode{eNORMAL};
    uint8_t _rD{};
    bool _rDF{};
    uint8_t _rB{};
    uint16_t _rR[16]{};
    uint16_t _rP:4;
    uint16_t _rX:4;
    uint16_t _rN:4;
    uint16_t _rI:4;
    uint8_t _rT{};
    bool _rIE{false};
    bool _rQ{false};
    bool _irq{false};
    int64_t _cycles;
    Time::ticks_t _clockSpeed{};
    Time _systemTime;
};

}
