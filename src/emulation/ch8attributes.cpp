//---------------------------------------------------------------------------------------
// src/emulation/ch8attributes.cpp
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

#include <emulation/ch8attributes.hpp>
#include <emulation/coreregistry.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <array>

namespace emu {

namespace {

constexpr uint8_t TAG_VERSION_0 = 0xB0;
constexpr size_t FIXED_SIZE = 7;  // jump, tag, "CH8" and the length

// Cadmium presets by variant ID, the original interpreters where Cadmium only emulates the hardware
constexpr std::array<std::string_view, 0x13> VARIANT_PRESETS = {
    "",               // 00 not specified
    "chip-8",         // 01
    "schip-1-1",      // 02
    "schip-modern",   // 03
    "xo-chip",        // 04
    "megachip",       // 05
    "chip-48",        // 06
    "schip-1-0",      // 07
    "eti-chip-8",     // 08
    "chip-8-x",       // 09
    "chip-8-e",       // 0A
    "chip-10",        // 0B
    "chip-8-tpd",     // 0C
    "chip-8-fpd",     // 0D
    "schipc",         // 0E
    "dream-chip-8",   // 0F
    "vip-chip-8",     // 10 hybrid, needs the 1802 of the VIP
    "eti-chip-8",     // 11 hybrid, needs the 1802 of the ETI-660
    "dream-chip-8",   // 12 hybrid, needs the 6800 of the DREAM 6800
};

// Ids of the generic core's properties the quirk fields map to
constexpr std::string_view PROP_DONT_RESET_VF = "dontResetVf";
constexpr std::string_view PROP_INC_I_BY_X_PLUS_1 = "loadStoreIncIByXPlus1";
constexpr std::string_view PROP_INC_I_BY_X = "loadStoreIncIByx";
constexpr std::string_view PROP_JUST_SHIFT_VX = "justShiftVx";
constexpr std::string_view PROP_JUMP0_BXNN = "jump0Bxnn";
constexpr std::string_view PROP_WRAP_SPRITES = "wrapSprites";
constexpr std::string_view PROP_INSTANT_DXYN = "instantDxyn";
constexpr std::string_view PROP_ALLOW_HIRES = "allowHires";
constexpr std::string_view PROP_DXY0_8X16 = "loresDxy0Is8x16";
constexpr std::string_view PROP_DXY0_16X16 = "loresDxy0Is16x16";
constexpr std::string_view PROP_SC11_COLLISION = "schip11Collision";
constexpr std::string_view PROP_HALF_PIXEL_SCROLL = "halfPixelScroll";
constexpr std::string_view PROP_MODE_CHANGE_CLEAR = "modeChangeClear";
constexpr std::string_view PROP_SC_LORES_DRAWING = "schipLoresDrawing";

void setBool(Properties& props, std::string_view id, bool value)
{
    if (auto* prop = props.find(id))
        prop->setBool(value);
}

}

std::optional<Ch8Attributes> Ch8Attributes::parse(std::span<const uint8_t> program, std::optional<uint32_t> loadAddress)
{
    if (program.size() < FIXED_SIZE || (program[0] & 0xF0) != 0x10 || program[2] != TAG_VERSION_0 || program[3] != 'C' || program[4] != 'H' || program[5] != '8')
        return {};
    const size_t length = program[6];
    if (length < 1 || program.size() < FIXED_SIZE + length)
        return {};
    const uint32_t target = ((program[0] & 0x0Fu) << 8) | program[1];
    if (target < loadAddress.value_or(loadAddressOfVariant(program[7])) + FIXED_SIZE + length)
        return {};
    Ch8Attributes result;
    result.variant = program[7];
    if (length >= 2)
        result.ipfCode = program[8];
    if (length >= 3)
        result.quirks = program[9] & 0x7F;
    if (length >= 4)
        result.display = program[10] & 0x3F;
    return result;
}

uint32_t Ch8Attributes::loadAddressOfVariant(uint8_t variant)
{
    switch (variant) {
        case 0x08:
        case 0x11:
            return 0x600;
        case 0x09:
            return 0x300;
        default:
            return 0x200;
    }
}

std::string_view Ch8Attributes::presetOfVariant(uint8_t variant)
{
    return variant < VARIANT_PRESETS.size() ? VARIANT_PRESETS[variant] : std::string_view{};
}

std::optional<uint64_t> Ch8Attributes::instructionsPerFrame() const
{
    if (ipfCode < 0x10)
        return {};
    uint64_t ipf = ipfCode >> 4;
    for (int e = ipfCode & 0x0F; e > 0; --e)
        ipf *= 10;
    return ipf;
}

Properties Ch8Attributes::properties(const Properties& base) const
{
    Properties result = base;
    if (const auto preset = presetOfVariant(variant); !preset.empty()) {
        if (auto props = CoreRegistry::propertiesForPreset(preset))
            result = props;
    }
    // Cores without these properties (the hardware emulations running the original
    // interpreters) ignore the hints, as the interpreter defines its behavior
    if (const auto ipf = instructionsPerFrame()) {
        if (auto* prop = result.find(propid::INSTRUCTIONS_PER_FRAME); prop && std::holds_alternative<Property::Integer>(prop->getValue()))
            prop->setInt(static_cast<int>(std::clamp<uint64_t>(*ipf, std::max(prop->getIntMin(), 0), prop->getIntMax())));
    }
    if (quirks) {
        setBool(result, PROP_DONT_RESET_VF, !(*quirks & QUIRK_LOGIC));
        setBool(result, PROP_INC_I_BY_X_PLUS_1, !(*quirks & (QUIRK_MEM_INC_X | QUIRK_MEM_KEEP_I)));
        setBool(result, PROP_INC_I_BY_X, *quirks & QUIRK_MEM_INC_X);
        setBool(result, PROP_JUST_SHIFT_VX, *quirks & QUIRK_SHIFT);
        setBool(result, PROP_JUMP0_BXNN, *quirks & QUIRK_JUMP);
        setBool(result, PROP_WRAP_SPRITES, *quirks & QUIRK_WRAP);
        setBool(result, PROP_INSTANT_DXYN, !(*quirks & QUIRK_VBLANK));
    }
    // The display quirks only concern variants with a SUPER-CHIP style hires mode
    if (const auto* hires = result.find(PROP_ALLOW_HIRES); display && hires && hires->getBool()) {
        if (const auto dxy0 = *display & DISPLAY_DXY0_MASK; dxy0 == DISPLAY_DXY0_8X16 || dxy0 == DISPLAY_DXY0_16X16) {
            setBool(result, PROP_DXY0_8X16, dxy0 == DISPLAY_DXY0_8X16);
            setBool(result, PROP_DXY0_16X16, dxy0 == DISPLAY_DXY0_16X16);
        }
        setBool(result, PROP_SC11_COLLISION, *display & DISPLAY_SC11_COLLISION);
        setBool(result, PROP_HALF_PIXEL_SCROLL, *display & DISPLAY_HALF_SCROLL);
        setBool(result, PROP_MODE_CHANGE_CLEAR, *display & DISPLAY_MODE_CLEAR);
        setBool(result, PROP_SC_LORES_DRAWING, *display & DISPLAY_HP48_LORES);
    }
    return result;
}

std::string Ch8Attributes::toString() const
{
    const auto preset = presetOfVariant(variant);
    auto result = fmt::format("variant {:02X} ({})", variant, variant == 0 ? "not specified" : preset.empty() ? "unknown" : preset);
    if (const auto ipf = instructionsPerFrame())
        result += fmt::format(", IPF {}", *ipf);
    if (quirks)
        result += fmt::format(", quirks {:02X}", *quirks);
    if (display)
        result += fmt::format(", display {:02X}", *display);
    return result;
}

}
