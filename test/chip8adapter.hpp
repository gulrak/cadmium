//---------------------------------------------------------------------------------------
// tests/chip8adapter.hpp
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

#include <emulation/ichip8.hpp>
#include <emulation/emulatorhost.hpp>
#include <emulation/chip8options.hpp>
#include <memory>

//#define TEST_CHIP8EMULATOR_FP
//#define TEST_CHIP8EMULATOR_TS
//#define TEST_C_OCTO
//#define TEST_JAMES_GRIFFIN_CHIP_8
//#define TEXT_WERNSEY_CHIP_8

class Chip8HeadlessTestHost : public emu::EmulatorHost
{
public:
    explicit Chip8HeadlessTestHost(const emu::Chip8EmulatorOptions& options_) : options(options_) {}
    ~Chip8HeadlessTestHost() override = default;
    bool isHeadless() const override { return true; }
    int getKeyPressed() override { return 0; }
    bool isKeyDown(uint8_t key) override { return false; }
    const std::array<bool,16>& getKeyStates() const override { static const std::array<bool,16> keys{}; return keys; }
    void updateScreen() override {}
    void vblank() override {}
    void updatePalette(const std::array<uint8_t,16>& palette) override {}
    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override {}
    emu::Chip8EmulatorOptions options;
};

enum Chip8TestVariant { C8TV_GENERIC, C8TV_C8, C8TV_C10, C8TV_C48, C8TV_SC10, C8TV_SC11, C8TV_MC8, C8TV_XO };
using EmuCore = std::unique_ptr<emu::IChip8Emulator>;
extern EmuCore createChip8Instance(Chip8TestVariant variant = C8TV_GENERIC);
