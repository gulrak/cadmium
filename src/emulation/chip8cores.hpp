//---------------------------------------------------------------------------------------
// src/emulation/chip8.hpp
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
// sound def:
// 0    tone in quarter halftones
// 1    pulse width
// 2    control (sin|saw|rect|noise)
// 3    attack/decay
// 4    sustain/release
// 5    filter cu-off
// 6    lp/bp/hp / resonance

#pragma once

#include <chiplet/chip8meta.hpp>
#include <emulation/chip8options.hpp>
#include <emulation/chip8emulatorbase.hpp>
#include <emulation/time.hpp>

namespace emu
{

//---------------------------------------------------------------------------------------
// ChipEmulator - a templated switch based CHIP-8 core
//---------------------------------------------------------------------------------------
template<uint16_t addressLines = 12, uint16_t quirks = 0>
class Chip8Emulator : public Chip8EmulatorBase
{
public:
    using Chip8EmulatorBase::ExecMode;
    using Chip8EmulatorBase::CpuState;
    constexpr static uint16_t ADDRESS_MASK = (1<<addressLines)-1;
    constexpr static uint32_t MEMORY_SIZE = 1<<addressLines;
    constexpr static int SCREEN_WIDTH = quirks&HiresSupport ? 128 : 64;
    constexpr static int SCREEN_HEIGHT = quirks&HiresSupport ? 64 : 32;

    Chip8Emulator(Chip8EmulatorHost& host, Chip8EmulatorOptions& options, IChip8Emulator* other = nullptr)
    : Chip8EmulatorBase(host, options, other)
    {
        _memory.resize(MEMORY_SIZE, 0);
    }
    ~Chip8Emulator() override = default;

    std::string name() const override
    {
        return "Chip-8-TS";
    }

    inline uint16_t readWord(uint32_t addr) const
    {
        return (_memory[addr] << 8) | _memory[addr + 1];
    }

    void executeInstruction() override
    {
        if (_execMode == ePAUSED || _cpuState == eERROR)
            return;
        uint16_t opcode = readWord(_rPC);
        _rPC = (_rPC + 2) & ADDRESS_MASK;
        ++_cycleCounter;
        addCycles(68);
        switch (opcode >> 12) {
            case 0:
                if((opcode & 0xfff0) == 0x00C0) { // scroll-down
                    auto n = (opcode & 0xf);
                    _screen.scrollDown(n);
                }
                else if((opcode & 0xfff0) == 0x00D0) { // scroll-up
                    auto n = (opcode & 0xf);
                    _screen.scrollUp(n);
                }
                else if (opcode == 0x00E0) {  // 00E0 - CLS
                    clearScreen();
                    ++_clearCounter;
                    addCycles(24);
                }
                else if (opcode == 0x00EE) {  // 00EE - RET
                    _rPC = _stack[--_rSP];
                    addCycles(10);
                    if (_execMode == eSTEPOUT)
                        _execMode = ePAUSED;
                }
                else if(opcode == 0x00FB) { // scroll-right
                    _screen.scrollRight(4);
                }
                else if(opcode == 0x00FC) { // scroll-left
                    _screen.scrollLeft(4);
                }
                else if(opcode == 0x00FD) {
                    halt();
                }
                else if(opcode == 0x00FE && _options.optAllowHires && !_options.optOnlyHires) { // LORES
                    _isHires = false;
                }
                else if(opcode == 0x00FF && _options.optAllowHires) { // HIRES
                    _isHires = true;
                }
                break;
            case 1:  // 1nnn - JP addr
                if((opcode & 0xFFF) == _rPC - 2)
                    _execMode = ePAUSED; 
                _rPC = opcode & 0xFFF;
                addCycles(12);
                break;
            case 2:  // 2nnn - CALL addr
                _stack[_rSP++] = _rPC;
                _rPC = opcode & 0xFFF;
                addCycles(26);
                break;
            case 3:  // 3xkk - SE Vx, byte
                if (_rV[(opcode >> 8) & 0xF] == (opcode & 0xff)) {
                    _rPC += 2;
                    addCycles(14);
                }
                else {
                    addCycles(10);
                }
                break;
            case 4:  // 4xkk - SNE Vx, byte
                if (_rV[(opcode >> 8) & 0xF] != (opcode & 0xFF)) {
                    _rPC += 2;
                    addCycles(14);
                }
                else {
                    addCycles(10);
                }
                break;
            case 5: {
                switch (opcode & 0xF) {
                    case 0: // 5xy0 - SE Vx, Vy
                        if (_rV[(opcode >> 8) & 0xF] == _rV[(opcode >> 4) & 0xF]) {
                            _rPC += 2;
                            addCycles(18);
                        }
                        else {
                            addCycles(14);
                        }
                        break;
                    case 2: {  // 5xy2  - save vx - vy
                        auto x = (opcode >> 8) & 0xF;
                        auto y = (opcode >> 4) & 0xF;
                        auto l = std::abs(x-y);
                        for(int i=0; i <= l; ++i)
                            _memory[(_rI + i) & ADDRESS_MASK] = _rV[x < y ? x + i : x - i];
                        if(_rI + l >= ADDRESS_MASK)
                            fixupSafetyPad();
                        break;
                    }
                    case 3: {  // 5xy3 - load vx - vy
                        auto x = (opcode >> 8) & 0xF;
                        auto y = (opcode >> 4) & 0xF;
                        for(int i=0; i <= std::abs(x-y); ++i)
                            _rV[x < y ? x + i : x - i] = _memory[(_rI + i) & ADDRESS_MASK];
                        break;
                    }
                    case 4: { // palette x y
                        auto x = (opcode >> 8) & 0xF;
                        auto y = (opcode >> 4) & 0xF;
                        for(int i=0; i <= std::abs(x-y); ++i)
                            _xxoPalette[x < y ? x + i : x - i] = _memory[(_rI + i) & ADDRESS_MASK];
                        _host.updatePalette(_xxoPalette);
                        break;
                    }
                    default:
                        errorHalt();
                        break;
                }
                break;
            }
            case 6:  // 6xkk - LD Vx, byte
                _rV[(opcode >> 8) & 0xF] = opcode & 0xFF;
                addCycles(6);
                break;
            case 7:  // 7xkk - ADD Vx, byte
                _rV[(opcode >> 8) & 0xF] += opcode & 0xFF;
                addCycles(10);
                break;
            case 8: {
                switch (opcode & 0xF) {
                    case 0:  // 8xy0 - LD Vx, Vy
                        _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF];
                        addCycles(12);
                        break;
                    case 1:  // 8xy1 - OR Vx, Vy
                        _rV[(opcode >> 8) & 0xF] |= _rV[(opcode >> 4) & 0xF];
                        if (!_options.optDontResetVf)
                            _rV[0xF] = 0;
                        break;
                    case 2:  // 8xy2 - AND Vx, Vy
                        _rV[(opcode >> 8) & 0xF] &= _rV[(opcode >> 4) & 0xF];
                        if (!_options.optDontResetVf)
                            _rV[0xF] = 0;
                        break;
                    case 3:  // 8xy3 - XOR Vx, Vy
                        _rV[(opcode >> 8) & 0xF] ^= _rV[(opcode >> 4) & 0xF];
                        if (!_options.optDontResetVf)
                            _rV[0xF] = 0;
                        break;
                    case 4: {  // 8xy4 - ADD Vx, Vy
                        uint16_t result = _rV[(opcode >> 8) & 0xF] + _rV[(opcode >> 4) & 0xF];
                        _rV[(opcode >> 8) & 0xF] = result;
                        _rV[0xF] = result>>8;
                        addCycles(44);
                        break;
                    }
                    case 5: {  // 8xy5 - SUB Vx, Vy
                        uint16_t result = _rV[(opcode >> 8) & 0xF] - _rV[(opcode >> 4) & 0xF];
                        _rV[(opcode >> 8) & 0xF] = result;
                        _rV[0xF] = result > 255 ? 0 : 1;
                        addCycles(44);
                        break;
                    }
                    case 6:  // 8xy6 - SHR Vx, Vy
                        if (!_options.optJustShiftVx) {
                            uint8_t carry = _rV[(opcode >> 4) & 0xF] & 1;
                            _rV[(opcode >> 8) & 0xF] /*= rV[(opcode >> 4) & 0xF]*/ = _rV[(opcode >> 4) & 0xF] >> 1;
                            _rV[0xF] = carry;
                        }
                        else {
                            uint8_t carry = _rV[(opcode >> 8) & 0xF] & 1;
                            _rV[(opcode >> 8) & 0xF] >>= 1;
                            _rV[0xF] = carry;
                        }
                        addCycles(44);
                        break;
                    case 7: {  // 8xy7 - SUBN Vx, Vy
                        uint16_t result = _rV[(opcode >> 4) & 0xF] - _rV[(opcode >> 8) & 0xF];
                        _rV[(opcode >> 8) & 0xF] = result;
                        _rV[0xF] = result > 255 ? 0 : 1;
                        addCycles(44);
                        break;
                    }
                    case 0xE:  // 8xyE - SHL Vx, Vy
                        if (!_options.optJustShiftVx) {
                            uint8_t carry = _rV[(opcode >> 4) & 0xF] >> 7;
                            _rV[(opcode >> 8) & 0xF] /*= rV[(opcode >> 4) & 0xF]*/ = _rV[(opcode >> 4) & 0xF] << 1;
                            _rV[0xF] = carry;
                        }
                        else {
                            uint8_t carry = _rV[(opcode >> 8) & 0xF] >> 7;
                            _rV[(opcode >> 8) & 0xF] <<= 1;
                            _rV[0xF] = carry;
                        }
                        addCycles(44);
                        break;
                    default:
                        errorHalt();
                        break;
                }
                break;
            }
            case 9:  // 9xy0 - SNE Vx, Vy
                if (_rV[(opcode >> 8) & 0xF] != _rV[(opcode >> 4) & 0xF]) {
                    _rPC += 2;
                    addCycles(18);
                }
                else {
                    addCycles(14);
                }
                break;
            case 0xA:  // Annn - LD I, addr
                _rI = opcode & 0xFFF;
                addCycles(12);
                break;
            case 0xB:  // Bnnn - JP V0, addr / Bxnn - JP Vx, addr
                _rPC = _options.optJump0Bxnn ? (_rV[(opcode >> 8) & 0xF] + (opcode & 0xFFF)) & ADDRESS_MASK : (_rV[0] + (opcode & 0xFFF)) & ADDRESS_MASK;
                addCycles(22); // TODO: Check page crossing +2MC
                break;
            case 0xC: {  // Cxkk - RND Vx, byte
                ++_randomSeed;
                uint16_t val = _randomSeed>>8;
                val += _chip8_cosmac_vip[0x100 + (_randomSeed&0xFF)];
                uint8_t result = val;
                val >>= 1;
                val += result;
                _randomSeed = (_randomSeed & 0xFF) | (val << 8);
                result = val & (opcode & 0xFF);
                _rV[(opcode >> 8) & 0xF] = result; // GetRandomValue(0, 255) & (opcode & 0xFF);
                addCycles(36);
                break;
            }
            case 0xD: {  // Dxyn - DRW Vx, Vy, nibble
                if constexpr (quirks&HiresSupport) {
                    if(_isHires)
                    {
                        int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH - 1);
                        int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT - 1);
                        int lines = opcode & 0xF;
                        _rV[15] = drawSprite(x, y, &_memory[_rI & ADDRESS_MASK], lines, true) ? 1 : 0;
                    }
                    else
                    {
                        int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH / 2 - 1);
                        int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT / 2 - 1);
                        int lines = opcode & 0xF;
                        _rV[15] = drawSprite(x*2, y*2, &_memory[_rI & ADDRESS_MASK], lines, false) ? 1 : 0;
                    }
                }
                else {
                    int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH - 1);
                    int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT - 1);
                    int lines = opcode & 0xF;
                    _rV[15] = drawSprite(x, y, &_memory[_rI & ADDRESS_MASK], lines, false) ? 1 : 0;
                }
                break;
            }
            case 0xE:
                if ((opcode & 0xff) == 0x9E) {  // Ex9E - SKP Vx
                    if (_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
                        _rPC += 2;
                        addCycles(18);
                    }
                    else {
                        addCycles(14);
                    }
                }
                else if ((opcode & 0xff) == 0xA1) {  // ExA1 - SKNP Vx
                    if (_host.isKeyUp(_rV[(opcode >> 8) & 0xF] & 0xF)) {
                        _rPC += 2;
                        addCycles(18);
                    }
                    else {
                        addCycles(14);
                    }
                }
                break;
            case 0xF: {
                addCycles(4);
                switch (opcode & 0xFF) {
                    case 0x00: // i := long nnnn
                        if constexpr (addressLines == 16) {
                            _rI = ((_memory[_rPC & ADDRESS_MASK] << 8) | _memory[(_rPC + 1) & ADDRESS_MASK]) & ADDRESS_MASK;
                            _rPC = (_rPC + 2) & ADDRESS_MASK;
                        }
                        else {
                            errorHalt();
                        }
                        break;
                    case 0x01: // Fx01 - planes x
                        _planes = (opcode >> 8) & 0xF;
                        break;
                    case 0x02: // F002 - audio
                        if(opcode == 0xF002) {
                            for(int i = 0; i < 16; ++i) {
                                _xoAudioPattern[i] = _memory[(_rI + i) & ADDRESS_MASK];
                            }
                        }
                        else
                            errorHalt();
                    case 0x07:  // Fx07 - LD Vx, DT
                        _rV[(opcode >> 8) & 0xF] = _rDT;
                        break;
                    case 0x0A: {  // Fx0A - LD Vx, K
                        auto key = _host.getKeyPressed();
                        if (key) {
                            _rV[(opcode >> 8) & 0xF] = key - 1;
                            _cpuState = eNORMAL;
                        }
                        else {
                            // keep waiting...
                            _rPC -= 2;
                            --_cycleCounter;
                            _cpuState = eWAITING;
                        }
                        break;
                    }
                    case 0x15:  // Fx15 - LD DT, Vx
                        _rDT = _rV[(opcode >> 8) & 0xF];
                        break;
                    case 0x18:  // Fx18 - LD ST, Vx
                        _rST = _rV[(opcode >> 8) & 0xF];
                        if(!_rST) _wavePhase = 0;
                        break;
                    case 0x1E:  // Fx1E - ADD I, Vx
                        _rI = (_rI + _rV[(opcode >> 8) & 0xF]) & ADDRESS_MASK;
                        break;
                    case 0x29:  // Fx29 - LD F, Vx
                        _rI = (_rV[(opcode >> 8) & 0xF] & 0xF) * 5;
                        break;
                    case 0x30:  // Fx30 - LD big F, Vx
                        _rI = (_rV[(opcode >> 8) & 0xF] & 0xF) * 10 + 16*5;
                        break;
                    case 0x33: {  // Fx33 - LD B, Vx
                        uint8_t val = _rV[(opcode >> 8) & 0xF];
                        _memory[_rI & ADDRESS_MASK] = val / 100;
                        _memory[(_rI + 1) & ADDRESS_MASK] = (val / 10) % 10;
                        _memory[(_rI + 2) & ADDRESS_MASK] = val % 10;
                        break;
                    }
                    case 0x3A: // Fx3A - pitch vx
                        _xoPitch.store(_rV[(opcode >> 8) & 0xF]);
                        break;
                    case 0x55: {  // Fx55 - LD [I], Vx
                        uint8_t upto = (opcode >> 8) & 0xF;
                        addCycles(14);
                        for (int i = 0; i <= upto; ++i) {
                            _memory[(_rI + i) & ADDRESS_MASK] = _rV[i];
                            addCycles(14);
                        }
                        if(_rI + upto > ADDRESS_MASK)
                            fixupSafetyPad();
                        if (_options.optLoadStoreIncIByX) {
                            _rI = (_rI + upto) & ADDRESS_MASK;
                        }
                        else if (!_options.optLoadStoreDontIncI) {
                            _rI = (_rI + upto + 1) & ADDRESS_MASK;
                        }
                        break;
                    }
                    case 0x65: {  // Fx65 - LD Vx, [I]
                        uint8_t upto = (opcode >> 8) & 0xF;
                        addCycles(14);
                        for (int i = 0; i <= upto; ++i) {
                            _rV[i] = _memory[(_rI + i) & ADDRESS_MASK];
                            addCycles(14);
                        }
                        if (_options.optLoadStoreIncIByX) {
                            _rI = (_rI + upto) & ADDRESS_MASK;
                        }
                        else if (!_options.optLoadStoreDontIncI) {
                            _rI = (_rI + upto + 1) & ADDRESS_MASK;
                        }
                        break;
                    }
                    default:
                        errorHalt();
                        break;
                }
                break;
            }
        }
        if (_execMode == eSTEP || (_execMode == eSTEPOVER && _rSP <= _stepOverSP)) {
            _execMode = ePAUSED;
        }
    }

    void executeInstructions(int numInstructions) override
    {
        if(_options.optInstantDxyn) {
            for (int i = 0; i < numInstructions; ++i)
                Chip8Emulator::executeInstruction();
        }
        else {
            for (int i = 0; i < numInstructions; ++i) {
                if (i && (((_memory[_rPC] << 8) | _memory[_rPC + 1]) & 0xF000) == 0xD000)
                    return;
                Chip8Emulator::executeInstruction();
            }
        }
    }

    inline bool drawSpritePixelEx(uint8_t x, uint8_t y, uint8_t planes, bool hires)
    {
        if constexpr (quirks&HiresSupport) {
            return _screen.drawSpritePixelDoubled(x, y, planes, hires);
        }
        return _screen.drawSpritePixel(x, y, planes);
    }

    bool drawSprite(uint8_t x, uint8_t y, const uint8_t* data, uint8_t height, bool hires)
    {
        bool collision = false;
        const int scrWidth = quirks&HiresSupport ? MAX_SCREEN_WIDTH : MAX_SCREEN_WIDTH/2;
        const int scrHeight = quirks&HiresSupport ? MAX_SCREEN_HEIGHT : MAX_SCREEN_HEIGHT/2;
        int scale = quirks&HiresSupport ? (hires ? 1 : 2) : 1;
        int width = 8;
        x %= scrWidth;
        y %= scrHeight;
        if(height == 0) {
            width = height = 16;
        }
        uint8_t planes = quirks&MultiColor ? _planes : 1;
        while(planes) {
            auto plane = planes & -planes;
            planes &= planes - 1;
            for (int l = 0; l < height; ++l) {
                uint8_t value = *data++;
                if constexpr ((quirks&WrapSprite) != 0) {
                    for (unsigned b = 0; b < width; ++b, value <<= 1) {
                        if (b == 8)
                            value = *data++;
                        if (value & 0x80) {
                            if (drawSpritePixelEx((x + b * scale) % scrWidth, (y + l * scale) % scrHeight, plane, hires))
                                collision = true;
                        }
                    }
                }
                else {
                    if (y + l * scale < scrHeight) {
                        for (unsigned b = 0; b < width; ++b, value <<= 1) {
                            if (b == 8)
                                value = *data++;
                            if (x + b * scale < scrWidth && (value & 0x80)) {
                                if (drawSpritePixelEx(x + b * scale, y + l * scale, plane, hires))
                                    collision = true;
                            }
                        }
                    }
                    else if (width == 16)
                        ++data;
                }
            }
        }
        _screenNeedsUpdate = true;
        return collision;
    }
};

using Chip8EmulatorVIP = Chip8Emulator<12>;
using Chip8EmulatorXO = Chip8Emulator<16>;


//---------------------------------------------------------------------------------------
// ChipEmulatorFP - a method pointer table based CHIP-8 core
//---------------------------------------------------------------------------------------
class Chip8EmulatorFP : public Chip8EmulatorBase
{
public:
    using OpcodeHandler = void (Chip8EmulatorFP::*)(uint16_t);
    const uint32_t ADDRESS_MASK;
    const int SCREEN_WIDTH;
    const int SCREEN_HEIGHT;
    
    Chip8EmulatorFP(Chip8EmulatorHost& host, Chip8EmulatorOptions& options, IChip8Emulator* other = nullptr);
    ~Chip8EmulatorFP() override;

    std::string name() const override
    {
        return "Chip-8-MPT";
    }

    void reset() override;
    void executeInstruction() override;
    void executeInstructionNoBreakpoints();
    void executeInstructions(int numInstructions) override;

    uint8_t getNextMCSample() override;

    void on(uint16_t mask, uint16_t opcode, OpcodeHandler handler);

    void setHandler();

    void opNop(uint16_t opcode);
    void opInvalid(uint16_t opcode);
    void op0010(uint16_t opcode);
    void op0011(uint16_t opcode);
    void op00Bn(uint16_t opcode);
    void op00Cn(uint16_t opcode);
    void op00Cn_masked(uint16_t opcode);
    void op00Dn(uint16_t opcode);
    void op00Dn_masked(uint16_t opcode);
    void op00E0(uint16_t opcode);
    void op00E0_megachip(uint16_t opcode);
    void op00EE(uint16_t opcode);
    void op00FB(uint16_t opcode);
    void op00FB_masked(uint16_t opcode);
    void op00FC(uint16_t opcode);
    void op00FC_masked(uint16_t opcode);
    void op00FD(uint16_t opcode);
    void op00FE(uint16_t opcode);
    void op00FE_withClear(uint16_t opcode);
    void op00FE_megachip(uint16_t opcode);
    void op00FF(uint16_t opcode);
    void op00FF_withClear(uint16_t opcode);
    void op00FF_megachip(uint16_t opcode);
    void op01nn(uint16_t opcode);
    void op02nn(uint16_t opcode);
    void op03nn(uint16_t opcode);
    void op04nn(uint16_t opcode);
    void op05nn(uint16_t opcode);
    void op060n(uint16_t opcode);
    void op0700(uint16_t opcode);
    void op080n(uint16_t opcode);
    void op09nn(uint16_t opcode);
    void op1nnn(uint16_t opcode);
    void op2nnn(uint16_t opcode);
    void op3xnn(uint16_t opcode);
    void op3xnn_with_F000(uint16_t opcode);
    void op3xnn_with_01nn(uint16_t opcode);
    void op4xnn(uint16_t opcode);
    void op4xnn_with_F000(uint16_t opcode);
    void op4xnn_with_01nn(uint16_t opcode);
    void op5xy0(uint16_t opcode);
    void op5xy0_with_F000(uint16_t opcode);
    void op5xy0_with_01nn(uint16_t opcode);
    void op5xy2(uint16_t opcode);
    void op5xy3(uint16_t opcode);
    void op5xy4(uint16_t opcode);
    void op6xnn(uint16_t opcode);
    void op7xnn(uint16_t opcode);
    void op8xy0(uint16_t opcode);
    void op8xy1(uint16_t opcode);
    void op8xy1_dontResetVf(uint16_t opcode);
    void op8xy2(uint16_t opcode);
    void op8xy2_dontResetVf(uint16_t opcode);
    void op8xy3(uint16_t opcode);
    void op8xy3_dontResetVf(uint16_t opcode);
    void op8xy4(uint16_t opcode);
    void op8xy5(uint16_t opcode);
    void op8xy6(uint16_t opcode);
    void op8xy6_justShiftVx(uint16_t opcode);
    void op8xy7(uint16_t opcode);
    void op8xyE(uint16_t opcode);
    void op8xyE_justShiftVx(uint16_t opcode);
    void op9xy0(uint16_t opcode);
    void op9xy0_with_F000(uint16_t opcode);
    void op9xy0_with_01nn(uint16_t opcode);
    void opAnnn(uint16_t opcode);
    void opBnnn(uint16_t opcode);
    void opBxnn(uint16_t opcode);
    void opBxyn(uint16_t opcode);
    void opCxnn(uint16_t opcode);
    void opCxnn_randLCG(uint16_t opcode);
    void opCxnn_counting(uint16_t opcode);
    void opDxyn_megaChip(uint16_t opcode);
    void opEx9E(uint16_t opcode);
    void opEx9E_with_F000(uint16_t opcode);
    void opEx9E_with_01nn(uint16_t opcode);
    void opExA1(uint16_t opcode);
    void opExA1_with_F000(uint16_t opcode);
    void opExA1_with_01nn(uint16_t opcode);
    void opF000(uint16_t opcode);
    void opF002(uint16_t opcode);
    void opFx01(uint16_t opcode);
    void opFx07(uint16_t opcode);
    void opFx0A(uint16_t opcode);
    void opFx15(uint16_t opcode);
    void opFx18(uint16_t opcode);
    void opFx1E(uint16_t opcode);
    void opFx29(uint16_t opcode);
    void opFx29_ship10Beta(uint16_t opcode);
    void opFx30(uint16_t opcode);
    void opFx33(uint16_t opcode);
    void opFx3A(uint16_t opcode);
    void opFx55(uint16_t opcode);
    void opFx55_loadStoreIncIByX(uint16_t opcode);
    void opFx55_loadStoreDontIncI(uint16_t opcode);
    void opFx65(uint16_t opcode);
    void opFx65_loadStoreIncIByX(uint16_t opcode);
    void opFx65_loadStoreDontIncI(uint16_t opcode);
    void opFx75(uint16_t opcode);
    void opFx85(uint16_t opcode);

    template<uint16_t quirks>
    void opDxyn(uint16_t opcode)
    {
        if constexpr (quirks&HiresSupport) {
            if(_isHires)
            {
                int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH - 1);
                int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT - 1);
                int lines = opcode & 0xF;
                _rV[15] = drawSprite<quirks>(x, y, &_memory[_rI & ADDRESS_MASK], lines, true) ? 1 : 0;
            }
            else
            {
                int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH / 2 - 1);
                int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT / 2 - 1);
                int lines = opcode & 0xF;
                _rV[15] = drawSprite<quirks>(x*2, y*2, &_memory[_rI & ADDRESS_MASK], lines, false) ? 1 : 0;
            }
        }
        else {
            int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH - 1);
            int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT - 1);
            int lines = opcode & 0xF;
            _rV[15] = drawSprite<quirks>(x, y, &_memory[_rI & ADDRESS_MASK], lines, false) ? 1 : 0;
        }
        _screenNeedsUpdate = true;
    }

    template<uint16_t quirks>
    void opDxyn_displayWait(uint16_t opcode)
    {
        if constexpr (quirks&HiresSupport) {
            if(_isHires)
            {
                int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH - 1);
                int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT - 1);
                int lines = opcode & 0xF;
                _rV[15] = drawSprite<quirks>(x, y, &_memory[_rI & ADDRESS_MASK], lines, true) ? 1 : 0;
            }
            else
            {
                int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH / 2 - 1);
                int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT / 2 - 1);
                int lines = opcode & 0xF;
                _rV[15] = drawSprite<quirks>(x*2, y*2, &_memory[_rI & ADDRESS_MASK], lines, false) ? 1 : 0;
            }
        }
        else {
            int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH - 1);
            int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT - 1);
            int lines = opcode & 0xF;
            if(_cpuState != eWAITING) {
                auto s = lines + (x & 7);
                if(lines > 4 && s > 9) {
                    _rPC -= 2;
                    _cpuState = eWAITING;
                    return;
                }
            }
            else {
                _cpuState = eNORMAL;
            }
            _rV[15] = drawSprite<quirks>(x, y, &_memory[_rI & ADDRESS_MASK], lines, false) ? 1 : 0;
        }
        _screenNeedsUpdate = true;
    }

    template<uint16_t quirks>
    inline bool drawSpritePixelEx(uint8_t x, uint8_t y, uint8_t planes, bool hires)
    {
        if constexpr (quirks&HiresSupport) {
            return _screen.drawSpritePixelDoubled(x, y, planes, hires);
        }
        return _screen.drawSpritePixel(x, y, planes);
    }

    template<uint16_t quirks, int MAX_WIDTH = 128, int MAX_HEIGHT = 64>
    bool drawSprite(uint8_t x, uint8_t y, const uint8_t* data, uint8_t height, bool hires)
    {
        int collision = 0;
        constexpr int scrWidth = quirks&HiresSupport ? MAX_WIDTH : MAX_WIDTH/2;
        constexpr int scrHeight = quirks&HiresSupport ? MAX_HEIGHT : MAX_HEIGHT/2;
        int scale = quirks&HiresSupport ? (hires ? 1 : 2) : 1;
        int width = 8;
        x %= scrWidth;
        y %= scrHeight;
        if(height == 0) {
            height = 16;
            // Thanks @NinjaWeedle: if not hires, draw 16x16 in XO-CHIP, 8x16 in SCHIP1.0/1.1 and nothing on the rest of the variants
            // width = hires ? 16 : (_options.behaviorBase == Chip8EmulatorOptions::eXOCHIP ? 16 : (_options.behaviorBase == Chip8EmulatorOptions::eSCHIP10 || _options.behaviorBase == Chip8EmulatorOptions::eSCHIP11 ? 16 : 0));
            if(_options.optLoresDxy0Is16x16)
                width = 16;
            else if(!_options.optLoresDxy0Is8x16)
                width = 0;
        }
        uint8_t planes;
        if constexpr ((quirks&MultiColor) != 0) planes = _planes; else planes = 1;
        while(planes) {
            auto plane = planes & -planes;
            planes &= planes - 1;
            for (int l = 0; l < height; ++l) {
                uint8_t value = *data++;
                if constexpr ((quirks&WrapSprite) != 0) {
                    for (unsigned b = 0; b < width; ++b, value <<= 1) {
                        if (b == 8)
                            value = *data++;
                        if (value & 0x80) {
                            if (drawSpritePixelEx<quirks>((x + b * scale) % scrWidth, (y + l * scale) % scrHeight, plane, hires))
                                ++collision;
                        }
                    }
                }
                else {
                    if (y + l * scale < scrHeight) {
                        int lineCol = 0;
                        for (unsigned b = 0; b < width; ++b, value <<= 1) {
                            if (b == 8)
                                value = *data++;
                            if (x + b * scale < scrWidth && (value & 0x80)) {
                                if (drawSpritePixelEx<quirks>(x + b * scale, y + l * scale, plane, hires))
                                    lineCol = 1;
                            }
                        }
                        collision += lineCol;
                    }
                    else {
                        if constexpr (quirks&SChip11Collisions)
                            ++collision;
                        else
                            break;
                        //if (width == 16)
                        //    ++data;
                    }
                }
            }
        }
        if constexpr (quirks&SChip11Collisions)
            return collision;
        else
            return (bool)collision;
    }

private:
    inline uint16_t readWord(uint32_t addr) const
    {
        return (_memory[addr] << 8) | (addr == ADDRESS_MASK ? _memory[0] : _memory[addr + 1]);
    }
    std::vector<OpcodeHandler> _opcodeHandler;
    uint32_t _simpleRandSeed{12345};
    uint32_t _simpleRandState{12345};
};


class Chip8HeadlessHost : public Chip8EmulatorHost
{
public:
    explicit Chip8HeadlessHost(const Chip8EmulatorOptions& options_) : options(options_) {}
    ~Chip8HeadlessHost() override = default;
    bool isHeadless() const override { return true; }
    uint8_t getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void updateScreen() override {}
    void updatePalette(const std::array<uint8_t,16>& palette) override {}
    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override {}
    Chip8EmulatorOptions options;
};

}
