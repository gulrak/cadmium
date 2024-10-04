//---------------------------------------------------------------------------------------
// tests/chip8testhelper.hpp
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

#include <emulation/coreregistry.hpp>
#include <iostream>

struct Chip8State
{
    int i{};
    int pc{};
    int sp{};
    int dt{};
    int st{};
    std::array<int,16> v{};
    std::array<int,16> stack{};
    inline static int stepCount = 0;
    inline static std::string pre;
    inline static std::string post;
};

inline void CheckState(emu::IChip8Emulator* chip8, const Chip8State& expected, std::string comment = "")
{
    //std::cerr << "MC: " << chip8->getMachineCycles() << std::endl;
    std::string message = (comment.empty()?"":"\nAfter step #" + std::to_string(Chip8State::stepCount) + "\nCOMMENT: " + comment) + "\nPRE:  " + Chip8State::pre + "\nPOST: " + Chip8State::post;
    INFO(message);
    if(expected.i >= 0) CHECK(expected.i == chip8->getI());
    if(expected.pc >= 0) CHECK(expected.pc == chip8->getPC());
    if(expected.sp >= 0) CHECK(expected.sp == chip8->getSP());
    if(expected.dt >= 0) CHECK(expected.dt == chip8->delayTimer());
    if(expected.st >= 0) CHECK(expected.st == chip8->soundTimer());

    if(expected.v[0] >= 0) CHECK(expected.v[0] == chip8->getV(0));
    if(expected.v[1] >= 0) CHECK(expected.v[1] == chip8->getV(1));
    if(expected.v[2] >= 0) CHECK(expected.v[2] == chip8->getV(2));
    if(expected.v[3] >= 0) CHECK(expected.v[3] == chip8->getV(3));
    if(expected.v[4] >= 0) CHECK(expected.v[4] == chip8->getV(4));
    if(expected.v[5] >= 0) CHECK(expected.v[5] == chip8->getV(5));
    if(expected.v[6] >= 0) CHECK(expected.v[6] == chip8->getV(6));
    if(expected.v[7] >= 0) CHECK(expected.v[7] == chip8->getV(7));
    if(expected.v[8] >= 0) CHECK(expected.v[8] == chip8->getV(8));
    if(expected.v[9] >= 0) CHECK(expected.v[9] == chip8->getV(9));
    if(expected.v[10] >= 0) CHECK(expected.v[0xA] == chip8->getV(0xA));
    if(expected.v[11] >= 0) CHECK(expected.v[0xB] == chip8->getV(0xB));
    if(expected.v[12] >= 0) CHECK(expected.v[0xC] == chip8->getV(0xC));
    if(expected.v[13] >= 0) CHECK(expected.v[0xD] == chip8->getV(0xD));
    if(expected.v[14] >= 0) CHECK(expected.v[0xE] == chip8->getV(0xE));
    if(expected.v[15] >= 0) CHECK(expected.v[0xF] == chip8->getV(0xF));

    for (int i = 0; i < expected.sp; ++i) {
        if(expected.stack[i] >= 0)
            CHECK(expected.stack[i] == chip8->stackElement(i));
    }
}

inline void write(emu::IChip8Emulator* chip8, uint32_t address, std::initializer_list<uint16_t> values)
{
    size_t offset = 0;
    for(auto val : values) {
        chip8->memory()[address + offset++] = val>>8;
        chip8->memory()[address + offset++] = val & 0xFF;
    }
    Chip8State::stepCount = 0;
}

inline void step(emu::IChip8Emulator* chip8)
{
    Chip8State::pre = chip8->dumpStateLine();
    chip8->setExecMode(emu::IChip8Emulator::eRUNNING);
    chip8->executeInstruction();
    Chip8State::stepCount++;
    Chip8State::post = chip8->dumpStateLine();
}

class HeadlessTestHost : public emu::EmulatorHost
{
public:
    explicit HeadlessTestHost(const emu::Properties& props) : _props(props) {}
    ~HeadlessTestHost() override = default;
    bool isHeadless() const override { return true; }
    int getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return key < 16 ? _keys & (1 << key) : false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void updateScreen() override {}
    void vblank() override { _lastKeys = _keys; }
    void updatePalette(const std::array<uint8_t,16>& palette) override {}
    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override {}
    void setCore(std::unique_ptr<emu::IEmulationCore>&& core) { _core = std::move(core); }
    emu::IChip8Emulator* chip8Emulator() const { return dynamic_cast<emu::IChip8Emulator*>(_core->executionUnit(0)); }
    emu::Properties& properties() { return _props; }
    void keyDown(uint8_t key) { if(key < 16) _keys |= 1 << key; }
    void keyUp(uint8_t key) { if(key < 16) _keys &= ~(1 << key); }
    bool load(std::span<const uint8_t> data)
    {
        return _core->loadData(data, {});
    }
    void executeFrame()
    {
        _core->executeFrame();
    }
    std::string chip8EmuScreen(bool hires = false)
    {
        std::string result;
        auto width = _core->getCurrentScreenWidth();
        auto height = _core->getCurrentScreenHeight();
        if(!hires) {
            auto scaleX = width / 64;
            auto scaleY = height / 32;
            const auto* screen = _core->getScreen();
            if(screen) {
                result.reserve(width * height + height);
                for (int y = 0; y < height; y += scaleY) {
                    for (int x = 0; x < width; x += scaleX) {
                        result.push_back(screen->getPixel(x, y) ? '#' : '.');
                    }
                    result.push_back('\n');
                }
            }
        }
        return result;
    }
private:
    emu::Properties _props;
    std::unique_ptr<emu::IEmulationCore> _core;
    uint16_t _keys{0};
    uint16_t _lastKeys{0};
};

inline std::tuple<std::unique_ptr<HeadlessTestHost>, emu::IChip8Emulator*, int> createChip8Instance(std::string presetName)
{
    auto properties = emu::CoreRegistry::propertiesForPreset(presetName);
    auto startAddress = properties.get<emu::Property::Integer>("startAddress");
    auto host = std::make_unique<HeadlessTestHost>(properties);
    auto [variantName, emuCore] = emu::CoreRegistry::create(*host, host->properties());
    host->setCore(std::move(emuCore));
    return std::make_tuple(std::move(host), host->chip8Emulator(), startAddress ? startAddress->intValue : 0x200);
}
