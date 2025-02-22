//---------------------------------------------------------------------------------------
// eti660.cpp
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

#include <date/date.h>

#include <chiplet/utility.hpp>
#include <emulation/eti660.hpp>
#include <emulation/hardware/cdp186x.hpp>
#include <emulation/hardware/keymatrix.hpp>
#include <emulation/hardware/mc682x.hpp>
#include <emulation/logger.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <ghc/random.hpp>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <memory>

#define VIDEO_FIRST_VISIBLE_LINE 80
#define VIDEO_FIRST_INVISIBLE_LINE  208

namespace emu {

static const std::string PROP_CLASS = "ETI660";
static const std::string PROP_TRACE_LOG = "Trace Log";
static const std::string PROP_CPU = "CPU";
static const std::string PROP_CLOCK = "Clock Rate";
static const std::string PROP_RAM = "Memory";
static const std::string PROP_CLEAN_RAM = "Clean RAM";
static const std::string PROP_VIDEO = "Video";
static const std::string PROP_AUDIO = "Audio";
static const std::string PROP_KEYBOARD = "Keyboard";
static const std::string PROP_ROM_NAME = "ROM Name";
static const std::string PROP_INTERPRETER = "Interpreter";
static const std::string PROP_START_ADDRESS = "Start Address";

enum ETIVideoType { ETI_VT_CDP1864 };
enum ETIAudioType { ETI_AT_CDP1864 };
enum ETIKeyboard { ETIK_HEX, ETI_TWO_ROW, ETI_VIP_HEX };

struct Eti660Options
{
    Properties asProperties() const
    {
        auto result = registeredPrototype();
        result[PROP_TRACE_LOG].setBool(traceLog);
        result[PROP_CPU].setString(cpuType);
        result[PROP_CLOCK].setInt(clockFrequency);
        result[PROP_RAM].setSelectedText(std::to_string(ramSize)); // !!!!
        result[PROP_CLEAN_RAM].setBool(cleanRam);
        result[PROP_VIDEO].setSelectedIndex(toType(videoType));
        result[PROP_AUDIO].setSelectedIndex(toType(audioType));
        result[PROP_KEYBOARD].setSelectedIndex(toType(keyboard));
        result[PROP_ROM_NAME].setString(romName);
        //result[PROP_INTERPRETER].setSelectedIndex(toType(interpreter));
        result[PROP_START_ADDRESS].setInt(startAddress);
        result.palette() = palette;
        return result;
    }
    static Eti660Options fromProperties(const Properties& props)
    {
        Eti660Options opts{};
        opts.traceLog = props[PROP_TRACE_LOG].getBool();
        opts.cpuType = props[PROP_CPU].getString();
        opts.clockFrequency = props[PROP_CLOCK].getInt();
        opts.ramSize = std::stoul(props[PROP_RAM].getSelectedText()); // !!!!
        opts.cleanRam = props[PROP_CLEAN_RAM].getBool();
        opts.videoType = static_cast<ETIVideoType>(props[PROP_VIDEO].getSelectedIndex());
        opts.audioType = static_cast<ETIAudioType>(props[PROP_AUDIO].getSelectedIndex());
        opts.keyboard = static_cast<ETIKeyboard>(props[PROP_KEYBOARD].getSelectedIndex());
        opts.romName = props[PROP_ROM_NAME].getString();
        //opts.interpreter = static_cast<VIPChip8Interpreter>(props[PROP_INTERPRETER].getSelectedIndex());
        opts.startAddress = props[PROP_START_ADDRESS].getInt();
        opts.palette = props.palette();
        return opts;
    }
    static Properties& registeredPrototype()
    {
        using namespace std::string_literals;
        auto& prototype = Properties::getProperties(PROP_CLASS);
        if(!prototype) {
            prototype.registerProperty({PROP_TRACE_LOG, false, "Enable trace log", eWritable});
            prototype.registerProperty({PROP_CPU, "CDP1802"s, "CPU type (currently only cdp1802)"});
            prototype.registerProperty({PROP_CLOCK, Property::Integer{1773448, 100000, 500'000'000}, "Clock frequency, default is 1773448", eWritable});
            prototype.registerProperty({PROP_RAM, Property::Combo{"3072"s}, "Size of ram in bytes", eWritable});
            prototype.registerProperty({PROP_CLEAN_RAM, false, "Delete ram on startup", eWritable});
            prototype.registerProperty({PROP_VIDEO, Property::Combo{"CDP1864"}, "Video hardware, only cdp1864"});
            prototype.registerProperty({PROP_AUDIO, Property::Combo{"CDP1864"}, "Audio hardware, only cdp1864"});
            prototype.registerProperty({PROP_KEYBOARD, Property::Combo{"ETI660 Hex", "ETI660 2-ROW", "VIP Hex"}, "Keyboard type, default is ETI660 hex"});
            prototype.registerProperty({"", nullptr, ""});
            prototype.registerProperty({PROP_ROM_NAME, "C8-MONITOR"s, "Rom image name, default c8-monitor"});
            //prototype.registerProperty({PROP_INTERPRETER, Property::Combo{"NONE", "CHIP8", "CHIP10", "CHIP8RB", "CHIP8TPD", "CHIP8FPD", "CHIP8X", "CHIP8XTPD", "CHIP8XFPD", "CHIP8E"}, "CHIP-8 interpreter variant"});
            prototype.registerProperty({PROP_START_ADDRESS, Property::Integer{512, 0, 4095}, "Initial CHIP-8 interpreter PC address"});
        }
        return prototype;
    }
    std::string cpuType;
    int clockFrequency;
    size_t ramSize;
    bool cleanRam;
    bool traceLog;
    ETIVideoType videoType;
    ETIAudioType audioType;
    ETIKeyboard keyboard;
    std::string romName;
    uint16_t startAddress;
    Palette palette;
};

struct Eti660SetupInfo {
    const char* presetName;
    const char* description;
    const char* defaultExtensions;
    chip8::VariantSet supportedChip8Variants{chip8::Variant::NONE};
    Eti660Options options;
};

// clang-format off
static Eti660SetupInfo etiPresets[] = {
    {
        "NONE",
        "Raw ETI660",
        ".bin;.hex;.ram;.raw",
        chip8::Variant::CHIP_8_ETI660,
        { .cpuType = "CDP1802", .clockFrequency = 1773448, .ramSize = 3072, .cleanRam = false, .traceLog = false, .videoType = ETI_VT_CDP1864, .audioType = ETI_AT_CDP1864, .keyboard = ETIK_HEX, .romName = "C8-MONITOR", .startAddress = 0}
    }
};
// clang-format on

struct Eti660FactoryInfo final : public CoreRegistry::FactoryInfo<Eti660, Eti660SetupInfo, Eti660Options>
{
    explicit Eti660FactoryInfo(const char* description)
        : FactoryInfo(300, etiPresets, description)
    {}
    std::string prefix() const override
    {
        return "ETI";
    }
    VariantIndex variantIndex(const Properties& props) const override
    {
        auto idx = 0u;
        return {idx, etiPresets[idx].options.asProperties() == props};
    }
};

static bool registeredEti660 = CoreRegistry::registerFactory(PROP_CLASS, std::make_unique<Eti660FactoryInfo>("Hardware emulation of an ETI660"));

class Eti660::Private {
public:
    static constexpr uint64_t CPU_CLOCK_FREQUENCY = 1760640;
    explicit Private(EmulatorHost& host, Cdp1802Bus& bus, Properties& properties)
        : _host(host)
        , _properties(properties)
        , _options(Eti660Options::fromProperties(properties))
        , _cpu(bus, CPU_CLOCK_FREQUENCY)
        , _video(Cdp186x::eCDP1864, _cpu, false)
    {
        using namespace std::string_literals;
        (void)registeredEti660;
        if(_video.getType() == Cdp186x::eVP590) {
            _colorRamMask = 0x3ff;
            _colorRamMaskLores = 0x3e7;
        }
        _properties = _options.asProperties();
        _properties[PROP_ROM_NAME].setAdditionalInfo(fmt::format("(sha1: {})", calculateSha1(_eti660_c8_monitor, 1024).to_hex().substr(0,8)));
        _memorySize = _options.ramSize;
        _ram.resize(_options.ramSize, 0);
    }
    EmulatorHost& _host;
    Properties& _properties;
    Eti660Options _options;
    uint32_t _memorySize{4096};
    Cdp1802 _cpu;
    Cdp186x _video;
    MC682x _pia;
    KeyMatrix<4,4> _keyMatrix;
    //int64_t _irqStart{0};
    int64_t _nextFrame{0};
    uint8_t _keyLatch{0};
    uint8_t _frequencyLatch{0};
    uint16_t _lastOpcode{0};
    uint16_t _currentOpcode{0};
    uint16_t _initialChip8SP{0};
    uint16_t _colorRamMask{0xff};
    uint16_t _colorRamMaskLores{0xe7};
    uint16_t _startAddress{0};
    uint16_t _fetchEntry{0};
    bool _mapRam{false};
    bool _powerOn{true};
    float _wavePhase{0};
    std::vector<uint8_t> _ram{};
    std::array<uint8_t,256> _colorRam{};
    std::array<uint8_t,1024> _rom{};
    std::span<uint8_t> _stackSpan{};
    VideoType _screen;
};


const uint8_t _eti660_c8_monitor[0x400] = {
    0xf8, 0x04, 0xb2, 0xb6, 0xf6, 0xb4, 0xf6, 0xb1, 0xf6, 0xb5, 0xa4, 0xf8, 0x38, 0xa1, 0xa2, 0xf6, 0xa5, 0xf8, 0x0f, 0x52, 0xe2, 0x62, 0xf8, 0x20, 0x52, 0x62, 0xa8, 0xd4, 0x20, 0x4e, 0xf0, 0x0a, 0x00, 0xfc, 0xb0, 0x3a, 0x20, 0x4e, 0xf0, 0x0a, 0x00,
    0x88, 0x78, 0x10, 0x00, 0xdd, 0x20, 0x62, 0xf0, 0x0a, 0x10, 0x4a, 0x01, 0xbc, 0x20, 0x6e, 0x10, 0x2a, 0x10, 0x24, 0x01, 0x8b, 0x01, 0x60, 0x02, 0x40, 0x00, 0xe0, 0x00, 0xf8, 0x26, 0x00, 0x00, 0x00, 0x00, 0x72, 0x10, 0x36, 0x02, 0xeb, 0x00, 0xf8,
    0x68, 0x10, 0x69, 0x2a, 0x00, 0xbf, 0x20, 0x62, 0x78, 0x04, 0x00, 0xbd, 0x20, 0x62, 0x00, 0xee, 0xf1, 0x29, 0xd8, 0x95, 0x78, 0x04, 0xf0, 0x29, 0xd8, 0x95, 0x00, 0xee, 0x02, 0xf2, 0x10, 0x52, 0x0b, 0xfe, 0xfe, 0xfe, 0xfe, 0x5b, 0xeb, 0x8d, 0xf4,
    0x5b, 0xd4, 0xff, 0xef, 0xd3, 0x8b, 0xfe, 0xab, 0x9b, 0x7e, 0xbb, 0x30, 0x7e, 0x96, 0xbf, 0xaf, 0xf8, 0x80, 0xbe, 0xae, 0xf8, 0x24, 0xa5, 0xde, 0xde, 0xde, 0xde, 0x5f, 0x1f, 0x8b, 0x5f, 0x8d, 0xf4, 0x5f, 0xd4, 0x42, 0x30, 0xa7, 0x42, 0x32, 0xa6,
    0x15, 0x15, 0xd4, 0x32, 0xa4, 0xd4, 0x22, 0xf8, 0x03, 0xbc, 0xf8, 0xcb, 0xac, 0x06, 0xfa, 0x0f, 0xfc, 0x01, 0x52, 0xdc, 0xe2, 0xf5, 0x52, 0x45, 0xa3, 0x8b, 0x38, 0x9b, 0x22, 0x52, 0x96, 0xbe, 0xf8, 0x70, 0xae, 0x42, 0x5e, 0x1e, 0xf6, 0xf6, 0xf6,
    0xf6, 0x5e, 0xd4, 0x1b, 0x4b, 0x32, 0xdc, 0xff, 0x31, 0x32, 0xd0, 0xff, 0x01, 0x3a, 0xd1, 0xd4, 0x0b, 0x30, 0xc0, 0x96, 0xbf, 0xf8, 0x80, 0xaf, 0x93, 0x5f, 0x1f, 0x9f, 0xff, 0x06, 0x3a, 0xe5, 0xd4, 0x42, 0xb5, 0x42, 0xa5, 0xd4, 0x45, 0xe6, 0xf4,
    0x56, 0xd4, 0x22, 0x69, 0x12, 0xd4, 0x22, 0x6c, 0x12, 0xd4, 0x18, 0x1d, 0x28, 0x30, 0x1a, 0x26, 0x2a, 0x1c, 0x2c, 0x2e, 0x16, 0x14, 0x12, 0x20, 0x24, 0x10, 0xe0, 0x80, 0xe0, 0x80, 0x80, 0x80, 0xe0, 0xa0, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0, 0x20, 0x20,
    0x20, 0x20, 0x20, 0xe0, 0xa0, 0xe0, 0x80, 0xe0, 0x80, 0xe0, 0x20, 0xe0, 0x80, 0xe0, 0xa0, 0xe0, 0xa0, 0xe0, 0x20, 0xe0, 0x20, 0xe0, 0x7a, 0x42, 0x70, 0x22, 0x78, 0x22, 0x52, 0xc4, 0x19, 0xf8, 0x80, 0xa0, 0x96, 0xb0, 0xe2, 0xe2, 0x80, 0xe2, 0xe2,
    0x20, 0xa0, 0xe2, 0x20, 0xa0, 0xe2, 0x20, 0xa0, 0x3c, 0x45, 0x98, 0x32, 0x59, 0xa0, 0x20, 0x80, 0xb8, 0x88, 0x32, 0x35, 0x7b, 0x28, 0x30, 0x36, 0xf8, 0xe2, 0xa1, 0xf8, 0xd4, 0xd1, 0x81, 0xbd, 0xd7, 0x3b, 0x66, 0x9d, 0x3a, 0x68, 0xd7, 0x33, 0x6e,
    0x93, 0xbd, 0xad, 0xd7, 0x9d, 0x7e, 0xbd, 0x3b, 0x74, 0xd7, 0x8d, 0xf6, 0x33, 0xfe, 0x9d, 0x5e, 0x8e, 0xd1, 0x1e, 0x2c, 0x9c, 0x3a, 0x6e, 0xc0, 0x00, 0x00, 0xf8, 0xe2, 0xa1, 0xf8, 0xbf, 0xd1, 0xf8, 0xe0, 0xbd, 0xff, 0x00, 0xd7, 0x9d, 0x3a, 0x94,
    0x8e, 0xd1, 0x7b, 0x4e, 0xbb, 0xfc, 0x00, 0xf8, 0x09, 0xab, 0xad, 0xd7, 0x2b, 0x8b, 0x32, 0xaf, 0x9b, 0xfe, 0xbb, 0x30, 0xa5, 0x8d, 0xf6, 0xd7, 0x2c, 0x9c, 0x3a, 0x9a, 0xd7, 0xd7, 0xd7, 0x30, 0x88, 0xff, 0x1b, 0xd4, 0xd3, 0x7b, 0xf8, 0x33, 0x3b,
    0xc7, 0xf8, 0x0d, 0x1d, 0x52, 0xff, 0x01, 0x33, 0xc8, 0x39, 0xbe, 0x7a, 0x02, 0x30, 0xc8, 0x1d, 0xd3, 0xf8, 0x17, 0x35, 0xd6, 0x35, 0xd2, 0xff, 0x01, 0x33, 0xd8, 0x3d, 0xde, 0x30, 0xd3, 0xa7, 0x91, 0xb7, 0x96, 0xbd, 0x95, 0xad, 0x4d, 0xbe, 0x4d,
    0xae, 0x1d, 0xed, 0xf5, 0xac, 0x2d, 0x9e, 0x75, 0xfc, 0x01, 0xbc, 0xe2, 0xd3, 0x22, 0x52, 0x64, 0x30, 0xf8, 0x7b, 0x00, 0x96, 0xb7, 0xe2, 0x94, 0xbc, 0x45, 0xaf, 0xf6, 0xf6, 0xf6, 0xf6, 0x32, 0x29, 0xf9, 0x30, 0xac, 0x8f, 0xfa, 0x0f, 0xf9, 0x70,
    0xa6, 0x05, 0xf6, 0xf6, 0xf6, 0xf6, 0xf9, 0x70, 0xa7, 0x4c, 0xb3, 0x8c, 0xfc, 0x0f, 0xac, 0x0c, 0xa3, 0xd3, 0x30, 0x00, 0x8f, 0xb3, 0x45, 0x30, 0x25, 0x45, 0x56, 0xd4, 0x03, 0x03, 0x03, 0x03, 0x03, 0x02, 0x00, 0x03, 0x03, 0x02, 0x03, 0x02, 0x02,
    0x00, 0x03, 0xdb, 0x7c, 0x75, 0x9e, 0xa8, 0xb2, 0x2e, 0xf3, 0x81, 0xae, 0x50, 0xb6, 0x55, 0x60, 0xaa, 0x05, 0x45, 0xaa, 0x86, 0xba, 0xd4, 0xe9, 0x99, 0xf4, 0xe6, 0xf4, 0xb9, 0x56, 0x45, 0xf2, 0x56, 0xd4, 0x06, 0xbe, 0xfa, 0x3f, 0xf6, 0xf6, 0xf6,
    0x22, 0x52, 0x07, 0xfe, 0xfe, 0xfe, 0xf1, 0xac, 0x96, 0x7c, 0x00, 0xbc, 0x8c, 0xfc, 0x80, 0xac, 0x9c, 0x7c, 0x00, 0xbc, 0x45, 0xfa, 0x0f, 0xad, 0xa7, 0x9c, 0xff, 0x06, 0x32, 0xdb, 0xf8, 0x50, 0xa6, 0xf8, 0x00, 0xaf, 0x87, 0x32, 0xe2, 0x27, 0x4a,
    0xbd, 0x9e, 0xfa, 0x07, 0xae, 0x8e, 0x32, 0xa1, 0x9d, 0xf6, 0xbd, 0x8f, 0x76, 0xaf, 0x2e, 0x30, 0x95, 0x9d, 0x56, 0x16, 0x8f, 0x56, 0x16, 0x30, 0x88, 0xec, 0xec, 0xf8, 0x50, 0xa6, 0xf8, 0x00, 0xa7, 0x8d, 0x32, 0xdb, 0x06, 0xf2, 0x2d, 0x32, 0xbb,
    0x91, 0xa7, 0x46, 0xf3, 0x5c, 0x02, 0xfb, 0x07, 0x32, 0xce, 0x1c, 0x06, 0xf2, 0x32, 0xca, 0x91, 0xa7, 0x06, 0xf3, 0x5c, 0x2c, 0x16, 0x8c, 0xfc, 0x08, 0xac, 0x9c, 0x7c, 0x00, 0xbc, 0xff, 0x06, 0x3a, 0xb1, 0xf8, 0x7f, 0xa6, 0x87, 0x56, 0x12, 0xd4,
    0x8d, 0xa7, 0x87, 0x32, 0xa9, 0x2a, 0x27, 0x30, 0xe4, 0x96, 0xbf, 0xaf, 0x4f, 0xbb, 0x0f, 0xab, 0xf8, 0x05, 0xbf, 0xf8, 0xc8, 0xaf, 0xf8, 0xff, 0x5f, 0x1f, 0x8f, 0x3a, 0xf8, 0xd4, 0x22, 0x06, 0x52, 0x64, 0xd4, 0x45, 0xa3, 0x98, 0x56, 0xd4, 0x93,
    0xbc, 0xf8, 0xcb, 0xac, 0xdc, 0x3a, 0x0f, 0xdc, 0x30, 0xf7, 0x06, 0xb8, 0xd4, 0x06, 0xa8, 0xd4, 0x64, 0x0a, 0x01, 0xe6, 0x8a, 0xf4, 0xaa, 0x3b, 0x28, 0x9a, 0xfc, 0x01, 0xba, 0xd4, 0x91, 0xba, 0x06, 0xfa, 0x0f, 0xaa, 0x0a, 0xaa, 0xd4, 0xff, 0xe6,
    0x06, 0xbf, 0x93, 0xbe, 0xf8, 0x1b, 0xae, 0x2a, 0x1a, 0xf8, 0x00, 0x5a, 0x0e, 0xf5, 0x3b, 0x4b, 0x56, 0x0a, 0xfc, 0x01, 0x5a, 0x30, 0x40, 0x4e, 0xf6, 0x3b, 0x3c, 0x9f, 0x56, 0x2a, 0x2a, 0xd4, 0xff, 0x22, 0x86, 0x52, 0xf8, 0x70, 0xa7, 0x07, 0x5a,
    0x87, 0xf3, 0x17, 0x1a, 0x3a, 0x5b, 0x12, 0xd4, 0x22, 0x86, 0x52, 0xf8, 0x70, 0xa7, 0x0a, 0x57, 0x87, 0xf3, 0x17, 0x1a, 0x3a, 0x6b, 0x12, 0xd4, 0x15, 0x85, 0x22, 0x73, 0x95, 0x52, 0x25, 0x45, 0xa5, 0x86, 0xb5, 0xd4, 0x45, 0xfa, 0x0f, 0x3a, 0x89,
    0x07, 0x56, 0xd4, 0xaf, 0x22, 0xf8, 0xd3, 0x73, 0x8f, 0xf9, 0xf0, 0x52, 0xe6, 0x07, 0xd2, 0x56, 0xf8, 0x7f, 0xa6, 0xf8, 0x00, 0x7e, 0x56, 0xd4, 0x45, 0xe6, 0xf3, 0x3a, 0xa7, 0x3f, 0xa3, 0x15, 0x15, 0xd4, 0x45, 0xe6, 0xf3, 0x3a, 0xa5, 0xd4, 0x45,
    0x07, 0x30, 0xa9, 0x45, 0x07, 0x30, 0x9f, 0xf8, 0x70, 0xa7, 0xe7, 0x45, 0xf4, 0xa5, 0x86, 0xfa, 0x0f, 0x3b, 0xc4, 0xfc, 0x01, 0xb5, 0xd4, 0x2d, 0x2d, 0x2d, 0x8d, 0xd3, 0x96, 0xbf, 0xbe, 0xf8, 0x4c, 0xaf, 0xf8, 0x48, 0xae, 0xf8, 0x10, 0xad, 0xf8,
    0xf7, 0xbd, 0x5e, 0xee, 0x62, 0x2e, 0xef, 0x6a, 0xfe, 0x3b, 0xc9, 0xfe, 0x3b, 0xc8, 0xfe, 0x3b, 0xc7, 0xfe, 0x3b, 0xc6, 0x2d, 0x2d, 0x2d, 0x2d, 0x9d, 0xf6, 0xbd, 0x33, 0xda, 0x30, 0xc9, 0x3f, 0xa3, 0x32, 0x12, 0xa8, 0x2d, 0x8d, 0x56, 0xd4
};



Eti660::Eti660(EmulatorHost& host, Properties& properties, IEmulationCore* other)
    : Chip8RealCoreBase(host)
    , _impl(new Private(host, *this, properties))
{
    //options.optTraceLog = true;
    _execChip8 = false; //_impl->_options.interpreter != VC8I_NONE;
    //if(_impl->_options.interpreter == VC8I_NONE)
        _isHybridChipMode = false;
    std::memcpy(_impl->_rom.data(), _eti660_c8_monitor, sizeof(_eti660_c8_monitor));
    if(_impl->_ram.size() > 4096) {
        _impl->_rom[0x10] = (_impl->_ram.size() >> 8) - 1;
    }
    _impl->_cpu.setInputHandler([this](uint8_t port) {
        if(port == 1)
            _impl->_video.enableDisplay();
        return 0;
    });
    _impl->_cpu.setOutputHandler([this](uint8_t port, uint8_t val) {
        switch (port) {
        case 1:
            _impl->_video.disableDisplay();
            break;
        case 2:
            _impl->_keyLatch = val & 0xf;
            break;
        case 3:
            _impl->_frequencyLatch = val ? val : 0x80;
            break;
        case 4:
            _impl->_mapRam = true;
            break;
        case 5:
            if(_impl->_video.getType() == Cdp186x::eVP590)
                _impl->_video.incrementBackground();
            break;
        default:
            break;
        }
    });
    _impl->_cpu.setNEFInputHandler([this](uint8_t idx) {
       switch(idx) {
           case 0: { // EF1 is set from four machine cycles before the video line to four before the end
               return _impl->_video.getNEFX();
           }
           case 2: {
               return _impl->_host.isKeyDown(_impl->_keyLatch);
           }
           default:
               return true;
       }
    });
    //reset();
    auto prev = dynamic_cast<IChip8Emulator*>(other);
    if(prev && false) {
        std::memcpy(_impl->_ram.data() + 0x200, prev->memory() + 0x200, std::min(_impl->_ram.size() - 0x200 - 0x170, static_cast<size_t>(prev->memSize())));
        for(size_t i = 0; i < 16; ++i) {
            _state.v[i] = prev->getV(i);
        }
        _state.i = prev->getI();
        _state.pc = prev->getPC();
        _state.sp = prev->getSP();
        _state.dt = prev->delayTimer();
        _state.st = prev->soundTimer();
        std::memcpy(_state.s.data(), prev->stack().content.data(), stackSize() * sizeof(uint16_t));
        forceState();
    }
#if 0 //ndef PLATFORM_WEB
    {
        static bool first = true;
        if(first) {
            std::ofstream os("chip8tpd.bin", std::ios::binary);
            os.write((char*)_chip8tdp_cvip, sizeof(_chip8tdp_cvip));
            first = false;
        }
    }
#endif
}

Eti660::~Eti660() = default;

void Eti660::handleReset()
{
    if(_impl->_options.traceLog)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.cycles(), {_frames, frameCycle()}, fmt::format("--- RESET ---", _impl->_cpu.cycles(), frameCycle()).c_str());
    if(_impl->_properties[PROP_CLEAN_RAM].getBool()) {
        std::fill(_impl->_ram.begin(), _impl->_ram.end(), 0);
    }
    else {
        if(_impl->_powerOn) {
            ghc::RandomLCG rnd(42);
            std::ranges::generate(_impl->_ram, rnd);
        }
    }
    _impl->_powerOn = false;
    std::memset(_impl->_colorRam.data(), 0, _impl->_colorRam.size());
    if(_isHybridChipMode) {
        /*
        std::memcpy(_impl->_ram.data(), _chip8_cvip, sizeof(_chip8_cvip));
        _impl->_startAddress = _impl->_options.startAddress;
        _impl->_fetchEntry = 0x1B;
        if (_impl->_options.interpreter != VC8I_CHIP8) {
            auto size = patchRAM(_impl->_options.interpreter, _impl->_ram.data(), _impl->_ram.size());
            _impl->_properties[PROP_INTERPRETER].setSelectedIndex(_impl->_options.interpreter);
            _impl->_properties[PROP_INTERPRETER].setAdditionalInfo(fmt::format("(sha1: {})", calculateSha1Hex(_impl->_ram.data(), size).substr(0, 8)));
            //_impl->_properties[PROP_INTERPRETER_SHA1] = calculateSha1Hex(_impl->_ram.data(), size).substr(0,8);
        }
        */
    }
    else {
        _impl->_startAddress = 0;
        _impl->_fetchEntry = 0;
        //_impl->_properties[PROP_INTERPRETER].setSelectedIndex(VIPChip8Interpreter::VC8I_NONE);
        //_impl->_properties[PROP_INTERPRETER].setAdditionalInfo("No CHIP-8 interpreter used");
    }
    _impl->_screen.setAll(0);
    _impl->_video.reset();
    _impl->_cpu.reset();
    _cycles = 0;
    _frames = 0;
    _impl->_nextFrame = 0;
    _impl->_lastOpcode = 0;
    _impl->_currentOpcode = 0;
    _impl->_initialChip8SP = 0;
    _impl->_frequencyLatch = 0x80;
    _impl->_keyLatch = 0;
    _impl->_mapRam = false;
    _impl->_wavePhase = 0;
    _cpuState = eNORMAL;
    _errorMessage.clear();
    if (_isHybridChipMode) {
        setExecMode(eRUNNING);
        while (_impl->_cpu.execMode() == eRUNNING && (!executeCdp1802() || getPC() != _impl->_startAddress))
            ;  // fast-forward to fetch/decode loop
    }
    else {
        setExecMode(ePAUSED);
        /*
        while (_impl->_cpu.execMode() == eRUNNING && !executeCdp1802())
            if(_impl->_cpu.getR(_impl->_cpu.getP()) == 0)
                break;  // fast-forward to fetch/decode loop
                */
    }
    setExecMode(_impl->_host.isHeadless() ? eRUNNING : ePAUSED);
    if(_impl->_options.traceLog)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.cycles(), {_frames, frameCycle()}, fmt::format("End of reset: {}/{}", _impl->_cpu.cycles(), frameCycle()).c_str());
}

bool Eti660::updateProperties(Properties& props, Property& changed)
{
    if(fuzzyAnyOf(changed.getName(), {"TraceLog", "InstructionsPerFrame", "FrameRate"})) {
        _impl->_options = Eti660Options::fromProperties(props);
        return false;
    }
    return true;
}

std::string Eti660::name() const
{
    return "Chip-8-RVIP";
}

unsigned Eti660::stackSize() const
{
    return 16;
}

GenericCpu::StackContent Eti660::stack() const
{
    /*switch(_impl->_options.interpreter) {
        case VC8I_CHIP8:
        case VC8I_CHIP8RB:
        case VC8I_CHIP8E:
        case VC8I_CHIP8X:
            return {2, eBIG, eDOWNWARDS, std::span(_impl->_ram.data() + _impl->_options.ramSize - 0x15f, 46)};
        case VC8I_CHIP10:
            return {2, eBIG, eDOWNWARDS, std::span(_impl->_ram.data() + _impl->_options.ramSize - 0x45f, 46)};
        case VC8I_CHIP8FPD:
        case VC8I_CHIP8XFPD:
            return {2, eBIG, eDOWNWARDS, std::span(_impl->_ram.data() + _impl->_options.ramSize - 0x45f, 46)};
        case VC8I_CHIP8TPD:
        case VC8I_CHIP8XTPD:
            return {2, eBIG, eDOWNWARDS, std::span(_impl->_ram.data() + _impl->_options.ramSize - 0x25f, 46)};
        default:
            return {};
    }*/
    return {};
}

size_t Eti660::numberOfExecutionUnits() const
{
    return 1; //_impl->_options.interpreter == VC8I_NONE ? 1 : 2;
}

GenericCpu* Eti660::executionUnit(size_t index)
{
    if(index >= numberOfExecutionUnits())
        return nullptr;
    if(true/*_impl->_options.interpreter == VC8I_NONE*/) {
        return &_impl->_cpu;
    }
    return index == 0 ? static_cast<GenericCpu*>(this) : static_cast<GenericCpu*>(&_impl->_cpu);
}

void Eti660::setFocussedExecutionUnit(GenericCpu* unit)
{
    _execChip8 = false; //_impl->_options.interpreter != VC8I_NONE && dynamic_cast<IChip8Emulator*>(unit);
}

GenericCpu* Eti660::focussedExecutionUnit()
{
    return _execChip8 ? static_cast<GenericCpu*>(this) : static_cast<GenericCpu*>(&_impl->_cpu);
}

uint32_t Eti660::defaultLoadAddress() const
{
    return _impl->_options.startAddress;
}

bool Eti660::loadData(std::span<const uint8_t> data, std::optional<uint32_t> loadAddress)
{
    auto offset = loadAddress ? *loadAddress : _impl->_options.startAddress;
    if(offset < _impl->_options.ramSize) {
        auto size = std::min(_impl->_options.ramSize - offset, data.size());
        std::memcpy(_impl->_ram.data() + offset, data.data(), size);
        return true;
    }
    return false;
}

emu::GenericCpu::ExecMode Eti660::execMode() const
{
    auto backendMode = _impl->_cpu.execMode();
    if(backendMode == ePAUSED || _execMode == ePAUSED)
        return ePAUSED;
    if(backendMode == eRUNNING)
        return _execMode;
    return backendMode;
}

void Eti660::setExecMode(ExecMode mode)
{
    if(_execChip8) {
        if(mode == ePAUSED) {
            if(_execMode != ePAUSED)
                _backendStopped = false;
            GenericCpu::setExecMode(ePAUSED);
            getBackendCpu().setExecMode(ePAUSED);
        }
        else {
            GenericCpu::setExecMode(mode);
            getBackendCpu().setExecMode(eRUNNING);
        }
    }
    else {
        if(mode == ePAUSED) {
            GenericCpu::setExecMode(ePAUSED);
            getBackendCpu().setExecMode(ePAUSED);
        }
        else {
            GenericCpu::setExecMode(eRUNNING);
            getBackendCpu().setExecMode(mode);
        }
    }
}

Properties& Eti660::getProperties()
{
    return _impl->_properties;
}

void Eti660::fetchState()
{
    _state.cycles = _cycles;
    _state.frameCycle = frameCycle();
    if(!_impl->_initialChip8SP)
        _impl->_initialChip8SP = _impl->_cpu.getR(2);
    auto base = _impl->_initialChip8SP & 0xFF00;
    if(base + 0x100 <= _impl->_memorySize)
        std::memcpy(_state.v.data(), &_impl->_ram[base + 0xF0], 16);
    else
        _impl->_cpu.setExecMode(GenericCpu::ePAUSED), _cpuState = eERROR, _errorMessage = "BASE ADDRESS OUT OF RAM";
    _state.i = _impl->_cpu.getR(0xA);
    _state.pc = _impl->_cpu.getR(5);
    _state.sp = ((_impl->_initialChip8SP - _impl->_cpu.getR(2)) >> 1) & 0xffff;
    _state.dt = _impl->_cpu.getR(8) >> 8;
    _state.st = _impl->_cpu.getR(8) & 0xff;
    if(_impl->_initialChip8SP < _impl->_memorySize && _impl->_initialChip8SP > stackSize() * 2) {
        for (int i = 0; i < stackSize() && i < _state.sp; ++i) {
            _state.s[i] = (_impl->_ram[_impl->_initialChip8SP - i * 2 - 2] << 8) | _impl->_ram[_impl->_initialChip8SP - i * 2 - 1];
        }
    }
    else
        _impl->_cpu.setExecMode(GenericCpu::ePAUSED), _cpuState = eERROR, _errorMessage = "BASE ADDRESS OUT OF RAM";
}

void Eti660::forceState()
{
    _state.cycles = _cycles;
    _state.frameCycle = frameCycle();
    if(!_impl->_initialChip8SP)
        _impl->_initialChip8SP = _impl->_cpu.getR(2);
    auto base = _impl->_initialChip8SP & 0xFF00;
    std::memcpy(&_impl->_ram[base + 0xF0], _state.v.data(), 16);
    _impl->_cpu.setR(0xA, (uint16_t)_state.i);
    _impl->_cpu.setR(0x5, (uint16_t)_state.pc);
    _impl->_cpu.setR(0x8, (uint16_t)(_state.dt << 8 | _state.st));
    _impl->_cpu.setR(0x2, (uint16_t)(_impl->_initialChip8SP - _state.sp * 2));
    for(int i = 0; i < stackSize() && i < _state.sp; ++i) {
        _impl->_ram[_impl->_initialChip8SP - i*2 - 2] = _state.s[i] >> 8;
        _impl->_ram[_impl->_initialChip8SP - i*2 - 1] = _state.s[i] & 0xFF;
    }
}

int64_t Eti660::machineCycles() const
{
    return _impl->_cpu.cycles() >> 3;
}

int Eti660::frameRate() const
{
    return std::lround(_impl->_options.clockFrequency / 8.0 / _impl->_video.cyclesPerFrame());
}

bool Eti660::executeCdp1802()
{
    static int lastFC = 0;
    static int endlessLoops = 0;
    auto [fc,vsync] = _impl->_video.executeStep();
    if(vsync)
        _host.vblank();
    if(_impl->_options.traceLog  && _impl->_cpu.getCpuState() != Cdp1802::eIDLE)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.cycles(), {_frames, fc}, fmt::format("{:24} ; {}", _impl->_cpu.disassembleInstructionWithBytes(-1, nullptr), _impl->_cpu.dumpStateLine()).c_str());
    if(_isHybridChipMode && _impl->_cpu.PC() == _impl->_fetchEntry) {
        _cycles++;
        //std::cout << fmt::format("{:06d}:{:04x}", _impl->_cpu.getCycles()>>3, opcode()) << std::endl;
        _impl->_currentOpcode = opcode();
        if(_impl->_options.traceLog)
            Logger::log(Logger::eCHIP8, _cycles, {_frames, fc}, fmt::format("CHIP8: {:30} ; {}", disassembleInstructionWithBytes(-1, nullptr), dumpStateLine()).c_str());
    }
    _impl->_cpu.executeInstruction();
    if(_isHybridChipMode && _impl->_cpu.PC() == _impl->_fetchEntry) {
        _impl->_lastOpcode = _impl->_currentOpcode;
#ifdef DIFFERENTIATE_CYCLES
        static int64_t lastCycles{}, lastIdle{}, lastIrq{};
#endif
        fetchState();
#ifdef DIFFERENTIATE_CYCLES
        if((_impl->_lastOpcode & 0xF000) == 0xD000) {
            static int64_t lastDrawCycle{};
            int64_t machineCycles = _impl->_cpu.getCycles() - lastCycles;
            int64_t idleTime = _impl->_cpu.getIdleCycles() - lastIdle;
            int64_t irqTime = _impl->_cpu.getIrqCycles() - lastIrq;
            int64_t nonCode = idleTime + irqTime;
            int64_t betweenDraws = (_impl->_cpu.getCycles() - lastDrawCycle) >> 3;
            int fetchTime = (_impl->_lastOpcode&0xF000)?68:40;
            std::cout << fmt::format("{:04x},{},{},{},{},{},{},{},{},{},{}", _impl->_lastOpcode, _state.v[(_impl->_lastOpcode&0xF00)>>8], _state.v[(_impl->_lastOpcode&0xF0)>>4], _impl->_lastOpcode&0xF,
                                     fetchTime, ((machineCycles - nonCode)>>3) - fetchTime,
                                     (machineCycles - nonCode)>>3, idleTime>>3, irqTime>>3, machineCycles>>3, betweenDraws) << std::endl;
            lastDrawCycle = _impl->_cpu.getCycles();
        }
        lastCycles = _impl->_cpu.getCycles();
        lastIdle = _impl->_cpu.getIdleCycles();
        lastIrq = _impl->_cpu.getIrqCycles();
#endif
        if(_impl->_cpu.execMode() == ePAUSED) {
            setExecMode(ePAUSED);
            _backendStopped = true;
        }
        else if (_execMode == eSTEP || (_execMode == eSTEPOVER && getSP() <= _stepOverSP)) {
            setExecMode(ePAUSED);
        }
        auto nextOp = opcode();
        bool newFrame = lastFC > fc;
        lastFC = fc;
        if(newFrame) {
            _host.updateScreen();
            if ((nextOp & 0xF000) == 0x1000 && (opcode() & 0xFFF) == getPC()) {
                if (++endlessLoops > 2) {
                    setExecMode(ePAUSED);
                    endlessLoops = 0;
                }
            }
            else {
                endlessLoops = 0;
            }
        }
        if(tryTriggerBreakpoint(getPC())) {
            setExecMode(ePAUSED);
            _breakpointTriggered = true;
        }
        return true;
    }
    else if(_impl->_cpu.execMode() == ePAUSED || _impl->_cpu.getCpuState() == Cdp1802::eERROR) {
        setExecMode(ePAUSED);
        _backendStopped = true;
    }
    if(!_isHybridChipMode)
        _cycles++;
    return false;
}

int Eti660::executeInstruction()
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        setExecMode(ePAUSED);
        return 0;
    }
    //std::clog << "CHIP8: " << dumpStateLine() << std::endl;
    const auto start = _impl->_cpu.cycles();
    while(!executeCdp1802() && _execMode != ePAUSED && _impl->_cpu.cycles() - start < _impl->_video.cyclesPerFrame()*14);
    return static_cast<int>(_impl->_cpu.cycles() - start);
}

void Eti660::executeInstructions(int numInstructions)
{
    for(int i = 0; i < numInstructions; ++i) {
        executeInstruction();
    }
}

//---------------------------------------------------------------------------
// For easier handling we shift the line/cycle counting to the start of the
// interrupt (if display is enabled)

inline int Eti660::frameCycle() const
{
    return _impl->_video.frameCycle(_impl->_cpu.cycles()); // _impl->_irqStart ? ((_impl->_cpu.getCycles() >> 3) - _impl->_irqStart) : 0;
}

inline int Eti660::videoLine() const
{
    return _impl->_video.videoLine(_impl->_cpu.cycles()); // (frameCycle() + (78*14)) % 3668) / 14;
}

void Eti660::executeFrame()
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        setExecMode(ePAUSED);
        return;
    }
    auto nextFrame = _impl->_video.nextFrame(_impl->_cpu.cycles());
    while(_execMode != ePAUSED && _impl->_cpu.cycles() < nextFrame) {
        executeCdp1802();
    }
}

int64_t Eti660::executeFor(int64_t microseconds)
{
    if(_execMode != ePAUSED) {
        auto cpuTime = _impl->_cpu.time();
        auto endTime = cpuTime + Time::fromMicroseconds(microseconds);
        while(_execMode != GenericCpu::ePAUSED && _impl->_cpu.time() < endTime) {
            executeInstruction();
        }
        return _impl->_cpu.time().difference_us(endTime);
    }
    return 0;
}

bool Eti660::isDisplayEnabled() const
{
    return _impl->_video.isDisplayEnabled();
}

uint8_t* Eti660::memory()
{
    return _impl->_ram.data();
}

int Eti660::memSize() const
{
    return _impl->_memorySize;
}

int64_t Eti660::frames() const
{
    return _impl->_video.frames();
}

void Eti660::renderAudio(int16_t* samples, size_t frames, int sampleFrequency)
{
    if(_impl->_cpu.getQ()) {
        auto audioFrequency = _impl->_options.audioType == ETI_AT_CDP1864 ? 27535.0f / ((unsigned)_impl->_frequencyLatch + 1) : 1400.0f;
        const float step = audioFrequency / sampleFrequency;
        for (int i = 0; i < frames; ++i) {
            *samples++ = (_impl->_wavePhase > 0.5f) ? 16384 : -16384;
            _impl->_wavePhase = std::fmod(_impl->_wavePhase + step, 1.0f);
        }
    }
    else {
        // Default is silence
        _impl->_wavePhase = 0;
        IEmulationCore::renderAudio(samples, frames, sampleFrequency);
    }
}

uint16_t Eti660::getCurrentScreenWidth() const
{
    return 64;
}

uint16_t Eti660::getCurrentScreenHeight() const
{
    return 192;
}

uint16_t Eti660::getMaxScreenWidth() const
{
    return 64;
}

uint16_t Eti660::getMaxScreenHeight() const
{
    return 192;
}

const VideoType* Eti660::getScreen() const
{
    return &_impl->_video.getScreen();
}

void Eti660::setPalette(const Palette& palette)
{
    _impl->_video.setPalette(palette);
}

GenericCpu& Eti660::getBackendCpu()
{
    return _impl->_cpu;
}

uint8_t Eti660::readByte(uint16_t addr) const
{
    if(addr < _impl->_rom.size()) {
        return _impl->_rom[addr];
    }
    addr -= _impl->_rom.size();
    if(addr < _impl->_memorySize) {
        return _impl->_ram[addr];
    }
    return 255;
}

uint8_t Eti660::readByteDMA(uint16_t addr) const
{
    return Eti660::readByte(addr);
}

uint8_t Eti660::readMemoryByte(uint32_t addr) const
{
    return readByteDMA(addr);
}

void Eti660::writeByte(uint16_t addr, uint8_t val)
{
    if(addr > _impl->_rom.size()) addr -= _impl->_rom.size();
    if(addr < _impl->_memorySize) {
        _impl->_ram[addr] = val;
    }
}

} // emu