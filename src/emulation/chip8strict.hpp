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

    void reset() override
    {
        Chip8EmulatorBase::reset();
        _machineCycles = 3206;  // This is the amount of cycles a VIP needs to get to the start of the program
        _nextFrame = calcNextFrame();
        _cycleCounter = 2;
        ++_frameCounter;
    }

    inline uint16_t readWord(uint32_t addr) const
    {
        return (_memory[addr] << 8) | _memory[addr + 1];
    }

    int64_t getMachineCycles() const override { return _machineCycles; }

    void tick(int instructionsPerFrame) override
    {
        if (_execMode == ePAUSED || _cpuState == eERROR) {
            setExecMode(ePAUSED);
            return;
        }
        auto nextFrame = _nextFrame;
        while(_execMode != ePAUSED && _machineCycles < nextFrame) {
            Chip8StrictEmulator::executeInstruction();
        }
    }

    void executeInstruction() override
    {
        if (_execMode == ePAUSED || _cpuState == eERROR)
            return;
        uint16_t opcode = readWord(_rPC);
        _rPC = (_rPC + 2) & ADDRESS_MASK;
        if(_cpuState != eWAITING) {
            ++_cycleCounter;
            addCycles((opcode & 0xF000) ? 68 : 40);
        }
        switch (opcode >> 12) {
            case 0:
                if (opcode == 0x00E0) {  // 00E0 - clear
                    clearScreen();
                    ++_clearCounter;
                    addCycles(3078);
                }
                else if (opcode == 0x00EE) {  // 00EE - return
                    _rPC = _stack[--_rSP];
                    addCycles(10);
                    if (_execMode == eSTEPOUT)
                        _execMode = ePAUSED;
                }
                else {
                    errorHalt();
                }
                break;
            case 1:  // 1nnn - jump NNN
                if((opcode & 0xFFF) == _rPC - 2)
                    _execMode = ePAUSED;
                _rPC = opcode & 0xFFF;
                addCycles(12);
                break;
            case 2:  // 2nnn - :call NNN
                _stack[_rSP++] = _rPC;
                _rPC = opcode & 0xFFF;
                addCycles(26);
                break;
            case 3:  // 3xnn - if vX != NN then
                if (_rV[(opcode >> 8) & 0xF] == (opcode & 0xff)) {
                    _rPC += 2;
                    addCycles(14);
                }
                else {
                    addCycles(10);
                }
                break;
            case 4:  // 4xnn - if vX == NN then
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
                    case 0: // 5xy0 - if vX != vY then
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
            case 6:  // 6xnn - vX := NN
                _rV[(opcode >> 8) & 0xF] = opcode & 0xFF;
                addCycles(6);
                break;
            case 7:  // 7xnn - vX += NN
                _rV[(opcode >> 8) & 0xF] += opcode & 0xFF;
                addCycles(10);
                break;
            case 8: {
                switch (opcode & 0xF) {
                    case 0:  // 8xy0 - vX := vY
                        _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF];
                        addCycles(12);
                        break;
                    case 1:  // 8xy1 - vX |= vY
                        _rV[(opcode >> 8) & 0xF] |= _rV[(opcode >> 4) & 0xF];
                        _rV[0xF] = 0;
                        addCycles(44);
                        break;
                    case 2:  // 8xy2 - vX &= vY
                        _rV[(opcode >> 8) & 0xF] &= _rV[(opcode >> 4) & 0xF];
                        _rV[0xF] = 0;
                        addCycles(44);
                        break;
                    case 3:  // 8xy3 - vX ^= vY
                        _rV[(opcode >> 8) & 0xF] ^= _rV[(opcode >> 4) & 0xF];
                        _rV[0xF] = 0;
                        addCycles(44);
                        break;
                    case 4: {  // 8xy4 - vX += vY
                        uint16_t result = _rV[(opcode >> 8) & 0xF] + _rV[(opcode >> 4) & 0xF];
                        _rV[(opcode >> 8) & 0xF] = result;
                        _rV[0xF] = result>>8;
                        addCycles(44);
                        break;
                    }
                    case 5: {  // 8xy5 - vX -= vY
                        uint16_t result = _rV[(opcode >> 8) & 0xF] - _rV[(opcode >> 4) & 0xF];
                        _rV[(opcode >> 8) & 0xF] = result;
                        _rV[0xF] = result > 255 ? 0 : 1;
                        addCycles(44);
                        break;
                    }
                    case 6: {  // 8xy6 - vX >>= vY
                        uint8_t carry = _rV[(opcode >> 4) & 0xF] & 1;
                        _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] >> 1;
                        _rV[0xF] = carry;
                        addCycles(44);
                        break;
                    }
                    case 7: {  // 8xy7 - vX =- vY
                        uint16_t result = _rV[(opcode >> 4) & 0xF] - _rV[(opcode >> 8) & 0xF];
                        _rV[(opcode >> 8) & 0xF] = result;
                        _rV[0xF] = result > 255 ? 0 : 1;
                        addCycles(44);
                        break;
                    }
                    case 0xE: {  // 8xyE - vX <<= vY
                        uint8_t carry = _rV[(opcode >> 4) & 0xF] >> 7;
                        _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] << 1;
                        _rV[0xF] = carry;
                        addCycles(44);
                        break;
                    }
                    default:
                        errorHalt();
                        break;
                }
                break;
            }
            case 9:  // 9xy0 - if vX == vY then
                if (_rV[(opcode >> 8) & 0xF] != _rV[(opcode >> 4) & 0xF]) {
                    _rPC += 2;
                    addCycles(18);
                }
                else {
                    addCycles(14);
                }
                break;
            case 0xA:  // Annn - i := NNN
                _rI = opcode & 0xFFF;
                addCycles(12);
                break;
            case 0xB: {  // Bnnn - jump0 NNN
                auto t = _rPC = opcode & 0xFFF;
                _rPC += _rV[0];
                addCycles(((_rPC ^ t) & 0xFF00) ? 24 : 22);
                _rPC &= ADDRESS_MASK;
                break;
            }
            case 0xC: {  // Cxnn - vX := random NN
                uint16_t val = ++_randomSeed>>8;
                val += _chip8_cosmac_vip[0x100 + (_randomSeed&0xFF)];
                val = (val & 0xFF) + (val >> 1);
                _randomSeed = (_randomSeed & 0xFF) | (val << 8);
                _rV[(opcode >> 8) & 0xF] = val & (opcode & 0xFF);
                addCycles(36);
                break;
            }
            case 0xD: {  // Dxyn - sprite vX vY N
                // spt = 66 + (46 + 20*(VX&7))*N (+IDL);
                int x = _rV[(opcode >> 8) & 0xF] & (SCREEN_WIDTH - 1);
                int y = _rV[(opcode >> 4) & 0xF] & (SCREEN_HEIGHT - 1);
                int lines = opcode & 0xF;
                auto cyclesLeftInFrame = _nextFrame - _machineCycles;
                if(_cpuState != eWAITING) {
                    auto prepareTime = 68 + lines * (46 + 20 * (x & 7));
                    _cpuState = eWAITING;
                    _rPC -= 2;
                    _instructionCycles = (prepareTime > cyclesLeftInFrame) ? prepareTime - cyclesLeftInFrame : 0;
                    addCycles(cyclesLeftInFrame);
                }
                else
                {
                    if(_instructionCycles) {
                        _rPC -= 2;
                        _instructionCycles -= (_instructionCycles > cyclesLeftInFrame) ? cyclesLeftInFrame : _instructionCycles;
                        addCycles(cyclesLeftInFrame);
                    }
                    else {
                        _cpuState = eNORMAL;
                        _rV[15] = drawSprite(x, y, &_memory[_rI & ADDRESS_MASK], lines, false) ? 1 : 0;
                    }
                }
                break;
            }
            case 0xE:
                if ((opcode & 0xff) == 0x9E) {  // Ex9E - if vX -key then
                    if (_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
                        _rPC += 2;
                        addCycles(18);
                    }
                    else {
                        addCycles(14);
                    }
                }
                else if ((opcode & 0xff) == 0xA1) {  // ExA1 - if vX key then
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
                    case 0x07:  // Fx07 - vX := delay
                        _rV[(opcode >> 8) & 0xF] = _rDT;
                        addCycles(6);
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
                        addCycles(6);
                        break;
                    case 0x18:  // Fx18 - buzzer := vX
                        _rST = _rV[(opcode >> 8) & 0xF];
                        if(!_rST) _wavePhase = 0;
                        addCycles(6);
                        break;
                    case 0x1E: {  // Fx1E - i += vX
                        auto oldIH = _rI >> 8;
                        _rI = (_rI + _rV[(opcode >> 8) & 0xF]) & ADDRESS_MASK;
                        addCycles(_rI>>8 != oldIH ? 18 : 12);
                        break;
                    }
                    case 0x29:  // Fx29 - i := hex vX
                        _rI = (_rV[(opcode >> 8) & 0xF] & 0xF) * 5;
                        addCycles(16);
                        break;
                    case 0x33: {  // Fx33 - bcd vX
                        uint8_t val = _rV[(opcode >> 8) & 0xF];
                        _memory[_rI & ADDRESS_MASK] = val / 100;
                        _memory[(_rI + 1) & ADDRESS_MASK] = (val / 10) % 10;
                        _memory[(_rI + 2) & ADDRESS_MASK] = val % 10;
                        addCycles(80 + (_memory[_rI & ADDRESS_MASK] + _memory[(_rI + 1) & ADDRESS_MASK] + _memory[(_rI + 2) & ADDRESS_MASK]) * 16);
                        break;
                    }
                    case 0x55: {  // Fx55 - save vX
                        uint8_t upto = (opcode >> 8) & 0xF;
                        addCycles(14);
                        for (int i = 0; i <= upto; ++i) {
                            _memory[(_rI + i) & ADDRESS_MASK] = _rV[i];
                            addCycles(14);
                        }
                        if(_rI + upto > ADDRESS_MASK)
                            fixupSafetyPad();
                        _rI = (_rI + upto + 1) & ADDRESS_MASK;
                        break;
                    }
                    case 0x65: {  // Fx65 - load vX
                        uint8_t upto = (opcode >> 8) & 0xF;
                        addCycles(14);
                        for (int i = 0; i <= upto; ++i) {
                            _rV[i] = _memory[(_rI + i) & ADDRESS_MASK];
                            addCycles(14);
                        }
                        _rI = (_rI + upto + 1) & ADDRESS_MASK;
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
            if(_cpuState != eWAITING)
                _execMode = ePAUSED;
        }
        if(hasBreakPoint(_rPC)) {
            if(Chip8EmulatorBase::findBreakpoint(_rPC))
                _execMode = ePAUSED;
        }
    }

    void executeInstructions(int numInstructions) override
    {
        for (int i = 0; i < numInstructions; ++i)
            Chip8StrictEmulator::executeInstruction();
    }

    bool drawSprite(uint8_t x, uint8_t y, const uint8_t* data, uint8_t height, bool hires)
    {
        // sdt = 26 + 34*VisN + 4*ColRow1 + (VX < 56 ? 16*VisN + 4*ColRow2 : 0);
        bool collision = false;
        auto bitOffset = x & 7;
        int drawTime = 26;
        x %= 64;
        y %= 32;
        for (int l = 0; l < height; ++l) {
            uint8_t value = *data++;
            if (y + l < 32) {
                unsigned col1 = 0, col2 = 0;
                for (unsigned b = 0; b < 8; ++b, value <<= 1) {
                    unsigned vipBit = b + bitOffset;
                    if (x + b < 64 && (value & 0x80)) {
                        unsigned& col = vipBit < 8 ? col1 : col2;
                        if (_screen.drawSpritePixel(x + b, y + l, 1)) {
                            collision = true;
                            col = 4;
                        }
                    }
                }
                drawTime += 34 + col1 + (x < 56 ? 16 : 0) + col2;
            }
        }
        addCycles(drawTime);
        _screenNeedsUpdate = true;
        return collision;
    }
protected:
    int64_t calcNextFrame() const { return ((_machineCycles + 2572) / 3668) * 3668 + 1096; }
    void handleTimer() override
    {
        ++_frameCounter;
        ++_randomSeed;
        if (_rDT > 0)
            --_rDT;
        if (_rST > 0)
            --_rST;
        if (!_rST)
            _wavePhase = 0;
    }
    inline void addCycles(emu::cycles_t cycles)
    {
        _machineCycles += cycles;
        _systemTime.addCycles(cycles * 8, 1760640);
        if(_machineCycles >= _nextFrame) {
            handleTimer();
            auto irqTime = 1832 + (_rST ? 4 : 0) + (_rDT ? 8 : 0);
            _machineCycles += irqTime;
            _systemTime.addCycles(irqTime * 8, 1760640);
            _nextFrame = calcNextFrame();
        }
    }
    int64_t _machineCycles{0};
    int64_t _nextFrame{1122};
    int _instructionCycles{0};
    Time _systemTime{};
};

}
