//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/font.hpp
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
#include <array>
#include <cstdint>
#include <vector>

namespace hp48 {

    constexpr uint8_t MAX_FONT_HEIGHT = 10;

    struct Glyph {
        uint8_t width;
        std::array<uint8_t, MAX_FONT_HEIGHT> rows;
    };

    struct Font {
        int height;
        int firstChar;
        std::vector<Glyph> glyphs;

        const Glyph* getGlyph(uint8_t c) const {
            if (c < firstChar || c >= firstChar + glyphs.size()) {
                return &glyphs[0];
            }
            return &glyphs[c - firstChar];
        }
    };

    extern const Font* getFont(int size);

} // hp48
