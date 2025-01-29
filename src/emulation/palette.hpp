//---------------------------------------------------------------------------------------
// src/emulation/palette.hpp
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
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace emu {
class Palette
{
public:
    struct Color {
        Color() = default;
        Color(const Color& other) = default;
        Color(const uint8_t rval, const uint8_t gval, const uint8_t bval) : r(rval), g(gval), b(bval) {}
        explicit Color(uint32_t val) : r(val >> 16), g(val >> 8), b(val) {}
        explicit Color(std::string_view hex)
        {
            if(hex.length()>1 && hex[0] == '#') {
                uint32_t colInt = std::strtoul(hex.data() + 1, nullptr, 16);
                r = colInt>>16;
                g = colInt>>8;
                b = colInt;
            }
            else {
                r = 0, g = 0, b = 0;
            }
        }
        uint32_t toRGBInt() const
        {
            return (r << 16u) | (g << 8u) | b;
        }
        uint32_t toRGBAInt(const uint8_t alpha = 255) const
        {
            return (r << 24u) | (g << 16u) | (b << 8) | alpha;
        }
        bool operator==(const Color& other) const
        {
            return r == other.r && g == other.g && b == other.b;
        }
        std::string toString() const;
        static Color fromRGBA(const uint32_t val) { return Color( val >> 24, val >> 16, val >> 8); }
        uint8_t r, g, b;
    };
    Palette() = default;
    Palette(const std::initializer_list<Color> cols, const std::initializer_list<Color> backgroundCols = {})
        : colors(cols)
        , backgroundColors(backgroundCols)
    {}
    Palette(const std::initializer_list<std::string> cols, const std::initializer_list<std::string> backgroundCols = {})
    {
        colors.reserve(cols.size());
        for(const auto& col : cols) {
            colors.emplace_back(col);
        }
        backgroundColors.reserve(backgroundCols.size());
        for(const auto& col : backgroundCols) {
            backgroundColors.emplace_back(col);
        }
    }

    bool empty() const { return colors.empty(); }
    size_t size() const { return colors.size(); }
    bool operator==(const Palette& other) const
    {
        return colors == other.colors && borderColor == other.borderColor && signalColor == other.signalColor;
    }
    std::vector<Color> colors;
    std::optional<Color> borderColor{};
    std::optional<Color> signalColor{};
    std::vector<Color> backgroundColors;
};

}
