//---------------------------------------------------------------------------------------
// src/emulation/stylemanager.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2023, Steffen Schümann <s.schuemann@pobox.com>
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

#include <algorithm>
#include <string>
#include <vector>

enum class Style {
    BORDER_COLOR_NORMAL,
    BASE_COLOR_NORMAL,
    TEXT_COLOR_NORMAL,
    BORDER_COLOR_FOCUSED,
    BASE_COLOR_FOCUSED,
    DEFAULT_TEXT_COLOR_FOCUSED,
    BORDER_COLOR_PRESSED,
    BASE_COLOR_PRESSED,
    TEXT_COLOR_PRESSED,
    BORDER_COLOR_DISABLED,
    BASE_COLOR_DISABLED,
    TEXT_COLOR_DISABLED,
    TEXT_SIZE,
    TEXT_SPACING,
    LINE_COLOR,
    BACKGROUND_COLOR
};

struct Color;

class StyleManager
{
public:
    struct Entry { int ctrl; int prop; uint32_t val; };
    class Scope
    {
    public:
        Scope() = default;
        ~Scope();
        void setStyle(Style style, int value);
        void setStyle(Style style, const Color& color);
        int getStyle(Style style) const;
    private:
        std::vector<Entry> styles;
    };

    StyleManager();
    ~StyleManager();

    void setDefaultTheme() const;
    void setTheme(size_t idx) const;

private:
    struct StyleSet {
        std::string name;
        std::vector<Entry> styles;
    };
    std::vector<StyleSet> _styleSets;
    static StyleManager* _instance;
};

