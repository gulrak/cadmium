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

#include <emulation/chip8emulatorhost.hpp>
#include <emulation/chip8options.hpp>
#include <chiplet/octocompiler.hpp>
#include <librarian.hpp>

#include <array>
#include <string>
#include <vector>

namespace emu {

class IChip8Emulator;

class Chip8EmuHostEx : public Chip8EmulatorHost
{
public:
    Chip8EmuHostEx();
    ~Chip8EmuHostEx() override = default;
    //virtual bool isHeadless() const = 0;
    //virtual uint8_t getKeyPressed() = 0;
    //virtual bool isKeyDown(uint8_t key) = 0;
    //virtual bool isKeyUp(uint8_t key) { return !isKeyDown(key); }
    //virtual const std::array<bool,16>& getKeyStates() const = 0;
    //virtual void updateScreen() = 0;
    //virtual void updatePalette(const std::array<uint8_t, 16>& palette) = 0;
    //virtual void updatePalette(const std::vector<uint32_t>& palette, size_t offset) = 0;
    virtual bool loadRom(const char* filename, bool andRun);
    void updateEmulatorOptions(Chip8EmulatorOptions options);
    void setPalette(const std::vector<uint32_t>& colors, size_t offset = 0);
protected:
    virtual void whenRomLoaded(const std::string& filename, bool autoRun, emu::OctoCompiler* compiler, const std::string& source) {}
    virtual void whenEmuChanged(IChip8Emulator& emu) {}
    CadmiumConfiguration _cfg;
    std::string _cfgPath;
    std::string _databaseDirectory;
    std::string _currentDirectory;
    std::string _currentFileName;
    Librarian _librarian;
    std::unique_ptr<IChip8Emulator> _chipEmu;
    std::string _romName;
    std::vector<uint8_t> _romImage;
    std::string _romSha1Hex;
    bool _romIsWellKnown{false};
    bool _customPalette{false};
    std::array<uint32_t, 256> _colorPalette{};
    std::array<uint32_t, 256> _defaultPalette{};
    emu::Chip8EmulatorOptions _options;
    emu::Chip8EmulatorOptions _romWellKnownOptions;
    emu::Chip8EmulatorOptions _previousOptions;
};

class Chip8HeadlessHostEx : public Chip8EmuHostEx
{
public:
    explicit Chip8HeadlessHostEx() {}
    ~Chip8HeadlessHostEx() override = default;
    Chip8EmulatorOptions& options() { return _options; }
    IChip8Emulator& chipEmu() { return *_chipEmu; }
    bool isHeadless() const override { return true; }
    uint8_t getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void updateScreen() override {}
    void updatePalette(const std::array<uint8_t,16>& palette) override {}
    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override {}
};

}
