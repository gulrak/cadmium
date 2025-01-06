//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
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
#include <rlguipp/rlguipp.hpp>
#include <stylemanager.hpp>

class RlGuippApp
{
public:
    RlGuippApp(int width, int height, int minWidth = 640, int minHeight = 400)
        : MIN_WIDTH(minWidth), MIN_HEIGHT(minHeight)
        , _width(width), _height(height)
        , _scale(2)
    {
        InitWindow(_width * _scale, _height * _scale, "rlGui++ Test");
        SetMouseScale(1.0f/_scale, 1.0f/_scale);
        SetTargetFPS(60);
        _renderTexture = LoadRenderTexture(_width, _height);
        SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);
        _styleManager.setTheme(0);
    }
    ~RlGuippApp()
    {
        UnloadRenderTexture(_renderTexture);
        CloseWindow();
    }

    static void updateAndDrawFrame(void* self)
    {
        static_cast<RlGuippApp*>(self)->updateAndDraw();
    }

    void updateAndDraw()
    {
        updateResolution();

        BeginTextureMode(_renderTexture);
        drawGui();
        EndTextureMode();

        BeginDrawing();
        {
            ClearBackground(_backgroundColor);
#ifdef RESIZABLE_GUI
            Vector2 guiOffset = {(GetScreenWidth() - _screenWidth*screenScale)/2.0f, (GetScreenHeight() - _screenHeight*screenScale)/2.0f};
            if(guiOffset.x < 0) guiOffset.x = 0;
            if(guiOffset.y < 0) guiOffset.y = 0;
            if (_scaleBy2) {
                drawScreen({_screenOverlay.x * 2, _screenOverlay.y * 2, _screenOverlay.width * 2, _screenOverlay.height * 2}, _screenScale);
                DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){0, 0, (float)_renderTexture.texture.width * 2, (float)_renderTexture.texture.height * 2},
                               (Vector2){0, 0}, 0.0f, WHITE);
            }
            else {
                drawScreen(_screenOverlay, _screenScale);
                DrawTextureRec(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Vector2){0, 0}, WHITE);
            }
            //DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){guiOffset.x, guiOffset.y, (float)_renderTexture.texture.width * screenScale, (float)_renderTexture.texture.height * screenScale},
            //               (Vector2){0, 0}, 0.0f, WHITE);
#else
            //drawScreen({_screenOverlay.x * _scaleMode, _screenOverlay.y * _scaleMode, _screenOverlay.width * _scaleMode, _screenOverlay.height * _scaleMode}, _screenScale);
            DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){0, 0, (float)_renderTexture.texture.width * _scale, (float)_renderTexture.texture.height * _scale},
                           (Vector2){0, 0}, 0.0f, WHITE);
#endif
#if 0
            int width{0}, height{0};
#if defined(PLATFORM_WEB)
            double devicePixelRatio = emscripten_get_device_pixel_ratio();
            width = GetScreenWidth() * devicePixelRatio;
            height = GetScreenHeight() * devicePixelRatio;
#else
            glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
#endif
            //TraceLog(LOG_INFO, "Window resized: %dx%d, fb: %dx%d", GetScreenWidth(), GetScreenHeight(), width, height);
            DrawText(TextFormat("Window resized: %dx%d, fb: %dx%d, rzc: %d", GetScreenWidth(), GetScreenHeight(), width, height, resizeCount), 10,30,10,GREEN);
#endif
            // DrawText(TextFormat("Res: %dx%d", GetMonitorWidth(GetCurrentMonitor()), GetMonitorHeight(GetCurrentMonitor())), 10, 30, 10, GREEN);
            // DrawFPS(10,45);
        }
        EndDrawing();
    }
    
    void updateResolution()
    {
#ifdef RESIZABLE_GUI
#if 1
        static int resizeCount = 0;
        if (IsWindowResized()) {
            int width{0}, height{0};
            resizeCount++;
#if defined(PLATFORM_WEB)
            double devicePixelRatio = emscripten_get_device_pixel_ratio();
            width = GetScreenWidth() * devicePixelRatio;
            height = GetScreenHeight() * devicePixelRatio;
#else
            // TODO: glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
#endif
            TraceLog(LOG_INFO, "Window resized: %dx%d, fb: %dx%d", GetScreenWidth(), GetScreenHeight(), width, height);
        }
#endif
        auto screenScale = std::min(std::clamp(int(GetScreenWidth() / _screenWidth), 1, 8), std::clamp(int(GetScreenHeight() / _screenHeight), 1, 8));
        SetMouseScale(1.0f/screenScale, 1.0f/screenScale);
#else
        if (!_scale || GetMonitorWidth(GetCurrentMonitor()) <= _width * _scale) {
            _scale = 1;
        }
        if (GetScreenWidth() != _width * _scale) {
            SetWindowSize(_width * _scale, _height * _scale);
            //CenterWindow(_screenWidth * 2, _screenHeight * 2);
            SetMouseScale(1.0f/_scale, 1.0f/_scale);
        }
#endif

#ifdef RESIZABLE_GUI
        auto width = std::max(GetScreenWidth(), _width) / (_scaleBy2 ? 2 : 1);
        auto height = std::max(GetScreenHeight(), _height) / (_scaleBy2 ? 2 : 1);

        if(GetScreenWidth() < width || GetScreenHeight() < height)
            SetWindowSize(width, height);
        if(width != _width || height != _height) {
            UnloadRenderTexture(_renderTexture);
            _width = width;
            _height = height;
            _renderTexture = LoadRenderTexture(_width, _height);
            SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);
        }
#else
        if(_height < MIN_HEIGHT ||_width < MIN_WIDTH) {
            UnloadRenderTexture(_renderTexture);
            _width = MIN_WIDTH;
            _height = MIN_HEIGHT;
            _renderTexture = LoadRenderTexture(_width, _height);
            SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);
            SetWindowSize(_width * _scale, _height * _scale);
        }
#endif
    }

    bool windowShouldClose() const
    {
        return _shouldClose || WindowShouldClose();
    }

    Vector2 guiScaling() const { return {static_cast<float>(_scale), static_cast<float>(_scale)}; }

    static bool iconButton(int iconId, bool isPressed = false, Color color = {3, 127, 161}, Color foreground = {0x51, 0xbf, 0xd3, 0xff})
    {
        using namespace gui;
        StyleManager::Scope guard;
        auto fg = guard.getStyle(Style::TEXT_COLOR_NORMAL);
        auto bg = guard.getStyle(Style::BASE_COLOR_NORMAL);
        if(isPressed) {
            guard.setStyle(Style::BASE_COLOR_NORMAL, fg);
            guard.setStyle(Style::TEXT_COLOR_NORMAL, bg);
        }
        //guard.setStyle(Style::TEXT_COLOR_NORMAL, foreground);
        SetNextWidth(20);
        auto result = Button(GuiIconText(iconId, ""));
        return result;
    }

    void drawGui()
    {
        using namespace gui;
        ClearBackground(GetColor(GetStyle(DEFAULT, BACKGROUND_COLOR)));
        BeginGui({}, &_renderTexture, {0, 0}, guiScaling());
        {
            SetStyle(STATUSBAR, TEXT_PADDING, 4);
            SetStyle(LISTVIEW, SCROLLBAR_WIDTH, 6);
            SetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING, 0);
            SetStyle(SPINNER, TEXT_PADDING, 4);
            SetRowHeight(16);
            SetSpacing(0);

            StatusBar({{0.3f, "Status"}, {0.7f, "Bar"}});

            SetSpacing(0);
            BeginColumns();
            {
                auto start = GetContentAvailable().x;
                auto width = GetContentAvailable().width;
                SetSpacing(0);
                SetRowHeight(20);
                if (iconButton(ICON_BURGER_MENU, false)) {}
                if (iconButton(ICON_ROM, false)) {}
                Space(GetContentAvailable().width - 20);
                auto r = GetLastWidgetRect();
                DrawRectangleRec(r, StyleManager::getStyleColor(Style::BASE_COLOR_NORMAL));
                if (iconButton(ICON_HIDPI, _scale != 1))
                    _scale = _scale >= 3 ? 1 : _scale + 1;
                SetTooltip("TOGGLE ZOOM    ");
            }
            EndColumns();

            Begin();
            BeginPanel("Library / Research");
            {
                SetSpacing(5);
                TextBox(_queryLine, 4096);
                auto area = GetContentAvailable();
                BeginColumns();
                {
                    auto tagsWidth = area.width / 3 - 5;
                    auto listWidth = area.width - tagsWidth - 5;
                    SetSpacing(0);
                    SetNextWidth(tagsWidth);
                    SetNextHeight(area.height - 135);
#if 0
                    BeginPanel("Panel1");
                    Label("Some label");
                    Label("Another one");
                    EndPanel();
                    SetNextWidth(tagsWidth);
                    BeginPanel("Panel2");
                    Label("Some label");
                    EndPanel();
#else
#if 0
                    static Vector2 scrollPos{};
                    BeginScrollPanel(area.height - 135, {0,0,100,1000}, &scrollPos);
                    EndScrollPanel();
                    BeginScrollPanel(area.height - 135, {0,0,100,1000}, &scrollPos);
                    EndScrollPanel();
#else
                    BeginTableView(area.height - 135, 2, &_tagsScrollPos);
                    TableNextRow(22);
                    TableNextColumn(64);
                    Label("Table1");
                    EndTableView();
                    SetNextWidth(tagsWidth);
                    BeginTableView(area.height - 135, 2, &_tagsScrollPos);
                    TableNextRow(22);
                    TableNextColumn(64);
                    Label("Table2");
                    EndTableView();
#endif
#endif
                }
                EndColumns();
                TextBox(_queryLine, 4096);
            }
            //Space(_screenHeight - GetCurrentPos().y - 20 - 1);
            EndPanel();
            End();
        }
        EndGui();
    }

private:
    const int MIN_WIDTH = 640;
    const int MIN_HEIGHT = 400;
    int _width{};
    int _height{};
    std::string _queryLine;
    Vector2 _tagsScrollPos{};
    RenderTexture _renderTexture;
    Color _backgroundColor{BLACK};
    gui::StyleManager _styleManager{};
    bool _shouldClose{false};
    int _scale{1};
};

int main() {
    RlGuippApp app(640, 480);
    while (!app.windowShouldClose()) {
        app.updateAndDraw();
    }
    return 0;
}