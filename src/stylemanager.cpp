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
#include <fmt/format.h>

#include <cmath>

namespace gui {
#if 0
static const uint32_t cadmiumPalette[7] = {
    0x00222bff, //E = 0!
    0x134b5aff, //D = 1!
    0x2f7486ff, //F = 2!
    0x3299b4ff, //B = 3!
    0x51bfd3ff, //G = 4!
    0x82cde0ff, //A = 5!
    0xeff8ffff  //C = 6!
};
#else
static const uint32_t cadmiumPalette[7] = {
    0x00222bff, //E = 0!
    0x134b5aff, //D = 1!
    0x2f7486ff, //F = 2!
    0x3299b4ff, //B = 3!
    0x51bfd3ff, //G = 4!
    0x85d2e6ff, //A = 5!
    0xeff8ffff  //C = 6!
};
#endif
static float cadmiumAverageHue = 200.0f;


#define CHIP8_STYLE_PROPS_COUNT 16
static const StyleManager::Entry chip8StyleProps[CHIP8_STYLE_PROPS_COUNT] = {
    {0, 0, 2},   // F! DEFAULT_BORDER_COLOR_NORMAL
    {0, 1, 1},   // D! DEFAULT_BASE_COLOR_NORMAL
    {0, 2, 4},   // G! DEFAULT_TEXT_COLOR_NORMAL
    {0, 3, 5},   // A! DEFAULT_BORDER_COLOR_FOCUSED
    {0, 4, 3},   // B! DEFAULT_BASE_COLOR_FOCUSED
    {0, 5, 6},   // C! DEFAULT_TEXT_COLOR_FOCUSED
    {0, 6, 5},   // A! DEFAULT_BORDER_COLOR_PRESSED
    {0, 7, 3},   // B! DEFAULT_BASE_COLOR_PRESSED
    {0, 8, 6},   // C! DEFAULT_TEXT_COLOR_PRESSED
    {0, 9, 1},   // D! DEFAULT_BORDER_COLOR_DISABLED
    {0, 10, 0},  // E! DEFAULT_BASE_COLOR_DISABLED
    {0, 11, 1},  // D! DEFAULT_TEXT_COLOR_DISABLED
    {0, 18, 5},  // A! DEFAULT_LINE_COLOR
    {0, 19, 0},  // E! DEFAULT_BACKGROUND_COLOR
    {0, 16, 0x00000008},  // DEFAULT_TEXT_SIZE
    {0, 17, 0x00000000},  // DEFAULT_TEXT_SPACING
};

#if 0
static const StyleManager::Entry chip8StyleProps[CHIP8_STYLE_PROPS_COUNT] = {
    {0, 0, 0x2f7486ff},   // F! DEFAULT_BORDER_COLOR_NORMAL
    //{0, 1, 0x024658ff},   // DEFAULT_BASE_COLOR_NORMAL
      {0, 1, 0x134b5aff},   // D! DEFAULT_BASE_COLOR_NORMAL
    {0, 2, 0x51bfd3ff},   // G! DEFAULT_TEXT_COLOR_NORMAL
      {0, 3, 0x82cde0ff},   // A! DEFAULT_BORDER_COLOR_FOCUSED
      {0, 4, 0x3299b4ff},   // B! DEFAULT_BASE_COLOR_FOCUSED
    //{0, 5, 0xb6e1eaff},   // DEFAULT_TEXT_COLOR_FOCUSED
      {0, 5, 0xeff8ffff},   // C! DEFAULT_TEXT_COLOR_FOCUSED
      {0, 6, 0x82cde0ff},   // A! DEFAULT_BORDER_COLOR_PRESSED
      {0, 7, 0x3299b4ff},   // B! DEFAULT_BASE_COLOR_PRESSED
      {0, 8, 0xeff8ffff},   // C! DEFAULT_TEXT_COLOR_PRESSED
    //{0, 9, 0x134b5aff},   // DEFAULT_BORDER_COLOR_DISABLED
      {0, 9, 0x134b5aff},   // D! DEFAULT_BORDER_COLOR_DISABLED
      {0, 10, 0x00222bff},  // E! DEFAULT_BASE_COLOR_DISABLED
    //{0, 11, 0x17505fff},  // DEFAULT_TEXT_COLOR_DISABLED
      {0, 11, 0x134b5aff},  // D! DEFAULT_TEXT_COLOR_DISABLED
      {0, 18, 0x82cde0ff},  // A! DEFAULT_LINE_COLOR
      {0, 19, 0x00222bff},  // E! DEFAULT_BACKGROUND_COLOR
    {0, 16, 0x00000008},  // DEFAULT_TEXT_SIZE
    {0, 17, 0x00000000},  // DEFAULT_TEXT_SPACING
};

static const StyleManager::Entry chip8DarkStyleProps[] = {
    /*
    { 0, 0, 0x878787ff },    // DEFAULT_BORDER_COLOR_NORMAL
    //{ 0, 1, 0x2c2c2cff },    // DEFAULT_BASE_COLOR_NORMAL
    { 0, 1, 0x3c3c40ff },    // DEFAULT_BASE_COLOR_NORMAL
    { 0, 2, 0xc3c3d0ff },    // DEFAULT_TEXT_COLOR_NORMAL
    { 0, 3, 0xe1e1f0ff },    // DEFAULT_BORDER_COLOR_FOCUSED
    { 0, 4, 0x84848cff },    // DEFAULT_BASE_COLOR_FOCUSED
    { 0, 5, 0x18181cff },    // DEFAULT_TEXT_COLOR_FOCUSED
    { 0, 6, 0x000000ff },    // DEFAULT_BORDER_COLOR_PRESSED
    { 0, 7, 0xefeffeff },    // DEFAULT_BASE_COLOR_PRESSED
    { 0, 8, 0x202024ff },    // DEFAULT_TEXT_COLOR_PRESSED
    { 0, 9, 0x6a6a70ff },    // DEFAULT_BORDER_COLOR_DISABLED
    { 0, 10, 0x818188ff },    // DEFAULT_BASE_COLOR_DISABLED
    { 0, 11, 0x606066ff },    // DEFAULT_TEXT_COLOR_DISABLED
    { 0, 16, 0x0000000e },    // DEFAULT_TEXT_SIZE
    { 0, 17, 0x00000000 },    // DEFAULT_TEXT_SPACING
    { 0, 18, 0x9d9da4ff },    // DEFAULT_LINE_COLOR
    //{ 0, 19, 0x3c3c3cff },    // DEFAULT_BACKGROUND_COLOR
    { 0, 19, 0x2c2c30ff },    // DEFAULT_BACKGROUND_COLOR

    { 0, 20, 0x00000018 },    // DEFAULT_TEXT_LINE_SPACING
    { 1, 5, 0xf7f7ffff },    // LABEL_TEXT_COLOR_FOCUSED
    { 1, 8, 0x898990ff },    // LABEL_TEXT_COLOR_PRESSED
    { 4, 5, 0xb0b0bbff },    // SLIDER_TEXT_COLOR_FOCUSED
    { 5, 5, 0x84848cff },    // PROGRESSBAR_TEXT_COLOR_FOCUSED
    { 9, 5, 0xf5f5ffff },    // TEXTBOX_TEXT_COLOR_FOCUSED
    { 10, 5, 0xf6f6ffff },    // VALUEBOX_TEXT_COLOR_FOCUSED
    */
    { 0, 0, 0x787c86ff },    // DEFAULT_BORDER_COLOR_NORMAL
    //{ 0, 1, 0x27282bff },    // DEFAULT_BASE_COLOR_NORMAL
    { 0, 1, 0x36383cff },    // DEFAULT_BASE_COLOR_NORMAL
    { 0, 2, 0xafb4c3ff },    // DEFAULT_TEXT_COLOR_NORMAL
    { 0, 3, 0xc9cfe0ff },    // DEFAULT_BORDER_COLOR_FOCUSED
    { 0, 4, 0x7c808aff },    // DEFAULT_BASE_COLOR_FOCUSED
    { 0, 5, 0x151617ff },    // DEFAULT_TEXT_COLOR_FOCUSED
    { 0, 6, 0x000000ff },    // DEFAULT_BORDER_COLOR_PRESSED
    { 0, 7, 0xd8deefff },    // DEFAULT_BASE_COLOR_PRESSED
    { 0, 8, 0x1e1f21ff },    // DEFAULT_TEXT_COLOR_PRESSED
    { 0, 9, 0x60636bff },    // DEFAULT_BORDER_COLOR_DISABLED
    { 0, 10, 0x757882ff },    // DEFAULT_BASE_COLOR_DISABLED
    { 0, 11, 0x55585eff },    // DEFAULT_TEXT_COLOR_DISABLED
    { 0, 16, 0x00000010 },    // DEFAULT_TEXT_SIZE
    { 0, 17, 0x00000000 },    // DEFAULT_TEXT_SPACING
    { 0, 18, 0x8e929dff },    // DEFAULT_LINE_COLOR
    //{ 0, 19, 0x36383cff },    // DEFAULT_BACKGROUND_COLOR
    { 0, 19, 0x1c1d1eff },    // DEFAULT_BACKGROUND_COLOR
    { 0, 20, 0x00000018 },    // DEFAULT_TEXT_LINE_SPACING

    { 9, 5, 0xf5f5ffff }    // TEXTBOX_TEXT_COLOR_FOCUSED
};

static const StyleManager::Entry chip8BluishStyleProps[] = {
    { 0, 0, 0x5ca6a6ff },    // DEFAULT_BORDER_COLOR_NORMAL
    { 0, 1, 0xb4e8f3ff },    // DEFAULT_BASE_COLOR_NORMAL
    { 0, 2, 0x447e77ff },    // DEFAULT_TEXT_COLOR_NORMAL
    { 0, 3, 0x5f8792ff },    // DEFAULT_BORDER_COLOR_FOCUSED
    { 0, 4, 0xcdeff7ff },    // DEFAULT_BASE_COLOR_FOCUSED
    { 0, 5, 0x4c6c74ff },    // DEFAULT_TEXT_COLOR_FOCUSED
    { 0, 6, 0x3b5b5fff },    // DEFAULT_BORDER_COLOR_PRESSED
    { 0, 7, 0xeaffffff },    // DEFAULT_BASE_COLOR_PRESSED
    { 0, 8, 0x275057ff },    // DEFAULT_TEXT_COLOR_PRESSED
    { 0, 9, 0x96aaacff },    // DEFAULT_BORDER_COLOR_DISABLED
    { 0, 10, 0xc8d7d9ff },    // DEFAULT_BASE_COLOR_DISABLED
    { 0, 11, 0x8c9c9eff },    // DEFAULT_TEXT_COLOR_DISABLED
    { 0, 16, 0x0000000e },    // DEFAULT_TEXT_SIZE
    { 0, 17, 0x00000000 },    // DEFAULT_TEXT_SPACING
    { 0, 18, 0x84adb7ff },    // DEFAULT_LINE_COLOR
    { 0, 19, 0xe8eef1ff },    // DEFAULT_BACKGROUND_COLOR
};
#endif

/*
 * {0, 0, 0x2f7486ff},   // DEFAULT_BORDER_COLOR_NORMAL
//{0, 1, 0x024658ff},   // DEFAULT_BASE_COLOR_NORMAL
  {0, 1, 0x134b5aff},   // D! DEFAULT_BASE_COLOR_NORMAL
{0, 2, 0x51bfd3ff},   // DEFAULT_TEXT_COLOR_NORMAL
  {0, 3, 0x82cde0ff},   // A! DEFAULT_BORDER_COLOR_FOCUSED
  {0, 4, 0x3299b4ff},   // B! DEFAULT_BASE_COLOR_FOCUSED
//{0, 5, 0xb6e1eaff},   // DEFAULT_TEXT_COLOR_FOCUSED
  {0, 5, 0xeff8ffff},   // C! DEFAULT_TEXT_COLOR_FOCUSED
  {0, 6, 0x82cde0ff},   // A! DEFAULT_BORDER_COLOR_PRESSED
  {0, 7, 0x3299b4ff},   // B! DEFAULT_BASE_COLOR_PRESSED
  {0, 8, 0xeff8ffff},   // C! DEFAULT_TEXT_COLOR_PRESSED
//{0, 9, 0x134b5aff},   // DEFAULT_BORDER_COLOR_DISABLED
  {0, 9, 0x134b5aff},   // D! DEFAULT_BORDER_COLOR_DISABLED
{0, 10, 0x0e273aff},  // DEFAULT_BASE_COLOR_DISABLED
//{0, 11, 0x17505fff},  // DEFAULT_TEXT_COLOR_DISABLED
  {0, 11, 0x134b5aff},  // D! DEFAULT_TEXT_COLOR_DISABLED
  {0, 18, 0x82cde0ff},  // A! DEFAULT_LINE_COLOR
{0, 19, 0x00222bff},  // DEFAULT_BACKGROUND_COLOR
 */

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
    {DEFAULT, LINE_COLOR},
    {DEFAULT, BACKGROUND_COLOR},
    {DEFAULT, TEXT_SIZE},
    {DEFAULT, TEXT_SPACING},

    {DEFAULT, TEXT_LINE_SPACING},
    {LABEL, TEXT_COLOR_FOCUSED},
    {LABEL, TEXT_COLOR_PRESSED},
    {SLIDER, TEXT_COLOR_FOCUSED},
    {PROGRESSBAR, TEXT_COLOR_FOCUSED},
    {TEXTBOX, TEXT_COLOR_FOCUSED},
    {VALUEBOX, TEXT_COLOR_FOCUSED}
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

static inline float diff(float a1, float a2)
{
    auto a = a1 - a2;
    if(a > 180) a -= 360;
    if(a < -180) a += 360;
    return a;
}

static inline uint32_t tintedColor(uint32_t color, float hue, float sat, bool invert)
{
    auto col = GetColor(color);
    auto hsv = gui::HsvFromColor(col);
    auto hueDelta = diff(hsv.x, cadmiumAverageHue);
    hue += hueDelta;
    if(hue >= 360) hue -= 360;
    if(hue < 0) hue += 360;
    hsv.x = hue;
    hsv.y *= sat / 100.0f;
    if (invert)
        hsv.z = 1.0f - hsv.z;
    return ColorToInt(gui::ColorFromHsv(hsv));
}

StyleManager::StyleManager()
{
    _instance = this;
    _styleSets.push_back({"default", {}});
    double y_part = 0, x_part = 0;
    int count = 0;
    for(auto color : cadmiumPalette) {
        auto hsv = gui::HsvFromColor(GetColor(color));
        x_part += cos (hsv.x * M_PI / 180);
        y_part += sin (hsv.x * M_PI / 180);
        count++;
        _styleSets.front().palette.push_back(color);
    }
    cadmiumAverageHue = static_cast<float>(std::atan2 (y_part / count, x_part / count) * 180 / M_PI);
    /*
    int idx = 0;
    for (auto chip8StyleProp : chip8StyleProps) {
        if(idx < (int)Style::COLOR_END) {
            chip8StyleProp.val = cadmiumPalette[chip8StyleProp.val];
        }
        _styleSets.front().styles.push_back(chip8StyleProp);
        idx++;
    }
     */
}

StyleManager::~StyleManager()
{
    _instance = nullptr;
}

void StyleManager::addTheme(const std::string& name, float hue, float sat, bool invert)
{
    _styleSets.push_back({name, invert, {}});
    for(auto color : cadmiumPalette) {
        /*
        auto col = GetColor(color);
        auto hsv = gui::HsvFromColor(col);
        hsv.x = hue;
        hsv.y = sat;
        if(invert)
            hsv.z = 1.0f - hsv.z;
        _styleSets.back().palette.push_back(ColorToInt(gui::ColorFromHsv(hsv)));
         */
        _styleSets.back().palette.push_back(tintedColor(color, hue, sat, invert));
    }
}


void StyleManager::updateStyle(uint16_t hue, uint8_t sat, bool invert)
{
    int idx = 0;
    _guiHue = hue;
    _guiSaturation = sat;
    for(auto& color : _currentStyle.palette) {
        /*
        color = cadmiumPalette[idx];
        auto col = GetColor(color);
        auto hsv = gui::HsvFromColor(col);
        hsv.x = hue;
        hsv.y *= sat / 100.0f;
        if (invert)
            hsv.z = 1.0f - hsv.z;
        color = ColorToInt(gui::ColorFromHsv(hsv));
         */
        color = tintedColor(cadmiumPalette[idx], hue, sat, invert);
        idx++;
    }
    idx = 0;
    for(auto [ctrl, prop, val] : chip8StyleProps) {
        if(idx < (int)Style::COLOR_END) {
            val = _currentStyle.palette[val];
            gui::SetStyle(ctrl, prop, val);
        }
        idx++;
    }
}

void StyleManager::setTheme(size_t themeIndex)
{
    if(themeIndex >= _styleSets.size()) themeIndex = 0;
    _currentStyle = _styleSets[themeIndex];
    int idx = 0;
    for(auto [ctrl, prop, val] : chip8StyleProps) {
        if(idx < (int)Style::COLOR_END) {
            val = _styleSets[themeIndex].palette[val];
        }
        gui::SetStyle(ctrl, prop, val);
        idx++;
    }
}

void StyleManager::setDefaultTheme()
{
    setTheme(0);
}

Color StyleManager::getStyleColor(Style style)
{
    const auto& [control, property] = styleMapping[(int)style];
    return GetColor(gui::GetStyle(control, property));
}

Color StyleManager::mappedColor(const Color& col)
{
    using namespace gui;
    if(_instance && _instance->_currentStyle.isInverted) {
        auto hsv = gui::HsvFromColor(col);
        if(hsv.z > 0.9f && hsv.y > 0.9f) {
            hsv.y = 1.0f;
            hsv.z = 0.7f;
        }
        else {
            hsv.y = 1.0f;
            hsv.z = 1.0f - hsv.z;
        }
        return ColorFromHsv(hsv);
    }
    return col;
}

void StyleManager::renderAppearanceEditor()
{
    using namespace gui;
    Space(4);
    Begin();
    SetSpacing(2);
    SetIndent(90);
    SetNextWidth(150);
    Spinner("UI-Tint ", &_guiHue, 0, 360);
    SetNextWidth(150);
    Spinner("UI-Saturation ", &_guiSaturation, 0, 100);
    SetIndent(26);
    StyleManager::Scope guard;
    auto pos = GetCurrentPos();
    static Vector3 hsv{};
    Color col;
    //SetNextWidth(52.0f + 16*18);
    Label("UI Colors ");
    int xoffset = 64;
    updateStyle(_guiHue, _guiSaturation, false);
    for (int i = 0; i < 7; ++i) {
        DrawRectangleRec({pos.x + xoffset + i * 18, pos.y, 16, 16}, GetColor(guard.getStyle(/*hover ? Style::BORDER_COLOR_FOCUSED :*/ Style::BORDER_COLOR_NORMAL)));
        DrawRectangleRec({pos.x + xoffset + i * 18 + 1, pos.y + 1, 14, 14}, GetColor(guard.getStyle(/*hover ? Style::BORDER_COLOR_FOCUSED :*/ Style::BACKGROUND_COLOR)));
        col = GetColor(_currentStyle.palette[i]);
        DrawRectangle(pos.x + xoffset + i * 18 + 2, pos.y + 2 , 12, 12,col);
        bool hover =  CheckCollisionPointRec(GetMousePosition(), {pos.x + xoffset + i * 18, pos.y, 16, 16});
        if(hover) {
            hsv = gui::HsvFromColor(col);
        }
        //if(!GuiIsLocked() && IsMouseButtonReleased(0) && hover) {
        /*_selectedColor = &_colorPalette[i];
        _previousColor = _colorPalette[i];
        _colorText = fmt::format("{:06x}", _colorPalette[i]>>8);
        _colorSelectOpen = true;
         */
        //}
    }
    Label(fmt::format("H:{}, S:{}, V:{}", hsv.x, hsv.y, hsv.z).c_str());
    SetNextWidth(120);
    if(Button("Reset to Default")) {
        updateStyle(200, 80, false); // 192,90?
    }
    End();
}

}
