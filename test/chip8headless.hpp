//---------------------------------------------------------------------------------------
// test/chip8headless.hpp
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
// Shared headless host and preset factory for tests and tools.
#pragma once

#include <emulation/coreregistry.hpp>
#include <memory>
#include <span>
#include <tuple>

class HeadlessTestHost : public emu::EmulatorHost
{
public:
    explicit HeadlessTestHost(const emu::Properties& props) : _props(props) {}
    ~HeadlessTestHost() override = default;
    bool isHeadless() const override { return true; }
    int getKeyPressed() override
    {
        auto released = (_keys ^ _lastKeys) & ~_keys;
        for(uint8_t i = 0; i < 16; ++i) {
            if(released & (1 << i)) {
                _lastKeys &= ~(1 << i);
                return i + 1;
            }
        }
        return -1;
    }
    bool isKeyDown(uint8_t key) override { return key < 16 ? _keys & (1 << key) : false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void updateScreen() override {}
    void vblank() override {}
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
    void writeByte(uint32_t address, uint8_t value)
    {
        if(address < _core->memSize()) {
            _core->memory()[address] = value;
        }
    }
    void executeFrame()
    {
        _core->setExecMode(emu::GenericCpu::eRUNNING);
        _core->executeFrame();
    }
    struct Rect
    {
        int x{-1};
        int y{-1};
        int w{};
        int h{};
    };
    Rect findContentRectangle(int scaleX, int scaleY) const
    {
        Rect result;
        int right = -1;
        int bottom = -1;
        auto width = _core->getCurrentScreenWidth();
        auto height = _core->getCurrentScreenHeight();
        auto scanScreen = [&](const auto* screen) {
            for (int y = 0; y < height; y += scaleY) {
                for (int x = 0; x < width; x += scaleX) {
                    if(screen->getPixelDebugBW(x, y) == '#') {
                        if(result.x < 0 || result.x > x) result.x = x;
                        if(result.y < 0) result.y = y;
                        if(right < x) right = x;
                        if(bottom < y) bottom = y;
                    }
                }
            }
        };
        if(const auto* screenPal = _core->getScreen()) {
            scanScreen(screenPal);
        }
        else if(const auto* screenRGB = _core->getScreenRGBA()) {
            scanScreen(screenRGB);
        }
        else {
            return {0,0,0,0};
        }
        if(result.x < 0) return {0,0,0,0};
        result.w = right - result.x + 1;
        result.h = bottom - result.y + 1;
        return result;
    }
    std::string chip8EmuScreen(bool hires = false) const
    {
        std::string result;
        auto width = _core->getCurrentScreenWidth();
        auto height = _core->getCurrentScreenHeight();
        if(!hires) {
            auto scaleX = width / 64;
            auto scaleY = height / 32;
            auto workScreen = [&](const auto* screen) {
                result.reserve(width * height + height);
                for (int y = 0; y < height; y += scaleY) {
                    for (int x = 0; x < width; x += scaleX) {
                        result.push_back(screen->getPixelDebugBW(x, y));
                    }
                    result.push_back('\n');
                }
            };
            if(const auto* screenPal = _core->getScreen()) {
                workScreen(screenPal);
            }
            else if(const auto* screenRGB = _core->getScreenRGBA()) {
                workScreen(screenRGB);
            }
        }
        return result;
    }
    std::pair<Rect, std::string> chip8UsedScreen(int scaleX = 1, int scaleY = 1)
    {
        std::string result;
        auto width = _core->getCurrentScreenWidth();
        auto height = _core->getCurrentScreenHeight();
        executeFrame();
        executeFrame();
        auto workScreen = [&](const auto* screen) -> std::pair<Rect, std::string> {
            auto rect = findContentRectangle(scaleX, scaleY);
            if(rect.w) {
                result.reserve(rect.w * rect.h + rect.h);
                for (int y = 0; y < height; y += scaleY) {
                    if(y >= rect.y && y < rect.y + rect.h) {
                        for (int x = 0; x < width; x += scaleX) {
                            if(x >= rect.x && x < rect.x + rect.w) {
                                result.push_back(screen->getPixelDebugBW(x, y));
                            }
                        }
                        result.push_back('\n');
                    }
                }
                return {{rect.x/scaleX, rect.y/scaleY, rect.w/scaleX, rect.h/scaleY}, result};
            }
            return {{0,0,0,0}, result};
        };
        if(const auto* screen = _core->getScreen()) {
            return workScreen(screen);
        }
        if(const auto* screen = _core->getScreenRGBA()) {
            return workScreen(screen);
        }
        return {{0,0,0,0}, result};
    }
    std::pair<Rect, std::string> chip8UsedScreenCol(int scaleX = 1, int scaleY = 1)
    {
        std::string result;
        auto width = _core->getCurrentScreenWidth();
        auto height = _core->getCurrentScreenHeight();
        executeFrame();
        executeFrame();
        auto workScreen = [&](const auto* screen) -> std::pair<Rect, std::string> {
            auto rect = findContentRectangle(scaleX, scaleY);
            if(rect.w) {
                result.reserve(rect.w * rect.h + rect.h);
                for (int y = 0; y < height; y += scaleY) {
                    if(y >= rect.y && y < rect.y + rect.h) {
                        for (int x = 0; x < width; x += scaleX) {
                            if(x >= rect.x && x < rect.x + rect.w) {
                                result.push_back(screen->getPixelDebugCol(x, y));
                            }
                        }
                        result.push_back('\n');
                    }
                }
                return {{rect.x/scaleX, rect.y/scaleY, rect.w/scaleX, rect.h/scaleY}, result};
            }
            return {{0,0,0,0}, result};
        };
        if(const auto* screen = _core->getScreen()) {
            return workScreen(screen);
        }
        if(const auto* screen = _core->getScreenRGBA()) {
            return workScreen(screen);
        }
        return {{0,0,0,0}, result};
    }

    std::pair<Rect, std::string> screenUsedOnNextKeyWait(int scaleX = 1, int scaleY = 1)
    {
        auto core = chip8Emulator();
        int overFrames = 2;
        while (core->execMode() != emu::GenericCpu::ePAUSED) {
            executeFrame();
            if((core->opcode() & 0xF0FF) == 0xF00A && --overFrames == 0) {
                break;
            }
        }
        return chip8UsedScreen(scaleX, scaleY);
    }

    void selectKey(uint8_t key)
    {
        _lastKeys = _keys;
        _keys |= (1 << (key & 0xF));
        executeFrame();
        executeFrame();
        _lastKeys = _keys;
        _keys &= ~(1 << (key & 0xF));
        executeFrame();
        executeFrame();
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
    auto [variantName, emuCore] = emu::CoreRegistry::create(*host, host->properties(), nullptr);
    host->setCore(std::move(emuCore));
    return std::make_tuple(std::move(host), host->chip8Emulator(), startAddress ? startAddress->intValue : 0x200);
}
