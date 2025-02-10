//---------------------------------------------------------------------------------------
// highlighter.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2025, Steffen Schümann <s.schuemann@pobox.com>
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

#include <raylib.h>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_set>

class Highlighter
{
public:
    static constexpr int COLUMN_WIDTH = 6;

protected:
    void highlightLine(const char* text, const char* end);
    void drawHighlightedTextLine(Font& font, const char* textRoot, const char* text, const char* end, Vector2 position, float width, int columnOffset, int lineHeight);

    enum { eNORMAL, eNUMBER, eSTRING, eOPCODE, eREGISTER, eLABEL, eDIRECTIVE, eCOMMENT };
    struct ColorPair {
        Color front;
        Color back;
    };
    inline static Color _defaultColors[]{{200, 200, 200, 255}, {33, 210, 242, 255}, {238, 205, 51, 255}, {247, 83, 20, 255}, {219, 167, 39, 255}, {66, 176, 248, 255}, {183, 212, 247, 255}, {115, 154, 202, 255}};
    inline static Color _defaultSelected{100, 100, 120, 255};
    inline static Color _colors[]{{200, 200, 200, 255}, {33, 210, 242, 255}, {238, 205, 51, 255}, {247, 83, 20, 255}, {219, 167, 39, 255}, {66, 176, 248, 255}, {183, 212, 247, 255}, {115, 154, 202, 255}};
    inline static Color _selected{100, 100, 120, 255};

    uint32_t _selectionStart{0};
    uint32_t _selectionEnd{0};
    std::vector<ColorPair> _highlighting;
    static std::unordered_set<std::string> _opcodes;
    static std::unordered_set<std::string> _directives;
};
