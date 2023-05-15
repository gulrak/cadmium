//---------------------------------------------------------------------------------------
// src/emulation/chip8strict.hpp
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

#include <chiplet/chip8meta.hpp>
#include <emulation/chip8options.hpp>
#include <emulation/chip8emulatorbase.hpp>
#include <emulation/time.hpp>

namespace emu
{

class Chip8StrictEmulator : public Chip8EmulatorBase
{
public:
    using Chip8EmulatorBase::ExecMode;
    using Chip8EmulatorBase::CpuState;
    constexpr static uint16_t ADDRESS_MASK = 0xFFF;
    constexpr static uint32_t MEMORY_SIZE = 4096;
    constexpr static int SCREEN_WIDTH = 64;
    constexpr static int SCREEN_HEIGHT = 32;

    Chip8StrictEmulator(Chip8EmulatorHost& host, Chip8EmulatorOptions& options, IChip8Emulator* other = nullptr)
        : Chip8EmulatorBase(host, options, other)
    {
        _memory.resize(MEMORY_SIZE, 0);
    }
    ~Chip8StrictEmulator() override = default;

    std::string name() const override
    {
        return "Chip-8-Strict";
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
                if (opcode == 0x00E0) {  // 00E0 - CLS
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
                else {
                    errorHalt();
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
                        addCycles(44);
                        break;
                    case 2:  // 8xy2 - AND Vx, Vy
                        _rV[(opcode >> 8) & 0xF] &= _rV[(opcode >> 4) & 0xF];
                        if (!_options.optDontResetVf)
                            _rV[0xF] = 0;
                        addCycles(44);
                        break;
                    case 3:  // 8xy3 - XOR Vx, Vy
                        _rV[(opcode >> 8) & 0xF] ^= _rV[(opcode >> 4) & 0xF];
                        if (!_options.optDontResetVf)
                            _rV[0xF] = 0;
                        addCycles(44);
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
                if (_options.optAllowHires) {
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
                    case 0x07:  // Fx07 - LD Vx, DT
                        _rV[(opcode >> 8) & 0xF] = _rDT;
                        addCycles(6);
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
                        addCycles(6);
                        break;
                    case 0x18:  // Fx18 - LD ST, Vx
                        _rST = _rV[(opcode >> 8) & 0xF];
                        if(!_rST) _wavePhase = 0;
                        addCycles(6);
                        break;
                    case 0x1E: {  // Fx1E - ADD I, Vx
                        auto oldIH = _rI >> 8;
                        _rI = (_rI + _rV[(opcode >> 8) & 0xF]) & ADDRESS_MASK;
                        addCycles(_rI>>8 != oldIH ? 18 : 12);
                        break;
                    }
                    case 0x29:  // Fx29 - LD F, Vx
                        _rI = (_rV[(opcode >> 8) & 0xF] & 0xF) * 5;
                        addCycles(16);
                        break;
                    case 0x33: {  // Fx33 - LD B, Vx
                        uint8_t val = _rV[(opcode >> 8) & 0xF];
                        _memory[_rI & ADDRESS_MASK] = val / 100;
                        _memory[(_rI + 1) & ADDRESS_MASK] = (val / 10) % 10;
                        _memory[(_rI + 2) & ADDRESS_MASK] = val % 10;
                        addCycles(80 + (_memory[_rI & ADDRESS_MASK] + _memory[(_rI + 1) & ADDRESS_MASK] + _memory[(_rI + 2) & ADDRESS_MASK]) * 16);
                        break;
                    }
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
                Chip8StrictEmulator::executeInstruction();
        }
        else {
            for (int i = 0; i < numInstructions; ++i) {
                if (i && (((_memory[_rPC] << 8) | _memory[_rPC + 1]) & 0xF000) == 0xD000)
                    return;
                Chip8StrictEmulator::executeInstruction();
            }
        }
    }

    inline bool drawSpritePixelEx(uint8_t x, uint8_t y, uint8_t planes, bool hires)
    {
        if (_options.optAllowHires) {
            return _screen.drawSpritePixelDoubled(x, y, planes, hires);
        }
        return _screen.drawSpritePixel(x, y, planes);
    }

    bool drawSprite(uint8_t x, uint8_t y, const uint8_t* data, uint8_t height, bool hires)
    {
        bool collision = false;
        const int scrWidth = _options.optAllowHires ? MAX_SCREEN_WIDTH : MAX_SCREEN_WIDTH/2;
        const int scrHeight = _options.optAllowHires ? MAX_SCREEN_HEIGHT : MAX_SCREEN_HEIGHT/2;
        int scale = _options.optAllowHires ? (hires ? 1 : 2) : 1;
        int width = 8;
        x %= scrWidth;
        y %= scrHeight;
        if(height == 0) {
            width = height = 16;
        }
        auto plane = 1;
        for (int l = 0; l < height; ++l) {
            uint8_t value = *data++;
            if (_options.optWrapSprites) {
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
        _screenNeedsUpdate = true;
        return collision;
    }
};

}
