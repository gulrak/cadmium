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

namespace emu
{

Chip8EmulatorFP::Chip8EmulatorFP(Chip8EmulatorHost& host, Chip8EmulatorOptions& options, const Chip8EmulatorBase* other)
: Chip8EmulatorBase(host, options, other)
, ADDRESS_MASK(options.optHas16BitAddr ? 0xFFFF : 0xFFF)
, SCREEN_WIDTH(options.optAllowHires ? 128 : 64)
, SCREEN_HEIGHT(options.optAllowHires ? 64 : 32)
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
            else
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<0>);
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
            //on(0xF0FF, 0xF075, &Chip8EmulatorFP::opFx75);
            //on(0xF0FF, 0xF085, &Chip8EmulatorFP::opFx85);
            break;
        case Chip8EmulatorOptions::eSCHIP11:
            on(0xFFF0, 0x00C0, &Chip8EmulatorFP::op00Cn);
            on(0xFFFF, 0x00FB, &Chip8EmulatorFP::op00FB);
            on(0xFFFF, 0x00FC, &Chip8EmulatorFP::op00FC);
            on(0xFFFF, 0x00FD, &Chip8EmulatorFP::op00FD);
            on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE);
            on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF);
            on(0xF0FF, 0xF030, &Chip8EmulatorFP::opFx30);
            //on(0xF0FF, 0xF075, &Chip8EmulatorFP::opFx75);
            //on(0xF0FF, 0xF085, &Chip8EmulatorFP::opFx85);
            break;
        case Chip8EmulatorOptions::eXOCHIP:
            on(0xFFF0, 0x00C0, &Chip8EmulatorFP::op00Cn);
            on(0xFFF0, 0x00D0, &Chip8EmulatorFP::op00Dn);
            on(0xFFFF, 0x00FB, &Chip8EmulatorFP::op00FB);
            on(0xFFFF, 0x00FC, &Chip8EmulatorFP::op00FC);
            on(0xFFFF, 0x00FD, &Chip8EmulatorFP::op00FD);
            on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE_withClear);
            on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF_withClear);
            on(0xF00F, 0x5002, &Chip8EmulatorFP::op5xy2);
            on(0xF00F, 0x5003, &Chip8EmulatorFP::op5xy3);
            on(0xFFFF, 0xF000, &Chip8EmulatorFP::opF000);
            on(0xF0FF, 0xF001, &Chip8EmulatorFP::opFx01);
            on(0xFFFF, 0xF002, &Chip8EmulatorFP::opF002);
            on(0xF0FF, 0xF030, &Chip8EmulatorFP::opFx30);
            on(0xF0FF, 0xF03A, &Chip8EmulatorFP::opFx3A);
            //on(0xF0FF, 0xF075, &Chip8EmulatorFP::opFx75);
            //on(0xF0FF, 0xF085, &Chip8EmulatorFP::opFx85);
            break;
        case Chip8EmulatorOptions::eCHICUEYI:
            on(0xFFF0, 0x00C0, &Chip8EmulatorFP::op00Cn);
            on(0xFFF0, 0x00D0, &Chip8EmulatorFP::op00Dn);
            on(0xFFFF, 0x00FB, &Chip8EmulatorFP::op00FB);
            on(0xFFFF, 0x00FC, &Chip8EmulatorFP::op00FC);
            on(0xFFFF, 0x00FD, &Chip8EmulatorFP::op00FD);
            on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE_withClear);
            on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF_withClear);
            on(0xF00F, 0x5002, &Chip8EmulatorFP::op5xy2);
            on(0xF00F, 0x5003, &Chip8EmulatorFP::op5xy3);
            on(0xF00F, 0x5004, &Chip8EmulatorFP::op5xy4);
            on(0xFFFF, 0xF000, &Chip8EmulatorFP::opF000);
            on(0xF0FF, 0xF001, &Chip8EmulatorFP::opFx01);
            on(0xFFFF, 0xF002, &Chip8EmulatorFP::opF002);
            on(0xF0FF, 0xF030, &Chip8EmulatorFP::opFx30);
            on(0xF0FF, 0xF03A, &Chip8EmulatorFP::opFx3A);

        default: break;
    }
}

Chip8EmulatorFP::~Chip8EmulatorFP()
{
}

void Chip8EmulatorFP::executeInstructions(int numInstructions)
{
    if(_options.optInstantDxyn) {
        for (int i = 0; i < numInstructions; ++i)
            Chip8EmulatorFP::executeInstruction();
    }
    else {
        for (int i = 0; i < numInstructions; ++i) {
            if (i && (((_memory[_rPC] << 8) | _memory[_rPC + 1]) & 0xF000) == 0xD000)
                return;
            Chip8EmulatorFP::executeInstruction();
        }
    }
}

inline void Chip8EmulatorFP::executeInstruction()
{
    if(_execMode == eRUNNING) {
        uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
        ++_cycleCounter;
        _rPC = (_rPC + 2) & ADDRESS_MASK;
        (this->*_opcodeHandler[opcode])(opcode);
    }
    else {
        if (_execMode == ePAUSED || _cpuState == eERROR)
            return;
        uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
        ++_cycleCounter;
        _rPC = (_rPC + 2) & ADDRESS_MASK;
        (this->*_opcodeHandler[opcode])(opcode);
        if (_execMode == eSTEP || (_execMode == eSTEPOVER && _rSP <= _stepOverSP)) {
            _execMode = ePAUSED;
        }
    }
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
    _cpuState = eERROR;
    halt();
}

void Chip8EmulatorFP::op00Cn(uint16_t opcode)
{
    auto n = (opcode & 0xf);
    std::memmove(_screenBuffer.data() + n * MAX_SCREEN_WIDTH, _screenBuffer.data(), _screenBuffer.size() - n * MAX_SCREEN_WIDTH);
    std::memset(_screenBuffer.data(), 0, n * MAX_SCREEN_WIDTH);
}

void Chip8EmulatorFP::op00Dn(uint16_t opcode)
{
    auto n = (opcode & 0xf);
    std::memmove(_screenBuffer.data(), _screenBuffer.data() + n * MAX_SCREEN_WIDTH, _screenBuffer.size() - n * MAX_SCREEN_WIDTH);
    std::memset(_screenBuffer.data() + _screenBuffer.size() - n * MAX_SCREEN_WIDTH, 0, n * MAX_SCREEN_WIDTH);
}

void Chip8EmulatorFP::op00E0(uint16_t opcode)
{
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
    for(int y = 0; y < MAX_SCREEN_HEIGHT; ++y) {
        std::memmove(_screenBuffer.data() + y * MAX_SCREEN_WIDTH + 4, _screenBuffer.data() + y * MAX_SCREEN_WIDTH, MAX_SCREEN_WIDTH - 4);
        std::memset(_screenBuffer.data() + y * MAX_SCREEN_WIDTH, 0, 4);
    }
}

void Chip8EmulatorFP::op00FC(uint16_t opcode)
{
    for(int y = 0; y < MAX_SCREEN_HEIGHT; ++y) {
        std::memmove(_screenBuffer.data() + y * MAX_SCREEN_WIDTH, _screenBuffer.data() + y * MAX_SCREEN_WIDTH + 4, MAX_SCREEN_WIDTH - 4);
        std::memset(_screenBuffer.data() + y * MAX_SCREEN_WIDTH + MAX_SCREEN_WIDTH - 4, 0, 4);
    }
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
    if(_isHires) {
        _isHires = false;
        clearScreen();
        ++_clearCounter;
    }
}

void Chip8EmulatorFP::op00FF(uint16_t opcode)
{
    _isHires = true;
}

void Chip8EmulatorFP::op00FF_withClear(uint16_t opcode)
{
    if(!_isHires) {
        _isHires = true;
        clearScreen();
        ++_clearCounter;
    }
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

void Chip8EmulatorFP::op4xnn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != (opcode & 0xFF)) {
        _rPC += 2;
    }
}

void Chip8EmulatorFP::op5xy0(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == _rV[(opcode >> 4) & 0xF]) {
        _rPC += 2;
    }
}

void Chip8EmulatorFP::op5xy2(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    for(int i=0; i <= std::abs(x-y); ++i)
        _memory[(_rI + i) & ADDRESS_MASK] = _rV[x < y ? x + i : x - i];
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
    uint8_t carry = _rV[(opcode >> 4) & 0xF] >> 7;
    _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] << 1;
    _rV[0xF] = carry;
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

void Chip8EmulatorFP::opCxnn(uint16_t opcode)
{
    ++_randomSeed;
    uint16_t val = _randomSeed>>8;
    val += _chip8_cosmac_vip[0x100 + (_randomSeed&0xFF)];
    uint8_t result = val;
    val >>= 1;
    val += result;
    _randomSeed = (_randomSeed & 0xFF) | (val << 8);
    result = val & (opcode & 0xFF);
    _rV[(opcode >> 8) & 0xF] = result; // GetRandomValue(0, 255) & (opcode & 0xFF);
}

void Chip8EmulatorFP::opEx9E(uint16_t opcode)
{
    if (_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC += 2;
    }
}

void Chip8EmulatorFP::opExA1(uint16_t opcode)
{
    if (!_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC += 2;
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
    _rI = (_rI + upto + 1) & ADDRESS_MASK;
}


void Chip8EmulatorFP::opFx55_loadStoreIncIByX(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _memory[(_rI + i) & ADDRESS_MASK] = _rV[i];
    }
    _rI = (_rI + upto) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx55_loadStoreDontIncI(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _memory[(_rI + i) & ADDRESS_MASK] = _rV[i];
    }
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


}
