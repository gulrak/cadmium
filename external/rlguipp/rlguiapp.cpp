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
#include <rlguipp/rlguiapp.hpp>
#include <rlguipp/rlguipp_font.hpp>
#include <vector>
#include <cstdlib>

namespace gui {

Application::Application(const char* title, Config cfg)
    : _cfg(cfg)
    , _scale(cfg.scale)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | cfg.windowFlags);
    InitWindow(_cfg.logicalWidth * _scale, _cfg.logicalHeight * _scale, title);
    SetMouseScale(1.0f / _scale, 1.0f / _scale);
    SetTargetFPS(_cfg.targetFps);
    _renderTexture = LoadRenderTexture(_cfg.logicalWidth, _cfg.logicalHeight);
    SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);
    generateFont();
}

Application::~Application()
{
    UnloadFont(_font);
    UnloadRenderTexture(_renderTexture);
    UnloadGui();
    CloseWindow();
}

void Application::exportFrame(const char* path)
{
    Image img = LoadImageFromTexture(_renderTexture.texture);
    ImageFlipVertical(&img);
    ExportImage(img, path);
    UnloadImage(img);
}

bool Application::shouldClose() const
{
    return _shouldClose || WindowShouldClose();
}

void Application::update()
{
    adjustScale();

    BeginTextureMode(_renderTexture);
    ClearBackground(GetColor(GetStyle(DEFAULT, BACKGROUND_COLOR)));
    BeginGui({}, &_renderTexture, {0, 0}, {(float)_scale, (float)_scale});
    drawGui();
    EndGui();
    EndTextureMode();


    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(
        _renderTexture.texture,
        {0, 0, (float)_cfg.logicalWidth, -(float)_cfg.logicalHeight},
        {0, 0, (float)_cfg.logicalWidth * _scale, (float)_cfg.logicalHeight * _scale},
        {0, 0}, 0.0f, WHITE);
    EndDrawing();
}

void Application::adjustScale()
{
    int monW = GetMonitorWidth(GetCurrentMonitor());
    int monH = GetMonitorHeight(GetCurrentMonitor());
    int newScale = _cfg.scale;
    while (newScale > 1 && (_cfg.logicalWidth * newScale > monW || _cfg.logicalHeight * newScale > monH))
        --newScale;
    if (newScale != _scale) {
        _scale = newScale;
        SetWindowSize(_cfg.logicalWidth * _scale, _cfg.logicalHeight * _scale);
        SetMouseScale(1.0f / _scale, 1.0f / _scale);
    }
}

void Application::generateFont()
{
    constexpr int IMAGE_W = 256, IMAGE_H = 256;
    Image image = GenImageColor(IMAGE_W, IMAGE_H, {0, 0, 0, 0});
    std::vector<Rectangle> recs;
    std::vector<GlyphInfo> glyphs;
    float glyphX = 0, glyphY = 0;
    for (int i = 0; i < fontRomCount; ++i) {
        const auto& fci = fontRom[i];
        if (glyphX + fci.width + 1 > IMAGE_W) { glyphX = 0; glyphY += 8; }
        drawFontChar(image, fci.codepoint, (int)glyphX, (int)glyphY, WHITE);
        recs.push_back({glyphX, glyphY, fci.width + 1.0f, 8.0f});
        glyphs.push_back(GlyphInfo{fci.codepoint, 0, 0, fci.width + 1});
        glyphX += fci.width + 1;
    }
    _font.baseSize   = 8;
    _font.glyphCount = (int)glyphs.size();
    _font.texture    = LoadTextureFromImage(image);
    _font.recs       = static_cast<Rectangle*>(std::calloc(_font.glyphCount, sizeof(Rectangle)));
    _font.glyphs     = static_cast<GlyphInfo*>(std::calloc(_font.glyphCount, sizeof(GlyphInfo)));
    std::ranges::copy(recs,   _font.recs);
    std::ranges::copy(glyphs, _font.glyphs);
    UnloadImage(image);
    GuiSetFont(_font);
    // Render at the font's native pixel size — any non-integer TEXT_SIZE would
    // scale the glyph texture fractionally and make the bitmap font look blurry.
    GuiSetStyle(DEFAULT, TEXT_SIZE,    _font.baseSize);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 0);
}

} // namespace gui
