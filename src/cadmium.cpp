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
#include <rlgl.h>
#include <rlguipp/rlguipp.hpp>
#include <rlguipp/rlguipp_filedialog.hpp>
#include <rlguipp/rlguipp_font.hpp>
#include <stylemanager.hpp>
#include "configuration.hpp"

#include <raymath.h>

#include <date/date.h>
#include <fmt/format.h>
#include <stdendian/stdendian.h>
#include <about.hpp>
#include <chiplet/chip8decompiler.hpp>
#include <chiplet/utility.hpp>
#include <circularbuffer.hpp>
#include <debugger.hpp>
#include <emuhostex.hpp>
#include <emulation/c8bfile.hpp>
//#include <emulation/chip8generic.hpp>
#include <emulation/coreregistry.hpp>
//#include <emulation/dream6800.hpp>
#include <c8db/database.hpp>
#include <emulation/time.hpp>
#include <emulation/timecontrol.hpp>
#include <ghc/cli.hpp>
#include <ghc/utf8.hpp>
#include <headlessrun.hpp>
#include <logview.hpp>
#include <nlohmann/json.hpp>
#include <resourcemanager.hpp>
#include <systemtools.hpp>
#include <texturescaler.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <list>
#include <memory>
#include <mutex>
#include <new>
#include <regex>
#include <thread>

#include "emulation/chip8strict.hpp"

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

extern "C" EMSCRIPTEN_KEEPALIVE void openFileCallbackC(const char* name, const uint8_t* data, size_t size);
extern "C" EMSCRIPTEN_KEEPALIVE void webPasteCallbackC(const char* text, size_t size);
static std::function<void(std::string_view, ghc::span<const uint8_t>)> openFileCallback;
static std::string webClip;
static bool webPastePending = false;

void openFileCallbackC(const char* name, const uint8_t* data, size_t size)
{
    if (openFileCallback)
        openFileCallback(name, {data, size});
}

void webPasteCallbackC(const char* text, size_t size)
{
    webClip.assign(text, size);
    webPastePending = true;
}

EM_JS(void, webInitClipboard, (), {
    if (Module.cadmiumClipboardInitialized) return;
    Module.cadmiumClipboardInitialized = true;
    document.addEventListener('paste', function(event) {
        if (document.activeElement !== Module.canvas) return;
        const text = event.clipboardData.getData('text/plain');
        const size = lengthBytesUTF8(text);
        const data = _malloc(size + 1);
        stringToUTF8(text, data, size + 1);
        Module._webPasteCallbackC(data, size);
        _free(data);
        event.preventDefault();
    });
});

EM_JS(void, webWriteClipboard, (const char* text), {
    const value = UTF8ToString(text);
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(value).catch(function(error) {
            console.warn('Cadmium could not write to the clipboard:', error);
        });
        return;
    }
    const input = document.createElement('textarea');
    input.value = value;
    input.style.position = 'fixed';
    input.style.opacity = '0';
    document.body.appendChild(input);
    input.select();
    document.execCommand('copy');
    input.remove();
    Module.canvas.focus();
});

EM_JS(void, webRequestOpenFile, (const char* accept), {
    if (Module.cadmiumFileInput) {
        Module.cadmiumFileInput.value = "";
        Module.cadmiumFileInput.accept = UTF8ToString(accept);
        Module.cadmiumFileInput.click();
    }
});

EM_JS(void, webInitFilePicker, (const char* accept, int logicalWidth, int logicalHeight), {
    const acceptedTypes = UTF8ToString(accept);
    if (!Module.cadmiumFilePickerInitialized) {
        Module.cadmiumFilePickerInitialized = true;
        const input = document.createElement('input');
        input.type = 'file';
        input.hidden = true;
        input.addEventListener('change', async function() {
            const file = input.files && input.files[0];
            if (!file) return;
            try {
                const bytes = new Uint8Array(await file.arrayBuffer());
                const nameSize = lengthBytesUTF8(file.name);
                const name = _malloc(nameSize + 1);
                const data = _malloc(bytes.length || 1);
                stringToUTF8(file.name, name, nameSize + 1);
                HEAPU8.set(bytes, data);
                Module._openFileCallbackC(name, data, bytes.length);
                _free(data);
                _free(name);
            } catch (error) {
                console.error('Cadmium could not open the selected file:', error);
            } finally {
                Module.canvas.focus();
            }
        });
        document.body.appendChild(input);
        Module.cadmiumFileInput = input;
        Module.canvas.addEventListener('pointerdown', function(event) {
            const bounds = Module.canvas.getBoundingClientRect();
            const x = (event.clientX - bounds.left) * logicalWidth / bounds.width;
            const y = (event.clientY - bounds.top) * logicalHeight / bounds.height;
            if (x >= 20 && x < 40 && y >= 0 && y < 20) {
                Module.cadmiumFileInput.value = "";
                Module.cadmiumFileInput.accept = acceptedTypes;
                Module.cadmiumFileInput.click();
            }
        });
    }
});

EM_JS(void, webDownloadFile, (const char* name, const uint8_t* data, size_t size, const char* mimeType), {
    const bytes = HEAPU8.slice(data, data + size);
    const blob = new Blob([bytes], {type: UTF8ToString(mimeType)});
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = UTF8ToString(name);
    link.rel = 'noopener';
    document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(function() { URL.revokeObjectURL(url); }, 1000);
});
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

#include <editor.hpp>
#include <backgroundjob.hpp>

inline Color toRaylibColor(const emu::Palette::Color& col)
{
    return {col.r, col.g, col.b, col.a};
}

inline emu::Palette::Color toEmuColor(const Color& col)
{
    return {col.r, col.g, col.b, col.a};
}

#define CHIP8_STYLE_PROPS_COUNT 16
static const GuiStyleProp chip8StyleProps[CHIP8_STYLE_PROPS_COUNT] = {
    {0, 0, 0x2f7486ff},   // DEFAULT_BORDER_COLOR_NORMAL
    {0, 1, 0x024658ff},   // DEFAULT_BASE_COLOR_NORMAL
    {0, 2, 0x51bfd3ff},   // DEFAULT_TEXT_COLOR_NORMAL
      {0, 3, (int)0x82cde0ff},   // B! DEFAULT_BORDER_COLOR_FOCUSED
      {0, 4, 0x3299b4ff},   // A! DEFAULT_BASE_COLOR_FOCUSED
//    {0, 5, (int)0xb6e1eaff},   // DEFAULT_TEXT_COLOR_FOCUSED
    {0, 5, (int)0xeff8ffff},   // DEFAULT_TEXT_COLOR_FOCUSED
      {0, 6, (int)0x82cde0ff},   // B! DEFAULT_BORDER_COLOR_PRESSED
      {0, 7, 0x3299b4ff},   // A! DEFAULT_BASE_COLOR_PRESSED
    {0, 8, (int)0xeff8ffff},   // DEFAULT_TEXT_COLOR_PRESSED
    {0, 9, 0x134b5aff},   // DEFAULT_BORDER_COLOR_DISABLED
    {0, 10, 0x0e273aff},  // DEFAULT_BASE_COLOR_DISABLED
    {0, 11, 0x17505fff},  // DEFAULT_TEXT_COLOR_DISABLED
    {0, 16, 0x0000000e},  // DEFAULT_TEXT_SIZE
    {0, 17, 0x00000000},  // DEFAULT_TEXT_SPACING
    {0, 18, (int)0x81c0d0ff},  // DEFAULT_LINE_COLOR
    {0, 19, 0x00222bff},  // DEFAULT_BACKGROUND_COLOR
};


std::string GetClipboardTextX()
{
#ifdef PLATFORM_WEB
    webPastePending = false;
    return webClip;
#else
    return GetClipboardText();
#endif
}

void SetClipboardTextX(const std::string& text)
{
#ifdef PLATFORM_WEB
    webClip = text;
    webWriteClipboard(text.c_str());
#else
    SetClipboardText(text.c_str());
#endif
}

bool isClipboardPaste()
{
#ifdef PLATFORM_WEB
    return webPastePending;
#else
    return false;
#endif
}

void CenterWindow(int width, int height)
{
    auto monitor = GetCurrentMonitor();
    SetWindowPosition((GetMonitorWidth(monitor) - width) / 2, (GetMonitorHeight(monitor) - height) / 2);
}

void LogHandler(int msgType, const char *text, va_list args)
{
    static char buffer[4096];
    vsnprintf(buffer, 4095, text, args);
    emu::Logger::log((emu::LogLevel)msgType, buffer);
}

std::atomic_uint8_t g_soundTimer{0};
std::atomic_int g_frameBoost{1};

class Cadmium : public emu::EmuHostEx
{
public:
    using ExecMode = emu::IChip8Emulator::ExecMode;
    using CpuState = emu::IChip8Emulator::CpuState;
    enum MemFlags { eNONE = 0, eBREAKPOINT = 1, eWATCHPOINT = 2 };
    enum MainView { eVIDEO, eDEBUGGER, eEDITOR, eTRACELOG, eSETTINGS, eROM_SELECTOR, eROM_EXPORT, eLIBRARY };
    enum EmulationMode { eCOSMAC_VIP_CHIP8, eGENERIC_CHIP8 };
    enum VideoRenderMode { eFAST, eHIRES };
    enum FileBrowserMode { eLOAD, eSAVE, eWEB_SAVE };
    static constexpr int MIN_SCREEN_WIDTH = 640; // 512;
    static constexpr int MIN_SCREEN_HEIGHT = 480; // 192*2+36;
    explicit Cadmium(CadmiumConfiguration& cfg, emu::Properties& props)
        : EmuHostEx(cfg)
        , _audioBuffer(44100)
        , _screenWidth(MIN_SCREEN_WIDTH)
        , _screenHeight(MIN_SCREEN_HEIGHT)
#ifndef PLATFORM_WEB
        , _editor(_threadPool)
#endif
    {
        SetTraceLogCallback(LogHandler);
#ifdef NDEBUG
        SetTraceLogLevel(LOG_INFO);
#else
        SetTraceLogLevel(LOG_DEBUG);
#endif

    #ifdef RESIZABLE_GUI
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    #else
        // SetConfigFlags(FLAG_VSYNC_HINT);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        _windowInvisible = true;
        if (cfg.scaleMode) {
            _scaleMode = cfg.scaleMode;
        }
        if (!_scaleMode) {
            _scaleMode = (GetMonitorWidth(_currentMonitor) > 1680 || GetWindowScaleDPI().x > 1.0f) ? 2 : 1;
        }
    #endif

        InitWindow(_screenWidth * _scaleMode, _screenHeight * _scaleMode, "Cadmium - A CHIP-8 variant environment");
        SetMouseScale(1.0f/_scaleMode, 1.0f/_scaleMode);
        auto winPos = GetWindowPosition();
        if (cfg.windowPosX != 0xFFFF) {
            winPos.x = cfg.windowPosX;
        }
        if (cfg.windowPosY != 0xFFFF) {
            winPos.y = cfg.windowPosY;
        }
        SetWindowPosition(winPos.x, winPos.y);
        _currentMonitor = GetCurrentMonitor();
        _recentFiles = cfg.recentFiles;
#ifdef RESIZABLE_GUI
        if(GetMonitorWidth(_curentMonitor) > 1680 || GetWindowScaleDPI().x > 1.0f) {
            SetWindowSize(_screenWidth * 2, _screenHeight * 2);
            CenterWindow(_screenWidth * 2, _screenHeight * 2);
        }
#else
        auto scale2d = GetWindowScaleDPI();
        auto monitorResolutionX = GetMonitorWidth(_currentMonitor);
        auto monitorResolutionY = GetMonitorHeight(_currentMonitor);
        TRACELOG(LOG_INFO, fmt::format("WindowScaleDPI: {}x{}", scale2d.x, scale2d.y).c_str());
#endif
        _textureScaler = std::make_unique<TextureScaler>(MIN_SCREEN_WIDTH, MIN_SCREEN_HEIGHT);
        _textureScaler->setOutputSize(_screenWidth * _scaleMode, _screenHeight * _scaleMode);

        SetExitKey(0);

        _instance = this;
        InitAudioDevice();
        SetAudioStreamBufferSizeDefault(1470);
        _audioStream = LoadAudioStream(44100, 16, 1);
        SetAudioStreamCallback(_audioStream, Cadmium::audioInputCallback);
        PlayAudioStream(_audioStream);
        SetTargetFPS(60);

        //_renderTexture = LoadRenderTexture(_screenWidth, _screenHeight);
        //if (GetWindowScaleDPI().x != std::trunc(GetWindowScaleDPI().x))
        //    SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_BILINEAR);
        //else
        //    SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);

#ifdef PLATFORM_WEB
        const char* scanLineShader = R"(
#version 100

precision mediump float;

// Input vertex attributes (from vertex shader)
varying vec2 fragTexCoord;
varying vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// NOTE: Add here your custom variables

float offset = 0.0;
float frequency = 450.0/3.0;

uniform float time;

void main()
{
/*
    // Scanlines method 1
    float tval = 0; //time
    vec2 uv = 0.5 + (fragTexCoord - 0.5)*(0.9 + 0.01*sin(0.5*tval));

    vec4 color = texture2D(texture0, fragTexCoord);

    color = clamp(color*0.5 + 0.5*color*color*1.2, 0.0, 1.0);
    color *= 0.5 + 0.5*16.0*uv.x*uv.y*(1.0 - uv.x)*(1.0 - uv.y);
    color *= vec4(0.8, 1.0, 0.7, 1);
    color *= 0.9 + 0.1*sin(10.0*tval + uv.y*1000.0);
    color *= 0.97 + 0.03*sin(110.0*tval);

    fragColor = color;
*/
    // Scanlines method 2
    float globalPos = (fragTexCoord.y + offset) * frequency;
    float wavePos = cos((fract(globalPos) - 0.5)*3.14);

    vec4 color = texture2D(texture0, fragTexCoord);

    gl_FragColor = mix(vec4(0.0, 0.3, 0.0, 0.0), color, wavePos);
}
)";
#else
        const char* scanLineShader = R"(
#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Output fragment color
out vec4 finalColor;

// NOTE: Add here your custom variables
vec2 res=vec2(128.0/1.0,128.0/1.0);
float hardScan=-10.0;
float hardPix=-2.0;
vec2 warp=vec2(1.0/64.0,1.0/24.0);
float shape=3.0;
float overdrive=1.25;

float ToLinear1(float c){return(c<=0.04045)?c/12.92:pow((c+0.055)/1.055,2.4);}
vec3 ToLinear(vec3 c){return vec3(ToLinear1(c.r),ToLinear1(c.g),ToLinear1(c.b));}

// Linear to sRGB.
// Assuing using sRGB typed textures this should not be needed.
float ToSrgb1(float c){return(c<0.0031308?c*12.92:1.055*pow(c,0.41666)-0.055);}
vec3 ToSrgb(vec3 c){return vec3(ToSrgb1(c.r),ToSrgb1(c.g),ToSrgb1(c.b));}

// Testing only, something to help generate a dark signal for bloom test.
// Set to zero, or remove Test() if using this shader.
#if 1
 vec3 Test(vec3 c){return c*(1.0/64.0)+c*c;}
#else
 vec3 Test(vec3 c){return c;}
#endif

// Nearest emulated sample given floating point position and texel offset.
// Also zero's off screen.
vec3 Fetch(vec2 pos,vec2 off){
  pos=floor(pos*res+off)/res;
  if(max(abs(pos.x-0.5),abs(pos.y-0.5))>0.5)return vec3(0.0,0.0,0.0);
    return Test(ToLinear(texture(texture0,vec2(pos.x,pos.y),-16.0).rgb));}

// Distance in emulated pixels to nearest texel.
vec2 Dist(vec2 pos){pos=pos*res;return -((pos-floor(pos))-vec2(0.5));}

// Try different filter kernels.
float Gaus(float pos,float scale){return exp2(scale*pow(abs(pos),shape));}

// 3-tap Gaussian filter along horz line.
vec3 Horz3(vec2 pos,float off){
  vec3 b=Fetch(pos,vec2(-1.0,off));
  vec3 c=Fetch(pos,vec2( 0.0,off));
  vec3 d=Fetch(pos,vec2( 1.0,off));
  float dst=Dist(pos).x;
  // Convert distance to weight.
  float scale=hardPix;
  float wb=Gaus(dst-1.0,scale);
  float wc=Gaus(dst+0.0,scale);
  float wd=Gaus(dst+1.0,scale);
  // Return filtered sample.
  return (b*wb+c*wc+d*wd)/(wb+wc+wd);}

// 5-tap Gaussian filter along horz line.
vec3 Horz5(vec2 pos,float off){
  vec3 a=Fetch(pos,vec2(-2.0,off));
  vec3 b=Fetch(pos,vec2(-1.0,off));
  vec3 c=Fetch(pos,vec2( 0.0,off));
  vec3 d=Fetch(pos,vec2( 1.0,off));
  vec3 e=Fetch(pos,vec2( 2.0,off));
  float dst=Dist(pos).x;
  // Convert distance to weight.
  float scale=hardPix;
  float wa=Gaus(dst-2.0,scale);
  float wb=Gaus(dst-1.0,scale);
  float wc=Gaus(dst+0.0,scale);
  float wd=Gaus(dst+1.0,scale);
  float we=Gaus(dst+2.0,scale);
  // Return filtered sample.
  return (a*wa+b*wb+c*wc+d*wd+e*we)/(wa+wb+wc+wd+we);}

// 7-tap Gaussian filter along horz line.
vec3 Horz7(vec2 pos,float off){
  vec3 a=Fetch(pos,vec2(-3.0,off));
  vec3 b=Fetch(pos,vec2(-2.0,off));
  vec3 c=Fetch(pos,vec2(-1.0,off));
  vec3 d=Fetch(pos,vec2( 0.0,off));
  vec3 e=Fetch(pos,vec2( 1.0,off));
  vec3 f=Fetch(pos,vec2( 2.0,off));
  vec3 g=Fetch(pos,vec2( 3.0,off));
  float dst=Dist(pos).x;
  // Convert distance to weight.
  float scale=hardPix;
  float wa=Gaus(dst-3.0,scale);
  float wb=Gaus(dst-2.0,scale);
  float wc=Gaus(dst-1.0,scale);
  float wd=Gaus(dst+0.0,scale);
  float we=Gaus(dst+1.0,scale);
  float wf=Gaus(dst+2.0,scale);
  float wg=Gaus(dst+3.0,scale);
  // Return filtered sample.
  return (a*wa+b*wb+c*wc+d*wd+e*we+f*wf+g*wg)/(wa+wb+wc+wd+we+wf+wg);}

// Return scanline weight.
float Scan(vec2 pos,float off){
  float dst=Dist(pos).y;
  return Gaus(dst+off,hardScan);}

// Allow nearest three lines to effect pixel.
vec3 Tri(vec2 pos){
  vec3 a=Horz5(pos,-2.0);
  vec3 b=Horz7(pos,-1.0);
  vec3 c=Horz7(pos, 0.0);
  vec3 d=Horz7(pos, 1.0);
  vec3 e=Horz5(pos, 2.0);
  float wa=Scan(pos,-2.0);
  float wb=Scan(pos,-1.0);
  float wc=Scan(pos, 0.0);
  float wd=Scan(pos, 1.0);
  float we=Scan(pos, 2.0);
  return (a*wa+b*wb+c*wc+d*wd+e*we)*overdrive;}

// Distortion of scanlines, and end of screen alpha.
vec2 Warp(vec2 pos){
  pos=pos*2.0-1.0;
  pos*=vec2(1.0+(pos.y*pos.y)*warp.x,1.0+(pos.x*pos.x)*warp.y);
  return pos*0.5+0.5;}

// Draw dividing bars.
float Bar(float pos,float bar){pos-=bar;return pos*pos<4.0?0.0:1.0;}

// Entry.
void main(){
  vec2 pos=Warp(fragTexCoord.xy/res.xy);
  finalColor.rgb=Tri(pos) * 1.5;//*Mask(fragTexCoord.xy);
  finalColor.a=1.0;
  finalColor.rgb=ToSrgb(finalColor.rgb);
}

/*
// NOTE: Render size values must be passed from code
const float renderWidth = 800;
const float renderHeight = 2048;
float offset = 0.0;

uniform float time;

void main()
{
    float frequency = renderHeight/3.0;
*/
/*
    // Scanlines method 1
    float tval = 0; //time
    vec2 uv = 0.5 + (fragTexCoord - 0.5)*(0.9 + 0.01*sin(0.5*tval));

    vec4 color = texture(texture0, fragTexCoord);

    color = clamp(color*0.5 + 0.5*color*color*1.2, 0.0, 1.0);
    color *= 0.5 + 0.5*16.0*uv.x*uv.y*(1.0 - uv.x)*(1.0 - uv.y);
    color *= vec4(0.8, 1.0, 0.7, 1);
    color *= 0.9 + 0.1*sin(10.0*tval + uv.y*1000.0);
    color *= 0.97 + 0.03*sin(110.0*tval);

    fragColor = color;
*/
/*
    // Scanlines method 2
    float globalPos = (fragTexCoord.y + offset) * frequency;
    float wavePos = cos((fract(globalPos) - 0.5)*3.14);

    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);

    finalColor = mix(vec4(0.0, 0.3, 0.0, 0.0), texelColor, wavePos);
}*/
)";
#endif

        _scanLineShader = LoadShaderFromMemory(NULL, scanLineShader);

//        _styleManager.setDefaultTheme();
        _styleManager.addTheme("dark", 235.0f, 0.1, false);

        _microFont = LoadImage("micro-font.png");
        generateFont();
        if (props) {
            if (props.palette().empty())
                setPalette(_defaultPalette);
            else
                setPalette(props.palette());
        }
        else
            _mainView = eSETTINGS;
        EmuHostEx::updateEmulatorOptions(props);
        _screen = GenImageColor(emu::SUPPORTED_SCREEN_WIDTH, emu::SUPPORTED_SCREEN_HEIGHT, BLACK);
        _overlayTexture = LoadRenderTexture(emu::SUPPORTED_SCREEN_WIDTH,emu::SUPPORTED_SCREEN_HEIGHT);
        _screenTexture = LoadTextureFromImage(_screen);
        _crt = GenImageColor(256,512,BLACK);
        _crtTexture = LoadTextureFromImage(_crt);
        _screenShot = GenImageColor(emu::SUPPORTED_SCREEN_WIDTH, emu::SUPPORTED_SCREEN_HEIGHT, BLACK);
        _screenShotTexture = LoadTextureFromImage(_screen);
        SetTextureFilter(_crtTexture, TEXTURE_FILTER_BILINEAR);
        SetTextureFilter(_screenShotTexture, TEXTURE_FILTER_POINT);
        _titleImage = LoadImage("cadmium-title.png");
        _keyboardOverlay = LoadRenderTexture(40,40);
        updateScreen();
        std::string versionStr(CADMIUM_VERSION);
        drawMicroText(_titleImage, "v" CADMIUM_VERSION, 91u - std::strlen("v" CADMIUM_VERSION)*4, 6, WHITE);
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

        _styleManager.setTheme(0);

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
#ifndef PLATFORM_WEB
        _cfgPath = (fs::path(dataPath())/"config.json").string();
        _volume = _volumeSlider = _cfg.volume;
        _styleManager.updateStyle(_cfg.guiHue, _cfg.guiSat, false);
#else
        webInitClipboard();
        openFileCallback = [this](std::string_view filename, ghc::span<const uint8_t> data) {
            loadBinary(filename, data, LoadOptions::None);
        };
        constexpr const char* acceptedFiles = ".ch8,.hex,.ch10,.hc8,.sc8,.xo8,.c8b,.8o,.gif,.bin,.ram,.c8x,.c8e,.mc8";
        webInitFilePicker(acceptedFiles, _screenWidth, _screenHeight);
#endif
        if(_volume > 1.0f)
            _volume = _volumeSlider = 1.0f;
        updateKeyboardOverlay();
        updateMainMenu();
        SetMasterVolume(_volume);
    }

    ~Cadmium() override
    {
        if(!_cfgPath.empty()) {
            _cfg.workingDirectory = _currentDirectory;
            _cfg.guiHue = _styleManager.getGuiHue();
            _cfg.guiSat = _styleManager.getGuiSaturation();
            saveConfig();
        }
        gui::UnloadGui();
#ifndef PLATFORM_WEB
        if (_database)
            _database.reset();
#endif
        _textureScaler.reset();
        UnloadFont(_font);
        UnloadImage(_fontImage);
        UnloadImage(_microFont);
        //UnloadRenderTexture(_renderTexture);
        UnloadRenderTexture(_overlayTexture);
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
    }

    void updateResolution()
    {
        static unsigned counter = 0;
        if (++counter > 120) {
            counter = 0;
            _currentMonitor = GetCurrentMonitor(); // surprisingly expensive call
        }
        if (_windowInvisible) {
            ClearWindowState(FLAG_WINDOW_HIDDEN);
            _windowInvisible = false;
        }
        if (!_scaleMode || GetMonitorWidth(_currentMonitor) <= _screenWidth * _scaleMode) {
            _scaleMode = 1;
        }

#if defined(PLATFORM_WEB) && !defined(RESIZABLE_GUI)
        // CSS controls the displayed size, not the WebGL backing resolution.
        // Keep the GUI logical size fixed, but let the video overlay use all
        // available device pixels instead of enlarging a 640x480 canvas.
        double cssWidth = 0.0, cssHeight = 0.0;
        if (emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight) == EMSCRIPTEN_RESULT_SUCCESS &&
            cssWidth > 0.0 && cssHeight > 0.0) {
            const double pixelRatio = emscripten_get_device_pixel_ratio();
            const int width = std::max(1, static_cast<int>(std::round(cssWidth * pixelRatio)));
            const int height = std::max(1, static_cast<int>(std::round(cssHeight * pixelRatio)));
            if (GetScreenWidth() != width || GetScreenHeight() != height) {
                SetWindowSize(width, height);
                SetMouseScale(static_cast<float>(_screenWidth) / width, static_cast<float>(_screenHeight) / height);
                _textureScaler->setOutputSize(width, height);
            }
        }
#elif defined(RESIZABLE_GUI)
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

        auto screenScale = std::min(std::clamp(int(GetScreenWidth() / _screenWidth), 1, 8), std::clamp(int(GetScreenHeight() / _screenHeight), 1, 8));
        SetMouseScale(1.0f/screenScale, 1.0f/screenScale);

        auto width = std::max(GetScreenWidth(), _screenWidth) / (_scaleBy2 ? 2 : 1);
        auto height = std::max(GetScreenHeight(), _screenHeight) / (_scaleBy2 ? 2 : 1);

        if(GetScreenWidth() < width || GetScreenHeight() < height)
            SetWindowSize(width, height);
        if(width != _screenWidth || height != _screenHeight) {
            UnloadRenderTexture(_renderTexture);
            _screenWidth = width;
            _screenHeight = height;
            _renderTexture = LoadRenderTexture(_screenWidth, _screenHeight);
            SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);
        }
#else
        if(_screenHeight < MIN_SCREEN_HEIGHT ||_screenWidth < MIN_SCREEN_WIDTH || GetScreenWidth() != _screenWidth * _scaleMode) {
            //UnloadRenderTexture(_renderTexture);
            _screenWidth = MIN_SCREEN_WIDTH;
            _screenHeight = MIN_SCREEN_HEIGHT;
            //_renderTexture = LoadRenderTexture(_screenWidth, _screenHeight);
            //SetTextureFilter(_renderTexture.texture, TEXTURE_FILTER_POINT);
            Vector2 scale2d{1.0f, 1.0f};// GetWindowScaleDPI();
            SetWindowSize(_screenWidth * _scaleMode / scale2d.x, _screenHeight * _scaleMode / scale2d.y);
            SetMouseScale(1.0f/_scaleMode*scale2d.x, 1.0f/_scaleMode*scale2d.y);
            _textureScaler->setOutputSize(_screenWidth * _scaleMode / scale2d.x, _screenHeight * _scaleMode / scale2d.y);
        }
#endif
    }

    bool isHeadless() const override { return false; }

    void drawMicroText(Image& dest, const std::string& text, int x, int y, Color tint) const
    {
        for(auto c : text) {
            if (static_cast<uint8_t>(c) < 128)
                ImageDraw(&dest, _microFont, {(c%32)*4.0f, (c/32)*6.0f, 4, 6}, {(float)x, (float)y, 4, 6}, tint);
            x += 4;
        }
    }

    void drawMicroText2(Image& dest, const std::string& text, int x, int y, Color tint) const
    {
        for(auto c : text) {
            if (static_cast<uint8_t>(c) < 128) {
                for (int yy = 0; yy < 6; ++yy) {
                    for (int xx = 0; xx < 4; ++xx) {
                        if (GetImageColor(_microFont, (c%32)*4.0f + xx, (c/32)*6.0f + yy).r > 128) {
                            ImageDrawPixel(&dest, (float)x + xx, (float)y + yy, tint);
                        }
                    }
                }
                //ImageDraw(&dest, _microFont, {(c%32)*4.0f, (c/32)*6.0f, 4, 6}, {(float)x, (float)y, 4, 6}, tint);
            }
            x += 4;
        }
    }

    static Cadmium* instance() { return _instance; }

    static void audioInputCallback(void *buffer, unsigned int frames)
    {
        if (auto* inst = Cadmium::instance()) {
            inst->renderAudio(static_cast<int16_t*>(buffer), frames);
        }
    }

    void renderAudio(int16_t *samples, unsigned int frames)
    {
        std::scoped_lock lock(_audioMutex);
        _audioCallbackAvgFrames = _audioCallbackAvgFrames ? (_audioCallbackAvgFrames + frames)/2u : frames;
        if(_chipEmu) {
            if(_chipEmu->execMode() == emu::GenericCpu::eRUNNING) {
                const auto len = _audioBuffer.read(samples, frames);
                if(!len) {
                    while(frames--) {
                        *samples++ = 0;
                    }
                    return;
                }
                frames -= len;
                samples += len;
                if(frames) {
                    _chipEmu->renderAudio(samples, frames, 44100);
                    frames = 0;
                }
            }
        }
        while(frames--) {
            *samples++ = 0;
        }
    }

    void pushAudio(int frames)
    {
        static int16_t sampleBuffer[44100];
        if(_chipEmu->execMode() == emu::IChip8Emulator::eRUNNING) {
            //if(_audioBuffer.dataAvailable() < _audioCallbackAvgFrames) ++frames;
            if(frames > _audioBuffer.spaceAvailable()) frames = _audioBuffer.spaceAvailable();
            _chipEmu->renderAudio(sampleBuffer, frames, 44100);
            _audioBuffer.write(sampleBuffer, frames);
        }
    }

    void vblank() override
    {
        if(_chipEmu)
            pushAudio(44100 / _chipEmu->frameRate());
    }

    int getKeyPressed() override
    {
#if 1
        static uint32_t instruction = 0;
        static int waitKeyUp = 0;
        static int keyId = 0;
        auto now = GetTime();
        for(int i = 0; i < 16; ++i)
            _keyScanTime[i] = now;
        if(waitKeyUp && instruction == _chipEmu->focussedExecutionUnit()->getPC()) {
            if(IsKeyUp(waitKeyUp)) {
                waitKeyUp = 0;
                instruction = 0;
                return keyId;
            }
            return -1;
        }
        waitKeyUp = 0;
        auto key = GetKeyPressed();
        if (!gui::IsSysKeyDown() && key) {
            for (int i = 0; i < 16; ++i) {
                if (key == _keyMapping[i]) {
                    instruction = _chipEmu->focussedExecutionUnit()->getPC();
                    waitKeyUp = key;
                    keyId = i + 1;
                    return 0;
                }
            }
        }
        return waitKeyUp ? -1 : 0;
#else
        //static uint32_t instruction = 0;
        //static int waitKeyUp = 0;
        //static int keyId = 0;
        auto now = GetTime();
        int keyId = -1;
        for(int i = 0; i < 16; ++i) {
            _keyScanTime[i] = now;
            if(IsKeyReleased(_keyMapping[i]))
                keyId = i;
        }
        return keyId;
        /*
        if(waitKeyUp && instruction == _chipEmu->getPC()) {
            if(IsKeyUp(waitKeyUp)) {
                waitKeyUp = 0;
                instruction = 0;
                return keyId;
            }
            return -1;
        }
        waitKeyUp = 0;
        auto key = GetKeyPressed();
        if (!gui::IsSysKeyDown() && key) {
            for (int i = 0; i < 16; ++i) {
                if (key == _keyMapping[i]) {
                    instruction = _chipEmu->getPC();
                    waitKeyUp = key;
                    keyId = i + 1;
                    return 0;
                }
            }
        }
        return waitKeyUp ? -1 : 0;
        */
#endif
    }

    bool isKeyDown(uint8_t key) override
    {
        if (key == emu::EmulatorHost::KEY_ETI_STEP)
            return !gui::IsSysKeyDown() && IsKeyDown(KEY_SPACE);
        if (key >= 16)
            return false;
        _keyScanTime[key & 0xF] = GetTime();
        return !gui::IsSysKeyDown() && IsKeyDown(_keyMapping[key & 0xF]);
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
                if(IsKeyDown(_keyMapping[_keyPosition[i*4+j]])) {
                    DrawRectangleLines(j*10.0f, i*10.0f, 9.0f, 9.0f, {0x80, 0x80, 0x80, 0xff});
                }
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

    static uint32_t rgb332To888(uint8_t c)
    {
        static uint8_t b3[] = {0, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xff};
        static uint8_t b2[] = {0, 0x60, 0xA0, 0xff};
        return (b3[(c & 0xe0) >> 5] << 16) | (b3[(c & 0x1c) >> 2] << 8) | (b2[c & 3]);
    }

    void updatePalette(const std::array<uint8_t,16>& palette) override
    {
        /*
         * TODO: Fix this
        if(!_customPalette) {
            for (int i = 0; i < palette.size(); ++i) {
                _colorPalette[i] = ((rgb332To888(palette[i]) << 8) | 0xff);
            }
            _updateScreen = true;
        }*/
    }

    void updatePalette(const std::vector<uint32_t>& palette, size_t offset) override
    {
        setPalette(palette, offset);
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
        for(int fi = 0; fi < gui::fontRomCount; ++fi) {
            const auto& fci = gui::fontRom[fi];
            auto c = fci.codepoint;
            if (glyphX + fci.width + 1 > imageWidth) {
                glyphX = 0;
                glyphY += 8;
            }
            gui::drawFontChar(_fontImage, c, glyphX, glyphY, WHITE);
            rectangles.push_back(Rectangle{glyphX, glyphY, fci.width + 1.0f, 8.0f});
            glyphs.push_back(GlyphInfo{c, 0, 0, fci.width+1});
            glyphX += fci.width + 1;
            ++glyphCount;
        }
        float badgeX = 0;
        float badgeY = glyphX < 1 ? glyphY : glyphY + 8;
        int badgeCount = 0;
        struct BadgeInfo {
            std::string text;
            Color badgeColor, textColor;
        };
        std::vector<BadgeInfo> badges;
        auto textColor = DARKGRAY;
        for(const auto& [name, info] : _cores) {
            //std::cout << toOptionName(name) << std::endl;
            //coresAvailable += fmt::format("        {} - {}\n", toOptionName(name), info->description);
            //presetsDescription += fmt::format("        {}:\n", info->description);
            for(size_t i = 0; i < info->numberOfVariants(); ++i) {
                std::string presetName;
                if(info->prefix().empty())
                    presetName = toOptionName(info->variantName(i));
                else
                    presetName = toOptionName(info->prefix() + '-' + info->variantName(i));
                badges.push_back(BadgeInfo{presetName, {0x00, 0xE0, 0x00, 0xFF}, textColor});
            }
        }
        badges.push_back(BadgeInfo{"GENERIC-CHIP-8", {0xE0, 0xC0, 0x00, 0xFF}, textColor});
        badges.push_back(BadgeInfo{"???", {0xE0, 0x40, 0x40, 0xFF}, LIGHTGRAY});
        badges.push_back(BadgeInfo{"NEW", {0x00, 0xC0, 0xE0, 0xFF}, textColor});
        for(uint16_t i = 0; i < badges.size(); ++i) {
            auto& [label, badgeCol, textCol] = badges[i];
            float width = label.length() * 4 + 3;
            if (badgeX + width > imageWidth) {
                badgeX = 0;
                badgeY += 8;
            }
            ImageDrawRectangle(&_fontImage, badgeX, badgeY+1, width, 5,badgeCol);
            ImageDrawRectangle(&_fontImage, badgeX+1, badgeY, width-2, 7, badgeCol);
            drawMicroText2(_fontImage, toUpper(label), badgeX + 2, badgeY + 1, textCol);
            rectangles.push_back(Rectangle{badgeX, badgeY, width, 7});
            glyphs.push_back(GlyphInfo{0xE100 + i, 0, 0, (int)width + 1});
            std::string badgeUtf8;
            fs::detail::appendUTF8(badgeUtf8, 0xE100 + i);
            _badges[toLower(label)] = badgeUtf8;
            badgeX += width + 1;
        }
#if !defined(NDEBUG) // && defined(EXPORT_FONT)
        ExportImage(_fontImage, "Test.png");
        /*{
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
        }*/
#endif
        _font.baseSize = 8;
        _font.glyphCount = glyphs.size();
        _font.texture = LoadTextureFromImage(_fontImage);
        _font.recs = static_cast<Rectangle*>(std::calloc(_font.glyphCount, sizeof(Rectangle)));
        _font.glyphs = static_cast<GlyphInfo*>(std::calloc(_font.glyphCount, sizeof(GlyphInfo)));
        std::ranges::copy(rectangles, _font.recs);
        std::ranges::copy(glyphs, _font.glyphs);
        GuiSetFont(_font);
    }

    bool screenChanged() const
    {
        return _updateScreen;
    }

    int getFrameBoost() const { return _chipEmu->supportsFrameBoost() && _frameBoost > 0 ? _frameBoost : 1; }

    void updateScreen() override
    {
        auto* pixel = (uint32_t*)_screen.data;
        if(pixel) {
            auto rgbaSpan = _chipEmu->getRGBASpan(
                {static_cast<uint32_t*>(_screen.data), static_cast<size_t>(_screen.width), static_cast<size_t>(_screen.height)}
                );
            // The pre-rendered HP LCD (including the generic core's HP display
            // option) contains a fine grid, not bare CHIP-8 pixels.
            _screenIsPreRendered = !rgbaSpan.empty();
            SetTextureFilter(_screenTexture, _screenIsPreRendered ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
            if(!rgbaSpan.empty()) {
                auto textureSpan = ghc::span2d(static_cast<uint32_t*>(_screen.data), _screen.width, _screen.height);
                for (size_t y = 0; y < rgbaSpan.height(); ++y) {
                    std::memmove(textureSpan.row(y).data(), rgbaSpan.row(y).data(), rgbaSpan.width() * sizeof(uint32_t));
                }
                UpdateTexture(_screenTexture, _screen.data);
                _screenRatio = 1;
                _screenSize = {static_cast<float>(rgbaSpan.width()), static_cast<float>(rgbaSpan.height())};
                _screenBorderColor = {0,0,0,255};
            }
            else {
                const auto* screen = _chipEmu->getScreen();
                if (screen) {
                    if (!_renderCrt) {
                        screen->convert(pixel, _screen.width, 255, nullptr);
                        UpdateTexture(_screenTexture, _screen.data);
                        _screenRatio = screen->ratio();
                        _screenSize = {static_cast<float>(_chipEmu->getCurrentScreenWidth()), static_cast<float>(_chipEmu->getCurrentScreenHeight())};
                        _screenBorderColor = GetColor(be32(screen->getBorderColor()));
                    }
                    else {
                    }
                }
                else {
                    // TraceLog(LOG_INFO, "Updating MC8 screen!");
                    const auto* screenRgb = _chipEmu->getScreenRGBA();
                    if(screenRgb) {
                        screenRgb->convert(pixel, _screen.width, _chipEmu->getScreenAlpha(), _chipEmu->getWorkRGBA());
                        UpdateTexture(_screenTexture, _screen.data);
                        _screenRatio = screenRgb->ratio();
                        _screenSize = {static_cast<float>(_chipEmu->getCurrentScreenWidth()), static_cast<float>(_chipEmu->getCurrentScreenHeight())};
                        _screenBorderColor = GetColor(be32(screenRgb->getBorderColor()));
                    }
                }
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
        bool breakpointTriggered = false;

        updateResolution();

        _librarian.update(*_properties); // allows librarian to complete background tasks

        if (IsFileDropped()) {
            auto files = LoadDroppedFiles();
            if (files.count > 0) {
                //TraceLog(LOG_INFO, "About to load one of %d dropped files.", (int)files.count);
                loadRom(files.paths[0], LoadOptions::None);
            }
            UnloadDroppedFiles(files);
        }

        updateEditorDisassembly();
        if(_mainView == eEDITOR) {
            _editor.update();
            if(_editor.contentRevision() != _loadedSourceRevision && _editor.isCompiledUpToDate() &&
               !_editor.compiler().isError() && _editor.compiler().sha1() != _romSha1) {
                _debugger.updateOctoBreakpoints(_editor.compiler());
                replaceProgramFromEditor({_editor.compiler().code(), _editor.compiler().code() + _editor.compiler().codeSize()}, _editor.compiler().sha1());
            }
        }

        for(uint8_t key = 0; key < 16; ++key) {
            _keyMatrix[key] = IsKeyDown(_keyMapping[key & 0xF]);
        }

        if (_traceFrame) {
            _chipEmu->traceNextFrame();
            _traceFrame = false;
        }
        if(_chipEmu->execMode() != ExecMode::ePAUSED) {
            _partialFrameTime += GetFrameTime()*1000 * _chipEmu->frameRate();
            if(_partialFrameTime > 10000) {
                _fps.reset();
                _partialFrameTime = 1000;
            }
            if(_partialFrameTime >= 1000) {
                while (_partialFrameTime >= 1000) {
                    _partialFrameTime -= 1000;
                    for(int i = 0; i < getFrameBoost(); ++i) {
                        _chipEmu->executeFrame();
                        bool breakpoint = false;
                        for(auto& unit : *_chipEmu) {
                            if(unit.isBreakpointTriggered()) {
                                _mainView = eDEBUGGER;
                                _debugger.setBreakpointTriggered(true);
                                breakpoint = true;
                                break;
                            }
                        }
                        if (!breakpoint)
                            _debugger.setBreakpointTriggered(false);
                    }
                    _fps.add(GetTime()*1000);
                }
            }
            if(_chipEmu->needsScreenUpdate())
                updateScreen();
            updateKeyboardOverlay();
        }

        static Stopwatch guiRenderTime;
        guiRenderTime.start();
        _screenOverlay = {0,0,0,0};
        BeginTextureMode(_textureScaler->getRenderTexture());
        drawGui();
        EndTextureMode();
        _textureScaler->updateIntermediateTexture();
        guiRenderTime.stop();
        _avgGuiRenderTime = guiRenderTime.getElapsedAvgString();

        BeginDrawing();
        {
            ClearBackground(CADMIUM_VERSION_DECIMAL & 1 ? Color{16,0,0,255} : BLACK);
            //ClearBackground(BLACK);
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
            if (_videoRenderMode == eHIRES && _screenOverlay.width > 0 && _screenOverlay.height > 0) {
#ifdef PLATFORM_WEB
                const float scaleX = static_cast<float>(GetScreenWidth()) / _screenWidth;
                const float scaleY = static_cast<float>(GetScreenHeight()) / _screenHeight;
                drawScreen({_screenOverlay.x * scaleX, _screenOverlay.y * scaleY, _screenOverlay.width * scaleX, _screenOverlay.height * scaleY}, _screenScale);
#else
                drawScreen({_screenOverlay.x * _scaleMode, _screenOverlay.y * _scaleMode, _screenOverlay.width * _scaleMode, _screenOverlay.height * _scaleMode}, _screenScale);
#endif
            }
            _textureScaler->draw(0, 0);
            //DrawTexturePro(_renderTexture.texture, (Rectangle){0, 0, (float)_renderTexture.texture.width, -(float)_renderTexture.texture.height}, (Rectangle){0, 0, (float)_renderTexture.texture.width * _scaleMode / scale2d.x, (float)_renderTexture.texture.height * _scaleMode / scale2d.y},
            //               (Vector2){0, 0}, 0.0f, WHITE);
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
            // DrawText(TextFormat("Res: %dx%d", GetMonitorWidth(_currentMonitor), GetMonitorHeight(_currentMonitor)), 10, 30, 10, GREEN);
            // DrawFPS(10,45);
        }
        EndDrawing();
    }

    void drawScreen(Rectangle dest, int /*gridScale*/)
    {
        int scrWidth = _screenSize.x; //_chipEmu->getCurrentScreenWidth();
        int scrHeight = _screenSize.y; //_chipEmu->getCurrentScreenHeight();

        //int ratio = 1;
        Color borderColor = _screenBorderColor; //{0,0,0,255};
        //if (auto* s = _chipEmu->getScreen()) { ratio = s->ratio(); borderColor = GetColor(be32(s->getBorderColor())); }
        //else if (auto* srgba = _chipEmu->getScreenRGBA()) { ratio = srgba->ratio(); }
        int ratio = _screenRatio;
        if (ratio <= 0) ratio = 1;

        // Background (keep your existing look)
        DrawRectangleRec(dest, borderColor);

        if (scrWidth <= 0 || scrHeight <= 0 || dest.width <= 0.0f || dest.height <= 0.0f)
            return;

        // Logical "display" height includes the vertical stretch ratio.
        const float contentW = static_cast<float>(scrWidth);
        const float contentH = static_cast<float>(scrHeight / ratio);

        // Bare CHIP-8 pixels retain integer scaling. Pre-rendered LCDs use
        // filtered fractional scaling to avoid aliasing their fine pixel grid.
        const float maxScaleX = dest.width / contentW;
        const float maxScaleY = dest.height / contentH;
        const float fitScale = std::min(maxScaleX, maxScaleY);
        const float scale = _screenIsPreRendered ? fitScale :
            (fitScale >= 1.0f ? std::floor(fitScale) : 1.0f / std::ceil(1.0f / fitScale));

        const float dstW = contentW * static_cast<float>(scale);
        const float dstH = contentH * static_cast<float>(scale);

        // Center inside dest. Round to whole pixels to avoid subpixel wobble.
        const float videoX = std::floor(dest.x + (dest.width - dstW) * 0.5f);
        const float videoY = std::floor(dest.y + (dest.height - dstH) * 0.5f);

        const Rectangle srcRect{0.0f, 0.0f, static_cast<float>(scrWidth), static_cast<float>(scrHeight)};
        const Rectangle dstRect{videoX, videoY, dstW, dstH};

        DrawTexturePro(_screenTexture, srcRect, dstRect, {0.0f, 0.0f}, 0.0f, WHITE);

        if(_showKeyMap) {
            auto s = scale / std::max(1, _screenScale);
            DrawTexturePro(_keyboardOverlay.texture, {0, 0, 40, -40}, {videoX + scrWidth * scale - 40.0f*s, videoY + scrHeight * scale - 40.0f*s, 40.0f*s, 40.0f*s}, {0, 0}, 0.0f, {255, 255, 255, 128});
        }
        if(GetTime() < 5 && _romImage.empty()) {
            auto scale = dest.width / 128;
            auto offsetX = (dest.width - 60*scale) / 2;
            auto offsetY = (dest.height - 60*scale) / 2;
            DrawTexturePro(_titleTexture, {34, 2, 60, 60}, {dest.x + offsetX, dest.y + offsetY, 60*scale, 60*scale}, {0, 0}, 0, {255,255,255,uint8_t(GetTime()>4 ? 255.0f*(4.0f-GetTime()) : 255.0f)});
        }
    }
#if 0
    void drawScreen(Rectangle dest, int /*gridScale*/)
    {
        const Color gridLineCol{40,40,40,255};
        int scrWidth = _chipEmu->getCurrentScreenWidth();
        int scrHeight = _chipEmu->getCurrentScreenHeight();
        auto videoScaleX = dest.width / as<float>(scrWidth);
        auto ratio = _chipEmu->getScreen() ? _chipEmu->getScreen()->ratio() : (_chipEmu->getScreenRGBA() ? _chipEmu->getScreenRGBA()->ratio() : 1);
        auto videoScaleY = ratio ? videoScaleX / as<float>(ratio) : videoScaleX;
        auto videoX = (dest.width - as<float>(_chipEmu->getCurrentScreenWidth()) * videoScaleX) / 2 + dest.x;
        auto videoY = (dest.height - as<float>(_chipEmu->getCurrentScreenHeight()) * videoScaleY) / 2 + dest.y;
        if(_chipEmu->getMaxScreenWidth() > 128)
            DrawRectangleRec(dest, {0,0,0,255});
        else
            DrawRectangleRec(dest, {0,12,24,255});

        DrawTexturePro(_screenTexture, {0, 0, (float)scrWidth, (float)scrHeight}, {videoX, videoY, scrWidth * videoScaleX, scrHeight * videoScaleY}, {0, 0}, 0, WHITE);
    }
#endif

#if 0
    void drawScreen(Rectangle dest, int gridScale)
    {
        const Color gridLineCol{40,40,40,255};
        int scrWidth = _chipEmu->getCurrentScreenWidth();
        //int scrHeight = crt ? 385 : (_chipEmu->isGenericEmulation() ? _chipEmu->getCurrentScreenHeight() : 128);
        int scrHeight = _chipEmu->getCurrentScreenHeight();
        auto videoScaleX = dest.width / as<float>(scrWidth);
        auto ratio = _chipEmu->getScreen() ? _chipEmu->getScreen()->ratio() : (_chipEmu->getScreenRGBA() ? _chipEmu->getScreenRGBA()->ratio() : 1);
        auto videoScaleY = ratio ? videoScaleX / as<float>(ratio) : videoScaleX;
        auto videoX = (dest.width - as<float>(_chipEmu->getCurrentScreenWidth()) * videoScaleX) / 2 + dest.x;
        auto videoY = (dest.height - as<float>(_chipEmu->getCurrentScreenHeight()) * videoScaleY) / 2 + dest.y;
        if(_chipEmu->getMaxScreenWidth() > 128)
            DrawRectangleRec(dest, {0,0,0,255});
        else
            DrawRectangleRec(dest, {0,12,24,255});

        //BeginShaderMode(_scanLineShader);
        DrawTexturePro(_screenTexture, {0, 0, (float)scrWidth, (float)scrHeight}, {videoX, videoY, scrWidth * videoScaleX, scrHeight * videoScaleY}, {0, 0}, 0, WHITE);
        //EndShaderMode();

        if (_grid) {
            for (short x = 0; x < scrWidth; ++x) {
                DrawRectangle(videoX + x * gridScale, videoY, 1, scrHeight * videoScaleY, gridLineCol);
            }
            if(_chipEmu->isGenericEmulation()) {
                for (short y = 0; y < scrHeight; ++y) {
                    DrawRectangle(videoX, videoY + y * gridScale, scrWidth * videoScaleX, 1, gridLineCol);
                }
            }
        }
        if(_showKeyMap) {
            auto s = videoScaleX / _screenScale;
            DrawTexturePro(_keyboardOverlay.texture, {0, 0, 40, -40}, {videoX + scrWidth * videoScaleX - 40.0f*s, videoY + scrHeight * videoScaleY - 40.0f*s, 40.0f*s, 40.0f*s}, {0, 0}, 0.0f, {255, 255, 255, 128});
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
#endif

    static bool iconButton(int iconId, bool isPressed = false, Color color = {3, 127, 161}, Color foreground = {0x51, 0xbf, 0xd3, 0xff})
    {
        using namespace gui;
        StyleManager::Scope guard;
        auto fg = guard.getStyle(gui::Style::TEXT_COLOR_NORMAL);
        auto bg = guard.getStyle(gui::Style::BASE_COLOR_NORMAL);
        if(isPressed) {
            guard.setStyle(gui::Style::BASE_COLOR_NORMAL, fg);
            guard.setStyle(gui::Style::TEXT_COLOR_NORMAL, bg);
        }
        //guard.setStyle(Style::TEXT_COLOR_NORMAL, foreground);
        gui::SetNextWidth(20);
        auto result = gui::Button(GuiIconText(iconId, ""));
        return result;
    }
    static std::optional<MouseButton> iconButtonDual(int iconId, bool isPressed = false, Color color = {3, 127, 161}, Color foreground = {0x51, 0xbf, 0xd3, 0xff})
    {
        using namespace gui;
        StyleManager::Scope guard;
        auto fg = guard.getStyle(gui::Style::TEXT_COLOR_NORMAL);
        auto bg = guard.getStyle(gui::Style::BASE_COLOR_NORMAL);
        if(isPressed) {
            guard.setStyle(gui::Style::BASE_COLOR_NORMAL, fg);
            guard.setStyle(gui::Style::TEXT_COLOR_NORMAL, bg);
        }
        //guard.setStyle(Style::TEXT_COLOR_NORMAL, foreground);
        SetNextWidth(20);
        return ButtonMulti(GuiIconText(iconId, ""));
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

    void updateMainMenu()
    {
        using namespace gui;
        PopupMenu::Close();
        PopupMenu::Menu lruFiles;
        for (auto& recent : _recentFiles) {
            lruFiles.push_back(PopupMenu::Filename(ghc::utf8::truncateWithEllipsis(recent, 64), [this, recent]() {
                loadRom(recent, LoadOptions::None);
                _mainView = eVIDEO;
            }));
        }
        if (!lruFiles.empty()) {
            lruFiles.push_back(PopupMenu::Separator());
            lruFiles.push_back(PopupMenu::Action("Clear recent files", [this]() { _recentFiles.clear(); updateMainMenu(); }));
        }
        _mainMenu = PopupMenu::Menu{
            PopupMenu::Action("About Cadmium...", [this]() {
                _aboutOpen = true;
                _aboutScroll = {0, 0};
            }),
            PopupMenu::Separator(),
            PopupMenu::Action("New...", [this](){
                _mainView = eEDITOR;
                _editor.setText(": main\n    jump main");
                _romName = "unnamed.8o";
                _editor.setFilename("");
                _sourcePath.clear();
                for(auto& unit : *_chipEmu) {
                  unit.removeAllBreakpoints();
                }
            }, "⌘N"),
            PopupMenu::Action("Open...", [this]() {
#ifdef PLATFORM_WEB
                loadFileWeb();
#else
                _mainView = eROM_SELECTOR;
                _librarian.fetchDir(_currentDirectory);
#endif
            }, "⌘O")
        };
        if(!_recentFiles.empty()) {
            _mainMenu.push_back(PopupMenu::Submenu("Open recent", lruFiles));
        }
        _mainMenu.insert(_mainMenu.end(), {
            PopupMenu::Action("Save...", [this]() {
                openSavePage(false);
            }, "⌘S"),
            PopupMenu::Separator(),
            PopupMenu::Toggle("Key Map", &_showKeyMap, [this](){ _showKeyMap = !_showKeyMap; }, "⌘K"),
#ifndef PLATFORM_WEB
            PopupMenu::Separator(),
            PopupMenu::Action("Quit", [this](){ _shouldClose = true; }, "⌘Q")
#endif
        });
    }

    void drawGui()
    {
        using namespace gui;
        ClearBackground(GetColor(GetStyle(DEFAULT, BACKGROUND_COLOR)));
        Rectangle video;
        int gridScale = 4;
        static int64_t lastInstructionCount = 0;
        static int64_t lastFrameCount = 0;
        static bool selectorOpen = false;

        static std::chrono::steady_clock::time_point volumeClick{};

#ifdef RESIZABLE_GUI
        //auto screenScale = std::min(std::clamp(int(GetScreenWidth() / _screenWidth), 1, 8), std::clamp(int(GetScreenHeight() / _screenHeight), 1, 8));
        auto screenScale = std::min(std::clamp(int(GetScreenWidth() / _screenWidth), 1, 8), std::clamp(int(GetScreenHeight() / _screenHeight), 1, 8));
        Vector2 mouseOffset = {-(GetScreenWidth() - _screenWidth * screenScale) / 2.0f, -(GetScreenHeight() - _screenHeight * screenScale) / 2.0f};
        if (mouseOffset.x > 0)
            mouseOffset.x = 0;
        if (mouseOffset.y > 0)
            mouseOffset.y = 0;
        BeginGui({}, &_renderTexture, mouseOffset, {(float)screenScale, (float)screenScale});
//        BeginGui({}, &_renderTexture, {0, 0}, {_scaleBy2 ? 2.0f : 1.0f, _scaleBy2 ? 2.0f : 1.0f});
#elif defined(PLATFORM_WEB)
        BeginGui({}, &_textureScaler->getRenderTexture(), {0, 0},
                 {static_cast<float>(GetScreenWidth()) / _screenWidth, static_cast<float>(GetScreenHeight()) / _screenHeight});
#else
        BeginGui({}, &_textureScaler->getRenderTexture(), {0, 0}, {static_cast<float>(_scaleMode), static_cast<float>(_scaleMode)});
#endif
        {
            SetStyle(STATUSBAR, TEXT_PADDING, 4);
            SetStyle(LISTVIEW, SCROLLBAR_WIDTH, 6);
            SetStyle(DROPDOWNBOX, DROPDOWN_ITEMS_SPACING, 0);
            SetStyle(SPINNER, TEXT_PADDING, 4);

            SetRowHeight(16);
            SetSpacing(0);
            auto instructionsThisUpdate = _chipEmu->executedInstructions() - lastInstructionCount;
            auto framesThisUpdate = _chipEmu->frames() - lastFrameCount;
            if (_chipEmu->execMode() == emu::GenericCpu::eRUNNING) {
                _ipfAverage.add(instructionsThisUpdate);
                _frameTimeAverage_us.add(GetFrameTime() * 1000000);
                _frameDelta.add(framesThisUpdate);
            }
            auto ipfAvg = _ipfAverage.get();
            auto ftAvg_us = _frameTimeAverage_us.get();
            auto fdAvg = _frameDelta.get();
            auto ips = instructionsThisUpdate / GetFrameTime();

            auto ipsAvg = float(ipfAvg) * 1000000 / ftAvg_us;
            if (_mainView == eEDITOR) {
                StatusBar({{0.55f, fmt::format("UI:{}", _avgGuiRenderTime).c_str()}, {0.15f, fmt::format("{} byte", _editor.compiler().codeSize()).c_str()}, {0.1f, fmt::format("{}:{}", _editor.line(), _editor.column()).c_str()}, {0.15f, _variantName.c_str()}});
            }
            else if (_chipEmu->coreState() == emu::IEmulationCore::ECS_ERROR) {
                StatusBar({{0.55f, fmt::format("{}, UI:{}", _chipEmu->errorMessage(), _avgGuiRenderTime).c_str()}, {0.15f, formatUnit(ipsAvg, "IPS").c_str()}, {0.1f, formatUnit(_fps.getFps(), "FPS").c_str()}, {0.15f, _variantName.c_str()}});
            }
            else if (getFrameBoost() > 1) {
                StatusBar({{0.5f, fmt::format("Cycles: {}, UI:{}", _chipEmu->cycles(), _avgGuiRenderTime).c_str()}, {0.2f, formatUnit(ipsAvg, "IPS").c_str()}, {0.1f, formatUnit(_fps.getFps() * getFrameBoost(), "eFPS").c_str()}, {0.15f, _variantName.c_str()}});
            }
            else {
                if (_chipEmu->cycles() != _chipEmu->machineCycles()) {
                    StatusBar({{0.55f, fmt::format("Cycles: {}/{} [{}], UI:{}", _chipEmu->cycles(), _chipEmu->machineCycles(), _chipEmu->frames(), _avgGuiRenderTime).c_str()},
                               {0.15f, formatUnit(ipsAvg, "IPS").c_str()},
                               {0.1f, formatUnit(_fps.getFps(), "FPS").c_str()},
                               {0.15f, _variantName.c_str()}});
                }
                else {
                    StatusBar({{0.55f, fmt::format("Cycles: {} [{}], UI:{}", _chipEmu->cycles(), _chipEmu->frames(), _avgGuiRenderTime).c_str()},
                               {0.15f, formatUnit(ipsAvg, "IPS").c_str()},
                               //{0.15f, formatUnit((double)getFrameBoost() * GetFPS(), "FPS").c_str()},
                               {0.1f, formatUnit(_fps.getFps(), "FPS").c_str()},
                               {0.15f, _variantName.c_str()}});
                }
            }
            lastInstructionCount = _chipEmu->executedInstructions();
            lastFrameCount = _chipEmu->frames();
            BeginColumns();
            {
                SetRowHeight(20);
                SetSpacing(0);
                SetNextWidth(20);
                static Vector2 aboutScroll{};
                if (iconButton(ICON_BURGER_MENU, PopupMenu::IsMenuOpen(_mainMenu)) && !PopupMenu::IsOpen())
                    PopupMenu::Open({1.0f, GetCurrentPos().y + 20}, _mainMenu);
                if (IsSysKeyDown()) {
                    if (IsKeyPressed(KEY_N)) {
                        PopupMenu::Close();
                        _mainView = eEDITOR;
                        _editor.setText(": main\n    jump main");
                        _romName = "unnamed.8o";
                        _editor.setFilename("");
                        _sourcePath.clear();
                        for(auto& unit : *_chipEmu) {
                            unit.removeAllBreakpoints();
                        }
                    }
                    if (IsKeyPressed(KEY_O)) {
                        PopupMenu::Close();
#ifdef PLATFORM_WEB
                        loadFileWeb();
#else
                        _mainView = eROM_SELECTOR;
                        _librarian.fetchDir(_currentDirectory);
#endif
                    }
                    if (IsKeyPressed(KEY_S)) {
                        PopupMenu::Close();
                        saveShortcut();
                    }
                    if (IsKeyPressed(KEY_K)) {
                        PopupMenu::Close();
                        _showKeyMap = !_showKeyMap;
                    }
#ifndef PLATFORM_WEB
                    if (IsKeyPressed(KEY_Q)) {
                        PopupMenu::Close();
                        _shouldClose = true;
                    }
#endif
                }
                if (BeginWindowBox({-1, -1, 460, 300}, "About Cadmium", &_aboutOpen, WindowBoxFlags(WBF_CLOSABLE | WBF_MOVABLE | WBF_MODAL))) {
                    SetStyle(DEFAULT, BORDER_WIDTH, 0);
                    static size_t newlines = std::count_if(aboutText.begin(), aboutText.end(), [](char c) { return c ==        '\n'; });
                    BeginScrollPanel(-1, {0, 0, 445, newlines * 10.0f + 100}, &aboutScroll);
                    SetRowHeight(10);
                    DrawTextureRec(_titleTexture, {34, 2, 60, 60}, {aboutScroll.x + 8.0f, aboutScroll.y + 31.0f}, WHITE);
                    auto styleColor = GetStyle(LABEL, TEXT_COLOR_NORMAL);
                    SetStyle(LABEL, TEXT_COLOR_NORMAL, ColorToInt(WHITE));
                    Label("           Cadmium v" CADMIUM_VERSION "-" CADMIUM_GIT_HASH);
                    SetStyle(LABEL, TEXT_COLOR_NORMAL, styleColor);
                    Space(4);
                    Label("           (c) 2022 by Steffen 'Gulrak' Schümann");
                    if (LabelButton("           https://github.com/gulrak/cadmium")) {
                        OpenURL("https://github.com/gulrak/cadmium");
                    }
                    Space(8);
                    std::istringstream iss(aboutText);
                    for (std::string line; std::getline(iss, line);) {
                        auto trimmedLine = trim(line);
                        if (startsWith(trimmedLine, "http")) {
                            if (LabelButton(line.c_str()))
                                OpenURL(trimmedLine.c_str());
                        }
                        else if (startsWith(line, "# ")) {
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
                    if (IsKeyPressed(KEY_ESCAPE))
                        _aboutOpen = false;
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
                SetTooltip("OPEN PROGRAM");
#ifndef PLATFORM_WEB
                if (iconButton(ICON_NOTEBOOK, _mainView == eLIBRARY)) {
                    _mainView = eLIBRARY;
                }
                SetTooltip("OPEN LIBRARY");
#endif
                if (iconButton(ICON_FILE_SAVE_CLASSIC)) {
                    openSavePage(false);
                }
                SetTooltip("SAVE PROGRAM");
                //SetNextWidth(130);
                //SetStyle(TEXTBOX, BORDER_WIDTH, 1);
                //TextBox(_romName, 4095);
                Space(64);

                bool chip8Control = _debugger.isControllingChip8();
                Color controlBack = {3, 127, 161};
                Color controlColor = Color{0x51, 0xbf, 0xd3, 0xff};  // chip8Control ? Color{0x51, 0xbf, 0xd3, 0xff} : Color{0x51, 0xff, 0xbf, 0xff};
                if (iconButton(ICON_PLAYER_PAUSE, _chipEmu->execMode() == ExecMode::ePAUSED /*, controlBack, controlColor*/) || ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) && IsKeyPressed(KEY_F5))) {
                    _chipEmu->focussedExecutionUnit()->setExecMode(ExecMode::ePAUSED);
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eVIDEO;
                    }
                }
                SetTooltip("PAUSE [Shift+F5]");
                if (iconButton(ICON_PLAYER_PLAY, _chipEmu->execMode() == ExecMode::eRUNNING /*, controlBack, controlColor*/) || (!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT) && IsKeyPressed(KEY_F5))) {
                    _debugger.setExecMode(ExecMode::eRUNNING);
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = _lastRunView;
                    }
                }
                SetTooltip("RUN [F5]");
                if (!_debugger.supportsStepOver())
                    GuiDisable();
                if (iconButton(ICON_STEP_OVER, _chipEmu->execMode() == ExecMode::eSTEPOVER /*, controlBack, controlColor*/) || (!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT) && IsKeyPressed(KEY_F8))) {
                    _debugger.setExecMode(ExecMode::eSTEPOVER);
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eDEBUGGER;
                    }
                }
                GuiEnable();
                SetTooltip("STEP OVER [F8]");
                if (iconButton(ICON_STEP_INTO, _chipEmu->execMode() == ExecMode::eSTEP /*, controlBack, controlColor*/) || (!IsKeyDown(KEY_LEFT_SHIFT) && !IsKeyDown(KEY_RIGHT_SHIFT) && IsKeyPressed(KEY_F7))) {
                    _debugger.setExecMode(ExecMode::eSTEP);
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eDEBUGGER;
                    }
                }
                SetTooltip("STEP INTO [F7]");
                if (!_debugger.supportsStepOver())
                    GuiDisable();
                if (iconButton(ICON_STEP_OUT, _chipEmu->execMode() == ExecMode::eSTEPOUT /*, controlBack, controlColor*/) || ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) && IsKeyPressed(KEY_F7))) {
                    _debugger.setExecMode(ExecMode::eSTEPOUT);
                    if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                        _mainView = eDEBUGGER;
                    }
                }
                GuiEnable();
                SetTooltip("STEP OUT [Shift+F7]");
                if (const auto button = iconButtonDual(ICON_RESTART); button.has_value()) {
                    if (button == MOUSE_BUTTON_LEFT) {
                        reloadProgram();
                        resetStats();
                        if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                            _mainView = _lastRunView;
                        }
                    }
                    if (button == MOUSE_BUTTON_RIGHT) {
                        auto oldMode = _chipEmu->execMode();
                        _chipEmu->reset(false);
                        _audioBuffer.reset();
                        updateScreen();
                        if (oldMode == ExecMode::eRUNNING) {
                            _debugger.setExecMode(ExecMode::eRUNNING);
                        }
                        resetStats();
                        if (_mainView == eEDITOR || _mainView == eSETTINGS) {
                            _mainView = _lastRunView;
                        }
                    }
                }
                SetTooltip("RESTART/RESET");
                int buttonsRight = 8;
                ++buttonsRight;
                int avail = _screenWidth - GetCurrentPos().x;
#ifdef PLATFORM_WEB
                --buttonsRight;
                avail += 10;
#endif
                auto spacePos = GetCurrentPos();
                auto spaceWidth = avail - buttonsRight * 20;
                Space(spaceWidth);
                if (_chipEmu->getMaxScreenWidth() > 128)
                    GuiDisable();
                if (iconButton(ICON_BOX_GRID, _grid))
                    _grid = !_grid;
                GuiEnable();
                SetTooltip("TOGGLE GRID");
                Space(10);
                if (const auto button = iconButtonDual(ICON_ZOOM_ALL, _mainView == eVIDEO); button.has_value()) {
                    if (button == MOUSE_BUTTON_LEFT) {
                        _mainView = eVIDEO;
                    }
                    if (button == MOUSE_BUTTON_RIGHT) {
                        _traceFrame = true;
                    }
                }
                SetTooltip("FULL VIDEO");
                if (iconButton(ICON_CPU, _mainView == eDEBUGGER))
                    _mainView = eDEBUGGER;
                SetTooltip("DEBUGGER");
                if (iconButton(ICON_FILETYPE_TEXT, _mainView == eEDITOR))
                    _mainView = eEDITOR, _chipEmu->setExecMode(ExecMode::ePAUSED);
                SetTooltip("EDITOR");
                if (iconButton(ICON_PRINTER, _mainView == eTRACELOG))
                    _mainView = eTRACELOG;
                SetTooltip("TRACE-LOG");
                if (iconButton(ICON_GEAR, _mainView == eSETTINGS))
                    _mainView = eSETTINGS;
                SetTooltip("SETTINGS");
                if (iconButton(ICON_AUDIO, false))
                    volumeClick = std::chrono::steady_clock::now();
                SetTooltip("VOLUME");

                static const char* titleString = "v" CADMIUM_VERSION "-" CADMIUM_GIT_HASH;
                static Vector2 versionSize = MeasureTextEx(GuiGetFont(), titleString, 8, 0);
                DrawTextEx(GuiGetFont(), titleString, {spacePos.x + (spaceWidth - versionSize.x) / 2, spacePos.y + 6}, 8, 0, WHITE);
#ifndef PLATFORM_WEB
                Space(10);
                if (iconButton(ICON_HIDPI, _scaleMode != 1))
                    _scaleMode = _scaleMode >= 3 ? 1 : _scaleMode + 1;
                SetTooltip("TOGGLE ZOOM");
#endif
            }
            EndColumns();

            switch (_mainView) {
                case eDEBUGGER: {
                    _lastView = _lastRunView = _mainView;
                    _debugger.render(_font, [this](Rectangle video, int scale) {
                        _screenOverlay = video;
                        _screenScale = scale;
                        if (_videoRenderMode == eHIRES) {
                            rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
                            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                            DrawRectangleRec(_screenOverlay, {0,0,0,255});
                            rlSetBlendMode(RL_BLEND_ALPHA);
                        }
                    });
                    if (_videoRenderMode == eFAST) {
                        drawScreen(_screenOverlay, _screenScale);
                    }
                    break;
                }
                case eVIDEO: {
                    _lastView = _lastRunView = _mainView;
                    gridScale = _screenWidth / _chipEmu->getCurrentScreenWidth();
                    _screenOverlay = {0, 20, (float)_screenWidth, (float)_screenHeight - 36};
                    _screenScale = gridScale;
                    if (_videoRenderMode == eHIRES) {
                        rlSetBlendFactorsSeparate(RL_ZERO, RL_ONE, RL_ZERO, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
                        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
                        DrawRectangleRec(_screenOverlay, {0,0,0,255});
                        rlSetBlendMode(RL_BLEND_ALPHA);
                    }
                    else {
                        drawScreen(_screenOverlay, _screenScale);
                    }
                    break;
                }
                case eEDITOR:
                    if (_lastView != eEDITOR)
                        _editor.setFocus();
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Editor", {1, 1});
                    {
                        auto rect = GetContentAvailable();
                        _editor.draw(_font, {rect.x, rect.y - 1, rect.width, rect.height});
                    }
                    EndPanel();
                    End();
                    break;
                case eTRACELOG: {
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Trace-Log", {1, 1});
                    {
                        auto rect = GetContentAvailable();
                        _logView.draw(_font, {rect.x, rect.y - 1, rect.width, rect.height});
                    }
                    EndPanel();
                    End();
                    break;
                }
                case eSETTINGS: {
                    static FileDialogInfo fdi{FileDialogInfo::SelectFolder, "Select Directory", fs::current_path().string()};
                    static Vector2 scrollPos = {};
                    static int activeEntry = -1;
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Settings");
                    {
                        static int activeTab = 0;
                        BeginTabView(&activeTab);
                        if (BeginTab("Emulation", {5, 0})) {
                            renderEmulationSettings();
                            EndTab();
                        }
                        if (BeginTab("Appearance", {5, 0})) {
                            _styleManager.renderAppearanceEditor();
                            auto pos = GetCurrentPos();
                            Space(_screenHeight - pos.y - 20 - 1);
                            EndTab();
                        }
                        if (BeginTab("Misc", {5, 0})) {
                            Space(3);
                            SetSpacing(0);
                            Label("Config Directory:");
                            GuiDisable();
                            TextBox(_cfgPath, 4096);
                            GuiEnable();
                            Space(4);
                            BeginColumns();
                            SetSpacing(0);
                            SetNextWidth(GetContentAvailable().width - 20*5);
                            Label("CHIP-8 Library Directories:");
                            SetDefaultWidth(20);
                            if (Button("+")) {
                                selectorOpen = true;
                                activeEntry = -1;
                            }
                            if (activeEntry < 0) GuiDisable();
                            if (Button("-")) {
                                if (activeEntry >= 0 && activeEntry < _cfg.libraryPath.size()) {
                                    _cfg.libraryPath.erase(_cfg.libraryPath.begin() + activeEntry);
                                    activeEntry = -1;
                                }
                            }
                            if (Button(GuiIconText(ICON_PENCIL, ""))) {
                                if (activeEntry >= 0 && activeEntry < _cfg.libraryPath.size()) {
                                    fdi.path = _cfg.libraryPath[activeEntry];
                                    selectorOpen = true;
                                }
                            }
                            if (Button(GuiIconText(ICON_ARROW_UP, ""))) {
                                if (activeEntry > 0) {
                                    std::swap(_cfg.libraryPath[activeEntry], _cfg.libraryPath[activeEntry - 1]);
                                    activeEntry--;
                                }
                            }
                            if (Button(GuiIconText(ICON_ARROW_DOWN, ""))) {
                                if (activeEntry >= 0 && activeEntry < _cfg.libraryPath.size() - 1) {
                                    std::swap(_cfg.libraryPath[activeEntry], _cfg.libraryPath[activeEntry + 1]);
                                    activeEntry++;
                                }
                            }
                            GuiEnable();
                            EndColumns();
                            SimpleListView(15*5+4, _cfg.libraryPath, &scrollPos, &activeEntry);
                            Space(4);
                            /*
                            if (TextBox(_databaseDirectory, 4096)) {
                                saveConfig();
                            }
                            Space(4);
                            if (Button("File Dialog")) {
                                selectorOpen = true;
                            }
                            */
                            auto pos = GetCurrentPos();
                            Space(_screenHeight - pos.y - 20 - 1);
                            EndTab();
                        }
                        EndTabView();
                    }
                    EndPanel();
                    End();
                    if (selectorOpen) {
                        if (ModalFileDialog(fdi, &selectorOpen)) {
                            selectorOpen = false;
                            if (fdi.action == FileDialogInfo::CloseAction::Okay) {
                                if (!fdi.path.empty()) {
                                    if (activeEntry == -1 || activeEntry >= _cfg.libraryPath.size())
                                        _cfg.libraryPath.push_back(fdi.path);
                                    else
                                        _cfg.libraryPath[activeEntry] = fdi.path;
                                }
                            }
                        }
                    }
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
                    if (IsKeyPressed(KEY_ESCAPE))
                        _mainView = _lastView;
                    break;
                }
#else
                case eROM_SELECTOR:
                    break;
#endif  // !PLATFORM_WEB
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
                    if (IsKeyPressed(KEY_ESCAPE))
                        _mainView = _lastView;
                    break;
                }
#ifndef PLATFORM_WEB
                case eLIBRARY: {
                    _lastView = _mainView;
                    SetSpacing(0);
                    Begin();
                    BeginPanel("Library / Research");
                    if (_database && _database->render(_font)) {
                        auto program = _database->getSelectedProgram();
                        if (program) {
                            loadBinary(program->name, program->data, program->properties, true);
                            _lastRunView = _mainView = eDEBUGGER;
                        }
                    }
                    Space(_screenHeight - GetCurrentPos().y - 20 - 1);
                    EndPanel();
                    End();
                    break;
                }
#endif
            }

            if (_colorSelectOpen) {
                if (!BeginWindowBox({-1, -1, 200, 250}, "Select Color", &_colorSelectOpen, WindowBoxFlags(WBF_CLOSABLE | WBF_MOVABLE | WBF_MODAL))) {
                    *_selectedColor = _previousColor;
                    _selectedColor = nullptr;
                }
                else {
                    auto pickedColor = toRaylibColor(*_selectedColor);
                    if (ColorPicker(&pickedColor)) {
                        *_selectedColor = toEmuColor(pickedColor);
                        _colorText = fmt::format("{:06x}", _selectedColor->toRGBInt());
                    }
                    Space(5);
                    BeginColumns();
                    SetNextWidth(40);
                    Label("Color:");
                    SetNextWidth(60);
                    TextBox(_colorText, 7);
                    if (IsLastWidgetEditFinished()) {
                        *_selectedColor = emu::Palette::Color("#"+_colorText);
                    }
                    EndColumns();
                    Space(5);
                    BeginColumns();
                    Space(30);
                    SetNextWidth(60);
                    if (Button("Ok")) {
                        _defaultPalette = _colorPalette;
                        setPalette(_defaultPalette);
                        _chipEmu->setPalette(_defaultPalette);
                        _selectedColor = nullptr;
                        _colorSelectOpen = false;
                    }
                    SetNextWidth(60);
                    if (Button("Cancel") || IsKeyPressed(KEY_ESCAPE)) {
                        *_selectedColor = _previousColor;
                        _selectedColor = nullptr;
                        _colorSelectOpen = false;
                    }
                    EndColumns();
                    EndWindowBox();
                }
            }
            if (IsKeyDown(KEY_ESCAPE))
                volumeClick = std::chrono::steady_clock::time_point{};
            if (volumeClick != std::chrono::steady_clock::time_point{}) {
                if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - volumeClick).count() < 2) {
                    Rectangle bounds{430.0, 21.0f, 80.0f, 14.0f};
                    DrawRectangleRec({bounds.x - 56, bounds.y - 2, bounds.width + 58, bounds.height + 4}, {0, 0, 0, 128});
                    GuiSliderBar(bounds, "Volume: ", "", &_volumeSlider, 0.0001f, 1.0f);
                    if (_volumeSlider != _volume)
                        SetMasterVolume(_volumeSlider);
                    if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
                        volumeClick = std::chrono::steady_clock::now();
                    }
                }
                else {
                    if (_volumeSlider != _volume) {
                        _volume = _volumeSlider;
                        _cfg.volume = _volume;
                        saveConfig();
                    }
                }
            }
            EndGui();
        }
        static auto lastExecMode = _chipEmu->execMode();
        if (_chipEmu->execMode() == ExecMode::eRUNNING || (_chipEmu->execMode() != ExecMode::ePAUSED && lastExecMode == ExecMode::ePAUSED)) {
            _debugger.captureStates();
        }
        lastExecMode = _chipEmu->execMode();
    }

    enum PropertyAlign { PA_LEFT, PA_RIGHT };

    static int editProperty(emu::Property& prop, bool forceUpdate, PropertyAlign pa = PA_RIGHT)
    {
        auto prevTextAlignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);
        if(pa == PA_RIGHT) {
            gui::BeginColumns();
            gui::SetSpacing(4);
            gui::SetNextWidth(90);
            gui::SetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_RIGHT);
            gui::Label(fmt::format("{}", prop.getLabel()).c_str());
            gui::SetStyle(LABEL, TEXT_ALIGNMENT, prevTextAlignment);
        }
        //gui::SetNextWidth(150);
        // inactive properties keep their value, but changing it has no effect
        const bool disabled = !prop.hasFlags(emu::PropertyFlags::eWritable) || !prop.isActive();
        if (disabled)
            GuiDisable();
        const auto rc = std::visit(emu::visitor{
                                [](std::nullptr_t) -> int { gui::Label(""); return 0; },
                                [pa, &prop](bool& val) -> int { gui::CheckBox(pa == PA_RIGHT ? "" : prop.getLabel().c_str(), &val); return val ? 1 : 0; },
                                [pa, &prop](emu::Property::Integer& val) -> int { gui::Spinner(pa == PA_RIGHT ? "" : prop.getLabel().c_str(), &val.intValue, val.minValue, val.maxValue); return val.intValue; },
                                [](std::string& val) -> int {
                                    auto prevTextAlignment = GuiGetStyle(TEXTBOX, TEXT_ALIGNMENT);
                                    gui::SetStyle(TEXTBOX, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
                                    gui::TextBox(val, 4096);
                                    gui::SetStyle(TEXTBOX, TEXT_ALIGNMENT, prevTextAlignment);
                                    return 0;
                                },
                                [&forceUpdate](emu::Property::Combo& val) -> int {
                                    if (gui::DropdownBox(val.rgCombo.c_str(), &val.index))
                                        forceUpdate = true;
                                    return val.index;
                                }},
                   prop.getValue());
        if (disabled)
            GuiEnable();
        if(pa == PA_RIGHT) {
            gui::EndColumns();
        }
        return rc;
    }

    // Shows a disabled placeholder with the given label if the core has no such property.
    int editPropertySpinner(std::string_view id, const char* label, bool forceUpdate, int defaultValue = 0)
    {
        if(auto* prop = _properties->find(id)) {
            return editProperty(*prop, forceUpdate, PA_LEFT);
        }
        static int dummyInt = defaultValue;
        GuiDisable();
        gui::Spinner(label, &dummyInt, defaultValue, defaultValue);
        GuiEnable();
        return -1;
    }

    void renderColorField(Vector2 pos, emu::Palette::Color& color)
    {
        using namespace gui;
        bool hover =  CheckCollisionPointRec(GetMousePosition(), {pos.x, pos.y, 16, 16});
        DrawRectangle(pos.x, pos.y, 16, 16, StyleManager::getStyleColor(hover ? Style::BORDER_COLOR_FOCUSED : Style::BORDER_COLOR_NORMAL));
        DrawRectangle(pos.x + 1, pos.y + 1 , 14, 14, StyleManager::getStyleColor(Style::BACKGROUND_COLOR));
        DrawRectangle(pos.x + 2, pos.y + 2 , 12, 12, toRaylibColor(color));
        if(!GuiIsLocked() && IsMouseButtonReleased(0) && hover) {
            _selectedColor = &color;
            _previousColor = color;
            _colorText = fmt::format("{:06x}", color.toRGBInt());
            _colorSelectOpen = true;
        }
    }

    void renderEmulationSettings()
    {
        using namespace gui;
        bool forceUpdate = false;
        BeginColumns();
        SetNextRelativeWidth(0.42f);
        BeginGroupBox("CHIP-8 variant / Core");
        Space(5);
        SetSpacing(2);
        if(DropdownBox(_cores.getCoresCombo().c_str(), &_behaviorSel)) {
            _subBehaviorSel = 0;
            auto preset = _cores[_behaviorSel].variantProperties(0);
            _frameBoost = 1;
            updateEmulatorOptions(preset);
        }
        if(DropdownBox(_cores[_behaviorSel].variantsCombo.c_str(), &_subBehaviorSel)) {
            auto preset = _cores[_behaviorSel].variantProperties(_subBehaviorSel);
            _frameBoost = 1;
            updateEmulatorOptions(preset);
        }
        if(auto* traceLog = _properties->find(emu::propid::TRACE_LOG)) {
            BeginColumns();
            SetSpacing(4);
            SetNextWidth(90);
            editProperty(*traceLog, forceUpdate, PA_LEFT);
            Button("File ...");
            EndColumns();
        }
        else {
            static bool dummyTrace = false;
            GuiDisable();
            CheckBox("Trace-Log", &dummyTrace);
            GuiEnable();
        }
        EndGroupBox();
        //Space(10);
        BeginGroupBox("Emulation Speed");
        Space(5);
        SetIndent(150);
        //SetRowHeight(20);
        SetSpacing(2);
        auto ipf = editPropertySpinner(emu::propid::INSTRUCTIONS_PER_FRAME, "Instructions per frame", forceUpdate);
        editPropertySpinner(emu::propid::FRAME_RATE, "Frame rate", forceUpdate, _chipEmu->frameRate());
        if(ipf != 0) {
            Spinner("Frame boost", &_frameBoost, 1, 1000);
        }
        else {
            static int _fb1{1};
            GuiDisable();
            Spinner("Frame boost", &_fb1, 1, 1000);
            GuiEnable();
        }
        g_frameBoost = getFrameBoost();
        EndGroupBox();
        EndColumns();
        Space(5);
        const int quirksHeight = 181;
        BeginGroupBox("System Configuration");
        auto startY = GetCurrentPos().y;
        auto colWidth1 = GetContentAvailable().width / 2 - 1;
        auto colWidth2 = GetContentAvailable().width - colWidth1 - 1;
        auto colHeight = GetContentAvailable().height;
        auto rowCount = 0;
        Space(5);
        BeginColumns();
        SetSpacing(2);
        SetNextWidth(colWidth1);
        Begin();
        SetSpacing(2);
        // bind only here, a core switch above can change what _properties points to
        auto& props = *_properties;
        for(size_t i = 0; i < props.numProperties(); ++i) {
            auto& prop = props[i];
            if(prop.getId().empty()) {
                auto used = GetCurrentPos().y - startY;
                Space(quirksHeight - used - 4);
                End();
                Begin();
                SetSpacing(2);
            }
            else if(!prop.hasFlags(emu::PropertyFlags::eInvisible) && prop.isApplicable() && prop.getId() != emu::propid::TRACE_LOG && prop.getId() != emu::propid::INSTRUCTIONS_PER_FRAME && prop.getId() != emu::propid::FRAME_RATE) {
                if(props.numProperties() > 20 && std::holds_alternative<bool>(prop.getValue())) {
                    editProperty(prop, forceUpdate, PA_LEFT);
                }
                else {
                    editProperty(prop, forceUpdate);
                }
                ++rowCount;
            }
        }
        auto* changedProp = props.changedProperty(_propsMemento);
        if(changedProp) {
            emu::CoreRegistry::applyRules(props, changedProp->getId());
            // on change...
            if(_chipEmu->updateProperties(props, *changedProp)) {
                // core asks for a cold start...
                updateEmulatorOptions(props);
            }
            else {
                // applied live, so the running core now matches props
                _propsMemento = props;
                _previousProperties = props;
            }
        }
        auto used = GetCurrentPos().y - startY;
        Space(quirksHeight - used - 4);
        End();
        EndColumns();
        EndGroupBox();

        float xOffset = 65.0f; //_colorPalette.numBackgroundColors > 1 ? 68.0f : 52.0f;
        Space(5);
        BeginGroupBox("Palette");
        {
            Space(4);
            StyleManager::Scope guard;
            BeginColumns();
            auto pos = GetCurrentPos();
            pos.x = std::ceil(pos.x);
            pos.y = std::ceil(pos.y);
            SetNextWidth(xOffset + 16*18);
            Label("    Colors");
//#if 0 // TODO: fix this
            auto numColors = _colorPalette.colors.size()>16 ? 2 : _colorPalette.colors.size();
            auto prevPalette = _colorPalette.colors;
            for (int i = 0; i < numColors; ++i) {
                renderColorField({pos.x + xOffset + i*18, pos.y}, _colorPalette.colors[i]);
            }
            if(prevPalette != _colorPalette.colors) {
                setPalette(_colorPalette);
            }
//#endif
            static constexpr int customSel = 8;
            static int sel = customSel;
            if(_colorPalette.numBackgroundColors <= 1 && DropdownBox("Cadmium;Silicon-8;Pico-8;Octo Classic;LCD;C64;Intellivision;CGA;Custom", &sel, true)) {
                switch(sel) {
                    case 0: // Cadmium
                        setPalette({
                            0x1a1c2cff, 0xf4f4f4ff, 0x94b0c2ff, 0x333c57ff,
                            0xb13e53ff, 0xa7f070ff, 0x3b5dc9ff, 0xffcd75ff,
                            0x5d275dff, 0x38b764ff, 0x29366fff, 0x566c86ff,
                            0xef7d57ff, 0x73eff7ff, 0x41a6f6ff, 0x257179ff
                        }, 0, true);
                        sel = customSel;
                        break;
                    case 1: // Silicon-8
                        setPalette({
                            0x000000ff, 0xffffffff, 0xaaaaaaff, 0x555555ff,
                            0xff0000ff, 0x00ff00ff, 0x0000ffff, 0xffff00ff,
                            0x880000ff, 0x008800ff, 0x000088ff, 0x888800ff,
                            0xff00ffff, 0x00ffffff, 0x880088ff, 0x008888ff
                        }, 0, true);
                        sel = customSel;
                        break;
                    case 2: // Pico-8
                        setPalette({
                            0x000000ff, 0xfff1e8ff, 0xc2c3c7ff, 0x5f574fff,
                            0xef7d57ff, 0x00e436ff, 0x29adffff, 0xffec27ff,
                            0xab5236ff, 0x008751ff, 0x1d2b53ff, 0xffa300ff,
                            0xff77a8ff, 0xffccaaff, 0x7e2553ff, 0x83769cff
                        }, 0, true);
                        sel = customSel;
                        break;
                    case 3: // Octo Classic
                        setPalette({
                            0x996600ff, 0xFFCC00ff, 0xFF6600ff, 0x662200ff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff
                        }, 0, true);
                        sel = customSel;
                        break;
                    case 4: // LCD
                        setPalette({
                            0xf2fff2ff, 0x5b8c7cff, 0xadd9bcff, 0x0d1a1aff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff,
                            0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff
                        }, 0, true);
                        sel = customSel;
                        break;
                    case 5: // C64
                        setPalette({
                            0x000000ff, 0xffffffff, 0xadadadff, 0x626262ff,
                            0xa1683cff, 0x9ae29bff, 0x887ecbff, 0xc9d487ff,
                            0x9f4e44ff, 0x5cab5eff, 0x50459bff, 0x6d5412ff,
                            0xcb7e75ff, 0x6abfc6ff, 0xa057a3ff, 0x898989ff
                        }, 0, true);
                        sel = customSel;
                        break;
                    case 6: // Intellivision
                        setPalette({
                            0x0c0005ff, 0xfffcffff, 0xa7a8a8ff, 0x3c5800ff,
                            0xff3e00ff, 0x6ccd30ff, 0x002dffff, 0xfaea27ff,
                            0xffa600ff, 0x00a720ff, 0xbd95ffff, 0xc9d464ff,
                            0xff3276ff, 0x5acbffff, 0xc81a7dff, 0x00780fff
                        }, 0, true);
                        sel = customSel;
                        break;
                    case 7: // CGA
                        setPalette({
                            0x000000ff, 0xffffffff, 0xaaaaaaff, 0x555555ff,
                            0xff5555ff, 0x55ff55ff, 0x5555ffff, 0xffff55ff,
                            0xaa0000ff, 0x00aa00ff, 0x0000aaff, 0xaa5500ff,
                            0xff55ffff, 0x55ffffff, 0xaa00aaff, 0x00aaaaff
                        }, 0, true);
                        sel = customSel;
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
        if (_colorPalette.numBackgroundColors > 1) {
            Space(2);
            StyleManager::Scope guard;
            BeginColumns();
            auto pos = GetCurrentPos();
            pos.x = std::ceil(pos.x);
            pos.y = std::ceil(pos.y);
            //SetNextWidth(52.0f + 16*18);
            Label("Background");
            //#if 0 // TODO: fix this
            for (int i = 0; i < _colorPalette.backgroundColors.size(); ++i) {
                renderColorField({pos.x + xOffset + i * 18, pos.y}, _colorPalette.backgroundColors[i]);
            }
            EndColumns();
        }
        {
            Space(2);
            StyleManager::Scope guard;
            BeginColumns();
            SetSpacing(0);
            auto pos = GetCurrentPos();
            pos.x = std::ceil(pos.x);
            pos.y = std::ceil(pos.y);
            SetNextWidth(xOffset + 18*2 + 2);
            Label("    Border");
            renderColorField({pos.x + xOffset, pos.y}, _colorPalette.borderColor ? _colorPalette.borderColor.value() : _colorPalette.colors[0]);
            renderColorField({pos.x + xOffset + 18, pos.y}, _colorPalette.signalColor ? _colorPalette.signalColor.value() : _colorPalette.colors[1]);
            Label("Signal");
            EndColumns();
        }
        Space(4);
        EndGroupBox();
        Space(8);
        BeginColumns();
        Space(150);
        SetNextRelativeWidth(0.21f);
        bool romRemembered = _cfg.romConfigs.contains(_romSha1);
        if((romRemembered && matchesSettings(_cfg.romConfigs[_romSha1])) || (_romIsWellKnown && matchesSettings(_romWellKnownProperties))) {
            GuiDisable();
        }
        if (Button(!romRemembered ? "Remember for ROM" : "Update for ROM")) {
            _cfg.romConfigs[_romSha1] = *_properties;
            saveConfig();
        }
        GuiEnable();
        if(!romRemembered)
            GuiDisable();
        SetNextRelativeWidth(0.21f);
        if(Button("Forget ROM")) {
            _cfg.romConfigs.erase(_romSha1);
            saveConfig();
        }
        GuiEnable();
        EndColumns();
        auto pos = GetCurrentPos();
        Space(_screenHeight - pos.y - 20 - 1);
        //SetIndent(110);
        //Label("(C) 2022 by Steffen '@gulrak' Schümann");

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
        TextBox(_currentDirectory, 4096);
        if (IsLastWidgetEditFinished()) {
            _librarian.fetchDir(_currentDirectory);
            _currentDirectory = _librarian.currentDirectory();
        }
        Space(1);
        auto tabPos = GetCurrentPos();
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
            auto pos = GetCurrentPos();
            bool hover = false; //CheckCollisionPointRec(GetMousePosition(), {5, pos.y + scroll.y + tabPos.y, area.width, 16.0f});
            TableNextRow(16, hover ? _styleManager.getStyleColor(Style::BASE_COLOR_NORMAL) : rowCol);
            if(TableNextColumn(24)) {
                int icon = ICON_HELP2;
                switch (info.type) {
                    case Librarian::Info::eDIRECTORY: icon = ICON_FOLDER_OPEN; break;
                    case Librarian::Info::eROM_FILE: icon = ICON_ROM; break;
                    case Librarian::Info::eOCTO_SOURCE: icon = ICON_FILETYPE_TEXT; break;
                    default: icon = ICON_FILE_DELETE; break;
                }
                {
                    StyleManager::Scope guard;
                    if (info.type == Librarian::Info::eROM_FILE)
                        guard.setStyle(Style::TEXT_COLOR_NORMAL, info.isKnown ? GREEN : YELLOW);
                    Label(GuiIconText(icon, ""));
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
        SetNextWidth(36);
        Label("File:");
        TextBox(_currentFileName, 4096);
        EndColumns();
        Space(2);
        switch(mode) {
            case eLOAD: {
                auto infoPos = GetCurrentPos();
                Label(fmt::format("SHA1:    {}", selectedInfo.analyzed ? selectedInfo.sha1sum.to_hex() : "").c_str());
                if(!selectedInfo.analyzed || selectedInfo.isKnown) {
                    Label(fmt::format("Type:    {}", selectedInfo.analyzed ?selectedInfo.variant : "").c_str());
                }
                else {
                    Label(fmt::format("Type:    {} (estimated)", selectedInfo.minimumOpcodeProfile()).c_str());
                }
                if(selectedInfo.analyzed) {
                    if(_screenShotSha1 != selectedInfo.sha1sum) {
                        // TODO: Fix this
                        _screenshotData = {}; //_librarian.genScreenshot(selectedInfo, _defaultPalette);
                        _screenShotSha1 = selectedInfo.sha1sum;
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
                    if(_screenShotSha1 == selectedInfo.sha1sum && _screenshotData.width) {
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
                    auto mainView = _mainView;
                    loadRom(_librarian.fullPath(selectedInfo.filePath).c_str(), LoadOptions::None);
                    if(_mainView == mainView)
                        _mainView = _lastView;
                }
                SetNextWidth(110);
                if(Button("Load w/o Config") && selectedInfo.analyzed) {
                    loadRom(_librarian.fullPath(selectedInfo.filePath).c_str(), LoadOptions::DontChangeOptions);
                    _mainView = _lastView;
                }
                GuiEnable();
                EndColumns();
                break;
            }
            case eWEB_SAVE:
            case eSAVE: {
                BeginColumns();
                SetNextWidth(110);
                Label("Select file type:");
                SetNextWidth(70);
                ToggleGroup("ROM File;Source Code", &_saveFileType);
                EndColumns();
                Space(3);
                SetNextWidth(80);
                SetIndent(32);
                if(_currentFileName.empty() && ((_saveFileType == 0 && _romImage.empty()) || (_saveFileType == 1 && _editor.getText().empty()))) GuiDisable();
                if(Button("Save") && !_currentFileName.empty()) {
                    // TODO: Fix this
                    std::string romExtension = ".ch8";
                    if (_saveFileType == 0 && fs::path(_currentFileName).extension() != romExtension) {
                        if (fs::path(_currentFileName).has_extension())
                            _currentFileName = fs::path(_currentFileName).replace_extension(romExtension).string();
                        else
                            _currentFileName += romExtension;
                    }
                    else if (_saveFileType == 1 && fs::path(_currentFileName).extension() != ".8o") {
                        if (fs::path(_currentFileName).has_extension())
                            _currentFileName = fs::path(_currentFileName).replace_extension(".8o").string();
                        else
                            _currentFileName += ".8o";
                    }
#ifndef PLATFORM_WEB
                    auto targetFile = _librarian.fullPath(_currentFileName);
                    std::error_code ec;
                    if (fs::exists(targetFile, ec) && !(_saveFileType == 1 && targetFile == _sourcePath) && _confirmOverwrite != targetFile) {
                        _confirmOverwrite = targetFile;
                    }
                    else {
                        _confirmOverwrite.clear();
                        if(_saveFileType == 0)
                            writeFile(targetFile, (const char*)_romImage.data(), _romImage.size());
                        else
                            saveSource(targetFile);
                        _mainView = _lastView;
                    }
#else
                    if(_saveFileType == 0)
                        webDownloadFile(_currentFileName.c_str(), _romImage.data(), _romImage.size(), "application/octet-stream");
                    else
                        saveSource(_currentFileName);
                    _mainView = _lastView;
#endif
                }
                GuiEnable();
                if (!_confirmOverwrite.empty()) {
                    SetIndent(32);
                    Label(fmt::format("'{}' exists, press Save again to replace it.", fs::path(_confirmOverwrite).filename().string()).c_str());
                }
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
        webRequestOpenFile(".ch8,.hex,.ch10,.hc8,.sc8,.xo8,.c8b,.8o,.gif,.bin,.ram,.c8x,.c8e,.mc8");
    }
#endif

    void saveConfig()
    {
#ifndef PLATFORM_WEB
        if(!_cfgPath.empty()) {
            auto opt = _properties;
            std::vector<std::string> pal(16, "");
            // TODO. Fix this
            //for(size_t i = 0; i < 16; ++i) {
            //    pal[i] = fmt::format("#{:06x}", _defaultPalette[i] >> 8);
            //}
            // opt.advanced["palette"] = pal;
            _cfg.emuProperties = *_properties;
            _cfg.workingDirectory = _currentDirectory;
            //_cfg.libraryPath = _databaseDirectory;
            _cfg.windowPosX = GetWindowPosition().x;
            _cfg.windowPosY = GetWindowPosition().y;
            _cfg.scaleMode = _scaleMode;
            _cfg.recentFiles = _recentFiles;
            if(!_cfg.save(_cfgPath)) {
                TraceLog(LOG_ERROR, "Couldn't write config to '%s'", _cfgPath.c_str());
            }
        }
#endif
    }

    void updateBehaviorSelects()
    {
        if (auto idx = _cores.classIndex(*_properties); idx >= 0) {
            _behaviorSel = idx;
            _subBehaviorSel = static_cast<int>(emu::CoreRegistry::variantIndex(*_properties).index);
        }
    }

    void whenEmuChanged(emu::IEmulationCore& emu) override
    {
        _debugger.updateCore(&emu);
        _debugger.updateOctoBreakpoints(_editor.compiler());
        _propsMemento = *_properties;
        auto startAddress = _chipEmu->defaultLoadAddress();
        _editor.updateCompilerOptions(startAddress);
        updateBehaviorSelects();
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
        updateBehaviorSelects();
        if (source.empty() && !compiler) {
            // A binary program, its disassembly is created when the editor needs it.
            _editor.setGeneratedText(DISASSEMBLY_PLACEHOLDER);
        }
        else {
            _editor.setText(source);
        }
        _editorDisassemblyKey.reset();
        _loadedSourceRevision = _editor.contentRevision();
        _editor.setFilename(filename);
        _sourcePath = compiler && fs::path(filename).extension() == ".8o" ? filename : std::string{};
        if (_romOrigin != ProgramOrigin::DATABASE)
            _recentFiles.usingFile(filename);
        updateMainMenu();
        resetStats();
        if(compiler)
            _debugger.updateOctoBreakpoints(*compiler);
        saveConfig();
        if(autoRun)
            _mainView = eVIDEO;
        else if(compiler && compiler->isError())
            _mainView = eEDITOR;
    }

    bool matchesSettings(const emu::Properties& reference) const
    {
        auto ruled = reference;
        emu::CoreRegistry::applyRules(ruled);
        return *_properties == ruled && _properties->palette() == completedPalette(ruled.palette());
    }

    void whenMachineReset() override
    {
        _audioBuffer.reset();
        updateScreen();
        _debugger.captureStates();
    }

    void saveShortcut()
    {
        if (_mainView == eEDITOR && !_sourcePath.empty())
            saveSource(_sourcePath);
        else
            openSavePage(_mainView == eEDITOR);
    }

    void openSavePage(bool sourceCode)
    {
        if (sourceCode) {
            // Suggest a source file next to the loaded program.
            _saveFileType = 1;
            const auto romPath = fs::path(_romName);
#ifndef PLATFORM_WEB
            std::error_code ec;
            if (romPath.has_parent_path() && fs::is_directory(romPath.parent_path(), ec))
                _currentDirectory = romPath.parent_path().string();
#endif
            if (!romPath.empty())
                _currentFileName = fs::path(romPath.filename()).replace_extension(".8o").string();
        }
        _confirmOverwrite.clear();
        _mainView = eROM_EXPORT;
#ifndef PLATFORM_WEB
        _librarian.fetchDir(_currentDirectory);
#endif
    }

    void saveSource(const std::string& path)
    {
        updateEditorDisassembly(true);
        const auto& source = _editor.getText();
#ifndef PLATFORM_WEB
        writeFile(path, source.data(), source.size());
#else
        webDownloadFile(fs::path(path).filename().string().c_str(), reinterpret_cast<const uint8_t*>(source.data()), source.size(), "text/plain;charset=utf-8");
#endif
        TraceLog(LOG_INFO, "Saved source to %s", path.c_str());
        _sourcePath = path;
    }

    struct EditorDisassembly
    {
        DisassemblyKey key;
        std::string source;
    };

    void updateEditorDisassembly(bool waitForResult = false)
    {
        if (auto result = _disassemblyJob.poll())
            applyEditorDisassembly(std::move(*result));
        if (_editor.contentRevision() != _loadedSourceRevision || (_mainView != eEDITOR && !waitForResult))
            return;
        const auto key = disassemblyKey();
        if (!key || key == _editorDisassemblyKey)
            return;
        if (!_disassemblyJob.busy() || _disassemblyJobKey != key) {
            auto request = disassemblyRequest();
            _disassemblyJobKey = key;
            _editor.setGeneratedText(DISASSEMBLY_PLACEHOLDER);
            _loadedSourceRevision = _editor.contentRevision();
            _disassemblyJob.start([request = std::move(*request)]() {
                return EditorDisassembly{request.key, emu::EmuHostEx::disassemble(request)};
            });
        }
        if (waitForResult) {
            if (auto result = _disassemblyJob.wait())
                applyEditorDisassembly(std::move(*result));
        }
    }

    void applyEditorDisassembly(EditorDisassembly result)
    {
        if (_editor.contentRevision() != _loadedSourceRevision || result.key != disassemblyKey())
            return;
        _editor.setGeneratedText(std::move(result.source));
        _loadedSourceRevision = _editor.contentRevision();
        _editorDisassemblyKey = result.key;
    }

    bool windowShouldClose() const
    {
        return _shouldClose || WindowShouldClose();
    }

private:
    std::mutex _audioMutex;
    ResourceManager _resources;
    gui::StyleManager _styleManager;
    gui::PopupMenu::Menu _mainMenu{};
    Image _fontImage{};
    Image _microFont{};
    Image _titleImage{};
    Image _overlayImage{};
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
    Sha1::Digest _screenShotSha1;
    uint32_t _loadedSourceRevision{};
    std::string _sourcePath;      // the file the editor's source is saved to, empty if there is none yet
    int _saveFileType{0};         // save page: 0 = ROM file, 1 = source code
    std::string _confirmOverwrite;
    static constexpr const char* DISASSEMBLY_PLACEHOLDER = "# Disassembling, please wait...\n";
#ifndef PLATFORM_WEB
    BackgroundJob<EditorDisassembly> _disassemblyJob{_threadPool};
#else
    BackgroundJob<EditorDisassembly> _disassemblyJob;
#endif
    std::optional<DisassemblyKey> _disassemblyJobKey;
    std::optional<DisassemblyKey> _editorDisassemblyKey;
    RenderTexture _overlayTexture{};
    RenderTexture _keyboardOverlay{};
    CircularBuffer<int16_t,1> _audioBuffer;
    int64_t _audioGaps{};
    bool _shouldClose{false};
    bool _showKeyMap{false};
    int _screenWidth{};
    int _screenHeight{};
    bool _windowInvisible{false};
    VideoRenderMode _videoRenderMode{eHIRES};
    std::unique_ptr<TextureScaler> _textureScaler;
    //RenderTexture _renderTexture{};
    AudioStream _audioStream{};
    float _volumeSlider{0.5f};
    float _volume{0.5f};
    SMA<60,uint64_t> _ipfAverage;
    SMA<120,uint32_t> _frameTimeAverage_us;
    SMA<120,int> _frameDelta;
    emu::FpsMeasure _fps;
    int _partialFrameTime{0};
    int _scaleMode{1};
    int _behaviorSel{0};
    int _subBehaviorSel{0};
    //float _messageTime{};
    std::string _timedMessage;
    bool _renderCrt{false};
    bool _updateScreen{false};
    int _frameBoost{1};
    std::atomic_uint _audioCallbackAvgFrames{};
    bool _traceFrame{false};
    bool _colorSelectOpen{false};
    emu::Palette::Color* _selectedColor{nullptr};
    std::string _colorText;
    emu::Palette::Color _previousColor{};
    Rectangle _screenOverlay{};
    int _currentMonitor{0};
    int _screenScale{1};
    int _screenRatio{1};
    bool _screenIsPreRendered{false};
    Vector2 _screenSize{0,0};
    Color _screenBorderColor{0,0,0,255};
    emu::Properties _propsMemento{};
    std::string _avgGuiRenderTime;

    //std::string _romName;
    //std::vector<uint8_t> _romImage;
    //std::string _romSha1Hex;
    //bool _romIsWellKnown{false};
    //emu::Chip8EmulatorOptions _romWellKnownOptions;
    std::array<double,16> _keyScanTime{};
    std::array<bool,16> _keyMatrix{};
    volatile bool _grid{false};
    MainView _mainView{eDEBUGGER};
    MainView _lastView{eDEBUGGER};
    MainView _lastRunView{eDEBUGGER};
    Debugger _debugger;
    LogView _logView;
    Editor _editor;
    Shader _scanLineShader;
    bool _aboutOpen{false};
    Vector2 _aboutScroll{0,0};
    RecentFiles _recentFiles;

    inline static KeyboardKey _keyMapping[16] = {KEY_X, KEY_ONE, KEY_TWO, KEY_THREE, KEY_Q, KEY_W, KEY_E, KEY_A, KEY_S, KEY_D, KEY_Z, KEY_C, KEY_FOUR, KEY_R, KEY_F, KEY_V};
    inline static int _keyPosition[16] = {1,2,3,12, 4,5,6,13, 7,8,9,14, 10,0,11,15};
    inline static Cadmium* _instance{};
};

#ifndef PLATFORM_WEB
std::string dumOctoStateLine(octo_emulator* octo)
{
    if (emu::Logger::getTraceFormat() == emu::Logger::TraceFormat::eCADMIUM2) [[likely]] {
        return fmt::format("{:04x}:{:04x} V=[{:02x} {:02x} {:02x} {:02x}  {:02x} {:02x} {:02x} {:02x}  {:02x} {:02x} {:02x} {:02x}  {:02x} {:02x} {:02x} {:02x}] I={:04x} SP={:1x}", octo->pc, (octo->ram[octo->pc]<<8)|octo->ram[octo->pc+1],
            octo->v[0], octo->v[1], octo->v[2], octo->v[3], octo->v[4], octo->v[5], octo->v[6], octo->v[7],
            octo->v[8], octo->v[9], octo->v[10], octo->v[11], octo->v[12], octo->v[13], octo->v[14], octo->v[15],
            octo->i, octo->rp);
    }
    return fmt::format("V0:{:02x} V1:{:02x} V2:{:02x} V3:{:02x} V4:{:02x} V5:{:02x} V6:{:02x} V7:{:02x} V8:{:02x} V9:{:02x} VA:{:02x} VB:{:02x} VC:{:02x} VD:{:02x} VE:{:02x} VF:{:02x} I:{:04x} SP:{:1x} PC:{:04x} O:{:04x}",
        octo->v[0], octo->v[1], octo->v[2], octo->v[3], octo->v[4], octo->v[5], octo->v[6], octo->v[7],
        octo->v[8], octo->v[9], octo->v[10], octo->v[11], octo->v[12], octo->v[13], octo->v[14], octo->v[15],
        octo->i, octo->rp, octo->pc, (octo->ram[octo->pc]<<8)|octo->ram[octo->pc+1]);
}
#endif

std::string chip8EmuScreen(emu::IEmulationCore& chip8)
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

std::string chip8EmuScreenANSI(emu::IEmulationCore& chip8)
{
    static int col[16] = {0,15,7,8, 9,10,12,11, 1,2,4,3, 13,14,5,6};
    std::string result;
    auto width = chip8.getCurrentScreenWidth();
    auto maxWidth = 256; //chip8.getMaxScreenWidth();
    auto height = chip8.getCurrentScreenHeight();
    if (const auto* screen = chip8.getScreen()) {
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

void dumpOpcodeTable(std::ostream& os, emu::chip8::VariantSet variants = emu::chip8::ALL_VARIANTS)
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
    for (auto variant : variants) {
        os << R"(<th class="rotate"><div><span>)" << emu::Chip8Decompiler::chipVariantName(variant).first << "</span></div></th>";
    }
    /*
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
    */
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    os << "</ul><div class=\"footer\">Generated by Cadmium v" << CADMIUM_VERSION << ", on " << std::put_time( std::gmtime( &t ), "%F" ) << "</div></body></html>";
}

void dumpOpcodeJSON(std::ostream& os, emu::chip8::VariantSet variants = emu::chip8::ALL_VARIANTS)
{
    using namespace nlohmann;
    ordered_json root = ordered_json::object({});
    ordered_json collection = json::array({});
    std::regex quirkRE(R"(\s*\[Q:([^\]]+)\])");
    std::map<std::string, size_t> quirkMap;
    std::vector<std::string> quirkList;
    for(const auto& info : emu::detail::opcodes) {
        if(info.variants.containsAny(variants)) {
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
            for (auto variant : variants) {
                obj["platforms"].push_back(emu::Chip8Decompiler::chipVariantName(variant).first);
            }
            /*
            auto mask = static_cast<uint64_t>(variants & info.variants);
            while(mask) {
                auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
                mask &= mask - 1;
                obj["platforms"].push_back(emu::Chip8Decompiler::chipVariantName(cv).first);
            }*/
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

void dumpLibraryNickel()
{
#if 0
    std::set<std::pair<std::string,std::string>> variantSet;
    size_t romCount = 0;
    for(size_t i = 0; i < Librarian::numKnownRoms(); ++i) {
        const auto& rom = Librarian::getRomInfo(i);
        auto options = Librarian::getOptionsForSha1(rom.sha1);
        if ((options.behaviorBase != emu::Chip8EmulatorOptions::eCHIP8 &&
            options.behaviorBase != emu::Chip8EmulatorOptions::eSCHIP11 &&
            options.behaviorBase != emu::Chip8EmulatorOptions::eSCHIP_MODERN &&
            options.behaviorBase != emu::Chip8EmulatorOptions::eSCHPC &&
            options.behaviorBase != emu::Chip8EmulatorOptions::eXOCHIP) ||
            options.optLoadStoreIncIByX || options.optOnlyHires || options.optCyclicStack) {
            std::cout << fmt::format("    //{{0x{}, {{{}, {}, {}}}}},  // {}", std::string(rom.sha1, 8), emu::Chip8EmulatorOptions::shortNameOfPreset(options.behaviorBase), "???", options.instructionsPerFrame, rom.name ? rom.name : "") << std::endl;
        }
        else {
            std::string variant = "CHIP8";
            std::ostringstream quirks;
            //{ LogicQuirk = 1, ShiftQuirk = 2, MemoryIUnchanged = 4, JumpQuirk = 8, BytePixel = 16, DisplayWait = 32, ColorSupport = 64, Lores16Pixel = 128, Wrapping = 256 };
            if (!options.optDontResetVf)
                quirks << "|LogicQuirk";
            if (options.optJustShiftVx)
                quirks << "|ShiftQuirk";
            if (options.optLoadStoreDontIncI)
                quirks << "|MemoryIUnchanged";
            if (options.optJump0Bxnn)
                quirks << "|JumpQuirk";
            if (!options.optInstantDxyn)
                quirks << "|DisplayWait";
            if (options.optWrapSprites)
                quirks << "|Wrapping";
            if (options.optLoresDxy0Is16x16)
                quirks << "|Lores16Pixel";
            if (options.optAllowColors)
                quirks << "|ColorSupport|BytePixel";
            if (options.optSCLoresDrawing)
                quirks << "|ColorSupport|BytePixel|SCLoresDraw";
            if (options.optHas16BitAddr)
                variant = "XOCHIP";
            auto quirkStr = quirks.str();
            quirkStr = quirkStr.empty() ? "0" : quirkStr.substr(1);
            std::cout << fmt::format("    {{0x{}, {{{}, {}, {}}}}},  // {}: {}", std::string(rom.sha1, 16), variant, quirkStr, options.instructionsPerFrame, emu::Chip8EmulatorOptions::shortNameOfPreset(options.behaviorBase), rom.name ? rom.name : "") << std::endl;
            variantSet.insert({variant, quirkStr});
            ++romCount;
        }
    }
    std::cout << "------ " << variantSet.size() << " variants in " << romCount << " roms" << std::endl;
    for(const auto& vari : variantSet) {
        std::cout << "VARIANT(" << vari.first << ", " << (vari.first == "XOCHIP" ? 65536 : 4096) << ", " << vari.second << ")" << std::endl;
    }
#endif
}

int main(int argc, char* argv[])
{
    static emu::Properties coreProperties;
    static std::list<ghc::CLI::Combo> coreComboOptions; // stable storage, the CLI keeps pointers into it
    static std::vector<std::string> cliChangedProperties; // for the warning about options without effect
    static auto cliPropertyChanged = [](const std::string& id) {
        // the last option of an exclusive group given wins
        emu::CoreRegistry::applyRules(coreProperties, id);
        cliChangedProperties.push_back(id);
    };
    ghc::CLI cli(argc, argv);
    int64_t traceLines = -1;
    bool compareRun = false;
    int64_t benchmark= 0;
    bool showHelp = false;
    bool showVersion = false;
    bool opcodeTable = false;
    bool opcodeJSON = false;
    bool dumpLibNickel = false;
    bool startRom = false;
    bool screenDump = false;
    bool drawDump = false;
    bool headless = false;
    bool headlessLimitSpecified = false;
    HeadlessRunOptions headlessOptions;
    std::string dumpInterpreter;
    int64_t execSpeed = -1;
    std::string randomGen;
    std::string emulationCore;
    int64_t randomSeed = 12345;
    std::vector<std::string> romFile;
    std::string presetName;
    int64_t testSuiteMenuVal = 0;
    cli.category("General Options");
#ifndef PLATFORM_WEB
    cli.optionEnable({"-h", "--help"}, showHelp, "Show this help text");
    cli.optionEnable({"-v", "--version"}, showVersion, "Show version information");
    cli.option({"-t", "--trace"}, traceLines, "Run headless and dump given number of trace lines");
    cli.optionEnable({"-c", "--compare"}, compareRun, "Run and compare with reference engine, trace until diff");
    cli.option({"-b", "--benchmark"}, benchmark, "Run given number of cycles as benchmark");
    cli.optionEnable({"--headless"}, headless, "Run a binary ROM deterministically without a window (generic CHIP-8 core)");
    cli.option({"--until-presentations"}, headlessOptions.untilPresentations, "Stop after this many display updates; requires --headless").range(1, INT64_MAX);
    cli.option({"--max-instructions"}, headlessOptions.maxInstructions, "Limit headless instruction steps (default 3000000)",
               [&headlessLimitSpecified](std::string) { headlessLimitSpecified = true; }).range(1, INT64_MAX);
    cli.option({"--dump-screen"}, headlessOptions.dumpScreen, "Export the last headless presentation as an image");
    cli.option({"--dump-state"}, headlessOptions.dumpState, "Export headless machine state as JSON");
    cli.optionEnable({"--screen-dump"}, screenDump, "When in trace mode, dump the final screen content to the console");
    cli.optionEnable({"--draw-dump"}, drawDump, "Dump screen after every draw when in trace mode.");
    cli.option({"--test-suite-menu"}, testSuiteMenuVal, "Sets 0x1ff to the given value before starting emulation in trace mode, useful for test suite runs.");
    cli.optionEnable({"--opcode-json"}, opcodeJSON, "Dump opcode information as JSON to stdout");
#ifndef NDEBUG
    cli.option({"--dump-interpreter"}, dumpInterpreter, "Dump the given interpreter in a local file named '<interpreter>.ram' and exit");
    cli.optionEnable({"--dump-library-nickel"}, dumpLibNickel, "Dump library table for Nickel");
#endif
#else
#ifdef WEB_WITH_FETCHING
    std::string urlLoad;
    cli.option({"-u", "--url"}, urlLoad, "An url that will be tried to load a rom or source from");
#endif
#endif
    cli.option({"-r", "--run"}, startRom, "if a ROM is given (positional) start it");
    emu::CoreRegistry reg{};
    std::string coresAvailable;
    std::string presetsDescription;
    for(const auto& [name, info] : reg) {
        //std::cout << toOptionName(name) << std::endl;
        coresAvailable += fmt::format("        {} - {}\n", toOptionName(name), info->description);
        presetsDescription += fmt::format("        {}:\n", info->description);
        for(size_t i = 0; i < info->numberOfVariants(); ++i) {
            if(info->prefix().empty())
                presetsDescription += fmt::format("            {} - {} ({})\n", toOptionName(info->variantName(i)), info->variantDescription(i), info->variantExtensions(i));
            else
                presetsDescription += fmt::format("            {} - {} ({})\n", toOptionName(info->prefix() + '-' + info->variantName(i)), info->variantDescription(i), info->variantExtensions(i));
        }
        auto proto = info->propertiesPrototype();
        auto oldCat = cli.category(fmt::format("{} Options (only available if preset uses {} core)", name, info->prefix().empty() ? "default" : toOptionName(info->prefix())));
        for(size_t i = 0; i < proto.numProperties(); ++i) {
            auto& prop = proto[i];
            if(prop.hasFlags(emu::PropertyFlags::eWritable)) {
                auto dependencyCheck = [&presetName, info, id = prop.getId()]() {
                    const auto* current = coreProperties.find(id);
                    return info->hasVariant(presetName) && current && current->isApplicable();
                };
                std::visit(emu::visitor{
                   [](std::nullptr_t) -> void {  },
                   [dependencyCheck,&prop,&cli](bool& val) -> void { cli.option<bool>({fmt::format("--{}", prop.getOptionName())}, [id = prop.getId()](const std::string&, const bool& value) {
                       coreProperties.at(id).setBool(value);
                       cliPropertyChanged(id);
                   }, prop.getDescription()).dependsOn(dependencyCheck); },
                   [dependencyCheck,&prop,&cli](emu::Property::Integer& val) -> void { cli.option<int>({fmt::format("--{}", prop.getOptionName())}, [id = prop.getId()](const std::string&, const int& value) {
                       coreProperties.at(id).setInt(value);
                       cliPropertyChanged(id);
                   }, prop.getDescription()).dependsOn(dependencyCheck).range(prop.getIntMin(), prop.getIntMax()); },
                   [dependencyCheck,&prop,&cli](std::string& val) -> void { cli.option<std::string>({fmt::format("--{}", prop.getOptionName())}, [id = prop.getId()](const std::string&, const std::string& value) {
                       coreProperties.at(id).setString(value);
                       cliPropertyChanged(id);
                   }, prop.getDescription()).dependsOn(dependencyCheck); },
                   [dependencyCheck,&prop,&cli](emu::Property::Combo& val) -> void {
                       std::ostringstream optionList;
                       for (auto i = val.options.begin(); i != val.options.end(); ++i) {
                           if (i != val.options.begin()) {
                               optionList << ", ";
                           }
                           optionList << toOptionName(*i);
                       }
                       // The CLI matches the argument against Combo::combinations, so they must be filled in
                       auto& cliCombo = coreComboOptions.emplace_back(ghc::CLI::Combo{val.index, val.options});
                       cli.option({fmt::format("--{}", prop.getOptionName())}, cliCombo, prop.getDescription() + " (" + optionList.str() +")", [&cliCombo, id = prop.getId()](const std::string&) {
                            coreProperties.at(id).setSelectedIndex(cliCombo.index);
                            cliPropertyChanged(id);
                       }).dependsOn(dependencyCheck);
                   }
               }, prop.getValue());
            }
        }
        cli.category(oldCat);
    }
    cli.option({"-p", "--preset"}, presetName, "Select one of the following available preset:\n" + trimRight(presetsDescription), [&presetName](std::string) {
        // Todo: Set coreProperties to a matching instance
        coreProperties = emu::CoreRegistry::propertiesForPreset(presetName);
        if(!coreProperties) {
            throw std::runtime_error(fmt::format("Unknown preset: '{}' (use --help to see supported presets)", presetName));
        }
        emu::CoreRegistry::applyRules(coreProperties);
    });
#if 0
    cli.option({"-s", "--exec-speed"}, execSpeed, "Set execution speed in instructions per frame (0-500000, 0: unlimited)");
    cli.option({"--random-gen"}, randomGen, "Select a predictable random generator used for trace log mode (rand-lgc or counting)");
    cli.option({"--random-seed"}, randomSeed, "Select a random seed for use in combination with --random-gen, default: 12345");
    cli.option({"--trace-log"}, options.optTraceLog, "If true, enable trace logging into log-view");
    //cli.option({"--opcode-table"}, opcodeTable, "Dump an opcode table to stdout");
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
    cli.option({"--half-pixel-scroll"}, options.optHalfPixelScroll, "If true, use SCHIP1.1 lores half pixel scrolling");
    cli.option({"--mode-change-clear"}, options.optModeChangeClear, "If true, clear screen on lores/hires changes");
    cli.option({"--jump0-bxnn"}, options.optJump0Bxnn, "If true, use Vx as offset for Bxnn");
    cli.option({"--allow-hires"}, options.optAllowHires, "If true, support for hires (128x64) is enabled");
    cli.option({"--only-hires"}, options.optOnlyHires, "If true, emulation has hires mode only");
    cli.option({"--allow-color"}, options.optAllowColors, "If true, support for multi-plane drawing is enabled");
    cli.option({"--cyclic-stack"}, options.optCyclicStack, "If true, stack operations wrap around, overwriting used slots");
    cli.option({"--has-16bit-addr"}, options.optHas16BitAddr, "If true, address space is 16bit (64k ram)");
    cli.option({"--xo-chip-sound"}, options.optXOChipSound, "If true, use XO-CHIP sound instead of buzzer");
    cli.option({"--extended-display-wait"}, options.optExtendedVBlank, "If true, Dxyn might even wait 2 screens depending on size and position");
#endif
    static auto supportedExtensions = reg.getSupportedExtensions();
    auto extensions = join(supportedExtensions.begin(), supportedExtensions.end(), ", ");
    auto icons = gui::GetDefaultFileIcons();
    for (const auto& extension : supportedExtensions) {
        icons[std::string_view(extension).substr(1)] = ICON_ROM;
    }
    gui::SetDefaultFileIcons(icons);
    cli.positional(romFile, fmt::format("ROM file or source to load ({})", extensions));

    CadmiumConfiguration config;
#ifndef PLATFORM_WEB
    auto cfgPath = (fs::path(dataPath())/"config.json").string();
    if(config.load(cfgPath)) {
        coreProperties = config.emuProperties;
    }
#endif
    try {
        cli.parse();
    }
    catch(std::exception& ex) {
        std::cerr << "ERROR: " << ex.what() << std::endl;
        exit(1);
    }
    if(showHelp) {
        cli.usage();
        exit(0);
    }
    for (const auto& id : cliChangedProperties) {
        if (const auto& prop = coreProperties.at(id); !prop.isActive()) {
            std::cerr << "WARNING: --" << prop.getOptionName() << " has no effect with the other settings of this preset" << std::endl;
        }
    }
    if(showVersion) {
        std::cout << "Cadmium v" << CADMIUM_VERSION << "-" << CADMIUM_GIT_HASH << std::endl;
        std::cout << "(c) Copyright 2022 by Steffen 'Gulrak' Schümann" << std::endl;
        exit(0);
    }
    // No preset given and no configuration yet (e.g. first start), use a default.
    if (!coreProperties) {
        coreProperties = emu::CoreRegistry::propertiesForPreset(presetName.empty() ? "schipc" : presetName);
    }
#ifndef PLATFORM_WEB
    if(opcodeTable) {
        dumpOpcodeTable(std::cout, emu::C8V::CHIP_8|emu::C8V::CHIP_10|emu::C8V::CHIP_48|emu::C8V::SCHIP_1_0|emu::C8V::SCHIP_1_1|emu::C8V::MEGA_CHIP|emu::C8V::XO_CHIP);
        exit(0);
    }
    if(opcodeJSON) {
        dumpOpcodeJSON(std::cout, emu::C8V::CHIP_8|emu::C8V::CHIP_8_I|emu::C8V::CHIP_8X|emu::C8V::CHIP_8E|emu::C8V::CHIP_10|emu::C8V::CHIP_8_D6800|emu::C8V::CHIP_48|emu::C8V::SCHIP_1_0|emu::C8V::SCHIP_1_1|emu::C8V::SCHIPC|emu::C8V::SCHIP_MODERN|emu::C8V::MEGA_CHIP|emu::C8V::XO_CHIP);
        exit(0);
    }
    if(dumpLibNickel) {
        dumpLibraryNickel();
        exit(0);
    }
    if(!dumpInterpreter.empty()) {
        // TODO: Fix this
        /*
        auto data = emu::CosmacVIP::getInterpreterCode(toUpper(dumpInterpreter));
        if(!data.empty()) {
            {
                std::ofstream os(dumpInterpreter + ".ram", std::ios::binary);
                os.write((const char*)data.data(), static_cast<ssize_t>(data.size()));
            }
            std::cout << "Written " << data.size() << " bytes to '" << dumpInterpreter << ".ram'." << std::endl;
            exit(0);
        }
        else {
            std::cerr << "ERROR: Unknown interpreter '" << dumpInterpreter << "'." << std::endl;
            exit(1);
        }
        */
    }
#endif
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
    //if(execSpeed >= 0) {
    //    options.instructionsPerFrame = execSpeed;
    //}
#ifndef PLATFORM_WEB
    if (headless) {
        if (romFile.empty() || traceLines >= 0 || compareRun || benchmark) {
            std::cerr << "ERROR: --headless requires one ROM and cannot be combined with --trace, --compare, or --benchmark" << std::endl;
            return 1;
        }
        try {
            return runHeadless(romFile.front(), coreProperties, headlessOptions);
        }
        catch (const std::exception& ex) {
            std::cerr << "ERROR: " << ex.what() << std::endl;
            return 1;
        }
    }
    if (headlessOptions.untilPresentations >= 0 || headlessLimitSpecified ||
        !headlessOptions.dumpScreen.empty() || !headlessOptions.dumpState.empty()) {
        std::cerr << "ERROR: headless run options require --headless" << std::endl;
        return 1;
    }
#endif
    if(traceLines < 0 && !compareRun && !benchmark) {

#ifndef PLATFORM_WEB
        Cadmium cadmium(config, coreProperties);
        if (!romFile.empty()) {
            Cadmium::LoadOptions loadOpt = Cadmium::LoadOptions::None;
            if(startRom)
                loadOpt |= Cadmium::LoadOptions::SetToRun;
            if(!presetName.empty())
                loadOpt |= Cadmium::LoadOptions::DontChangeOptions;
            cadmium.loadRom(romFile.front().c_str(), loadOpt);
        }
        //SetTargetFPS(60);
        while (!cadmium.windowShouldClose()) {
            cadmium.updateAndDraw();
        }
#else
        try {
            Cadmium cadmium(config, coreProperties);
#ifdef WEB_WITH_FETCHING
            if(!urlLoad.empty()) {
                emscripten_fetch_attr_t attr;
                emscripten_fetch_attr_init(&attr);
                strcpy(attr.requestMethod, "GET");
                attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
                attr.onsuccess = loadBinaryCallbackC;
                attr.onerror = downloadFailedCallbackC;
                Cadmium::LoadOptions loadOpt = Cadmium::LoadOptions::None;
                if(!presetName.empty())
                    loadOpt |= Cadmium::LoadOptions::DontChangeOptions;
                loadBinaryCallback = [&](std::string filename, const uint8_t* data, size_t size) {
                    cadmium.loadBinary(filename, {data, size}, loadOpt);
                };
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
#if !defined(PLATFORM_WEB)
    else {
        emu::HeadlessHost host;
        //chip8options.optHas16BitAddr = true;
        //chip8options.optWrapSprites = true;
        //chip8options.optAllowColors = true;
        //chip8options.optJustShiftVx = false;
        //chip8options.optLoadStoreDontIncI = false;
        //chip8options.optDontResetVf = true;
        //chip8options.optInstantDxyn = true;
        //options.optExtendedVBlank = false;
        //if(!randomGen.empty()) {
        //    options.advanced = nlohmann::ordered_json::object({
        //        {"random", randomGen},
        //        {"seed", randomSeed}
        //    });
        //    options.updatedAdvanced();
        //}
        host.updateEmulatorOptions(coreProperties);
        auto* chip8 = dynamic_cast<emu::IChip8Emulator*>(host.emuCore().executionUnit(0));
        if (!chip8) {
            std::cerr << "Selected core is not capable of CHIP-8 control." << std::endl;
            exit(1);
        }
        std::clog << "Engine:  " << chip8->name() << ", active variant: " << presetName << std::endl;
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
        int64_t i = 0;
        if(compareRun) {
            octo_emulator_init(&octo, (char*)chip8->memory() + 512, 4096 - 512, &oopt, nullptr);
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
                    std::cout << chip8EmuScreen(host.emuCore());
                }
                ++i;
            } while ((i & 0xfff) || (chip8->dumpStateLine() == dumOctoStateLine(&octo) && chip8EmuScreen(host.emuCore()) == octoScreen(octo)));
            std::clog << i << ": " << chip8->dumpStateLine() << std::endl;
            std::clog << i << "| " << dumOctoStateLine(&octo) << std::endl;
            std::cerr << chip8EmuScreen(host.emuCore());
            std::cerr << "---" << std::endl;
            std::cerr << octoScreen(octo) << std::endl;
        }
        else if(benchmark > 0) {
            uint64_t instructions = benchmark;
            int ipf = 42;
            if(coreProperties.contains(emu::propid::INSTRUCTIONS_PER_FRAME)) {
                ipf = coreProperties.at(emu::propid::INSTRUCTIONS_PER_FRAME).getInt();
            }
            std::cout << "Executing benchmark (" << /*chip8->options.instructionsPerFrame*/ ipf << "ipf)..." << std::endl;
            auto startChip8 = std::chrono::steady_clock::now();
            auto ticks = uint64_t(instructions / ipf);
            for(i = 0; i < ticks; ++i) {
                host.emuCore().executeFrame();
            }
            int64_t lastCycles = -1;
            int64_t cycles = 0;
            while((cycles = chip8->cycles()) < instructions && cycles != lastCycles) {
                chip8->executeInstruction();
                lastCycles = cycles;
            }
            auto durationChip8 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startChip8);
            if(screenDump) {
                std::cout << chip8EmuScreenANSI(host.emuCore());
            }
            std::cout << "Executed instructions: " << chip8->cycles() << std::endl;
            std::cout << "Executed frames: " << chip8->frames() << std::endl;
            std::cout << "Cadmium: " << durationChip8.count() << "us, " << int(double(chip8->cycles())/durationChip8.count()) << "MIPS" << std::endl;
        }
        else if(traceLines >= 0) {
#if 0
            chip8.memory()[0x1ff] = testSuiteMenuVal & 0xff;
            size_t waits = 0;
            do {
                bool isDraw = (chip8.opcode() & 0xF000) == 0xD000;
                bool isWait = !(!isDraw || options.optInstantDxyn || (chip8.getCycles() % options.instructionsPerFrame == 0));
                if ((chip8.getCycles() % options.instructionsPerFrame) == 0) {
                    std::cout << "--- handle timer ---" << std::endl;
                    chip8.handleTimer();
                }
                std::cout << (i - waits) << "/" << chip8.getCycles() << ": " << chip8.dumpStateLine() << (isWait ? " (WAIT)" : "") << std::endl;
                if(isWait) ++waits;
                uint16_t opcode = chip8.opcode();
                chip8.executeInstruction();
                if(chip8.needsScreenUpdate()) {
                    std::cout << chip8EmuScreen(chip8);
                }
                else if((opcode & 0xF0FF) == 0xF00A)
                    break;
                ++i;
            } while (i <= traceLines && chip8.getExecMode() == emu::IChip8Emulator::ExecMode::eRUNNING);
            if(screenDump) {
                std::cout << chip8EmuScreenANSI(chip8);
            }
#endif
        }
    }
#endif
    return 0;
}
