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


namespace emu {

static const std::string PROP_CLASS = "ETI660";
static const std::string PROP_TRACE_LOG = "traceLog";
static const std::string PROP_CPU = "cpu";
static const std::string PROP_CLOCK = "clockRate";
static const std::string PROP_RAM = "memory";
static const std::string PROP_CLEAN_RAM = "cleanRam";
static const std::string PROP_VIDEO = "video";
static const std::string PROP_AUDIO = "audio";
static const std::string PROP_KEYBOARD = "keyboard";
static const std::string PROP_ROM_NAME = "romName";
static const std::string PROP_START_ADDRESS = "startAddress";

enum ETIVideoType { ETI_VT_CDP1864 };
enum ETIAudioType { ETI_AT_CDP1864 };
enum ETIKeyboard { ETIK_HEX, ETI_TWO_ROW, ETI_VIP_HEX };

namespace {
uint8_t reverseBits(uint8_t value)
{
    value = ((value & 0x55) << 1) | ((value >> 1) & 0x55);
    value = ((value & 0x33) << 2) | ((value >> 2) & 0x33);
    return (value << 4) | (value >> 4);
}
}


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
            prototype.registerProperty({PROP_TRACE_LOG, "Trace Log", false, "Enable trace log", PropertyFlags::eWritable});
            prototype.registerProperty({PROP_CPU, "CPU", "CDP1802"s, "CPU type (currently only cdp1802)"});
            prototype.registerProperty({PROP_CLOCK, "Clock Rate", Property::Integer{1773448, 100000, 500'000'000}, "Clock frequency, default is 1773448", PropertyFlags::eWritable});
            prototype.registerProperty({PROP_RAM, "Memory", Property::Combo{"3072"s}, "Size of ram in bytes"});
            prototype.registerProperty({PROP_CLEAN_RAM, "Clean RAM", false, "Delete ram on startup", PropertyFlags::eWritable});
            prototype.registerProperty({PROP_VIDEO, "Video", Property::Combo{"CDP1864"}, "Video hardware, only cdp1864"});
            prototype.registerProperty({PROP_AUDIO, "Audio", Property::Combo{"CDP1864"}, "Audio hardware, only cdp1864"});
            prototype.registerProperty({PROP_KEYBOARD, "Keyboard", Property::Combo{"ETI660 Hex", "ETI660 2-ROW", "VIP Hex"}, "Keyboard type, default is ETI660 hex"});
            prototype.registerProperty(Property::separator());
            prototype.registerProperty({PROP_ROM_NAME, "ROM Name", "C8-MONITOR"s, "Rom image name, default c8-monitor"});
            //prototype.registerProperty({PROP_INTERPRETER, Property::Combo{"NONE", "CHIP8", "CHIP10", "CHIP8RB", "CHIP8TPD", "CHIP8FPD", "CHIP8X", "CHIP8XTPD", "CHIP8XFPD", "CHIP8E"}, "CHIP-8 interpreter variant"});
            prototype.registerProperty({PROP_START_ADDRESS, "Start Address", Property::Integer{0x600, 0, 4095}, "CHIP-8 entry address (0 boots the monitor)"});
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
        ".et660;.bin;.hex;.ram;.raw",
        chip8::Variant::NONE,
        { .cpuType = "CDP1802", .clockFrequency = 1773448, .ramSize = 3072, .cleanRam = false, .traceLog = false, .videoType = ETI_VT_CDP1864, .audioType = ETI_AT_CDP1864, .keyboard = ETI_VIP_HEX, .romName = "C8-MONITOR", .startAddress = 0}
    },
    {
        "CHIP-8",
        "CHIP-8 on the ETI-660 (64x48)",
        ".c8eti",
        chip8::Variant::CHIP_8_ETI660,
        { .cpuType = "CDP1802", .clockFrequency = 1773448, .ramSize = 3072, .cleanRam = false, .traceLog = false, .videoType = ETI_VT_CDP1864, .audioType = ETI_AT_CDP1864, .keyboard = ETI_VIP_HEX, .romName = "C8-MONITOR", .startAddress = 0x600 }
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
        auto idx = props[PROP_START_ADDRESS].getInt() ? 1u : 0u;
        return {idx, etiPresets[idx].options.asProperties() != props};
    }
};

static bool registeredEti660 = CoreRegistry::registerFactory(PROP_CLASS, std::make_unique<Eti660FactoryInfo>("Hardware emulation of an ETI660"));

class Eti660::Private {
public:
    explicit Private(EmulatorHost& host, Cdp1802Bus& bus, Properties& properties)
        : _host(host)
        , _properties(properties)
        , _options(Eti660Options::fromProperties(properties))
        , _cpu(bus, _options.clockFrequency)
        , _video(Cdp186x::eCDP1864, _cpu, false)
    {
        using namespace std::string_literals;
        (void)registeredEti660;
        _properties = _options.asProperties();
        _properties[PROP_ROM_NAME].setAdditionalInfo(fmt::format("(sha1: {})", calculateSha1(_eti660_c8_monitor).to_hex().substr(0,8)));
        _memorySize = sizeof(_eti660_c8_monitor) + _options.ramSize;
        _ram.resize(_memorySize, 0);
    }
    EmulatorHost& _host;
    Properties& _properties;
    Eti660Options _options;
    uint32_t _memorySize{4096};
    Cdp1802 _cpu;
    Cdp186x _video;
    mutable MC682x _pia;
    KeyMatrix<4,4> _keyMatrix;
    //int64_t _irqStart{0};
    int _lastFrameCycle{0};
    int _endlessLoops{0};
    uint8_t _frequencyLatch{0};
    uint16_t _lastOpcode{0};
    uint16_t _currentOpcode{0};
    uint16_t _initialChip8SP{0};
    uint16_t _startAddress{0};
    uint16_t _fetchEntry{0};
    bool _powerOn{true};
    float _wavePhase{0};
    std::vector<uint8_t> _ram{};
    std::array<uint8_t,192> _colorRam{};
    std::array<uint8_t,1024> _rom{};
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
    if (other) {
        if (auto otherVIP = dynamic_cast<Eti660*>(other)) {
            _impl->_cpu.swapBreakpoints(otherVIP->_impl->_cpu);
        }
        if (auto otherC8 = other->chip8Core()) {
            swapBreakpoints(*otherC8);
        }
    }
    _isHybridChipMode = _impl->_options.startAddress != 0;
    _execChip8 = _isHybridChipMode;
    setVariant(chip8::Variant::CHIP_8_ETI660);
    std::memcpy(_impl->_rom.data(), _eti660_c8_monitor, sizeof(_eti660_c8_monitor));
    std::copy(_impl->_rom.begin(), _impl->_rom.end(), _impl->_ram.begin());

    // The ETI PCB reverses D0..D7 at the PIA. Its register is selected
    // by the low address bits of R(X), rather than by the I/O port number.
    _impl->_pia.portAOutputHandler = [this](uint8_t data, uint8_t mask) {
        _impl->_keyMatrix.setRows(reverseBits(data) & 0xF, reverseBits(mask) & 0xF);
    };
    _impl->_pia.portAInputHandler = [this](uint8_t mask) -> MC682x::InputWithConnection {
        std::array<bool,16> keys{};
        for (uint8_t key = 0; key < keys.size(); ++key)
            keys[key] = _host.isKeyDown(key);
        _impl->_keyMatrix.updateKeys(keys);
        auto [value, connections] = _impl->_keyMatrix.getCols(0xF);
        return {uint8_t(reverseBits(value << 4) & mask), uint8_t(reverseBits(connections << 4) & mask)};
    };
    _impl->_pia.pinCA2OutputHandler = [this](bool level) {
        if (!level)
            _impl->_video.enableColor();
    };
    _impl->_cpu.setInputHandler([this](uint8_t port) -> uint8_t {
        switch (port) {
            case 1: _impl->_video.enableDisplay(); break;
            case 2: return reverseBits(_impl->_pia.readByte(_impl->_cpu.getR(_impl->_cpu.getX()) & 3));
            case 4: _impl->_video.disableDisplay(); break;
        }
        return 255;
    });
    _impl->_cpu.setOutputHandler([this](uint8_t port, uint8_t value) {
        switch (port) {
            case 1:
                _impl->_video.enableDisplay();
                _impl->_video.incrementBackground();
                break;
            case 2: {
                // OUT has already incremented R(X) when the handler runs.
                const auto index = (_impl->_cpu.getR(_impl->_cpu.getX()) - 1) & 3;
                const auto piaValue = reverseBits(value);
                _impl->_pia.writeByte(index, piaValue);
                // Driving CA2 low also enables colour when the pin was
                // previously an input and no level-change callback ran.
                if (index == 1 && (piaValue & MC682x::C2C2) && !_impl->_pia.pinCA2())
                    _impl->_video.enableColor();
                break;
            }
            case 3: {
                const auto address = (_impl->_cpu.getR(_impl->_cpu.getX()) - 1) & 0xFFF;
                if (address >= 0xC80) {
                    const auto offset = address - 0xC80;
                    const auto index = ((offset & 0x1F0) >> 1) | (offset & 7);
                    if (index < _impl->_colorRam.size())
                        _impl->_colorRam[index] = value & 7;
                }
                break;
            }
            case 4: _impl->_frequencyLatch = value; break;
        }
    });
    _impl->_cpu.setNEFInputHandler([this](uint8_t index) {
        if (index == 0)
            return _impl->_video.getNEFX();
        if (index == 3)
            // STEP pulls EF4 low; the monitor also uses it to end input.
            return !_host.isKeyDown(EmulatorHost::KEY_ETI_STEP);
        return false;
    });
    _impl->_cpu.setQChangeHandler([this](bool enabled) {
        if (!enabled)
            _impl->_frequencyLatch = 0x35;
    });

}

Eti660::~Eti660() = default;

void Eti660::handleReset(bool cold)
{
    if(_impl->_options.traceLog)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.cycles(), {_frames, frameCycle()}, fmt::format("--- RESET ---", _impl->_cpu.cycles(), frameCycle()).c_str());
    if (cold) {
        if (_impl->_options.cleanRam) {
            std::fill(_impl->_ram.begin() + _impl->_rom.size(), _impl->_ram.end(), 0);
        }
        else if (_impl->_powerOn) {
            ghc::RandomLCG rnd(ghc::RandomLCG::Type::eNormal, 42);
            std::generate(_impl->_ram.begin() + _impl->_rom.size(), _impl->_ram.end(), rnd);
        }
        std::fill(_impl->_colorRam.begin(), _impl->_colorRam.end(), 0);
    }
    _impl->_powerOn = false;
    _isHybridChipMode = _impl->_options.startAddress != 0;
    _execChip8 = _isHybridChipMode;
    _impl->_startAddress = _impl->_options.startAddress;
    _impl->_fetchEntry = 0x200;
    _impl->_pia.reset();
    _impl->_keyMatrix = {};
    _impl->_video.reset();
    _impl->_cpu.reset();
    _cycles = 0;
    _frames = 0;
    _impl->_lastFrameCycle = 0;
    _impl->_endlessLoops = 0;
    _state = {};
    _impl->_lastOpcode = 0;
    _impl->_currentOpcode = 0;
    _impl->_initialChip8SP = 0;
    _impl->_frequencyLatch = 0x35;
    _impl->_wavePhase = 0;
    _cpuState = eNORMAL;
    _errorMessage.clear();
    if (_isHybridChipMode) {
        setExecMode(eRUNNING);
        const auto setupLimit = _impl->_cpu.cycles() + 4 * 8 * _impl->_video.cyclesPerFrame();
        while (_impl->_cpu.PC() != _impl->_fetchEntry && _execMode != ePAUSED && _impl->_cpu.cycles() < setupLimit)
            executeCdp1802();
        if (_impl->_cpu.PC() == _impl->_fetchEntry) {
            // Run the monitor's clear-screen, display-enable and call sequence.
            // This clears display RAM through the ROM even when Clean RAM is off,
            // and leaves 0x438/0x439 available as PIA scratch space.
            _impl->_cpu.setR(5, 0x42);
            do {
                executeCdp1802();
            } while ((_impl->_cpu.PC() != _impl->_fetchEntry || (_impl->_cpu.getR(5) & 0xFFF) != 0x600) &&
                     _execMode != ePAUSED && _impl->_cpu.cycles() < setupLimit);
        }
        if (_impl->_cpu.PC() != _impl->_fetchEntry || (_impl->_cpu.getR(5) & 0xFFF) != 0x600) {
            _cpuState = eERROR;
            _errorMessage = "ETI INTERPRETER DIDN'T SETUP";
        }
        else {
            _impl->_initialChip8SP = _impl->_cpu.getR(2);
            _impl->_cpu.setR(5, _impl->_startAddress);
            _impl->_cpu.setR(8, 0);
            fetchState();
            _cycles = 0;
        }
    }
    setExecMode(inErrorState() || !_host.isHeadless() ? ePAUSED : eRUNNING);
    if(_impl->_options.traceLog)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.cycles(), {_frames, frameCycle()}, fmt::format("End of reset: {}/{}", _impl->_cpu.cycles(), frameCycle()).c_str());
}

bool Eti660::updateProperties(Properties& props, Property& changed)
{
    if(changed.getId() == PROP_TRACE_LOG) {
        _impl->_options = Eti660Options::fromProperties(props);
        return false;
    }
    return true;
}

std::string Eti660::name() const
{
    return "ETI660";
}

chip8::VariantSet Eti660::supportedChip8Variants() const
{
    return _isHybridChipMode ? chip8::Variant::CHIP_8_ETI660 : chip8::Variant::NONE;
}

unsigned Eti660::stackSize() const
{
    return 16;
}

GenericCpu::StackContent Eti660::stack() const
{
    if (!_isHybridChipMode || !_impl->_initialChip8SP)
        return {};
    return {2, eBIG, eDOWNWARDS, std::span(_impl->_ram.data() + _impl->_initialChip8SP - stackSize() * 2, stackSize() * 2)};

}

size_t Eti660::numberOfExecutionUnits() const
{
    return _isHybridChipMode ? 2 : 1;
}

GenericCpu* Eti660::executionUnit(size_t index)
{
    if(index >= numberOfExecutionUnits())
        return nullptr;
    if(!_isHybridChipMode) {
        return &_impl->_cpu;
    }
    return index == 0 ? static_cast<GenericCpu*>(this) : static_cast<GenericCpu*>(&_impl->_cpu);
}

void Eti660::setFocussedExecutionUnit(GenericCpu* unit)
{
    _execChip8 = _isHybridChipMode && unit == static_cast<GenericCpu*>(this);
}

GenericCpu* Eti660::focussedExecutionUnit()
{
    return _execChip8 ? static_cast<GenericCpu*>(this) : static_cast<GenericCpu*>(&_impl->_cpu);
}

uint32_t Eti660::defaultLoadAddress() const
{
    return _impl->_options.startAddress ? _impl->_options.startAddress : 0x600;
}

bool Eti660::loadData(std::span<const uint8_t> data, std::optional<uint32_t> loadAddress)
{
    auto offset = loadAddress.value_or(defaultLoadAddress());
    if (offset >= _impl->_rom.size() && offset < _impl->_memorySize && data.size() <= _impl->_memorySize - offset) {
        std::memcpy(_impl->_ram.data() + offset, data.data(), data.size());
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
        std::memcpy(_state.v.data(), &_impl->_ram[base + 0x70], 16);
    else
        _impl->_cpu.setExecMode(GenericCpu::ePAUSED), _cpuState = eERROR, _errorMessage = "BASE ADDRESS OUT OF RAM";
    _state.i = _impl->_cpu.getR(0xA) & 0xFFF;
    _state.pc = _impl->_cpu.getR(5) & 0xFFF;
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
    std::memcpy(&_impl->_ram[base + 0x70], _state.v.data(), 16);
    _impl->_cpu.setR(0xA, (uint16_t)_state.i);
    _impl->_cpu.setR(0x5, (uint16_t)_state.pc);
    _impl->_cpu.setR(0x8, (uint16_t)(_state.dt << 8 | _state.st));
    _impl->_cpu.setR(0x2, (uint16_t)(_impl->_initialChip8SP - _state.sp * 2));
    for(int i = 0; i < stackSize() && i < _state.sp; ++i) {
        _impl->_ram[_impl->_initialChip8SP - i*2 - 2] = _state.s[i] >> 8;
        _impl->_ram[_impl->_initialChip8SP - i*2 - 1] = _state.s[i] & 0xFF;
    }
}

bool Eti660::restoreState(const Chip8State& state)
{
    if (!state.isValid(stackSize()) || state.pc >= _impl->_memorySize || state.i >= _impl->_memorySize ||
        !_isHybridChipMode || _impl->_cpu.PC() != _impl->_fetchEntry || !_impl->_initialChip8SP)
        return false;
    _state = state;
    forceState();
    return true;
}

void Eti660::setRegister(size_t index, uint32_t value)
{
    auto state = _state;
    if (index < 16)
        state.v[index] = value;
    else {
        switch (index) {
            case 16: state.i = value; break;
            case 17: state.dt = value; break;
            case 18: state.st = value; break;
            case 19: state.pc = value; break;
            case 20: state.sp = value; break;
            default: return;
        }
    }
    restoreState(state);
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
    auto [fc,vsync] = _impl->_video.executeStep();
    if(vsync) {
        _frames = _impl->_video.frames();
        _host.vblank();
        _host.updateScreen();
    }
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
        bool newFrame = _impl->_lastFrameCycle > fc;
        _impl->_lastFrameCycle = fc;
        if(newFrame) {
            _host.updateScreen();
            if ((nextOp & 0xF000) == 0x1000 && (opcode() & 0xFFF) == getPC()) {
                if (++_impl->_endlessLoops > 2) {
                    setExecMode(ePAUSED);
                    _impl->_endlessLoops = 0;
                }
            }
            else {
                _impl->_endlessLoops = 0;
            }
        }
        if(tryTriggerBreakpoint(getPC())) {
            setExecMode(ePAUSED);
            _breakpointTriggered = true;
        }
        return true;
    }
    else if(_impl->_cpu.execMode() == ePAUSED || _impl->_cpu.getCpuState() == Cdp1802::eERROR) {
        if (_impl->_cpu.getCpuState() == Cdp1802::eERROR) {
            _cpuState = eERROR;
            _errorMessage = _impl->_cpu.errorMessage();
        }
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
        auto audioFrequency = _impl->_options.audioType == ETI_AT_CDP1864 ? _impl->_options.clockFrequency / 64.0f / (unsigned(_impl->_frequencyLatch) + 1) : 1400.0f;
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

const VideoRGBAType* Eti660::getScreenRGBA() const
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
    // The board decodes only A0..A11. The interpreter relies on these
    // aliases when using register offsets as the high byte of a jump.
    addr &= 0xFFF;
    return addr < _impl->_memorySize ? _impl->_ram[addr] : 255;
}

uint8_t Eti660::readByteDMA(uint16_t addr) const
{
    // Like the VIP colour board, Cdp186x requests the colour separately
    // from the bitmap byte through a DMA-only address window.
    if ((addr & 0xF000) == 0xD000) {
        const auto address = addr & 0xFFF;
        if (address >= 0x480 && address < 0x600) {
            const auto offset = address - 0x480;
            return _impl->_colorRam[((offset & 0x1F0) >> 1) | (offset & 7)];
        }
        return 7;
    }
    return readByte(addr);
}

uint8_t Eti660::readMemoryByte(uint32_t addr) const
{
    return addr < _impl->_memorySize ? _impl->_ram[addr] : 255;
}

void Eti660::writeByte(uint16_t addr, uint8_t value)
{
    addr &= 0xFFF;
    if (addr >= _impl->_rom.size() && addr < _impl->_memorySize)
        _impl->_ram[addr] = value;
}

} // emu
