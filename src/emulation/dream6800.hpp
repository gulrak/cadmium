//---------------------------------------------------------------------------------------
// src/emulation/chip8dream.hpp
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

#include <emulation/emulatorhost.hpp>
#include <emulation/chip8realcorebase.hpp>
#include <emulation/coreregistry.hpp>
#include <emulation/hardware/m6800.hpp>
#include <emulation/iemulationcore.hpp>

namespace emu {

class Dream6800 : public Chip8RealCoreBase, public M6800Bus<>
{
public:
    //constexpr static uint32_t MAX_MEMORY_SIZE = 4096;
    //constexpr static uint32_t MAX_ADDRESS_MASK = MAX_MEMORY_SIZE-1;

    Dream6800(EmulatorHost& host, Properties& properties, IChip8Emulator* other = nullptr);
    ~Dream6800() override;

    void reset() override;
    bool updateProperties(Properties& props, Property& changed) override;
    std::string name() const override;

    // IEmulationCore
    size_t numberOfExecutionUnits() const override;
    GenericCpu* executionUnit(size_t index) override;
    void setFocussedExecutionUnit(GenericCpu* unit) override;
    GenericCpu* focussedExecutionUnit() override;

    void executeFrame() override;
    int frameRate() const override { return 50; }

    bool loadData(std::span<const uint8_t> data, std::optional<uint32_t> loadAddress) override;

    ExecMode execMode() const override;
    void setExecMode(ExecMode mode) override;

    int64_t executeFor(int64_t microseconds) override;
    int executeInstruction() override;
    void executeInstructions(int numInstructions) override;
    int64_t machineCycles() const override;

    uint8_t* memory() override;
    int memSize() const override;
    unsigned stackSize() const override;
    StackContent stack() const override;

    int64_t frames() const override;

    bool isGenericEmulation() const override { return false; }

    uint16_t getCurrentScreenWidth() const override;
    uint16_t getCurrentScreenHeight() const override;
    uint16_t getMaxScreenWidth() const override;
    uint16_t getMaxScreenHeight() const override;
    const VideoType* getScreen() const override;

    bool isDisplayEnabled() const override;

    void renderAudio(int16_t* samples, size_t frames, int sampleFrequency) override;

    // M6800-Bus
    uint8_t readByte(uint16_t addr) const override;
    uint8_t readDebugByte(uint16_t addr) const override;
    uint8_t readMemoryByte(uint32_t addr) const override;
    void writeByte(uint16_t addr, uint8_t val) override;

    GenericCpu& getBackendCpu() override;

    Properties& getProperties() override;
    void updateProperties(Property& changedProp) override;

private:
    int frameCycle() const;
    cycles_t nextFrame() const;
    //int videoLine() const;
    bool executeM6800();
    int executeVDG();
    void flushScreen();
    void fetchState();
    void forceState();
    class Private;
    std::unique_ptr<Private> _impl;
};

};
