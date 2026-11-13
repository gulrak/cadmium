//---------------------------------------------------------------------------------------
// src/highlighter.hpp
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

#include <rlguipp/rlguipp_codeeditor.hpp>

#include <array>
#include <cstdint>

// Highlights Octo source and the disassembly of the CPUs Cadmium shows. In Octo source, the body of an
// `:asm <cpu> { ... }` block is highlighted in the dialect of that CPU.
class Highlighter : public gui::SyntaxHighlighter
{
public:
    enum Dialect { eNONE, eCHIP8OCTO, eCHIP8CHIPPER, eCDP1802, eM6800, eSATURN };
    // The style ids written by highlightLine(), the index into the palette
    enum Style : uint8_t { eNORMAL, eNUMBER, eSTRING, eOPCODE, eREGISTER, eLABEL, eDIRECTIVE, eCOMMENT };
    static constexpr int NUM_STYLES = eCOMMENT + 1;
    // The colors of the styles in the dark theme, Cadmium maps them for the current theme
    static constexpr std::array<Color, NUM_STYLES> DEFAULT_PALETTE{{{200, 200, 200, 255}, {33, 210, 242, 255}, {238, 205, 51, 255}, {247, 83, 20, 255}, {219, 167, 39, 255}, {66, 176, 248, 255}, {183, 212, 247, 255}, {115, 154, 202, 255}}};
    static constexpr Color DEFAULT_SELECTION{100, 100, 120, 255};

    void setDialect(Dialect dialect) { _dialect = dialect; }
    Dialect dialect() const { return _dialect; }

    LineState nextLineState(std::string_view line, LineState state) const override;
    void highlightLine(std::string_view line, LineState state, std::span<uint8_t> styles) const override;

private:
    Dialect _dialect{eCHIP8OCTO};
};
