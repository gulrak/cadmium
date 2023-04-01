//---------------------------------------------------------------------------------------
// src/emulation/chip8.cpp
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

#include <emulation/chip8cores.hpp>
#include <emulation/logger.hpp>

#include <iostream>
#include <nlohmann/json.hpp>

namespace emu
{

Chip8EmulatorFP::Chip8EmulatorFP(Chip8EmulatorHost& host, Chip8EmulatorOptions& options, IChip8Emulator* other)
: Chip8EmulatorBase(host, options, other)
, ADDRESS_MASK(options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP ? 0xFFFFFF : options.optHas16BitAddr ? 0xFFFF : 0xFFF)
, SCREEN_WIDTH(options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP ? 256 : options.optAllowHires ? 128 : 64)
, SCREEN_HEIGHT(options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP ? 192 : options.optAllowHires ? 64 : 32)
, _opcodeHandler(0x10000, &Chip8EmulatorFP::opInvalid)
{
    setHandler();
    if(!other) {
        reset();
    }
}


void Chip8EmulatorFP::setHandler()
{
    on(0xFFFF, 0x00E0, &Chip8EmulatorFP::op00E0);
    on(0xFFFF, 0x00EE, &Chip8EmulatorFP::op00EE);
    on(0xF000, 0x1000, &Chip8EmulatorFP::op1nnn);
    on(0xF000, 0x2000, &Chip8EmulatorFP::op2nnn);
    on(0xF000, 0x3000, &Chip8EmulatorFP::op3xnn);
    on(0xF000, 0x4000, &Chip8EmulatorFP::op4xnn);
    on(0xF00F, 0x5000, &Chip8EmulatorFP::op5xy0);
    on(0xF000, 0x6000, &Chip8EmulatorFP::op6xnn);
    on(0xF000, 0x7000, &Chip8EmulatorFP::op7xnn);
    on(0xF00F, 0x8000, &Chip8EmulatorFP::op8xy0);
    on(0xF00F, 0x8001, _options.optDontResetVf ? &Chip8EmulatorFP::op8xy1_dontResetVf : &Chip8EmulatorFP::op8xy1);
    on(0xF00F, 0x8002, _options.optDontResetVf ? &Chip8EmulatorFP::op8xy2_dontResetVf : &Chip8EmulatorFP::op8xy2);
    on(0xF00F, 0x8003, _options.optDontResetVf ? &Chip8EmulatorFP::op8xy3_dontResetVf : &Chip8EmulatorFP::op8xy3);
    on(0xF00F, 0x8004, &Chip8EmulatorFP::op8xy4);
    on(0xF00F, 0x8005, &Chip8EmulatorFP::op8xy5);
    on(0xF00F, 0x8006, _options.optJustShiftVx ? &Chip8EmulatorFP::op8xy6_justShiftVx : &Chip8EmulatorFP::op8xy6);
    on(0xF00F, 0x8007, &Chip8EmulatorFP::op8xy7);
    on(0xF00F, 0x800E, _options.optJustShiftVx ? &Chip8EmulatorFP::op8xyE_justShiftVx : &Chip8EmulatorFP::op8xyE);
    on(0xF00F, 0x9000, &Chip8EmulatorFP::op9xy0);
    on(0xF000, 0xA000, &Chip8EmulatorFP::opAnnn);
    on(0xF000, 0xB000, _options.optJump0Bxnn ? &Chip8EmulatorFP::opBxnn : &Chip8EmulatorFP::opBnnn);
    std::string randomGen;
    if(_options.advanced && _options.advanced->contains("random")) {
        randomGen = _options.advanced->at("random");
        _randomSeed = _options.advanced->at("seed");
    }
    if(randomGen == "rand-lcg")
        on(0xF000, 0xC000, &Chip8EmulatorFP::opCxnn_randLCG);
    else if(randomGen == "counting")
        on(0xF000, 0xC000, &Chip8EmulatorFP::opCxnn_counting);
    else
        on(0xF000, 0xC000, &Chip8EmulatorFP::opCxnn);
    if(_options.optAllowHires) {
        if(_options.optAllowColors) {
            if (_options.optWrapSprites)
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<HiresSupport|MultiColor|WrapSprite>);
            else
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<HiresSupport|MultiColor>);
        }
        else {
            if (_options.optWrapSprites)
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<HiresSupport|WrapSprite>);
            else
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<HiresSupport>);
        }
    }
    else {
        if(_options.optAllowColors) {
            if (_options.optWrapSprites)
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<MultiColor|WrapSprite>);
            else
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<MultiColor>);
        }
        else {
            if (_options.optWrapSprites)
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<WrapSprite>);
            else {
                if(_options.optInstantDxyn)
                    on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<0>);
                else
                    on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn_displayWait<0>);
            }
        }
    }
    //on(0xF000, 0xD000, _options.optAllowHires ? &Chip8EmulatorFP::opDxyn_allowHires : &Chip8EmulatorFP::opDxyn);
    on(0xF0FF, 0xE09E, &Chip8EmulatorFP::opEx9E);
    on(0xF0FF, 0xE0A1, &Chip8EmulatorFP::opExA1);
    on(0xF0FF, 0xF007, &Chip8EmulatorFP::opFx07);
    on(0xF0FF, 0xF00A, &Chip8EmulatorFP::opFx0A);
    on(0xF0FF, 0xF015, &Chip8EmulatorFP::opFx15);
    on(0xF0FF, 0xF018, &Chip8EmulatorFP::opFx18);
    on(0xF0FF, 0xF01E, &Chip8EmulatorFP::opFx1E);
    on(0xF0FF, 0xF029, &Chip8EmulatorFP::opFx29);
    on(0xF0FF, 0xF033, &Chip8EmulatorFP::opFx33);
    on(0xF0FF, 0xF055, _options.optLoadStoreIncIByX ? &Chip8EmulatorFP::opFx55_loadStoreIncIByX : (_options.optLoadStoreDontIncI ? &Chip8EmulatorFP::opFx55_loadStoreDontIncI : &Chip8EmulatorFP::opFx55));
    on(0xF0FF, 0xF065, _options.optLoadStoreIncIByX ? &Chip8EmulatorFP::opFx65_loadStoreIncIByX : (_options.optLoadStoreDontIncI ? &Chip8EmulatorFP::opFx65_loadStoreDontIncI : &Chip8EmulatorFP::opFx65));

    switch(_options.behaviorBase) {
        case Chip8EmulatorOptions::eSCHIP10:
            on(0xFFFF, 0x00FD, &Chip8EmulatorFP::op00FD);
            on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE);
            on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF);
            on(0xF0FF, 0xF029, &Chip8EmulatorFP::opFx29_ship10Beta);
            on(0xF0FF, 0xF075, &Chip8EmulatorFP::opFx75);
            on(0xF0FF, 0xF085, &Chip8EmulatorFP::opFx85);
            break;
        case Chip8EmulatorOptions::eSCHIP11:
        case Chip8EmulatorOptions::eSCHPC:
            on(0xFFF0, 0x00C0, &Chip8EmulatorFP::op00Cn);
            on(0xFFFF, 0x00FB, &Chip8EmulatorFP::op00FB);
            on(0xFFFF, 0x00FC, &Chip8EmulatorFP::op00FC);
            on(0xFFFF, 0x00FD, &Chip8EmulatorFP::op00FD);
            on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE);
            on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF);
            on(0xF0FF, 0xF030, &Chip8EmulatorFP::opFx30);
            on(0xF0FF, 0xF075, &Chip8EmulatorFP::opFx75);
            on(0xF0FF, 0xF085, &Chip8EmulatorFP::opFx85);
            break;
        case Chip8EmulatorOptions::eMEGACHIP:
            on(0xFFFF, 0x0010, &Chip8EmulatorFP::op0010);
            on(0xFFFF, 0x0011, &Chip8EmulatorFP::op0011);
            on(0xFFF0, 0x00B0, &Chip8EmulatorFP::op00Bn);
            on(0xFFF0, 0x00C0, &Chip8EmulatorFP::op00Cn);
            on(0xFFFF, 0x00E0, &Chip8EmulatorFP::op00E0_megachip);
            on(0xFFFF, 0x00FB, &Chip8EmulatorFP::op00FB);
            on(0xFFFF, 0x00FC, &Chip8EmulatorFP::op00FC);
            on(0xFFFF, 0x00FD, &Chip8EmulatorFP::op00FD);
            on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE_megachip);
            on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF_megachip);
            on(0xFF00, 0x0100, &Chip8EmulatorFP::op01nn);
            on(0xFF00, 0x0200, &Chip8EmulatorFP::op02nn);
            on(0xFF00, 0x0300, &Chip8EmulatorFP::op03nn);
            on(0xFF00, 0x0400, &Chip8EmulatorFP::op04nn);
            on(0xFF00, 0x0500, &Chip8EmulatorFP::op05nn);
            on(0xFFF0, 0x0600, &Chip8EmulatorFP::op060n);
            on(0xFFFF, 0x0700, &Chip8EmulatorFP::op0700);
            on(0xFFF0, 0x0800, &Chip8EmulatorFP::op080n);
            on(0xFF00, 0x0900, &Chip8EmulatorFP::op09nn);
            on(0xF000, 0x3000, &Chip8EmulatorFP::op3xnn_with_01nn);
            on(0xF000, 0x4000, &Chip8EmulatorFP::op4xnn_with_01nn);
            on(0xF00F, 0x5000, &Chip8EmulatorFP::op5xy0_with_01nn);
            on(0xF00F, 0x9000, &Chip8EmulatorFP::op9xy0_with_01nn);
            on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn_megaChip);
            on(0xF0FF, 0xE09E, &Chip8EmulatorFP::opEx9E_with_01nn);
            on(0xF0FF, 0xE0A1, &Chip8EmulatorFP::opExA1_with_01nn);
            on(0xF0FF, 0xF030, &Chip8EmulatorFP::opFx30);
            on(0xF0FF, 0xF075, &Chip8EmulatorFP::opFx75);
            on(0xF0FF, 0xF085, &Chip8EmulatorFP::opFx85);
            break;
        case Chip8EmulatorOptions::eXOCHIP:
            on(0xFFF0, 0x00C0, &Chip8EmulatorFP::op00Cn_masked);
            on(0xFFF0, 0x00D0, &Chip8EmulatorFP::op00Dn_masked);
            on(0xFFFF, 0x00FB, &Chip8EmulatorFP::op00FB_masked);
            on(0xFFFF, 0x00FC, &Chip8EmulatorFP::op00FC_masked);
            on(0xFFFF, 0x00FD, &Chip8EmulatorFP::op00FD);
            on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE_withClear);
            on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF_withClear);
            on(0xF000, 0x3000, &Chip8EmulatorFP::op3xnn_with_F000);
            on(0xF000, 0x4000, &Chip8EmulatorFP::op4xnn_with_F000);
            on(0xF00F, 0x5000, &Chip8EmulatorFP::op5xy0_with_F000);
            on(0xF00F, 0x5002, &Chip8EmulatorFP::op5xy2);
            on(0xF00F, 0x5003, &Chip8EmulatorFP::op5xy3);
            on(0xF00F, 0x9000, &Chip8EmulatorFP::op9xy0_with_F000);
            on(0xF0FF, 0xE09E, &Chip8EmulatorFP::opEx9E_with_F000);
            on(0xF0FF, 0xE0A1, &Chip8EmulatorFP::opExA1_with_F000);
            on(0xFFFF, 0xF000, &Chip8EmulatorFP::opF000);
            on(0xF0FF, 0xF001, &Chip8EmulatorFP::opFx01);
            on(0xFFFF, 0xF002, &Chip8EmulatorFP::opF002);
            on(0xF0FF, 0xF030, &Chip8EmulatorFP::opFx30);
            on(0xF0FF, 0xF03A, &Chip8EmulatorFP::opFx3A);
            on(0xF0FF, 0xF075, &Chip8EmulatorFP::opFx75);
            on(0xF0FF, 0xF085, &Chip8EmulatorFP::opFx85);
            break;
        case Chip8EmulatorOptions::eCHICUEYI:
            on(0xFFF0, 0x00C0, &Chip8EmulatorFP::op00Cn_masked);
            on(0xFFF0, 0x00D0, &Chip8EmulatorFP::op00Dn_masked);
            on(0xFFFF, 0x00FB, &Chip8EmulatorFP::op00FB_masked);
            on(0xFFFF, 0x00FC, &Chip8EmulatorFP::op00FC_masked);
            on(0xFFFF, 0x00FD, &Chip8EmulatorFP::op00FD);
            on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE_withClear);
            on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF_withClear);
            on(0xF000, 0x3000, &Chip8EmulatorFP::op3xnn_with_F000);
            on(0xF000, 0x4000, &Chip8EmulatorFP::op4xnn_with_F000);
            on(0xF00F, 0x5000, &Chip8EmulatorFP::op5xy0_with_F000);
            on(0xF00F, 0x5002, &Chip8EmulatorFP::op5xy2);
            on(0xF00F, 0x5003, &Chip8EmulatorFP::op5xy3);
            on(0xF00F, 0x5004, &Chip8EmulatorFP::op5xy4);
            on(0xF00F, 0x9000, &Chip8EmulatorFP::op9xy0_with_F000);
            on(0xF0FF, 0xE09E, &Chip8EmulatorFP::opEx9E_with_F000);
            on(0xF0FF, 0xE0A1, &Chip8EmulatorFP::opExA1_with_F000);
            on(0xFFFF, 0xF000, &Chip8EmulatorFP::opF000);
            on(0xF0FF, 0xF001, &Chip8EmulatorFP::opFx01);
            on(0xFFFF, 0xF002, &Chip8EmulatorFP::opF002);
            on(0xF0FF, 0xF030, &Chip8EmulatorFP::opFx30);
            on(0xF0FF, 0xF03A, &Chip8EmulatorFP::opFx3A);
            break;
        default: break;
    }
}

Chip8EmulatorFP::~Chip8EmulatorFP()
{
}

void Chip8EmulatorFP::reset()
{
    Chip8EmulatorBase::reset();
    _simpleRandState = _simpleRandSeed;
}

inline void Chip8EmulatorFP::executeInstructionNoBreakpoints()
{
    uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
    ++_cycleCounter;
    _rPC = (_rPC + 2) & ADDRESS_MASK;
    (this->*_opcodeHandler[opcode])(opcode);
}

void Chip8EmulatorFP::executeInstructions(int numInstructions)
{
    if(_execMode == ePAUSED)
        return;
    if(_isMegaChipMode) {
        for (int i = 0; i < numInstructions; ++i) {
            if (i && ((_memory[_rPC] << 8) | _memory[_rPC + 1]) == 0x00E0)
                return;
            if(_execMode == eRUNNING && _breakpoints.empty() && !_options.optTraceLog)
                Chip8EmulatorFP::executeInstructionNoBreakpoints();
            else
                Chip8EmulatorFP::executeInstruction();
        }
    }
    else if(_options.optInstantDxyn) {
        if(_execMode ==  eRUNNING && _breakpoints.empty() && !_options.optTraceLog) {
            for (int i = 0; i < numInstructions; ++i) {
                uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
                _rPC = (_rPC + 2) & ADDRESS_MASK;
                (this->*_opcodeHandler[opcode])(opcode);
            }
            _cycleCounter += numInstructions;
            //    Chip8EmulatorFP::executeInstructionNoBreakpoints();
        }
        else  {
            for (int i = 0; i < numInstructions; ++i)
                Chip8EmulatorFP::executeInstruction();
        }
    }
    else {
        for (int i = 0; i < numInstructions; ++i) {
            if (i && (((_memory[_rPC] << 8) | _memory[_rPC + 1]) & 0xF000) == 0xD000)
                return;
            if(_execMode == eRUNNING && _breakpoints.empty() && !_options.optTraceLog)
                Chip8EmulatorFP::executeInstructionNoBreakpoints();
            else
                Chip8EmulatorFP::executeInstruction();
        }
    }
}

inline void Chip8EmulatorFP::executeInstruction()
{
    if(_execMode == eRUNNING) {
        if(_options.optTraceLog)
            Logger::log(Logger::eCHIP8, _cycleCounter, {_frameCounter, int(_cycleCounter % 9999)}, dumpStateLine().c_str());
        uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
        ++_cycleCounter;
        _rPC = (_rPC + 2) & ADDRESS_MASK;
        (this->*_opcodeHandler[opcode])(opcode);
    }
    else {
        if (_execMode == ePAUSED || _cpuState == eERROR)
            return;
        if(_options.optTraceLog)
            Logger::log(Logger::eCHIP8, _cycleCounter, {_frameCounter, int(_cycleCounter % 9999)}, dumpStateLine().c_str());
        uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
        ++_cycleCounter;
        _rPC = (_rPC + 2) & ADDRESS_MASK;
        (this->*_opcodeHandler[opcode])(opcode);
        if (_execMode == eSTEP || (_execMode == eSTEPOVER && _rSP <= _stepOverSP)) {
            _execMode = ePAUSED;
        }
    }
    if(hasBreakPoint(_rPC)) {
        if(Chip8EmulatorBase::findBreakpoint(_rPC))
            _execMode = ePAUSED;
    }
}

uint8_t Chip8EmulatorFP::getNextMCSample()
{
    if(_isMegaChipMode && _sampleLength>0 && _execMode == eRUNNING) {
        auto val = _memory[(_sampleStart + uint32_t(_mcSamplePos)) & ADDRESS_MASK];
        double pos = _mcSamplePos + _sampleStep;
        if(pos >= _sampleLength)
            pos -= _sampleLength;
        _mcSamplePos.store(pos);
        return val;
    }
    return 127;
}

void Chip8EmulatorFP::on(uint16_t mask, uint16_t opcode, OpcodeHandler handler)
{
    uint16_t argMask = ~mask;
    int shift = 0;
    if(argMask) {
        while((argMask & 1) == 0) {
            argMask >>= 1;
            ++shift;
        }
        uint16_t val = 0;
        do {
            _opcodeHandler[opcode | ((val & argMask) << shift)] = handler;
        }
        while(++val & argMask);
    }
    else {
        _opcodeHandler[opcode] = handler;
    }
}

void Chip8EmulatorFP::opNop(uint16_t)
{
}

void Chip8EmulatorFP::opInvalid(uint16_t opcode)
{
    errorHalt();
}

void Chip8EmulatorFP::op0010(uint16_t opcode)
{
    _isMegaChipMode = false;
    clearScreen();
    ++_clearCounter;
}

void Chip8EmulatorFP::op0011(uint16_t opcode)
{
    _isMegaChipMode = true;
    clearScreen();
    ++_clearCounter;
}

void Chip8EmulatorFP::op00Bn(uint16_t opcode)
{
    auto n = (opcode & 0xf);
    if(_isMegaChipMode) {
        std::memmove(_screenBuffer32.data(), _screenBuffer32.data() + n * MAX_SCREEN_WIDTH, (_screenBuffer32.size() - n * MAX_SCREEN_WIDTH) * sizeof(uint32_t));
        const auto black = be32(0x000000FF);
        for(size_t i = 0; i < n * MAX_SCREEN_WIDTH; ++i) {
            _screenBuffer32[_screenBuffer32.size() - n * MAX_SCREEN_WIDTH + i] = black;
        }
        _host.updateScreen();
    }
    else {
        std::memmove(_screenBuffer.data(), _screenBuffer.data() + n * MAX_SCREEN_WIDTH, _screenBuffer.size() - n * MAX_SCREEN_WIDTH);
        std::memset(_screenBuffer.data() + _screenBuffer.size() - n * MAX_SCREEN_WIDTH, 0, n * MAX_SCREEN_WIDTH);
        _screenNeedsUpdate = true;
    }

}

void Chip8EmulatorFP::op00Cn(uint16_t opcode)
{
    auto n = (opcode & 0xf);
    if(_isMegaChipMode) {
        std::memmove(_screenBuffer32.data() + n * MAX_SCREEN_WIDTH, _screenBuffer32.data(), (_screenBuffer32.size() - n * MAX_SCREEN_WIDTH) * sizeof(uint32_t));
        const auto black = be32(0x000000FF);
        for(size_t i = 0; i < n * MAX_SCREEN_WIDTH; ++i) {
            _screenBuffer32[i] = black;
        }
        _host.updateScreen();
    }
    else {
        std::memmove(_screenBuffer.data() + n * MAX_SCREEN_WIDTH, _screenBuffer.data(), _screenBuffer.size() - n * MAX_SCREEN_WIDTH);
        std::memset(_screenBuffer.data(), 0, n * MAX_SCREEN_WIDTH);
        _screenNeedsUpdate = true;
    }
}



void Chip8EmulatorFP::op00Cn_masked(uint16_t opcode)
{
    auto n = (opcode & 0xf);
    if(!_isHires) n <<= 1;
    auto width = Chip8EmulatorBase::getCurrentScreenWidth();
    auto height = Chip8EmulatorBase::getCurrentScreenHeight();
    for(int sy = height - n - 1; sy >= 0; --sy) {
        for(int sx = 0; sx < width; ++sx) {
            movePixelMasked(sx, sy, sx, sy + n);
        }
    }
    for(int sy = 0; sy < n; ++sy) {
        for(int sx = 0; sx < width; ++sx) {
            clearPixelMasked(sx, sy);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::op00Dn(uint16_t opcode)
{
    auto n = (opcode & 0xf);
    std::memmove(_screenBuffer.data(), _screenBuffer.data() + n * MAX_SCREEN_WIDTH, _screenBuffer.size() - n * MAX_SCREEN_WIDTH);
    std::memset(_screenBuffer.data() + (Chip8EmulatorBase::getCurrentScreenHeight() - n) * MAX_SCREEN_WIDTH, 0, _screenBuffer.size() - (Chip8EmulatorBase::getCurrentScreenHeight() - n) * MAX_SCREEN_WIDTH);
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::op00Dn_masked(uint16_t opcode)
{
    auto n = (opcode & 0xf);
    if(!_isHires) n <<= 1;
    auto width = Chip8EmulatorBase::getCurrentScreenWidth();
    auto height = Chip8EmulatorBase::getCurrentScreenHeight();
    for(int sy = n; sy < height; ++sy) {
        for(int sx = 0; sx < width; ++sx) {
            movePixelMasked(sx, sy, sx, sy - n);
        }
    }
    for(int sy = height - n - 1; sy < height; ++sy) {
        for(int sx = 0; sx < width; ++sx) {
            clearPixelMasked(sx, sy);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::op00E0(uint16_t opcode)
{
    clearScreen();
    _screenNeedsUpdate = true;
    ++_clearCounter;
}

void Chip8EmulatorFP::op00E0_megachip(uint16_t opcode)
{
    _host.updateScreen();
    clearScreen();
    ++_clearCounter;
}

void Chip8EmulatorFP::op00EE(uint16_t opcode)
{
        _rPC = _stack[--_rSP];
    if (_execMode == eSTEPOUT)
        _execMode = ePAUSED;
}

void Chip8EmulatorFP::op00FB(uint16_t opcode)
{
    if(_isMegaChipMode) {
        const auto black = be32(0x000000FF);
        for (int y = 0; y < MAX_SCREEN_HEIGHT; ++y) {
            std::memmove(_screenBuffer32.data() + y * MAX_SCREEN_WIDTH + 4, _screenBuffer32.data() + y * MAX_SCREEN_WIDTH, (MAX_SCREEN_WIDTH - 4) * sizeof(uint32_t));
            for(size_t i = 0; i < 4; ++i)
                _screenBuffer32[y * MAX_SCREEN_WIDTH + i] = black;
        }
        _host.updateScreen();
    }
    else {
        for (int y = 0; y < MAX_SCREEN_HEIGHT; ++y) {
            std::memmove(_screenBuffer.data() + y * MAX_SCREEN_WIDTH + 4, _screenBuffer.data() + y * MAX_SCREEN_WIDTH, MAX_SCREEN_WIDTH - 4);
            std::memset(_screenBuffer.data() + y * MAX_SCREEN_WIDTH, 0, 4);
        }
        _screenNeedsUpdate = true;
    }
}

void Chip8EmulatorFP::op00FB_masked(uint16_t opcode)
{
    auto n = 4;
    if(!_isHires) n <<= 1;
    auto width = Chip8EmulatorBase::getCurrentScreenWidth();
    auto height = Chip8EmulatorBase::getCurrentScreenHeight();
    for(int sy = 0; sy < height; ++sy) {
        for(int sx = width - n - 1; sx >= 0; --sx) {
            movePixelMasked(sx, sy, sx + n, sy);
        }
        for(int sx = 0; sx < n; ++sx) {
            clearPixelMasked(sx, sy);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::op00FC(uint16_t opcode)
{
    if(_isMegaChipMode) {
        const auto black = be32(0x000000FF);
        for (int y = 0; y < MAX_SCREEN_HEIGHT; ++y) {
            std::memmove(_screenBuffer32.data() + y * MAX_SCREEN_WIDTH, _screenBuffer32.data() + y * MAX_SCREEN_WIDTH + 4, (MAX_SCREEN_WIDTH - 4) * sizeof(uint32_t));
            for(size_t i = 0; i < 4; ++i)
                _screenBuffer32[y * MAX_SCREEN_WIDTH + MAX_SCREEN_WIDTH - 4 + i] = black;
        }
        _host.updateScreen();
    }
    else {
        for (int y = 0; y < MAX_SCREEN_HEIGHT; ++y) {
            std::memmove(_screenBuffer.data() + y * MAX_SCREEN_WIDTH, _screenBuffer.data() + y * MAX_SCREEN_WIDTH + 4, MAX_SCREEN_WIDTH - 4);
            std::memset(_screenBuffer.data() + y * MAX_SCREEN_WIDTH + Chip8EmulatorBase::getCurrentScreenWidth() - 4, 0, MAX_SCREEN_WIDTH - Chip8EmulatorBase::getCurrentScreenWidth() + 4);
        }
    }
}

void Chip8EmulatorFP::op00FC_masked(uint16_t opcode)
{
    auto n = 4;
    if(!_isHires) n <<= 1;
    auto width = Chip8EmulatorBase::getCurrentScreenWidth();
    auto height = Chip8EmulatorBase::getCurrentScreenHeight();
    for(int sy = 0; sy < height; ++sy) {
        for(int sx = n; sx < width; ++sx) {
            movePixelMasked(sx, sy, sx - n, sy);
        }
        for(int sx = width - n; sx < width; ++sx) {
            clearPixelMasked(sx, sy);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::op00FD(uint16_t opcode)
{
    halt();
}

void Chip8EmulatorFP::op00FE(uint16_t opcode)
{
    _isHires = false;
}

void Chip8EmulatorFP::op00FE_withClear(uint16_t opcode)
{
    _isHires = false;
    std::memset(_screenBuffer.data(), 0, MAX_SCREEN_WIDTH * MAX_SCREEN_HEIGHT);
    _screenNeedsUpdate = true;
    ++_clearCounter;
}

void Chip8EmulatorFP::op00FE_megachip(uint16_t opcode)
{
    if(_isHires && !_isMegaChipMode) {
        _isHires = false;
        clearScreen();
        _screenNeedsUpdate = true;
        ++_clearCounter;
    }
}

void Chip8EmulatorFP::op00FF(uint16_t opcode)
{
    _isHires = true;
}

void Chip8EmulatorFP::op00FF_withClear(uint16_t opcode)
{
    _isHires = true;
    std::memset(_screenBuffer.data(), 0, MAX_SCREEN_WIDTH * MAX_SCREEN_HEIGHT);
    _screenNeedsUpdate = true;
    ++_clearCounter;
}

void Chip8EmulatorFP::op00FF_megachip(uint16_t opcode)
{
    if(!_isHires && !_isMegaChipMode) {
        _isHires = true;
        clearScreen();
        _screenNeedsUpdate = true;
        ++_clearCounter;
    }
}

void Chip8EmulatorFP::op01nn(uint16_t opcode)
{
    _rI = ((opcode & 0xFF) << 16 | (_memory[_rPC & ADDRESS_MASK] << 8) | _memory[(_rPC + 1) & ADDRESS_MASK]) & ADDRESS_MASK;
    _rPC = (_rPC + 2) & ADDRESS_MASK;
}

void Chip8EmulatorFP::op02nn(uint16_t opcode)
{
    auto numCols = opcode & 0xFF;
    std::vector<uint32_t> cols;
    cols.reserve(255);
    size_t address = _rI;
    for(size_t i = 0; i < numCols; ++i) {
        auto a = _memory[address++ & ADDRESS_MASK];
        auto r = _memory[address++ & ADDRESS_MASK];
        auto g = _memory[address++ & ADDRESS_MASK];
        auto b = _memory[address++ & ADDRESS_MASK];
        _mcPalette[i + 1] = be32((r << 24) | (g << 16) | (b << 8) | a);
        cols.push_back(be32((r << 24) | (g << 16) | (b << 8) | a));
    }
    _host.updatePalette(cols, 1);
}

void Chip8EmulatorFP::op03nn(uint16_t opcode)
{
    _spriteWidth = opcode & 0xFF;
    if(!_spriteWidth)
        _spriteWidth = 256;
}

void Chip8EmulatorFP::op04nn(uint16_t opcode)
{
    _spriteHeight = opcode & 0xFF;
    if(!_spriteHeight)
        _spriteHeight = 256;
}

void Chip8EmulatorFP::op05nn(uint16_t opcode)
{

}

void Chip8EmulatorFP::op060n(uint16_t opcode)
{
    uint32_t frequency = (_memory[_rI & ADDRESS_MASK] << 8) | _memory[(_rI + 1) & ADDRESS_MASK];
    uint32_t length = (_memory[(_rI + 2) & ADDRESS_MASK] << 16) | (_memory[(_rI + 3) & ADDRESS_MASK] << 8) | _memory[(_rI + 4) & ADDRESS_MASK];
    _sampleStart.store(_rI + 6);
    _sampleStep.store(frequency / 44100.0f);
    _sampleLength.store(length);
    _mcSamplePos.store(0);
}

void Chip8EmulatorFP::op0700(uint16_t opcode)
{
    _sampleLength.store(0);
    _mcSamplePos.store(0);
}

void Chip8EmulatorFP::op080n(uint16_t opcode)
{
    auto bm = opcode & 0xF;
    _blendMode = bm < 6 ? MegaChipBlendMode(bm) : eBLEND_NORMAL;
}

void Chip8EmulatorFP::op09nn(uint16_t opcode)
{
    _collisionColor = opcode & 0xFF;
}

void Chip8EmulatorFP::op1nnn(uint16_t opcode)
{
    if((opcode & 0xFFF) == _rPC - 2)
        _execMode = ePAUSED; 
    _rPC = opcode & 0xFFF;
}

void Chip8EmulatorFP::op2nnn(uint16_t opcode)
{
    _stack[_rSP++] = _rPC;
    _rPC = opcode & 0xFFF;
}

void Chip8EmulatorFP::op3xnn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == (opcode & 0xff)) {
        _rPC += 2;
    }
}

#define CONDITIONAL_SKIP_DISTANCE(ifOpcode,mask) ((_memory[_rPC & ADDRESS_MASK]&(mask>>8)) == (ifOpcode>>8) && (_memory[(_rPC + 1) & ADDRESS_MASK]&(mask&0xff)) == (ifOpcode&0xff) ? 4 : 2)

void Chip8EmulatorFP::op3xnn_with_F000(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == (opcode & 0xff)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000,0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::op3xnn_with_01nn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == (opcode & 0xff)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100,0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::op4xnn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != (opcode & 0xFF)) {
        _rPC += 2;
    }
}

void Chip8EmulatorFP::op4xnn_with_F000(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != (opcode & 0xFF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000, 0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::op4xnn_with_01nn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != (opcode & 0xFF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100, 0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::op5xy0(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == _rV[(opcode >> 4) & 0xF]) {
        _rPC += 2;
    }
}

void Chip8EmulatorFP::op5xy0_with_F000(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == _rV[(opcode >> 4) & 0xF]) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000, 0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::op5xy0_with_01nn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == _rV[(opcode >> 4) & 0xF]) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100, 0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::op5xy2(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    auto l = std::abs(x-y);
    for(int i=0; i <= l; ++i)
        _memory[(_rI + i) & ADDRESS_MASK] = _rV[x < y ? x + i : x - i];
    if(_rI + l >= ADDRESS_MASK)
        fixupSafetyPad();
}

void Chip8EmulatorFP::op5xy3(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    for(int i=0; i <= std::abs(x-y); ++i)
        _rV[x < y ? x + i : x - i] = _memory[(_rI + i) & ADDRESS_MASK];
}

void Chip8EmulatorFP::op5xy4(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    for(int i=0; i <= std::abs(x-y); ++i)
        _xxoPalette[x < y ? x + i : x - i] = _memory[(_rI + i) & ADDRESS_MASK];
    _host.updatePalette(_xxoPalette);
}

void Chip8EmulatorFP::op6xnn(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = opcode & 0xFF;
}

void Chip8EmulatorFP::op7xnn(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] += opcode & 0xFF;
}

void Chip8EmulatorFP::op8xy0(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF];
}

void Chip8EmulatorFP::op8xy1(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] |= _rV[(opcode >> 4) & 0xF];
    _rV[0xF] = 0;
}

void Chip8EmulatorFP::op8xy1_dontResetVf(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] |= _rV[(opcode >> 4) & 0xF];
}

void Chip8EmulatorFP::op8xy2(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] &= _rV[(opcode >> 4) & 0xF];
    _rV[0xF] = 0;
}

void Chip8EmulatorFP::op8xy2_dontResetVf(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] &= _rV[(opcode >> 4) & 0xF];
}

void Chip8EmulatorFP::op8xy3(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] ^= _rV[(opcode >> 4) & 0xF];
    _rV[0xF] = 0;
}

void Chip8EmulatorFP::op8xy3_dontResetVf(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] ^= _rV[(opcode >> 4) & 0xF];
}

void Chip8EmulatorFP::op8xy4(uint16_t opcode)
{   
    uint16_t result = _rV[(opcode >> 8) & 0xF] + _rV[(opcode >> 4) & 0xF];
    _rV[(opcode >> 8) & 0xF] = result;
    _rV[0xF] = result>>8;
}

void Chip8EmulatorFP::op8xy5(uint16_t opcode)
{   
    uint16_t result = _rV[(opcode >> 8) & 0xF] - _rV[(opcode >> 4) & 0xF];
    _rV[(opcode >> 8) & 0xF] = result;
    _rV[0xF] = result > 255 ? 0 : 1;
}

void Chip8EmulatorFP::op8xy6(uint16_t opcode)
{   
    uint8_t carry = _rV[(opcode >> 4) & 0xF] & 1;
    _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] >> 1;
    _rV[0xF] = carry;
}

void Chip8EmulatorFP::op8xy6_justShiftVx(uint16_t opcode)
{   
    uint8_t carry = _rV[(opcode >> 8) & 0xF] & 1;
    _rV[(opcode >> 8) & 0xF] >>= 1;
    _rV[0xF] = carry;
}

void Chip8EmulatorFP::op8xy7(uint16_t opcode)
{   
    uint16_t result = _rV[(opcode >> 4) & 0xF] - _rV[(opcode >> 8) & 0xF];
    _rV[(opcode >> 8) & 0xF] = result;
    _rV[0xF] = result > 255 ? 0 : 1;
}

void Chip8EmulatorFP::op8xyE(uint16_t opcode)
{
#if 1
    uint8_t carry = _rV[(opcode >> 4) & 0xF] >> 7;
    _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] << 1;
    _rV[0xF] = carry;
#else
    _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] << 1;
    _rV[0xF] = _rV[(opcode >> 4) & 0xF] >> 7;
#endif
}

void Chip8EmulatorFP::op8xyE_justShiftVx(uint16_t opcode)
{   
    uint8_t carry = _rV[(opcode >> 8) & 0xF] >> 7;
    _rV[(opcode >> 8) & 0xF] <<= 1;
    _rV[0xF] = carry;
}

void Chip8EmulatorFP::op9xy0(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != _rV[(opcode >> 4) & 0xF]) {
        _rPC += 2;
    }
}

void Chip8EmulatorFP::op9xy0_with_F000(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != _rV[(opcode >> 4) & 0xF]) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000, 0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::op9xy0_with_01nn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != _rV[(opcode >> 4) & 0xF]) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100, 0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::opAnnn(uint16_t opcode)
{
    _rI = opcode & 0xFFF;
}

void Chip8EmulatorFP::opBnnn(uint16_t opcode)
{
    _rPC = (_rV[0] + (opcode & 0xFFF)) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opBxnn(uint16_t opcode)
{
    _rPC = (_rV[(opcode >> 8) & 0xF] + (opcode & 0xFFF)) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opBxyn(uint16_t opcode)
{
    // TODO: CHIP-8X
}


inline uint8_t classicRand(uint32_t& state)
{
    state = ((state * 1103515245) + 12345) & 0x7FFFFFFF;
    return state >> 16;
}

inline uint8_t countingRand(uint32_t& state)
{
    return state++;
}

void Chip8EmulatorFP::opCxnn(uint16_t opcode)
{
    if(_options.behaviorBase < emu::Chip8EmulatorOptions::eSCHIP10) {
        ++_randomSeed;
        uint16_t val = _randomSeed >> 8;
        val += _chip8_cosmac_vip[0x100 + (_randomSeed & 0xFF)];
        uint8_t result = val;
        val >>= 1;
        val += result;
        _randomSeed = (_randomSeed & 0xFF) | (val << 8);
        result = val & (opcode & 0xFF);
        _rV[(opcode >> 8) & 0xF] = result;
    }
    else {
        _rV[(opcode >> 8) & 0xF] = (rand() >> 4) & (opcode & 0xFF);
    }
}

void Chip8EmulatorFP::opCxnn_randLCG(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = classicRand(_simpleRandState) & (opcode & 0xFF);
}

void Chip8EmulatorFP::opCxnn_counting(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = countingRand(_simpleRandState) & (opcode & 0xFF);
}

static void blendColorsAlpha(uint32_t* dest, const uint32_t* col, uint8_t alpha)
{
    int a = alpha;
    auto* dst = (uint8_t*)dest;
    const auto* c1 = dst;
    const auto* c2 = (const uint8_t*)col;
    *dst++ = (a * *c2++ + (255 - a) * *c1++) >> 8;
    *dst++ = (a * *c2++ + (255 - a) * *c1++) >> 8;
    *dst++ = (a * *c2 + (255 - a) * *c1) >> 8;
    *dst = 255;
}

static void blendColorsAdd(uint32_t* dest, const uint32_t* col)
{
    auto* dst = (uint8_t*)dest;
    const auto* c1 = dst;
    const auto* c2 = (const uint8_t*)col;
    *dst++ = std::min((int)*c1++ + *c2++, 255);
    *dst++ = std::min((int)*c1++ + *c2++, 255);
    *dst++ = std::min((int)*c1 + *c2, 255);
    *dst = 255;
}

static void blendColorsMul(uint32_t* dest, const uint32_t* col)
{
    auto* dst = (uint8_t*)dest;
    const auto* c1 = dst;
    const auto* c2 = (const uint8_t*)col;
    *dst++ = (int)*c1++ * *c2++ / 255;
    *dst++ = (int)*c1++ * *c2++ / 255;
    *dst++ = (int)*c1 * *c2 / 255;
    *dst = 255;
}

void Chip8EmulatorFP::opDxyn_megaChip(uint16_t opcode)
{
    if(!_isMegaChipMode)
        opDxyn<HiresSupport>(opcode);
    else {
        int xpos = _rV[(opcode >> 8) & 0xF];
        int ypos = _rV[(opcode >> 4) & 0xF];
        _rV[0xF] = 0;
        if(ypos >= 192)
            return;
        for(int y = 0; y < _spriteHeight && ypos + y < 192; ++y) {
            uint8_t* pixelBuffer = _screenBuffer.data() + (ypos + y) * 256 + xpos;
            uint32_t* pixelBuffer32 = _screenBuffer32.data() + (ypos + y) * 256 + xpos;
            for(int x = 0; x < _spriteWidth && xpos + x < 256; ++x, ++pixelBuffer, ++pixelBuffer32) {
                auto col = _memory[(_rI + y * _spriteWidth + x) & ADDRESS_MASK];
                if(col) {
                    if(*pixelBuffer == _collisionColor)
                        _rV[0xF] = 1;
                    *pixelBuffer = col;
                    switch (_blendMode) {
                        case Chip8EmulatorBase::eBLEND_ALPHA_25:
                            blendColorsAlpha(pixelBuffer32, &_mcPalette[col], 63);
                            break;
                        case Chip8EmulatorBase::eBLEND_ALPHA_50:
                            blendColorsAlpha(pixelBuffer32, &_mcPalette[col], 127);
                            break;
                        case Chip8EmulatorBase::eBLEND_ALPHA_75:
                            blendColorsAlpha(pixelBuffer32, &_mcPalette[col], 191);
                            break;
                        case Chip8EmulatorBase::eBLEND_ADD:
                            blendColorsAdd(pixelBuffer32, &_mcPalette[col]);
                            break;
                        case Chip8EmulatorBase::eBLEND_MUL:
                            blendColorsMul(pixelBuffer32, &_mcPalette[col]);
                            break;

                        case Chip8EmulatorBase::eBLEND_NORMAL:
                        default:
                            *pixelBuffer32 = _mcPalette[col];
                            break;
                    }
                }
            }
        }
    }
}

void Chip8EmulatorFP::opEx9E(uint16_t opcode)
{
    if (_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC += 2;
    }
}

void Chip8EmulatorFP::opEx9E_with_F000(uint16_t opcode)
{
    if (_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000, 0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::opEx9E_with_01nn(uint16_t opcode)
{
    if (_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100, 0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::opExA1(uint16_t opcode)
{
    if (_host.isKeyUp(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC += 2;
    }
}

void Chip8EmulatorFP::opExA1_with_F000(uint16_t opcode)
{
    if (_host.isKeyUp(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000, 0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::opExA1_with_01nn(uint16_t opcode)
{
    if (_host.isKeyUp(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100, 0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::opF000(uint16_t opcode)
{
    _rI = ((_memory[_rPC & ADDRESS_MASK] << 8) | _memory[(_rPC + 1) & ADDRESS_MASK]) & ADDRESS_MASK;
    _rPC = (_rPC + 2) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opF002(uint16_t opcode)
{
    for(int i = 0; i < 16; ++i) {
        _xoAudioPattern[i] = _memory[(_rI + i) & ADDRESS_MASK];
    }
}

void Chip8EmulatorFP::opFx01(uint16_t opcode)
{
    _planes = (opcode >> 8) & 0xF;
}

void Chip8EmulatorFP::opFx07(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = _rDT;
}

void Chip8EmulatorFP::opFx0A(uint16_t opcode)
{
    auto key = _host.getKeyPressed();
    if (key) {
        _rV[(opcode >> 8) & 0xF] = key - 1;
        _cpuState = eNORMAL;
    }
    else {
        // keep waiting...
        _rPC -= 2;
        --_cycleCounter;
        if(_isMegaChipMode && _cpuState != eWAITING)
            _host.updateScreen();
        _cpuState = eWAITING;
    }
}

void Chip8EmulatorFP::opFx15(uint16_t opcode)
{
    _rDT = _rV[(opcode >> 8) & 0xF];
}

void Chip8EmulatorFP::opFx18(uint16_t opcode)
{
    _rST = _rV[(opcode >> 8) & 0xF];
    if(!_rST) _wavePhase = 0;
}

void Chip8EmulatorFP::opFx1E(uint16_t opcode)
{
    _rI = (_rI + _rV[(opcode >> 8) & 0xF]) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx29(uint16_t opcode)
{
    _rI = (_rV[(opcode >> 8) & 0xF] & 0xF) * 5;
}

void Chip8EmulatorFP::opFx29_ship10Beta(uint16_t opcode)
{
    auto n = _rV[(opcode >> 8) & 0xF];
    _rI = (n >= 10 && n <=19) ? (n-10) * 10 + 16*5 : (n & 0xF) * 5;
}

void Chip8EmulatorFP::opFx30(uint16_t opcode)
{
    _rI = (_rV[(opcode >> 8) & 0xF] & 0xF) * 10 + 16*5;
}

void Chip8EmulatorFP::opFx33(uint16_t opcode)
{
    uint8_t val = _rV[(opcode >> 8) & 0xF];
    _memory[_rI & ADDRESS_MASK] = val / 100;
    _memory[(_rI + 1) & ADDRESS_MASK] = (val / 10) % 10;
    _memory[(_rI + 2) & ADDRESS_MASK] = val % 10;
}

void Chip8EmulatorFP::opFx3A(uint16_t opcode)
{
    _xoPitch.store(_rV[(opcode >> 8) & 0xF]);
}

void Chip8EmulatorFP::opFx55(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _memory[(_rI + i) & ADDRESS_MASK] = _rV[i];
    }
    if(_rI + upto > ADDRESS_MASK)
        fixupSafetyPad();
    _rI = (_rI + upto + 1) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx55_loadStoreIncIByX(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _memory[(_rI + i) & ADDRESS_MASK] = _rV[i];
    }
    if(_rI + upto > ADDRESS_MASK)
        fixupSafetyPad();
    _rI = (_rI + upto) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx55_loadStoreDontIncI(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _memory[(_rI + i) & ADDRESS_MASK] = _rV[i];
    }
    if(_rI + upto > ADDRESS_MASK)
        fixupSafetyPad();
}

void Chip8EmulatorFP::opFx65(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = _memory[(_rI + i) & ADDRESS_MASK];
    }
    _rI = (_rI + upto + 1) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx65_loadStoreIncIByX(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = _memory[(_rI + i) & ADDRESS_MASK];
    }
    _rI = (_rI + upto) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx65_loadStoreDontIncI(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = _memory[(_rI + i) & ADDRESS_MASK];
    }
}

static uint8_t registerSpace[16]{};

void Chip8EmulatorFP::opFx75(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        registerSpace[i] = _rV[i];
    }
}

void Chip8EmulatorFP::opFx85(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = registerSpace[i];
    }
}

}
