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

#include <emulation/emulatorhost.hpp>
#include <emulation/chip8realcorebase.hpp>
#include <emulation/coreregistry.hpp>
#include <emulation/hardware/cdp1802.hpp>
#include <emulation/iemulationcore.hpp>

#include <memory>

namespace emu {

extern const uint8_t _chip8_cvip[0x200];
extern const uint8_t _rom_cvip[0x200];

enum VIPChip8Interpreter { VC8I_NONE, VC8I_CHIP8, VC8I_CHIP10, VC8I_CHIP8RB, VC8I_CHIP8TPD, VC8I_CHIP8FPD, VC8I_CHIP8X, VC8I_CHIP8XTPD, VC8I_CHIP8XFPD, VC8I_CHIP8E };

/// COSMAC VIP Emulation Core
/// This core emulates the hardware of a COSMAC VIP and allows a few hardware configuration
/// options. It offers two execution units:
///   - A low-level CDP1802 driven one that runs the COSMAC VIP (Unit 0)
///   - A high-level CHIP-8 interpreter driven one that sits on-top of the COSMAC VIP
///     and allows to use the COSMAC VIP via a CIP-8 centric API as if it was a generic
///     VIP emulator (Unit 1, only available if an integrated interpreter is selected)
class CosmacVIP : public Chip8RealCoreBase, public Cdp1802Bus
{
public:
    CosmacVIP(EmulatorHost& host, Properties& properties, IEmulationCore* other = nullptr);
    ~CosmacVIP() override;

    void reset() override;
    bool updateProperties(Properties& props, Property& changed) override;
    std::string name() const override;

    // IEmulationCore
    size_t numberOfExecutionUnits() const override;
    GenericCpu* executionUnit(size_t index) override;
    void setFocussedExecutionUnit(GenericCpu* unit) override;
    GenericCpu* focussedExecutionUnit() override;

    void executeFrame() override;
    int frameRate() const override;

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
    void setPalette(const Palette& palette) override;

    bool isDisplayEnabled() const override;

    //float getAudioPhase() const override;
    //void setAudioPhase(float phase) override;
    //float getAudioFrequency() const override;
    void renderAudio(int16_t* samples, size_t frames, int sampleFrequency) override;

    // CDP1802-Bus
    uint8_t readByte(uint16_t addr) const override;
    uint8_t readByteDMA(uint16_t addr) const override;
    uint8_t readMemoryByte(uint32_t addr) const override;
    void writeByte(uint16_t addr, uint8_t val) override;

    GenericCpu& getBackendCpu() override;

    Properties& getProperties() override;

    static std::vector<uint8_t> getInterpreterCode(const std::string& name);

private:
    static uint16_t justPatchRAM(VIPChip8Interpreter interpreter, uint8_t* ram, size_t size);
    uint16_t patchRAM(VIPChip8Interpreter interpreter, uint8_t* ram, size_t size);
    int frameCycle() const;
    int videoLine() const;
    bool executeCdp1802();
    void fetchState();
    void forceState();
    class Private;
    std::unique_ptr<Private> _impl;
};

}
