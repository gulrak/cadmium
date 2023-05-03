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

#include <stylemanager.hpp>

#include <rlguipp/rlguipp.hpp>

#define CHIP8_STYLE_PROPS_COUNT 16
static const StyleManager::Entry chip8StyleProps[CHIP8_STYLE_PROPS_COUNT] = {
    {0, 0, 0x2f7486ff},   // DEFAULT_BORDER_COLOR_NORMAL
    {0, 1, 0x024658ff},   // DEFAULT_BASE_COLOR_NORMAL
    {0, 2, 0x51bfd3ff},   // DEFAULT_TEXT_COLOR_NORMAL
    {0, 3, 0x82cde0ff},   // DEFAULT_BORDER_COLOR_FOCUSED
    {0, 4, 0x3299b4ff},   // DEFAULT_BASE_COLOR_FOCUSED
    {0, 5, 0xb6e1eaff},   // DEFAULT_TEXT_COLOR_FOCUSED
    {0, 6, 0x82cde0ff},   // DEFAULT_BORDER_COLOR_PRESSED
    {0, 7, 0x3299b4ff},   // DEFAULT_BASE_COLOR_PRESSED
    {0, 8, 0xeff8ffff},   // DEFAULT_TEXT_COLOR_PRESSED
    {0, 9, 0x134b5aff},   // DEFAULT_BORDER_COLOR_DISABLED
    {0, 10, 0x0e273aff},  // DEFAULT_BASE_COLOR_DISABLED
    {0, 11, 0x17505fff},  // DEFAULT_TEXT_COLOR_DISABLED
    {0, 16, 0x0000000e},  // DEFAULT_TEXT_SIZE
    {0, 17, 0x00000000},  // DEFAULT_TEXT_SPACING
    {0, 18, 0x81c0d0ff},  // DEFAULT_LINE_COLOR
    {0, 19, 0x00222bff},  // DEFAULT_BACKGROUND_COLOR
};

static const std::pair<int,int> styleMapping[] = {
    {DEFAULT, BORDER_COLOR_NORMAL},
    {DEFAULT, BASE_COLOR_NORMAL},
    {DEFAULT, TEXT_COLOR_NORMAL},
    {DEFAULT, BORDER_COLOR_FOCUSED},
    {DEFAULT, BASE_COLOR_FOCUSED},
    {DEFAULT, TEXT_COLOR_FOCUSED},
    {DEFAULT, BORDER_COLOR_PRESSED},
    {DEFAULT, BASE_COLOR_PRESSED},
    {DEFAULT, TEXT_COLOR_PRESSED},
    {DEFAULT, BORDER_COLOR_DISABLED},
    {DEFAULT, BASE_COLOR_DISABLED},
    {DEFAULT, TEXT_COLOR_DISABLED},
    {DEFAULT, TEXT_SIZE},
    {DEFAULT, TEXT_SPACING},
    {DEFAULT, LINE_COLOR},
    {DEFAULT, BACKGROUND_COLOR}
};


StyleManager::Scope::~Scope()
{
    for(const auto& [ctrl, prop, value] : styles) {
        gui::SetStyle(ctrl, prop, value);
    }
}
void StyleManager::Scope::setStyle(Style style, int value)
{
    const auto& pair = styleMapping[(int)style];
    const auto& control = pair.first;
    const auto& property = pair.second;
    auto iter = std::find_if(styles.begin(), styles.end(), [&](const Entry& e) { return control == e.ctrl && property == e.prop; });
    if(iter == styles.end())
        styles.push_back({control, property, (uint32_t)gui::GetStyle(control, property)});
    gui::SetStyle(control, property, value);
}

void StyleManager::Scope::setStyle(Style style, const Color& color)
{
    setStyle(style, ColorToInt(color));
}

int StyleManager::Scope::getStyle(Style style) const
{
    const auto& [control, property] = styleMapping[(int)style];
    return gui::GetStyle(control, property);
}

StyleManager* StyleManager::_instance = nullptr;

StyleManager::StyleManager()
{
    _instance = this;
    _styleSets.push_back({"default", {}});
    for (auto chip8StyleProp : chip8StyleProps) {
        _styleSets.front().styles.push_back(chip8StyleProp);
    }
}

StyleManager::~StyleManager()
{
    _instance = nullptr;
}

void StyleManager::setTheme(size_t idx) const
{
    if(idx >= _styleSets.size()) idx = 0;
    for(auto& [ctrl, prop, val] : _styleSets[idx].styles) {
        gui::SetStyle(ctrl, prop, val);
    }
}

void StyleManager::setDefaultTheme() const
{
    setTheme(0);
}
