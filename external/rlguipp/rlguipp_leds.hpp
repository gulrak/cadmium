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
// LED style display widgets
//---------------------------------------------------------------------------------------
#pragma once

#include <rlguipp/rlguipp.hpp>

#include <cstdint>

namespace gui {

// Draw an N-digit 7-segment LED display at the current layout position.
// height     - pixel height of the display; width is derived automatically.
// value      - value to show; negative values are shown as absolute.
// numDigits  - number of digit cells.
// decimalPos - which dp dot to light, counted from the right (0 = none lit).
//              Every digit always has a dp dot slot to its right (always drawn,
//              dim or bright), so the total width never changes when decimalPos
//              changes — no digit positions ever shift.
// hexMode    - false → decimal, true → hexadecimal (0-9 A-F).
// Colors:    - lit segments/dot use TEXT_COLOR_NORMAL, dark use TEXT_COLOR_DISABLED.
RLGUIPP_API void SevenSegmentDisplay(float height, int64_t value, int numDigits, int decimalPos = 0, bool hexMode = false);

// Til311Display — TIL311-style 4*7 dot-matrix hex display.
// dotSize  - side length of each dot in pixels (integer); pitch = 2*dotSize so the
//            gap between adjacent dots equals one dot.  Natural cell height = 13*dotSize.
// decimalPos > 0  — dp slot to the right of that digit position (1=LSDigit) is lit;
//                   every digit always has its dp slot drawn so the total width is
//                   constant regardless of decimalPos.
// hexMode         — false: decimal (mod 10 per digit), true: hex 0-F per digit.
RLGUIPP_API void Til311Display(int dotSize, int64_t value, int numDigits, int decimalPos = 0, bool hexMode = false);

}  // namespace gui
