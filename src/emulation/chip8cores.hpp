//---------------------------------------------------------------------------------------
// src/emulation/chip8cores.hpp
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

    uint16_t readWord(uint32_t addr) const
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
                else if (opcode == 0x00E0) {  // 00E0 - clear
                    clearScreen();
                    ++_clearCounter;
                }
                else if (opcode == 0x00EE) {  // 00EE - return
                    _rPC = _stack[--_rSP];
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
                else {
                    errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
                }
                break;
            case 1:  // 1nnn - jump NNN
                if((opcode & 0xFFF) == _rPC - 2)
                    _execMode = ePAUSED;
                _rPC = opcode & 0xFFF;
                break;
            case 2:  // 2nnn - :call NNN
                _stack[_rSP++] = _rPC;
                _rPC = opcode & 0xFFF;
                break;
            case 3:  // 3xnn - if vX != NN then
                if (_rV[(opcode >> 8) & 0xF] == (opcode & 0xff)) {
                    _rPC += 2;
                }
                break;
            case 4:  // 4xnn - if vX == NN then
                if (_rV[(opcode >> 8) & 0xF] != (opcode & 0xFF)) {
                    _rPC += 2;
                }
                break;
            case 5: {
                switch (opcode & 0xF) {
                    case 0: // 5xy0 - if vX != vY then
                        if (_rV[(opcode >> 8) & 0xF] == _rV[(opcode >> 4) & 0xF]) {
                            _rPC += 2;
                        }
                        break;
                    case 2: {  // 5xy2  - save vx - vy
                        auto x = (opcode >> 8) & 0xF;
                        auto y = (opcode >> 4) & 0xF;
                        auto l = std::abs(x-y);
                        for(int i=0; i <= l; ++i)
                            write(_rI + i, _rV[x < y ? x + i : x - i]);
                        break;
                    }
                    case 3: {  // 5xy3 - load vx - vy
                        auto x = (opcode >> 8) & 0xF;
                        auto y = (opcode >> 4) & 0xF;
                        for(int i=0; i <= std::abs(x-y); ++i)
                            _rV[x < y ? x + i : x - i] = _memory[_rI + i];
                        break;
                    }
                    case 4: { // palette x y
                        auto x = (opcode >> 8) & 0xF;
                        auto y = (opcode >> 4) & 0xF;
                        for(int i=0; i <= std::abs(x-y); ++i)
                            _xxoPalette[x < y ? x + i : x - i] = _memory[_rI + i];
                        _host.updatePalette(_xxoPalette);
                        break;
                    }
                    default:
                        errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
                        break;
                }
                break;
            }
            case 6:  // 6xnn - vX := NN
                _rV[(opcode >> 8) & 0xF] = opcode & 0xFF;
                break;
            case 7:  // 7xnn - vX += NN
                _rV[(opcode >> 8) & 0xF] += opcode & 0xFF;
                break;
            case 8: {
                switch (opcode & 0xF) {
                    case 0:  // 8xy0 - vX := vY
                        _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF];
                        break;
                    case 1:  // 8xy1 - vX |= vY
                        _rV[(opcode >> 8) & 0xF] |= _rV[(opcode >> 4) & 0xF];
                        if (!_options.optDontResetVf)
                            _rV[0xF] = 0;
                        break;
                    case 2:  // 8xy2 - vX &= vY
                        _rV[(opcode >> 8) & 0xF] &= _rV[(opcode >> 4) & 0xF];
                        if (!_options.optDontResetVf)
                            _rV[0xF] = 0;
                        break;
                    case 3:  // 8xy3 - vX ^= vY
                        _rV[(opcode >> 8) & 0xF] ^= _rV[(opcode >> 4) & 0xF];
                        if (!_options.optDontResetVf)
                            _rV[0xF] = 0;
                        break;
                    case 4: {  // 8xy4 - vX += vY
                        uint16_t result = _rV[(opcode >> 8) & 0xF] + _rV[(opcode >> 4) & 0xF];
                        _rV[(opcode >> 8) & 0xF] = result;
                        _rV[0xF] = result>>8;
                        break;
                    }
                    case 5: {  // 8xy5 - vX -= vY
                        uint16_t result = _rV[(opcode >> 8) & 0xF] - _rV[(opcode >> 4) & 0xF];
                        _rV[(opcode >> 8) & 0xF] = result;
                        _rV[0xF] = result > 255 ? 0 : 1;
                        break;
                    }
                    case 6:  // 8xy6 - vX >>= vY
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
                        break;
                    case 7: {  // 8xy7 - vX =- vY
                        uint16_t result = _rV[(opcode >> 4) & 0xF] - _rV[(opcode >> 8) & 0xF];
                        _rV[(opcode >> 8) & 0xF] = result;
                        _rV[0xF] = result > 255 ? 0 : 1;
                        break;
                    }
                    case 0xE:  // 8xyE - vX <<= vY
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
                        break;
                    default:
                        errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
                        break;
                }
                break;
            }
            case 9:  // 9xy0 - if vX == vY then
                if (_rV[(opcode >> 8) & 0xF] != _rV[(opcode >> 4) & 0xF]) {
                    _rPC += 2;
                }
                break;
            case 0xA:  // Annn - i := NNN
                _rI = opcode & 0xFFF;
                break;
            case 0xB:  // Bnnn - jump0 NNN / Bxnn - JP Vx, addr
                _rPC = _options.optJump0Bxnn ? (_rV[(opcode >> 8) & 0xF] + (opcode & 0xFFF)) & ADDRESS_MASK : (_rV[0] + (opcode & 0xFFF)) & ADDRESS_MASK;
                break;
            case 0xC: {  // Cxnn - vX := random NN
                ++_randomSeed;
                uint16_t val = _randomSeed>>8;
                val += _chip8_cosmac_vip[0x100 + (_randomSeed&0xFF)];
                uint8_t result = val;
                val >>= 1;
                val += result;
                _randomSeed = (_randomSeed & 0xFF) | (val << 8);
                result = val & (opcode & 0xFF);
                _rV[(opcode >> 8) & 0xF] = result; // GetRandomValue(0, 255) & (opcode & 0xFF);
                break;
            }
            case 0xD: {  // Dxyn - sprite vX vY N
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
                if ((opcode & 0xff) == 0x9E) {  // Ex9E - if vX -key then
                    if (_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
                        _rPC += 2;
                    }
                }
                else if ((opcode & 0xff) == 0xA1) {  // ExA1 - if vX key then
                    if (_host.isKeyUp(_rV[(opcode >> 8) & 0xF] & 0xF)) {
                        _rPC += 2;
                    }
                }
                break;
            case 0xF: {
                switch (opcode & 0xFF) {
                    case 0x00: // i := long nnnn
                        if(opcode != 0xF000)
                            errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
                        if constexpr (addressLines == 16) {
                            _rI = ((_memory[_rPC] << 8) | _memory[_rPC + 1]) & ADDRESS_MASK;
                            _rPC = (_rPC + 2) & ADDRESS_MASK;
                        }
                        else {
                            errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
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
                            errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
                    case 0x07:  // Fx07 - vX := delay
                        _rV[(opcode >> 8) & 0xF] = _rDT;
                        break;
                    case 0x0A: {  // Fx0A - vX := key
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
                    case 0x15:  // Fx15 - delay := vX
                        _rDT = _rV[(opcode >> 8) & 0xF];
                        break;
                    case 0x18:  // Fx18 - buzzer := vX
                        _rST = _rV[(opcode >> 8) & 0xF];
                        if(!_rST) _wavePhase = 0;
                        break;
                    case 0x1E:  // Fx1E - i += vX
                        _rI = (_rI + _rV[(opcode >> 8) & 0xF]) & ADDRESS_MASK;
                        break;
                    case 0x29:  // Fx29 - i := hex vX
                        _rI = (_rV[(opcode >> 8) & 0xF] & 0xF) * 5;
                        break;
                    case 0x30:  // Fx30 - i := bighex vX
                        _rI = (_rV[(opcode >> 8) & 0xF] & 0xF) * 10 + 16*5;
                        break;
                    case 0x33: {  // Fx33 - bcd vX
                        uint8_t val = _rV[(opcode >> 8) & 0xF];
                        write(_rI, val / 100);
                        write(_rI + 1, (val / 10) % 10);
                        write(_rI + 2, val % 10);
                        break;
                    }
                    case 0x3A: // Fx3A - pitch vx
                        _xoPitch.store(_rV[(opcode >> 8) & 0xF]);
                        break;
                    case 0x55: {  // Fx55 - save vX
                        uint8_t upto = (opcode >> 8) & 0xF;
                        for (int i = 0; i <= upto; ++i) {
                            write(_rI + i, _rV[i]);
                        }
                        if (_options.optLoadStoreIncIByX) {
                            _rI = (_rI + upto) & ADDRESS_MASK;
                        }
                        else if (!_options.optLoadStoreDontIncI) {
                            _rI = (_rI + upto + 1) & ADDRESS_MASK;
                        }
                        break;
                    }
                    case 0x65: {  // Fx65 - load vX
                        uint8_t upto = (opcode >> 8) & 0xF;
                        for (int i = 0; i <= upto; ++i) {
                            _rV[i] = _memory[_rI + i];
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
                        errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
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
private:
    void write(const uint32_t addr, uint8_t val)
    {
        if(addr <= ADDRESS_MASK)
            _memory[addr] = val;
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

    const VideoRGBAType* getWorkRGBA() const override { return _isMegaChipMode && _options.optWrapSprites ? _workRGBA : nullptr; }

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
    void op00ED_c8e(uint16_t opcode);
    void op00EE(uint16_t opcode);
    void op00EE_cyclic(uint16_t opcode);
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
    void op0151_c8e(uint16_t opcode);
    void op0188_c8e(uint16_t opcode);
    void op02A0_c8x(uint16_t opcode);
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
    void op2nnn_cyclic(uint16_t opcode);
    void op3xnn(uint16_t opcode);
    void op3xnn_with_F000(uint16_t opcode);
    void op3xnn_with_01nn(uint16_t opcode);
    void op4xnn(uint16_t opcode);
    void op4xnn_with_F000(uint16_t opcode);
    void op4xnn_with_01nn(uint16_t opcode);
    void op5xy0(uint16_t opcode);
    void op5xy0_with_F000(uint16_t opcode);
    void op5xy0_with_01nn(uint16_t opcode);
    void op5xy1_c8e(uint16_t opcode);
    void op5xy1_c8x(uint16_t opcode);
    void op5xy2(uint16_t opcode);
    void op5xy2_c8e(uint16_t opcode);
    void op5xy3(uint16_t opcode);
    void op5xy3_c8e(uint16_t opcode);
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
    void opBBnn_c8e(uint16_t opcode);
    void opBFnn_c8e(uint16_t opcode);
    void opBxy0_c8x(uint16_t opcode);
    void opBxyn_c8x(uint16_t opcode);
    void opBxnn(uint16_t opcode);
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
    void opExF2_c8x(uint16_t opcode);
    void opExF5_c8x(uint16_t opcode);
    void opF000(uint16_t opcode);
    void opF002(uint16_t opcode);
    void opFx01(uint16_t opcode);
    void opFx07(uint16_t opcode);
    void opFx0A(uint16_t opcode);
    void opFx15(uint16_t opcode);
    void opFx18(uint16_t opcode);
    void opFx1B_c8e(uint16_t opcode);
    void opFx1E(uint16_t opcode);
    void opFx29(uint16_t opcode);
    void opFx29_ship10Beta(uint16_t opcode);
    void opFx30(uint16_t opcode);
    void opFx33(uint16_t opcode);
    void opFx3A(uint16_t opcode);
    void opFx4F_c8e(uint16_t opcode);
    void opFx55(uint16_t opcode);
    void opFx55_loadStoreIncIByX(uint16_t opcode);
    void opFx55_loadStoreDontIncI(uint16_t opcode);
    void opFx65(uint16_t opcode);
    void opFx65_loadStoreIncIByX(uint16_t opcode);
    void opFx65_loadStoreDontIncI(uint16_t opcode);
    void opFx75(uint16_t opcode);
    void opFx85(uint16_t opcode);
    void opFxF8_c8x(uint16_t opcode);
    void opFxFB_c8x(uint16_t opcode);

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
                if constexpr ((quirks&SChip1xLoresDraw) != 0) {
                    if(_options.instructionsPerFrame && _cycleCounter % _options.instructionsPerFrame != 0) {
                        _rPC -= 2;
                        return;
                    }
                }
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
            if(_options.instructionsPerFrame && _cycleCounter % _options.instructionsPerFrame != 0) {
                _rPC -= 2;
                return;
            }
            int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH - 1);
            int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT - 1);
            int lines = opcode & 0xF;
            if(!_isInstantDxyn && _options.optExtendedVBlank && _cpuState != eWAITING) {
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
            if constexpr ((quirks&SChip1xLoresDraw) != 0) {
                return _screen.drawSpritePixelDoubledSC(x, y, planes, hires);
            }
            else {
                return _screen.drawSpritePixelDoubled(x, y, planes, hires);
            }
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
            if(_options.optLoresDxy0Is16x16 || (_isHires && !_options.optOnlyHires))
                width = 16;
            else if(!_options.optLoresDxy0Is8x16) {
                width = 0;
                height = 0;
            }
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
                            if constexpr ((quirks&SChip1xLoresDraw) != 0) {
                                if (x + b * scale < scrWidth && drawSpritePixelEx<quirks>(x + b * scale, y + l * scale, value & 0x80 ? plane : 0, hires))
                                    lineCol = 1;
                            }
                            else {
                                if (x + b * scale < scrWidth && (value & 0x80)) {
                                    if (drawSpritePixelEx<quirks>(x + b * scale, y + l * scale, plane, hires))
                                        lineCol = 1;
                                }
                            }
                        }
                        if constexpr ((quirks&SChip1xLoresDraw) != 0) {
                            if(!hires) {
                                auto x1 = x & 0x70;
                                auto x2 = std::min(x1 + 32, 128);
                                _screen.copyPixelRow(x1, x2, y + l * scale, y + l * scale + 1);
                            }
                        }
                        collision += lineCol;
                    }
                    else {
                        if constexpr (quirks&SChip11Collisions)
                            ++collision;
                        else
                            break;
                    }
                }
            }
        }
        if constexpr (quirks&SChip11Collisions)
            return hires ? collision : (bool)collision;
        else
            return (bool)collision;
    }

    void renderAudio(int16_t* samples, size_t frames, int sampleFrequency) override;

private:
    void write(const uint32_t addr, uint8_t val)
    {
        if(addr <= ADDRESS_MASK)
            _memory[addr] = val;
    }
    std::vector<OpcodeHandler> _opcodeHandler;
    uint32_t _simpleRandSeed{12345};
    uint32_t _simpleRandState{12345};
    int _chip8xBackgroundColor{0};
    uint8_t _vp595Frequency{0x80};
#ifdef GEN_OPCODE_STATS
    std::map<uint16_t,int64_t> _opcodeStats;
#endif
};


}
