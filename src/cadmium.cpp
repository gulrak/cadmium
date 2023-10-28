//---------------------------------------------------------------------------------------
// src/cadmium.cpp
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
#include <raylib.h>
#include "icons.h"  // Custom icons set provided, generated with rGuiIcons tool

#include <rlguipp/rlguipp4.hpp>
#include <stylemanager.hpp>
#include "configuration.hpp"

extern "C" {
#include <raymath.h>
};

#include <date/date.h>
#include <fmt/format.h>
#include <stdendian/stdendian.h>
#include <about.hpp>
#include <emulation/c8bfile.hpp>
#include <chiplet/chip8decompiler.hpp>
#include <emulation/chip8cores.hpp>
#include <emulation/chip8dream.hpp>
#include <emulation/time.hpp>
#include <chiplet/utility.hpp>
#include <ghc/cli.hpp>
#include <chip8emuhostex.hpp>
#include <systemtools.hpp>
#include <resourcemanager.hpp>
#include <circularbuffer.hpp>
#include <debugger.hpp>
#include <logview.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <memory>
#include <regex>
#include <thread>
#include <mutex>
#include <new>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
extern "C" void openFileCallbackC(const char* str) __attribute__((used));
static std::function<void(const std::string&)> openFileCallback;
void openFileCallbackC(const char* str)
{
    if (openFileCallback)
        openFileCallback(str);
}
#ifdef WEB_WITH_FETCHING
#include <emscripten/fetch.h>
extern "C" void loadBinaryCallbackC(emscripten_fetch_t *fetch) __attribute__((used));
extern "C" void downloadFailedCallbackC(emscripten_fetch_t *fetch) __attribute__((used));
static std::function<void(std::string, const uint8_t*, size_t)> loadBinaryCallback;
void loadBinaryCallbackC(emscripten_fetch_t *fetch)
{
    if (loadBinaryCallback) {
        std::string_view url{fetch->url};
        auto pos = url.rfind('/');
        loadBinaryCallback(std::string(pos != std::string_view::npos ? url.substr(pos + 1) : url), reinterpret_cast<const unsigned char*>(fetch->data), fetch->numBytes);
    }
    emscripten_fetch_close(fetch);
}
void downloadFailedCallbackC(emscripten_fetch_t *fetch)
{
    emscripten_fetch_close(fetch);
}
#endif
#define RESIZABLE_GUI
#else
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma GCC diagnostic ignored "-Wenum-compare"
#if __clang__
#pragma GCC diagnostic ignored "-Wenum-compare-conditional"
#endif
#endif  // __GNUC__

extern "C" {
#include <octo_emulator.h>
}

#pragma GCC diagnostic pop

#include <fstream>

#endif

//#ifndef PLATFORM_WEB
#define WITH_EDITOR
#include <editor.hpp>
// #endif


#define CHIP8_STYLE_PROPS_COUNT 16
static const GuiStyleProp chip8StyleProps[CHIP8_STYLE_PROPS_COUNT] = {
    {0, 0, 0x2f7486ff},   // DEFAULT_BORDER_COLOR_NORMAL
    {0, 1, 0x024658ff},   // DEFAULT_BASE_COLOR_NORMAL
    {0, 2, 0x51bfd3ff},   // DEFAULT_TEXT_COLOR_NORMAL
    {0, 3, (int)0x82cde0ff},   // DEFAULT_BORDER_COLOR_FOCUSED
    {0, 4, 0x3299b4ff},   // DEFAULT_BASE_COLOR_FOCUSED
    {0, 5, (int)0xb6e1eaff},   // DEFAULT_TEXT_COLOR_FOCUSED
    {0, 6, (int)0x82cde0ff},   // DEFAULT_BORDER_COLOR_PRESSED
    {0, 7, 0x3299b4ff},   // DEFAULT_BASE_COLOR_PRESSED
    {0, 8, (int)0xeff8ffff},   // DEFAULT_TEXT_COLOR_PRESSED
    {0, 9, 0x134b5aff},   // DEFAULT_BORDER_COLOR_DISABLED
    {0, 10, 0x0e273aff},  // DEFAULT_BASE_COLOR_DISABLED
    {0, 11, 0x17505fff},  // DEFAULT_TEXT_COLOR_DISABLED
    {0, 16, 0x0000000e},  // DEFAULT_TEXT_SIZE
    {0, 17, 0x00000000},  // DEFAULT_TEXT_SPACING
    {0, 18, (int)0x81c0d0ff},  // DEFAULT_LINE_COLOR
    {0, 19, 0x00222bff},  // DEFAULT_BACKGROUND_COLOR
};

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
    {10240,0,0,0,0,0},
    {10495,85,85,0,85,85},
    {65103,64,128,128,128,64},
    {65533,126,251,173,243,126}
};

#ifdef PLATFORM_WEB
#ifdef WEB_WITH_CLIPBOARD
#include <jsct/JsClipboardTricks.h>
#if 0
EM_ASYNC_JS(void, copyClip, (const char* str), {
    document.getElementById("clipping").focus();
    const rtn = await navigator.clipboard.writeText(UTF8ToString(str));
    document.getElementById("canvas").focus();
});

EM_ASYNC_JS(char*, pasteClip, (), {
    document.getElementById("clipping").focus();
    const str = await navigator.clipboard.readText();
    document.getElementById("canvas").focus();
    const size = lengthBytesUTF8(str) + 1;
    const rtn = _malloc(size);
    stringToUTF8(str, rtn, size);
    return rtn;
});
#endif
#else
static std::string webClip;
#endif
#endif

std::string GetClipboardTextX()
{
#ifdef PLATFORM_WEB
#ifdef WEB_WITH_CLIPBOARD
    return JsClipboard_getClipText();//pasteClip();
#else
    return webClip;
#endif
#else
    return GetClipboardText();
#endif
}

void SetClipboardTextX(std::string text)
{
#ifdef PLATFORM_WEB
#ifdef WEB_WITH_CLIPBOARD
    //copyClip(text.c_str());
    JsClipboard_SetClipboardText(text.c_str());
#else
    webClip = text;
#endif
#else
    SetClipboardText(text.c_str());
#endif
}

bool isClipboardPaste()
{
#ifdef PLATFORM_WEB
#ifdef WEB_WITH_CLIPBOARD
    return JsClipboard_hasClipText();
#else
    return false;
#endif
#else
    return false;
#endif
}

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

void CenterWindow(int width, int height)
{
    auto monitor = GetCurrentMonitor();
    SetWindowPosition((GetMonitorWidth(monitor) - width) / 2, (GetMonitorHeight(monitor) - height) / 2);
}

void LogHandler(int msgType, const char *text, va_list args)
{
    static char buffer[4096];
#ifndef PLATFORM_WEB
    static std::ofstream ofs((fs::path(dataPath())/"logfile.txt").string().c_str());
    ofs << date::format("[%FT%TZ]", std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now()));
    switch (msgType)
    {
        case LOG_INFO: ofs << "[INFO] : "; break;
        case LOG_ERROR: ofs << "[ERROR]: "; break;
        case LOG_WARNING: ofs << "[WARN] : "; break;
        case LOG_DEBUG: ofs << "[DEBUG]: "; break;
        default: break;
    }
#endif
    vsnprintf(buffer, 4095, text, args);
#ifndef PLATFORM_WEB
    ofs << buffer << std::endl;
#endif
    emu::Logger::log(emu::Logger::eHOST, 0, {0,0}, buffer);
}

template <size_t N, typename ValueType = uint64_t, typename SumType = uint64_t>
class SMA
{
public:
    void reset()
    {
        _fill = _index = 0;
        _sum = 0;
    }
    void add(ValueType nextVal)
    {
        if(_fill < N)
            ++_fill;
        else
            _sum -= _history[_index];
        _sum += nextVal;
        _history[_index] = nextVal;
        if (++_index == N)
            _index = 0;
    }
    double get() const { return _fill ? double(_sum) / _fill : 0.0; }
private:
    size_t _fill{0};
    size_t _index{0};
    ValueType _history[N]{};
    SumType _sum{0};
};

std::atomic_uint8_t g_soundTimer{0};
std::atomic_int g_frameBoost{1};

class Cadmium : public emu::Chip8EmuHostEx
{
public:
    using ExecMode = emu::IChip8Emulator::ExecMode;
    using CpuState = emu::IChip8Emulator::CpuState;
    enum MemFlags { eNONE = 0, eBREAKPOINT = 1, eWATCHPOINT = 2 };
    enum MainView { eVIDEO, eDEBUGGER, eEDITOR, eTRACELOG, eSETTINGS, eROM_SELECTOR, eROM_EXPORT };
    enum EmulationMode { eCOSMAC_VIP_CHIP8, eGENERIC_CHIP8 };
    enum FileBrowserMode { eLOAD, eSAVE, eWEB_SAVE };
    static constexpr int MIN_SCREEN_WIDTH = 512;
    static constexpr int MIN_SCREEN_HEIGHT = 192*2+36;
    Cadmium(const emu::Chip8EmulatorOptions* chip8options = nullptr)
        : _audioBuffer(44100)
        , _screenWidth(MIN_SCREEN_WIDTH)
        , _screenHeight(MIN_SCREEN_HEIGHT)
    {
        SetTraceLogCallback(LogHandler);
#ifdef RESIZABLE_GUI
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_COCOA_GRAPHICS_SWITCHING);
#else
        SetConfigFlags(FLAG_COCOA_GRAPHICS_SWITCHING/*|FLAG_VSYNC_HINT*/);
#endif

        InitWindow(_screenWidth, _screenHeight, "Cadmium - A CHIP-8 variant environment");
#ifdef RESIZABLE_GUI
        if(GetMonitorWidth(GetCurrentMonitor()) > 1680 || GetWindowScaleDPI().x > 1.0f) {
            SetWindowSize(_screenWidth * 2, _screenHeight * 2);
            CenterWindow(_screenWidth * 2, _screenHeight * 2);
        }
#else
        _scaleBy2 = GetMonitorWidth(GetCurrentMonitor()) > 1680 || GetWindowScaleDPI().x > 1.0f;
#endif
        SetExitKey(0);

        _instance = this;
        InitAudioDevice();
        SetAudioStreamBufferSizeDefault(1470);
        _audioStream = LoadAudioStream(44100, 16, 1);
        SetAudioStreamCallback(_audioStream, Cadmium::audioInputCallback);
        PlayAudioStream(_audioStream);
        SetTargetFPS(60);

        _renderTexture = LoadRenderTexture(_screenWidth, _screenHeight);
        SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);

        _styleManager.setDefaultTheme();
        /*
        for (auto chip8StyleProp : chip8StyleProps) {
            GuiSetStyle(chip8StyleProp.controlId, chip8StyleProp.propertyId, chip8StyleProp.propertyValue);
        }
         */
        generateFont();
        if(chip8options) {
            _options = *chip8options;
            setPalette({_defaultPalette.begin(), _defaultPalette.end()});
        }
        else
            _mainView = eSETTINGS;
        updateEmulatorOptions(_options);
        whenEmuChanged(*_chipEmu);
        _debugger.updateCore(_chipEmu.get());
        _screen = GenImageColor(emu::Chip8EmulatorBase::MAX_SCREEN_WIDTH, emu::Chip8EmulatorBase::MAX_SCREEN_HEIGHT, BLACK);
        _screenTexture = LoadTextureFromImage(_screen);
        _crt = GenImageColor(256,512,BLACK);
        _crtTexture = LoadTextureFromImage(_crt);
        _screenShot = GenImageColor(emu::Chip8EmulatorBase::MAX_SCREEN_WIDTH, emu::Chip8EmulatorBase::MAX_SCREEN_HEIGHT, BLACK);
        _screenShotTexture = LoadTextureFromImage(_screen);
        SetTextureFilter(_crtTexture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(_screenShotTexture, TEXTURE_FILTER_POINT);
        _titleImage = LoadImage("cadmium-title.png");
        _microFont = LoadImage("micro-font.png");
        _keyboardOverlay = LoadRenderTexture(40,40);
        _chipEmu->reset();
        std::string versionStr(CADMIUM_VERSION);
        drawMicroText(_titleImage, "v" CADMIUM_VERSION, 91 - std::strlen("v" CADMIUM_VERSION)*4, 6, WHITE);
        if(!versionStr.empty() && (versionStr.back() & 1))
            drawMicroText(_titleImage, "WIP", 38, 53, WHITE);
        std::string buildDate = __DATE__;
        auto dateText = buildDate.substr(0, 3);
        bool shortDate = (buildDate[4] == ' ');
        drawMicroText(_titleImage, buildDate.substr(9), 83, 53, WHITE);
        drawMicroText(_titleImage, buildDate.substr(4,2), 75, 52, WHITE);
        drawMicroText(_titleImage, buildDate.substr(0,3), shortDate ? 67 : 63, 53, WHITE);
        //ImageColorReplace(&titleImage, {0,0,0,255}, {0x00,0x22,0x2b,0xff});
        ImageColorReplace(&_titleImage, {0,0,0,255}, {0x1a,0x1c,0x2c,0xff});
        ImageColorReplace(&_titleImage, {255,255,255,255}, {0x51,0xbf,0xd3,0xff});
        _icon = GenImageColor(64,64,{0,0,0,0});
        ImageDraw(&_icon, _titleImage, {34,2,60,60}, {2,2,60,60}, WHITE);
#ifndef __APPLE__
        SetWindowIcon(_icon);
#endif
        _titleTexture = LoadTextureFromImage(_titleImage);
        if(_currentDirectory.empty())
            _currentDirectory = _librarian.currentDirectory();
        else
            _librarian.fetchDir(_currentDirectory);

        updateResolution();

        // SWEETIE-16:
        // {0x1a1c2c, 0xf4f4f4, 0x94b0c2, 0x333c57, 0xef7d57, 0xa7f070, 0x3b5dc9, 0xffcd75, 0xb13e53, 0x38b764, 0x29366f, 0x566c86, 0x41a6f6, 0x73eff7, 0x5d275d, 0x257179}
        // PICO-8:
        // {0x000000, 0xfff1e8, 0xc2c3c7, 0x5f574f, 0xff004d, 0x00e436, 0x29adff, 0xffec27, 0xab5236, 0x008751, 0x1d2b53, 0xffa300, 0xff77a8, 0xffccaa, 0x7e2553, 0x83769c}
        // C64:
        // {0x000000, 0xffffff, 0xadadad, 0x626262, 0xa1683c, 0x9ae29b, 0x887ecb, 0xc9d487, 0x9f4e44, 0x5cab5e, 0x50459b, 0x6d5412, 0xcb7e75, 0x6abfc6, 0xa057a3, 0x898989}
        // Intellivision:
        // {0x0c0005, 0xfffcff, 0xa7a8a8, 0x3c5800, 0xff3e00, 0x6ccd30, 0x002dff, 0xfaea27, 0xffa600, 0x00a720, 0xbd95ff, 0xc9d464, 0xff3276, 0x5acbff, 0xc81a7d, 0x00780f}
        // CGA
        // {0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff5555, 0x55ff55, 0x5555ff, 0xffff55, 0xaa0000, 0x00aa00, 0x0000aa, 0xaa5500, 0xff55ff, 0x55ffff, 0xaa00aa, 0x00aaaa}
        // Silicon-8 1.0
        // {0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0x880000, 0x008800, 0x000088, 0x888800, 0xff00ff, 0x00ffff, 0x880088, 0x008888}
        // Macintosh II
        // {0x000000, 0xffffff, 0xb9b9b9, 0x454545, 0xdc0000, 0x00a800, 0x0000ca, 0xffff00, 0xff6500, 0x006500, 0x360097, 0x976536, 0xff0097, 0x0097ff, 0x653600, 0x868686}
        // IBM PCjr
        // {0x1c2536, 0xced9ed, 0x81899e, 0x030625, 0xe85685, 0x2cc64e, 0x0000e8, 0xa7c251, 0x9f2441, 0x077c35, 0x0e59f0, 0x4b7432, 0xc137ff, 0x0bc3a9, 0x6b03ca, 0x028566}
        // Daylight-16
        // {0x272223, 0xf2d3ac, 0xe7a76c, 0x6a422c, 0xb55b39, 0xb19e3f, 0x7a6977, 0xf8c65c, 0x996336, 0x606b31, 0x513a3d, 0xd58b39, 0xc28462, 0xb5c69a, 0x905b54, 0x878c87}
        // Soul of the Sea
        // {0x01141a, 0xcfbc95, 0x93a399, 0x2f4845, 0x92503f, 0x949576, 0x425961, 0x81784d, 0x703a28, 0x7a7e67, 0x203633, 0x605f33, 0x56452b, 0x467e73, 0x403521, 0x51675a}
/*
        setPalette({
            0x1a1c2cff, 0xf4f4f4ff, 0x94b0c2ff, 0x333c57ff,
            0xb13e53ff, 0xa7f070ff, 0x3b5dc9ff, 0xffcd75ff,
            0x5d275dff, 0x38b764ff, 0x29366fff, 0x566c86ff,
            0xef7d57ff, 0x73eff7ff, 0x41a6f6ff, 0x257179ff
        });
*/
#ifdef PLATFORM_WEB
        JsClipboard_AddJsHook();
#endif
    }

    ~Cadmium() override
    {
        gui::UnloadGui();
        UnloadFont(_font);
        UnloadImage(_fontImage);
        UnloadImage(_microFont);
        UnloadRenderTexture(_renderTexture);
        UnloadRenderTexture(_keyboardOverlay);
        UnloadImage(_titleImage);
        UnloadTexture(_titleTexture);
        UnloadTexture(_screenShotTexture);
        UnloadTexture(_crtTexture);
        UnloadTexture(_screenTexture);
        UnloadAudioStream(_audioStream);
        CloseAudioDevice();
        UnloadImage(_screenShot);
        UnloadImage(_crt);
        UnloadImage(_screen);
        UnloadImage(_icon);
        _instance = nullptr;
        CloseWindow();
        if(!_cfgPath.empty()) {
            _cfg.workingDirectory = _currentDirectory;
            saveConfig();
        }
    }

    void updateResolution()
    {
#ifdef RESIZABLE_GUI
        auto width = std::max(GetScreenWidth(), _screenWidth);
        auto height = std::max(GetScreenHeight(), _screenHeight);

//        if(GetScreenWidth() < width || GetScreenHeight() < height)
//            SetWindowSize(width, height);
#else
        if(_screenHeight < MIN_SCREEN_HEIGHT ||_screenWidth < MIN_SCREEN_WIDTH) {
            UnloadRenderTexture(_renderTexture);
            _screenWidth = MIN_SCREEN_WIDTH;
            _screenHeight = MIN_SCREEN_HEIGHT;
            _renderTexture = LoadRenderTexture(_screenWidth, _screenHeight);
            SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);
            SetWindowSize(_screenWidth * (_scaleBy2 ? 2 : 1), _screenHeight * (_scaleBy2 ? 2 : 1));
        }
#endif
    }

    bool isHeadless() const override { return false; }

    void drawMicroText(Image& dest, std::string text, int x, int y, Color tint)
    {
        for(auto c : text) {
            if((uint8_t)c < 128)
                ImageDraw(&dest, _microFont, {(c%32)*4.0f, (c/32)*6.0f, 4, 6}, {(float)x, (float)y, 4, 6}, tint);
            x += 4;
        }
    }

    static Cadmium* instance() { return _instance; }

    static void audioInputCallback(void *buffer, unsigned int frames)
    {
        auto* inst = Cadmium::instance();
        if(inst) {
            inst->renderAudio(static_cast<int16_t*>(buffer), frames);
        }
    }

    void renderAudio(int16_t *samples, unsigned int frames)
    {
        std::scoped_lock lock(_audioMutex);
        if(_chipEmu) {
            if(_options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) {
                while(frames--) {
                    *samples++ = ((int16_t)_chipEmu->getNextMCSample() - 128) * 256;
                }
                return;
            }
            else {
                auto st = _chipEmu->soundTimer();
                if(st && _chipEmu->getExecMode() == emu::GenericCpu::eRUNNING) {
                    auto samplesLeftToPlay = std::min(st * (44100 / 60) / g_frameBoost, (int)frames);
                    float phase = _chipEmu->getAudioPhase();
                    if (!_options.optXOChipSound) {
                        const float step = _chipEmu->getAudioFrequency() / 44100;
                        for (int i = 0; i < samplesLeftToPlay; ++i, --frames) {
                            *samples++ = (phase > 0.5f) ? 16384 : -16384;
                            phase = std::fmod(phase + step, 1.0f);
                        }
                        _chipEmu->setAudioPhase(phase);
                    }
                    else {
                        auto len = _audioBuffer.read(samples, frames);
                        frames -= len;
                        if (frames > 0) {
                            //TraceLog(LOG_WARNING, "AudioBuffer underrun: %d frames", frames);
                            auto step = 4000 * std::pow(2.0f, (float(_chipEmu->getXOPitch()) - 64) / 48.0f) / 128 / 44100;
                            for (; frames > 0; --frames) {
                                auto pos = int(std::clamp(phase * 128.0f, 0.0f, 127.0f));
                                *samples++ = _chipEmu->getXOAudioPattern()[pos >> 3] & (1 << (7 - (pos & 7))) ? 16384 : -16384;
                                phase = std::fmod(phase + step, 1.0f);
                            }
                            _chipEmu->setAudioPhase(phase);
                        }
                    }
                }
            }
        }
        while(frames--) {
            *samples++ = 0;
        }
    }

    void pushAudio(float deltaT = 1.0f/60)
    {
        static int16_t sampleBuffer[44100];
        auto st = _chipEmu->soundTimer();
        if(_chipEmu->getExecMode() == emu::IChip8Emulator::eRUNNING && st && _options.optXOChipSound) {
            auto samples = int(44100 * deltaT + 0.75f);
            if(samples > 44100) samples = 44100;
            auto step = 4000 * std::pow(2.0f, (float(_chipEmu->getXOPitch()) - 64) / 48.0f) / 128 / 44100;
            float phase = st ? _chipEmu->getAudioPhase() : 0.0f;
            auto* dest = sampleBuffer;
            for (int i = 0; i < samples; ++i) {
                auto pos = int(std::clamp(phase * 128.0f, 0.0f, 127.0f));
                *dest++ = _chipEmu->getXOAudioPattern()[pos >> 3] & (1 << (7 - (pos & 7))) ? 16384 : -16384;
                phase = std::fmod(phase + step, 1.0f);
            }
            _audioBuffer.write(sampleBuffer, samples);
            _chipEmu->setAudioPhase(phase);
        }
    }

    uint8_t getKeyPressed() override
    {
        static uint32_t instruction = 0;
        static int waitKeyUp = 0;
        static int keyId = 0;
        auto now = GetTime();
        for(int i = 0; i < 16; ++i)
            _keyScanTime[i] = now;
        if(waitKeyUp && instruction == _chipEmu->getPC()) {
            if(IsKeyUp(waitKeyUp)) {
                waitKeyUp = 0;
                instruction = 0;
                return keyId;
            }
            return 0;
        }
        waitKeyUp = 0;
        auto key = GetKeyPressed();
        if (key) {
            for (int i = 0; i < 16; ++i) {
                if (key == _keyMapping[i]) {
                    instruction = _chipEmu->getPC();
                    waitKeyUp = key;
                    keyId = i + 1;
                    return 0;
                }
            }
        }
        return 0;
    }

    bool isKeyDown(uint8_t key) override
    {
        _keyScanTime[key & 0xF] = GetTime();
        return IsKeyDown(_keyMapping[key & 0xF]);
    }

    const std::array<bool, 16>& getKeyStates() const override
    {
        return _keyMatrix;
    }

    void updateKeyboardOverlay()
    {
        static const char* keys = "1\0" "2\0" "3\0" "C\0" "4\0" "5\0" "6\0" "D\0" "7\0" "8\0" "9\0" "E\0" "A\0" "0\0" "B\0" "F\0";
        BeginTextureMode(_keyboardOverlay);
        ClearBackground({0,0,0,0});
        auto now = GetTime();
        for(int i = 0; i < 4; ++i) {
            for(int j = 0; j < 4; ++j) {
                DrawRectangleRec({j*10.0f, i*10.0f, 9.0f, 9.0f}, now - _keyScanTime[_keyPosition[i*4+j]] < 0.2 ? WHITE : GRAY);
                DrawTextEx(_font, &keys[i*8+j*2], {j*10.0f + 2.0f, i*10.0f + 1.0f}, 8.0f, 0, BLACK);
            }
        }
        EndTextureMode();
    }

    static Vector3 rgbToXyz(Color c)
    {
        float x, y, z, r, g, b;

        r = c.r / 255.0f; g = c.g / 255.0f; b = c.b / 255.0f;

        if (r > 0.04045f)
            r = std::pow(((r + 0.055f) / 1.055f), 2.4f);
        else r /= 12.92;

        if (g > 0.04045f)
            g = std::pow(((g + 0.055f) / 1.055f), 2.4f);
        else g /= 12.92;

        if (b > 0.04045f)
            b = std::pow(((b + 0.055f) / 1.055f), 2.4f);
        else b /= 12.92f;

        r *= 100; g *= 100; b *= 100;

        x = r * 0.4124f + g * 0.3576f + b * 0.1805f;
        y = r * 0.2126f + g * 0.7152f + b * 0.0722f;
        z = r * 0.0193f + g * 0.1192f + b * 0.9505f;

        return {x, y, z};
    }

    static Vector3 xyzToCIELAB(Vector3 c)
    {
        float x, y, z, l, a, b;
        const float refX = 95.047f, refY = 100.0f, refZ = 108.883f;

        x = c.x / refX; y = c.y / refY; z = c.z / refZ;

        if (x > 0.008856f)
            x = powf(x, 1 / 3.0f);
        else x = (7.787f * x) + (16.0f / 116.0f);

        if (y > 0.008856f)
            y = powf(y, 1 / 3.0);
        else y = (7.787f * y) + (16.0f / 116.0f);

        if (z > 0.008856f)
            z = powf(z, 1 / 3.0);
        else z = (7.787f * z) + (16.0f / 116.0f);

        l = 116 * y - 16;
        a = 500 * (x - y);
        b = 200 * (y - z);

        return {l, a, b};
    }

    static float getColorDeltaE(Color c1, Color c2)
    {
        Vector3 xyzC1 = rgbToXyz(c1), xyzC2 = rgbToXyz(c2);
        Vector3 labC1 = xyzToCIELAB(xyzC1), labC2 = xyzToCIELAB(xyzC2);
        return Vector3Distance(labC1, labC2);;
    }

    static inline uint32_t rgb332To888(uint8_t c)
    {
        static uint8_t b3[] = {0, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xff};
        static uint8_t b2[] = {0, 0x60, 0xA0, 0xff};
        return (b3[(c & 0xe0) >> 5] << 16) | (b3[(c & 0x1c) >> 2] << 8) | (b2[c & 3]);
    }

    void updatePalette(const std::array<uint8_t,16>& palette) override
    {
        if(!_customPalette) {
            for (int i = 0; i < palette.size(); ++i) {
                _colorPalette[i] = ((rgb332To888(palette[i]) << 8) | 0xff);
            }
            _updateScreen = true;
        }
    }

    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override
    {
        setPalette(palette, offset);
    }

    void generateFont()
    {
        _fontImage = GenImageColor(256, 256, {0, 0, 0, 0});
        int glyphCount = 0;
        for(const auto& fci : fontRom) {
            auto c = fci.codepoint;
            drawChar(_fontImage, c, (glyphCount % 32) * 6, (glyphCount / 32) * 8, WHITE);
            ++glyphCount;
        }
#if !defined(NDEBUG) && defined(EXPORT_FONT)
        ExportImage(_fontImage, "Test.png");
        {
            std::ofstream fos("font.txt");
            for (uint8_t c = 32; c < 128; ++c) {
                fos << "char: " << fmt::format("0x{:04x}", c) << " " << c << std::endl;
                for(int y = 0; y < 8; ++y) {
                    for(int x = 0; x < 5; ++x) {
                        fos << (getFontPixel(c, x, y) ? "#" : "-");
                    }
                    fos << "-" << std::endl;
                }
            }
            fos << std::endl;
        }
#endif
        _font.baseSize = 8;
        _font.glyphCount = glyphCount;
        _font.texture = LoadTextureFromImage(_fontImage);
        _font.recs = (Rectangle*)std::calloc(_font.glyphCount, sizeof(Rectangle));
        _font.glyphs = (GlyphInfo*)std::calloc(_font.glyphCount, sizeof(GlyphInfo));
        int idx = 0;
        for(const auto& fci : fontRom) {
            int i = fci.codepoint;
            _font.recs[idx].x = (idx % 32) * 6;
            _font.recs[idx].y = (idx / 32) * 8;
            _font.recs[idx].width = 6;
            _font.recs[idx].height = 8;
            _font.glyphs[idx].value = i;
            _font.glyphs[idx].offsetX = 0;
            _font.glyphs[idx].offsetY = 0;
            _font.glyphs[idx].advanceX = 6;
            ++idx;
        }
        GuiSetFont(_font);
    }

    bool screenChanged() const
    {
        return _updateScreen;
    }

    int getInstrPerFrame() const { return _options.instructionsPerFrame>=0 ? _options.instructionsPerFrame : 0; }
    int getFrameBoost() const { return _frameBoost > 0 && getInstrPerFrame() > 0 ? _frameBoost : 1; }

    void updateScreen() override
    {
        auto* pixel = (uint32_t*)_screen.data;
        if(pixel) {
            const auto* screen = _chipEmu->getScreen();
            if (screen) {
                if (!_renderCrt) {
                    screen->convert(pixel, _screen.width);
                    UpdateTexture(_screenTexture, _screen.data);
                }
                else {
                }
            }
            else {
                // TraceLog(LOG_INFO, "Updating MC8 screen!");
                const auto* screen = _chipEmu->getScreenRGBA();
                screen->convert(pixel, _screen.width);
                UpdateTexture(_screenTexture, _screen.data);
            }
        }
    }

    static void updateAndDrawFrame(void* self)
    {
        static_cast<Cadmium*>(self)->updateAndDraw();
    }

    void updateAndDraw()
    {
        static auto lastFrameTime = std::chrono::steady_clock::now() - std::chrono::milliseconds(16);
        auto now = std::chrono::steady_clock::now();
        double deltaTC = std::chrono::duration<double>(now - lastFrameTime).count();
        lastFrameTime = now;
        float deltaT = GetFrameTime();
#ifdef RESIZABLE_GUI
#if 0
        static int resizeCount = 0;
        if (IsWindowResized()) {
            int width{0}, height{0};
            resizeCount++;
#if defined(PLATFORM_WEB)
            double devicePixelRatio = emscripten_get_device_pixel_ratio();
            width = GetScreenWidth() * devicePixelRatio;
            height = GetScreenHeight() * devicePixelRatio;
#else
            glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
#endif
            TraceLog(LOG_INFO, "Window resized: %dx%d, fb: %dx%d", GetScreenWidth(), GetScreenHeight(), width, height);
        }
#endif
        auto screenScale = std::min(std::clamp(int(GetScreenWidth() / _screenWidth), 1, 8), std::clamp(int(GetScreenHeight() / _screenHeight), 1, 8));
        SetMouseScale(1.0f/screenScale, 1.0f/screenScale);
#else
        if (_scaleBy2) {
            // Screen size x2
            if (GetScreenWidth() < _screenWidth * 2) {
                SetWindowSize(_screenWidth * 2, _screenHeight * 2);
                //CenterWindow(_screenWidth * 2, _screenHeight * 2);
                SetMouseScale(0.5f, 0.5f);
            }
        }
        else {
            // Screen size x1
            if (_screenWidth < GetScreenWidth()) {
                SetWindowSize(_screenWidth, _screenHeight);
                //CenterWindow(_screenWidth, _screenHeight);
                SetMouseScale(1.0f, 1.0f);
            }
        }
#endif

        updateResolution();

        _librarian.update(_options); // allows librarian to complete background tasks

        if (IsFileDropped()) {
            auto files = LoadDroppedFiles();
            if (files.count > 0) {
                loadRom(files.paths[0], false);
            }
            UnloadDroppedFiles(files);
        }

#ifdef WITH_EDITOR
        if(_mainView == eEDITOR) {
            _editor.update();
            if(!_editor.compiler().isError() && _editor.compiler().sha1Hex() != _romSha1Hex) {
                _romImage.assign(_editor.compiler().code(), _editor.compiler().code() + _editor.compiler().codeSize());
                _romSha1Hex = _editor.compiler().sha1Hex();
                _debugger.updateOctoBreakpoints(_editor.compiler());
                reloadRom();
            }
        }
#endif

        for(uint8_t key = 0; key < 16; ++key) {
            _keyMatrix[key] = IsKeyDown(_keyMapping[key & 0xF]);
        }
        static int cntx = 0;
        static int64_t excessTime = 0;
        //if(!(cntx++ & 0x7f))
        //    std::clog << fmt::format("Frame time: rl: {}ms, chrono: {}ms, excessTime_us: {}", deltaT, deltaTC, excessTime) << std::endl;
        if(excessTime < deltaTC * 1000000) {
            excessTime = _chipEmu->executeFor(deltaTC * 1000000 - excessTime);
        }
        else {
            excessTime = 0;
        }
        /*
        auto fb = getFrameBoost();
        for(int i = 0; i < fb; ++i) {
            _chipEmu->tick(getInstrPerFrame());
            g_soundTimer.store(_chipEmu->soundTimer());
        }
         */
        pushAudio(deltaT);

        if(_chipEmu->needsScreenUpdate())
            updateScreen();
        if(_showKeyMap)
            updateKeyboardOverlay();

        BeginTextureMode(_renderTexture);
        drawGui();
        EndTextureMode();

        BeginDrawing();
        {
            ClearBackground(CADMIUM_VERSION_DECIMAL & 1 ? RED : BLACK);
#ifdef RESIZABLE_GUI
            Vector2 guiOffset = {(GetScreenWidth() - _screenWidth*screenScale)/2.0f, (GetScreenHeight() - _screenHeight*screenScale)/2.0f};
            if(guiOffset.x < 0) guiOffset.x = 0;
            if(guiOffset.y < 0) guiOffset.y = 0;
            DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){guiOffset.x, guiOffset.y, (float)_renderTexture.texture.width * screenScale, (float)_renderTexture.texture.height * screenScale},
                           (Vector2){0, 0}, 0.0f, WHITE);
#else
            if (_scaleBy2) {
                DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){0, 0, (float)_renderTexture.texture.width * 2, (float)_renderTexture.texture.height * 2},
                               (Vector2){0, 0}, 0.0f, WHITE);
            }
            else {
                DrawTextureRec(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Vector2){0, 0}, WHITE);
            }
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


    void drawScreen(Rectangle dest, int gridScale)
    {
        const Color gridLineCol{40,40,40,255};
        bool crt = _renderCrt;
        int scrWidth = crt ? 130 : _chipEmu->getCurrentScreenWidth();
        int scrHeight = crt ? 385 : (_chipEmu->isGenericEmulation() ? _chipEmu->getCurrentScreenHeight() : 128);
        auto videoScale = dest.width / scrWidth;
        auto videoScaleY = _chipEmu->isGenericEmulation() ? videoScale : videoScale/4;
        auto videoX = crt ? (dest.width - scrWidth * videoScale) / 2 + dest.x : (dest.width - _chipEmu->getCurrentScreenWidth() * videoScale) / 2 + dest.x;
        auto videoY = crt ? (dest.height - scrHeight * videoScaleY) / 2 + dest.y : (dest.height - _chipEmu->getCurrentScreenHeight() * videoScaleY) / 2 + dest.y;
        DrawRectangleRec(dest, {0,12,24,255});
        if(crt)
            DrawTexturePro(_crtTexture, {1, 1, (float)scrWidth-2, (float)scrHeight-2}, {videoX, videoY, scrWidth * videoScale, scrHeight * videoScaleY}, {0, 0}, 0, WHITE);
        else
            DrawTexturePro(_screenTexture, {0, 0, (float)scrWidth, (float)scrHeight}, {videoX, videoY, scrWidth * videoScale, scrHeight * videoScaleY}, {0, 0}, 0, WHITE);
//        DrawRectangleLines(videoX, videoY, scrWidth * videoScale, scrHeight * videoScaleY, RED);
        if (_grid && !crt) {
            for (short x = 0; x < scrWidth; ++x) {
                DrawRectangle(videoX + x * gridScale, videoY, 1, scrHeight * videoScaleY, gridLineCol);
            }
            if(_chipEmu->isGenericEmulation()) {
                for (short y = 0; y < scrHeight; ++y) {
                    DrawRectangle(videoX, videoY + y * gridScale, scrWidth * videoScale, 1, gridLineCol);
                }
            }
        }
        if(_showKeyMap) {
            DrawTexturePro(_keyboardOverlay.texture, {0, 0, 40, -40}, {videoX + scrWidth * videoScale - 40.0f, videoY + scrHeight * videoScaleY - 40.0f, 40.0f, 40.0f}, {0, 0}, 0.0f, {255, 255, 255, 128});
        }
        if(GetTime() < 5 && _romImage.empty()) {
            auto scale = dest.width / 128;
            auto offsetX = (dest.width - 60*scale) / 2;
            auto offsetY = (dest.height - 60*scale) / 2;
            DrawTexturePro(_titleTexture, {34, 2, 60, 60}, {dest.x + offsetX, dest.y + offsetY, 60*scale, 60*scale}, {0, 0}, 0, {255,255,255,uint8_t(GetTime()>4 ? 255.0f*(4.0f-GetTime()) : 255.0f)});
        }
#if 0
        rlSetBlendFactors(1, 0, 0x8006);
        rlSetBlendMode(RL_BLEND_CUSTOM);
        DrawRectangle(25,25,75,75, {0,0,0,0});
        rlSetBlendMode(RL_BLEND_ALPHA);
#endif
    }

    static bool iconButton(int iconId, bool isPressed = false, Color color = {3, 127, 161}, Color foreground = {0x51, 0xbf, 0xd3, 0xff})
    {
        StyleManager::Scope guard;
        if (isPressed)
            guard.setStyle(Style::BASE_COLOR_NORMAL, color);
        guard.setStyle(Style::TEXT_COLOR_NORMAL, foreground);
        gui::SetNextWidth(20);
        auto result = gui::Button(GuiIconText(iconId, ""));
        return result;
    }

    const std::vector<std::pair<uint32_t,std::string>>& disassembleNLinesBackwardsGeneric(uint32_t addr, int n)
    {
        static std::vector<std::pair<uint32_t,std::string>> disassembly;
        auto* rcb = dynamic_cast<emu::Chip8RealCoreBase*>(_chipEmu.get());
        if(rcb) {
            n *= 4;
            uint32_t start = n > addr ? 0 : addr - n;
            disassembly.clear();
            bool inIf = false;
            while (start < addr) {
                int bytes = 0;
                auto instruction = rcb->getBackendCpu().disassembleInstructionWithBytes(start, &bytes);
                disassembly.emplace_back(start, instruction);
                start += bytes;
            }
        }
        return disassembly;
    }

    void drawGui()
    {
        using namespace gui;
        ClearBackground(GetColor(GetStyle(DEFAULT, BACKGROUND_COLOR)));
        Rectangle video;
        int gridScale = 4;
        static int64_t lastInstructionCount = 0;
        static int64_t lastFrameCount = 0;
        static bool colorSelectOpen = false;
        static uint32_t* selectedColor = nullptr;
        static std::string colorText;
        static uint32_t previousColor{};

#ifdef RESIZABLE_GUI
        auto screenScale = std::min(std::clamp(int(GetScreenWidth() / _screenWidth), 1, 8), std::clamp(int(GetScreenHeight() / _screenHeight), 1, 8));
        Vector2 mouseOffset = {-(GetScreenWidth() - _screenWidth*screenScale)/2.0f, -(GetScreenHeight() - _screenHeight*screenScale)/2.0f};
        if(mouseOffset.x > 0) mouseOffset.x = 0;
        if(mouseOffset.y > 0) mouseOffset.y = 0;
        BeginGui({}, &_renderTexture, mouseOffset, {(float)screenScale, (float)screenScale});
#else
        BeginGui({}, &_renderTexture, {0,0}, {_scaleBy2?2.0f:1.0f, _scaleBy2?2.0f:1.0f});
#endif
        {
            SetStyle(STATUSBAR, TEXT_PADDING, 4);
            SetStyle(LISTVIEW, SCROLLBAR_WIDTH, 6);
            SetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING, 0);

            SetRowHeight(16);
            SetSpacing(0);
            auto instructionsThisUpdate = _chipEmu->getCycles() - lastInstructionCount;
            auto framesThisUpdate = _chipEmu->frames() - lastFrameCount;
            if(_chipEmu->getExecMode() == emu::GenericCpu::eRUNNING) {
                _ipfAverage.add(instructionsThisUpdate);
                _frameTimeAverage_us.add(GetFrameTime() * 1000000);
                _frameDelta.add(framesThisUpdate);
            }
            auto ipfAvg = _ipfAverage.get();
            auto ftAvg_us = _frameTimeAverage_us.get();
            auto fdAvg = _frameDelta.get();
            auto ips = instructionsThisUpdate / GetFrameTime();

            auto ipsAvg = float(ipfAvg) * 1000000 / ftAvg_us;
            if(_mainView == eEDITOR) {
                StatusBar({{0.55f, ""},
                           {0.15f, fmt::format("{} byte", _editor.compiler().codeSize()).c_str()},
                           {0.15f, fmt::format("{}:{}", _editor.line(), _editor.column()).c_str()},
                           {0.1f, emu::Chip8EmulatorOptions::shortNameOfPreset(_options.behaviorBase)}});
            }
            else if(_chipEmu->cpuState() == emu::IChip8Emulator::eERROR) {
                StatusBar({{0.55f, _chipEmu->errorMessage().c_str()},
                           {0.15f, formatUnit(ipsAvg, "IPS").c_str()},
                           {0.15f, formatUnit(fdAvg * 1000000 / ftAvg_us, "FPS").c_str()},
                           {0.1f, emu::Chip8EmulatorOptions::shortNameOfPreset(_options.behaviorBase)}});
            }
            else if(getFrameBoost() > 1) {
                StatusBar({{0.5f, fmt::format("Instruction cycles: {}", _chipEmu->getCycles()).c_str()},
                           {0.2f, formatUnit(ipsAvg, "IPS").c_str()},
                           {0.15f, formatUnit(fdAvg * 1000000 / ftAvg_us, "eFPS").c_str()},
                           {0.1f, emu::Chip8EmulatorOptions::shortNameOfPreset(_options.behaviorBase)}});
            }
            else {
                if(_chipEmu->getCycles() != _chipEmu->getMachineCycles()) {
                    StatusBar({{0.55f, fmt::format("Instruction cycles: {}/{} [{}]", _chipEmu->getCycles(), _chipEmu->getMachineCycles(), _chipEmu->frames()).c_str()},
                               {0.15f, formatUnit(ipsAvg, "IPS").c_str()},
                               {0.15f, formatUnit(fdAvg * 1000000 / ftAvg_us, "FPS").c_str()},
                               {0.1f, emu::Chip8EmulatorOptions::shortNameOfPreset(_options.behaviorBase)}});
                }
                else {
                    StatusBar({{0.55f, fmt::format("Instruction cycles: {} [{}]", _chipEmu->getCycles(), _chipEmu->frames()).c_str()},
                               {0.15f, formatUnit(ipsAvg, "IPS").c_str()},
                               //{0.15f, formatUnit((double)getFrameBoost() * GetFPS(), "FPS").c_str()},
                               {0.15f, formatUnit(fdAvg * 1000000 / ftAvg_us, "FPS").c_str()},
                               {0.1f, emu::Chip8EmulatorOptions::shortNameOfPreset(_options.behaviorBase)}});
                }
            }
            lastInstructionCount = _chipEmu->getCycles();
            lastFrameCount = _chipEmu->frames();
            BeginColumns();
            {
                SetRowHeight(20);
                SetSpacing(0);
                SetNextWidth(20);
                static bool menuOpen = false;
                static bool aboutOpen = false;
                static Vector2 aboutScroll{};
                if(Button(GuiIconText(ICON_BURGER_MENU, "")))
                    menuOpen = true;
                if(menuOpen) {
#ifndef PLATFORM_WEB
                    Rectangle menuRect = {1, GetCurrentPos().y + 20, 110, 84};
#else
                    Rectangle menuRect = {1, GetCurrentPos().y + 20, 110, 69};
#endif
                    BeginPopup(menuRect, &menuOpen);
                    SetRowHeight(12);
                    Space(3);
                    if(LabelButton(" About Cadmium..."))
                        aboutOpen = true, aboutScroll = {0,0}, menuOpen = false;
                    Space(3);
                    if(LabelButton(" New...")) {
                        _mainView = eEDITOR;
                        menuOpen = false;
                        _editor.setText(": main\n    jump main");
                        _romName = "unnamed.8o";
                        _editor.setFilename("");
                        _chipEmu->removeAllBreakpoints();
                    }
                    if(LabelButton(" Open...")) {
#ifdef PLATFORM_WEB
                        loadFileWeb();
#else
                        _mainView = eROM_SELECTOR;
                        _librarian.fetchDir(_currentDirectory);
#endif
                        menuOpen = false;
                    }
                    if(LabelButton(" Save...")) {
                        _mainView = eROM_EXPORT;
#ifndef PLATFORM_WEB
                        _librarian.fetchDir(_currentDirectory);
#endif
                        menuOpen = false;
                    }
                    if(LabelButton(" Key Map")) {
                        _showKeyMap = !_showKeyMap;
                        menuOpen = false;
                    }
#ifndef PLATFORM_WEB
                    Space(3);
                    if(LabelButton(" Quit"))
                        menuOpen = false, _shouldClose = true;
#endif
                    EndPopup();
                    if(IsKeyPressed(KEY_ESCAPE) || (IsMouseButtonPressed(0) && !CheckCollisionPointRec(GetMousePosition(), menuRect)))
                        menuOpen = false;
                }
                if(aboutOpen) {
                    aboutOpen = !BeginWindowBox({-1,-1,460,300}, "About Cadmium", &aboutOpen, WindowBoxFlags(WBF_MOVABLE|WBF_MODAL));
                    SetStyle(DEFAULT, BORDER_WIDTH, 0);
                    static size_t newlines = std::count_if( aboutText.begin(), aboutText.end(), [](char c){ return c =='\n'; });
                    BeginScrollPanel(-1, {0,0,445,newlines*10.0f + 100}, &aboutScroll);
                    SetRowHeight(10);
                    DrawTextureRec(_titleTexture, {34,2,60,60}, {aboutScroll.x + 8.0f, aboutScroll.y + 31.0f}, WHITE);
                    auto styleColor = GetStyle(LABEL, TEXT_COLOR_NORMAL);
                    SetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(WHITE));
                    Label("           Cadmium v" CADMIUM_VERSION);
                    SetStyle(LABEL, TEXT_COLOR_NORMAL, styleColor);
                    Space(4);
                    Label("           (c) 2022 by Steffen 'Gulrak' Schümann");
                    if(LabelButton("           https://github.com/gulrak/cadmium")) {
                        OpenURL("https://github.com/gulrak/cadmium");
                    }
                    Space(8);
                    std::istringstream iss(aboutText);
                    for (std::string line; std::getline(iss, line); )
                    {
                        auto trimmedLine = trim(line);
                        if(startsWith(trimmedLine, "http")) {
                            if(LabelButton(line.c_str()))
                                OpenURL(trimmedLine.c_str());
                        }
                        else if(startsWith(line, "# "))
                        {
                            SetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(WHITE));
                            Label(line.substr(2).c_str());
                            SetStyle(LABEL, TEXT_COLOR_NORMAL, styleColor);
                        }
                        else
                            Label(line.c_str());
                    }
                    EndScrollPanel();
                    SetStyle(DEFAULT, BORDER_WIDTH, 1);
                    EndWindowBox();
                    if(IsKeyPressed(KEY_ESCAPE))
                        aboutOpen = false;
                }
                SetNextWidth(20);
                if (iconButton(ICON_ROM, _mainView == eROM_SELECTOR)) {
#ifdef PLATFORM_WEB
                    loadFileWeb();
#else
                    _mainView = eROM_SELECTOR;
                    _librarian.fetchDir(_currentDirectory);
#endif
                }
                SetNextWidth(130);
                SetStyle(TEXTBOX, BORDER_WIDTH, 1);
                TextBox(_romName, 4095);

                bool chip8Control = _debugger.isControllingChip8();
                Color controlBack = {3, 127, 161};
                Color controlColor = Color{0x51, 0xbf, 0xd3, 0xff}; //chip8Control ? Color{0x51, 0xbf, 0xd3, 0xff} : Color{0x51, 0xff, 0xbf, 0xff};
                if (iconButton(ICON_PLAYER_PAUSE, _chipEmu->getExecMode() == ExecMode::ePAUSED, controlBack, controlColor)) {
                    _chipEmu->setExecMode(ExecMode::ePAUSED);
                    if(_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eVIDEO;
                    }
                }
                SetTooltip("PAUSE");
                if (iconButton(ICON_PLAYER_PLAY, _chipEmu->getExecMode() == ExecMode::eRUNNING, controlBack, controlColor)) {
                    _debugger.setExecMode(ExecMode::eRUNNING);
                    if(_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eVIDEO;
                    }
                }
                SetTooltip("RUN");
                if(!_debugger.supportsStepOver())
                    GuiDisable();
                if (iconButton(ICON_STEP_OVER, _chipEmu->getExecMode() == ExecMode::eSTEPOVER, controlBack, controlColor)) {
                    _debugger.setExecMode(ExecMode::eSTEPOVER);
                    if(_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eDEBUGGER;
                    }
                }
                GuiEnable();
                SetTooltip("STEP OVER");
                if (iconButton(ICON_STEP_INTO, _chipEmu->getExecMode() == ExecMode::eSTEP, controlBack, controlColor)) {
                    _debugger.setExecMode(ExecMode::eSTEP);
                    if(_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eDEBUGGER;
                    }
                }
                SetTooltip("STEP INTO");
                if(!_debugger.supportsStepOver())
                    GuiDisable();
                if (iconButton(ICON_STEP_OUT, _chipEmu->getExecMode() == ExecMode::eSTEPOUT, controlBack, controlColor)) {
                    _debugger.setExecMode(ExecMode::eSTEPOUT);
                    if(_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eDEBUGGER;
                    }
                }
                GuiEnable();
                SetTooltip("STEP OUT");
                if (iconButton(ICON_RESTART)) {
                    reloadRom();
                    resetStats();
                    if(_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eDEBUGGER;
                    }
                }
                SetTooltip("RESTART");
                int buttonsRight = 6;
#ifdef WITH_EDITOR
                ++buttonsRight;
#endif
                int avail = 202;
#ifdef RESIZABLE_GUI
                --buttonsRight;
                avail += 10;
#endif
                auto spacePos = GetCurrentPos();
                auto spaceWidth = avail - buttonsRight * 20;
                Space(spaceWidth);
                if(_options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP)
                    GuiDisable();
                if (iconButton(ICON_BOX_GRID, _grid))
                    _grid = !_grid;
                GuiEnable();
                SetTooltip("TOGGLE GRID");
                Space(10);
                if (iconButton(ICON_ZOOM_ALL, _mainView == eVIDEO))
                    _mainView = eVIDEO;
                SetTooltip("FULL VIDEO");
                if (iconButton(ICON_CPU, _mainView == eDEBUGGER))
                    _mainView = eDEBUGGER;
                SetTooltip("DEBUGGER");
#ifdef WITH_EDITOR
                if (iconButton(ICON_FILETYPE_TEXT, _mainView == eEDITOR))
                    _mainView = eEDITOR, _chipEmu->setExecMode(ExecMode::ePAUSED);
                SetTooltip("EDITOR");
#endif
                if (iconButton(ICON_PRINTER, _mainView == eTRACELOG))
                    _mainView = eTRACELOG;
                SetTooltip("TRACE-LOG");
                if (iconButton(ICON_GEAR, _mainView == eSETTINGS))
                    _mainView = eSETTINGS;
                SetTooltip("SETTINGS");

                static Vector2 versionSize = MeasureTextEx(GuiGetFont(), "v" CADMIUM_VERSION, 8, 0);
                DrawTextEx(GuiGetFont(), "v" CADMIUM_VERSION, {spacePos.x + (spaceWidth - versionSize.x) / 2, spacePos.y + 6}, 8, 0, WHITE);
#ifndef RESIZABLE_GUI
                Space(10);
                if (iconButton(ICON_HIDPI, _scaleBy2))
                    _scaleBy2 = !_scaleBy2;
                SetTooltip("TOGGLE ZOOM    ");
#endif
            }
            EndColumns();

            switch (_mainView) {
                case eDEBUGGER: {
                    _lastView = _mainView;
                    _debugger.render(_font, [this](Rectangle video, int scale){ drawScreen(video, scale); });
                    break;
                }
                case eVIDEO: {
                    _lastView = _mainView;
                    gridScale = _screenWidth / _chipEmu->getCurrentScreenWidth();
                    video = {0, 20, (float)_screenWidth, (float)_screenHeight - 36};
                    drawScreen(video, gridScale);
                    break;
                }
                case eEDITOR:
                    if(_lastView != eEDITOR)
                        _editor.setFocus();
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Editor", {1,1});
                    {
                        auto rect = GetContentAvailable();
#ifdef WITH_EDITOR
                        _editor.draw(_font, {rect.x, rect.y - 1, rect.width, rect.height});
#endif
                    }
                    EndPanel();
                    End();
                    break;
                case eTRACELOG: {
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Trace-Log", {1,1});
                    {
                        auto rect = GetContentAvailable();
                        _logView.draw(_font, {rect.x, rect.y - 1, rect.width, rect.height});
                    }
                    EndPanel();
                    End();
                    break;
                }
                case eSETTINGS: {
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Settings");
                    {
                        static int activeTab = 0;
                        BeginTabView(&activeTab);
                        if(BeginTab("Emulation", {5, 0})) {
                            emu::Chip8EmulatorOptions oldOptions = _options;
                            BeginColumns();
                            SetNextWidth(0.6f);
                            BeginGroupBox("Emulation Speed");
                            Space(5);
                            SetIndent(150);
                            SetRowHeight(20);
                            if(!_chipEmu->isGenericEmulation() || _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8TE)
                                GuiDisable();
                            Spinner("Instructions per frame", &_options.instructionsPerFrame, 0, 500000);
                            Spinner("Frame rate", &_options.frameRate, 10, 120);
                            if(!_chipEmu->isGenericEmulation() || _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8TE)
                                GuiEnable();
                            if (!_options.instructionsPerFrame) {
                                static int _fb1{1};
                                GuiDisable();
                                Spinner("Frame boost", &_fb1, 1, 100000);
                                GuiEnable();
                            }
                            else {
                                Spinner("Frame boost", &_frameBoost, 1, 100000);
                            }
                            g_frameBoost = getFrameBoost();
                            EndGroupBox();
                            Space(10);
                            //SetNextWidth(_screenWidth - 383);
                            Begin();
                            Label("CHIP-8 variant / Core:");
                            if(DropdownBox("CHIP-8;CHIP-8-STRICT;CHIP-10;CHIP-8X;CHIP-48;SCHIP 1.0;SCHIP 1.1;SCHIP-COMP;SCHIP-MODERN;MEGACHIP8;XO-CHIP;VIP-CHIP-8;VIP-CHIP-8 64x64;VIP-HI-RES-CHIP-8;VIP-CHIP-8X;VIP-CHIP-8X-64x64;VIP-HI-RES-CHIP-8X;CHIP-8 DREAM6800", &_behaviorSel)) {
                                auto preset = static_cast<emu::Chip8EmulatorOptions::SupportedPreset>(_behaviorSel);
                                _frameBoost = 1;
                                updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(preset));
                            }
                            if(!_chipEmu->isGenericEmulation()) {
                                auto rcb = dynamic_cast<emu::Chip8RealCoreBase*>(_chipEmu.get());
                                Label(TextFormat("   [%s based]", rcb->getBackendCpu().getName().c_str()));
                            }
                            Space(2);
                            _options.optTraceLog = CheckBox("Trace-Log", _options.optTraceLog);
                            End();
                            EndColumns();
                            Space(16);
                            if(!_chipEmu->isGenericEmulation() || _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8TE)
                                GuiDisable();
                            BeginGroupBox("Quirks");
                            Space(5);
                            BeginColumns();
                            SetNextWidth(GetContentAvailable().width/2);
                            Begin();
                            _options.optJustShiftVx = CheckBox("8xy6/8xyE just shift VX", _options.optJustShiftVx);
                            _options.optDontResetVf = CheckBox("8xy1/8xy2/8xy3 don't reset VF", _options.optDontResetVf);
                            bool oldInc = !(_options.optLoadStoreIncIByX | _options.optLoadStoreDontIncI);
                            bool newInc = CheckBox("Fx55/Fx65 increment I by X + 1", oldInc);
                            if(newInc != oldInc) {
                                _options.optLoadStoreIncIByX = !newInc;
                                _options.optLoadStoreDontIncI = false;
                            }
                            oldInc = _options.optLoadStoreIncIByX;
                            _options.optLoadStoreIncIByX = CheckBox("Fx55/Fx65 increment I only by X", _options.optLoadStoreIncIByX);
                            if(_options.optLoadStoreIncIByX != oldInc) {
                                _options.optLoadStoreDontIncI = false;
                            }
                            oldInc = _options.optLoadStoreDontIncI;
                            _options.optLoadStoreDontIncI = CheckBox("Fx55/Fx65 don't increment I", _options.optLoadStoreDontIncI);
                            if(_options.optLoadStoreDontIncI != oldInc) {
                                _options.optLoadStoreIncIByX = false;
                            }
                            _options.optJump0Bxnn = CheckBox("Bxnn/jump0 uses Vx", _options.optJump0Bxnn);
                            _options.optCyclicStack = CheckBox("Cyclic stack", _options.optCyclicStack);
                            _options.optXOChipSound = CheckBox("XO-CHIP sound engine", _options.optXOChipSound);
                            _options.optAllowColors = CheckBox("Multicolor support", _options.optAllowColors);
                            _options.optHas16BitAddr = CheckBox("Has 16 bit addresses", _options.optHas16BitAddr);
                            End();
                            Begin();
                            _options.optWrapSprites = CheckBox("Wrap sprite pixels", _options.optWrapSprites);
                            _options.optInstantDxyn = CheckBox("Dxyn doesn't wait for vsync", _options.optInstantDxyn);
                            bool oldLoresWidth = _options.optLoresDxy0Is8x16;
                            _options.optLoresDxy0Is8x16 = CheckBox("Lores Dxy0 draws 8 pixel width", _options.optLoresDxy0Is8x16);
                            if(!oldLoresWidth && _options.optLoresDxy0Is8x16)
                                _options.optLoresDxy0Is16x16 = false;
                            oldLoresWidth = _options.optLoresDxy0Is16x16;
                            _options.optLoresDxy0Is16x16 = CheckBox("Lores Dxy0 draws 16 pixel width", _options.optLoresDxy0Is16x16);
                            if(!oldLoresWidth && _options.optLoresDxy0Is16x16)
                                _options.optLoresDxy0Is8x16 = false;
                            bool oldVal = _options.optSC11Collision;
                            _options.optSC11Collision = CheckBox("Dxyn uses SCHIP1.1 collision", _options.optSC11Collision);
                            if(!oldVal && _options.optSC11Collision) {
                                _options.optAllowHires = true;
                            }
                            _options.optSCLoresDrawing = CheckBox("HP SuperChip lores drawing", _options.optSCLoresDrawing);
                            _options.optHalfPixelScroll = CheckBox("Half pixel scrolling", _options.optHalfPixelScroll);
                            bool oldAllowHires = _options.optAllowHires;
                            _options.optAllowHires = CheckBox("128x64 hires support", _options.optAllowHires);
                            if(!_options.optAllowHires && oldAllowHires) {
                                _options.optOnlyHires = false;
                                _options.optSC11Collision = false;
                            }
                            bool oldOnlyHires = _options.optOnlyHires;
                            _options.optOnlyHires = CheckBox("Only 128x64 mode", _options.optOnlyHires);
                            if(_options.optOnlyHires && !oldOnlyHires)
                                _options.optAllowHires = true;
                            _options.optModeChangeClear = CheckBox("Mode change clear", _options.optModeChangeClear);
                            End();
                            EndColumns();
                            EndGroupBox();
                            if(!_chipEmu->isGenericEmulation() || _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8TE)
                                GuiEnable();
                            Space(10);
                            {
                                StyleManager::Scope guard;
                                BeginColumns();
                                auto pos = GetCurrentPos();
                                SetNextWidth(52.0f + 16*18);
                                Label("Colors:");
                                for (int i = 0; i < 16; ++i) {
                                    DrawRectangle(pos.x + 52 + i * 18 + 2, pos.y + 2 , 12, 12, GetColor(_colorPalette[i]));
                                    bool hover =  CheckCollisionPointRec(GetMousePosition(), {pos.x + 52 + i * 18, pos.y, 16, 16});
                                    if(!GuiIsLocked() && IsMouseButtonReleased(0) && hover) {
                                        selectedColor = &_colorPalette[i];
                                        previousColor = _colorPalette[i];
                                        colorText = fmt::format("{:06x}", _colorPalette[i]>>8);
                                        colorSelectOpen = true;
                                    }
                                    DrawRectangleLines(pos.x + 52 + i * 18, pos.y, 16, 16, GetColor(guard.getStyle(hover ? Style::BORDER_COLOR_FOCUSED : Style::BORDER_COLOR_NORMAL)));
                                }
                                static std::vector<uint32_t> prevPalette(_colorPalette.begin(), _colorPalette.end());
                                if(std::memcmp(prevPalette.data(), _colorPalette.data(), 16*sizeof(uint32_t)) != 0) {
                                    setPalette({_colorPalette.begin(), _colorPalette.end()});
                                    prevPalette.assign(_colorPalette.begin(), _colorPalette.end());
                                }
                                static int sel = 5;

                                if(DropdownBox("Cadmium;Silicon-8;Pico-8;Octo Classic;LCD;Custom", &sel)) {
                                    switch(sel) {
                                        case 0:
                                            setPalette({
                                                0x1a1c2cff, 0xf4f4f4ff, 0x94b0c2ff, 0x333c57ff,
                                                0xb13e53ff, 0xa7f070ff, 0x3b5dc9ff, 0xffcd75ff,
                                                0x5d275dff, 0x38b764ff, 0x29366fff, 0x566c86ff,
                                                0xef7d57ff, 0x73eff7ff, 0x41a6f6ff, 0x257179ff
                                            });
                                            _defaultPalette = _colorPalette;
                                            sel = 5;
                                            break;
                                        case 1:
                                            setPalette({
                                                0x000000ff, 0xffffffff, 0xaaaaaaff, 0x555555ff,
                                                0xff0000ff, 0x00ff00ff, 0x0000ffff, 0xffff00ff,
                                                0x880000ff, 0x008800ff, 0x000088ff, 0x888800ff,
                                                0xff00ffff, 0x00ffffff, 0x880088ff, 0x008888ff
                                            });
                                            _defaultPalette = _colorPalette;
                                            sel = 5;
                                            break;
                                        case 2:
                                            setPalette({
                                                0x000000ff, 0xfff1e8ff, 0xc2c3c7ff, 0x5f574fff,
                                                0xef7d57ff, 0x00e436ff, 0x29adffff, 0xffec27ff,
                                                0xab5236ff, 0x008751ff, 0x1d2b53ff, 0xffa300ff,
                                                0xff77a8ff, 0xffccaaff, 0x7e2553ff, 0x83769cff
                                            });
                                            _defaultPalette = _colorPalette;
                                            sel = 5;
                                            break;
                                        case 3:
                                            setPalette({
                                                0x996600ff, 0xFFCC00ff, 0xFF6600ff, 0x662200ff,
                                                0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                                                0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                                                0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff
                                            });
                                            _defaultPalette = _colorPalette;
                                            sel = 5;
                                            break;
                                        case 4:
                                            setPalette({
                                                0xf2fff2ff, 0x5b8c7cff, 0xadd9bcff, 0x0d1a1aff,
                                                0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                                                0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                                                0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff
                                            });
                                            _defaultPalette = _colorPalette;
                                            sel = 5;
                                            break;
                                        default:
                                            break;
                                    }
                                    // SWEETIE-16:
                                    // {0x1a1c2c, 0xf4f4f4, 0x94b0c2, 0x333c57,  0xef7d57, 0xa7f070, 0x3b5dc9, 0xffcd75,  0xb13e53, 0x38b764, 0x29366f, 0x566c86,  0x41a6f6, 0x73eff7, 0x5d275d, 0x257179}
                                    // PICO-8:
                                    // {0x000000, 0xfff1e8, 0xc2c3c7, 0x5f574f,  0xff004d, 0x00e436, 0x29adff, 0xffec27,  0xab5236, 0x008751, 0x1d2b53, 0xffa300,  0xff77a8, 0xffccaa, 0x7e2553, 0x83769c}
                                    // C64:
                                    // {0x000000, 0xffffff, 0xadadad, 0x626262, 0xa1683c, 0x9ae29b, 0x887ecb, 0xc9d487, 0x9f4e44, 0x5cab5e, 0x50459b, 0x6d5412, 0xcb7e75, 0x6abfc6, 0xa057a3, 0x898989}
                                    // Intellivision:
                                    // {0x0c0005, 0xfffcff, 0xa7a8a8, 0x3c5800, 0xff3e00, 0x6ccd30, 0x002dff, 0xfaea27, 0xffa600, 0x00a720, 0xbd95ff, 0xc9d464, 0xff3276, 0x5acbff, 0xc81a7d, 0x00780f}
                                    // CGA
                                    // {0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff5555, 0x55ff55, 0x5555ff, 0xffff55, 0xaa0000, 0x00aa00, 0x0000aa, 0xaa5500, 0xff55ff, 0x55ffff, 0xaa00aa, 0x00aaaa}
                                    // Silicon-8 1.0
                                    // {0x000000, 0xffffff, 0xaaaaaa, 0x555555, 0xff0000, 0x00ff00, 0x0000ff, 0xffff00, 0x880000, 0x008800, 0x000088, 0x888800, 0xff00ff, 0x00ffff, 0x880088, 0x008888}
                                    // Macintosh II
                                    // {0x000000, 0xffffff, 0xb9b9b9, 0x454545, 0xdc0000, 0x00a800, 0x0000ca, 0xffff00, 0xff6500, 0x006500, 0x360097, 0x976536, 0xff0097, 0x0097ff, 0x653600, 0x868686}
                                    // IBM PCjr
                                    // {0x1c2536, 0xced9ed, 0x81899e, 0x030625, 0xe85685, 0x2cc64e, 0x0000e8, 0xa7c251, 0x9f2441, 0x077c35, 0x0e59f0, 0x4b7432, 0xc137ff, 0x0bc3a9, 0x6b03ca, 0x028566}
                                    // Daylight-16
                                    // {0x272223, 0xf2d3ac, 0xe7a76c, 0x6a422c, 0xb55b39, 0xb19e3f, 0x7a6977, 0xf8c65c, 0x996336, 0x606b31, 0x513a3d, 0xd58b39, 0xc28462, 0xb5c69a, 0x905b54, 0x878c87}
                                    // Soul of the Sea
                                    // {0x01141a, 0xcfbc95, 0x93a399, 0x2f4845, 0x92503f, 0x949576, 0x425961, 0x81784d, 0x703a28, 0x7a7e67, 0x203633, 0x605f33, 0x56452b, 0x467e73, 0x403521, 0x51675a}

                                }
                                EndColumns();
                            }
                            Space(8);
                            if(oldOptions != _options) {
                                updateEmulatorOptions(_options);
                                saveConfig();
                            }
                            BeginColumns();
                            Space(100);
                            SetNextWidth(0.21f);
                            bool romRemembered = _cfg.romConfigs.count(_romSha1Hex) > 0;
                            if((romRemembered && _options == _cfg.romConfigs[_romSha1Hex]) || (_romIsWellKnown && _options == _romWellKnownOptions)) {
                                GuiDisable();
                            }
                            if (Button(!romRemembered ? "Remember for ROM" : "Update for ROM")) {
                                _cfg.romConfigs[_romSha1Hex] = _options;
                                saveConfig();
                            }
                            GuiEnable();
                            if(!romRemembered)
                                GuiDisable();
                            SetNextWidth(0.21f);
                            if(Button("Forget ROM")) {
                                _cfg.romConfigs.erase(_romSha1Hex);
                                saveConfig();
                            }
                            GuiEnable();
                            EndColumns();
                            auto pos = GetCurrentPos();
                            Space(_screenHeight - pos.y - 20 - 1);
                            //SetIndent(110);
                            //Label("(C) 2022 by Steffen '@gulrak' Schümann");
                            EndTab();
                        }
                        if(BeginTab("Appearance", {5, 0})) {
                            Label("[Not implemented yet.]");
                            auto pos = GetCurrentPos();
                            Space(_screenHeight - pos.y - 20 - 1);
                            EndTab();
                        }
                        if(BeginTab("Misc", {5, 0})) {
                            Space(3);
                            Label("Config directory:");
                            GuiDisable();
                            TextBox(_cfgPath, 4096);
                            GuiEnable();
                            Label("CHIP-8 database directory:");
                            if(TextBox(_databaseDirectory, 4096)) {
                                saveConfig();
                            }
                            auto pos = GetCurrentPos();
                            Space(_screenHeight - pos.y - 20 - 1);
                            EndTab();
                        }
                        EndTabView();
                    }
                    EndPanel();
                    End();
                    break;
                }
#ifndef PLATFORM_WEB
                case eROM_SELECTOR: {
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Load/Import ROM or Octo Source");
                    {
                        renderFileBrowser(eLOAD);
                    }
                    EndPanel();
                    End();
                    if(IsKeyPressed(KEY_ESCAPE))
                        _mainView = _lastView;
                    break;
                }
#else
                case eROM_SELECTOR:
                    break;
#endif // !PLATFORM_WEB
                case eROM_EXPORT: {
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Save/Export ROM or Source");
                    {
#ifdef PLATFORM_WEB
                        renderFileBrowser(eWEB_SAVE);
#else
                        renderFileBrowser(eSAVE);
#endif
                    }
                    EndPanel();
                    End();
                    if(IsKeyPressed(KEY_ESCAPE))
                        _mainView = _lastView;
                    break;
                }
            }

            if(colorSelectOpen) {
                colorSelectOpen = !BeginWindowBox({-1, -1, 200, 250}, "Select Color", &colorSelectOpen, WindowBoxFlags(WBF_MOVABLE | WBF_MODAL));
                uint32_t prevCol = *selectedColor;
                *selectedColor = ColorToInt(ColorPicker(GetColor(*selectedColor)));
                if(*selectedColor != prevCol) {
                    colorText = fmt::format("{:06x}", *selectedColor>>8);
                }
                Space(5);
                BeginColumns();
                SetNextWidth(40);
                Label("Color:");
                SetNextWidth(60);
                if(TextBox(colorText, 7)) {
                    *selectedColor = (std::strtoul(colorText.c_str(), nullptr, 16)<<8)+255;
                }
                EndColumns();
                Space(5);
                BeginColumns();
                Space(30);
                SetNextWidth(60);
                if(Button("Ok")) {
                    _defaultPalette = _colorPalette;
                    selectedColor = nullptr;
                    colorSelectOpen = false;
                }
                SetNextWidth(60);
                if(Button("Cancel") || IsKeyPressed(KEY_ESCAPE)) {
                    *selectedColor = previousColor;
                    selectedColor = nullptr;
                    colorSelectOpen = false;
                }
                EndColumns();
                EndWindowBox();
            }
            EndGui();
        }
        if(_chipEmu->getExecMode() != ExecMode::ePAUSED) {
            _instructionOffset = -1;
            _debugger.captureStates();
        }
    }
    void showGenericRegs(const int lineSpacing, const Vector2& pos) const
    {
        auto* rcb = dynamic_cast<emu::Chip8RealCoreBase*>(_chipEmu.get());
        auto& cpu = rcb->getBackendCpu();
        int i, line = 0, lastSize = 0;
        for (i = 0; i < cpu.getNumRegisters(); ++i, ++line) {
            auto reg = cpu.getRegister(i);
            if(i && reg.size != lastSize)
                ++line;
            switch(reg.size) {
                case 1:
                case 4:
                    DrawTextEx(_font, TextFormat("%2s: %X", cpu.getRegisterNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, LIGHTGRAY);
                    break;
                case 8:
                    DrawTextEx(_font, TextFormat("%2s: %02X", cpu.getRegisterNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, LIGHTGRAY);
                    break;
                case 12:
                    DrawTextEx(_font, TextFormat("%2s: %03X", cpu.getRegisterNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, LIGHTGRAY);
                    break;
                case 16:
                    DrawTextEx(_font, TextFormat("%2s:%04X", cpu.getRegisterNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, LIGHTGRAY);
                    break;
                default:
                    DrawTextEx(_font, TextFormat("%2s:%X", cpu.getRegisterNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, MAGENTA);
                    break;
            }
            lastSize = reg.size;
        }
        ++line;
        DrawTextEx(_font, TextFormat("Scr: %s", rcb->isDisplayEnabled() ? "ON" : "OFF"), {pos.x, pos.y + line * lineSpacing}, 8, 0, LIGHTGRAY);
    }

    void renderFileBrowser(FileBrowserMode mode)
    {
        using namespace gui;
        static Vector2 scroll{0,0};
        static Librarian::Info selectedInfo;
        SetRowHeight(16);
        auto area = GetContentAvailable();
#ifdef PLATFORM_WEB
        Space(area.height - 54);
#else
        if(TextBox(_currentDirectory, 4096)) {
            _librarian.fetchDir(_currentDirectory);
            _currentDirectory = _librarian.currentDirectory();
        }
        Space(1);
        BeginTableView(area.height - 135, 4, &scroll);
        for(int i = 0; i < _librarian.numEntries(); ++i) {
            const auto& info = _librarian.getInfo(i);
            auto rowCol = Color{0,0,0,0};
            if(info.analyzed) {
                //if(info.type == Librarian::Info::eROM_FILE)
                //    rowCol = Color{0,128,128,32}; //ColorAlpha(GetColor(GetStyle(DEFAULT, BASE_COLOR_NORMAL)), 64);
            }
            else {
                rowCol = Color{0,128,0,10};
            }
            TableNextRow(16, rowCol);
            if(TableNextColumn(24)) {
                int icon = ICON_HELP2;
                switch (info.type) {
                    case Librarian::Info::eDIRECTORY: icon = ICON_FOLDER_OPEN; break;
                    case Librarian::Info::eROM_FILE: icon = ICON_ROM; break;
                    case Librarian::Info::eOCTO_SOURCE: icon = ICON_FILETYPE_TEXT; break;
                    default: icon = ICON_FILE_DELETE; break;
                }
                auto oldFG = gui::GetStyle(LABEL, TEXT_COLOR_NORMAL);
                {
                    StyleManager::Scope guard;
                    if (info.type == Librarian::Info::eROM_FILE)
                        guard.setStyle(Style::TEXT_COLOR_NORMAL, info.isKnown ? GREEN : YELLOW);
                    //    gui::SetStyle(LABEL, TEXT_COLOR_NORMAL, info.isKnown ? ColorToInt(GREEN) : ColorToInt(YELLOW));
                    Label(GuiIconText(icon, ""));
                    //gui::SetStyle(LABEL, TEXT_COLOR_NORMAL, oldFG);
                }
            }
            if(TableNextColumn(.66f)) {
                if(info.filePath.size() > 50 ? LabelButton(info.filePath.substr(0,50).c_str()) : LabelButton(info.filePath.c_str())) {
                    if(info.type == Librarian::Info::eDIRECTORY) {
                        if(info.filePath != "..") {
                            _librarian.intoDir(info.filePath);
                            _currentDirectory = _librarian.currentDirectory();
                            if(mode == eLOAD)
                                _currentFileName = "";
                        }
                        else {
                            _librarian.parentDir();
                            _currentDirectory = _librarian.currentDirectory();
                            if(mode == eLOAD)
                                _currentFileName = "";
                        }
                        selectedInfo.analyzed = false;
                        selectedInfo.isKnown = false;
                        break;
                    }
                    else if(info.type == Librarian::Info::eOCTO_SOURCE) {
                        selectedInfo = info;
                        _currentFileName = info.filePath;
                    }
                    else if(info.type == Librarian::Info::eROM_FILE) {
                        selectedInfo = info;
                        _currentFileName = info.filePath;
                    }
                }
            }
            if(TableNextColumn(.145f))
                Label(info.type == Librarian::Info::eDIRECTORY ? "" : fmt::format("{:>8s}", formatUnit(info.fileSize, "")).c_str());
            if(TableNextColumn(.13f) && info.filePath != "..")
                Label(date::format("%F", date::floor<std::chrono::seconds>(info.changeDate)).c_str());
        }
        EndTableView();
#endif
        Space(1);
        BeginColumns();
        SetNextWidth(25);
        Label("File:");
        TextBox(_currentFileName, 4096);
        EndColumns();
        Space(2);
        switch(mode) {
            case eLOAD: {
                auto infoPos = GetCurrentPos();
                Label(fmt::format("SHA1:  {}", selectedInfo.analyzed ? selectedInfo.sha1sum : "").c_str());
                if(!selectedInfo.analyzed || selectedInfo.isKnown) {
                    Label(fmt::format("Type:  {}", selectedInfo.analyzed ? emu::Chip8EmulatorOptions::nameOfPreset(selectedInfo.variant) : "").c_str());
                }
                else {
                    Label(fmt::format("Type:  {} (estimated)", selectedInfo.minimumOpcodeProfile()).c_str());
                }
                if(selectedInfo.analyzed) {
                    if(_screenShotSha1sum != selectedInfo.sha1sum) {
                        _screenshotData = _librarian.genScreenshot(selectedInfo, _defaultPalette);
                        _screenShotSha1sum = selectedInfo.sha1sum;
                        if(_screenshotData.width && _screenshotData.pixel.size() == _screenshotData.width * _screenshotData.height) {
                            auto* image = (uint32_t*)_screenShot.data;
                            for(int y = 0; y < _screenshotData.height; ++y) {
                                for(int x = 0; x < _screenshotData.width; ++x) {
                                    image[y * _screenShot.width + x] = _screenshotData.pixel[y * _screenshotData.width + x];
                                }
                            }
                            UpdateTexture(_screenShotTexture, _screenShot.data);
                        }
                    }
                    if(_screenShotSha1sum == selectedInfo.sha1sum && _screenshotData.width) {
                        DrawTexturePro(_screenShotTexture, {0, 0, (float)_screenshotData.width, (float)_screenshotData.height}, {300, infoPos.y + 2, 192, 96}, {0,0}, 0, WHITE);
                        DrawRectangleLinesEx({299, infoPos.y + 1, 194, 98}, 1, GetColor(GetStyle(DEFAULT, BORDER_COLOR_NORMAL)));
                    }
                }
                Space(3);
                BeginColumns();
                Space(32);
                SetNextWidth(80);
                if(!selectedInfo.analyzed) GuiDisable();
                if(Button("Load") && selectedInfo.analyzed) {
                    //if(selectedInfo.variant != _options.behaviorBase && selectedInfo.variant != emu::Chip8EmulatorOptions::eCHIP8) {
                    //    updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(selectedInfo.variant));
                    //}
                    loadRom(_librarian.fullPath(selectedInfo.filePath).c_str(), false);
                    _mainView = _lastView;
                }
                SetNextWidth(110);
                if(Button("Load w/o Config") && selectedInfo.analyzed) {
                    //if(selectedInfo.variant != _options.behaviorBase && selectedInfo.variant != emu::Chip8EmulatorOptions::eCHIP8) {
                    //    updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(selectedInfo.variant));
                    //}
                    auto options = _options;
                    loadRom(_librarian.fullPath(selectedInfo.filePath).c_str(), false);
                    updateEmulatorOptions(options);
                    _mainView = _lastView;
                }
                GuiEnable();
                EndColumns();
                break;
            }
            case eWEB_SAVE:
            case eSAVE: {
                BeginColumns();
                SetNextWidth(100);
                Label("Select file type:");
                static int activeType = 0;
                SetNextWidth(70);
                activeType = ToggleGroup("ROM File;Source Code", activeType);
                EndColumns();
                Space(3);
                SetNextWidth(80);
                SetIndent(32);
                if(_currentFileName.empty() && ((activeType == 0 && _romImage.empty()) || (activeType == 1 && _editor.getText().empty()))) GuiDisable();
                if(Button("Save") && !_currentFileName.empty()) {
                    if (activeType == 0 && fs::path(_currentFileName).extension() != romExtension()) {
                        if (fs::path(_currentFileName).has_extension())
                            _currentFileName = fs::path(_currentFileName).replace_extension(romExtension()).string();
                        else
                            _currentFileName += romExtension();
                    }
                    else if (activeType == 1 && fs::path(_currentFileName).extension() != ".8o") {
                        if (fs::path(_currentFileName).has_extension())
                            _currentFileName = fs::path(_currentFileName).replace_extension(".8o").string();
                        else
                            _currentFileName += ".8o";
                    }
#ifdef PLATFORM_WEB
                    auto targetFile = _currentFileName;
#else
                    auto targetFile = _librarian.fullPath(_currentFileName);
#endif
                    if(activeType == 0) {
                        writeFile(targetFile, (const char*)_romImage.data(), _romImage.size());
                    }
                    else {
                        writeFile(targetFile, _editor.getText().data(), _editor.getText().size());
                    }
#ifdef PLATFORM_WEB
                    // can only use path-less filenames
                    emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", targetFile.c_str(), targetFile.c_str()));
#endif
                    _mainView = _lastView;
                }
                GuiEnable();
                break;
            }
        }
        BeginColumns();
        EndColumns();
        auto pos = GetCurrentPos();
        Space(_screenHeight - pos.y - 20 - 1);
    }

#ifdef PLATFORM_WEB
    void loadFileWeb()
    {
        //-------------------------------------------------------------------------------
        // This file upload dialog is heavily inspired by MIT licensed code from
        // https://github.com/daid/SeriousProton2 - specifically:
        //
        // https://github.com/daid/SeriousProton2/blob/f13f32336360230788054822183049a0153c0c07/src/io/fileSelectionDialog.cpp#L67-L102
        //
        // All Rights Reserved.
        //
        // Permission is hereby granted, free of charge, to any person obtaining a copy
        // of this software and associated documentation files (the "Software"), to deal
        // in the Software without restriction, including without limitation the rights
        // to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
        // copies of the Software, and to permit persons to whom the Software is
        // furnished to do so, subject to the following conditions:
        //
        // The above copyright notice and this permission notice shall be included in
        // all copies or substantial portions of the Software.
        //
        // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
        // IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
        // FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
        // AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
        // LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
        // OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
        // THE SOFTWARE.
        //-------------------------------------------------------------------------------
        openFileCallback = [&](const std::string& filename)
        {
            loadRom(filename.c_str(), false);
        };
        EM_ASM({
            if (typeof(open_file_element) == "undefined")
            {
                open_file_element = document.createElement('input');
                open_file_element.type = "file";
                open_file_element.style.display = "none";
                document.body.appendChild(open_file_element);
                open_file_element.addEventListener("change", function() {
                    var filename = "/upload/" + this.files[0].name;
                    var name = this.files[0].name;
                    this.files[0].arrayBuffer().then(function(buffer) {
                         try { FS.unlink(filename); } catch (exception) { }
                         FS.createDataFile("/upload/", name, new Uint8Array(buffer), true, true, true);
                         var stack = Module.stackSave();
                         var name_ptr = Module.stackAlloc(filename.length * 4 + 1);
                         stringToUTF8(filename, name_ptr, filename.length * 4 + 1);
                         Module._openFileCallbackC(name_ptr);
                         stackRestore(stack);
                        });
                    }, false
                );
                FS.mkdir("/upload");
            }
            open_file_element.value = "";
            open_file_element.accept = '.ch8,.ch10,.hc8,.sc8,.xo8,.c8b,.8o';
            open_file_element.click();
        });
    }
#endif

    const std::string& romExtension()
    {
        static std::string extensions[] = {".ch8", ".sc10", ".sc8", ".mc8", ".xo8", ".c8h"};
        switch(_options.behaviorBase) {
            case emu::Chip8EmulatorOptions::eCHIP10: return extensions[1];
            case emu::Chip8EmulatorOptions::eSCHIP10:
            case emu::Chip8EmulatorOptions::eSCHIP11: return extensions[2];
            case emu::Chip8EmulatorOptions::eMEGACHIP: return extensions[3];
            case emu::Chip8EmulatorOptions::eXOCHIP: return extensions[4];
            case emu::Chip8EmulatorOptions::eCHIP8VIP_TPD: return extensions[5];
            default: return extensions[0];
        }
    }

    void saveConfig()
    {
#ifndef PLATFORM_WEB
        if(!_cfgPath.empty()) {
            auto opt = _options.clone();
            std::vector<std::string> pal(16, "");
            for(size_t i = 0; i < 16; ++i) {
                pal[i] = fmt::format("#{:06x}", _defaultPalette[i] >> 8);
            }
            opt.advanced["palette"] = pal;
            _cfg.emuOptions = opt;
            _cfg.workingDirectory = _currentDirectory;
            _cfg.databaseDirectory = _databaseDirectory;
            if(!_cfg.save(_cfgPath)) {
                TraceLog(LOG_ERROR, "Couldn't write config to '%s'", _cfgPath.c_str());
            }
        }
#endif
    }

    void whenEmuChanged(emu::IChip8Emulator& emu) override
    {
        _debugger.updateCore(&emu);
        _editor.updateCompilerOptions(_options.startAddress);
        reloadRom();
        _behaviorSel = _options.behaviorBase != emu::Chip8EmulatorOptions::eCHICUEYI ? _options.behaviorBase : emu::Chip8EmulatorOptions::eXOCHIP;
        resetStats();
    }

    void resetStats()
    {
        _ipfAverage.reset();
        _frameTimeAverage_us.reset();
        _frameDelta.reset();
        updateScreen();
    }

    void whenRomLoaded(const std::string& filename, bool autoRun, emu::OctoCompiler* compiler, const std::string& source) override
    {
        _logView.clear();
        _audioBuffer.reset();
        _frameBoost = 1;
        _behaviorSel = _options.behaviorBase != emu::Chip8EmulatorOptions::eCHICUEYI ? _options.behaviorBase : emu::Chip8EmulatorOptions::eXOCHIP;
#ifdef WITH_EDITOR
        _editor.setText(source);
        _editor.setFilename(filename);
#endif
        resetStats();
        if(compiler)
            _debugger.updateOctoBreakpoints(*compiler);
        saveConfig();
        if(autoRun)
            _mainView = eVIDEO;
    }

    void reloadRom()
    {
        if(!_romImage.empty()) {
            unsigned int size = 0;
            _chipEmu->reset();
            _audioBuffer.reset();
            updateScreen();
            _instructionOffset = -1;
            if(Librarian::isPrefixedTPDRom(_romImage.data(), _romImage.size()))
                std::memcpy(_chipEmu->memory() + 512, _romImage.data(), std::min(_romImage.size(),size_t(_chipEmu->memSize() - 512)));
            else
                std::memcpy(_chipEmu->memory() + _options.startAddress, _romImage.data(), std::min(_romImage.size(),size_t(_chipEmu->memSize() - _options.startAddress)));
        }
        _debugger.captureStates();
    }

    bool windowShouldClose() const
    {
        return _shouldClose || WindowShouldClose();
    }

private:
    std::mutex _audioMutex;
    ResourceManager _resources;
    StyleManager _styleManager;
    Image _fontImage{};
    Image _microFont{};
    Image _titleImage{};
    Image _icon{};
    Font _font{};
    Image _screen{};
    Image _crt{};
    Image _screenShot{};
    Texture2D _titleTexture{};
    Texture2D _screenTexture{};
    Texture2D _crtTexture{};
    Texture2D _screenShotTexture{};
    Librarian::Screenshot _screenshotData;
    std::string _screenShotSha1sum;
    RenderTexture _keyboardOverlay{};
    CircularBuffer<int16_t,1> _audioBuffer;
    bool _shouldClose{false};
    bool _showKeyMap{false};
    int _screenWidth{};
    int _screenHeight{};
    RenderTexture _renderTexture{};
    AudioStream _audioStream{};
    SMA<60,uint64_t> _ipfAverage;
    SMA<120,uint32_t> _frameTimeAverage_us;
    SMA<120,int> _frameDelta;
#ifndef RESIZABLE_GUI
    bool _scaleBy2{false};
#endif
    int _behaviorSel{0};
    //float _messageTime{};
    std::string _timedMessage;
    bool _renderCrt{false};
    bool _updateScreen{false};
    int _frameBoost{1};
    int _memoryOffset{-1};
    int _instructionOffset{-1};

    //std::string _romName;
    //std::vector<uint8_t> _romImage;
    //std::string _romSha1Hex;
    //bool _romIsWellKnown{false};
    //emu::Chip8EmulatorOptions _romWellKnownOptions;
    std::array<double,16> _keyScanTime{};
    std::array<bool,16> _keyMatrix;
    volatile bool _grid{false};
    MainView _mainView{eDEBUGGER};
    MainView _lastView{eDEBUGGER};
    Debugger _debugger;
    LogView _logView;
#ifdef WITH_EDITOR
    Editor _editor;
#endif

    inline static KeyboardKey _keyMapping[16] = {KEY_X, KEY_ONE, KEY_TWO, KEY_THREE, KEY_Q, KEY_W, KEY_E, KEY_A, KEY_S, KEY_D, KEY_Z, KEY_C, KEY_FOUR, KEY_R, KEY_F, KEY_V};
    inline static int _keyPosition[16] = {1,2,3,12, 4,5,6,13, 7,8,9,14, 10,0,11,15};
    inline static Cadmium* _instance{};
};

#ifndef PLATFORM_WEB
std::string dumOctoStateLine(octo_emulator* octo)
{
    return fmt::format("V0:{:02x} V1:{:02x} V2:{:02x} V3:{:02x} V4:{:02x} V5:{:02x} V6:{:02x} V7:{:02x} V8:{:02x} V9:{:02x} VA:{:02x} VB:{:02x} VC:{:02x} VD:{:02x} VE:{:02x} VF:{:02x} I:{:04x} SP:{:1x} PC:{:04x} O:{:04x}",
    octo->v[0], octo->v[1], octo->v[2], octo->v[3], octo->v[4], octo->v[5], octo->v[6], octo->v[7], 
    octo->v[8], octo->v[9], octo->v[10], octo->v[11], octo->v[12], octo->v[13], octo->v[14], octo->v[15],
    octo->i, octo->rp, octo->pc, (octo->ram[octo->pc]<<8)|octo->ram[octo->pc+1]);
}
#endif

std::string chip8EmuScreen(emu::IChip8Emulator& chip8)
{
    std::string result;
    auto width = chip8.getCurrentScreenWidth();
    auto maxWidth = 256; //chip8.getMaxScreenWidth();
    auto height = chip8.getCurrentScreenHeight();
    const auto* screen = chip8.getScreen();
    if(screen) {
        result.reserve(width * height + height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                result.push_back(screen->getPixel(x, y) ? '#' : '.');
            }
            result.push_back('\n');
        }
    }
    return result;
}

std::string chip8EmuScreenANSI(emu::IChip8Emulator& chip8)
{
    static int col[16] = {0,15,7,8, 9,10,12,11, 1,2,4,3, 13,14,5,6};
    std::string result;
    auto width = chip8.getCurrentScreenWidth();
    auto maxWidth = 256; //chip8.getMaxScreenWidth();
    auto height = chip8.getCurrentScreenHeight();
    const auto* screen = chip8.getScreen();
    if(screen) {
        result.reserve(width * height * 16);
        if (chip8.isDoublePixel()) {
            for (int y = 0; y < height; y += 4) {
                for (int x = 0; x < width; x += 2) {
                    auto c1 = screen->getPixel(x, y);
                    auto c2 = screen->getPixel(x, y + 2);
                    result += fmt::format("\033[38;5;{}m\033[48;5;{}m\xE2\x96\x84", col[c2 & 15], col[c1 & 15]);
                    // result.push_back(buffer[y*maxWidth + x] ? '#' : '.');
                }
                result += "\033[0m\n";
            }
        }
        else {
            for (int y = 0; y < height; y += 2) {
                for (int x = 0; x < width; ++x) {
                    auto c1 = screen->getPixel(x, y);
                    auto c2 = screen->getPixel(x, y + 1);
                    result += fmt::format("\033[38;5;{}m\033[48;5;{}m\xE2\x96\x84", col[c2 & 15], col[c1 & 15]);
                    // result.push_back(buffer[y*maxWidth + x] ? '#' : '.');
                }
                result += "\033[0m\n";
            }
        }
    }
    return result;
}

#ifndef PLATFORM_WEB
std::string octoScreen(octo_emulator& octo)
{
    std::string result;
    result.reserve(65*32+1);
    for(int y = 0; y < 32; ++y) {
        for(int x = 0; x < 64; ++x) {
            result.push_back(octo.px[y*64 + x] ? '#' : ' ');
        }
        result.push_back('\n');
    }
    return result;
}
#endif

#ifdef PLATFORM_WEBx
extern "C" {

EMSCRIPTEN_KEEPALIVE int load_file(uint8_t *buffer, size_t size) {
    /// Load a file - this function is called from javascript when the file upload is activated
    std::cout << "load_file triggered, buffer " << &buffer << " size " << size << std::endl;

    // do whatever you need with the file contents

    return 1;
}

}
#endif

std::string formatOpcodeString(emu::OpcodeType type, uint16_t opcode)
{
    static std::string patterns[] = {"FFFF", "FFFn", "FFnn", "Fnnn", "FxyF", "FxFF", "Fxyn", "Fxnn", "FFyF"};
    //                              0xFFFF, 0xFFF0, 0xFF00, 0xF000, 0xF00F, 0xF0FF, 0xF000, 0xF000, 0xFF0F
    auto opStr = fmt::format("{:04X}", opcode);
    for(size_t i = 0; i <4; ++i) {
        if(std::islower((uint8_t)patterns[type][i]))
            opStr[i] = patterns[type][i];
    }
    return opStr;
}

std::string formatOpcode(emu::OpcodeType type, uint16_t opcode)
{
    auto opStr = formatOpcodeString(type, opcode);
    auto dst = opStr;
    std::transform(dst.begin(), dst.end(), dst.begin(), [](unsigned char c){ return std::tolower(c); });
    return fmt::format("<a href=\"https://chip8.gulrak.net/reference/opcodes/{}\">{}</a>", dst, opStr);
}

void dumpOpcodeTable(std::ostream& os, emu::Chip8Variant variants = (emu::Chip8Variant)0x3FFFFFFFFFFFFFFF)
{
    std::regex quirkRE(R"(\s*\[Q:([^\]]+)\])");
    std::map<std::string, size_t> quirkMap;
    std::vector<std::string> quirkList;
    os << R"(<!DOCTYPE html><html><head><title>CHIP-8 Variant Opcode Table</title>
<style>
body { background: #1b1b1f; color: azure; font-family: Verdana, sans-serif; }
a { color: #8bf; }
table { border: 2px solid #ccc; border-collapse: collapse; }
th { border: 2px solid #ccc; padding: 0.5em; }
td { text-align: center; border: 2px solid #ccc; padding: 0.5em; }
td.clean { background-color: #080; }
td.quirked { background-color: #880; }
td.desc { text-align: left; }
th.rotate { height: 100px; white-space: nowrap; }
th.rotate > div { transform: translate(0px, 2em) rotate(-90deg); width: 30px; }
div.footer { font-size: 0.7em; }
</style></head>
<body><h2>CHIP-8 Variant Opcode Table</h2>
<table class="opcodes"><tr><th class="opcodes">Opcode</th>)";
    auto mask = static_cast<uint64_t>(variants);
    while(mask) {
        auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
        mask &= mask - 1;
        os << R"(<th class="rotate"><div><span>)" << emu::Chip8Decompiler::chipVariantName(cv).first << "</span></div></th>";
    }
    os << "<th>Description</th></tr>";
    for(const auto& info : emu::detail::opcodes) {
        if(uint64_t(info.variants & variants) != 0) {
            os << "<tr><th>" << formatOpcode(info.type, info.opcode) << "</th>";
            mask = static_cast<uint64_t>(variants);
            auto desc = info.description;
            std::smatch m;
            size_t qidx = 0;
            while (std::regex_search(desc, m, quirkRE)) {
                auto iter = quirkMap.find(m[1]);
                if (iter == quirkMap.end()) {
                    quirkMap.emplace(m[1], quirkList.size() + 1);
                    quirkList.push_back(m[1]);
                    qidx = quirkList.size();
                }
                else
                    qidx = iter->second;
                desc = desc.replace(m[0].first, m[0].second, fmt::format(" [<a href=\"#quirk{}\">Quirk {}</a>]", qidx, qidx));
            }
            while (mask) {
                auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
                mask &= mask - 1;
                if ((info.variants & cv) == cv) {
                    if (qidx)
                        os << "<td class=\"quirked\">&#x2713;</td>";
                    else
                        os << "<td class=\"clean\">&#x2713;</td>";
                }
                else
                    os << "<td></td>";
            }
            os << R"(<td class="desc">)" << desc << "</td></tr>" << std::endl;
        }
    }
    os << "</table>\n<ul>";
    size_t qidx = 1;
    for(const auto& quirk : quirkList) {
        os << "<li id=\"quirk" << qidx << "\"> Quirk " << qidx << ": " << quirk << "</li>\n";
        ++qidx;
    }
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    os << "</ul><div class=\"footer\">Generated by Cadmium v" << CADMIUM_VERSION << ", on " << std::put_time( std::gmtime( &t ), "%F" ) << "</div></body></html>";
}

void dumpOpcodeJSON(std::ostream& os, emu::Chip8Variant variants = (emu::Chip8Variant)0x3FFFFFFFFFFFFFFF)
{
    using namespace nlohmann;
    ordered_json root = ordered_json::object({});
    ordered_json collection = json::array({});
    std::regex quirkRE(R"(\s*\[Q:([^\]]+)\])");
    std::map<std::string, size_t> quirkMap;
    std::vector<std::string> quirkList;
    for(const auto& info : emu::detail::opcodes) {
        if(uint64_t(info.variants & variants) != 0) {
            auto obj = ordered_json::object({});
            obj["opcode"] = formatOpcodeString(info.type, info.opcode);
            obj["mask"] = emu::detail::opcodeMasks[info.type];
            obj["size"] = info.size;
            obj["octo"] = info.octo;
            auto mnemonic = info.octo.substr(0, info.octo.find(" "));
            if(emu::detail::octoMacros.count(mnemonic)) {
                obj["macro"] = emu::detail::octoMacros.at(mnemonic);
            }
            if(!info.mnemonic.empty()) {
                obj["chipper"] = info.mnemonic;
            }
            obj["platforms"] = json::array();
            auto mask = static_cast<uint64_t>(variants & info.variants);
            while(mask) {
                auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
                mask &= mask - 1;
                obj["platforms"].push_back(emu::Chip8Decompiler::chipVariantName(cv).first);
            }
            auto desc = info.description;
            std::smatch m;
            size_t qidx = 0;
            ordered_json quirks = json::array({});
            while (std::regex_search(desc, m, quirkRE)) {
                auto iter = quirkMap.find(m[1]);
                if (iter == quirkMap.end()) {
                    quirkMap.emplace(m[1], quirkList.size());
                    qidx = quirkList.size();
                    quirkList.push_back(trim(m[1]));
                }
                else
                    qidx = iter->second;
                quirks.push_back(qidx);
                desc = desc.replace(m[0].first, m[0].second, "");
            }
            obj["description"] = trim(desc);
            if(!quirks.empty())
                obj["quirks"] = quirks;
            collection.push_back(obj);
        }
    }
    root["generator"] = "Cadmium";
    root["version"] = CADMIUM_VERSION " " CADMIUM_GIT_HASH;
    std::stringstream oss;
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    oss << std::put_time( std::gmtime( &t ), "%F" );
    root["date"] = oss.str();
    root["opcodes"] = collection;
    root["quirks"] = json(quirkList);
    os << root.dump() << std::endl;
}

int main(int argc, char* argv[])
{
    auto preset = emu::Chip8EmulatorOptions::eXOCHIP;
#ifndef PLATFORM_WEB
    ghc::CLI cli(argc, argv);
    int64_t traceLines = -1;
    bool compareRun = false;
    int64_t benchmark= 0;
    bool showHelp = false;
    bool opcodeTable = false;
    bool opcodeJSON = false;
    bool startRom = false;
    bool screenDump = false;
    std::string dumpInterpreter;
    emu::Chip8EmulatorOptions options;
    int64_t execSpeed = -1;
    std::string randomGen;
    int64_t randomSeed = 12345;
    std::vector<std::string> romFile;
    std::string presetName;
    cli.category("General Options");
    cli.option({"-h", "--help"}, showHelp, "Show this help text");
    cli.option({"-t", "--trace"}, traceLines, "Run headless and dump given number of trace lines");
    cli.option({"-c", "--compare"}, compareRun, "Run and compare with reference engine, trace until diff");
    cli.option({"-r", "--run"}, startRom, "if a ROM is given (positional) start it");
    cli.option({"-b", "--benchmark"}, benchmark, "Run given number of cycles as benchmark");
    cli.option({"-p", "--preset"}, presetName, "Select CHIP-8 preset to use: chip-8, chip-10, chip-48, schip1.0, schip1.1, megachip8, xo-chip of vip-chip-8", [&](){
        if(!presetName.empty()) {
            try {
                preset = emu::Chip8EmulatorOptions::presetForName(presetName);
                options = emu::Chip8EmulatorOptions::optionsOfPreset(preset);
            }
            catch(std::runtime_error e) {
                std::cerr << "ERROR: " << e.what() << ", check help for supported presets." << std::endl;
                presetName = "";
            }
        }
    });
    cli.option({"-s", "--exec-speed"}, execSpeed, "Set execution speed in instructions per frame (0-500000, 0: unlimited)");
    cli.option({"--random-gen"}, randomGen, "Select a predictable random generator used for trace log mode (rand-lgc or counting)");
    cli.option({"--random-seed"}, randomSeed, "Select a random seed for use in combination with --random-gen, default: 12345");
    cli.option({"--screen-dump"}, screenDump, "When in trace mode, dump the final screen content to the console");
    cli.option({"--trace-log"}, options.optTraceLog, "If true, enable trace logging into log-view");
    //cli.option({"--opcode-table"}, opcodeTable, "Dump an opcode table to stdout");
    cli.option({"--opcode-json"}, opcodeJSON, "Dump opcode information as JSON to stdout");
#ifndef NDEBUG
    cli.option({"--dump-interpreter"}, dumpInterpreter, "Dump the given interpreter in a local file named '<interpreter>.ram' and exit");
#endif
    cli.category("Quirks");
    cli.option({"--just-shift-vx"}, options.optJustShiftVx, "If true, 8xy6/8xyE will just shift Vx and ignore Vy");
    cli.option({"--dont-reset-vf"}, options.optDontResetVf, "If true, Vf will not be reset by 8xy1/8xy2/8xy3");
    cli.option({"--load-store-inc-i-by-x"}, options.optLoadStoreIncIByX, "If true, Fx55/Fx65 increment I by x");
    cli.option({"--load-store-dont-inc-i"}, options.optLoadStoreDontIncI, "If true, Fx55/Fx65 don't change I");
    cli.option({"--wrap-sprites"}, options.optWrapSprites, "If true, Dxyn wrap sprites around border");
    cli.option({"--instant-dxyn"}, options.optInstantDxyn, "If true, Dxyn don't wait for vsync");
    cli.option({"--lores-dxy0-width-8"}, options.optLoresDxy0Is8x16, "If true, draw Dxy0 sprites have width 8");
    cli.option({"--lores-dxy0-width-16"}, options.optLoresDxy0Is16x16, "If true, draw Dxy0 sprites have width 16");
    cli.option({"--sc11-collision"}, options.optSC11Collision, "If true, use SCHIP1.1 collision logic");
    cli.option({"--jump0-bxnn"}, options.optJump0Bxnn, "If true, use Vx as offset for Bxnn");
    cli.option({"--allow-hires"}, options.optAllowHires, "If true, support for hires (128x64) is enabled");
    cli.option({"--only-hires"}, options.optOnlyHires, "If true, emulation has hires mode only");
    cli.option({"--allow-color"}, options.optAllowColors, "If true, support for multi-plane drawing is enabled");
    cli.option({"--has-16bit-addr"}, options.optHas16BitAddr, "If true, address space is 16bit (64k ram)");
    cli.option({"--xo-chip-sound"}, options.optXOChipSound, "If true, use XO-CHIP sound instead of buzzer");
    cli.positional(romFile, "ROM file or source to load");
    cli.parse();
    if(showHelp) {
        cli.usage();
        exit(0);
    }
    if(opcodeTable) {
        dumpOpcodeTable(std::cout, emu::C8V::CHIP_8|emu::C8V::CHIP_10|emu::C8V::CHIP_48|emu::C8V::SCHIP_1_0|emu::C8V::SCHIP_1_1|emu::C8V::MEGA_CHIP|emu::C8V::XO_CHIP);
        exit(0);
    }
    if(opcodeJSON) {
        dumpOpcodeJSON(std::cout, emu::C8V::CHIP_8|emu::C8V::CHIP_8_I|emu::C8V::CHIP_8X|emu::C8V::CHIP_10|emu::C8V::CHIP_8_D6800|emu::C8V::CHIP_48|emu::C8V::SCHIP_1_0|emu::C8V::SCHIP_1_1|emu::C8V::SCHIPC|emu::C8V::MEGA_CHIP|emu::C8V::XO_CHIP);
        exit(0);
    }
    if(!dumpInterpreter.empty()) {
        auto data = emu::Chip8VIP::getInterpreterCode(toUpper(dumpInterpreter));
        if(!data.empty()) {
            {
                std::ofstream os(dumpInterpreter + ".ram", std::ios::binary);
                os.write((const char*)data.data(), data.size());
            }
            std::cout << "Written " << data.size() << " bytes to '" << dumpInterpreter << ".ram'." << std::endl;
            exit(0);
        }
        else {
            std::cerr << "ERROR: Unknown interpreter '" << dumpInterpreter << "'." << std::endl;
            exit(1);
        }
    }
    if(romFile.size() > 1) {
        std::cerr << "ERROR: only one ROM/source file supported" << std::endl;
        exit(1);
    }
    if(romFile.empty() && startRom) {
        std::cerr << "ERROR: can't start anything without a ROM/source file" << std::endl;
        exit(1);
    }
    if(!randomGen.empty() && (traceLines<0 || (randomGen != "rand-lgc" && randomGen != "counting"))) {
        std::cerr << "ERROR: random generator must be 'rand-lgc' or 'counting' and trace must be used." << std::endl;
        exit(1);
    }
    if(execSpeed >= 0) {
        options.instructionsPerFrame = execSpeed;
    }
    if(traceLines < 0 && !compareRun && !benchmark) {
#else
    ghc::CLI cli(argc, argv);
    std::string presetName = "schipc";
#ifdef WEB_WITH_FETCHING
    std::string urlLoad;
#endif
    int64_t execSpeed = -1;
    cli.option({"-p", "--preset"}, presetName, "Select CHIP-8 preset to use: chip-8, chip-10, chip-48, schip1.0, schip1.1, megachip8, xo-chip of vip-chip-8");
    cli.option({"-s", "--exec-speed"}, execSpeed, "Set execution speed in instructions per frame (0-500000, 0: unlimited)");
#ifdef WEB_WITH_FETCHING
    cli.option({"-u", "--url"}, urlLoad, "An url that will be tried to load a rom or source from");
#endif
    cli.parse();
    if(!presetName.empty()) {
        try {
            preset = emu::Chip8EmulatorOptions::presetForName(presetName);
        }
        catch(std::runtime_error e) {
            std::cerr << "ERROR: " << e.what() << ", check help for supported presets." << std::endl;
            exit(1);
        }
    }
    auto chip8options = emu::Chip8EmulatorOptions::optionsOfPreset(preset);
    if(execSpeed >= 0) {
        chip8options.instructionsPerFrame = execSpeed;
    }
    {
#endif

#ifndef PLATFORM_WEB
        Cadmium cadmium(presetName.empty() ? nullptr : &options);
        if (!romFile.empty()) {
            cadmium.loadRom(romFile.front().c_str(), startRom);
        }
        //SetTargetFPS(60);
        while (!cadmium.windowShouldClose()) {
            cadmium.updateAndDraw();
        }
#else
        try {
            Cadmium cadmium(presetName.empty() ? nullptr : &chip8options);
#ifdef WEB_WITH_FETCHING
            if(!urlLoad.empty()) {
                emscripten_fetch_attr_t attr;
                emscripten_fetch_attr_init(&attr);
                strcpy(attr.requestMethod, "GET");
                attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
                attr.onsuccess = loadBinaryCallbackC;
                attr.onerror = downloadFailedCallbackC;
                loadBinaryCallback = [&](std::string filename, const uint8_t* data, size_t size) { cadmium.loadBinary(filename, data, size, false); };
                emscripten_fetch(&attr, urlLoad.c_str());
            }
#endif
            emscripten_set_main_loop_arg(Cadmium::updateAndDrawFrame, &cadmium, 0, 1);
        }
        catch(std::exception& ex) {
            std::cerr << "Exception: " << ex.what() << std::endl;
        }
#endif
    }
#ifndef PLATFORM_WEB
    else {
        emu::Chip8HeadlessHost host(options);
        //chip8options.optHas16BitAddr = true;
        //chip8options.optWrapSprites = true;
        //chip8options.optAllowColors = true;
        //chip8options.optJustShiftVx = false;
        //chip8options.optLoadStoreDontIncI = false;
        //chip8options.optDontResetVf = true;
        //chip8options.optInstantDxyn = true;
        if(!randomGen.empty()) {
            options.advanced = nlohmann::ordered_json::object({
                {"random", randomGen},
                {"seed", randomSeed}
            });
            options.updatedAdvanced();
        }
        auto chip8 = emu::Chip8EmulatorBase::create(host, emu::IChip8Emulator::eCHIP8MPT, options);
        std::clog << "Engine1: " << chip8->name() << ", active variant: " << emu::Chip8EmulatorOptions::nameOfPreset(options.behaviorBase) << std::endl;
        octo_emulator octo;
        octo_options oopt{};
        oopt.q_clip = 1;
        //oopt.q_loadstore = 1;

        chip8->reset();
        if(!romFile.empty()) {
            int size = 0;
            uint8_t* data = LoadFileData(romFile.front().c_str(), &size);
            if (size < chip8->memSize() - 512) {
                std::memcpy(chip8->memory() + 512, data, size);
            }
            UnloadFileData(data);
            //chip8.loadRom(romFile.c_str());
        }
        octo_emulator_init(&octo, (char*)chip8->memory() + 512, 4096 - 512, &oopt, nullptr);
        int64_t i = 0;
        if(compareRun) {
            std::clog << "Engine2: C-Octo" << std::endl;
            do {
                if ((i & 7) == 0) {
                    chip8->handleTimer();
                    if (octo.dt)
                        --octo.dt;
                    if (octo.st)
                        --octo.st;
                }
                chip8->executeInstruction();
                octo_emulator_instruction(&octo);
                if (!(i % 500000)) {
                    std::clog << i << ": " << chip8->dumpStateLine() << std::endl;
                    std::clog << i << "| " << dumOctoStateLine(&octo) << std::endl;
                }
                if(!(i % 500000)) {
                    std::cout << chip8EmuScreen(*chip8);
                }
                ++i;
            } while ((i & 0xfff) || (chip8->dumpStateLine() == dumOctoStateLine(&octo) && chip8EmuScreen(*chip8) == octoScreen(octo)));
            std::clog << i << ": " << chip8->dumpStateLine() << std::endl;
            std::clog << i << "| " << dumOctoStateLine(&octo) << std::endl;
            std::cerr << chip8EmuScreen(*chip8);
            std::cerr << "---" << std::endl;
            std::cerr << octoScreen(octo) << std::endl;
        }
        else if(benchmark > 0) {
            uint64_t instructions = benchmark;
            std::cout << "Executing benchmark (" << options.instructionsPerFrame << "ipf)..." << std::endl;
            auto startChip8 = std::chrono::steady_clock::now();
            auto ticks = uint64_t(instructions / options.instructionsPerFrame);
            for(i = 0; i < ticks; ++i) {
                chip8->tick(options.instructionsPerFrame);
            }
            chip8->handleTimer();
            int64_t lastCycles = -1;
            int64_t cycles = 0;
            while((cycles = chip8->getCycles()) < instructions && cycles != lastCycles) {
                chip8->executeInstruction();
                lastCycles = cycles;
            }
            auto durationChip8 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startChip8);
            if(screenDump) {
                std::cout << chip8EmuScreenANSI(*chip8);
            }
            std::cout << "Executed instructions: " << chip8->getCycles() << std::endl;
            std::cout << "Cadmium: " << durationChip8.count() << "us, " << int(double(chip8->getCycles())/durationChip8.count()) << "MIPS" << std::endl;
        }
        else if(traceLines >= 0) {
            do {
                std::cout << i << "/" << chip8->getCycles() << ": " << chip8->dumpStateLine() << std::endl;
                if ((i % options.instructionsPerFrame) == 0) {
                    chip8->handleTimer();
                }
                chip8->executeInstruction();
                ++i;
            } while (i <= traceLines && chip8->getExecMode() == emu::IChip8Emulator::ExecMode::eRUNNING);
            if(screenDump) {
                std::cout << chip8EmuScreenANSI(*chip8);
            }
        }
    }
#endif
    return 0;
}
