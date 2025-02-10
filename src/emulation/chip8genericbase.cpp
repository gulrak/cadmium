//---------------------------------------------------------------------------------------
// src/emulation/chip8genericbase.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2024, Steffen Schümann <s.schuemann@pobox.com>
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

#include <emulation/chip8genericbase.hpp>

namespace emu {

static uint8_t g_chip8VipFont[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x60, 0x20, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0xA0, 0xA0, 0xF0, 0x20, 0x20,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x10, 0x10, 0x10,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xF0, 0x50, 0x70, 0x50, 0xF0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xF0, 0x50, 0x50, 0x50, 0xF0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80   // F
};

static uint8_t  g_chip8EtiFont[] = {
    0xE0, 0xA0, 0xA0, 0xA0, 0xE0,  // 0
    0x20, 0x20, 0x20, 0x20, 0x20,  // 1
    0xE0, 0x20, 0xE0, 0x80, 0xE0,  // 2
    0xE0, 0x20, 0xE0, 0x20, 0xE0,  // 3
    0xA0, 0xA0, 0xE0, 0x20, 0x20,  // 4
    0xE0, 0x80, 0xE0, 0x20, 0xE0,  // 5
    0xE0, 0x80, 0xE0, 0xA0, 0xE0,  // 6
    0xE0, 0x20, 0x20, 0x20, 0x20,  // 7
    0xE0, 0xA0, 0xE0, 0xA0, 0xE0,  // 8
    0xE0, 0xA0, 0xE0, 0x20, 0xE0,  // 9
    0xE0, 0xA0, 0xE0, 0xA0, 0xA0,  // A
    0x80, 0x80, 0xE0, 0xA0, 0xE0,  // B
    0xE0, 0x80, 0x80, 0x80, 0xE0,  // C
    0x20, 0x20, 0xE0, 0xA0, 0xE0,  // D
    0xE0, 0x80, 0xE0, 0x80, 0xE0,  // E
    0xE0, 0x80, 0xC0, 0x80, 0x80,  // F
};

static uint8_t g_chip8DreamFont[] =   {
    0xE0, 0xA0, 0xA0, 0xA0, 0xE0,  // 0
    0x40, 0x40, 0x40, 0x40, 0x40,  // 1
    0xE0, 0x20, 0xE0, 0x80, 0xE0,  // 2
    0xE0, 0x20, 0xE0, 0x20, 0xE0,  // 3
    0x80, 0xA0, 0xA0, 0xE0, 0x20,  // 4
    0xE0, 0x80, 0xE0, 0x20, 0xE0,  // 5
    0xE0, 0x80, 0xE0, 0xA0, 0xE0,  // 6
    0xE0, 0x20, 0x20, 0x20, 0x20,  // 7
    0xE0, 0xA0, 0xE0, 0xA0, 0xE0,  // 8
    0xE0, 0xA0, 0xE0, 0x20, 0xE0,  // 9
    0xE0, 0xA0, 0xE0, 0xA0, 0xA0,  // A
    0xC0, 0xA0, 0xE0, 0xA0, 0xC0,  // B
    0xE0, 0x80, 0x80, 0x80, 0xE0,  // C
    0xC0, 0xA0, 0xA0, 0xA0, 0xC0,  // D
    0xE0, 0x80, 0xE0, 0x80, 0xE0,  // E
    0xE0, 0x80, 0xC0, 0x80, 0x80,  // F
};

static uint8_t g_chip48Font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
    0x20, 0x60, 0x20, 0x20, 0x70,  // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
    0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
    0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
    0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
    0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
    0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
};

static uint8_t g_fishNChipFont[] = {
    0x60, 0xA0, 0xA0, 0xA0, 0xC0,  // 0
    0x40, 0xC0, 0x40, 0x40, 0xE0,  // 1
    0xC0, 0x20, 0x40, 0x80, 0xE0,  // 2
    0xC0, 0x20, 0x40, 0x20, 0xC0,  // 3
    0x20, 0xA0, 0xE0, 0x20, 0x20,  // 4
    0xE0, 0x80, 0xC0, 0x20, 0xC0,  // 5
    0x40, 0x80, 0xC0, 0xA0, 0x40,  // 6
    0xE0, 0x20, 0x60, 0x40, 0x40,  // 7
    0x40, 0xA0, 0x40, 0xA0, 0x40,  // 8
    0x40, 0xA0, 0x60, 0x20, 0x40,  // 9
    0x40, 0xA0, 0xE0, 0xA0, 0xA0,  // A
    0xC0, 0xA0, 0xC0, 0xA0, 0xC0,  // B
    0x60, 0x80, 0x80, 0x80, 0x60,  // C
    0xC0, 0xA0, 0xA0, 0xA0, 0xC0,  // D
    0xE0, 0x80, 0xC0, 0x80, 0xE0,  // E
    0xE0, 0x80, 0xC0, 0x80, 0x80   // F
};

static uint8_t g_akouz1Font[] = {
    0x60, 0x90, 0x90, 0x90, 0x60, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xE0, 0x10, 0x60, 0x80, 0xF0, // 2
    0xE0, 0x10, 0xE0, 0x10, 0xE0, // 3
    0x30, 0x50, 0x90, 0xF0, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xE0, // 5
    0x70, 0x80, 0xF0, 0x90, 0x60, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0x60, 0x90, 0x60, 0x90, 0x60, // 8
    0x60, 0x90, 0x70, 0x10, 0x60, // 9
    0x60, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0x70, 0x80, 0x80, 0x80, 0x70, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xE0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xE0, 0x80, 0x80, // F
};

static uint8_t g_ship10BigFont[] = {
    0x3C, 0x7E, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0x7E, 0x3C, // big 0
    0x18, 0x38, 0x58, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, // big 1
    0x3E, 0x7F, 0xC3, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFF, 0xFF, // big 2
    0x3C, 0x7E, 0xC3, 0x03, 0x0E, 0x0E, 0x03, 0xC3, 0x7E, 0x3C, // big 3
    0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0xFF, 0xFF, 0x06, 0x06, // big 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFE, 0x03, 0xC3, 0x7E, 0x3C, // big 5
    0x3E, 0x7C, 0xE0, 0xC0, 0xFC, 0xFE, 0xC3, 0xC3, 0x7E, 0x3C, // big 6
    0xFF, 0xFF, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x60, // big 7
    0x3C, 0x7E, 0xC3, 0xC3, 0x7E, 0x7E, 0xC3, 0xC3, 0x7E, 0x3C, // big 8
    0x3C, 0x7E, 0xC3, 0xC3, 0x7F, 0x3F, 0x03, 0x03, 0x3E, 0x7C  // big 9
};

static uint8_t g_ship11BigFont[] = {
    0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C, // big 0
    0x18, 0x38, 0x58, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, // big 1
    0x3E, 0x7F, 0xC3, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFF, 0xFF, // big 2
    0x3C, 0x7E, 0xC3, 0x03, 0x0E, 0x0E, 0x03, 0xC3, 0x7E, 0x3C, // big 3
    0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0xFF, 0xFF, 0x06, 0x06, // big 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFE, 0x03, 0xC3, 0x7E, 0x3C, // big 5
    0x3E, 0x7C, 0xE0, 0xC0, 0xFC, 0xFE, 0xC3, 0xC3, 0x7E, 0x3C, // big 6
    0xFF, 0xFF, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x60, // big 7
    0x3C, 0x7E, 0xC3, 0xC3, 0x7E, 0x7E, 0xC3, 0xC3, 0x7E, 0x3C, // big 8
    0x3C, 0x7E, 0xC3, 0xC3, 0x7F, 0x3F, 0x03, 0x03, 0x3E, 0x7C  // big 9
};

static uint8_t g_fishNChipBigFont[] = {
    0x7c, 0xc6, 0xce, 0xde, 0xd6, 0xf6, 0xe6, 0xc6, 0x7c, 0x00, // big 0
    0x10, 0x30, 0xf0, 0x30, 0x30, 0x30, 0x30, 0x30, 0xfc, 0x00, // big 1
    0x78, 0xcc, 0xcc, 0xc,  0x18, 0x30, 0x60, 0xcc, 0xfc, 0x00, // big 2
    0x78, 0xcc, 0x0c, 0x0c, 0x38, 0x0c, 0x0c, 0xcc, 0x78, 0x00, // big 3
    0x0c, 0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x0c, 0x1e, 0x00, // big 4
    0xfc, 0xc0, 0xc0, 0xc0, 0xf8, 0x0c, 0x0c, 0xcc, 0x78, 0x00, // big 5
    0x38, 0x60, 0xc0, 0xc0, 0xf8, 0xcc, 0xcc, 0xcc, 0x78, 0x00, // big 6
    0xfe, 0xc6, 0xc6, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x00, // big 7
    0x78, 0xcc, 0xcc, 0xec, 0x78, 0xdc, 0xcc, 0xcc, 0x78, 0x00, // big 8
    0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x18, 0x18, 0x30, 0x70, 0x00, // big 9
    0x30, 0x78, 0xcc, 0xcc, 0xcc, 0xfc, 0xcc, 0xcc, 0xcc, 0x00, // big A
    0xfc, 0x66, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x66, 0xfc, 0x00, // big B
    0x3c, 0x66, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x66, 0x3c, 0x00, // big C
    0xf8, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00, // big D
    0xfe, 0x62, 0x60, 0x64, 0x7c, 0x64, 0x60, 0x62, 0xfe, 0x00, // big E
    0xfe, 0x66, 0x62, 0x64, 0x7c, 0x64, 0x60, 0x60, 0xf0, 0x00  // big F
};

static uint8_t g_megachip8BigFont[] = {
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x18, 0x38, 0x58, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, // big 1
    0x3e, 0x7f, 0xc3, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xff, 0xff, // big 2
    0x3c, 0x7e, 0xc3, 0x03, 0x0e, 0x0e, 0x03, 0xc3, 0x7e, 0x3c, // big 3
    0x06, 0x0e, 0x1e, 0x36, 0x66, 0xc6, 0xff, 0xff, 0x06, 0x06, // big 4
    0xff, 0xff, 0xc0, 0xc0, 0xfc, 0xfe, 0x03, 0xc3, 0x7e, 0x3c, // big 5
    0x3e, 0x7c, 0xc0, 0xc0, 0xfc, 0xfe, 0xc3, 0xc3, 0x7e, 0x3c, // big 6
    0xff, 0xff, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x60, 0x60, // big 7
    0x3c, 0x7e, 0xc3, 0xc3, 0x7e, 0x7e, 0xc3, 0xc3, 0x7e, 0x3c, // big 8
    0x3c, 0x7e, 0xc3, 0xc3, 0x7f, 0x3f, 0x03, 0x03, 0x3e, 0x7c, // big 9
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c, // big 0
    0x3c, 0x7e, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0x7e, 0x3c  // big 0
};


static uint8_t g_octoBigFont[] = {
    0xFF, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, // 0
    0x18, 0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF, // 1
    0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // 2
    0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 3
    0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0x03, 0x03, // 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 5
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 6
    0xFF, 0xFF, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x18, // 7
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, // 8
    0xFF, 0xFF, 0xC3, 0xC3, 0xFF, 0xFF, 0x03, 0x03, 0xFF, 0xFF, // 9
    0x7E, 0xFF, 0xC3, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, 0xC3, // A
    0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, 0xC3, 0xC3, 0xFC, 0xFC, // B
    0x3C, 0xFF, 0xC3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC3, 0xFF, 0x3C, // C
    0xFC, 0xFE, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xFE, 0xFC, // D
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, // E
    0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0  // F
};

// With the kind permission of @Madster (EmuDev Discord)
static uint8_t g_auchipBigFont[] = {
    0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C, // 0
    0x18, 0x78, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0xFF, 0xFF, // 1
    0x7E, 0xFF, 0xC3, 0x03, 0x07, 0x1E, 0x78, 0xE0, 0xFF, 0xFF, // 2
    0x7E, 0xFF, 0xC3, 0x03, 0x0E, 0x0E, 0x03, 0xC3, 0xFF, 0x7E, // 3
    0xC3, 0xC3, 0xC3, 0xC3, 0xFF, 0x7F, 0x03, 0x03, 0x03, 0x03, // 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFE, 0x7F, 0x03, 0x03, 0xFF, 0xFE, // 5
    0x7F, 0xFF, 0xC0, 0xC0, 0xFE, 0xFF, 0xC3, 0xC3, 0xFF, 0x7E, // 6
    0xFF, 0xFF, 0x03, 0x03, 0x07, 0x0E, 0x1C, 0x18, 0x18, 0x18, // 7
    0x7E, 0xFF, 0xC3, 0xC3, 0x7E, 0x7E, 0xC3, 0xC3, 0xFF, 0x7E, // 8
    0x7E, 0xFF, 0xC3, 0xC3, 0xFF, 0x7F, 0x03, 0x07, 0x7E, 0x7C, // 9
    0x18, 0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xFF, 0xFF, 0xC3, 0xC3, // A
    0xFE, 0xFF, 0xC3, 0xC3, 0xFE, 0xFE, 0xC3, 0xC3, 0xFF, 0xFE, // B
    0x3F, 0x7F, 0xE0, 0xC0, 0xC0, 0xC0, 0xC0, 0xE0, 0x7F, 0x3F, // C
    0xFC, 0xFE, 0xC7, 0xC3, 0xC3, 0xC3, 0xC3, 0xC7, 0xFE, 0xFC, // D
    0x7F, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xFF, 0x7F, // E
    0x7F, 0xFF, 0xC0, 0xC0, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0   // F
};

static uint8_t g_akouz1BigFont[] = {
    0x7E, 0xC7, 0xC7, 0xCB, 0xCB, 0xD3, 0xD3, 0xE3, 0xE3, 0x7E, // 0
    0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, // 1
    0x7E, 0xC3, 0x03, 0x03, 0x0E, 0x18, 0x30, 0x60, 0xC0, 0xFF, // 2
    0x7E, 0xC3, 0x03, 0x03, 0x1E, 0x03, 0x03, 0x03, 0xC3, 0x7E, // 3
    0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0xC6, 0xFF, 0x06, 0x06, // 4
    0xFF, 0xC0, 0xC0, 0xC0, 0xFE, 0x03, 0x03, 0x03, 0xC3, 0x7E, // 5
    0x7E, 0xC3, 0xC0, 0xC0, 0xFE, 0xC3, 0xC3, 0xC3, 0xC3, 0x7E, // 6
    0xFF, 0x03, 0x03, 0x03, 0x06, 0x0C, 0x18, 0x18, 0x18, 0x18, // 7
    0x7E, 0xC3, 0xC3, 0xC3, 0x7E, 0xC3, 0xC3, 0xC3, 0xC3, 0x7E, // 8
    0x7E, 0xC3, 0xC3, 0xC3, 0x7F, 0x03, 0x03, 0x03, 0xC3, 0x7E, // 9
    0x7E, 0xC3, 0xC3, 0xC3, 0xFF, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, // A
    0xFE, 0xC3, 0xC3, 0xC3, 0xFE, 0xC3, 0xC3, 0xC3, 0xC3, 0xFE, // B
    0x7E, 0xC3, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC3, 0x7E, // C
    0xFC, 0xC6, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC3, 0xC6, 0xFC, // D
    0xFF, 0xC0, 0xC0, 0xC0, 0xFE, 0xC0, 0xC0, 0xC0, 0xC0, 0xFF, // E
    0xFF, 0xC0, 0xC0, 0xC0, 0xFE, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, // F
};

std::pair<const uint8_t*, size_t> Chip8GenericBase::smallFontData(Chip8Font font)
{
    switch(font) {
        case C8F5_CHIP48: return {g_chip48Font, sizeof(g_chip48Font)};
        case C8F5_ETI: return {g_chip8EtiFont, sizeof(g_chip8EtiFont)};
        case C8F5_DREAM: return {g_chip8DreamFont, sizeof(g_chip8DreamFont)};
        case C8F5_FISHNCHIPS: return { g_fishNChipFont, sizeof(g_fishNChipFont)};
        case C8F5_AKOUZ1: return {g_akouz1BigFont, sizeof(g_akouz1BigFont)};
        default: return {g_chip8VipFont, sizeof(g_chip8VipFont)};
    }
}

std::pair<const uint8_t*, size_t> Chip8GenericBase::bigFontData(Chip8BigFont font)
{
    switch(font) {
        case C8F10_SCHIP10: return {g_ship10BigFont, sizeof(g_ship10BigFont)};
        case C8F10_MEGACHIP: return {g_megachip8BigFont, sizeof(g_megachip8BigFont)};
        case C8F10_FISHNCHIPS: return {g_fishNChipBigFont, sizeof(g_fishNChipBigFont)};
        case C8F10_XOCHIP: return {g_octoBigFont, sizeof(g_octoBigFont)};
        case C8F10_AUCHIP: return {g_auchipBigFont, sizeof(g_auchipBigFont)};
        case C8F10_AKOUZ1: return {g_akouz1BigFont, sizeof(g_akouz1BigFont)};
        default: return {g_ship11BigFont, sizeof(g_ship11BigFont)};
    }
}

Chip8GenericBase::Chip8GenericBase(Chip8Variant variant, std::optional<uint64_t> clockRate)
    : _disassembler(variant)
    , _systemTime(clockRate ? *clockRate : 1000000)
{
}

std::tuple<uint16_t, uint16_t, std::string> Chip8GenericBase::disassembleInstruction(const uint8_t* code, const uint8_t* end) const
{
    return _disassembler.disassembleInstruction(code, end);
}

size_t Chip8GenericBase::disassemblyPrefixSize() const
{
    return 17;
}

std::string Chip8GenericBase::disassembleInstructionWithBytes(int32_t pc, int* bytes) const
{
    if(pc < 0) pc = _rPC;
    uint8_t code[4];
    for(size_t i = 0; i < 4; ++i) {
        code[i] = readMemoryByte(pc + i);
    }
    auto [size, opcode, instruction] = _disassembler.disassembleInstruction(code, code + 4);
    if(bytes)
        *bytes = size;
    if (size == 2)
        return fmt::format("{:04X}: {:04X}       {}", pc, (code[0] << 8)|code[1], instruction);
    return fmt::format("{:04X}: {:04X} {:04X}  {}", pc, (code[0] << 8)|code[1], (code[2] << 8)|code[3], instruction);
}
std::string Chip8GenericBase::dumpStateLine() const
{
    return fmt::format("V0:{:02x} V1:{:02x} V2:{:02x} V3:{:02x} V4:{:02x} V5:{:02x} V6:{:02x} V7:{:02x} V8:{:02x} V9:{:02x} VA:{:02x} VB:{:02x} VC:{:02x} VD:{:02x} VE:{:02x} VF:{:02x} I:{:04x} SP:{:1x} PC:{:04x} O:{:04x}", _rV[0], _rV[1], _rV[2], _rV[3],
                       _rV[4], _rV[5], _rV[6], _rV[7], _rV[8], _rV[9], _rV[10], _rV[11], _rV[12], _rV[13], _rV[14], _rV[15], _rI, _rSP, _rPC, (_memory[_rPC & (memSize() - 1)] << 8) | _memory[(_rPC + 1) & (memSize() - 1)]);
}

bool Chip8GenericBase::loadData(std::span<const uint8_t> data, std::optional<uint32_t> loadAddress)
{
    auto offset = loadAddress ? *loadAddress : 0x200;
    if(offset < _memory.size()) {
        auto size = std::min(_memory.size() - offset, data.size());
        std::memcpy(_memory.data() + offset, data.data(), size);
        return true;
    }
    return false;
}

const std::vector<std::string>& Chip8GenericBase::registerNames() const {
    static const std::vector<std::string> registerNames = {
        "V0", "V1", "V2", "V3", "V4", "V5", "V6", "V7",
        "V8", "V9", "VA", "VB", "VC", "VD", "VE", "VF",
        "I", "DT", "ST", "PC", "SP"
    };
    return registerNames;
}

GenericCpu::RegisterValue Chip8GenericBase::registerbyIndex(size_t index) const
{
    if(index < 16)
        return {_rV[index], 8};
    if(index == 16) {
        uint32_t size = _memory.size()>65535 ? 24 : 16;
        return {_rI, size};
    }
    if(index == 17)
        return {_rDT, 8};
    if(index == 18)
        return {_rST, 8};
    if(index == 19) {
        uint32_t size = _memory.size()>65535 ? 24 : 16;
        return {_rPC, size};
    }
    return {_rSP, 8};
}
void Chip8GenericBase::setRegister(size_t index, uint32_t value)
{
    if(index < 16)
        _rV[index] = static_cast<uint8_t>(value);
    else if(index == 16)
        _rI = _memory.size()>65535 ? value : _memory.size() > 4095 ? value & 0xFFFF : value & 0xFFF;
    else if(index == 17)
        _rDT = static_cast<uint8_t>(value);
    else if(index == 18)
        _rST = static_cast<uint8_t>(value);
    else if(index == 19)
        _rPC = _memory.size()>65535 ? value : _memory.size() > 4095 ? value & 0xFFFF : value & 0xFFF;
    else
        _rSP = static_cast<uint8_t>(value);
}

} // emu

