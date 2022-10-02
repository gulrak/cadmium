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
#define RLGUIPP_IMPLEMENTATION
#define RAYGUI_CUSTOM_ICONS  // Custom icons set required
#include "icons.h"  // Custom icons set provided, generated with rGuiIcons tool
#include <rlguipp/rlguipp.hpp>

extern "C" {
#include <raymath.h>
};

#include <date/date.h>
#include <fmt/format.h>
#include <stdendian/stdendian.h>
#include <about.hpp>
#include <emulation/c8bfile.hpp>
#include <emulation/chip8compiler.hpp>
#include <emulation/chip8cores.hpp>
#include <emulation/chip8decompiler.hpp>
#include <emulation/time.hpp>
#include <emulation/utility.hpp>
#include <ghc/cli.hpp>
#include <librarian.hpp>
#include <systemtools.hpp>
#include <resourcemanager.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <thread>
#include <mutex>
#include <new>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
extern "C" void openFileCallbackC(const char* str) __attribute__((used));
static std::function<void(const std::string&)> openFileCallback;
void openFileCallbackC(const char* str)
{
    if (openFileCallback)
        openFileCallback(str);
}
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
#include <filesystem>
// #endif

namespace fs = std::filesystem;

#define CHIP8_STYLE_PROPS_COUNT 16
static const GuiStyleProp chip8StyleProps[CHIP8_STYLE_PROPS_COUNT] = {
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
#else
static std::string webClip;
#endif
#endif

std::string GetClipboardTextX()
{
#ifdef PLATFORM_WEB
#ifdef WEB_WITH_CLIPBOARD
    return pasteClip();
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
    copyClip(text.c_str());
#else
    webClip = text;
#endif
#else
    SetClipboardText(text.c_str());
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


std::atomic_uint8_t _soundTimer{0};
std::atomic_int _frameBoost{1};

class Cadmium : public emu::Chip8EmulatorHost
{
public:
    using ExecMode = emu::IChip8Emulator::ExecMode;
    using CpuState = emu::IChip8Emulator::CpuState;
    enum MemFlags { eNONE = 0, eBREAKPOINT = 1, eWATCHPOINT = 2 };
    enum MainView { eVIDEO, eDEBUGGER, eEDITOR, eSETTINGS, eROM_SELECTOR, eROM_EXPORT };
    enum EmulationMode { eCOSMAC_VIP_CHIP8, eGENERIC_CHIP8 };
    enum FileBrowserMode { eLOAD, eSAVE, eWEB_SAVE };
    Cadmium(int screenWidth_, int screenHeight_, emu::Chip8EmulatorOptions chip8options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eXOCHIP))
        : screenWidth(screenWidth_)
        , screenHeight(screenHeight_)
    {
#ifndef PLATFORM_WEB
        auto dataDir = dataPath();
#endif
        _instance = this;
        InitAudioDevice();
        SetAudioStreamBufferSizeDefault(1470);
        audioStream = LoadAudioStream(44100, 16, 1);
        SetAudioStreamCallback(audioStream, Cadmium::audioInputCallback);
        PlayAudioStream(audioStream);
        SetTargetFPS(60);
#ifdef PLATFORM_WEB
        scaleBy2 = false;
#else
        scaleBy2 = GetMonitorWidth(GetCurrentMonitor()) > 1680 || GetWindowScaleDPI().x > 1.0f;
#endif
        renderTexture = LoadRenderTexture(screenWidth, screenHeight);
        SetTextureFilter(renderTexture.texture, TEXTURE_FILTER_POINT);

        for (auto chip8StyleProp : chip8StyleProps) {
            GuiSetStyle(chip8StyleProp.controlId, chip8StyleProp.propertyId, chip8StyleProp.propertyValue);
        }
        generateFont();
        options = chip8options;
        updateEmulatorOptions();
        chipEmu->reset();
        screen = GenImageColor(chipEmu->getMaxScreenWidth(), chipEmu->getMaxScreenHeight(), BLACK);
        screenTexture = LoadTextureFromImage(screen);
        auto titleRes = resources.resourceForName("cadmium-title.png");
        titleImage = LoadImageFromMemory(".png", titleRes.data(), titleRes.size());
        auto microRes = resources.resourceForName("micro-font.png");
        microFont = LoadImageFromMemory(".png", microRes.data(), microRes.size());
        drawMicroText(titleImage, "v" CADMIUM_VERSION, 91 - std::strlen("v" CADMIUM_VERSION)*4, 6, WHITE);
        drawMicroText(titleImage, "Beta", 38, 53, WHITE);
        std::string buildDate = __DATE__;
        auto dateText = buildDate.substr(0, 3);
        bool shortDate = (buildDate[4] == ' ');
        drawMicroText(titleImage, buildDate.substr(9), 83, 53, WHITE);
        drawMicroText(titleImage, buildDate.substr(4,2), 75, 52, WHITE);
        drawMicroText(titleImage, buildDate.substr(0,3), shortDate ? 67 : 63, 53, WHITE);
        //ImageColorReplace(&titleImage, {0,0,0,255}, {0x00,0x22,0x2b,0xff});
        ImageColorReplace(&titleImage, {0,0,0,255}, {0x1a,0x1c,0x2c,0xff});
        ImageColorReplace(&titleImage, {255,255,255,255}, {0x51,0xbf,0xd3,0xff});
        titleTexture = LoadTextureFromImage(titleImage);
        currentDirectory = librarian.currentDirectory();

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

        colorPalette = {
            be32(0x1a1c2cff), be32(0xf4f4f4ff), be32(0x94b0c2ff), be32(0x333c57ff),
            be32(0xb13e53ff), be32(0xa7f070ff), be32(0x3b5dc9ff), be32(0xffcd75ff),
            be32(0x5d275dff), be32(0x38b764ff), be32(0x29366fff), be32(0x566c86ff),
            be32(0xef7d57ff), be32(0x73eff7ff), be32(0x41a6f6ff), be32(0x257179ff)
        };
    }

    ~Cadmium() override
    {
        gui::UnloadGui();
        UnloadFont(font);
        UnloadRenderTexture(renderTexture);
        UnloadImage(titleImage);
        UnloadTexture(titleTexture);
        UnloadTexture(screenTexture);
        UnloadAudioStream(audioStream);
        CloseAudioDevice();
        UnloadImage(screen);
        _instance = nullptr;
    }

    bool isHeadless() const override { return false; }

    void drawMicroText(Image& dest, std::string text, int x, int y, Color tint)
    {
        for(auto c : text) {
            if((uint8_t)c < 128)
                ImageDraw(&dest, microFont, {(c%32)*4.0f, (c/32)*6.0f, 4, 6}, {(float)x, (float)y, 4, 6}, tint);
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
        std::scoped_lock lock(audioMutex);
        if(chipEmu) {
            auto st = chipEmu->soundTimer();
            auto samplesLeftToPlay = std::min(st * (44100 / 60) / _frameBoost, (int)frames);
            float phase = st ? chipEmu->getAudioPhase() : 0.0f;
            if (!options.optXOChipSound) {
                const float step = 1400.0f / 44100;
                for (int i = 0; i < samplesLeftToPlay; ++i, --frames) {
                    *samples++ = (phase > 0.5f) ? 16384 : -16384;
                    phase = std::fmod(phase + step, 1.0f);
                }
            }
            else {
                auto step = 4000 * std::pow(2.0f, (float(chipEmu->getXOPitch()) - 64) / 48.0f) / 128 / 44100;
                for (int i = 0; i < samplesLeftToPlay; ++i, --frames) {
                    auto pos = int(std::clamp(phase * 128.0f, 0.0f, 127.0f));
                    *samples++ = chipEmu->getXOAudioPattern()[pos >> 3] & (1 << (7 - (pos & 7))) ? 16384 : -16384;
                    phase = std::fmod(phase + step, 1.0f);
                }
            }
            chipEmu->setAudioPhase(phase);
        }
        while(frames--) {
            *samples++ = 0;
        }
    }

    uint8_t getKeyPressed() override
    {
        static int64_t instruction = 0;
        static int waitKeyUp = 0;
        static int keyId = 0;
        if(waitKeyUp && instruction == chipEmu->cycles()) {
            if(IsKeyUp(waitKeyUp)) {
                waitKeyUp = 0;
                return keyId;
            }
            return 0;
        }
        waitKeyUp = 0;
        auto key = GetKeyPressed();
        if (key) {
            for (int i = 0; i < 16; ++i) {
                if (key == keyMapping[i]) {
                    instruction = chipEmu->cycles();
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
        return IsKeyDown(keyMapping[key & 0xF]);
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
        if(!customPalette) {
            for (int i = 0; i < palette.size(); ++i) {
                colorPalette[i] = be32((rgb332To888(palette[i]) << 8) | 0xff);
            }
            updateScreen = true;
        }
    }

    void generateFont()
    {
        fontImage = GenImageColor(256, 256, {0, 0, 0, 0});
        int glyphCount = 0;
        for(const auto& fci : fontRom) {
            auto c = fci.codepoint;
            drawChar(fontImage, c, (glyphCount % 32) * 6, (glyphCount / 32) * 8, WHITE);
            ++glyphCount;
        }
#if !defined(NDEBUG)
        ExportImage(fontImage, "Test.png");
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
        font.baseSize = 8;
        font.glyphCount = glyphCount;
        font.texture = LoadTextureFromImage(fontImage);
        font.recs = (Rectangle*)std::calloc(font.glyphCount, sizeof(Rectangle));
        font.glyphs = (GlyphInfo*)std::calloc(font.glyphCount, sizeof(GlyphInfo));
        int idx = 0;
        for(const auto& fci : fontRom) {
            int i = fci.codepoint;
            font.recs[idx].x = (idx % 32) * 6;
            font.recs[idx].y = (idx / 32) * 8;
            font.recs[idx].width = 6;
            font.recs[idx].height = 8;
            font.glyphs[idx].value = i;
            font.glyphs[idx].offsetX = 0;
            font.glyphs[idx].offsetY = 0;
            font.glyphs[idx].advanceX = 6;
            ++idx;
        }
        GuiSetFont(font);
    }

    bool screenChanged() const
    {
        return updateScreen;
    }

    int getFrameBoost() const { return frameBoost>0 ? frameBoost : 1; }
    int getInstrPerFrame() const { return options.instructionsPerFrame>=0 ? options.instructionsPerFrame : 0; }

    void updateScreenTexture()
    {
        if(true/*updateScreen*/) {
            auto* pixel = (uint32_t*)screen.data;
            const uint8_t* planes = chipEmu->getScreenBuffer();
            const uint8_t* end = planes + chipEmu->getMaxScreenWidth()*chipEmu->getMaxScreenHeight();
            while(planes < end) {
                *pixel++ = colorPalette[*planes++ & 0xF];
            }
            UpdateTexture(screenTexture, screen.data);
            updateScreen = false;
        }
    }

    static void updateAndDrawFrame(void* self)
    {
        static_cast<Cadmium*>(self)->updateAndDraw();
    }

    void updateAndDraw()
    {
#ifndef PLATFORM_WEB
        if (scaleBy2) {
            // Screen size x2
            if (GetScreenWidth() < screenWidth * 2) {
                SetWindowSize(screenWidth * 2, screenHeight * 2);
                CenterWindow(screenWidth * 2, screenHeight * 2);
                SetMouseScale(0.5f, 0.5f);
            }
        }
        else {
            // Screen size x1
            if (screenWidth < GetScreenWidth()) {
                SetWindowSize(screenWidth, screenHeight);
                CenterWindow(screenWidth, screenHeight);
                SetMouseScale(1.0f, 1.0f);
            }
        }
#endif

        librarian.update(options); // allows librarian to complete background tasks

        if (IsFileDropped()) {
            auto files = LoadDroppedFiles();
            if (files.count > 0) {
                loadRom(files.paths[0]);
            }
            UnloadDroppedFiles(files);
        }

#ifdef WITH_EDITOR
        if(mainView == eEDITOR) {
            editor.update();
            if(!editor.compiler().isError() && editor.compiler().sha1Hex() != romSha1Hex) {
                romImage.assign(editor.compiler().code(), editor.compiler().code() + editor.compiler().codeSize());
                romSha1Hex = editor.compiler().sha1Hex();
                reloadRom();
            }
        }
#endif

        auto fb = getFrameBoost();
        for(int i = 0; i < fb; ++i) {
            chipEmu->tick(getInstrPerFrame());
            _soundTimer.store(chipEmu->soundTimer());
        }

        updateScreenTexture();

        BeginTextureMode(renderTexture);
        drawGui();
        EndTextureMode();

        BeginDrawing();
        {
            if (scaleBy2) {
                DrawTexturePro(renderTexture.texture, (Rectangle){0, 0, (float)renderTexture.texture.width, -(float)renderTexture.texture.height}, (Rectangle){0, 0, (float)renderTexture.texture.width * 2, (float)renderTexture.texture.height * 2},
                               (Vector2){0, 0}, 0.0f, WHITE);
            }
            else {
                DrawTextureRec(renderTexture.texture, (Rectangle){0, 0, (float)renderTexture.texture.width, -(float)renderTexture.texture.height}, (Vector2){0, 0}, WHITE);
            }
            // DrawText(TextFormat("Res: %dx%d", GetMonitorWidth(GetCurrentMonitor()), GetMonitorHeight(GetCurrentMonitor())), 10, 30, 10, GREEN);
            // DrawFPS(10,10);
        }
        EndDrawing();
    }

    void drawScreen(Rectangle dest, int gridScale)
    {
        const Color gridLineCol{40,40,40,255};
        int scrWidth = chipEmu->getCurrentScreenWidth();
        int scrHeight = chipEmu->getCurrentScreenHeight();
        DrawTexturePro(screenTexture, {0, 0, (float)scrWidth, (float)scrHeight}, dest, {0, 0}, 0, WHITE);
        if (grid) {
            for (short x = 0; x < scrWidth; ++x) {
                DrawRectangle(dest.x + x * gridScale, dest.y, 1, dest.height, gridLineCol);
            }
            for (short y = 0; y < scrHeight; ++y) {
                DrawRectangle(dest.x, dest.y + y * gridScale, dest.width, 1, gridLineCol);
            }
        }
        if(GetTime() < 5 && romImage.empty()) {
            DrawTexturePro(titleTexture, {0, 0, 128, 64}, dest, {0, 0}, 0, {255,255,255,uint8_t(GetTime()>4 ? 255.0f*(4.0f-GetTime()) : 255.0f)});
        }
    }

    static bool iconButton(int iconId, bool isPressed = false, Color color = {3, 127, 161})
    {
        auto oldColor = gui::GetStyle(BUTTON, BASE_COLOR_NORMAL);
        if (isPressed)
            gui::SetStyle(BUTTON, BASE_COLOR_NORMAL, ColorToInt(color));
        gui::SetNextWidth(20);
        auto result = gui::Button(GuiIconText(iconId, ""));
        gui::SetStyle(BUTTON, BASE_COLOR_NORMAL, oldColor);
        return result;
    }

    void drawGui()
    {
        using namespace gui;
        ClearBackground(GetColor(GetStyle(DEFAULT, BACKGROUND_COLOR)));
        Rectangle video;
        int gridScale = 4;
        static int64_t lastInstructionCount = 0;

        BeginGui({}, &renderTexture);
        {
            SetStyle(STATUSBAR, TEXT_PADDING, 4);
            SetStyle(LISTVIEW, SCROLLBAR_WIDTH, 6);

            SetRowHeight(16);
            SetSpacing(0);
            auto ips = (chipEmu->cycles() - lastInstructionCount) / GetFrameTime();
            if(mainView == eEDITOR) {
#ifdef WITH_EDITOR
                StatusBar({{0.75f, editor.compiler().errorMessage().c_str()},
                    {0.25f, fmt::format("Cursor: {}:{}", editor.line(), editor.column()).c_str()}});
#endif
            }
            else if(getFrameBoost() > 1) {
                StatusBar({{0.5f, fmt::format("Instruction cycles: {}", chipEmu->cycles()).c_str()},
                           {0.25f, formatUnit(ips, "IPS").c_str()/*fmt::format("{:.2f}MIPS", ips / 1000000).c_str()*/},
                           {0.25f, formatUnit((double)getFrameBoost() * GetFPS(), "eFPS").c_str() /*fmt::format("{:.2f}k eFPS", (float)getFrameBoost() * GetFPS() / 1000).c_str()*/}});
            }
            else {
                StatusBar({{0.5f, fmt::format("Instruction cycles: {}", chipEmu->cycles()).c_str()},
                           {0.25f, formatUnit(ips, "IPS").c_str()},
                           {0.25f, formatUnit((double)getFrameBoost() * GetFPS(), "FPS").c_str()}});
            }
            lastInstructionCount = chipEmu->cycles();
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
                    Rectangle menuRect = {1, GetCurrentPos().y + 20, 110, 72};
#else
                    Rectangle menuRect = {1, GetCurrentPos().y + 20, 110, 57};
#endif
                    BeginPopup(menuRect, &menuOpen);
                    SetRowHeight(12);
                    Space(3);
                    if(LabelButton(" About Cadmium..."))
                        aboutOpen = true, aboutScroll = {0,0}, menuOpen = false;
                    Space(3);
                    if(LabelButton(" New...")) {
                        mainView = eEDITOR;
                        menuOpen = false;
                        editor.setText(": main\n    jump main");
                        romName = "unnamed.8o";
                    }
                    if(LabelButton(" Open...")) {
#ifdef PLATFORM_WEB
                        loadFileWeb();
#else
                        mainView = eROM_SELECTOR;
                        librarian.fetchDir(currentDirectory);
#endif
                        menuOpen = false;
                    }
                    if(LabelButton(" Save...")) {
                        mainView = eROM_EXPORT;
#ifndef PLATFORM_WEB
                        librarian.fetchDir(currentDirectory);
#endif
                        menuOpen = false;
                    }
#ifndef PLATFORM_WEB
                    Space(3);
                    if(LabelButton(" Quit"))
                        menuOpen = false, shouldClose = true;
#endif
                    EndPopup();
                    if(IsKeyPressed(KEY_ESCAPE) || (IsMouseButtonPressed(0) && !CheckCollisionPointRec(GetMousePosition(), menuRect)))
                        menuOpen = false;
                }
                if(aboutOpen) {
                    aboutOpen = !BeginWindowBox({-1,-1,450,200}, "About Cadmium", &aboutOpen, WindowBoxFlags(WBF_MOVABLE|WBF_MODAL));
                    SetStyle(DEFAULT, BORDER_WIDTH, 0);
                    static size_t newlines = std::count_if( aboutText.begin(), aboutText.end(), [](char c){ return c =='\n'; });
                    BeginScrollPanel(-1, {0,0,440,newlines*10.0f + 100}, &aboutScroll);
                    SetRowHeight(10);
                    DrawTextureRec(titleTexture, {34,2,60,60}, {aboutScroll.x + 8.0f, aboutScroll.y + 31.0f}, WHITE);
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
                if (iconButton(ICON_ROM, mainView == eROM_SELECTOR)) {
#ifdef PLATFORM_WEB
                    loadFileWeb();
#else
                    mainView = eROM_SELECTOR;
                    librarian.fetchDir(currentDirectory);
#endif
                }
                SetNextWidth(130);
                SetStyle(TEXTBOX, BORDER_WIDTH, 1);
                TextBox(romName, 4095);

                if (iconButton(ICON_PLAYER_PAUSE, chipEmu->execMode() == ExecMode::ePAUSED)) {
                    chipEmu->setExecMode(ExecMode::ePAUSED);
                    if(mainView == eEDITOR || mainView == eSETTINGS) {
                        mainView = eVIDEO;
                    }
                }
                SetTooltip("PAUSE");
                if (iconButton(ICON_PLAYER_PLAY, chipEmu->execMode() == ExecMode::eRUNNING)) {
                    chipEmu->setExecMode(ExecMode::eRUNNING);
                    if(mainView == eEDITOR || mainView == eSETTINGS) {
                        mainView = eVIDEO;
                    }
                }
                SetTooltip("RUN");
                if (iconButton(ICON_STEP_OVER, chipEmu->execMode() == ExecMode::eSTEPOVER)) {
                    chipEmu->setExecMode(ExecMode::eSTEPOVER);
                    if(mainView == eEDITOR || mainView == eSETTINGS) {
                        mainView = eDEBUGGER;
                    }
                }
                SetTooltip("STEP OVER");
                if (iconButton(ICON_STEP_INTO, chipEmu->execMode() == ExecMode::eSTEP)) {
                    chipEmu->setExecMode(ExecMode::eSTEP);
                    if(mainView == eEDITOR || mainView == eSETTINGS) {
                        mainView = eDEBUGGER;
                    }
                }
                SetTooltip("STEP INTO");
                if (iconButton(ICON_STEP_OUT, chipEmu->execMode() == ExecMode::eSTEPOUT)) {
                    chipEmu->setExecMode(ExecMode::eSTEPOUT);
                    if(mainView == eEDITOR || mainView == eSETTINGS) {
                        mainView = eDEBUGGER;
                    }
                }
                SetTooltip("STEP OUT");
                if (iconButton(ICON_RESTART)) {
                    reloadRom();
                    if(mainView == eEDITOR || mainView == eSETTINGS) {
                        mainView = eDEBUGGER;
                    }
                }
                SetTooltip("RESTART");
                int buttonsRight = 5;
#ifdef WITH_EDITOR
                ++buttonsRight;
#endif
                int avail = 202;
#ifdef PLATFORM_WEB
                --buttonsRight;
                avail += 10;
#endif
                auto spacePos = GetCurrentPos();
                auto spaceWidth = avail - buttonsRight * 20;
                Space(spaceWidth);
                if (iconButton(ICON_BOX_GRID, grid))
                    grid = !grid;
                SetTooltip("TOGGLE GRID");
                Space(10);
                if (iconButton(ICON_ZOOM_ALL, mainView == eVIDEO))
                    mainView = eVIDEO;
                SetTooltip("FULL VIDEO");
                if (iconButton(ICON_CPU, mainView == eDEBUGGER))
                    mainView = eDEBUGGER;
                SetTooltip("DEBUGGER");
#ifdef WITH_EDITOR
                if (iconButton(ICON_FILETYPE_TEXT, mainView == eEDITOR))
                    mainView = eEDITOR, chipEmu->setExecMode(ExecMode::ePAUSED);
                SetTooltip("Editor");
#endif
                if (iconButton(ICON_GEAR, mainView == eSETTINGS))
                    mainView = eSETTINGS;
                SetTooltip("SETTINGS");

                static Vector2 versionSize = MeasureTextEx(guiFont, "v" CADMIUM_VERSION, 8, 0);
                DrawTextEx(guiFont, "v" CADMIUM_VERSION, {spacePos.x + (spaceWidth - versionSize.x) / 2, spacePos.y + 6}, 8, 0, WHITE);
#ifndef PLATFORM_WEB
                Space(10);
                if (iconButton(ICON_HIDPI, scaleBy2))
                    scaleBy2 = !scaleBy2;
                SetTooltip("TOGGLE ZOOM    ");
#endif
            }
            EndColumns();

            switch (mainView) {
                case eDEBUGGER: {
                    const int lineSpacing = 10;
                    const int debugScale = 256/chipEmu->getCurrentScreenWidth();
                    lastView = mainView;
                    gridScale = debugScale;
                    BeginColumns();
                    SetSpacing(-1);
                    SetNextWidth(256 + 2);
                    Begin();
                    SetSpacing(0);
                    BeginPanel("Video", {1, 0});
                    {
                        video = {GetCurrentPos().x, GetCurrentPos().y, (float)256, (float)128};
                        Space(128 + 1);
                    }
                    EndPanel();
                    drawScreen(video, gridScale);
                    BeginPanel("Instructions", {5, 2});
                    {
                        auto area = GetContentAvailable();
                        Space(area.height);
                        if(CheckCollisionPointRec(GetMousePosition(), GetLastWidgetRect())) {
                            auto wheel = GetMouseWheelMoveV();
                            if(std::fabs(wheel.y) >= 0.5f) {
                                if(instructionOffset < 0) {
                                    instructionOffset = chipEmu->getPC();
                                }
                                int step = 0;
                                if(wheel.y >= 0.5f) step = 2;
                                else if(wheel.y <= 0.5f) step = -2;
                                instructionOffset = std::clamp(instructionOffset - step, 0, 4096 - 9*2);
                            }
                        }
                        auto insOff = chipEmu->execMode() == ExecMode::ePAUSED && instructionOffset >= 0 ? instructionOffset : (chipEmu->getPC() > 8 ? chipEmu->getPC() - 8 : 0);
                        bool inIf = false;
                        for (int i = -1; i < 9; ++i) {
                            if(insOff + i * 2 + 1 < chipEmu->memSize()) {
                                uint16_t opcode = (chipEmu->memory()[insOff + i * 2] << 8) | chipEmu->memory()[insOff + i * 2 + 1];
                                auto [bytes, instruction] = chipEmu->disassembleInstruction(chipEmu->memory() + insOff + i * 2, chipEmu->memory() + chipEmu->memSize());
                                if (i >= 0) {
                                    if (inIf)
                                        DrawTextEx(font, TextFormat("%04X: %04X         %s", insOff + i * 2, opcode, instruction.c_str()), {area.x, area.y + i * lineSpacing + 1}, 8, 0, chipEmu->getPC() == insOff + i * 2 ? YELLOW : LIGHTGRAY);
                                    else
                                        DrawTextEx(font, TextFormat("%04X: %04X       %s", insOff + i * 2, opcode, instruction.c_str()), {area.x, area.y + i * lineSpacing + 1}, 8, 0, chipEmu->getPC() == insOff + i * 2 ? YELLOW : LIGHTGRAY);
                                }
                                inIf = instruction.rfind("if ", 0) == 0;
                            }
                        }
                    }
                    EndPanel();
                    End();
                    SetNextWidth(50);
                    BeginPanel("Regs");
                    {
                        auto pos = GetCurrentPos();
                        auto area = GetContentAvailable();
                        pos.x += 0;
                        Space(area.height);
                        int i;
                        for (i = 0; i < 16; ++i) {
                            DrawTextEx(font, TextFormat("V%X: %02X", i, chipEmu->getV(i)), {pos.x, pos.y + i * lineSpacing}, 8, 0, chipEmu->getV(i) == chipEmu->getCopyV(i) ? LIGHTGRAY : YELLOW);
                        }
                        ++i;
                        DrawTextEx(font, chipEmu->memSize() > 4096 ? TextFormat("PC:%04X", chipEmu->getPC()) : TextFormat("PC: %03X", chipEmu->getPC()), {pos.x, pos.y + i * lineSpacing}, 8, 0, LIGHTGRAY);
                        ++i;
                        DrawTextEx(font, chipEmu->memSize() > 4096 ? TextFormat(" I:%04X", chipEmu->getI()) : TextFormat(" I: %03X", chipEmu->getI()), {pos.x, pos.y + i * lineSpacing}, 8, 0, chipEmu->getI() == chipEmu->getCopyI() ? LIGHTGRAY : YELLOW);
                        ++i;
                        DrawTextEx(font, TextFormat("SP: %02X", chipEmu->getSP()), {pos.x, pos.y + i * lineSpacing}, 8, 0, chipEmu->getSP() == chipEmu->getCopySP() ? LIGHTGRAY : YELLOW);
                        ++i;
                        ++i;
                        DrawTextEx(font, TextFormat("DT: %02X", chipEmu->delayTimer()), {pos.x, pos.y + i * lineSpacing}, 8, 0, chipEmu->delayTimer() == chipEmu->getCopyDT() ? LIGHTGRAY : YELLOW);
                        ++i;
                        DrawTextEx(font, TextFormat("ST: %02X", chipEmu->soundTimer()), {pos.x, pos.y + i * lineSpacing}, 8, 0, chipEmu->soundTimer() == chipEmu->getCopyST() ? LIGHTGRAY : YELLOW);
                    }
                    EndPanel();
                    SetNextWidth(44);
                    BeginPanel("Stack");
                    {
                        auto pos = GetCurrentPos();
                        auto area = GetContentAvailable();
                        pos.x += 0;
                        Space(area.height);
                        auto stackSize = chipEmu->stackSize();
                        const auto* stack = chipEmu->getStackElements();
                        const auto* stackCopy = chipEmu->getCopyStackElements();
                        for (int i = 0; i < stackSize; ++i) {
                            DrawTextEx(font, TextFormat("%X: %03X", i, stack[i]), {pos.x, pos.y + i * lineSpacing}, 8, 0, stack[i] == stackCopy[i] ? LIGHTGRAY : YELLOW);
                        }
                    }
                    EndPanel();
                    SetNextWidth(163);
                    BeginPanel("Memory");
                    {
                        auto pos = GetCurrentPos();
                        auto area = GetContentAvailable();
                        pos.x += 0;
                        Space(area.height);
                        for (int i = 0; i < 23; ++i) {
                            DrawTextEx(font, TextFormat("%04X", chipEmu->getI() + i * 8), {pos.x, pos.y + i * lineSpacing}, 8, 0, LIGHTGRAY);
                            for(int j = 0; j < 8; ++j) {
                                if(chipEmu->memory()[chipEmu->getI() + i * 8 + j] == chipEmu->memoryCopy()[chipEmu->getI() + i * 8 + j]) {
                                    DrawTextEx(font, TextFormat("%02X", chipEmu->memory()[chipEmu->getI() + i * 8 + j]), {pos.x + 30 + j * 16, pos.y + i * lineSpacing}, 8, 0, j&1 ? LIGHTGRAY : GRAY);
                                }
                                else {
                                    DrawTextEx(font, TextFormat("%02X", chipEmu->memory()[chipEmu->getI() + i * 8 + j]), {pos.x + 30 + j * 16, pos.y + i * lineSpacing}, 8, 0, j&1 ? YELLOW : BROWN);
                                }
                            }
                        }
                    }
                    EndPanel();
                    EndColumns();
                    break;
                }
                case eVIDEO: {
                    lastView = mainView;
                    gridScale = screenWidth / chipEmu->getCurrentScreenWidth();
                    video = {0, 20, (float)screenWidth, (float)screenHeight - 36};
                    drawScreen(video, gridScale);
                    break;
                }
                case eEDITOR:
                    lastView = mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Editor", {1,1});
                    {
                        auto rect = GetContentAvailable();
                        //Space(rect.height);
#ifdef WITH_EDITOR
                        //BeginScissorMode(rect.x, rect.y - 1, rect.width, rect.height);
                        editor.draw(font, {rect.x, rect.y - 1, rect.width, rect.height});
                        //EndScissorMode();
#endif
                    }
                    EndPanel();
                    End();
                    break;
                case eSETTINGS: {
                    lastView = mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Settings");
                    {
                        BeginColumns();
                        SetNextWidth(320);
                        BeginGroupBox("Emulation Speed");
                        Space(5);
                        SetIndent(180);
                        SetRowHeight(20);
                        Spinner("Instructions per frame", &options.instructionsPerFrame, 0, 500000);
                        Spinner("Frame boost", &frameBoost, 1, 100000);
                        _frameBoost = getFrameBoost();
                        EndGroupBox();
                        Space(20);
                        SetNextWidth(screenWidth - 373);
                        Begin();
                        Label("Opcode variant:");
                        if(DropdownBox("CHIP-8;CHIP-10;CHIP-48;SCHIP 1.0;SCHIP 1.1;XO-CHIP", &behaviorSel)) {
                            auto preset = static_cast<emu::Chip8EmulatorOptions::SupportedPreset>(behaviorSel);
                            setEmulatorPresetsTo(preset);
                        }
                        End();
                        EndColumns();
                        Space(16);
                        BeginGroupBox("Quirks");
                        Space(5);
                        BeginColumns();
                        SetNextWidth(GetContentAvailable().width/2);
                        Begin();
                        emu::Chip8EmulatorOptions oldOptions = options;
                        options.optJustShiftVx = CheckBox("8xy6/8xyE just shift VX", options.optJustShiftVx);
                        options.optDontResetVf = CheckBox("8xy1/8xy2/8xy3 don't reset VF", options.optDontResetVf);
                        bool oldInc = !(options.optLoadStoreIncIByX | options.optLoadStoreDontIncI);
                        bool newInc = CheckBox("Fx55/Fx65 increment I by X + 1", oldInc);
                        if(newInc != oldInc) {
                            options.optLoadStoreIncIByX = !newInc;
                            options.optLoadStoreDontIncI = false;
                        }
                        oldInc = options.optLoadStoreIncIByX;
                        options.optLoadStoreIncIByX = CheckBox("Fx55/Fx65 increment I only by X", options.optLoadStoreIncIByX);
                        if(options.optLoadStoreIncIByX != oldInc) {
                            options.optLoadStoreDontIncI = false;
                        }
                        oldInc = options.optLoadStoreDontIncI;
                        options.optLoadStoreDontIncI = CheckBox("Fx55/Fx65 don't increment I", options.optLoadStoreDontIncI);
                        if(options.optLoadStoreDontIncI != oldInc) {
                            options.optLoadStoreIncIByX = false;
                        }
                        options.optJump0Bxnn = CheckBox("Bxnn/jump0 uses Vx", options.optJump0Bxnn);
                        End();
                        Begin();
                        options.optWrapSprites = CheckBox("Wrap sprite pixels", options.optWrapSprites);
                        options.optInstantDxyn = CheckBox("Dxyn doesn't wait for vsync", options.optInstantDxyn);
                        bool oldAllowHires = options.optAllowHires;
                        options.optAllowHires = CheckBox("128x64 hires support", options.optAllowHires);
                        if(!options.optAllowHires && oldAllowHires)
                            options.optOnlyHires = false;
                        bool oldOnlyHires = options.optOnlyHires;
                        options.optOnlyHires = CheckBox("Only 128x64 mode", options.optOnlyHires);
                        if(options.optOnlyHires && !oldOnlyHires)
                            options.optAllowHires = true;
                        options.optAllowColors = CheckBox("Multicolor support", options.optAllowColors);
                        options.optXOChipSound = CheckBox("XO-CHIP sound engine", options.optXOChipSound);
                        End();
                        EndColumns();
                        EndGroupBox();
                        Space(30);
                        if(std::memcmp(&oldOptions, &options, sizeof(emu::Chip8EmulatorOptions)) != 0) {
                            updateEmulatorOptions();
                        }
                        auto pos = GetCurrentPos();
                        Space(screenHeight - pos.y - 20 - 16);
                        SetIndent(110);
                        Label("(C) 2022 by Steffen '@gulrak' Schümann");
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
#if 0
                        static Vector2 scroll{0,0};
                        static Librarian::Info selectedInfo;
                        SetRowHeight(16);
                        auto area = GetContentAvailable();
                        BeginTableView(area.height - 48, 4, &scroll);
                        for(int i = 0; i < librarian.numEntries(); ++i) {
                            const auto& info = librarian.getInfo(i);
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
                                Label(GuiIconText(icon, ""));
                            }
                            if(TableNextColumn(.6f)) {
                                if(LabelButton(info.filePath.c_str())) {
                                    if(info.type == Librarian::Info::eDIRECTORY) {
                                        if(info.filePath != "..") {
                                            librarian.intoDir(info.filePath);
                                        }
                                        else {
                                            librarian.parentDir();
                                        }
                                        selectedInfo.analyzed = false;
                                    }
                                    else if(info.type == Librarian::Info::eOCTO_SOURCE) {
                                        selectedInfo = info;
                                    }
                                    else if(info.type == Librarian::Info::eROM_FILE) {
                                        selectedInfo = info;
                                    }
                                }
                            }
                            if(TableNextColumn(.15f))
                                Label(info.type == Librarian::Info::eDIRECTORY ? "" : TextFormat("%8d", info.fileSize));
                            if(TableNextColumn(.2f))
                                Label(date::format("%F", date::floor<std::chrono::seconds>(info.changeDate)).c_str());
                        }
                        EndTableView();
                        //Space(5);
                        Label(TextFormat("ROM: %s", (selectedInfo.analyzed ? selectedInfo.filePath.c_str() : "<nothing selected>")));
                        Label(TextFormat("Estimated minimum opcode variant: %s", selectedInfo.minimumOpcodeProfile().c_str()));
                        SetNextWidth(80);
                        SetIndent(32);
                        if(!selectedInfo.analyzed) GuiDisable();
                        if(Button("Load") && selectedInfo.analyzed) {
                            if(selectedInfo.variant != options.behaviorBase) {
                                options = emu::Chip8EmulatorOptions::optionsOfPreset(selectedInfo.variant);
                                updateEmulatorOptions();
                            }
                            loadRom(librarian.fullPath(selectedInfo.filePath).c_str());
                            mainView = lastView;
                        }
                        GuiEnable();
                        BeginColumns();
                        EndColumns();
#endif
                    }
                    EndPanel();
                    End();
                    if(IsKeyPressed(KEY_ESCAPE))
                        mainView = lastView;
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
                        mainView = lastView;
                    break;
                }
            }
            EndGui();
        }
        if(chipEmu->execMode() != ExecMode::ePAUSED) {
            instructionOffset = -1;
            chipEmu->copyState();
        }
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
        if(TextBox(currentDirectory, 4096)) {
            librarian.fetchDir(currentDirectory);
            currentDirectory = librarian.currentDirectory();
        }
        Space(1);
        BeginTableView(area.height - 54, 4, &scroll);
        for(int i = 0; i < librarian.numEntries(); ++i) {
            const auto& info = librarian.getInfo(i);
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
                Label(GuiIconText(icon, ""));
            }
            if(TableNextColumn(.6f)) {
                if(LabelButton(info.filePath.c_str())) {
                    if(info.type == Librarian::Info::eDIRECTORY) {
                        if(info.filePath != "..") {
                            librarian.intoDir(info.filePath);
                            currentDirectory = librarian.currentDirectory();
                            selectedInfo.analyzed = false;
                            if(mode == eLOAD)
                                currentFileName = "";
                        }
                        else {
                            librarian.parentDir();
                            currentDirectory = librarian.currentDirectory();
                            selectedInfo.analyzed = false;
                            if(mode == eLOAD)
                                currentFileName = "";
                        }
                        selectedInfo.analyzed = false;
                    }
                    else if(info.type == Librarian::Info::eOCTO_SOURCE) {
                        selectedInfo = info;
                        currentFileName = info.filePath;
                    }
                    else if(info.type == Librarian::Info::eROM_FILE) {
                        selectedInfo = info;
                        currentFileName = info.filePath;
                    }
                }
            }
            if(TableNextColumn(.15f))
                Label(info.type == Librarian::Info::eDIRECTORY ? "" : TextFormat("%8d", info.fileSize));
            if(TableNextColumn(.2f) && info.filePath != "..")
                Label(date::format("%F", date::floor<std::chrono::seconds>(info.changeDate)).c_str());
        }
        EndTableView();
#endif
        Space(1);
        BeginColumns();
        SetNextWidth(25);
        Label("File:");
        TextBox(currentFileName, 4096);
        EndColumns();
        Space(2);
        switch(mode) {
            case eLOAD: {
                Label(TextFormat("Estimated minimum opcode variant: %s", selectedInfo.minimumOpcodeProfile().c_str()));
                Space(3);
                SetNextWidth(80);
                SetIndent(32);
                if(!selectedInfo.analyzed) GuiDisable();
                if(Button("Load") && selectedInfo.analyzed) {
                    if(selectedInfo.variant != options.behaviorBase) {
                        options = emu::Chip8EmulatorOptions::optionsOfPreset(selectedInfo.variant);
                        updateEmulatorOptions();
                    }
                    loadRom(librarian.fullPath(selectedInfo.filePath).c_str());
                    mainView = lastView;
                }
                GuiEnable();
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
                if(currentFileName.empty() && ((activeType == 0 && romImage.empty()) || (activeType == 1 && editor.getText().empty()))) GuiDisable();
                if(Button("Save") && !currentFileName.empty()) {
                    if (activeType == 0 && fs::path(currentFileName).extension() != romExtension()) {
                        if (fs::path(currentFileName).has_extension())
                            currentFileName = fs::path(currentFileName).replace_extension(romExtension()).string();
                        else
                            currentFileName += romExtension();
                    }
                    else if (activeType == 1 && fs::path(currentFileName).extension() != ".8o") {
                        if (fs::path(currentFileName).has_extension())
                            currentFileName = fs::path(currentFileName).replace_extension(".8o").string();
                        else
                            currentFileName += ".8o";
                    }
#ifdef PLATFORM_WEB
                    auto targetFile = currentFileName;
#else
                    auto targetFile = librarian.fullPath(currentFileName);
#endif
                    if(activeType == 0) {
                        writeFile(targetFile, (const char*)romImage.data(), romImage.size());
                    }
                    else {
                        writeFile(targetFile, editor.getText().data(), editor.getText().size());
                    }
#ifdef PLATFORM_WEB
                    // can only use path-less filenames
                    emscripten_run_script(TextFormat("saveFileFromMEMFSToDisk('%s','%s')", targetFile.c_str(), targetFile.c_str()));
#endif
                    mainView = lastView;
                }
                GuiEnable();
                break;
            }
        }
        BeginColumns();
        EndColumns();
    }

#ifdef PLATFORM_WEB
    void loadFileWeb()
    {
        //
        openFileCallback = [&](const std::string& filename)
        {
            loadRom(filename.c_str());
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
            open_file_element.accept = '.ch8,.ch10,.sc8,.xo8,.c8b,.8o';
            open_file_element.click();
        });
    }
#endif

    const std::string& romExtension()
    {
        static std::string extensions[] = {".ch8", ".sc10", ".sc8", ".xo8"};
        switch(options.behaviorBase) {
            case emu::Chip8EmulatorOptions::eCHIP10: return extensions[1];
            case emu::Chip8EmulatorOptions::eSCHIP10:
            case emu::Chip8EmulatorOptions::eSCHIP11: return extensions[2];
            case emu::Chip8EmulatorOptions::eXOCHIP: return extensions[3];
            default: return extensions[0];
        }
    }

    void setEmulatorPresetsTo(emu::Chip8EmulatorOptions::SupportedPreset preset)
    {
        options = emu::Chip8EmulatorOptions::optionsOfPreset(preset);
        frameBoost = 1;
        updateEmulatorOptions();
    }

    void updateEmulatorOptions()
    {
        std::scoped_lock lock(audioMutex);
        chipEmu = emu::Chip8EmulatorBase::create(*this, emu::IChip8Emulator::eCHIP8HT, options, chipEmu.get());
        behaviorSel = options.behaviorBase != emu::Chip8EmulatorOptions::eCHICUEYI ? options.behaviorBase : emu::Chip8EmulatorOptions::eXOCHIP;
    }

    void loadRom(const char* filename)
    {
        if (strlen(filename) < 4095) {
            unsigned int size = 0;
            bool valid = false;
            customPalette = false;
            chipEmu->reset();
#ifdef WITH_EDITOR
            editor.setText("");
#endif
            instructionOffset = -1;
            if(endsWith(filename, ".8o")) {
                std::string source = loadTextFile(filename);
                Chip8Compiler c8c;
                if(c8c.compile(source))
                {
                    romImage.assign(c8c.code(), c8c.code() + c8c.codeSize());
#ifdef WITH_EDITOR
                    editor.setText(source);
                    mainView = eEDITOR;
#endif
                    valid = true;
                }
            }
            else if(endsWith(filename, ".ch8")) {
                if (size < chipEmu->memSize() - 512) {
                    romImage = loadFile(filename);
                    valid = true;
                }
            }
            else if(endsWith(filename, ".ch10")) {
                options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eCHIP10);
                updateEmulatorOptions();
                if (size < chipEmu->memSize() - 512) {
                    romImage = loadFile(filename);
                    valid = true;
                }
            }
            else if(endsWith(filename, ".sc8")) {
                options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eSCHIP11);
                updateEmulatorOptions();
                if (size < chipEmu->memSize() - 512) {
                    romImage = loadFile(filename);
                    valid = true;
                }
            }
            else if(endsWith(filename, ".xo8")) {
                options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eXOCHIP);
                updateEmulatorOptions();
                if (size < chipEmu->memSize() - 512) {
                    romImage = loadFile(filename);
                    valid = true;
                }
            }
            else if(endsWith(filename, ".c8b")) {
                C8BFile c8b;
                if(c8b.load(filename) == C8BFile::eOK) {
                    if(!c8b.palette.empty()) {
                        customPalette = true;
                        auto numCol = std::max(c8b.palette.size(), (size_t)16);
                        for(size_t i = 0; i < numCol; ++i) {
                            colorPalette[i] = be32(ColorToInt({c8b.palette[i].r, c8b.palette[i].g, c8b.palette[i].b, 255}));
                        }
                    }
                    uint16_t codeOffset = 0;
                    uint16_t codeSize = 0;
                    auto iter = c8b.findBestMatch({C8BFile::C8V_XO_CHIP, C8BFile::C8V_SCHIP_1_1, C8BFile::C8V_SCHIP_1_0, C8BFile::C8V_CHIP_48, C8BFile::C8V_CHIP_10, C8BFile::C8V_CHIP_8});
                    if(iter != c8b.variantBytecode.end()) {
                        codeOffset = iter->second.first;
                        codeSize = iter->second.second;
                        switch (iter->first) {
                            case C8BFile::C8V_XO_CHIP:
                                setEmulatorPresetsTo(emu::Chip8EmulatorOptions::eXOCHIP);
                                break;
                            case C8BFile::C8V_SCHIP_1_1:
                                setEmulatorPresetsTo(emu::Chip8EmulatorOptions::eSCHIP11);
                                break;
                            case C8BFile::C8V_SCHIP_1_0:
                                setEmulatorPresetsTo(emu::Chip8EmulatorOptions::eSCHIP10);
                                break;
                            case C8BFile::C8V_CHIP_48:
                                setEmulatorPresetsTo(emu::Chip8EmulatorOptions::eCHIP48);
                                break;
                            case C8BFile::C8V_CHIP_10:
                                setEmulatorPresetsTo(emu::Chip8EmulatorOptions::eCHIP10);
                                break;
                            case C8BFile::C8V_CHIP_8:
                                setEmulatorPresetsTo(emu::Chip8EmulatorOptions::eCHIP8);
                                break;
                            default:
                                setEmulatorPresetsTo(emu::Chip8EmulatorOptions::eSCHIP11);
                                break;
                        }
                        if(c8b.executionSpeed > 0) {
                            options.instructionsPerFrame = c8b.executionSpeed;
                        }
                        romImage.assign(c8b.rawData.data() + codeOffset, c8b.rawData.data() + codeOffset + codeSize);
                        valid = true;
                    }
                    else {
                        customPalette = false;
                        chipEmu->reset();
                    }
                }
            }
            if (valid) {
                romSha1Hex = calculateSha1Hex(romImage.data(), romImage.size());
                romName = filename;
                std::memcpy(chipEmu->memory() + 512, romImage.data(), romImage.size());
#ifdef WITH_EDITOR
                if(editor.isEmpty()) {
                    std::stringstream os;
                    emu::Chip8Decompiler decomp;
                    decomp.decompile(filename, romImage.data(), 0x200, romImage.size(), 0x200, &os, false, true);
                    editor.setText(os.str());
                }
#endif
            }
            //memory[0x1FF] = 3;
        }
    }

    void reloadRom()
    {
        if(!romImage.empty()) {
            unsigned int size = 0;
            chipEmu->reset();
            instructionOffset = -1;
            std::memcpy(chipEmu->memory() + 512, romImage.data(), romImage.size());
        }
    }

    bool windowShouldClose() const
    {
        return shouldClose || WindowShouldClose();
    }

private:
    std::mutex audioMutex;
    ResourceManager resources;
    Image fontImage{};
    Image microFont{};
    Image titleImage{};
    Font font{};
    Image screen{};
    Texture2D titleTexture{};
    Texture2D screenTexture{};
    bool shouldClose{false};
    int screenWidth{};
    int screenHeight{};
    RenderTexture renderTexture{};
    AudioStream audioStream{};
    bool scaleBy2{false};
    bool customPalette{false};
    std::unique_ptr<emu::IChip8Emulator> chipEmu;
    emu::Chip8EmulatorOptions options;
    int behaviorSel{0};
    //float messageTime{};
    std::string timedMessage;
    bool updateScreen{false};
    int frameBoost{1};
    int memoryOffset{-1};
    int instructionOffset{-1};
    std::string currentDirectory;
    std::string currentFileName;
    std::string romName;
    std::vector<uint8_t> romImage;
    std::string romSha1Hex;
    std::array<uint32_t, 16> colorPalette{};
    volatile bool grid{false};
    MainView mainView{eDEBUGGER};
    MainView lastView{eDEBUGGER};
    Librarian librarian;
#ifdef WITH_EDITOR
    Editor editor;
#endif

    inline static KeyboardKey keyMapping[16] = {KEY_X, KEY_ONE, KEY_TWO, KEY_THREE, KEY_Q, KEY_W, KEY_E, KEY_A, KEY_S, KEY_D, KEY_Z, KEY_C, KEY_FOUR, KEY_R, KEY_F, KEY_V};
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
    auto height = chip8.getCurrentScreenHeight();
    const auto* buffer = chip8.getScreenBuffer();
    result.reserve(width*height+1);
    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            result.push_back(buffer[y*width + x] ? '#' : ' ');
        }
        result.push_back('\n');
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

static std::map<std::string, emu::Chip8EmulatorOptions::SupportedPreset> presetMap = {
    {"chip8", emu::Chip8EmulatorOptions::eCHIP8},
    {"chip10", emu::Chip8EmulatorOptions::eCHIP10},
    {"chip48", emu::Chip8EmulatorOptions::eCHIP48},
    {"schip10", emu::Chip8EmulatorOptions::eSCHIP10},
    {"superchip10", emu::Chip8EmulatorOptions::eSCHIP10},
    {"schip11", emu::Chip8EmulatorOptions::eSCHIP11},
    {"superchip11", emu::Chip8EmulatorOptions::eSCHIP11},
    {"xochip", emu::Chip8EmulatorOptions::eXOCHIP},
    {"chicueyi", emu::Chip8EmulatorOptions::eCHICUEYI}
};

int main(int argc, char* argv[])
{
    auto preset = emu::Chip8EmulatorOptions::eXOCHIP;
#ifndef PLATFORM_WEB
    ghc::CLI cli(argc, argv);
    int64_t traceLines = -1;
    bool compareRun = false;
    bool benchmark = false;
    bool showHelp = false;
    int64_t execSpeed = -1;
    std::string romFile;
    std::string presetName = "xo-chip";
    cli.option({"-h", "--help"}, showHelp, "Show this help text");
    cli.option({"-t", "--trace"}, traceLines, "Run headless and dump given number of trace lines");
    cli.option({"-c", "--compare"}, compareRun, "Run and compare with reference engine, trace until diff");
    cli.option({"-r", "--rom"}, romFile, "ROM file to load");
    cli.option({"-b", "--benchmark"}, benchmark, "Run benchmark against octo-c");
    cli.option({"-p", "--preset"}, presetName, "Select CHIP-8 preset to use: chip-8, chip-10, chip-48, schip1.0, schip1.1, xo-chip");
    cli.option({"-s", "--exec-speed"}, execSpeed, "Set execution speed in instructions per frame (0-500000, 0: unlimited)");
    cli.parse();

    if(showHelp) {
        cli.usage();
        exit(0);
    }
    if(!presetName.empty()) {
        auto presetUnified = presetName;
        {
            auto iter = std::remove_if(presetUnified.begin(), presetUnified.end(), [](unsigned char c) { return std::ispunct(c); });
            presetUnified.erase(iter, presetUnified.end());
            std::transform(presetUnified.begin(), presetUnified.end(), presetUnified.begin(), [](unsigned char c){ return std::tolower(c); });
        }
        auto iter = presetMap.find(presetUnified);
        if(iter == presetMap.end()) {
            std::cerr << "ERROR: Unsupported preset '" << presetName << "', check help for supported presets." << std::endl;
            exit(1);
        }
        preset = iter->second;
    }
    auto chip8options = emu::Chip8EmulatorOptions::optionsOfPreset(preset);
    if(execSpeed >= 0) {
        chip8options.instructionsPerFrame = execSpeed;
    }
    if(traceLines < 0 && !compareRun && !benchmark) {
#else
    auto chip8options = emu::Chip8EmulatorOptions::optionsOfPreset(preset);
    {
#endif
        const int screenWidth = 512;
        const int screenHeight = 256 + 36;

        SetConfigFlags(FLAG_COCOA_GRAPHICS_SWITCHING);
        InitWindow(screenWidth, screenHeight, "Cadmium - A CHIP-8 derivate environment");
        SetExitKey(0);
        {
            Cadmium cadmium(screenWidth, screenHeight, chip8options);
#ifndef PLATFORM_WEB
            if (!romFile.empty()) {
                cadmium.loadRom(romFile.c_str());
            }
#endif

#if defined(PLATFORM_WEB)
            emscripten_set_main_loop_arg(Cadmium::updateAndDrawFrame, &cadmium, 60, 1);
#else
            SetTargetFPS(60);
            while (!cadmium.windowShouldClose()) {
                cadmium.updateAndDraw();
            }
#endif
        }
        CloseWindow();
    }
#ifndef PLATFORM_WEB
    else {
        emu::Chip8HeadlessHost host(chip8options);
        //chip8options.optHas16BitAddr = true;
        //chip8options.optWrapSprites = true;
        //chip8options.optAllowColors = true;
        //chip8options.optJustShiftVx = false;
        //chip8options.optLoadStoreDontIncI = false;
        chip8options.optDontResetVf = true;
        chip8options.optInstantDxyn = true;
        auto chip8 = emu::Chip8EmulatorBase::create(host, emu::IChip8Emulator::eCHIP8HT, chip8options);
        std::clog << "Engine1: " << chip8->name() << std::endl;
        octo_emulator octo;
        octo_options oopt{};
        oopt.q_clip = 1;
        //oopt.q_loadstore = 1;

        chip8->reset();
        if(!romFile.empty()) {
            unsigned int size = 0;
            uint8_t* data = LoadFileData(romFile.c_str(), &size);
            if (size < 4096 - 512) {
                std::memcpy(chip8->memory() + 512, data, size);
            }
            UnloadFileData(data);
            //chip8.loadRom(romFile.c_str());
        }
        octo_emulator_init(&octo, (char*)chip8->memory() + 512, 4096 - 512, &oopt, nullptr);
        std::clog << "Engine2: C-Octo" << std::endl;
        int64_t i = 0;
        //for(int i = 0; i < traceLines; ++i) {
        if(compareRun) {
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
                    std::clog << i << ": " << chip8->dumStateLine() << std::endl;
                    std::clog << i << "| " << dumOctoStateLine(&octo) << std::endl;
                }
                if(!(i % 500000)) {
                    std::cout << chip8EmuScreen(*chip8);
                }
                ++i;
            } while ((i & 0xfff) || (chip8->dumStateLine() == dumOctoStateLine(&octo) && chip8EmuScreen(*chip8) == octoScreen(octo)));
            std::clog << i << ": " << chip8->dumStateLine() << std::endl;
            std::clog << i << "| " << dumOctoStateLine(&octo) << std::endl;
            std::cerr << chip8EmuScreen(*chip8);
            std::cerr << "---" << std::endl;
            std::cerr << octoScreen(octo) << std::endl;
        }
        else if(benchmark) {
            //const uint32_t BENCHMARK_INSTRUCTIONS = 100000000;
            const uint32_t BENCHMARK_INSTRUCTIONS = 3800000000u;
            uint32_t instructions = BENCHMARK_INSTRUCTIONS;
            std::cout << "Executing benchmark..." << std::endl;
            auto startChip8 = std::chrono::steady_clock::now();
            while(--instructions && chip8->execMode() == emu::IChip8Emulator::eRUNNING) {
                if ((instructions & 7) == 0) {
                    chip8->handleTimer();
                }
                chip8->executeInstruction();
            }
            auto durationChip8 = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startChip8);
            std::cout << "Executed instructions: " << chip8->cycles() << std::endl;
            std::cout << "Cadmium: " << durationChip8.count() << "ms, " << int(double(chip8->cycles())/durationChip8.count()/1000) << "MIPS" << std::endl;

            instructions = chip8->cycles();
            auto startOcto = std::chrono::steady_clock::now();
            while(--instructions) {
                if ((instructions & 7) == 0) {
                    if (octo.dt)
                        --octo.dt;
                    if (octo.st)
                        --octo.st;
                }
                octo_emulator_instruction(&octo);
            }
            auto durationOcto = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startOcto);
            std::cout << "Octo:    " << durationOcto.count() << "ms, " << int(double(chip8->cycles())/durationOcto.count()/1000) << "MIPS" << std::endl;
        }
        else if(traceLines >= 0) {
            do {
                std::cout << i << "/" << chip8->cycles() << ": " << chip8->dumStateLine() << std::endl;
                if ((i % chip8options.instructionsPerFrame) == 0) {
                    chip8->handleTimer();
                }
                chip8->executeInstruction();
                ++i;
            } while (i < traceLines || (traceLines == 0 && chip8->execMode() == emu::IChip8Emulator::ExecMode::eRUNNING));
            std::cout << chip8EmuScreen(*chip8);
        }
    }
#endif
    return 0;
}
