//---------------------------------------------------------------------------------------
// src/emulation/chip8emulatorbase.hpp
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
#include <emulation/chip8emulatorbase.hpp>

namespace emu {

std::unique_ptr<IChip8Emulator> Chip8EmulatorBase::create(Chip8EmulatorHost& host, Engine engine, Chip8EmulatorOptions& options, const IChip8Emulator* iother)
{
    const auto* other = dynamic_cast<const Chip8EmulatorBase*>(iother);
    if(engine == eCHIP8TS) {
        if (options.optAllowHires) {
            if (options.optHas16BitAddr) {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, HiresSupport | MultiColor | WrapSprite>>(host, options, other);
                    else
                        return std::make_unique<Chip8Emulator<16, HiresSupport | MultiColor>>(host, options, other);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, HiresSupport | WrapSprite>>(host, options, other);
                    else
                        return std::make_unique<Chip8Emulator<16, HiresSupport>>(host, options, other);
                }
            }
            else {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, HiresSupport | MultiColor | WrapSprite>>(host, options, other);
                    else
                        return std::make_unique<Chip8Emulator<12, HiresSupport | MultiColor>>(host, options, other);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, HiresSupport | WrapSprite>>(host, options, other);
                    else
                        return std::make_unique<Chip8Emulator<12, HiresSupport>>(host, options, other);
                }
            }
        }
        else {
            if (options.optHas16BitAddr) {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, MultiColor | WrapSprite>>(host, options, other);
                    else
                        return std::make_unique<Chip8Emulator<16, MultiColor>>(host, options, other);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, WrapSprite>>(host, options, other);
                    else
                        return std::make_unique<Chip8Emulator<16, 0>>(host, options, other);
                }
            }
            else {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, MultiColor | WrapSprite>>(host, options, other);
                    else
                        return std::make_unique<Chip8Emulator<12, MultiColor>>(host, options, other);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, WrapSprite>>(host, options, other);
                    else
                        return std::make_unique<Chip8Emulator<12, 0>>(host, options, other);
                }
            }
        }
    }
    else if(engine == eCHIP8MPT) {
        return std::make_unique<Chip8EmulatorFP>(host, options, other);
    }
    return std::make_unique<Chip8EmulatorVIP>(host, options, other);
}

void Chip8EmulatorBase::reset()
{
    //static const uint8_t defaultPalette[16] = {37, 255, 114, 41, 205, 153, 42, 213, 169, 85, 37, 114, 87, 159, 69, 9};
    static const uint8_t defaultPalette[16] = {0, 255, 182, 109, 224, 28, 3, 252, 160, 20, 2, 204, 227, 31, 162, 22};
    //static const uint8_t defaultPalette[16] = {172, 248, 236, 100, 205, 153, 42, 213, 169, 85, 37, 114, 87, 159, 69, 9};
    _cycleCounter = 0;
    _frameCounter = 0;
    _clearCounter = 0;
    _rI = 0;
    _rPC = _options.startAddress;
    std::memset(_stack.data(), 0, 16 * 2);
    _rSP = 0;
    _rDT = 0;
    _rST = 0;
    std::memset(_rV.data(), 0, 16);
    std::memset(_memory.data(), 0, _memory.size());
    std::memcpy(_memory.data(), _chip8font, 16 * 5 + 10*10);
    std::memcpy(_xxoPalette.data(), defaultPalette, 16);
    std::memset(_xoAudioPattern.data(), 0, 16);
    _xoPitch = 64;
    clearScreen();
    _host.updatePalette(_xxoPalette);
    _execMode = _host.isHeadless() ? eRUNNING : ePAUSED;
    _cpuState = eNORMAL;
    _isHires = _options.optOnlyHires ? true : false;
    _isMegaChipMode = false;
    _planes = 1;
    _spriteWidth = 0;
    _spriteHeight = 0;
    _collisionColor = 1;
    copyState();
}

void Chip8EmulatorBase::copyState()
{
    std::memcpy(_rV_b.data(), _rV.data(), sizeof(uint8_t)*16);
    std::memcpy(_stack_b.data(), _stack.data(), sizeof(uint16_t)*16);
    std::memcpy(_memory_b.data(), _memory.data(), sizeof(uint8_t)*_memory_b.size());
    _rSP_b = _rSP;
    _rDT_b = _rDT;
    _rST_b = _rST;
    _rI_b = _rI;
}

void Chip8EmulatorBase::tick(int instructionsPerFrame)
{
    handleTimer();
    if(!instructionsPerFrame) {
        auto start = std::chrono::steady_clock::now();
        do {
            executeInstructions(487);
        }
        while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < 14);
    }
    else {
        executeInstructions(instructionsPerFrame);
    }
}

std::pair<uint16_t, std::string> Chip8EmulatorBase::disassembleInstruction(const uint8_t* code, const uint8_t* end)
{
    auto opcode = (*code << 8) | *(code + 1);
    auto next = code + 3 < end ? (*(code + 2) << 8) | *(code + 3) : 0;

    switch (opcode >> 12) {
        case 0:
            if (opcode == 0x0010 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, "megaoff"};
            if (opcode == 0x0011 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, "megaon"};
            if ((opcode & 0xFFF0) == 0x00B0 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("scroll-up-alt {}", opcode & 0xF)};
            if ((opcode & 0xFFF0) == 0x00C0) return {2, fmt::format("scroll-down {}", opcode & 0xF)};
            if ((opcode & 0xFFF0) == 0x00D0 && _options.behaviorBase != emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("scroll-up {}", opcode & 0xF)};
            if (opcode == 0x00E0) return {2, "clear"};
            if (opcode == 0x00EE) return {2, "return"};
            if (opcode == 0x00FB) return {2, "scroll-right"};
            if (opcode == 0x00FC) return {2, "scroll-left"};
            if (opcode == 0x00FD) return {2, "exit"};
            if (opcode == 0x00FE) return {2, "lores"};
            if (opcode == 0x00FF) return {2, "hires"};
            if ((opcode & 0xFF00) == 0x0100 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {4, fmt::format("ldhi {}", _labelOrAddress(((opcode&0xFF)<<16)|next))};
            if ((opcode & 0xFF00) == 0x0200 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("ldpal {}", opcode & 0xFF)};
            if ((opcode & 0xFF00) == 0x0300 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("sprw {}", opcode & 0xFF)};
            if ((opcode & 0xFF00) == 0x0400 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("sprh {}", opcode & 0xFF)};
            if ((opcode & 0xFF00) == 0x0500 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("alpha {}", opcode & 0xFF)};
            if ((opcode & 0xFFF0) == 0x0600 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("digisnd {}", opcode & 0xF)};
            if (opcode == 0x0700 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, "stopsnd"};
            if ((opcode & 0xFFF0) == 0x0800 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("bmode {}", opcode & 0xF)};
            return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
        case 1: return {2, fmt::format("jump {}", _labelOrAddress(opcode & 0xFFF))};
        case 2: return {2, fmt::format(":call {}", _labelOrAddress(opcode & 0xFFF))};
        case 3: return {2, fmt::format("if v{:X} != 0x{:02X} then", (opcode >> 8) & 0xF, opcode & 0xFF)};
        case 4: return {2, fmt::format("if v{:X} == 0x{:02X} then", (opcode >> 8) & 0xF, opcode & 0xFF)};
        case 5:
            if ((opcode & 0xF) == 0)
                return {2, fmt::format("if v{:X} != v{:X} then", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
            else if((opcode & 0xF) == 2)
                return {2, fmt::format("save v{:X} - v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
            else if((opcode & 0xF) == 3)
                return {2, fmt::format("load v{:X} - v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
            else
                return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
        case 6: return {2, fmt::format("v{:X} := 0x{:02X}", (opcode >> 8) & 0xF, opcode & 0xff)};
        case 7: return {2, fmt::format("v{:X} += 0x{:02X}", (opcode >> 8) & 0xF, opcode & 0xff)};
        case 8:
            switch (opcode & 0xF) {
                case 0: return {2, fmt::format("v{:X} := v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 1: return {2, fmt::format("v{:X} |= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 2: return {2, fmt::format("v{:X} &= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 3: return {2, fmt::format("v{:X} ^= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 4: return {2, fmt::format("v{:X} += v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 5: return {2, fmt::format("v{:X} -= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 6: return {2, fmt::format("v{:X} >>= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 7: return {2, fmt::format("v{:X} =- v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 0xE: return {2, fmt::format("v{:X} <<= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                default: return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
            }
            break;
        case 9:
            if((opcode & 0xF) == 0)
                return {2, fmt::format("if v{:X} == v{:X} then", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
            else
                return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
        case 0xA: return {2, fmt::format("i := {}", _labelOrAddress(opcode & 0xFFF))};
        case 0xB: return {2, fmt::format("jump0 {}", _labelOrAddress(opcode & 0xFFF))};
        case 0xC: return {2, fmt::format("v{:X} := random 0x{:02X}", (opcode >> 8) & 0xF, opcode & 0xff)};
        case 0xD: return {2, fmt::format("sprite v{:X} v{:X} 0x{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF, opcode & 0xF)};
        case 0xE:
            switch (opcode & 0xFF) {
                case 0x9E: return {2, fmt::format("if v{:X} -key then", (opcode >> 8) & 0xF)};
                case 0xA1: return {2, fmt::format("if v{:X} key then", (opcode >> 8) & 0xF)};
                default: return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
            }
            break;
        case 0xF:
            switch (opcode & 0xFF) {
                case 0x00: return {4, fmt::format("i := long {}", _labelOrAddress(next))};
                case 0x01: return {2, fmt::format("plane {}", (opcode >> 8) & 0xF)};
                case 0x02:
                    if(opcode == 0xF002)
                        return {2, "audio"};
                    else
                        return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
                case 0x07: return {2, fmt::format("v{:X} := delay", (opcode >> 8) & 0xF)};
                case 0x0A: return {2, fmt::format("v{:X} := key", (opcode >> 8) & 0xF)};
                case 0x15: return {2, fmt::format("delay := v{:X}", (opcode >> 8) & 0xF)};
                case 0x18: return {2, fmt::format("sound := v{:X}", (opcode >> 8) & 0xF)};
                case 0x1E: return {2, fmt::format("i += v{:X}", (opcode >> 8) & 0xF)};
                case 0x29: return {2, fmt::format("i := hex v{:X}", (opcode >> 8) & 0xF)};
                case 0x30: return {2, fmt::format("i := bighex v{:X}", (opcode >> 8) & 0xF)};
                case 0x33: return {2, fmt::format("bcd v{:X}", (opcode >> 8) & 0xF)};
                case 0x3A: return {2, fmt::format("pitch := v{:X}", (opcode >> 8) & 0xF)};
                case 0x55: return {2, fmt::format("save v{:X}", (opcode >> 8) & 0xF)};
                case 0x65: return {2, fmt::format("load v{:X}", (opcode >> 8) & 0xF)};
                case 0x75: return {2, fmt::format("saveflags v{:X}", (opcode >> 8) & 0xF)};
                case 0x85: return {2, fmt::format("loadflags v{:X}", (opcode >> 8) & 0xF)};
                default:
                    return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
            }
            break;
        default:
            return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
    }
}

}  // namespace emu
