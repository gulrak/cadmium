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
// ChipEmulatorFP - a method pointer table based CHIP-8 core
//---------------------------------------------------------------------------------------
class Chip8EmulatorFP : public Chip8EmulatorBase
{
public:
    using OpcodeHandler = void (Chip8EmulatorFP::*)(uint16_t);
    const uint32_t ADDRESS_MASK;
    const int SCREEN_WIDTH;
    const int SCREEN_HEIGHT;
    
    Chip8EmulatorFP(EmulatorHost& host, Chip8EmulatorOptions& options, IChip8Emulator* other = nullptr);
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
                int x = _rV[(opcode >> 8) & 0xF] % SCREEN_WIDTH;
                int y = _rV[(opcode >> 4) & 0xF] % SCREEN_HEIGHT;
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
                int x = _rV[(opcode >> 8) & 0xF] % (SCREEN_WIDTH / 2);
                int y = _rV[(opcode >> 4) & 0xF] % (SCREEN_HEIGHT / 2);
                int lines = opcode & 0xF;
                _rV[15] = drawSprite<quirks>(x*2, y*2, &_memory[_rI & ADDRESS_MASK], lines, false) ? 1 : 0;
            }
        }
        else {
            int x = _rV[(opcode >> 8) & 0xF] % SCREEN_WIDTH;
            int y = _rV[(opcode >> 4) & 0xF] % SCREEN_HEIGHT;
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
                int x = _rV[(opcode >> 8) & 0xF] % SCREEN_WIDTH;
                int y = _rV[(opcode >> 4) & 0xF] % SCREEN_HEIGHT;
                int lines = opcode & 0xF;
                _rV[15] = drawSprite<quirks>(x, y, &_memory[_rI & ADDRESS_MASK], lines, true) ? 1 : 0;
            }
            else
            {
                int x = _rV[(opcode >> 8) & 0xF] % (SCREEN_WIDTH / 2);
                int y = _rV[(opcode >> 4) & 0xF] % (SCREEN_HEIGHT / 2);
                int lines = opcode & 0xF;
                _rV[15] = drawSprite<quirks>(x*2, y*2, &_memory[_rI & ADDRESS_MASK], lines, false) ? 1 : 0;
            }
        }
        else {
            if(_options.instructionsPerFrame && _cycleCounter % _options.instructionsPerFrame != 0) {
                _rPC -= 2;
                return;
            }
            int x = _rV[(opcode >> 8) & 0xF] % SCREEN_WIDTH;
            int y = _rV[(opcode >> 4) & 0xF] % SCREEN_HEIGHT;
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
            if(_options.optPalVideo)
                _rV[15] = drawSprite<quirks,128,96>(x, y, &_memory[_rI & ADDRESS_MASK], lines, false) ? 1 : 0;
            else
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
    uint8_t drawSprite(uint8_t x, uint8_t y, const uint8_t* data, uint8_t height, bool hires)
    {
        int collision = 0;
        constexpr int scrWidth = quirks&HiresSupport ? MAX_WIDTH : MAX_WIDTH/2;
        constexpr int scrHeight = quirks&HiresSupport ? MAX_HEIGHT : MAX_HEIGHT/2;
        int scale = quirks&HiresSupport ? (hires ? 1 : 2) : 1;
        int width = 8;
        //x %= scrWidth;
        //y %= scrHeight;
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
                        if constexpr (quirks&SChip11Collisions) {
                            ++collision;
                        }
                        if(width == 16)
                            ++data;
                    }
                }
            }
        }
        if constexpr (quirks&SChip11Collisions)
            return hires ? collision : static_cast<bool>(collision);
        else
            return static_cast<bool>(collision);
    }

    void renderAudio(int16_t* samples, size_t frames, int sampleFrequency) override;

private:
    uint8_t read(const uint32_t addr) const
    {
        if(addr <= ADDRESS_MASK)
            return _memory[addr];
        return 255;
    }
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
