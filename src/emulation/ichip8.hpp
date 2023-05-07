//---------------------------------------------------------------------------------------
// src/emulation/ichip8.hpp
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
#pragma once

#include <emulation/config.hpp>
#include <emulation/hardware/genericcpu.hpp>
#include <emulation/videoscreen.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace emu
{

struct Chip8State
{
    cycles_t cycles{};
    int frameCycle{};
    std::array<uint8_t,16> v{};
    std::array<uint16_t,16> s{};
    int i{};
    int pc{};
    int sp{};
    int dt{};
    int st{};
};

class IChip8Emulator : public GenericCpu
{
public:
    struct BreakpointInfo {
        enum Type { eTRANSIENT, eCODED };
        std::string label;
        Type type{eTRANSIENT};
        bool isEnabled{true};
    };
    enum Engine {
        eCHIP8TS,       // templated core based on nested switch - this is the fastest (ch8,ch10,ch48,sc10,sc11,xo)
        eCHIP8MPT,      // method table based core - this is the most capable one (ch8,ch10,ch48,sc10,sc11,mc8,xo)
        eCHIP8VIP,      // cdp1802 based vip core running original emulator (only supports <ch48 cores, but runs hybrids)
        eCHIP8DREAM     // M6800 based DREAM6800 code running CHIPOS
    };
    //enum ExecMode { ePAUSED, eRUNNING, eSTEP, eSTEPOVER, eSTEPOUT };
    enum CpuState { eNORMAL, eWAITING, eERROR };
    using VideoType = VideoScreen<uint8_t, 256, 192>;
    using VideoRGBAType = VideoScreen<uint32_t, 256, 192>;
    virtual ~IChip8Emulator() = default;
    virtual void reset() = 0;
    virtual std::string name() const = 0;
    virtual void executeInstruction() = 0;
    virtual void executeInstructions(int numInstructions) = 0;
    virtual void tick(int instructionsPerFrame) = 0;

    virtual uint8_t getV(uint8_t index) const = 0;
    virtual uint32_t getI() const = 0;
    //virtual uint8_t getSP() const = 0;
    virtual uint8_t stackSize() const = 0;
    virtual const uint16_t* getStackElements() const = 0;

    virtual uint8_t* memory() = 0;
    virtual int memSize() const = 0;

    virtual int64_t frames() const = 0;

    virtual uint8_t delayTimer() const = 0;
    virtual uint8_t soundTimer() const = 0;

    //---------------------------------------------------------
    // Additional interfaces have default implementations that
    // allow using the unit tests without much overhead
    //---------------------------------------------------------
    virtual std::pair<uint16_t, std::string> disassembleInstruction(const uint8_t* code, const uint8_t* end) const = 0;
    virtual std::string dumpStateLine() const = 0;

    virtual bool isGenericEmulation() const { return true; }

    // defaults for unused debugger support
    virtual CpuState cpuState() const { return eNORMAL; }
    virtual uint16_t opcode() {
        return (memory()[getPC()] << 8) | memory()[getPC() + 1];
    }

    // functions with default handling to get started with tests
    virtual void handleTimer() {}
    virtual bool needsScreenUpdate() { return true; }
    virtual uint16_t getCurrentScreenWidth() const { return 64; }
    virtual uint16_t getCurrentScreenHeight() const { return 32; }
    virtual uint16_t getMaxScreenWidth() const { return 64; }
    virtual uint16_t getMaxScreenHeight() const { return 32; }
    virtual bool isDoublePixel() const { return false; }
    virtual const VideoType* getScreen() const { return nullptr; }
    virtual const VideoRGBAType* getScreenRGBA() const { return nullptr; }
    virtual void setPalette(std::array<uint32_t,256>& palette) {}

    // optional interfaces for audio and/or modern CHIP-8 variant properties
    virtual float getAudioPhase() const { return 0.0f; }
    virtual void setAudioPhase(float) { }
    virtual const uint8_t* getXOAudioPattern() const { return nullptr; }
    virtual uint8_t getXOPitch() const { return 0; }
    virtual uint8_t getNextMCSample() { return 0; }
};


} // namespace emu
