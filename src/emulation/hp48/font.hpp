//
// Created by Steffen Schümann on 17.02.26.
//
#pragma once
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

