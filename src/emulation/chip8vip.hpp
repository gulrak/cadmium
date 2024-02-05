//---------------------------------------------------------------------------------------
// src/emulation/chip8vip.hpp
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

#include <emulation/chip8emulatorhost.hpp>
#include <emulation/chip8realcorebase.hpp>
#include <emulation/hardware/cdp1802.hpp>

#include <memory>

namespace emu {

extern const uint8_t _chip8_cvip[0x200];
extern const uint8_t _rom_cvip[0x200];

class Chip8VIP : public Chip8RealCoreBase, public Cdp1802Bus
{
public:
    constexpr static uint32_t MAX_MEMORY_SIZE = 4096;
    constexpr static uint32_t MAX_ADDRESS_MASK = MAX_MEMORY_SIZE-1;

    Chip8VIP(Chip8EmulatorHost& host, Chip8EmulatorOptions& options, IChip8Emulator* other = nullptr);
    ~Chip8VIP() override;

    void reset() override;
    std::string name() const override;
    int64_t executeFor(int64_t microseconds) override;
    void executeInstruction() override;
    void executeInstructions(int numInstructions) override;
    void tick(int instructionsPerFrame) override;
    int frameRate() const override { return 60; }
    int64_t getMachineCycles() const override;

    uint8_t* memory() override;
    int memSize() const override;

    int64_t frames() const override;

    bool isGenericEmulation() const override { return false; }

    uint16_t getCurrentScreenWidth() const override;
    uint16_t getCurrentScreenHeight() const override;
    uint16_t getMaxScreenWidth() const override;
    uint16_t getMaxScreenHeight() const override;
    const VideoType* getScreen() const override;

    bool isDisplayEnabled() const override;

    //float getAudioPhase() const override;
    //void setAudioPhase(float phase) override;
    //float getAudioFrequency() const override;
    void renderAudio(int16_t* samples, size_t frames, int sampleFrequency) override;

    // CDP1802-Bus
    uint8_t readByte(uint16_t addr) const override;
    uint8_t readByteDMA(uint16_t addr) const override;
    uint8_t getMemoryByte(uint32_t addr) const override;
    void writeByte(uint16_t addr, uint8_t val) override;

    GenericCpu& getBackendCpu() override;

    std::pair<std::string_view,std::string_view> romInfo() override;
    std::pair<std::string_view,std::string_view> interpreterInfo() override;

    static std::vector<uint8_t> getInterpreterCode(const std::string& name);

private:
    static uint16_t patchRAM(std::string name, uint8_t* ram, size_t size);
    int frameCycle() const;
    int videoLine() const;
    bool executeCdp1802();
    void fetchState();
    void forceState();
    class Private;
    std::unique_ptr<Private> _impl;
};

}
