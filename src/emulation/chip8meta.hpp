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

enum OpcodeType { OT_FFFF, OT_FFFn, OT_FFnn, OT_Fnnn, OT_FxyF, OT_FxFF, OT_Fxyn, OT_Fxnn, NUM_OPCODE_TYPES};

struct OpcodeInfo {
    OpcodeType type;
    uint16_t opcode;
    int size;
    std::string mnemonic;
    std::string octo;
    Chip8Variant variants;
    std::string description;
};

namespace detail {
// clang-format off
inline static std::array<uint16_t, NUM_OPCODE_TYPES> opcodeMasks = { 0xFFFF, 0xFFF0, 0xFF00, 0xF000, 0xF00F, 0xF0FF, 0xF000 };
inline static std::vector<OpcodeInfo> opcodes{
    { OT_FFFF, 0x0010, 2, "megaoff", "megaoff", C8V::MEGA_CHIP, "disable megachip mode" },
    { OT_FFFF, 0x0011, 2, "megaon", "megaon", C8V::MEGA_CHIP, "enable megachip mode" },
    { OT_FFFn, 0x00B0, 2, "dw #00bN", "scroll_up N", C8V::SCHIP_1_1_SCRUP, "scroll screen content up N hires pixel (half lores pixel)" },
    { OT_FFFn, 0x00B0, 2, "scru N", "0x00 0xbN", C8V::MEGA_CHIP, "scroll screen content up N pixel" },
    { OT_FFFn, 0x00C0, 2, "scd N", "scroll-down N", C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::MEGA_CHIP|C8V::XO_CHIP|C8V::OCTO, "scroll screen content down N hires pixel (half lores pixel)" },
    { OT_FFFn, 0x00D0, 2, "scu N", "scroll-up N", C8V::XO_CHIP|C8V::OCTO, "scroll screen content up N hires pixel (half lores pixel)" },
    { OT_FFFF, 0x00E0, 2, "cls", "clear", C8VG_BASE, "clear the screen, in megachip mode it updates the visible screen before clearing the draw buffer" },
    { OT_FFFF, 0x00EE, 2, "ret", "return", C8VG_BASE, "return from subroutine to address pulled from stack" },
    { OT_FFFF, 0x00FB, 2, "scr", "scroll-right", C8V::SCHIP_1_1|C8V::MEGA_CHIP|C8V::XO_CHIP|C8V::OCTO, "scroll screen content right one hires pixel (half lores pixel)" },
    { OT_FFFF, 0x00FC, 2, "scl", "scroll-left", C8V::SCHIP_1_1|C8V::MEGA_CHIP|C8V::XO_CHIP|C8V::OCTO, "scroll screen content left one hires pixel (half lores pixel)" },
    { OT_FFFF, 0x00FD, 2, "exit", "exit", C8V::SCHIP_1_1|C8V::XO_CHIP|C8V::MEGA_CHIP|C8V::OCTO, "exit interpreter" },
    { OT_FFFF, 0x00FE, 2, "low", "lores", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::MEGA_CHIP|C8V::XO_CHIP|C8V::OCTO, "switch to lores mode (64x32)" },
    { OT_FFFF, 0x00FF, 2, "high", "hires", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::MEGA_CHIP|C8V::XO_CHIP|C8V::OCTO, "switch to hires mode (128x64)" },
    { OT_FFnn, 0x0100, 4, "ldhi i,NNNNNN", "0x01 0xNN 0xNN 0xNN", C8V::MEGA_CHIP, "set I to NNNNNN (24 bit)" },
    { OT_FFnn, 0x0200, 2, "ldpal NN", "ldpal NN", C8V::MEGA_CHIP, "load NN colors from I into the palette, colors are in ARGB" },
    { OT_FFFF, 0x02A0, 2, "dw #02A0", "cycle-background", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "cycle background color one step between blue, black, green and red"},
    { OT_FFnn, 0x0300, 2, "sprw NN", "sprw NN", C8V::MEGA_CHIP, "set sprite width to NN (not used for font sprites)" },
    { OT_FFnn, 0x0400, 2, "sprh NN", "sprh NN", C8V::MEGA_CHIP, "set sprite height to NN (not used for font sprites)" },
    { OT_FFnn, 0x0500, 2, "alpha NN", "alpha NN", C8V::MEGA_CHIP, "set screen alpha to NN" },
    { OT_FFFn, 0x0600, 2, "digisnd N", "digisnd N", C8V::MEGA_CHIP, "play digitized sound at I N=loop/noloop" },
    { OT_FFFF, 0x0700, 2, "stopsnd", "stopsnd", C8V::MEGA_CHIP, "stop digitized sound" },
    { OT_FFFn, 0x0800, 2, "bmode N", "bmode N", C8V::MEGA_CHIP, "set sprite blend mode (0=normal,1=25%,2=50%,3=75%,4=additive,5=multiply)" },
    { OT_FFnn, 0x0900, 2, "ccol NN", "ccol NN", C8V::MEGA_CHIP, "set collision color to index NN" },
    { OT_Fnnn, 0x1000, 2, "jp NNN", "jump NNN", C8VG_BASE, "jump to address NNN" },
    { OT_Fnnn, 0x2000, 2, "call NNN", "call NNN", C8VG_BASE, "push return address onto stack and call subroutine at address NNN" },
    { OT_Fxnn, 0x3000, 2, "se vX,NN", "if vX != NN then", C8VG_BASE, "skip next opcode if vX == NN" },
    { OT_Fxnn, 0x4000, 2, "sne vX,NN", "if vX == NN then", C8VG_BASE, "skip next opcode if vX != NN" },
    { OT_FxyF, 0x5000, 2, "se vX,vY", "if vX != vY then", C8VG_BASE, "skip next opcode it vX == vY" },
    { OT_FxyF, 0x5002, 2, "ld [i],vX-vY", "save vX - vY", C8V::XO_CHIP|C8V::OCTO, "write registers vX to vY to memory pointed to by I" },
    { OT_FxyF, 0x5003, 2, "ld vX-vY,[i]", "load vX - vY", C8V::XO_CHIP|C8V::OCTO, "load registers vX to vY from memory pointed to by I" },
    { OT_Fxnn, 0x6000, 2, "ld vX,NN", "vX := NN", C8VG_BASE, "set vX to NN" },
    { OT_Fxnn, 0x7000, 2, "add vX,NN", "vX += NN", C8VG_BASE, "add NN to vX" },
    { OT_FxyF, 0x8000, 2, "ld vX,vY", "vX := vY", C8VG_BASE, "set vX to the value of vY" },
    { OT_FxyF, 0x8001, 2, "or vX,vY", "vX |= vY", C8VG_BASE, "set vX to the result of bitwise vX OR vY [Q: COSMAC based variants will reset VF]" },
    { OT_FxyF, 0x8002, 2, "and vX,vY", "vX &= vY", C8VG_BASE, "set vX to the result of bitwise vX AND vY [Q: COSMAC based variants will reset VF]" },
    { OT_FxyF, 0x8003, 2, "xor vX,vY", "vX ^= vY", C8VG_BASE & ~(C8V::CHIP_8_D6800), "set vX to the result of bitwise vX XOR vY [Q: COSMAC based variants will reset VF]" },
    { OT_FxyF, 0x8004, 2, "add vX,vY", "vX += vY", C8VG_BASE, "add vY to vX, vF is set to 1 if an overflow happened, to 0 if not, even if X=F!" },
    { OT_FxyF, 0x8005, 2, "sub vX,vY", "vX -= vY", C8VG_BASE, "subtract vY from vX, vF is set to 0 if an underflow happened, to 1 if not, even if X=F!" },
    { OT_FxyF, 0x8006, 2, "shr vX{,vY}", "vX >>= vY", C8VG_BASE & ~(C8V::CHIP_8_D6800), "set vX to vY and shift vX one bit to the right, set vF to the bit shifted out, even if X=F! [Q: CHIP-48/SCHIP dont set vX to vY, so only shift vX]" },
    { OT_FxyF, 0x8007, 2, "subn vX,vY", "vX =- vY", C8VG_BASE & ~(C8V::CHIP_8_D6800), "set vX to the result of subtracting vX from vY, vF is set to 0 if an underflow happened, to 1 if not, even if X=F!" },
    { OT_FxyF, 0x800e, 2, "shl vX{,vY}", "vX <<= vY", C8VG_BASE & ~(C8V::CHIP_8_D6800), "set vX to vY and shift vX one bit to the left, set vF to the bit shifted out, even if X=F! [Q: CHIP-48/SCHIP dont set vX to vY, so only shift vX]" },
    { OT_FxyF, 0x9000, 2, "sne vX,vY", "if vX == vY then", C8VG_BASE, "skip next opcode if vX != vY" },
    { OT_Fnnn, 0xA000, 2, "ld i,NNN", "i := NNN", C8VG_BASE, "set I to NNN" },
    { OT_Fnnn, 0xB000, 2, "jp v0,NNN", "jump0 NNN", C8VG_BASE & ~(C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X|C8V::CHIP_48|C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP), "jump to address NNN + v0" },
    { OT_Fxnn, 0xB000, 2, "jp vX,NNN", "jump0 NNN + vX", C8V::CHIP_48|C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP, "jump to address XNN + vX" },
    { OT_Fxyn, 0xB000, 2, "dw #bXYN", "0xbXYN", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "set foreground color for area" },
    { OT_Fxnn, 0xC000, 2, "rnd vX,NN", "vX := random NN", C8VG_BASE, "set vx to a random value masked (bitwise AND) with NN" },
    { OT_FxyF, 0xD000, 2, "drw vX,vY,0", "sprite vX vY 0", C8V::CHIP_48|C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::XO_CHIP, "draw 16x16 pixel sprite at position vX, vY [Q: XO-CHIP wraps pixels instead of clipping them]" },
    { OT_Fxyn, 0xD000, 2, "drw vX,vY,N", "sprite vX vY N", C8VG_BASE, "draw 8xN pixel sprite at position vX, vY [Q: XO-CHIP wraps pixels instead of clipping them] [Q: COSMAC based systems wait for vertical blank] [Q: CHIP-10 only has a hires mode]" },
    { OT_FxFF, 0xE09E, 2, "skp vX", "if vX -key then", C8VG_BASE, "skip next opcode if key in vX is pressed" },
    { OT_FxFF, 0xE0A1, 2, "sknp vX", "if vX key then", C8VG_BASE, "skip next opcode if key in vX is not pressed" },
    { OT_FFFF, 0xF000, 4, "", "i := long NNNN", C8V::XO_CHIP, "assign next 16 bit word to i, and set PC behind it" },
    { OT_FxFF, 0xF001, 2, "", "planes X", C8V::XO_CHIP, "select bit planes to draw on when drawing with Dxy0/Dxyn" },
    { OT_FFFF, 0xF002, 2, "", "audio", C8V::XO_CHIP, "load 16 bytes audio pattern pointed to by I into audio pattern buffer" },
    { OT_FxFF, 0xF007, 2, "", "vX := delay", C8VG_BASE, "set vX to the value of the delay timer" },
    { OT_FxFF, 0xF00A, 2, "", "vX := key", C8VG_BASE, "wait for a key pressed and released and set vX to it, in megachip mode it also updates the screen like clear" },
    { OT_FxFF, 0xF015, 2, "", "delay := vX", C8VG_BASE, "set delay timer to vX" },
    { OT_FxFF, 0xF018, 2, "", "sound := vX", C8VG_BASE, "set sound timer to vX, sound is played when sound timer is set greater 1 until it is zero" },
    { OT_FxFF, 0xF01E, 2, "", "i += vX", C8VG_BASE, "add vX to I" },
    { OT_FxFF, 0xF029, 2, "", "i := hex vX", C8VG_BASE, "set I to the hex sprite for the lowest nibble in vX" },
    { OT_FxFF, 0xF030, 2, "", "i := bighex vX", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::XO_CHIP|C8V::MEGA_CHIP, "set I to the 10 lines height hex sprite for the lowest nibble in vX" },
    { OT_FxFF, 0xF033, 2, "", "bcd vX", C8VG_BASE, "write the value of vX as BCD value at the addresses I, I+1 and I+2" },
    { OT_FxFF, 0xF03A, 2, "", "pitch := vX", C8V::XO_CHIP, "set audio pitch for a audio pattern playback rate of 4000*2^((vX-64)/48)Hz" },
    { OT_FxFF, 0xF055, 2, "", "save vX", C8VG_BASE, "write the content of v0 to vX at the memory pointed to by I, I is incremented by X+1 [Q: CHIP-48/SCHIP1.0 increment I only by X, SCHIP1.1 not at all]" },
    { OT_FxFF, 0xF065, 2, "", "load vX", C8VG_BASE, "read the bytes from memory pointed to by I into the registers v0 to vX, I is incremented by X+1 [Q: CHIP-48/SCHIP1.0 increment I only by X, SCHIP1.1 not at all]" },
    { OT_FxFF, 0xF075, 2, "", "saveflags vX", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::XO_CHIP|C8V::MEGA_CHIP, "store the content of the registers v0 to vX into flags storage (outside of the addressable ram)" },
    { OT_FxFF, 0xF085, 2, "", "loadflags vX", C8V::SCHIP_1_0|C8V::SCHIP_1_1|C8V::SCHIP_1_1_SCRUP|C8V::XO_CHIP|C8V::MEGA_CHIP, "load the registers v0 to vX from flags storage (outside the addressable ram)" },
    { OT_FxFF, 0xF0F8, 2, "", "0xfX 0xf8", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "output vX to io port" },
    { OT_FxFF, 0xF0FB, 2, "", "0xfX 0xfb", C8V::CHIP_8X|C8V::CHIP_8X_TPD|C8V::HI_RES_CHIP_8X, "wait for input from io and load into vX" }
};
// clang-format on
} // namespace detail

} // namespace emu
