//---------------------------------------------------------------------------------------
// src/emulation/hp48sx.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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

#include <emulation/chip8realcorebase.hpp>
#include <emulation/coreregistry.hpp>

#include <array>
#include <memory>
#include <vector>

namespace hp48 {
class G48sxMachine;
}

namespace emu {

class Hp48sxEmulationCore final : public Chip8RealCoreBase
{
public:
    Hp48sxEmulationCore(EmulatorHost& host, Properties& properties, IEmulationCore* other = nullptr);
    ~Hp48sxEmulationCore() override;

    bool updateProperties(Properties& properties, Property& changed) override;
    std::string name() const override;
    chip8::VariantSet supportedChip8Variants() const override;

    size_t numberOfExecutionUnits() const override;
    GenericCpu* executionUnit(size_t index) override;
    void setFocussedExecutionUnit(GenericCpu* unit) override;
    GenericCpu* focussedExecutionUnit() override;

    void executeFrame() override;
    int frameRate() const override;
    uint32_t defaultLoadAddress() const override;
    bool loadData(std::span<const uint8_t> data, std::optional<uint32_t> loadAddress) override;
    ExecMode execMode() const override;
    void setExecMode(ExecMode mode) override;
    int executeInstruction() override;
    void executeInstructions(int numInstructions) override;
    int64_t machineCycles() const override;

    uint8_t* memory() override;
    int memSize() const override;
    uint8_t readMemoryByte(uint32_t address) const override;
    unsigned stackSize() const override;
    StackContent stack() const override;

    bool isGenericEmulation() const override { return false; }
    uint16_t getCurrentScreenWidth() const override;
    uint16_t getCurrentScreenHeight() const override;
    uint16_t getMaxScreenWidth() const override;
    uint16_t getMaxScreenHeight() const override;
    const VideoType* getScreen() const override { return nullptr; }
    ghc::span2d<const uint32_t> getRGBASpan(ghc::span2d<uint32_t> target) const override;
    void setPalette(const Palette&) override {}
    int getMaxColors() const override { return 2; }
    bool isDisplayEnabled() const override { return true; }
    void renderAudio(int16_t* samples, size_t frames, int sampleFrequency) override;

    GenericCpu& getBackendCpu() override;
    Properties& getProperties() override;

protected:
    void handleReset(bool cold) override;

private:
    struct Private;
    std::unique_ptr<Private> _impl;
    bool executeSaturnInstruction();
    bool prepareInterpreter(std::span<const uint8_t> program);
    bool prepareRawObject(std::span<const uint8_t> object);
    void captureStartState();
    void restoreStartState();
    void fetchState();
    void refreshMemory();
    void flushMemory();
    void updateKeys();
};

}  // namespace emu
