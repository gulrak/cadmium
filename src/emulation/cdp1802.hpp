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
    Cdp1802(Cdp1802Bus& bus, Time::ticks_t clockFreq = 1760900)
        : _bus(bus)
        , _clockSpeed(clockFreq)
    {
        reset();
    }
    ~Cdp1802() {}

    void reset()
    {
        rI = 0;
        rN = 0;
        rP = 0;
        rQ = false;
        rX = 0;
        rR[0] = 0;
        rR[1] = 0xfff;
        rIE = true;
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

    void triggerIrq() { _irq = true; }
    void setEF(int idx, bool value) { ioEF[idx&3] = value; }
    bool getEF(int idx, bool value) const { return ioEF[idx&3]; }
    uint16_t getR(uint8_t index) const { return rR[index & 0xf]; }
    bool getIE() const { return rIE; }
    int64_t getCycles() const { return _cycles; }
    uint16_t& PC() { return rR[rP]; }
    uint16_t& RN() { return rR[rN]; }
    uint16_t& RX() { return rR[rX]; }
    uint8_t readByte(uint16_t addr) { return _bus.readByte(addr); /*_memory[addr & 0xFFF];*/ }
    void writeByte(uint16_t addr, uint8_t val) { _bus.writeByte(addr, val); /*_memory[addr & 0xFFF] = val;*/ }
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
       data[0] = _bus.readByte(rR[rP]);
       data[1] = _bus.readByte(rR[rP]+1);
       data[2] = _bus.readByte(rR[rP]+2);
       auto [size, text] = Cdp1802::disassembleInstruction(data, data+3);
       return fmt::format("{:04x}:  {:02x}    {}", rR[rP], data[0], text);
    }

    std::string dumpStateLine() const
    {
        return fmt::format("R0:{:04x} R1:{:04x} R2:{:04x} R3:{:04x} R4:{:04x} R5:{:04x} R6:{:04x} R7:{:04x} R8:{:04x} R9:{:04x} RA:{:04x} RB:{:04x} RC:{:04x} RD:{:04x} RE:{:04x} RF:{:04x} D:{:02x} P:{:1x} X:{:1x} N:{:1x} I:{:1x} T:{:02x} PC:{:04x} O:{:02x}", getR(0), getR(1), getR(2),
                           getR(3), getR(4), getR(5), getR(6), getR(7), getR(8), getR(9), getR(10), getR(11), getR(12), getR(13), getR(14), getR(15), rD, rP, rX, rN, rI, rT, rR[rP], _bus.readByte(rR[rP]));
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

    void handleInterrupt()
    {
        //std::clog << "CDP1802: --- IRQ ---" << std::endl;
        rIE = false;
        rT = (rX<<4)|rP;
        rP = 1;
        rX = 2;
    }

    void executeInstruction()
    {
        if(_irq) {
            if(rIE)
                handleInterrupt();
            _irq = false;
        }
        //if(!rIE)
        //    std::clog << "CDP1802: " << disassembleCurrentStatement() << std::endl;
        uint8_t opcode = readByte(PC()++);
        addCycles(16);
        rN = opcode & 0xF;
        switch (opcode) {
            case 0x00: // IDL ; WAIT FOR DMA OR INTERRUPT; M(R(0)) → BUS
                _execMode = eIDLE;
                break;
            CASE_15(0x01): // LDN Rn ; M(R(N)) → D; FOR N not 0
                rD = readByte(RN());
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
                branchShort(rQ);
                break;
            case 0x32: // BZ
                branchShort(!rD);
                break;
            case 0x33: // BDF
                branchShort(rDF);
                break;
            case 0x34: // B1
                branchShort(ioEF[0]);
                break;
            case 0x35: // B2
                branchShort(ioEF[1]);
                break;
            case 0x36: // B3
                branchShort(ioEF[2]);
                break;
            case 0x37: // B4
                branchShort(ioEF[3]);
                break;
            case 0x38: // SKP
                PC()++;
                break;
            case 0x39: // BNQ
                branchShort(!rQ);
                break;
            case 0x3A: // BNZ
                branchShort(rD != 0);
                break;
            case 0x3B: // BNF
                branchShort(!rDF);
                break;
            case 0x3c: // BN1
                branchShort(!ioEF[0]);
                break;
            case 0x3d: // BN2
                branchShort(!ioEF[1]);
                break;
            case 0x3e: // BN3
                branchShort(!ioEF[2]);
                break;
            case 0x3f: // BN4
                branchShort(!ioEF[3]);
                break;
            CASE_16(0x40): // LDA Rn ; M(R(N)) → D; R(N) + 1 → R(N)
                rD = readByte(RN()++);
                break;
            CASE_16(0x50): // STR Rn ; D → M(R(N))
                writeByte(RN(), rD);
                break;
            case 0x60: // IRX ; R(X) + 1 → R(X)
                RX()++;
                break;
            CASE_7(0x61): { // OUT 1/7 ; M(R(X)) → BUS; R(X) + 1 → R(X); N LINES = N
                _output(rN, readByte(RX()++));
                /*                switch (opcode & 7) {
                                    case 1:                 // pixie graphics off
                                        vip.pixie_on = 0;
                                        break;
                                    case 2:                 // hex keypad latch
                                        vip.key = r & 0xf;
                                        break;
                                    case 4:                 // reset address latch (unneeded)
                                        break;
                                    default:
                                        break;
                                }*/
                break;
            }
            case 0x68: break; // ILLEGAL
            CASE_7(0x69): { // INP 1/7 ; BUS → M(R(X)); BUS → D; N LINES = N
                /*
                uint8_t r = opcode;
                switch (opcode & 7) {
                    case 1:
                        vip.pixie_on = 1;   // pixie graphics on
                        break;
                    default:
                        break;
                }*/
                rD = _input(rN&7);
                writeByte(RX(), rD);
                break;
            }
            case 0x70: { // RET ; M(R(X)) → (X, P); R(X) + 1 → R(X), 1 → lE
                auto t = readByte(RX()++);
                rP = t & 0xF;
                rX = t >> 4;
                rIE = true;
                break;
            }
            case 0x71: { // DIS ; M(R(X)) → (X, P); R(X) + 1 → R(X), 0 → lE
                auto t = readByte(RX()++);
                rP = t & 0xF;
                rX = t >> 4;
                rIE = false;
                break;
            }
            case 0x72: // LDXA ; M(R(X)) → D; R(X) + 1 → R(X)
                rD = readByte(RX()++);
                break;
            case 0x73: // STXD ; D → M(R(X)); R(X) - 1 → R(X)
                writeByte(RX()--, rD);
                break;
            case 0x74: { // ADC ; M(R(X)) + D + DF → DF, D
                auto t = uint16_t(readByte(RX())) + rD + rDF;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
            case 0x75: { // SDB ; M(R(X)) - D - (NOT DF) → DF, D
                auto t = uint16_t(readByte(RX())) + (rD ^ 0xFF) + rDF;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
            case 0x76: { // SHRC ; SHIFT D RIGHT, LSB(D) → DF, DF → MSB(D)
                auto t = rDF << 7;
                rDF = rD & 1;
                rD = (rD >> 1) | t;
                break;
            }
            case 0x77: { // SMB ; D-M(R(X))-(NOT DF) → DF, D
                auto t = uint16_t(readByte(RX()) ^ 0xFF) + rD + rDF;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
            case 0x78: // SAV ; T → M(R(X))
                writeByte(RX(), rT);
                break;
            case 0x79: // MARK ; (X, P) → T; (X, P) → M(R(2)), THEN P → X; R(2) - 1 → R(2)
                rT = rX << 4 | rP;
                writeByte(rR[2], rT);
                rX = rP;
                rR[2]--;
                break;
            case 0x7A: // REQ ; 0 → Q
                rQ = false;
                break;
            case 0x7B: // SEQ ; 1 → Q
                rQ = true;
                break;
            case 0x7C: { // ADCI ; M(R(P)) + D + DF → DF, D; R(P) + 1 → R(P)
                auto t = uint16_t(readByte(PC()++)) + rD + rDF;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
            case 0x7D: { // SDBI ; M(R(P)) - D - (Not DF) → DF, D; R(P) + 1 → R(P)
                auto t = uint16_t(readByte(PC()++)) + (rD ^ 0xFF) + rDF;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
            case 0x7E: { // SHLC ; SHIFT D LEFT, MSB(D) → DF, DF → LSB(D)
                auto t = rDF;
                rDF = rD >> 7;
                rD = (rD << 1) | t;
                break;
            }
            case 0x7f: { // SMBI ; D-M(R(P))-(NOT DF) → DF, D; R(P) + 1 → R(P)
                auto t = uint16_t(readByte(PC()++) ^ 0xff) + rD + rDF;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
            CASE_16(0x80): // GLO Rn ; R(N).0 → D
                rD = RN() & 0xFF;
                break;
            CASE_16(0x90): // GHI Rn ; R(N).1 → D
                rD = RN() >> 8;
                break;
            CASE_16(0xA0): // PLO Rn
                RN() = (RN() & 0xff00) | rD;
                break;
            CASE_16(0xB0): // PHI Rn ; D → R(N).1
                RN() = (RN() & 0xFF) | (rD << 8);
                break;
            case 0xC0: // LBR ; M(R(P)) → R(P). 1, M(R(P) + 1) → R(P).0
                branchLong(true);
                break;
            case 0xC1: // LBQ ; IF Q = 1, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(rQ);
                break;
            case 0xC2: // LBZ ; IF D = 0, M(R(P)) → R(P).1, M(R(P) +1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(!rD);
                break;
            case 0xC3: // LBDF ; IF DF = 1, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(rDF);
                break;
            case 0xC4: // NOP ; CONTINUE
                addCycles(8);
                break;
            case 0xC5: // LSNQ ; IF Q = 0, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(!rQ);
                break;
            case 0xC6: // LSNZ ; IF D Not 0, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(rD == 0);
                break;
            case 0xC7: // LSNF ; IF DF = 0, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(rDF == 0);
                break;
            case 0xC8: // LSKP ; R(P) + 2 → R(P)
                skipLong(true);
                break;
            case 0xC9: // LBNQ ; IF Q = 0, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0 EISE R(P) + 2 → R(P)
                branchLong(rQ);
                break;
            case 0xCA: // LBNZ ; IF D Not 0, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(rD);
                break;
            case 0xCB: // LBNF ; IF DF = 0, M(R(P)) → R(P).1, M(R(P) + 1) → R(P).0, ELSE R(P) + 2 → R(P)
                branchLong(rDF);
                break;
            case 0xCC: // LSIE ; IF IE = 1, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(rIE);
                break;
            case 0xCD: // LSQ ; IF Q = 1, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(rQ);
                break;
            case 0xCE: // LSZ ; IF D = 0, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(rD);
                break;
            case 0xCF: // LSDF ; IF DF = 1, R(P) + 2 → R(P), ELSE CONTINUE
                skipLong(rDF);
                break;
            CASE_16(0xD0): // SEP Rx
                rP = rN;
                break;
            CASE_16(0xE0): // SEX Rx
                rX = rN;
                break;
            case 0xF0: // LDX ; M(R(X)) → D
                rD = readByte(RX());
                break;
            case 0xF1: // OR ; M(R(X)) OR D → D
                rD |= readByte(RX());
                break;
            case 0xF2: // AND ; M(R(X)) AND D → D
                rD &= readByte(RX());
                break;
            case 0xF3: // XOR ; M(R(X)) XOR D → D
                rD ^= readByte(RX());
                break;
            case 0xF4: { // ADD ; M(R(X)) + D → DF, D
                auto t = uint16_t(readByte(RX())) + rD;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
            case 0xF5: { // SD ; M(R(X)) - D → DF, D
                auto t = uint16_t(readByte(RX())) + (rD ^ 0xFF) + 1;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
            case 0xF6: // SHR ; SHIFT D RIGHT, LSB(D) → DF, 0 → MSB(D)
                rDF = rD & 1;
                rD >>= 1;
                break;
            case 0xF7: { // SM ; D-M(R(X)) → DF, D
                auto t = uint16_t(readByte(RX()) ^ 0xFF) + rD + 1;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
            case 0xF8: // LDI ; M(R(P)) → D; R(P) + 1 → R(P)
                rD = readByte(PC()++);
                break;
            case 0xF9: // ORI ; M(R(P)) OR D → D; R(P) + 1 → R(P)
                rD |= readByte(PC()++);
                break;
            case 0xFA: // ANI ; M(R(P)) AND D → D; R(P) + 1 → R(P)
                rD &= readByte(PC()++);
                break;
            case 0xFB: // XRI ; M(R(P)) XOR D → D; R(P) + 1 → R(P)
                rD ^= readByte(PC()++);
                break;
            case 0xFC: { // ADI ; M(R(P)) + D → DF, D; R(P) + 1 → R(P)
                auto t = readByte(PC()++) + rD;
                rDF = t >> 8;
                rD = t;
                break;
            }
            case 0xFD: { // SDI ; M(R(P)) - D → DF, D; R(P) + 1 → R(P)
                auto t = uint16_t(readByte(PC()++)) + (rD ^ 0xFF) + 1;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
            case 0xFE: // SHL ; SHIFT D LEFT, MSB(D) → DF, 0 → LSB(D)
                rDF = (rD >> 7) & 1;
                rD <<= 1;
                break;
            case 0xFF: { // SMI ; D-M(R(P)) → DF, D; R(P) + 1 → R(P)
                auto t = uint16_t(readByte(PC()++) ^ 0xFF) + rD + 1;
                rDF = (t >> 8) & 1;
                rD = t;
                break;
            }
        }
        //if(!rIE)
        //    std::clog << "CDP1802: " << dumpStateLine() << std::endl;
    }
private:
    Cdp1802Bus& _bus;
    OutputHandler _output;
    InputHandler _input;
    ExecMode _execMode{eNORMAL};
    uint8_t rD{};
    bool rDF{};
    uint8_t rB{};
    uint16_t rR[16]{};
    uint16_t rP:4;
    uint16_t rX:4;
    uint16_t rN:4;
    uint16_t rI:4;
    uint8_t rT{};
    bool rIE{false};
    bool rQ{false};
    bool ioEF[4]{};
    bool _irq{false};
    int64_t _cycles;
    Time::ticks_t _clockSpeed{};
    Time _systemTime;
};

}
