//---------------------------------------------------------------------------------------
// test/multipage_display_tests.cpp
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
#include <doctest/doctest.h>

#include "chip8headless.hpp"

#include <emulation/ichip8.hpp>

#include <cstdint>
#include <initializer_list>
#include <utility>
#include <string>
#include <vector>

namespace {

struct MultiPageVariant
{
    const char* generic;
    const char* vip;
    uint16_t startAddress;
    int lines;
};

const MultiPageVariant TPD{"chip-8-tpd", "vip-chip-8-tpd", 0x260, 64};
const MultiPageVariant FPD{"chip-8-fpd", "vip-chip-8-fpd", 0x244, 128};

constexpr int SPRITE_OFFSET = 0x40;

struct Result
{
    std::string screen;
    uint8_t vF{};
    uint8_t v5{};
};

// All programs are followed by a filled sprite and end with v5 := v5 + 1 and an endless loop on that last jump.
std::vector<uint8_t> program(const MultiPageVariant& variant, const std::vector<uint16_t>& opcodes)
{
    std::vector<uint8_t> rom;
    const auto addOpcode = [&](uint16_t opcode) {
        rom.push_back(opcode >> 8);
        rom.push_back(opcode & 0xFF);
    };
    addOpcode(0xA000 | (variant.startAddress + SPRITE_OFFSET));
    for (const auto opcode : opcodes) {
        addOpcode(opcode);
    }
    addOpcode(0x7501);
    addOpcode(0x1000 | (variant.startAddress + rom.size()));
    REQUIRE(rom.size() <= SPRITE_OFFSET);
    rom.resize(SPRITE_OFFSET, 0);
    rom.resize(SPRITE_OFFSET + 16, 0xFF);
    return rom;
}

Result run(const char* preset, int lines, const std::vector<uint8_t>& rom)
{
    auto instance = createChip8Instance(preset);
    auto& host = std::get<0>(instance);
    auto* chip8 = host->chip8Emulator();
    chip8->reset();
    REQUIRE(host->load(rom));
    for (int frame = 0; frame < 60; ++frame) {
        host->executeFrame();
    }
    Result result{{}, chip8->getV(15), chip8->getV(5)};
    auto* core = dynamic_cast<emu::IEmulationCore*>(chip8);
    REQUIRE(core != nullptr);
    // The VIP always shows 128 lines, so a line of the program covers several of them
    const int step = core->getCurrentScreenHeight() / lines;
    const auto dump = [&](const auto* screen) {
        for (int y = 0; y < lines; ++y) {
            for (int x = 0; x < 64; ++x) {
                result.screen.push_back(screen->getPixelDebugBW(x, y * step));
            }
            result.screen.push_back('\n');
        }
    };
    if (const auto* screen = core->getScreen()) {
        dump(screen);
    }
    else {
        dump(core->getScreenRGBA());
    }
    return result;
}

void checkSameScreenAsVip(const MultiPageVariant& variant, const std::vector<uint16_t>& opcodes, bool compareVF = true)
{
    const auto rom = program(variant, opcodes);
    const auto generic = run(variant.generic, variant.lines, rom);
    const auto vip = run(variant.vip, variant.lines, rom);
    CHECK(vip.v5 == 1);
    CHECK(generic.v5 == vip.v5);
    if (compareVF) {
        CHECK(generic.vF == vip.vF);
    }
    CHECK(generic.screen == vip.screen);
}

void checkSameAsVip(const MultiPageVariant& variant, const std::vector<uint16_t>& opcodes)
{
    checkSameScreenAsVip(variant, opcodes);
}

}

TEST_SUITE_BEGIN("MultiPageDisplay");

TEST_CASE("The screen has the logical lines and stays 2:1 when shown")
{
    for (const auto& [variant, ratio] : {std::pair{TPD, 2}, std::pair{FPD, 4}}) {
        INFO(std::string(variant.generic));
        auto instance = createChip8Instance(variant.generic);
        auto* core = dynamic_cast<emu::IEmulationCore*>(std::get<0>(instance)->chip8Emulator());
        REQUIRE(core != nullptr);
        CHECK(core->getCurrentScreenWidth() == 64);
        CHECK(core->getCurrentScreenHeight() == variant.lines);
        CHECK(core->getMaxScreenHeight() >= variant.lines);
        REQUIRE(core->getScreen() != nullptr);
        CHECK(core->getScreen()->ratio() == ratio);
        CHECK(core->defaultLoadAddress() == variant.startAddress);
    }
}

TEST_CASE("Sprites draw, clip and wrap like on the real interpreters")
{
    for (const auto* variantPtr : {&TPD, &FPD}) {
        const auto& variant = *variantPtr;
        INFO(std::string(variant.generic));
        const uint16_t lastLines = 0x6400 | (variant.lines - 4);
        const uint16_t beyondHeight = 0x6400 | (variant.lines + 8);
        const auto check = [&](const char* what, std::initializer_list<uint16_t> opcodes) {
            INFO(std::string(what));
            checkSameAsVip(variant, opcodes);
        };
        check("inside of the screen", {0x630B, 0x6408, 0xD348});
        check("below line 32", {0x630B, 0x6428, 0xD348});
        check("over the bottom edge", {0x630B, lastLines, 0xD348});
        check("y beyond the height wraps", {0x630B, beyondHeight, 0xD348});
        check("x beyond the width wraps", {0x634B, 0x6408, 0xD348});
        check("over the right edge", {0x633C, 0x6408, 0xD348});
        // What spills out of the last line of FPD is written to 0x1000, on the VIP that is the first interpreter byte at
        // 0x000 (RAM mirror), and its bits give a collision. There is no such byte here, so only the screen can match.
        INFO("over the right edge in the last lines");
        checkSameScreenAsVip(variant, {0x633C, lastLines, 0xD348}, &variant == &TPD);
        check("collision", {0x630B, 0x6408, 0xD348, 0xD342});
        check("collision only in the part over the right edge", {0x6300, 0x6409, 0xD341, 0x633C, 0x6408, 0xD348});
    }
}

TEST_CASE("Clearing the screen like on the real interpreters")
{
    // 15 lines each at x=11
    const std::initializer_list<uint16_t> tpdFill = {0x630B, 0x6408, 0xD34F, 0x6428, 0xD34F};
    const auto withFill = [](std::initializer_list<uint16_t> fill, std::initializer_list<uint16_t> opcodes) {
        std::vector<uint16_t> result(fill);
        result.insert(result.end(), opcodes);
        return result;
    };
    const auto check = [](const char* what, const MultiPageVariant& variant, const std::vector<uint16_t>& opcodes) {
        INFO(std::string(what));
        checkSameAsVip(variant, opcodes);
    };
    check("TPD 0230", TPD, withFill(tpdFill, {0x0230}));
    check("TPD 004B leaves the screen", TPD, withFill(tpdFill, {0x004B}));
    // One sprite in each of the four pages of 32 lines
    const std::initializer_list<uint16_t> fpdFill = {0x630B, 0x6408, 0xD34F, 0x6428, 0xD34F, 0x6448, 0xD34F, 0x6468, 0xD34F};
    check("FPD 0200", FPD, withFill(fpdFill, {0x0200}));
    check("FPD 004B", FPD, withFill(fpdFill, {0x004B}));
    check("FPD 0200 doesn't touch vF", FPD, withFill(fpdFill, {0xD34F, 0x0200}));
    for (uint16_t pages = 1; pages <= 4; ++pages) {
        CAPTURE(pages);
        check("FPD 0216 sets the pages of 0200", FPD, withFill(fpdFill, {static_cast<uint16_t>(0x6000 | pages), 0x0216, 0x0200}));
    }
}

TEST_CASE("00E0 is no clear on these interpreters")
{
    for (const auto& variant : {TPD, FPD}) {
        INFO(std::string(variant.generic));
        const auto result = run(variant.generic, variant.lines, program(variant, {0x00E0}));
        CHECK(result.v5 == 0);
        auto instance = createChip8Instance(variant.generic);
        auto& host = std::get<0>(instance);
        auto* chip8 = host->chip8Emulator();
        chip8->reset();
        REQUIRE(host->load(program(variant, {0x00E0})));
        host->executeFrame();
        CHECK(chip8->cpuState() == emu::GenericCpu::eERROR);
    }
}

TEST_CASE("Timing follows the interrupt routines of the real interpreters")
{
    const auto opcodesPerFrame = [](const MultiPageVariant& variant, const char* preset) {
        std::vector<uint8_t> rom{0x70, 0x01, static_cast<uint8_t>(0x10 | (variant.startAddress >> 8)), static_cast<uint8_t>(variant.startAddress)};
        auto instance = createChip8Instance(preset);
        auto& host = std::get<0>(instance);
        auto* chip8 = host->chip8Emulator();
        chip8->reset();
        REQUIRE(host->load(rom));
        for (int frame = 0; frame < 30; ++frame) {
            host->executeFrame();
        }
        int sum = 0;
        int last = chip8->getV(0);
        for (int frame = 0; frame < 200; ++frame) {
            host->executeFrame();
            sum += (chip8->getV(0) - last) & 0xFF;
            last = chip8->getV(0);
        }
        return sum * 2 / 200.0;
    };
    const double vipTpd = opcodesPerFrame(TPD, TPD.vip);
    const double vipFpd = opcodesPerFrame(FPD, FPD.vip);
    const double genericTpd = opcodesPerFrame(TPD, TPD.generic);
    const double genericFpd = opcodesPerFrame(FPD, FPD.generic);
    CHECK(vipFpd / vipTpd == doctest::Approx(genericFpd / genericTpd).epsilon(0.05));
}

TEST_CASE("File extensions stay with the real interpreters")
{
    CHECK(emu::CoreRegistry::presetForExtension(".c8tpd") == "VIP-CHIP-8 TPD");
    CHECK(emu::CoreRegistry::presetForExtension(".c8h") == "VIP-CHIP-8 TPD");
    CHECK(emu::CoreRegistry::presetForExtension(".c8fpd") == "VIP-CHIP-8 FPD");
}

TEST_SUITE_END();
