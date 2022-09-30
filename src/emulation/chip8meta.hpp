//---------------------------------------------------------------------------------------
// src/emulation/chip8meta.hpp
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

#include <emulation/chip8variants.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <bitset>

namespace emu {

enum OpcodeType { OT_FFFF, OT_FFFn, OT_FFnn, OT_Fnnn, OT_FxyF, OT_FxFF, OT_Fxyn, OT_Fxnn};

struct OpcodeInfo {
    OpcodeType type;
    uint16_t mask;
    uint16_t opcode;
    int size;
    std::string mnemonic;
    std::string octo;
    Chip8Variant variants;
};

namespace detail {
inline static std::vector<OpcodeInfo> opcodes{
    { OT_FFFn, 0xFFF0, 0x00B0, 2, "dw #00bN", "0x00 0x0N", C8V::SCHIP_1_1_SCRUP },
    { OT_FFFn, 0xFFF0, 0x00C0, 2, "scd N", "scroll-down N", C8V::SCHIP_1_1|C8V::XO_CHIP|C8V::OCTO },
    { OT_FFFn, 0xFFF0, 0x00D0, 2, "scu N", "scroll-up N", C8V::XO_CHIP|C8V::OCTO },
    { OT_FFFF, 0xFFFF, 0x00E0, 2, "cls", "clear", C8VG_BASE },
    { OT_FFFF, 0xFFFF, 0x00EE, 2, "ret", "return", C8VG_BASE },
    { OT_FFFF, 0xFFFF, 0x00FB, 2, "scr", "scroll-right", C8V::SCHIP_1_1|C8V::XO_CHIP|C8V::OCTO },
    { OT_FFFF, 0xFFFF, 0x00FC, 2, "scl", "scroll-left", C8V::SCHIP_1_1|C8V::XO_CHIP|C8V::OCTO },
    { OT_FFFF, 0xFFFF, 0x00FD, 2, "exit", "exit", C8V::SCHIP_1_1|C8V::XO_CHIP|C8V::OCTO },
    { OT_FFFF, 0xFFFF, 0x00FE, 2, "low", "lores", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::XO_CHIP|C8V::OCTO },
    { OT_FFFF, 0xFFFF, 0x00FF, 2, "high", "hires", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::XO_CHIP|C8V::OCTO },
    { OT_FFFF, 0xFFFF, 0x02A0, 2, "dw #02A0", "cycle-background", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X},
    { OT_Fnnn, 0xF000, 0x1000, 2, "jp NNN", "jump NNN", C8VG_BASE },
    { OT_Fnnn, 0xF000, 0x2000, 2, "call NNN", "call NNN", C8VG_BASE },
    { OT_Fxnn, 0xF000, 0x3000, 2, "se vX,NN", "if vX != NN then", C8VG_BASE },
    { OT_Fxnn, 0xF000, 0x4000, 2, "sne vX,NN", "if vX == NN then", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x5000, 2, "se vX,vY", "if vX != vY then", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x5002, 2, "ld [i],vX-vY", "save vX - vY", C8V::XO_CHIP|C8V::OCTO },
    { OT_FxyF, 0xF00F, 0x5003, 2, "ld vX-vY,[i]", "load vX - vY", C8V::XO_CHIP|C8V::OCTO },
    { OT_Fxnn, 0xF000, 0x6000, 2, "ld vX,NN", "vX := NN", C8VG_BASE },
    { OT_Fxnn, 0xF000, 0x7000, 2, "add vX,NN", "vX += NN", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x8000, 2, "ld vX,vY", "vX := vY", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x8001, 2, "or vX,vY", "vX |= vY", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x8002, 2, "and vX,vY", "vX &= vY", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x8003, 2, "xor vX,vY", "vX ^= vY", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x8004, 2, "add vX,vY", "vX += vY", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x8005, 2, "sub vX,vY", "vX -= vY", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x8006, 2, "shr vX{,vY}", "vX >>= vY", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x8007, 2, "subn vX,vY", "vX =- vY", C8VG_BASE },
    { OT_FxFF, 0xF00F, 0x800e, 2, "shl vX{,vY}", "vX <<= vY", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0x9000, 2, "sne vX,vY", "if vX == vY then", C8VG_BASE },
    { OT_Fnnn, 0xF000, 0xA000, 2, "ld i,NNN", "i := NNN", C8VG_BASE },
    { OT_Fnnn, 0xF000, 0xB000, 2, "jp v0,NNN", "jump0 NNN", C8VG_BASE & ~(C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X|C8V::CHIP_48|C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::XO_CHIP) },
    { OT_Fxnn, 0xF000, 0xB000, 2, "jp vX,NNN", "jump0 NNN + vX", C8V::CHIP_48|C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::XO_CHIP },
    { OT_Fxyn, 0xF000, 0xB000, 2, "dw #bXYN", "0xbXYN", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X},
    { OT_Fxnn, 0xF000, 0xC000, 2, "rnd vX,NN", "vX := random NN", C8VG_BASE },
    { OT_FxyF, 0xF00F, 0xD000, 2, "drw vX,vY,0", "sprite vX vY 0", C8V::CHIP_48|C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::XO_CHIP },
    { OT_Fxyn, 0xF000, 0xD000, 2, "drw vX,vY,N", "sprite vX vY N", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xE09E, 2, "skp vX", "if vX -key then", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xE0A1, 2, "sknp vX", "if vX key then", C8VG_BASE },
    { OT_FFFF, 0xFFFF, 0xF000, 4, "", "i := long NNNN", C8V::XO_CHIP },
    { OT_FxFF, 0xF0FF, 0xF001, 2, "", "planes X", C8V::XO_CHIP },
    { OT_FFFF, 0xFFFF, 0xF002, 2, "", "audio", C8V::XO_CHIP },
    { OT_FxFF, 0xF0FF, 0xF007, 2, "", "vX := delay", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xF00A, 2, "", "vX := key", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xF015, 2, "", "delay := vX", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xF018, 2, "", "sound := vX", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xF01E, 2, "", "i += vX", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xF029, 2, "", "i := hex vX", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xF030, 2, "", "i := hex vX 10", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::XO_CHIP },
    { OT_FxFF, 0xF0FF, 0xF033, 2, "", "bcd vX", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xF03A, 2, "", "pitch := vX", C8V::XO_CHIP },
    { OT_FxFF, 0xF0FF, 0xF055, 2, "", "save vX", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xF065, 2, "", "load vX", C8VG_BASE },
    { OT_FxFF, 0xF0FF, 0xF075, 2, "", "saveflags vX", C8V::XO_CHIP },
    { OT_FxFF, 0xF0FF, 0xF085, 2, "", "loadflags vX", C8V::XO_CHIP },
    { OT_FxFF, 0xF0FF, 0xF0F8, 2, "", "0xfX 0xf8", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X },
    { OT_FxFF, 0xF0FF, 0xF0FB, 2, "", "0xfX 0xfb", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X }
};
} // namespace detail

} // namespace emu
