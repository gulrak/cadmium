//---------------------------------------------------------------------------------------
// src/emulation/chip8emuhostex.hpp
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

#include <emulation/chip8options.hpp>
#include <emulation/emulatorhost.hpp>
#include <emulation/iemulationcore.hpp>
#include <emulation/palette.hpp>
#include <chiplet/octocompiler.hpp>
#include <ghc/bitenum.hpp>
#include <ghc/span.hpp>
#include <librarian.hpp>
#ifndef PLATFORM_WEB
#include <threadpool.hpp>
#include <database.hpp>
#endif

#include <array>
#include <string>
#include <vector>

namespace emu {

class IChip8Emulator;

/*
class Chip8HeadlessHost : public Chip8EmulatorHost
{
public:
    explicit Chip8HeadlessHost(const Chip8EmulatorOptions& options_) : options(options_) {}
    ~Chip8HeadlessHost() override = default;
    bool isHeadless() const override { return true; }
    uint8_t getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void updateScreen() override {}
    void vblank() override {}
    void updatePalette(const std::array<uint8_t,16>& palette) override {}
    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override {}
    Chip8EmulatorOptions options;
};
*/

class EmuHostEx : public EmulatorHost
{
public:
    enum LoadOptions { None = 0, DontChangeOptions = 1, SetToRun = 2 };
    explicit EmuHostEx(CadmiumConfiguration& cfg);
    ~EmuHostEx() override = default;
    //virtual bool isHeadless() const = 0;
    //virtual uint8_t getKeyPressed() = 0;
    //virtual bool isKeyDown(uint8_t key) = 0;
    //virtual bool isKeyUp(uint8_t key) { return !isKeyDown(key); }
    //virtual const std::array<bool,16>& getKeyStates() const = 0;
    //virtual void updateScreen() = 0;
    //virtual void updatePalette(const std::array<uint8_t, 16>& palette) = 0;
    //virtual void updatePalette(const std::vector<uint32_t>& palette, size_t offset) = 0;
    virtual bool loadRom(std::string_view filename, LoadOptions loadOpt);
    virtual bool loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, LoadOptions loadOpt);
    virtual bool loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, const Properties& props, bool isKnown);
    void updateEmulatorOptions(const Properties& properties);
    void setPalette(const std::vector<uint32_t>& colors, size_t offset = 0);
    void setPalette(const Palette& palette);

protected:
    std::unique_ptr<IEmulationCore> create(Properties& properties, IEmulationCore* iother = nullptr);
    virtual void whenRomLoaded(const std::string& filename, bool autoRun, emu::OctoCompiler* compiler, const std::string& source) {}
    virtual void whenEmuChanged(emu::IEmulationCore& emu) {}
    CadmiumConfiguration& _cfg;
    CoreRegistry _cores;
    std::string _cfgPath;
    std::string _databaseDirectory;
    std::string _currentDirectory;
    std::string _currentFileName;
    Librarian _librarian;
    std::unordered_map<std::string, std::string> _badges;
#ifndef PLATFORM_WEB
    ThreadPool _threadPool;
    Database _database;
#endif
    std::unique_ptr<IEmulationCore> _chipEmu;
    std::string _romName;
    std::vector<uint8_t> _romImage;
    Sha1::Digest _romSha1;
    bool _romIsWellKnown{false};
    bool _customPalette{false};
    Palette _colorPalette;
    Palette _defaultPalette;
    //std::array<uint32_t, 256> _colorPalette{};
    //std::array<uint32_t, 256> _defaultPalette{};
    emu::Properties* _properties{nullptr};
    std::map<std::string, Properties> _propertiesByClass;
    std::string _variantName;
    emu::Properties _romWellKnownProperties;
    emu::Properties _previousProperties;
};

GHC_ENUM_ENABLE_BIT_OPERATIONS(EmuHostEx::LoadOptions)

class HeadlessHost : public EmuHostEx
{
public:
    HeadlessHost() : EmuHostEx(_cfg) {}
    explicit HeadlessHost(const Properties& options) : EmuHostEx(_cfg) { updateEmulatorOptions(options); }
    ~HeadlessHost() override = default;
    Properties& getProperties() { return *_properties; }
    IEmulationCore& emuCore() { return *_chipEmu; }
    bool isHeadless() const override { return true; }
    int getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void updateScreen() override {}
    void vblank() override {}
    void updatePalette(const std::array<uint8_t,16>& palette) override {}
    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override {}
private:
    CadmiumConfiguration _cfg;
};

class ThreadedBackgroundHost : public EmuHostEx
{
public:
    ThreadedBackgroundHost() : EmuHostEx(_cfg)
    {
        _screen = GenImageColor(emu::SUPPORTED_SCREEN_WIDTH, emu::SUPPORTED_SCREEN_HEIGHT, BLACK);
        _screenTexture = LoadTextureFromImage(_screen);
    }
    explicit ThreadedBackgroundHost(const Properties& options) : EmuHostEx(_cfg) { updateEmulatorOptions(options); }
    ~ThreadedBackgroundHost() override = default;
    Properties& getProperties() { return *_properties; }
    IEmulationCore& emuCore() { return *_chipEmu; }
    bool isHeadless() const override { return true; }
    int getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void updateScreen() override
    {
        auto* pixel = static_cast<uint32_t*>(_screen.data);
        if(pixel) {
            if (const auto* screen = _chipEmu->getScreen()) {
                screen->convert(pixel, _screen.width, 255, nullptr);
                UpdateTexture(_screenTexture, _screen.data);
            }
            else {
                if (const auto* screenRgb = _chipEmu->getScreenRGBA()) {
                    screenRgb->convert(pixel, _screen.width, _chipEmu->getScreenAlpha(), _chipEmu->getWorkRGBA());
                    UpdateTexture(_screenTexture, _screen.data);
                }
            }
        }
    }
    void vblank() override {}
    void updatePalette(const std::array<uint8_t,16>& palette) override {}
    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override {}
private:
    CadmiumConfiguration _cfg;
    Image _screen;
    Texture2D _screenTexture;
};

}
