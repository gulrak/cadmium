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

#include <format>

using namespace gui;

struct FontCharInfo {
    uint16_t codepoint;
    uint8_t data[5];
};
static FontCharInfo fontRom[] = {
    {32,0,0,0,0,0},
    {33,0,0,95,0,0},
    {34,0,7,0,7,0},
    {35,20,62,20,62,20},
    {36,36,42,127,42,18},
    {37,35,19,8,100,98},
    {38,54,73,85,34,80},
    {39,0,0,11,7,0},
    {40,0,28,34,65,0},
    {41,0,65,34,28,0},
    {42,42,28,127,28,42},
    {43,8,8,62,8,8},
    {44,0,0,176,112,0},
    {45,8,8,8,8,8},
    {46,0,96,96,0,0},
    {47,32,16,8,4,2},
    {48,62,65,65,62,0},
    {49,0,2,127,0,0},
    {50,98,81,73,73,70},
    {51,65,65,73,77,51},
    {52,15,8,8,127,8},
    {53,71,69,69,69,57},
    {54,60,74,73,73,48},
    {55,97,17,9,5,3},
    {56,54,73,73,73,54},
    {57,6,73,73,41,30},
    {58,0,54,54,0,0},
    {59,0,182,118,0,0},
    {60,8,20,34,65,0},
    {61,20,20,20,20,20},
    {62,0,65,34,20,8},
    {63,2,1,81,9,6},
    {64,62,65,93,85,94},
    {65,126,9,9,9,126},
    {66,127,73,73,73,54},
    {67,62,65,65,65,34},
    {68,127,65,65,65,62},
    {69,127,73,73,73,65},
    {70,127,9,9,9,1},
    {71,62,65,73,73,122},
    {72,127,8,8,8,127},
    {73,0,65,127,65,0},
    {74,32,64,64,64,63},
    {75,127,8,20,34,65},
    {76,127,64,64,64,64},
    {77,127,2,12,2,127},
    {78,127,2,4,8,127},
    {79,62,65,65,65,62},
    {80,127,9,9,9,6},
    {81,62,65,81,33,94},
    {82,127,9,25,41,70},
    {83,38,73,73,73,50},
    {84,1,1,127,1,1},
    {85,63,64,64,64,63},
    {86,31,32,64,32,31},
    {87,127,32,24,32,127},
    {88,99,20,8,20,99},
    {89,7,8,112,8,7},
    {90,97,81,73,69,67},
    {91,0,127,65,65,0},
    {92,2,4,8,16,32},
    {93,0,65,65,127,0},
    {94,4,2,1,2,4},
    {95,128,128,128,128,128},
    {96,0,7,11,0,0},
    {97,112,84,84,120,64},
    {98,64,127,68,68,60},
    {99,0,56,68,68,72},
    {100,56,68,68,127,64},
    {101,0,56,84,84,72},
    {102,0,8,124,10,2},
    {103,0,140,146,146,126},
    {104,0,127,4,4,120},
    {105,0,0,122,0,0},
    {106,0,64,128,116,0},
    {107,0,126,16,40,68},
    {108,0,2,126,64,0},
    {109,124,4,124,4,120},
    {110,0,124,4,4,120},
    {111,0,56,68,68,56},
    {112,0,252,36,36,24},
    {113,24,36,36,252,128},
    {114,0,124,8,4,4},
    {115,0,72,84,84,36},
    {116,0,4,62,68,32},
    {117,60,64,64,124,64},
    {118,12,48,64,48,12},
    {119,60,64,48,64,60},
    {120,68,36,56,72,68},
    {121,0,28,32,160,252},
    {122,64,100,84,76,4},
    {123,0,8,54,65,65},
    {124,0,0,119,0,0},
    {125,0,65,65,54,8},
    {126,2,1,2,2,1},
    {127,85,42,85,42,85},
    {160,0,0,0,0,0},
    {161,0,0,125,0,0},
    {162,56,68,254,68,40},
    {163,72,126,73,73,66},
    {164,93,34,34,34,93},
    {165,41,42,124,42,41},
    {166,0,0,119,0,0},
    {167,74,85,85,85,41},
    {168,0,3,0,3,0},
    {169,62,73,85,85,62},
    {170,92,85,85,94,80},
    {171,16,40,84,40,68},
    {172,8,8,8,8,56},
    {173,0,8,8,8,0},
    {174,62,93,77,89,62},
    {175,1,1,1,1,1},
    {176,6,9,9,6,0},
    {177,68,68,95,68,68},
    {178,9,12,10,9,0},
    {179,17,21,23,9,0},
    {180,0,4,2,1,0},
    {181,252,64,64,60,64},
    {182,6,127,1,127,1},
    {183,0,24,24,0,0},
    {184,0,128,128,64,0},
    {185,2,31,0,0,0},
    {186,38,41,41,38,0},
    {187,68,40,84,40,16},
    {188,34,23,104,244,66},
    {189,34,23,168,212,162},
    {190,41,19,109,244,66},
    {191,32,64,69,72,48},
    {192,120,21,22,20,120},
    {193,120,20,22,21,120},
    {194,120,22,21,22,120},
    {195,122,21,22,22,121},
    {196,120,21,20,21,120},
    {197,122,21,21,21,122},
    {198,126,9,127,73,73},
    {199,30,161,225,33,18},
    {200,124,85,86,84,68},
    {201,124,84,86,85,68},
    {202,124,86,85,86,68},
    {203,124,85,84,85,68},
    {204,0,68,125,70,0},
    {205,0,70,125,68,0},
    {206,0,70,125,70,0},
    {207,0,68,125,70,0},
    {208,8,127,73,65,62},
    {209,126,9,18,34,125},
    {210,56,69,70,68,56},
    {211,56,68,70,69,56},
    {212,56,70,69,70,56},
    {213,58,69,70,70,57},
    {214,56,69,68,69,56},
    {215,0,40,16,40,0},
    {216,94,33,93,66,61},
    {217,60,65,66,64,60},
    {218,60,64,66,65,60},
    {219,60,66,65,66,60},
    {220,60,65,64,65,60},
    {222,12,16,98,17,12},
    {222,127,20,20,20,8},
    {223,126,1,73,78,48},
    {224,112,85,86,120,64},
    {225,112,86,85,120,64},
    {226,112,86,85,122,64},
    {227,114,85,86,122,65},
    {228,112,85,84,121,64},
    {229,114,85,85,122,64},
    {230,116,84,124,84,88},
    {231,0,28,162,98,36},
    {232,0,56,85,86,72},
    {233,0,56,86,85,72},
    {234,0,58,85,86,72},
    {235,0,57,84,84,73},
    {236,0,1,122,0,0},
    {237,0,0,122,1,0},
    {238,0,2,121,2,0},
    {239,0,1,120,1,0},
    {240,53,73,74,77,56},
    {241,2,125,6,6,121},
    {242,0,56,69,70,56},
    {243,0,56,70,69,56},
    {244,0,58,69,70,56},
    {245,2,57,70,70,57},
    {246,0,57,68,68,57},
    {247,8,8,42,8,8},
    {248,0,120,116,76,60},
    {249,60,65,66,124,64},
    {250,60,66,65,124,64},
    {251,62,65,66,124,64},
    {252,61,64,64,125,64},
    {253,0,28,34,161,252},
    {254,254,40,68,68,56},
    {255,0,29,32,160,253},
    {7680,30,69,165,69,30},
    {8230,64,0,64,0,64},
    {8250,0,0,34,20,8},
    {8984,119,73,62,73,119},
    {10003,32,64,32,16,8},
    {10240,0,0,0,0,0},
    {10495,85,85,0,85,85},
    {57376,0,0,0,0,0},
    {57377,0,0,92,0,0},
    {57378,0,12,0,12,0},
    {57379,40,124,40,124,40},
    {57380,72,84,124,84,36},
    {57381,76,44,16,104,100},
    {57382,40,84,88,32,80},
    {57383,0,0,44,28,0},
    {57384,0,56,68,0,0},
    {57385,0,0,68,56,0},
    {57386,40,16,124,16,40},
    {57387,16,16,124,16,16},
    {57388,0,0,176,112,0},
    {57389,16,16,16,16,16},
    {57390,0,96,96,0,0},
    {57391,64,32,16,8,4},
    {57392,56,68,68,56,0},
    {57393,0,8,124,0,0},
    {57394,100,84,84,84,72},
    {57395,68,68,84,92,36},
    {57396,28,16,16,124,16},
    {57397,92,84,84,84,36},
    {57398,56,84,84,84,32},
    {57399,4,4,100,20,12},
    {57400,40,84,84,84,40},
    {57401,8,84,84,84,56},
    {57402,0,108,108,0,0},
    {57403,0,168,104,0,0},
    {57404,8,20,34,0,0},
    {57405,40,40,40,40,40},
    {57406,0,0,68,40,16},
    {57407,0,4,84,20,12},
    {57408,56,68,92,84,88},
    {57409,120,20,20,20,120},
    {57410,124,84,84,84,40},
    {57411,56,68,68,68,40},
    {57412,124,68,68,68,56},
    {57413,124,84,84,84,68},
    {57414,124,20,20,20,4},
    {57415,56,68,84,84,116},
    {57416,124,16,16,16,124},
    {57417,0,68,124,68,0},
    {57418,32,64,64,64,60},
    {57419,124,16,16,40,68},
    {57420,124,64,64,64,64},
    {57421,124,8,48,8,124},
    {57422,124,8,16,32,124},
    {57423,56,68,68,68,56},
    {57424,124,20,20,20,8},
    {57425,56,68,84,36,88},
    {57426,124,20,20,52,72},
    {57427,72,84,84,84,36},
    {57428,4,4,124,4,4},
    {57429,60,64,64,64,60},
    {57430,28,32,64,32,28},
    {57431,124,32,24,32,124},
    {57432,68,40,16,40,68},
    {57433,12,16,96,16,12},
    {57434,68,100,84,76,68},
    {57435,0,124,68,68,0},
    {57436,4,8,16,32,64},
    {57437,0,68,68,124,0},
    {57438,16,8,4,8,16},
    {57439,128,128,128,128,128},
    {57440,0,7,11,0,0},
    {57441,120,20,20,20,120},
    {57442,124,84,84,84,40},
    {57443,56,68,68,68,40},
    {57444,124,68,68,68,56},
    {57445,124,84,84,84,68},
    {57446,124,20,20,20,4},
    {57447,56,68,84,84,116},
    {57448,124,16,16,16,124},
    {57449,0,68,124,68,0},
    {57450,32,64,64,64,60},
    {57451,124,16,16,40,68},
    {57452,124,64,64,64,64},
    {57453,124,8,48,8,124},
    {57454,124,8,16,32,124},
    {57455,56,68,68,68,56},
    {57456,124,20,20,20,8},
    {57457,56,68,84,36,88},
    {57458,124,20,20,52,72},
    {57459,72,84,84,84,36},
    {57460,4,4,124,4,4},
    {57461,60,64,64,64,60},
    {57462,28,32,64,32,28},
    {57463,124,32,24,32,124},
    {57464,68,40,16,40,68},
    {57465,12,16,96,16,12},
    {57466,68,100,84,76,68},
    {57467,0,16,108,68,68},
    {57468,0,0,108,0,0},
    {57469,0,68,68,108,16},
    {57470,8,4,8,8,4},
    {57471,84,40,84,40,84},
    {57520,127,62,28,8,0},
    {57521,65,34,20,8,0},
    {57522,0,8,28,62,127},
    {57523,0,8,20,34,65},
    {65103,64,128,64,192,64},
    {65533,126,251,173,243,126}
};

inline bool getFontPixel(uint32_t c, unsigned x, unsigned y)
{
    if (c > 0xffff) c = '?';
    const FontCharInfo* info = &fontRom['?' - ' '];
    for(const auto& fci : fontRom) {
        if(fci.codepoint == c) {
            info = &fci;
            break;
        }
    }
    uint8_t data = info->data[x];
    return (data & (1u << y)) != 0;
}

void drawChar(Image& image, uint32_t c, int xPos, int yPos, Color col)
{
    for (auto y = 0; y < 8; ++y) {
        for (auto x = 0; x < 5; ++x) {
            if (getFontPixel(c, x, y)) {
                ImageDrawPixel(&image, xPos + x, yPos + y, col);
            }
        }
    }
}

class RlGuippApp
{
public:
    // Application state (for demo purposes)
    struct AppState {
        Color bg_color = DARKGRAY;
        bool show_grid = true;
        bool snap_to_grid = false;
        int grid_size = 20;
        std::string last_action = "(none)";
        std::vector<Rectangle> shapes;
        int selected_shape = -1;
    } state;

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
        generateFont();
        _styleManager.setTheme(0);

            // Optional: customize style
    //popup.style.highlight = {65, 105, 225, 255};  // Royal blue
    //popup.style.submenuOpenDelay = 0.15f;

    // Define context menu structure
    // Note: We rebuild this each frame to reflect current state (for toggles)
    build_context_menu = [this]() -> PopupMenu::Menu {
        return {
            PopupMenu::Action("Add Rectangle", [this]{
                state.shapes.push_back({
                    static_cast<float>(GetRandomValue(50, 700)),
                    static_cast<float>(GetRandomValue(50, 500)),
                    static_cast<float>(GetRandomValue(40, 120)),
                    static_cast<float>(GetRandomValue(40, 120))
                });
                state.last_action = "Added rectangle";
            }),
            PopupMenu::Action("Clear All", [this]{
                state.shapes.clear();
                state.selected_shape = -1;
                state.last_action = "Cleared all shapes";
            }, "Ctrl+Del"),

            PopupMenu::Separator(),

            PopupMenu::Submenu("Background Color", {
                PopupMenu::Action("Dark Gray", [this]{
                    state.bg_color = DARKGRAY;
                    state.last_action = "Background: Dark Gray";
                }),
                PopupMenu::Action("Dark Blue", [this]{
                    state.bg_color = {20, 30, 48, 255};
                    state.last_action = "Background: Dark Blue";
                }),
                PopupMenu::Action("Dark Green", [this]{
                    state.bg_color = {20, 48, 32, 255};
                    state.last_action = "Background: Dark Green";
                }),
                PopupMenu::Action("Black", [this]{
                    state.bg_color = BLACK;
                    state.last_action = "Background: Black";
                }),
            }),

            PopupMenu::Submenu("Grid", {
                PopupMenu::Toggle("Show Grid", &state.show_grid, [this]{
                    state.last_action = state.show_grid ? "Grid shown" : "Grid hidden";
                    state.show_grid = !state.show_grid;
                }),
                PopupMenu::Toggle("Snap to Grid", &state.snap_to_grid, [this]{
                    state.last_action = state.snap_to_grid ? "Snap enabled" : "Snap disabled";
                    state.snap_to_grid = !state.snap_to_grid;
                }),
                PopupMenu::Separator(),
                PopupMenu::Submenu("Grid Size", {
                    PopupMenu::Action("10px", [this]{ state.grid_size = 10; state.last_action = "Grid: 10px"; }),
                    PopupMenu::Action("20px", [this]{ state.grid_size = 20; state.last_action = "Grid: 20px"; }),
                    PopupMenu::Action("40px", [this]{ state.grid_size = 40; state.last_action = "Grid: 40px"; }),
                }),
            }),

            PopupMenu::Separator(),

            PopupMenu::Submenu("More Options", {
                PopupMenu::Action("Option A", [this]{ state.last_action = "Option A"; }),
                PopupMenu::Action("Option B", [this]{ state.last_action = "Option B"; }),
                PopupMenu::Submenu("Even More", {
                    PopupMenu::Action("Deep Option 1", [this]{ state.last_action = "Deep Option 1"; }),
                    PopupMenu::Action("Deep Option 2", [this]{ state.last_action = "Deep Option 2"; }),
                    PopupMenu::Submenu("Deeper Still", {
                        PopupMenu::Action("Very Deep", [this]{ state.last_action = "Very Deep!"; }),
                        PopupMenu::Disabled(PopupMenu::Action("Disabled Item", []{})),
                    }),
                }),
            }),

            PopupMenu::Separator(),

            PopupMenu::Disabled(PopupMenu::Action("Disabled Action", []{}, "Ctrl+D")),
            PopupMenu::Action("About", [this]{
                state.last_action = "Popup Menu Demo v1.0";
            }),
        };
    };

    // Shape-specific context menu (when right-clicking on a shape)
    build_shape_menu = [this](int shape_index) -> PopupMenu::Menu {
        return {
            PopupMenu::Action("Delete Shape", [this, shape_index]{
                if (shape_index >= 0 && shape_index < static_cast<int>(state.shapes.size())) {
                    state.shapes.erase(state.shapes.begin() + shape_index);
                    state.selected_shape = -1;
                    state.last_action = "Deleted shape";
                }
            }, "Del"),
            PopupMenu::Action("Duplicate", [this, shape_index]{
                if (shape_index >= 0 && shape_index < static_cast<int>(state.shapes.size())) {
                    Rectangle copy = state.shapes[shape_index];
                    copy.x += 20;
                    copy.y += 20;
                    state.shapes.push_back(copy);
                    state.last_action = "Duplicated shape";
                }
            }, "Ctrl+D"),
            PopupMenu::Separator(),
            PopupMenu::Submenu("Resize", {
                PopupMenu::Action("Grow", [this, shape_index]{
                    if (shape_index >= 0 && shape_index < static_cast<int>(state.shapes.size())) {
                        state.shapes[shape_index].width *= 1.2f;
                        state.shapes[shape_index].height *= 1.2f;
                        state.last_action = "Shape grown";
                    }
                }),
                PopupMenu::Action("Shrink", [this, shape_index]{
                    if (shape_index >= 0 && shape_index < static_cast<int>(state.shapes.size())) {
                        state.shapes[shape_index].width *= 0.8f;
                        state.shapes[shape_index].height *= 0.8f;
                        state.last_action = "Shape shrunk";
                    }
                }),
            }),
            PopupMenu::Submenu("Send to", {
                PopupMenu::Action("Front", [this, shape_index]{
                    if (shape_index >= 0 && shape_index < static_cast<int>(state.shapes.size()) - 1) {
                        Rectangle shape = state.shapes[shape_index];
                        state.shapes.erase(state.shapes.begin() + shape_index);
                        state.shapes.push_back(shape);
                        state.last_action = "Sent to front";
                    }
                }),
                PopupMenu::Action("Back", [this, shape_index]{
                    if (shape_index > 0 && shape_index < static_cast<int>(state.shapes.size())) {
                        Rectangle shape = state.shapes[shape_index];
                        state.shapes.erase(state.shapes.begin() + shape_index);
                        state.shapes.insert(state.shapes.begin(), shape);
                        state.last_action = "Sent to back";
                    }
                }),
            }),
        };
    };

    }
    ~RlGuippApp()
    {
        UnloadRenderTexture(_renderTexture);
        CloseWindow();
    }

    void generateFont()
    {
        int imageWidth = 256;
        int imageHeight = 256;
        _fontImage = GenImageColor(imageWidth, imageHeight, {0, 0, 0, 0});
        int glyphCount = 0;
        std::vector<Rectangle> rectangles;
        std::vector<GlyphInfo> glyphs;
        float glyphX = 0, glyphY = 0;
        for(const auto& fci : fontRom) {
            auto c = fci.codepoint;
            if (glyphX + 6 > imageWidth) {
                glyphX = 0;
                glyphY += 8;
            }
            drawChar(_fontImage, c, glyphX, glyphY, WHITE);
            rectangles.push_back(Rectangle{glyphX, glyphY, 6.0f, 8.0f});
            glyphs.push_back(GlyphInfo{c, 0, 0, 6});
            glyphX += 6;
            ++glyphCount;
        }
        _font.baseSize = 8;
        _font.glyphCount = glyphs.size();
        _font.texture = LoadTextureFromImage(_fontImage);
        _font.recs = static_cast<Rectangle*>(std::calloc(_font.glyphCount, sizeof(Rectangle)));
        _font.glyphs = static_cast<GlyphInfo*>(std::calloc(_font.glyphCount, sizeof(GlyphInfo)));
        std::ranges::copy(rectangles, _font.recs);
        std::ranges::copy(glyphs, _font.glyphs);
        GuiSetFont(_font);
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
        Vector2 mouse = GetMousePosition();

        // Handle right-click to open context menu
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !PopupMenu::IsOpen()) {
            // Check if clicking on a shape
            int clicked_shape = -1;
            // Check in reverse order (top shapes first)
            for (int i = static_cast<int>(state.shapes.size()) - 1; i >= 0; --i) {
                if (CheckCollisionPointRec(mouse, state.shapes[i])) {
                    clicked_shape = i;
                    break;
                }
            }

            if (clicked_shape >= 0) {
                currentMenu = build_shape_menu(clicked_shape);
                state.selected_shape = clicked_shape;
            } else {
                currentMenu = build_context_menu();
                state.selected_shape = -1;
            }
            PopupMenu::Open(mouse, currentMenu);
        }

        // Clear selection when menu closes
        if (!PopupMenu::IsOpen()) {
            state.selected_shape = -1;
        }

        ClearBackground(GetColor(GetStyle(DEFAULT, BACKGROUND_COLOR)));
        BeginGui({}, &_renderTexture, {0, 0}, guiScaling());
        {
            SetStyle(STATUSBAR, TEXT_PADDING, 4);
            SetStyle(LISTVIEW, SCROLLBAR_WIDTH, 6);
            SetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING, 0);
            SetStyle(SPINNER, TEXT_PADDING, 4);
            SetRowHeight(16);
            SetSpacing(0);

            // Draw grid
            if (state.show_grid) {
                Color grid_color = {255, 255, 255, 30};
                for (int x = 0; x < _width; x += state.grid_size) {
                    DrawLine(x, 0, x, _height, grid_color);
                }
                for (int y = 0; y < _height; y += state.grid_size) {
                    DrawLine(0, y, _width, y, grid_color);
                }
            }

            // Draw shapes
            for (int i = 0; i < static_cast<int>(state.shapes.size()); ++i) {
                Color fill = {100, 149, 237, 200};  // Cornflower blue
                Color outline = (i == state.selected_shape) ? YELLOW : WHITE;
                int outline_thick = (i == state.selected_shape) ? 3 : 1;

                DrawRectangleRec(state.shapes[i], fill);
                DrawRectangleLinesEx(state.shapes[i], outline_thick, outline);
            }

            // Draw instructions
            DrawText("Right-click for context menu", 10, 10, 20, WHITE);
            DrawText("Right-click on shapes for shape menu", 10, 35, 20, WHITE);

            // Draw status
            std::string status = std::format("Shapes: {}  |  Last action: {}",
                                             state.shapes.size(), state.last_action);
            DrawText(status.c_str(), 10, _height - 30, 18, LIGHTGRAY);


            /*
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
            */
        }
        EndGui();
    }

private:
    std::function<PopupMenu::Menu()> build_context_menu;
    std::function<PopupMenu::Menu(int)> build_shape_menu;
    PopupMenu::Menu currentMenu;
    const int MIN_WIDTH = 640;
    const int MIN_HEIGHT = 400;
    Image _fontImage{};;
    Font _font{};
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
