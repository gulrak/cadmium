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

std::unique_ptr<IChip8Emulator> Chip8EmulatorBase::create(Chip8EmulatorHost& host, Engine engine, Chip8EmulatorOptions& options, IChip8Emulator* iother)
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
    else if(engine == eCHIP8VIP) {
        return std::make_unique<Chip8VIP>(host, options, iother);
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

void Chip8EmulatorBase::setBreakpoint(uint32_t address, const BreakpointInfo& bpi)
{
    _breakpoints[address] = bpi;
    _breakMap[address & 0xFFF] = 1;
}

void Chip8EmulatorBase::removeBreakpoint(uint32_t address)
{
    _breakpoints.erase(address);
    size_t count = 0;
    uint32_t masked = address & 0xFFF;
    for(const auto& [addr, bpi] : _breakpoints) {
        if((addr & 0xFFF) == masked) {
            _breakMap[masked] = 1;
            return;
        }
    }
    _breakMap[masked] = 0;
}

IChip8Emulator::BreakpointInfo* Chip8EmulatorBase::findBreakpoint(uint32_t address)
{
    if(_breakMap[address & 0xFFF]) {
        auto iter = _breakpoints.find(address);
        if(iter != _breakpoints.end())
            return &iter->second;
    }
    return nullptr;
}

size_t Chip8EmulatorBase::numBreakpoints() const
{
    return _breakpoints.size();
}

std::pair<uint32_t, IChip8Emulator::BreakpointInfo*> Chip8EmulatorBase::getNthBreakpoint(size_t index)
{
    size_t count = 0;
    for(auto& [addr, bpi] : _breakpoints) {
        if(count++ == index)
            return {addr, &bpi};
    }
    return {0, nullptr};
}

void Chip8EmulatorBase::removeAllBreakpoints()
{
    std::memset(_breakMap.data(), 0, 4096);
    _breakpoints.clear();
}

}  // namespace emu
