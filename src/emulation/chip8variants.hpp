//---------------------------------------------------------------------------------------
// src/emulation/chip8variants.hpp
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

#include <cstdint>
#include <type_traits>

namespace emu {

enum class Chip8Variant : uint64_t {
    CHIP_8 = 0x01,                      // CHIP-8
    CHIP_8_1_2 = 0x02,                  // CHIP-8 1/2
    CHIP_8_I = 0x04,                    // CHIP-8I
    CHIP_8_II = 0x08,                   // CHIP-8 II aka. Keyboard Kontrol
    CHIP_8_III = 0x10,                  // CHIP-8III
    CHIP_8_TPD = 0x20,                  // Two-page display for CHIP-8
    CHIP_8C = 0x40,                     // CHIP-8C
    CHIP_10 = 0x80,                     // CHIP-10
    CHIP_8_SRV = 0x100,                 // CHIP-8 modification for saving and restoring variables
    CHIP_8_SRV_I = 0x200,               // Improved CHIP-8 modification for saving and restoring variables
    CHIP_8_RB = 0x400,                  // CHIP-8 modification with relative branching
    CHIP_8_ARB = 0x800,                 // Another CHIP-8 modification with relative branching
    CHIP_8_FSD = 0x1000,                // CHIP-8 modification with fast, single-dot DXYN
    CHIP_8_IOPD = 0x2000,               // CHIP-8 with I/O port driver routine
    CHIP_8_8BMD = 0x4000,               // CHIP-8 8-bit multiply and divide
    HI_RES_CHIP_8 = 0x8000,             // HI-RES CHIP-8 (four-page display)
    HI_RES_CHIP_8_IO = 0x10000,         // HI-RES CHIP-8 with I/O
    HI_RES_CHIP_8_PS = 0x20000,         // HI-RES CHIP-8 with page switching
    CHIP_8E = 0x40000,                  // CHIP-8E
    CHIP_8_IBNNN = 0x80000,             // CHIP-8 with improved BNNN
    CHIP_8_SCROLL = 0x100000,           // CHIP-8 scrolling routine
    CHIP_8X = 0x200000,                 // CHIP-8X
    CHIP_8X_TPD = 0x400000,             // Two-page display for CHIP-8X
    HI_RES_CHIP_8X = 0x800000,          // Hi-res CHIP-8X
    CHIP_8Y = 0x1000000,                // CHIP-8Y
    CHIP_8_CtS = 0x2000000,             // CHIP-8 “Copy to Screen”
    CHIP_BETA = 0x4000000,              // CHIP-BETA
    CHIP_8M = 0x8000000,                // CHIP-8M
    MULTIPLE_NIM = 0x10000000,          // Multiple Nim interpreter
    DOUBLE_ARRAY_MOD = 0x20000000,      // Double Array Modification
    CHIP_8_D6800 = 0x40000000,          // CHIP-8 for DREAM 6800 (CHIPOS)
    CHIP_8_D6800_LOP = 0x80000000,      // CHIP-8 with logical operators for DREAM 6800 (CHIPOSLO)
    CHIP_8_D6800_JOY = 0x100000000,     // CHIP-8 for DREAM 6800 with joystick
    CHIPOS_2K_D6800 = 0x200000000,      // 2K CHIPOS for DREAM 6800
    CHIP_8_ETI660 = 0x400000000,        // CHIP-8 for ETI-660
    CHIP_8_ETI660_COL = 0x800000000,    // CHIP-8 with color support for ETI-660
    CHIP_8_ETI660_HR = 0x1000000000,    // CHIP-8 for ETI-660 with high resolution
    CHIP_8_COSMAC_ELF = 0x2000000000,   // CHIP-8 for COSMAC ELF
    CHIP_8_ACE_VDU = 0x4000000000,      // CHIP-VDU / CHIP-8 for the ACE VDU
    CHIP_8_AE = 0x8000000000,           // CHIP-8 AE (ACE Extended)
    CHIP_8_DC_V2 = 0x10000000000,       // Dreamcards Extended CHIP-8 V2.0
    CHIP_8_AMIGA = 0x20000000000,       // Amiga CHIP-8 interpreter
    CHIP_48 = 0x40000000000,            // CHIP-48
    SCHIP_1_0 = 0x80000000000,          // SUPER-CHIP 1.0
    SCHIP_1_1 = 0x100000000000,         // SUPER-CHIP 1.1
    GCHIP = 0x200000000000,             // GCHIP
    SCHIPC_GCHIPC = 0x400000000000,     // SCHIP Compatibility (SCHPC) and GCHIP Compatibility (GCHPC)
    VIP2K_CHIP_8 = 0x800000000000,      // VIP2K CHIP-8
    SCHIP_1_1_SCRUP = 0x1000000000000,  // SUPER-CHIP with scroll up
    CHIP8RUN = 0x2000000000000,         // chip8run
    MEGA_CHIP = 0x4000000000000,        // Mega-Chip
    XO_CHIP = 0x8000000000000,          // XO-CHIP
    OCTO = 0x10000000000000,            // Octo
    CHIP_8_CL_COL = 0x20000000000000,   // CHIP-8 Classic / Color

    NUM_VARIANTS
};

namespace detail {
template <typename Enum>
using EnableBitmask = typename std::enable_if<std::is_same<Enum, Chip8Variant>::value, Enum>::type;
}  // namespace detail

template <typename Enum>
constexpr detail::EnableBitmask<Enum> operator&(Enum X, Enum Y)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(X) & static_cast<underlying>(Y));
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum> operator|(Enum X, Enum Y)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(X) | static_cast<underlying>(Y));
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum> operator^(Enum X, Enum Y)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(static_cast<underlying>(X) ^ static_cast<underlying>(Y));
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum> operator~(Enum X)
{
    using underlying = typename std::underlying_type<Enum>::type;
    return static_cast<Enum>(~static_cast<underlying>(X));
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum>& operator&=(Enum& X, Enum Y)
{
    X = X & Y;
    return X;
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum>& operator|=(Enum& X, Enum Y)
{
    X = X | Y;
    return X;
}

template <typename Enum>
constexpr detail::EnableBitmask<Enum>& operator^=(Enum& X, Enum Y)
{
    X = X ^ Y;
    return X;
}

using C8V = Chip8Variant;

static constexpr Chip8Variant C8VG_BASE = static_cast<Chip8Variant>(0x3FFFFFFFFFFFFF) & ~(C8V::CHIP_8_1_2 | C8V::CHIP_8C | C8V::CHIP_8_SCROLL | C8V::MULTIPLE_NIM | C8V::CHIP_8_D6800 | C8V::CHIP_8_D6800_LOP | C8V::CHIP_8_D6800_JOY | C8V::CHIPOS_2K_D6800);
static constexpr Chip8Variant C8VG_D6800 = C8V::CHIP_8_D6800 | C8V::CHIP_8_D6800_LOP | C8V::CHIP_8_D6800_JOY | C8V::CHIPOS_2K_D6800;

} // namespace emu
