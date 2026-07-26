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
    void fetchState();
    void refreshMemory();
    void flushMemory();
    void updateKeys();
};

}  // namespace emu
