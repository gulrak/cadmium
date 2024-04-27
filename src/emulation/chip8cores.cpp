//---------------------------------------------------------------------------------------
// src/emulation/chip8cores.cpp
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

//#define ALIEN_INV8SION_BENCH

namespace emu
{

Chip8EmulatorFP::Chip8EmulatorFP(Chip8EmulatorHost& host, Chip8EmulatorOptions& options, IChip8Emulator* other)
: Chip8EmulatorBase(host, options, other)
, ADDRESS_MASK(options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP ? 0xFFFFFF : options.optHas16BitAddr ? 0xFFFF : 0xFFF)
, SCREEN_WIDTH(options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP ? 256 : options.optAllowHires ? 128 : 64)
, SCREEN_HEIGHT(options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP ? 192 : options.optAllowHires ? 64 : 32)
, _opcodeHandler(0x10000, &Chip8EmulatorFP::opInvalid)
{
    _screen.setMode(SCREEN_WIDTH, SCREEN_HEIGHT);
    _screenRGBA1.setMode(SCREEN_WIDTH, SCREEN_HEIGHT);
    _screenRGBA2.setMode(SCREEN_WIDTH, SCREEN_HEIGHT);
    setHandler();
    if(!other) {
        reset();
    }
}


void Chip8EmulatorFP::setHandler()
{
    on(0xFFFF, 0x00E0, &Chip8EmulatorFP::op00E0);
    on(0xFFFF, 0x00EE, _options.optCyclicStack ? &Chip8EmulatorFP::op00EE_cyclic : &Chip8EmulatorFP::op00EE);
    on(0xF000, 0x1000, &Chip8EmulatorFP::op1nnn);
    on(0xF000, 0x2000, _options.optCyclicStack ? &Chip8EmulatorFP::op2nnn_cyclic : &Chip8EmulatorFP::op2nnn);
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
    if(_options.behaviorBase != Chip8EmulatorOptions::eCHIP8X)
        on(0xF000, 0xB000, _options.optJump0Bxnn ? &Chip8EmulatorFP::opBxnn : &Chip8EmulatorFP::opBnnn);
    std::string randomGen;
    if(_options.advanced.contains("random")) {
        randomGen = _options.advanced.at("random");
        _randomSeed = _options.advanced.at("seed");
    }
    if(randomGen == "rand-lcg")
        on(0xF000, 0xC000, &Chip8EmulatorFP::opCxnn_randLCG);
    else if(randomGen == "counting")
        on(0xF000, 0xC000, &Chip8EmulatorFP::opCxnn_counting);
    else
        on(0xF000, 0xC000, &Chip8EmulatorFP::opCxnn);
    if(_options.behaviorBase == Chip8EmulatorOptions::eCHIP8X) {
        if(_options.optInstantDxyn)
            on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<0>);
        else
            on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn_displayWait<0>);
    }
    else if(_options.optAllowHires) {
        if(_options.optAllowColors) {
            if (_options.optWrapSprites)
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<HiresSupport|MultiColor|WrapSprite>);
            else
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<HiresSupport|MultiColor>);
        }
        else {
            if (_options.optWrapSprites)
                on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<HiresSupport|WrapSprite>);
            else {
                if (_options.optSCLoresDrawing)
                    on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<HiresSupport|SChip1xLoresDraw>);
                else
                    on(0xF000, 0xD000, &Chip8EmulatorFP::opDxyn<HiresSupport>);
            }
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
            if(_options.optModeChangeClear) {
                on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE_withClear);
                on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF_withClear);
            }
            else {
                on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE);
                on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF);
            }
            on(0xF0FF, 0xF029, &Chip8EmulatorFP::opFx29_ship10Beta);
            on(0xF0FF, 0xF075, &Chip8EmulatorFP::opFx75);
            on(0xF0FF, 0xF085, &Chip8EmulatorFP::opFx85);
            break;
        case Chip8EmulatorOptions::eCHIP8E:
            on(0xFFFF, 0x00ED, &Chip8EmulatorFP::op00ED_c8e);
            on(0xFFFF, 0x00F2, &Chip8EmulatorFP::opNop);
            on(0xFFFF, 0x0151, &Chip8EmulatorFP::op0151_c8e);
            on(0xFFFF, 0x0188, &Chip8EmulatorFP::op0188_c8e);
            on(0xF00F, 0x5001, &Chip8EmulatorFP::op5xy1_c8e);
            on(0xF00F, 0x5002, &Chip8EmulatorFP::op5xy2_c8e);
            on(0xF00F, 0x5003, &Chip8EmulatorFP::op5xy3_c8e);
            on(0xFF00, 0xBB00, &Chip8EmulatorFP::opBBnn_c8e);
            on(0xFF00, 0xBF00, &Chip8EmulatorFP::opBFnn_c8e);
            on(0xF0FF, 0xF003, &Chip8EmulatorFP::opNop);
            on(0xF0FF, 0xF01B, &Chip8EmulatorFP::opFx1B_c8e);
            on(0xF0FF, 0xF04F, &Chip8EmulatorFP::opFx4F_c8e);
            on(0xF0FF, 0xF0E3, &Chip8EmulatorFP::opNop);
            on(0xF0FF, 0xF0E7, &Chip8EmulatorFP::opNop);
            break;
        case Chip8EmulatorOptions::eCHIP8X:
            on(0xFFFF, 0x02A0, &Chip8EmulatorFP::op02A0_c8x);
            on(0xF00F, 0x5001, &Chip8EmulatorFP::op5xy1_c8x);
            on(0xF000, 0xB000, &Chip8EmulatorFP::opBxyn_c8x);
            on(0xF00F, 0xB000, &Chip8EmulatorFP::opBxy0_c8x);
            on(0xF0FF, 0xE0F2, &Chip8EmulatorFP::opExF2_c8x);
            on(0xF0FF, 0xE0F5, &Chip8EmulatorFP::opExF5_c8x);
            on(0xF0FF, 0xF0F8, &Chip8EmulatorFP::opFxF8_c8x);
            on(0xF0FF, 0xF0FB, &Chip8EmulatorFP::opFxFB_c8x);
            break;
        case Chip8EmulatorOptions::eSCHIP11:
        case Chip8EmulatorOptions::eSCHPC:
        case Chip8EmulatorOptions::eSCHIP_MODERN:
            on(0xFFF0, 0x00C0, &Chip8EmulatorFP::op00Cn);
            on(0xFFFF, 0x00C0, &Chip8EmulatorFP::opInvalid);
            on(0xFFFF, 0x00FB, &Chip8EmulatorFP::op00FB);
            on(0xFFFF, 0x00FC, &Chip8EmulatorFP::op00FC);
            on(0xFFFF, 0x00FD, &Chip8EmulatorFP::op00FD);
            if(_options.optModeChangeClear) {
                on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE_withClear);
                on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF_withClear);
            }
            else {
                on(0xFFFF, 0x00FE, &Chip8EmulatorFP::op00FE);
                on(0xFFFF, 0x00FF, &Chip8EmulatorFP::op00FF);
            }
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
#ifdef GEN_OPCODE_STATS
    std::vector<std::pair<uint16_t,int64_t>> result;
    for(const auto& p : _opcodeStats)
        result.push_back(p);
    std::sort(result.begin(), result.end(), [](const auto& p1, const auto& p2){ return p1.second < p2.second;});
    std::cout << "Opcode statistics:" << std::endl;
    for(const auto& p : result) {
        std::cout << fmt::format("{:04X}: {}", p.first, p.second) << std::endl;
    }
#endif
}

void Chip8EmulatorFP::reset()
{
    Chip8EmulatorBase::reset();
    _simpleRandState = _simpleRandSeed;
    if(_options.behaviorBase == Chip8EmulatorOptions::eCHIP8X) {
        _screen.setOverlayCellHeight(-1); // reset
        _chip8xBackgroundColor = 0;
    }
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
    auto start = _cycleCounter;
    if(_isMegaChipMode) {
        if(_execMode == eRUNNING) {
            auto end = _cycleCounter + numInstructions;
            while (_execMode == eRUNNING && _cycleCounter < end) {
                if (_breakpoints.empty() && !_options.optTraceLog)
                    Chip8EmulatorFP::executeInstructionNoBreakpoints();
                else
                    Chip8EmulatorFP::executeInstruction();
            }
        }
        else {
            for (int i = 0; i < numInstructions; ++i)
                Chip8EmulatorFP::executeInstruction();
        }
    }
    else if(_isInstantDxyn) {
        if(_execMode ==  eRUNNING && _breakpoints.empty() && !_options.optTraceLog) {
            for (int i = 0; i < numInstructions; ++i) {
                uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
                _rPC = (_rPC + 2) & ADDRESS_MASK;
#ifdef GEN_OPCODE_STATS
                if((opcode & 0xF000) == 0xD000)
                    _opcodeStats[opcode & 0xF00F]++;
                else {
                    const auto* info = _opcodeSet.getOpcodeInfo(opcode);
                    if(info)
                        _opcodeStats[info->opcode]++;
                }
#endif
                (this->*_opcodeHandler[opcode])(opcode);
                if(_cpuState == eWAITING) {
                    _cycleCounter += numInstructions - i;
                    break;
                }
                _cycleCounter++;
            }
            //_cycleCounter += numInstructions;
            //    Chip8EmulatorFP::executeInstructionNoBreakpoints();
        }
        else  {
            for (int i = 0; i < numInstructions; ++i)
                Chip8EmulatorFP::executeInstruction();
        }
    }
    else {
        for (int i = 0; i < numInstructions; ++i) {
            //if (i && (((_memory[_rPC] << 8) | _memory[_rPC + 1]) & 0xF000) == 0xD000) {
            //    _cycleCounter = calcNextFrame();
            //    _systemTime.addCycles(_cycleCounter - start);
            //    return;
            //}
            if(_execMode == eRUNNING && _breakpoints.empty() && !_options.optTraceLog)
                Chip8EmulatorFP::executeInstructionNoBreakpoints();
            else
                Chip8EmulatorFP::executeInstruction();
        }
    }
    _systemTime.addCycles(_cycleCounter - start);
}

inline void Chip8EmulatorFP::executeInstruction()
{
    if(_execMode == eRUNNING) {
        if(_options.optTraceLog && _cpuState != eWAITING)
            Logger::log(Logger::eCHIP8, _cycleCounter, {_frameCounter, int(_cycleCounter % 9999)}, dumpStateLine().c_str());
        uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
        _rPC = (_rPC + 2) & ADDRESS_MASK;
#ifdef GEN_OPCODE_STATS
        if((opcode & 0xF000) == 0xD000)
            _opcodeStats[opcode & 0xF00F]++;
        else {
            const auto* info = _opcodeSet.getOpcodeInfo(opcode);
            if(info)
                _opcodeStats[info->opcode]++;
        }
#endif
        (this->*_opcodeHandler[opcode])(opcode);
        ++_cycleCounter;
    }
    else {
        if (_execMode == ePAUSED || _cpuState == eERROR)
            return;
        if(_options.optTraceLog)
            Logger::log(Logger::eCHIP8, _cycleCounter, {_frameCounter, int(_cycleCounter % 9999)}, dumpStateLine().c_str());
        uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
        _rPC = (_rPC + 2) & ADDRESS_MASK;
        (this->*_opcodeHandler[opcode])(opcode);
        ++_cycleCounter;
        if (_execMode == eSTEP || (_execMode == eSTEPOVER && _rSP <= _stepOverSP)) {
            _execMode = ePAUSED;
        }
    }
    if(hasBreakPoint(_rPC)) {
        if(Chip8EmulatorBase::findBreakpoint(_rPC)) {
            _execMode = ePAUSED;
            _breakpointTriggered = true;
        }
    }
}

uint8_t Chip8EmulatorFP::getNextMCSample()
{
    if(_isMegaChipMode && _sampleLength>0 && _execMode == eRUNNING) {
        auto val = _memory[(_sampleStart + uint32_t(_mcSamplePos)) & ADDRESS_MASK];
        double pos = _mcSamplePos + _sampleStep;
        if(pos >= _sampleLength) {
            if(_sampleLoop)
                pos -= _sampleLength;
            else
                pos = _sampleLength = 0, val = 128;
        }
        _mcSamplePos.store(pos);
        return val;
    }
    return 128;
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
    errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
}

void Chip8EmulatorFP::op0010(uint16_t opcode)
{
    _isMegaChipMode = false;
    _host.preClear();
    clearScreen();
    ++_clearCounter;
}

void Chip8EmulatorFP::op0011(uint16_t opcode)
{
    _isMegaChipMode = true;
    _host.preClear();
    clearScreen();
    ++_clearCounter;
}

void Chip8EmulatorFP::op00Bn(uint16_t opcode)
{ // Scroll UP
    auto n = (opcode & 0xf);
    if(_isMegaChipMode) {
        _screen.scrollUp(n);
        _screenRGBA->scrollUp(n);
        _host.updateScreen();
    }
    else {
        _screen.scrollUp(_isHires || _options.optHalfPixelScroll ? n : (n<<1));
        _screenNeedsUpdate = true;
    }

}

void Chip8EmulatorFP::op00Cn(uint16_t opcode)
{ // Scroll DOWN
    auto n = (opcode & 0xf);
    if(_isMegaChipMode) {
        _screen.scrollDown(n);
        _screenRGBA->scrollDown(n);
        _host.updateScreen();
    }
    else {
        _screen.scrollDown(_isHires || _options.optHalfPixelScroll ? n : (n<<1));
        _screenNeedsUpdate = true;
    }
}

void Chip8EmulatorFP::op00Cn_masked(uint16_t opcode)
{ // Scroll DOWN masked
    auto n = (opcode & 0xf);
    if(!_isHires) n <<= 1;
    auto width = Chip8EmulatorBase::getCurrentScreenWidth();
    auto height = Chip8EmulatorBase::getCurrentScreenHeight();
    for(int sy = height - n - 1; sy >= 0; --sy) {
        for(int sx = 0; sx < width; ++sx) {
            _screen.movePixelMasked(sx, sy, sx, sy + n, _planes);
        }
    }
    for(int sy = 0; sy < n; ++sy) {
        for(int sx = 0; sx < width; ++sx) {
            _screen.clearPixelMasked(sx, sy, _planes);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::op00Dn(uint16_t opcode)
{ // Scroll UP
    auto n = (opcode & 0xf);
    _screen.scrollUp(_isHires || _options.optHalfPixelScroll ? n : (n<<1));
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::op00Dn_masked(uint16_t opcode)
{ // Scroll UP masked
    auto n = (opcode & 0xf);
    if(!_isHires) n <<= 1;
    auto width = Chip8EmulatorBase::getCurrentScreenWidth();
    auto height = Chip8EmulatorBase::getCurrentScreenHeight();
    for(int sy = n; sy < height; ++sy) {
        for(int sx = 0; sx < width; ++sx) {
            _screen.movePixelMasked(sx, sy, sx, sy - n, _planes);
        }
    }
    for(int sy = height - n; sy < height; ++sy) {
        for(int sx = 0; sx < width; ++sx) {
            _screen.clearPixelMasked(sx, sy, _planes);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::op00E0(uint16_t opcode)
{
    _host.preClear();
    clearScreen();
    _screenNeedsUpdate = true;
    ++_clearCounter;
}

void Chip8EmulatorFP::op00E0_megachip(uint16_t opcode)
{
    _host.preClear();
    swapMegaSchreens();
    _host.updateScreen();
    clearScreen();
    ++_clearCounter;
    _cycleCounter = calcNextFrame() - 1;
}

void Chip8EmulatorFP::op00ED_c8e(uint16_t opcode)
{
    halt();
}

void Chip8EmulatorFP::op00EE(uint16_t opcode)
{
    if(!_rSP)
        errorHalt("STACK UNDERFLOW");
    else {
        _rPC = _stack[--_rSP];
        if (_execMode == eSTEPOUT)
            _execMode = ePAUSED;
    }
}

void Chip8EmulatorFP::op00EE_cyclic(uint16_t opcode)
{
    _rPC = _stack[(--_rSP)&0xF];
    if (_execMode == eSTEPOUT)
        _execMode = ePAUSED;
}

void Chip8EmulatorFP::op00FB(uint16_t opcode)
{ // Scroll right 4 pixel
    if(_isMegaChipMode) {
        _screen.scrollRight(4);
        _screenRGBA->scrollRight(4);
        _host.updateScreen();
    }
    else {
        _screen.scrollRight(_isHires || _options.optHalfPixelScroll ? 4 : 8);
        _screenNeedsUpdate = true;
    }
}

void Chip8EmulatorFP::op00FB_masked(uint16_t opcode)
{ // Scroll right 4 pixel masked
    auto n = 4;
    if(!_isHires) n <<= 1;
    auto width = Chip8EmulatorBase::getCurrentScreenWidth();
    auto height = Chip8EmulatorBase::getCurrentScreenHeight();
    for(int sy = 0; sy < height; ++sy) {
        for(int sx = width - n - 1; sx >= 0; --sx) {
            _screen.movePixelMasked(sx, sy, sx + n, sy, _planes);
        }
        for(int sx = 0; sx < n; ++sx) {
            _screen.clearPixelMasked(sx, sy, _planes);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::op00FC(uint16_t opcode)
{ // Scroll left 4 pixel
    if(_isMegaChipMode) {
        _screen.scrollLeft(4);
        _screenRGBA->scrollLeft(4);
        _host.updateScreen();
    }
    else {
        _screen.scrollLeft(_isHires || _options.optHalfPixelScroll ? 4 : 8);
       _screenNeedsUpdate = true;
    }
}

void Chip8EmulatorFP::op00FC_masked(uint16_t opcode)
{ // Scroll left 4 pixels masked
    auto n = 4;
    if(!_isHires) n <<= 1;
    auto width = Chip8EmulatorBase::getCurrentScreenWidth();
    auto height = Chip8EmulatorBase::getCurrentScreenHeight();
    for(int sy = 0; sy < height; ++sy) {
        for(int sx = n; sx < width; ++sx) {
            _screen.movePixelMasked(sx, sy, sx - n, sy, _planes);
        }
        for(int sx = width - n; sx < width; ++sx) {
            _screen.clearPixelMasked(sx, sy, _planes);
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
    _host.preClear();
    _isHires = false;
    _isInstantDxyn = _options.optInstantDxyn;
}

void Chip8EmulatorFP::op00FE_withClear(uint16_t opcode)
{
    _host.preClear();
    _isHires = false;
    _isInstantDxyn = _options.optInstantDxyn;
    _screen.setAll(0);
    _screenNeedsUpdate = true;
    ++_clearCounter;
}

void Chip8EmulatorFP::op00FE_megachip(uint16_t opcode)
{
    if(_isHires && !_isMegaChipMode) {
        _host.preClear();
        _isHires = false;
        _isInstantDxyn = _options.optInstantDxyn;
        clearScreen();
        _screenNeedsUpdate = true;
        ++_clearCounter;
    }
}

void Chip8EmulatorFP::op00FF(uint16_t opcode)
{
    _host.preClear();
    _isHires = true;
    _isInstantDxyn = true;
}

void Chip8EmulatorFP::op00FF_withClear(uint16_t opcode)
{
    _host.preClear();
    _isHires = true;
    _isInstantDxyn = true;
    _screen.setAll(0);
    _screenNeedsUpdate = true;
    ++_clearCounter;
}

void Chip8EmulatorFP::op00FF_megachip(uint16_t opcode)
{
    if(!_isHires && !_isMegaChipMode) {
        _host.preClear();
        _isHires = true;
        _isInstantDxyn = true;
        clearScreen();
        _screenNeedsUpdate = true;
        ++_clearCounter;
    }
}

void Chip8EmulatorFP::op0151_c8e(uint16_t opcode)
{
    if(_rDT) {
        _rPC -= 2;
        _cpuState = eWAITING;
    }
    else {
        _cpuState = eNORMAL;
    }
}

void Chip8EmulatorFP::op0188_c8e(uint16_t opcode)
{
    _rPC = (_rPC + 2) & ADDRESS_MASK;
}

void Chip8EmulatorFP::op01nn(uint16_t opcode)
{
    _rI = ((opcode & 0xFF) << 16 | (_memory[_rPC & ADDRESS_MASK] << 8) | _memory[(_rPC + 1) & ADDRESS_MASK]) & ADDRESS_MASK;
    _rPC = (_rPC + 2) & ADDRESS_MASK;
}

void Chip8EmulatorFP::op02A0_c8x(uint16_t opcode)
{
    _chip8xBackgroundColor = (_chip8xBackgroundColor + 1) & 3;
    _screenNeedsUpdate = true;
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
    _screenAlpha = opcode & 0xFF;
}

void Chip8EmulatorFP::op060n(uint16_t opcode)
{
    uint32_t frequency = (_memory[_rI & ADDRESS_MASK] << 8) | _memory[(_rI + 1) & ADDRESS_MASK];
    uint32_t length = (_memory[(_rI + 2) & ADDRESS_MASK] << 16) | (_memory[(_rI + 3) & ADDRESS_MASK] << 8) | _memory[(_rI + 4) & ADDRESS_MASK];
    _sampleStart.store(_rI + 6);
    _sampleStep.store(frequency / 44100.0f);
    _sampleLength.store(length);
    _sampleLoop = (opcode & 0xf) == 0;
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

#ifdef ALIEN_INV8SION_BENCH
std::chrono::time_point<std::chrono::steady_clock> loopStart{};
int64_t loopCycles = 0;
int64_t maxCycles = 0;
int64_t minLoop = 1000000000;
int64_t maxLoop = 0;
double avgLoop = 0;
#endif

void Chip8EmulatorFP::op1nnn(uint16_t opcode)
{
    if((opcode & 0xFFF) == _rPC - 2)
        _execMode = ePAUSED;
    _rPC = opcode & 0xFFF;
#ifdef ALIEN_INV8SION_BENCH
    if(_rPC == 0x212) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - loopStart).count();
        avgLoop = (avgLoop * 63 + duration) / 64.0;
        if(minLoop > duration) minLoop = duration;
        if(maxLoop < duration) maxLoop = duration;
        auto lc = _cycleCounter - loopCycles;
        if(lc > maxCycles)
            maxCycles = lc;
        std::cout << "Loop: " <<  duration << "us (min: " << minLoop << "us, avg: " << avgLoop << "us, max:" << maxLoop << "us), " << (_cycleCounter - loopCycles) << " cycles, max: " << maxCycles << ", ips: " << (lc * 1000000 / duration) << std::endl;
    }
#endif
}

void Chip8EmulatorFP::op2nnn(uint16_t opcode)
{
    if(_rSP == 16)
        errorHalt("STACK OVERFLOW");
    else {
        _stack[_rSP++] = _rPC;
        _rPC = opcode & 0xFFF;
    }
}

void Chip8EmulatorFP::op2nnn_cyclic(uint16_t opcode)
{
    _stack[(_rSP++)&0xF] = _rPC;
    _rPC = opcode & 0xFFF;
}

void Chip8EmulatorFP::op3xnn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == (opcode & 0xff)) {
        _rPC += 2;
    }
}

#define CONDITIONAL_SKIP_DISTANCE(ifOpcode,mask) ((_memory[_rPC]&(mask>>8)) == (ifOpcode>>8) && (_memory[(_rPC + 1)]&(mask&0xff)) == (ifOpcode&0xff) ? 4 : 2)

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

void Chip8EmulatorFP::op5xy1_c8e(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] > _rV[(opcode >> 4) & 0xF]) {
        _rPC = (_rPC + 2) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::op5xy1_c8x(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = ((_rV[(opcode >> 8) & 0xF] & 0x77) + (_rV[(opcode >> 4) & 0xF] & 0x77)) & 0x77;
}

void Chip8EmulatorFP::op5xy2(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    auto l = std::abs(x-y);
    for(int i=0; i <= l; ++i)
        write(_rI + i, _rV[x < y ? x + i : x - i]);
}

void Chip8EmulatorFP::op5xy2_c8e(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    if(x < y) {
        auto l = y - x;
        for(int i=0; i <= l; ++i)
            write(_rI + i, _rV[x + i]);
        _rI = (_rI + l + 1) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::op5xy3(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    for(int i=0; i <= std::abs(x-y); ++i)
        _rV[x < y ? x + i : x - i] = read(_rI + i);
}

void Chip8EmulatorFP::op5xy3_c8e(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    if(x < y) {
        auto l = y - x;
        for(int i=0; i <= l; ++i)
            _rV[x + i] = read(_rI + i);
        _rI = (_rI + l + 1) & ADDRESS_MASK;
    }
}

void Chip8EmulatorFP::op5xy4(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    for(int i=0; i <= std::abs(x-y); ++i)
        _xxoPalette[x < y ? x + i : x - i] = _memory[_rI + i];
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

void Chip8EmulatorFP::opBBnn_c8e(uint16_t opcode)
{
    _rPC = (_rPC - 2 - (opcode & 0xff)) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opBFnn_c8e(uint16_t opcode)
{
    _rPC = (_rPC - 2 + (opcode & 0xff)) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opBxy0_c8x(uint16_t opcode)
{
    auto rx = _rV[(opcode >> 8) & 0xF];
    auto ry = _rV[((opcode >> 8) & 0xF) + 1];
    auto xPos = rx & 0xF;
    auto width = rx >> 4;
    auto yPos = ry & 0xF;
    auto height = ry >> 4;
    auto col = _rV[(opcode >> 4) & 0xF] & 7;
    _screen.setOverlayCellHeight(4);
    for(int y = 0; y <= height; ++y) {
        for(int x = 0; x <= width; ++x) {
            _screen.setOverlayCell(xPos + x, yPos + y, col);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::opBxyn_c8x(uint16_t opcode)
{
    auto rx = _rV[(opcode >> 8) & 0xF];
    auto ry = _rV[((opcode >> 8) & 0xF) + 1];
    auto xPos = (rx >> 3) & 7;
    auto yPos = ry & 0x1F;
    auto height = opcode & 0xF;
    auto col = _rV[(opcode >> 4) & 0xF] & 7;
    _screen.setOverlayCellHeight(1);
    for(int y = 0; y < height; ++y) {
        _screen.setOverlayCell(xPos, yPos + y, col);
    }
    _screenNeedsUpdate = true;
}

void Chip8EmulatorFP::opBnnn(uint16_t opcode)
{
    _rPC = (_rV[0] + (opcode & 0xFFF)) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opBxnn(uint16_t opcode)
{
    _rPC = (_rV[(opcode >> 8) & 0xF] + (opcode & 0xFFF)) & ADDRESS_MASK;
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
        if(_rI < 0x100) {
            int lines = opcode & 0xf;
            auto byteOffset = _rI;
            for (int l = 0; l < lines && ypos + l < 192; ++l) {
                auto value = _memory[byteOffset++];
                for (unsigned b = 0; b < 8 && xpos + b < 256 && value; ++b, value <<= 1) {
                    if (value & 0x80) {
                        uint8_t* pixelBuffer = &_screen.getPixelRef(xpos + b, ypos + l);
                        uint32_t* pixelBuffer32 = &_workRGBA->getPixelRef(xpos + b, ypos + l);
                        if (*pixelBuffer) {
                            _rV[0xf] = 1;
                            *pixelBuffer = 0;
                            *pixelBuffer32 = 0;
                        }
                        else {
                            *pixelBuffer = 254;
                            *pixelBuffer32 = 0xffffffff;
                        }
                    }
                }
            }
        }
        else {
            for (int y = 0; y < _spriteHeight; ++y) {
                int yy = ypos + y;
                if(_options.optWrapSprites) {
                    yy = (uint8_t)yy;
                    if(yy >= 192)
                        continue;
                }
                else {
                    if(yy >= 192)
                        break;
                }
                uint8_t* pixelBuffer = &_screen.getPixelRef(xpos, yy);
                uint32_t* pixelBuffer32 = &_workRGBA->getPixelRef(xpos, yy);
                for (int x = 0; x < _spriteWidth; ++x, ++pixelBuffer, ++pixelBuffer32) {
                    int xx = xpos + x;
                    if(xx > 255) {
                        if(_options.optWrapSprites) {
                            xx &= 0xff;
                            pixelBuffer = &_screen.getPixelRef(xx, yy);
                            pixelBuffer32 = &_workRGBA->getPixelRef(xx, yy);
                        }
                        else {
                            continue;
                        }
                    }
                    auto col = _memory[_rI + y * _spriteWidth + x];
                    if (col) {
                        if (*pixelBuffer == _collisionColor)
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

void Chip8EmulatorFP::opExF2_c8x(uint16_t opcode)
{
    // still nop
}

void Chip8EmulatorFP::opExF5_c8x(uint16_t opcode)
{
    _rPC += 2;
}

void Chip8EmulatorFP::opF000(uint16_t opcode)
{
    _rI = ((_memory[_rPC & ADDRESS_MASK] << 8) | _memory[(_rPC + 1) & ADDRESS_MASK]) & ADDRESS_MASK;
    _rPC = (_rPC + 2) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx01(uint16_t opcode)
{
    _planes = (opcode >> 8) & 0xF;
}

void Chip8EmulatorFP::opF002(uint16_t opcode)
{
    uint8_t anyBit = 0;
#ifndef EMU_AUDIO_DEBUG
    for(int i = 0; i < 16; ++i) {
        _xoAudioPattern[i] = _memory[(_rI + i) & ADDRESS_MASK];
        anyBit |= _xoAudioPattern[i];
    }
#else
    std::clog << "pattern: ";
    for(int i = 0; i < 16; ++i) {
        _xoAudioPattern[i] = _memory[(_rI + i) & ADDRESS_MASK];
        anyBit |= _xoAudioPattern[i];
        std::clog << (i ? ", " : "") << fmt::format("0x{:02x}", _xoAudioPattern[i]);
    }
    std::clog << std::endl;
#endif
    _xoSilencePattern = anyBit != 0;
}

void Chip8EmulatorFP::opFx07(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = _rDT;
#ifdef ALIEN_INV8SION_BENCH
    if(!_rDT) {
        loopCycles = _cycleCounter;
        loopStart = std::chrono::steady_clock::now();
    }
#endif
}

void Chip8EmulatorFP::opFx0A(uint16_t opcode)
{
    auto key = _host.getKeyPressed();
    if (key > 0) {
        _rV[(opcode >> 8) & 0xF] = key - 1;
        _cpuState = eNORMAL;
    }
    else {
        // keep waiting...
        _rPC -= 2;
        if(key < 0)
            _rST = 4;
        //--_cycleCounter;
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
#ifdef EMU_AUDIO_DEBUG
    std::clog << fmt::format("st := {}", (int)_rST) << std::endl;
#endif
}

void Chip8EmulatorFP::opFx1B_c8e(uint16_t opcode)
{
    _rPC = (_rPC + _rV[(opcode >> 8) & 0xF]) & ADDRESS_MASK;
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
    write(_rI, val / 100);
    write(_rI + 1, (val / 10) % 10);
    write(_rI + 2, val % 10);
}

void Chip8EmulatorFP::opFx3A(uint16_t opcode)
{
    _xoPitch.store(_rV[(opcode >> 8) & 0xF]);
#ifdef EMU_AUDIO_DEBUG
    std::clog << "pitch: " << (int)_xoPitch.load() << std::endl;
#endif
}

void Chip8EmulatorFP::opFx4F_c8e(uint16_t opcode)
{
    if(_cpuState != eWAITING) {
        _rDT = _rV[(opcode >> 8) & 0xF];
        _cpuState = eWAITING;
    }
    if(_rDT && (_cpuState == eWAITING)) {
        _rPC -= 2;
    }
    else {
        _cpuState = eNORMAL;
    }
}

void Chip8EmulatorFP::opFx55(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        write(_rI + i, _rV[i]);
    }
    _rI = (_rI + upto + 1) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx55_loadStoreIncIByX(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        write(_rI + i, _rV[i]);
    }
    _rI = (_rI + upto) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx55_loadStoreDontIncI(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        write(_rI + i, _rV[i]);
    }
}

void Chip8EmulatorFP::opFx65(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = read(_rI + i);
    }
    _rI = (_rI + upto + 1) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx65_loadStoreIncIByX(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = read(_rI + i);
    }
    _rI = (_rI + upto) & ADDRESS_MASK;
}

void Chip8EmulatorFP::opFx65_loadStoreDontIncI(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = read(_rI + i);
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

void Chip8EmulatorFP::opFxF8_c8x(uint16_t opcode)
{
    uint8_t val = _rV[(opcode >> 8) & 0xF];
    _vp595Frequency = val ? val : 0x80;
}

void Chip8EmulatorFP::opFxFB_c8x(uint16_t opcode)
{
    // still nop
}

static uint16_t g_hp48Wave[] = {
    0x99,   0x4cd,  0x2df,  0xfbc3, 0xf1e3, 0xe747, 0xddef, 0xd866, 0xda5c, 0xdef1, 0xe38e, 0xe664, 0xe9eb, 0xefd3, 0xf1fe, 0xf03a, 0xef66, 0xf1aa, 0xf7d1, 0x13a,  0xadd,  0x102d, 0xe8d,  0xb72,  0xa58,  0xe80,  0x17af, 0x21d1, 0x2718, 0x2245, 0x15f3,
    0x5a0,  0xfc82, 0xfef5, 0x6f7,  0xd5f,  0xac7,  0xfe89, 0xef7c, 0xe961, 0xef4e, 0xfba7, 0x440,  0x452,  0xfc8a, 0xf099, 0xe958, 0xeceb, 0xf959, 0x6f3,  0xcfd,  0x92f,  0x3c8,  0x2cd,  0x733,  0xd94,  0x12f0, 0x1531, 0x1147, 0x73d,  0xfbaf, 0xf3fb,
    0xf2e5, 0xf8d1, 0x2e,   0x3fb,  0x25c,  0xfc35, 0xf222, 0xe88f, 0xe260, 0xdf64, 0xe0f0, 0xe306, 0xe5e6, 0xe965, 0xed55, 0xf203, 0xf662, 0xfb37, 0x12c,  0x926,  0xf66,  0x10ac, 0xdd5,  0xa2b,  0xb84,  0x13b6, 0x1fe4, 0x2bef, 0x3168, 0x2dfc, 0x2380,
    0x1859, 0x1368, 0x14d1, 0x18ab, 0x190d, 0x141f, 0xa63,  0xfd36, 0xee1f, 0xe39e, 0xe201, 0xe4dc, 0xe7dd, 0xe748, 0xe452, 0xde58, 0xd77d, 0xd3e4, 0xd695, 0xde34, 0xe593, 0xec3e, 0xf229, 0xf714, 0xf841, 0xf93b, 0xfcdd, 0x671,  0x1661, 0x24fb, 0x2c00,
    0x27ce, 0x1dcb, 0x11bb, 0xb89,  0xfc6,  0x1991, 0x219c, 0x1fa7, 0x132d, 0x278,  0xf9df, 0xfd50, 0x566,  0x8c5,  0x33f,  0xf846, 0xeb34, 0xe28b, 0xe365, 0xeda5, 0xfb18, 0x1b3,  0xfe67, 0xf754, 0xf34f, 0xf63e, 0xff4c, 0x997,  0xea5,  0xb0c,  0x247,
    0xf98f, 0xf5af, 0xf914, 0x2e8,  0xd0b,  0x10ab, 0xbab,  0x145,  0xf7db, 0xf1ab, 0xedf7, 0xec64, 0xebb5, 0xea7b, 0xea61, 0xeb9b, 0xebad, 0xea86, 0xec28, 0xf2c9, 0xfc97, 0x688,  0xb10,  0x80e,  0xfff8, 0xfa73, 0xfd43, 0xa97,  0x20a1, 0x3393, 0x3a6d,
    0x3376, 0x256e, 0x1b72, 0x1a9f, 0x200a, 0x2470, 0x23bc, 0x1c60, 0x1091, 0x45,   0xee38, 0xe370, 0xe2d0, 0xe694, 0xe851, 0xe591, 0xdf8c, 0xd829, 0xd063, 0xcc6c, 0xcf8e, 0xd7ed, 0xdf45, 0xe306, 0xe752, 0xed90, 0xf362, 0xf85d, 0xfed5, 0x8df,  0x17dd,
    0x2691, 0x2daa, 0x2a67, 0x2132, 0x1755, 0x1288, 0x1816, 0x220b, 0x2981, 0x262f, 0x17f0, 0x6d2,  0xfc48, 0xfecb, 0x722,  0xc3d,  0x6e6,  0xf975, 0xe96f, 0xdd92, 0xdd6b, 0xe701, 0xf560, 0xfd48, 0xfa18, 0xf1db, 0xec67, 0xeea1, 0xf8c0, 0x5df,  0xdb2,
    0xbcb,  0x2f4,  0xfa82, 0xf691, 0xf960, 0x24d,  0xceb,  0x12a4, 0x1085, 0x82f,  0xfdc7, 0xf5dc, 0xf073, 0xed9d, 0xebec, 0xea65, 0xea44, 0xec13, 0xed4b, 0xeb5e, 0xeaa6, 0xeef3, 0xf8dd, 0x488,  0xc0c,  0xb48,  0x3b5,  0xfc88, 0xfd06, 0x881,  0x1dfb,
    0x32fb, 0x3c79, 0x37b2, 0x2964, 0x1d15, 0x19bd, 0x1e2d, 0x22d7, 0x22a8, 0x1c7a, 0x113a, 0x1aa,  0xef17, 0xe247, 0xdf2c, 0xe10d, 0xe1af, 0xdf86, 0xdb90, 0xd5bc, 0xcf35, 0xcb60, 0xcdd2, 0xd420, 0xdbff, 0xe438, 0xed32, 0xf5f9, 0xfb2e, 0xfcdb, 0xff15,
    0x77d,  0x183c, 0x2b67, 0x3764, 0x366f, 0x298d, 0x19d5, 0xfc3,  0x1274, 0x1e3b, 0x2745, 0x2505, 0x1596, 0x3d0,  0xfa58, 0xfc12, 0x1aa,  0x321,  0xfe2b, 0xf496, 0xe971, 0xe181, 0xe1c4, 0xe94d, 0xf25e, 0xf450, 0xf102, 0xeea0, 0xf1b1, 0xf932, 0x189,
    0x947,  0xcb3,  0xa84,  0x358,  0xfcac, 0xfa52, 0xff5b, 0x81f,  0xe37,  0xf9b,  0xbf3,  0x549,  0xfd0a, 0xf663, 0xf073, 0xecb1, 0xe9fc, 0xe70a, 0xe615, 0xe874, 0xec79, 0xecc6, 0xec80, 0xef6d, 0xf711, 0x108,  0x8e9,  0xb25,  0x6a4,  0x1a8,  0x2bf,
    0xd5b,  0x20d1, 0x33c0, 0x3b9c, 0x36bc, 0x293d, 0x1e71, 0x1c18, 0x2000, 0x245c, 0x22dd, 0x1b4f, 0xe5c,  0xff5d, 0xee97, 0xe1d2, 0xdd18, 0xdcb1, 0xdd6f, 0xdc59, 0xda44, 0xd6ad, 0xd1de, 0xce00, 0xcf2d, 0xd481, 0xdbc7, 0xe3d1, 0xec8a, 0xf597, 0xfb18,
    0xfdaa, 0x2b,   0x7bc,  0x173c, 0x29ba, 0x35c2, 0x3574, 0x2a46, 0x1bd4, 0x11ee, 0x1326, 0x1e20, 0x2725, 0x2582, 0x1618, 0x2f3,  0xf88a, 0xfa7b, 0x18e,  0x36b,  0xfde8, 0xf3a2, 0xe8ad, 0xe077, 0xe02d, 0xe784, 0xf15b, 0xf4a5, 0xf147, 0xee3a, 0xf029,
    0xf7cf, 0x8f,   0x90b,  0xdce,  0xd5e,  0x739,  0xff63, 0xfb1a, 0xfdc8, 0x66c,  0xd8e,  0x1090, 0xe3e,  0x834,  0xff66, 0xf71d, 0xf009, 0xeb4d, 0xe950, 0xe6f7, 0xe60f, 0xe79b, 0xebe7, 0xecd2, 0xebe0, 0xee31, 0xf4ed, 0xff03, 0x747,  0xaa4,  0x743,
    0x28c,  0x301,  0xc51,  0x1ecd, 0x3286, 0x3c24, 0x38de, 0x2bdf, 0x1ff0, 0x1c87, 0x1f36, 0x23e7, 0x2371, 0x1d31, 0x1172, 0x268,  0xf09b, 0xe118, 0xdacd, 0xda75, 0xdc8e, 0xdccd, 0xdb5f, 0xd81e, 0xd297, 0xccc6, 0xcba7, 0xd022, 0xd7a6, 0xe132, 0xeb51,
    0xf532, 0xfb7c, 0xfe72, 0xaf,   0x67c,  0x144a, 0x26f7, 0x3551, 0x37ff, 0x2eaa, 0x1fcb, 0x13e2, 0x1243, 0x1c21, 0x2667, 0x276f, 0x1a44, 0x660,  0xf95a, 0xf943, 0x64,   0x31c,  0xfe5e, 0xf4b4, 0xea60, 0xe1b4, 0xdf56, 0xe45a, 0xed0c, 0xf19c, 0xefb1,
    0xed9f, 0xef71, 0xf730, 0x0a,   0x806,  0xc69,  0xc0c,  0x6af,  0xff72, 0xfb45, 0xfd51, 0x5e8,  0xdc1,  0x118e, 0xfb9,  0x9f1,  0x176,  0xf949, 0xf26f, 0xed26, 0xeaf5, 0xe82b, 0xe6fe, 0xe86b, 0xed04, 0xeec0, 0xeda5, 0xef61, 0xf512, 0xfe8a, 0x6d3,
    0xada,  0x81d,  0x36e,  0x3d4,  0xcca,  0x1e53, 0x30fb, 0x3a79, 0x381e, 0x2c2b, 0x1f66, 0x1bbc, 0x1f93, 0x23c7, 0x1f81, 0x1567, 0x881,  0xfa5b, 0xec75, 0xe003, 0xd911, 0xd540, 0xd3de, 0xd1cc, 0xcfaa, 0xd06d, 0xd255, 0xd551, 0xda96, 0xe16d, 0xe908,
    0xef9c, 0xf3f2, 0xf659, 0xf6db, 0xfc6e, 0x8c4,  0x1911, 0x2a0c, 0x3669, 0x386e, 0x2e5c, 0x1f11, 0x1075, 0xab4,  0x1117, 0x1e06, 0x264a, 0x21df, 0x1021, 0xfb78, 0xf08e, 0xf1ee, 0xfc98, 0x69b,  0xb1d,  0x359,  0xef05, 0xda37, 0xd05a, 0xd614, 0xe2f4,
    0xee1b, 0xf226, 0xf0d5, 0xeead, 0xee2d, 0xf1d0, 0xf8ec, 0x38a,  0xd39,  0x100b, 0xc8e,  0x7f9,  0x60c,  0x7a9,  0xc0b,  0x1125, 0x15bc, 0x1847, 0x162b, 0xfb9,  0x8b7,  0x421,  0x98,   0xfbca, 0xf691, 0xf1cd, 0xeda5, 0xeb83, 0xeba0, 0xed32, 0xef40,
    0xf0b5, 0xf25b, 0xf4e8, 0xf71e, 0xf9bf, 0xfdf7, 0x255,  0x6f6,  0xc7a,  0xfc6,  0xdfc,  0x8d1,  0x727,  0xbf5,  0x1648, 0x1ef6, 0x1e58, 0x1419, 0x58e,  0xfb3a, 0xf7a7, 0xfe29, 0x8f0,  0xe36,  0xbd2,  0x1ec,  0xf764, 0xf2c7, 0xf5d6, 0xfa03, 0xf84e,
    0xf2ce, 0xedbb, 0xe9ee, 0xe59c, 0xe3eb, 0xe7b5, 0xed9d, 0xf2c8, 0xf6af, 0xfac1
};

void Chip8EmulatorFP::renderAudio(int16_t* samples, size_t frames, int sampleFrequency)
{
#ifdef EMU_AUDIO_DEBUG
    std::clog << fmt::format("render {} (ST:{})", frames, _rST) << std::endl;
#endif
    if(_isMegaChipMode && _sampleLength) {
        while(frames--) {
            *samples++ = ((int16_t)getNextMCSample() - 128) * 256;
        }
    }
    else if(_rST) {
        if (_options.optXOChipSound) {
            auto step = 4000 * std::pow(2.0f, (float(_xoPitch) - 64) / 48.0f) / 128 / sampleFrequency;
            for (int i = 0; i < frames; ++i) {
                auto pos = int(std::clamp(_wavePhase * 128.0f, 0.0f, 127.0f));
                *samples++ = _xoAudioPattern[pos >> 3] & (1 << (7 - (pos & 7))) ? 16384 : -16384;
                _wavePhase = std::fmod(_wavePhase + step, 1.0f);
            }
        }
        else if(_options.behaviorBase >= Chip8EmulatorOptions::eCHIP48 && _options.behaviorBase <= Chip8EmulatorOptions::eSCHPC) {
            for (int i = 0; i < frames; ++i) {
                *samples++ = g_hp48Wave[(int)_wavePhase];
                _wavePhase = std::fmod(_wavePhase + 1, sizeof(g_hp48Wave) / 2);
            }
        }
        else {
            auto audioFrequency = _options.behaviorBase == Chip8EmulatorOptions::eCHIP8X ? 27535.0f / ((unsigned)_vp595Frequency + 1) : 1531.555f;
            const float step = audioFrequency / sampleFrequency;
            for (int i = 0; i < frames; ++i) {
                *samples++ = (_wavePhase > 0.5f) ? 16384 : -16384;
                _wavePhase = std::fmod(_wavePhase + step, 1.0f);
            }
        }
    }
    else {
        // Default is silence
        _wavePhase = 0;
        IChip8Emulator::renderAudio(samples, frames, sampleFrequency);
    }
}

}
