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
#include <rlguipp/rlguipp_leds.hpp>

#include <algorithm>
#include <vector>

namespace gui {

// ----------------------------------------------------------------------------
// Seven-segment LED display
// ----------------------------------------------------------------------------

// "gfedcba" encoding (bit 0 = a, bit 6 = g) for hex digits 0-F.
static const uint8_t kSegEncoding[16] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
    0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71
};

// Draw one digit cell at (x0, y0) with the given segment mask and colors.
//
// Layout (H = height, T = thickness, W = width, HH = H/2, gY = g-segment top):
//
//   +--a--+        rows 0 .. T-1
//   f     b        rows T+1 .. gY-2
//   +--g--+        rows gY .. gY+T-1
//   e     c        rows gY+T+1 .. H-T-2
//   +--d--+        rows H-T .. H-1
//
// One-pixel gap separates every pair of adjacent segment ends.
static void drawOneSegDigit(float x0, float y0, float H, uint8_t segs, Color onColor, Color offColor)
{
    int T  = std::max(1, (int)(H / 8));
    int W  = std::max(T * 4 + 2, (int)(H * 0.55f + 0.5f));
    int gY = (int)(H / 2) - T / 2;
    int bH = gY - T - 2;          // vertical half-height (top)
    int cH = (int)H - gY - 2*T - 2; // vertical half-height (bottom)

    auto draw = [&](bool on, float sx, float sy, float sw, float sh) {
        if (sw >= 1.0f && sh >= 1.0f)
            DrawRectangleRec({x0 + sx, y0 + sy, sw, sh}, on ? onColor : offColor);
    };

    float fw = (float)(W - 2*T - 2);  // horizontal segment width
    draw(segs & 0x01, (float)(T+1),   0.0f,              fw,        (float)T);   // a top
    draw(segs & 0x02, (float)(W-T),   (float)(T+1),      (float)T,  (float)bH);  // b top-right
    draw(segs & 0x04, (float)(W-T),   (float)(gY+T+1),   (float)T,  (float)cH);  // c bot-right
    draw(segs & 0x08, (float)(T+1),   (float)((int)H-T), fw,        (float)T);   // d bottom
    draw(segs & 0x10, 0.0f,           (float)(gY+T+1),   (float)T,  (float)cH);  // e bot-left
    draw(segs & 0x20, 0.0f,           (float)(T+1),      (float)T,  (float)bH);  // f top-left
    draw(segs & 0x40, (float)(T+1),   (float)gY,         fw,        (float)T);   // g middle
}

void SevenSegmentDisplay(float height, int64_t value, int numDigits, int decimalPos, bool hexMode)
{
    if (numDigits <= 0) return;

    int T       = std::max(1, (int)(height / 8));
    int W       = std::max(T * 4 + 2, (int)(height * 0.55f + 0.5f));
    int dpDot   = T;             // decimal-point dot size (square)
    int dpExtra = 1 + dpDot + 1; // fixed slot after every digit: gap + dot + gap

    // Every digit cell is W + dpExtra wide.  The dp dot is always drawn (dim or
    // bright), so the total width is constant regardless of decimalPos.
    int totalW = numDigits * (W + dpExtra);

    auto bounds = AllocateWidget({(float)totalW, height});
    float baseX = bounds.x;
    float baseY = bounds.y;

    Color onColor  = Fade(GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)),   GetAlpha());
    Color offColor = Fade(GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_DISABLED)), GetAlpha());

    int base = hexMode ? 16 : 10;
    uint64_t uval = (uint64_t)(value < 0 ? -value : value);

    // Collect digit values right-to-left (index 0 = rightmost digit).
    std::vector<int> digitVals(numDigits, 0);
    uint64_t tmp = uval;
    for (int i = 0; i < numDigits; ++i, tmp /= base)
        digitVals[i] = (int)(tmp % base);

    // Draw left-to-right; dfr = digits from right for column j.
    float x = baseX;
    for (int j = 0; j < numDigits; ++j) {
        int dfr = numDigits - 1 - j;
        drawOneSegDigit(x, baseY, height, kSegEncoding[digitVals[dfr] & 0xF], onColor, offColor);
        x += W;

        // Dp dot: always occupies its slot; lit only for the selected position.
        bool dotLit = decimalPos > 0 && dfr == decimalPos;
        DrawRectangleRec({x + 1, baseY + height - dpDot, (float)dpDot, (float)dpDot},
                         dotLit ? onColor : offColor);
        x += dpExtra;
    }
}

// TIL311-style 4×7 dot-matrix display.
//
// 20 dot positions per character, stored as bits 0-19 in kTil311Font[].
// Layout (position label → row,col):
//   Row 0 (top):    0,1,2,3           (all 4 cols)
//   Row 1:          4,  ,  ,5         (outer cols only)
//   Row 2:          6,  ,  ,7
//   Row 3 (mid):    8,9,A,B           (all 4 cols)
//   Row 4:          C,  ,  ,D
//   Row 5:          E,  ,  ,F
//   Row 6 (bot):    G,H,I,J           (all 4 cols)
// Bit n of the font word = 1 if dot position n is lit.
static const uint32_t kTil311Font[16] = {
    0x6F9F6,  // 0
    0x8A8A8,  // 1
    0xF56A7,  // 2
    0x7A6A7,  // 3
    0x8AFF1,  // 4
    0x7A75F,  // 5
    0x6F756,  // 6
    0x8A8AF,  // 7
    0x6F6F6,  // 8
    0x7AEF6,  // 9
    0x9FFF6,  // A
    0x7F7F7,  // B
    0xE515E,  // C
    0x7F9F7,  // D
    0xF575F,  // E
    0x1575F,  // F
};

// [row, col] for each of the 20 dot positions (0=top-left)
static const int8_t kTil311Pos[20][2] = {
    {0,0},{0,1},{0,2},{0,3},  // 0-3
    {1,0},{1,3},               // 4-5
    {2,0},{2,3},               // 6-7
    {3,0},{3,1},{3,2},{3,3},  // 8-11
    {4,0},{4,3},               // 12-13
    {5,0},{5,3},               // 14-15
    {6,0},{6,1},{6,2},{6,3},  // 16-19
};

void Til311Display(int dotSize, int64_t value, int numDigits, int decimalPos, bool hexMode)
{
    if (numDigits <= 0 || dotSize < 1) return;

    // Uniform integer grid: pitch = 2*dotSize (dot + equal-sized gap), no floats.
    // Cell: 4 cols × 7 rows → width = 7*dotSize, height = 13*dotSize.
    // dp sits one dotSize below the cell bottom (like the real TIL311 chip).
    // dpExtra: gap(ds) + dp(ds) + inter-digit gap(ds) = 3*dotSize, so digits
    // are never glued to the adjacent dp dot.
    const int P       = 2 * dotSize;
    const int cellW   = 3 * P + dotSize;       // = 7 * dotSize
    const int cellH   = 6 * P + dotSize;       // = 13 * dotSize
    const int dpExtra = 3 * dotSize;           // gap + dp + trailing gap
    const int totalH  = cellH + 2 * dotSize;   // cell + gap + dp below
    const int totalW  = numDigits * (cellW + dpExtra);

    auto bounds = AllocateWidget({(float)totalW, (float)totalH});
    int baseX = (int)bounds.x;
    int baseY = (int)bounds.y;

    Color onColor  = Fade(GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL)),   GetAlpha());
    Color offColor = Fade(GetColor(GuiGetStyle(DEFAULT, TEXT_COLOR_DISABLED)), GetAlpha());

    int base = hexMode ? 16 : 10;
    uint64_t uval = (uint64_t)(value < 0 ? -value : value);

    std::vector<int> digitVals(numDigits, 0);
    uint64_t tmp = uval;
    for (int i = 0; i < numDigits; ++i, tmp /= base)
        digitVals[i] = (int)(tmp % base);

    int x = baseX;
    for (int j = 0; j < numDigits; ++j) {
        int dfr  = numDigits - 1 - j;
        uint32_t bits = kTil311Font[digitVals[dfr] & 0xF];

        for (int k = 0; k < 20; ++k) {
            bool lit = (bits >> k) & 1u;
            int dotX = x + kTil311Pos[k][1] * P;
            int dotY = baseY + kTil311Pos[k][0] * P;
            DrawRectangle(dotX, dotY, dotSize, dotSize, lit ? onColor : offColor);
        }
        x += cellW;

        bool dpLit = decimalPos > 0 && dfr == decimalPos;
        DrawRectangle(x + dotSize - 1, baseY + cellH + dotSize, dotSize, dotSize,
                      dpLit ? onColor : offColor);
        x += dpExtra;
    }
}

}  // namespace gui
