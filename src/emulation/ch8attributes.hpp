//---------------------------------------------------------------------------------------
// src/emulation/ch8attributes.hpp
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

#include <emulation/properties.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace emu {

// The CH8 attribute header (C8RFC-001, format version 0): variant, speed hint and quirks
// behind the jump at the start of a program, see examples/Universal/inc/C8RFC-001.md
struct Ch8Attributes
{
    static constexpr uint8_t QUIRK_LOGIC = 0x01;
    static constexpr uint8_t QUIRK_MEM_INC_X = 0x02;
    static constexpr uint8_t QUIRK_MEM_KEEP_I = 0x04;
    static constexpr uint8_t QUIRK_SHIFT = 0x08;
    static constexpr uint8_t QUIRK_JUMP = 0x10;
    static constexpr uint8_t QUIRK_WRAP = 0x20;
    static constexpr uint8_t QUIRK_VBLANK = 0x40;
    static constexpr uint8_t DISPLAY_DXY0_MASK = 0x03;
    static constexpr uint8_t DISPLAY_DXY0_8X16 = 0x01;
    static constexpr uint8_t DISPLAY_DXY0_16X16 = 0x02;
    static constexpr uint8_t DISPLAY_SC11_COLLISION = 0x04;
    static constexpr uint8_t DISPLAY_HALF_SCROLL = 0x08;
    static constexpr uint8_t DISPLAY_MODE_CLEAR = 0x10;
    static constexpr uint8_t DISPLAY_HP48_LORES = 0x20;

    uint8_t variant{};  // 0 = not specified
    uint8_t ipfCode{};  // 0 = no hint
    std::optional<uint8_t> quirks;
    std::optional<uint8_t> display;

    // The header of a program loaded at the given address, without one the load address of
    // the variant the header names is used
    static std::optional<Ch8Attributes> parse(std::span<const uint8_t> program, std::optional<uint32_t> loadAddress = {});
    static uint32_t loadAddressOfVariant(uint8_t variant);
    // The Cadmium preset for the variant, empty if not specified or unknown
    static std::string_view presetOfVariant(uint8_t variant);
    // The decoded speed hint, nothing for no hint and the reserved codes 0x01-0x0F
    std::optional<uint64_t> instructionsPerFrame() const;

    // The configuration for the program: the preset of the variant or the given base properties,
    // with the speed hint and the quirks on top where the core has them
    Properties properties(const Properties& base) const;
    // A short description for the log, like "variant 04 (xo-chip), IPF 2000"
    std::string toString() const;
};

}  // namespace emu
