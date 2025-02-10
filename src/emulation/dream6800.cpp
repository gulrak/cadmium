//---------------------------------------------------------------------------------------
// src/emulation/chip8dream.cpp
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

#include <chiplet/utility.hpp>
#include <emulation/dream6800.hpp>
#include <emulation/hardware/keymatrix.hpp>
#include <emulation/hardware/mc682x.hpp>
#include <emulation/logger.hpp>
#include <ghc/random.hpp>

#include <nlohmann/json.hpp>

//#define USE_CHIPOSLO

#include <atomic>
#include <thread>

namespace emu {

static const std::string PROP_CLASS = "DREAM6800";
static const std::string PROP_TRACE_LOG = "Trace Log";
static const std::string PROP_CPU = "CPU";
static const std::string PROP_CLOCK = "Clock Rate";
static const std::string PROP_RAM = "Memory";
static const std::string PROP_CLEAN_RAM = "Clean RAM";
static const std::string PROP_VIDEO = "Video";
static const std::string PROP_ROM_NAME = "ROM Name";
static const std::string PROP_START_ADDRESS = "Start Address";

enum class DreamVideoType { TTL };

struct Dream6800Options
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
        result[PROP_ROM_NAME].setSelectedText(romName);
        //result[PROP_INTERPRETER].setSelectedIndex(toType(interpreter));
        result[PROP_START_ADDRESS].setInt(startAddress);
        result.palette() = palette;
        return result;
    }
    static Dream6800Options fromProperties(const Properties& props)
    {
        Dream6800Options opts{};
        opts.traceLog = props[PROP_TRACE_LOG].getBool();
        opts.cpuType = props[PROP_CPU].getString();
        opts.clockFrequency = props[PROP_CLOCK].getInt();
        opts.ramSize = std::stoul(props[PROP_RAM].getSelectedText()); // !!!!
        opts.cleanRam = props[PROP_CLEAN_RAM].getBool();
        opts.videoType = static_cast<DreamVideoType>(props[PROP_VIDEO].getSelectedIndex());
        opts.romName = props[PROP_ROM_NAME].getSelectedText();
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
            prototype.registerProperty({PROP_CPU, "M6800"s, "CPU type (currently only M6800)"});
            prototype.registerProperty({PROP_CLOCK, Property::Integer{1000000, 100000, 500'000'000}, "Clock frequency, default is 1000000", eWritable});
            prototype.registerProperty({PROP_RAM, Property::Combo{"2048"s, "4096"s}, "Size of ram in bytes", eWritable});
            prototype.registerProperty({PROP_CLEAN_RAM, false, "Delete ram on startup", eWritable});
            prototype.registerProperty({PROP_VIDEO, Property::Combo{"TTL"}, "Video hardware, only TTL"});
            prototype.registerProperty({PROP_ROM_NAME, Property::Combo{"NONE", "CHIPOS"s, "CHIPOSLO"s}, "Rom image name, default c8-monitor", eWritable});
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
    DreamVideoType videoType;
    std::string romName;
    uint16_t startAddress;
    Palette palette;
};


struct Dream6800SetupInfo {
    const char* presetName;
    const char* description;
    const char* defaultExtensions;
    chip8::VariantSet supportedChip8Variants{chip8::Variant::NONE};
    Dream6800Options options;
};

// clang-format off
static Dream6800SetupInfo dreamPresets[] = {
    {
        "NONE",
        "Raw DREAM6800",
        ".bin;.hex;.ram;.raw",
        chip8::Variant::CHIP_8_D6800,
        { .cpuType = "M6800", .clockFrequency = 1000000, .ramSize = 2048, .cleanRam = false, .traceLog = false, .videoType = DreamVideoType::TTL, .romName = "CHIPOS", .startAddress = 0 }
    },
    {
        "CHIP-8",
        "CHIP-8 DREAM6800",
        ".bin;.hex;.ram;.raw",
        chip8::Variant::CHIP_8_D6800,
        { .cpuType = "M6800", .clockFrequency = 1000000, .ramSize = 4096, .cleanRam = false, .traceLog = false, .videoType = DreamVideoType::TTL, .romName = "CHIPOS", .startAddress = 0x200 }
    },
    {
        "CHIP-8-LOP",
        "CHIP-8 with logical operators on DREAM6800",
        ".bin;.hex;.ram;.raw",
        chip8::Variant::CHIP_8_D6800_LOP,
        { .cpuType = "M6800", .clockFrequency = 1000000, .ramSize = 4096, .cleanRam = false, .traceLog = false, .videoType = DreamVideoType::TTL, .romName = "CHIPOSLO", .startAddress = 0x200 }
    }
};
// clang-format on

struct Dream6800FactoryInfo final : public CoreRegistry::FactoryInfo<Dream6800, Dream6800SetupInfo, Dream6800Options>
{
    explicit Dream6800FactoryInfo(const char* description)
        : FactoryInfo(200, dreamPresets, description)
    {}
    std::string prefix() const override
    {
        return "DREAM";
    }
    VariantIndex variantIndex(const Properties& props) const override
    {
        auto idx = 0u;
        auto startAddress = props[PROP_START_ADDRESS].getInt();
        auto rom = props[PROP_ROM_NAME].getSelectedText();
        if(startAddress != 0x200)
            idx = 0;
        else
            idx = rom == "CHIPOS" ? 1 : 2;
        return {idx, dreamPresets[idx].options.asProperties() == props};
    }
};

static bool registeredDream6800 = CoreRegistry::registerFactory(PROP_CLASS, std::make_unique<Dream6800FactoryInfo>("Hardware emulation of a DREAM6800"));

class Dream6800::Private {
public:
    static constexpr uint16_t FETCH_LOOP_ENTRY = 0xC00C;
    explicit Private(EmulatorHost& host, M6800Bus<>& bus, Properties& props)
        : _host(host)
        , _options(Dream6800Options::fromProperties(props))
        , _cpu(bus, _options.clockFrequency)/*, _video(Cdp186x::eCDP1861, _cpu, options)*/
        , _properties(props)
    {
        using namespace std::string_literals;
        (void)registeredDream6800;
        _memorySize = std::stoul(_properties[PROP_RAM].getSelectedText());
        _ram.resize(_memorySize, 0);
    }
    EmulatorHost& _host;
    Dream6800Options _options;
    uint32_t _memorySize{4096};
    CadmiumM6800 _cpu;
    MC682x _pia;
    KeyMatrix<4,4> _keyMatrix;
    bool _ic20aNAnd{false};
    bool _soundEnabled{false};
    bool _lowFreq{true};
    int64_t _irqStart{0};
    int64_t _nextFrame{0};
    std::atomic<float> _wavePhase{0};
    std::vector<uint8_t> _ram{};
    std::array<uint8_t,1024> _rom{};
    VideoType _screen;
    Properties& _properties;
};

static const uint8_t dream6800Rom[] = {
    // Copyright (c) 1978, Michael J. Bauer
    0x8d, 0x77, 0xce, 0x02, 0x00, 0xdf, 0x22, 0xce, 0x00, 0x5f, 0xdf, 0x24, 0xde, 0x22, 0xee, 0x00, 0xdf, 0x28, 0xdf, 0x14, 0xbd, 0xc0, 0xd0, 0x96, 0x14, 0x84, 0x0f, 0x97, 0x14, 0x8d, 0x21, 0x97, 0x2e, 0xdf, 0x2a, 0x96, 0x29, 0x44, 0x44, 0x44,
    0x44, 0x8d, 0x15, 0x97, 0x2f, 0xce, 0xc0, 0x48, 0x96, 0x28, 0x84, 0xf0, 0x08, 0x08, 0x80, 0x10, 0x24, 0xfa, 0xee, 0x00, 0xad, 0x00, 0x20, 0xcc, 0xce, 0x00, 0x2f, 0x08, 0x4a, 0x2a, 0xfc, 0xa6, 0x00, 0x39, 0xc0, 0x6a, 0xc0, 0xa2, 0xc0, 0xac,
    0xc0, 0xba, 0xc0, 0xc1, 0xc0, 0xc8, 0xc0, 0xee, 0xc0, 0xf2, 0xc0, 0xfe, 0xc0, 0xcc, 0xc0, 0xa7, 0xc0, 0x97, 0xc0, 0xf8, 0xc2, 0x1f, 0xc0, 0xd7, 0xc1, 0x5f, 0xd6, 0x28, 0x26, 0x25, 0x96, 0x29, 0x81, 0xe0, 0x27, 0x05, 0x81, 0xee, 0x27, 0x0e,
    0x39, 0x4f, 0xce, 0x01, 0x00, 0xa7, 0x00, 0x08, 0x8c, 0x02, 0x00, 0x26, 0xf8, 0x39, 0x30, 0x9e, 0x24, 0x32, 0x97, 0x22, 0x32, 0x97, 0x23, 0x9f, 0x24, 0x35, 0x39, 0xde, 0x14, 0x6e, 0x00, 0x96, 0x30, 0x5f, 0x9b, 0x15, 0x97, 0x15, 0xd9, 0x14,
    0xd7, 0x14, 0xde, 0x14, 0xdf, 0x22, 0x39, 0xde, 0x14, 0xdf, 0x26, 0x39, 0x30, 0x9e, 0x24, 0x96, 0x23, 0x36, 0x96, 0x22, 0x36, 0x9f, 0x24, 0x35, 0x20, 0xe8, 0x96, 0x29, 0x91, 0x2e, 0x27, 0x10, 0x39, 0x96, 0x29, 0x91, 0x2e, 0x26, 0x09, 0x39,
    0x96, 0x2f, 0x20, 0xf0, 0x96, 0x2f, 0x20, 0xf3, 0xde, 0x22, 0x08, 0x08, 0xdf, 0x22, 0x39, 0xbd, 0xc2, 0x97, 0x7d, 0x00, 0x18, 0x27, 0x07, 0xc6, 0xa1, 0xd1, 0x29, 0x27, 0xeb, 0x39, 0xc6, 0x9e, 0xd1, 0x29, 0x27, 0xd0, 0x20, 0xd5, 0x96, 0x29,
    0x20, 0x3b, 0x96, 0x29, 0x9b, 0x2e, 0x20, 0x35, 0x8d, 0x38, 0x94, 0x29, 0x20, 0x2f, 0x96, 0x2e, 0xd6, 0x29, 0xc4, 0x0f, 0x26, 0x02, 0x96, 0x2f, 0x5a, 0x26, 0x02, 0x9a, 0x2f, 0x5a, 0x26, 0x02, 0x94, 0x2f, 0x5a, 0x5a, 0x26, 0x0a, 0x7f, 0x00,
    0x3f, 0x9b, 0x2f, 0x24, 0x03, 0x7c, 0x00, 0x3f, 0x5a, 0x26, 0x0a, 0x7f, 0x00, 0x3f, 0x90, 0x2f, 0x25, 0x03, 0x7c, 0x00, 0x3f, 0xde, 0x2a, 0xa7, 0x00, 0x39, 0x86, 0xc0, 0x97, 0x2c, 0x7c, 0x00, 0x2d, 0xde, 0x2c, 0x96, 0x0d, 0xab, 0x00, 0xa8,
    0xff, 0x97, 0x0d, 0x39, 0x07, 0xc1, 0x79, 0x0a, 0xc1, 0x7d, 0x15, 0xc1, 0x82, 0x18, 0xc1, 0x85, 0x1e, 0xc1, 0x89, 0x29, 0xc1, 0x93, 0x33, 0xc1, 0xde, 0x55, 0xc1, 0xfa, 0x65, 0xc2, 0x04, 0xce, 0xc1, 0x44, 0xc6, 0x09, 0xa6, 0x00, 0x91, 0x29,
    0x27, 0x09, 0x08, 0x08, 0x08, 0x5a, 0x26, 0xf4, 0x7e, 0xc3, 0x60, 0xee, 0x01, 0x96, 0x2e, 0x6e, 0x00, 0x96, 0x20, 0x20, 0xb0, 0xbd, 0xc2, 0xc4, 0x20, 0xab, 0x97, 0x20, 0x39, 0x16, 0x7e, 0xc2, 0xe1, 0x5f, 0x9b, 0x27, 0x97, 0x27, 0xd9, 0x26,
    0xd7, 0x26, 0x39, 0xce, 0xc1, 0xbc, 0x84, 0x0f, 0x08, 0x08, 0x4a, 0x2a, 0xfb, 0xee, 0x00, 0xdf, 0x1e, 0xce, 0x00, 0x08, 0xdf, 0x26, 0xc6, 0x05, 0x96, 0x1e, 0x84, 0xe0, 0xa7, 0x04, 0x09, 0x86, 0x03, 0x79, 0x00, 0x1f, 0x79, 0x00, 0x1e, 0x4a,
    0x26, 0xf7, 0x5a, 0x26, 0xeb, 0x39, 0xf6, 0xdf, 0x49, 0x25, 0xf3, 0x9f, 0xe7, 0x9f, 0x3e, 0xd9, 0xe7, 0xcf, 0xf7, 0xcf, 0x24, 0x9f, 0xf7, 0xdf, 0xe7, 0xdf, 0xb7, 0xdf, 0xd7, 0xdd, 0xf2, 0x4f, 0xd6, 0xdd, 0xf3, 0xcf, 0x93, 0x4f, 0xde, 0x26,
    0xc6, 0x64, 0x8d, 0x06, 0xc6, 0x0a, 0x8d, 0x02, 0xc6, 0x01, 0xd7, 0x0e, 0x5f, 0x91, 0x0e, 0x25, 0x05, 0x5c, 0x90, 0x0e, 0x20, 0xf7, 0xe7, 0x00, 0x08, 0x39, 0x0f, 0x9f, 0x12, 0x8e, 0x00, 0x2f, 0xde, 0x26, 0x20, 0x09, 0x0f, 0x9f, 0x12, 0x9e,
    0x26, 0x34, 0xce, 0x00, 0x30, 0xd6, 0x2b, 0xc4, 0x0f, 0x32, 0xa7, 0x00, 0x08, 0x7c, 0x00, 0x27, 0x5a, 0x2a, 0xf6, 0x9e, 0x12, 0x0e, 0x39, 0xd6, 0x29, 0x7f, 0x00, 0x3f, 0xde, 0x26, 0x86, 0x01, 0x97, 0x1c, 0xc4, 0x0f, 0x26, 0x02, 0xc6, 0x10,
    0x37, 0xdf, 0x14, 0xa6, 0x00, 0x97, 0x1e, 0x7f, 0x00, 0x1f, 0xd6, 0x2e, 0xc4, 0x07, 0x27, 0x09, 0x74, 0x00, 0x1e, 0x76, 0x00, 0x1f, 0x5a, 0x26, 0xf5, 0xd6, 0x2e, 0x8d, 0x28, 0x96, 0x1e, 0x8d, 0x15, 0xd6, 0x2e, 0xcb, 0x08, 0x8d, 0x1e, 0x96,
    0x1f, 0x8d, 0x0b, 0x7c, 0x00, 0x2f, 0xde, 0x14, 0x08, 0x33, 0x5a, 0x26, 0xcb, 0x39, 0x16, 0xe8, 0x00, 0xaa, 0x00, 0xe7, 0x00, 0x11, 0x27, 0x04, 0x86, 0x01, 0x97, 0x3f, 0x39, 0x96, 0x2f, 0x84, 0x1f, 0x48, 0x48, 0x48, 0xc4, 0x3f, 0x54, 0x54,
    0x54, 0x1b, 0x97, 0x1d, 0xde, 0x1c, 0x39, 0xc6, 0xf0, 0xce, 0x80, 0x10, 0x6f, 0x01, 0xe7, 0x00, 0xc6, 0x06, 0xe7, 0x01, 0x6f, 0x00, 0x39, 0x8d, 0xee, 0x7f, 0x00, 0x18, 0x8d, 0x55, 0xe6, 0x00, 0x8d, 0x15, 0x97, 0x17, 0xc6, 0x0f, 0x8d, 0xe1,
    0xe6, 0x00, 0x54, 0x54, 0x54, 0x54, 0x8d, 0x07, 0x48, 0x48, 0x9b, 0x17, 0x97, 0x17, 0x39, 0xc1, 0x0f, 0x26, 0x02, 0xd7, 0x18, 0x86, 0xff, 0x4c, 0x54, 0x25, 0xfc, 0x39, 0xdf, 0x12, 0x8d, 0xbf, 0xa6, 0x01, 0x2b, 0x07, 0x48, 0x2a, 0xf9, 0x6d,
    0x00, 0x20, 0x07, 0x8d, 0xc2, 0x7d, 0x00, 0x18, 0x26, 0xec, 0x8d, 0x03, 0xde, 0x12, 0x39, 0xc6, 0x04, 0xd7, 0x21, 0xc6, 0x41, 0xf7, 0x80, 0x12, 0x7d, 0x00, 0x21, 0x26, 0xfb, 0xc6, 0x01, 0xf7, 0x80, 0x12, 0x39, 0x8d, 0x00, 0x37, 0xc6, 0xc8,
    0x5a, 0x01, 0x26, 0xfc, 0x33, 0x39, 0xce, 0x80, 0x12, 0xc6, 0x3b, 0xe7, 0x01, 0xc6, 0x7f, 0xe7, 0x00, 0xa7, 0x01, 0xc6, 0x01, 0xe7, 0x00, 0x39, 0x8d, 0x13, 0xa6, 0x00, 0x2b, 0xfc, 0x8d, 0xdd, 0xc6, 0x09, 0x0d, 0x69, 0x00, 0x46, 0x8d, 0xd3,
    0x5a, 0x26, 0xf7, 0x20, 0x17, 0xdf, 0x12, 0xce, 0x80, 0x12, 0x39, 0x8d, 0xf8, 0x36, 0x6a, 0x00, 0xc6, 0x0a, 0x8d, 0xbf, 0xa7, 0x00, 0x0d, 0x46, 0x5a, 0x26, 0xf7, 0x32, 0xde, 0x12, 0x39, 0x20, 0x83, 0x86, 0x37, 0x8d, 0xb9, 0xde, 0x02, 0x39,
    0x8d, 0xf7, 0xa6, 0x00, 0x8d, 0xdd, 0x08, 0x9c, 0x04, 0x26, 0xf7, 0x20, 0x0b, 0x8d, 0xea, 0x8d, 0xb7, 0xa7, 0x00, 0x08, 0x9c, 0x04, 0x26, 0xf7, 0x8e, 0x00, 0x7f, 0xce, 0xc3, 0xe9, 0xdf, 0x00, 0x86, 0x3f, 0x8d, 0x92, 0x8d, 0x43, 0x0e, 0x8d,
    0xce, 0x4d, 0x2a, 0x10, 0x8d, 0xc9, 0x84, 0x03, 0x27, 0x23, 0x4a, 0x27, 0xd8, 0x4a, 0x27, 0xc8, 0xde, 0x06, 0x6e, 0x00, 0x8d, 0x0c, 0x97, 0x06, 0x8d, 0x06, 0x97, 0x07, 0x8d, 0x23, 0x20, 0xdf, 0x8d, 0xad, 0x48, 0x48, 0x48, 0x48, 0x97, 0x0f,
    0x8d, 0xa5, 0x9b, 0x0f, 0x39, 0x8d, 0x12, 0xde, 0x06, 0x8d, 0x25, 0x8d, 0x9a, 0x4d, 0x2b, 0x04, 0x8d, 0xe8, 0xa7, 0x00, 0x08, 0xdf, 0x06, 0x20, 0xec, 0x86, 0x10, 0x8d, 0x2b, 0xce, 0x01, 0xc8, 0x86, 0xff, 0xbd, 0xc0, 0x7d, 0xce, 0x00, 0x06,
    0x8d, 0x06, 0x08, 0x8d, 0x03, 0x8d, 0x15, 0x39, 0xa6, 0x00, 0x36, 0x44, 0x44, 0x44, 0x44, 0x8d, 0x01, 0x32, 0xdf, 0x12, 0xbd, 0xc1, 0x93, 0xc6, 0x05, 0xbd, 0xc2, 0x24, 0x86, 0x04, 0x9b, 0x2e, 0x97, 0x2e, 0x86, 0x1a, 0x97, 0x2f, 0xde, 0x12,
    0x39, 0x7a, 0x00, 0x20, 0x7a, 0x00, 0x21, 0x7d, 0x80, 0x12, 0x3b, 0xde, 0x00, 0x6e, 0x00, 0x00, 0xc3, 0xf3, 0x00, 0x80, 0x00, 0x83, 0xc3, 0x60
};

static const uint8_t dream6800ChipOslo[] = {
    /*
     * MIT License
     * Copyright (c) 1978, Michael J. Bauer
     * Copyright (c) 2020, Tobias V. Langhoff
     *
     * Permission is hereby granted, free of charge, to any person obtaining a copy
     * of this software and associated documentation files (the "Software"), to deal
     * in the Software without restriction, including without limitation the rights
     * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
     * copies of the Software, and to permit persons to whom the Software is
     * furnished to do so, subject to the following conditions:
     *
     * The above copyright notice and this permission notice shall be included in all
     * copies or substantial portions of the Software.
     *
     * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
     * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
     * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
     * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
     * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
     * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
     * SOFTWARE.
     */
    0x8d, 0x77, 0xce, 0x02, 0x00, 0xdf, 0x22, 0xce, 0x00, 0x5f, 0xdf, 0x24, 0xde, 0x22, 0xee, 0x00, 0xdf, 0x28, 0xdf, 0x14, 0xbd, 0xc0, 0xc7, 0xd6, 0x14, 0xc4, 0x0f, 0xd7, 0x14, 0x8d, 0x24, 0xd7, 0x2e, 0xd7, 0x0a, 0xdf, 0x2a, 0xd6, 0x29, 0x17,
    0x54, 0x54, 0x54, 0x54, 0x8d, 0x15, 0xd7, 0x2f, 0xce, 0xc0, 0x4b, 0xd6, 0x28, 0xc4, 0xf0, 0x08, 0x08, 0xc0, 0x10, 0x24, 0xfa, 0xee, 0x00, 0xad, 0x00, 0x20, 0xc9, 0xce, 0x00, 0x2f, 0x08, 0x5a, 0x2a, 0xfc, 0xe6, 0x00, 0x39, 0xc0, 0x6d, 0xc0,
    0xa2, 0xc0, 0xac, 0xc0, 0xba, 0xc0, 0xe1, 0xc0, 0xbf, 0xc1, 0x22, 0xc0, 0xe6, 0xc0, 0xf0, 0xc0, 0xc3, 0xc0, 0xa7, 0xc0, 0x97, 0xc0, 0xea, 0xc2, 0x1f, 0xc0, 0xce, 0xc1, 0x5f, 0xd6, 0x28, 0x26, 0x22, 0x81, 0xee, 0x27, 0x11, 0x81, 0xe0, 0x26,
    0x0c, 0x4f, 0xce, 0x01, 0x00, 0xa7, 0x00, 0x08, 0x8c, 0x02, 0x00, 0x26, 0xf8, 0x39, 0x30, 0x9e, 0x24, 0x32, 0x97, 0x22, 0x32, 0x97, 0x23, 0x9f, 0x24, 0x35, 0x39, 0xde, 0x14, 0x6e, 0x00, 0x96, 0x30, 0x5f, 0x9b, 0x15, 0x97, 0x15, 0xd9, 0x14,
    0xd7, 0x14, 0xde, 0x14, 0xdf, 0x22, 0x39, 0xde, 0x14, 0xdf, 0x26, 0x39, 0x30, 0x9e, 0x24, 0x96, 0x23, 0x36, 0x96, 0x22, 0x36, 0x9f, 0x24, 0x35, 0x20, 0xe8, 0x91, 0x2e, 0x27, 0x09, 0x39, 0x96, 0x2f, 0x20, 0xf7, 0x96, 0x2f, 0x20, 0x1a, 0xde,
    0x22, 0x08, 0x08, 0xdf, 0x22, 0x39, 0xbd, 0xc2, 0x97, 0x7d, 0x00, 0x18, 0x27, 0x07, 0xc6, 0xa1, 0xd1, 0x29, 0x27, 0xeb, 0x39, 0x81, 0x9e, 0x27, 0xd9, 0x91, 0x2e, 0x26, 0xe2, 0x39, 0x9b, 0x2e, 0x20, 0x38, 0x8d, 0x46, 0x94, 0x29, 0x20, 0x32,
    0x16, 0x96, 0x2f, 0xc4, 0x0f, 0x27, 0x2b, 0xce, 0x0a, 0x39, 0xc1, 0x05, 0x26, 0x05, 0x96, 0x2e, 0xce, 0x2f, 0x7e, 0xc1, 0x07, 0x26, 0x03, 0xce, 0x0a, 0x7e, 0xdf, 0x41, 0xce, 0xc1, 0x27, 0xdf, 0x43, 0x08, 0x5a, 0x26, 0xfc, 0xe6, 0x03, 0xd7,
    0x40, 0x7f, 0x00, 0x3f, 0xbd, 0x00, 0x40, 0x79, 0x00, 0x3f, 0xde, 0x2a, 0xa7, 0x00, 0x39, 0x59, 0x5c, 0x56, 0x39, 0x9a, 0x94, 0x98, 0x9b, 0x90, 0x44, 0x90, 0x86, 0xc0, 0x97, 0x47, 0x7c, 0x00, 0x48, 0xde, 0x47, 0x96, 0x0d, 0xab, 0x00, 0xa8,
    0xff, 0x97, 0x0d, 0x39, 0x07, 0xc1, 0x79, 0x0a, 0xc1, 0x7d, 0x15, 0xc1, 0x82, 0x18, 0xc1, 0x85, 0x1e, 0xc1, 0x89, 0x29, 0xc1, 0x93, 0x33, 0xc1, 0xde, 0x55, 0xc1, 0xfa, 0x65, 0xc2, 0x04, 0xce, 0xc1, 0x44, 0xc6, 0x09, 0xa6, 0x00, 0x91, 0x29,
    0x27, 0x09, 0x08, 0x08, 0x08, 0x5a, 0x26, 0xf4, 0x7e, 0xc3, 0x60, 0xee, 0x01, 0x96, 0x2e, 0x6e, 0x00, 0x96, 0x20, 0x20, 0xa5, 0xbd, 0xc2, 0xc4, 0x20, 0xa0, 0x97, 0x20, 0x39, 0x16, 0x7e, 0xc2, 0xe1, 0x5f, 0x9b, 0x27, 0x97, 0x27, 0xd9, 0x26,
    0xd7, 0x26, 0x39, 0xce, 0xc1, 0xbc, 0x84, 0x0f, 0x08, 0x08, 0x4a, 0x2a, 0xfb, 0xee, 0x00, 0xdf, 0x1e, 0xce, 0x00, 0x50, 0xdf, 0x26, 0xc6, 0x05, 0x96, 0x1e, 0x84, 0xe0, 0xa7, 0x04, 0x09, 0x86, 0x03, 0x79, 0x00, 0x1f, 0x79, 0x00, 0x1e, 0x4a,
    0x26, 0xf7, 0x5a, 0x26, 0xeb, 0x39, 0xf6, 0xdf, 0x49, 0x25, 0xf3, 0x9f, 0xe7, 0x9f, 0x3e, 0xd9, 0xe7, 0xcf, 0xf7, 0xcf, 0x24, 0x9f, 0xf7, 0xdf, 0xe7, 0xdf, 0xb7, 0xdf, 0xd7, 0xdd, 0xf2, 0x4f, 0xd6, 0xdd, 0xf3, 0xcf, 0x93, 0x4f, 0xde, 0x26,
    0xc6, 0x64, 0x8d, 0x06, 0xc6, 0x0a, 0x8d, 0x02, 0xc6, 0x01, 0xd7, 0x0e, 0x5f, 0x91, 0x0e, 0x25, 0x05, 0x5c, 0x90, 0x0e, 0x20, 0xf7, 0xe7, 0x00, 0x08, 0x39, 0x0f, 0x9f, 0x12, 0x8e, 0x00, 0x2f, 0xde, 0x26, 0x20, 0x09, 0x0f, 0x9f, 0x12, 0x9e,
    0x26, 0x34, 0xce, 0x00, 0x30, 0xd6, 0x2b, 0xc4, 0x0f, 0x32, 0xa7, 0x00, 0x08, 0x7c, 0x00, 0x27, 0x5a, 0x2a, 0xf6, 0x9e, 0x12, 0x0e, 0x39, 0x16, 0x7f, 0x00, 0x3f, 0x01, 0xde, 0x26, 0x86, 0x01, 0x97, 0x1c, 0xc4, 0x0f, 0x26, 0x02, 0xc6, 0x10,
    0x37, 0xdf, 0x14, 0xa6, 0x00, 0x97, 0x1e, 0x7f, 0x00, 0x1f, 0xd6, 0x2e, 0xc4, 0x07, 0x27, 0x09, 0x74, 0x00, 0x1e, 0x76, 0x00, 0x1f, 0x5a, 0x26, 0xf5, 0xd6, 0x2e, 0x8d, 0x28, 0x96, 0x1e, 0x8d, 0x15, 0xd6, 0x2e, 0xcb, 0x08, 0x8d, 0x1e, 0x96,
    0x1f, 0x8d, 0x0b, 0x7c, 0x00, 0x2f, 0xde, 0x14, 0x08, 0x33, 0x5a, 0x26, 0xcb, 0x39, 0x16, 0xe8, 0x00, 0xaa, 0x00, 0xe7, 0x00, 0x11, 0x27, 0x04, 0x86, 0x01, 0x97, 0x3f, 0x39, 0x96, 0x2f, 0x84, 0x1f, 0x48, 0x48, 0x48, 0xc4, 0x3f, 0x54, 0x54,
    0x54, 0x1b, 0x97, 0x1d, 0xde, 0x1c, 0x39, 0xc6, 0xf0, 0xce, 0x80, 0x10, 0x6f, 0x01, 0xe7, 0x00, 0xc6, 0x06, 0xe7, 0x01, 0x6f, 0x00, 0x39, 0x8d, 0xee, 0x7f, 0x00, 0x18, 0x8d, 0x55, 0xe6, 0x00, 0x8d, 0x15, 0x97, 0x17, 0xc6, 0x0f, 0x8d, 0xe1,
    0xe6, 0x00, 0x54, 0x54, 0x54, 0x54, 0x8d, 0x07, 0x48, 0x48, 0x9b, 0x17, 0x97, 0x17, 0x39, 0xc1, 0x0f, 0x26, 0x02, 0xd7, 0x18, 0x86, 0xff, 0x4c, 0x54, 0x25, 0xfc, 0x39, 0xdf, 0x12, 0x8d, 0xbf, 0xa6, 0x01, 0x2b, 0x07, 0x48, 0x2a, 0xf9, 0x6d,
    0x00, 0x20, 0x07, 0x8d, 0xc2, 0x7d, 0x00, 0x18, 0x26, 0xec, 0x8d, 0x03, 0xde, 0x12, 0x39, 0xc6, 0x04, 0xd7, 0x21, 0xc6, 0x41, 0xf7, 0x80, 0x12, 0x7d, 0x00, 0x21, 0x26, 0xfb, 0xc6, 0x01, 0xf7, 0x80, 0x12, 0x39, 0x8d, 0x00, 0x37, 0xc6, 0xc8,
    0x5a, 0x01, 0x26, 0xfc, 0x33, 0x39, 0xce, 0x80, 0x12, 0xc6, 0x3b, 0xe7, 0x01, 0xc6, 0x7f, 0xe7, 0x00, 0xa7, 0x01, 0xc6, 0x01, 0xe7, 0x00, 0x39, 0x8d, 0x13, 0xa6, 0x00, 0x2b, 0xfc, 0x8d, 0xdd, 0xc6, 0x09, 0x0d, 0x69, 0x00, 0x46, 0x8d, 0xd3,
    0x5a, 0x26, 0xf7, 0x20, 0x17, 0xdf, 0x12, 0xce, 0x80, 0x12, 0x39, 0x8d, 0xf8, 0x36, 0x6a, 0x00, 0xc6, 0x0a, 0x8d, 0xbf, 0xa7, 0x00, 0x0d, 0x46, 0x5a, 0x26, 0xf7, 0x32, 0xde, 0x12, 0x39, 0x20, 0x83, 0x86, 0x37, 0x8d, 0xb9, 0xde, 0x02, 0x39,
    0x8d, 0xf7, 0xa6, 0x00, 0x8d, 0xdd, 0x08, 0x9c, 0x04, 0x26, 0xf7, 0x20, 0x0b, 0x8d, 0xea, 0x8d, 0xb7, 0xa7, 0x00, 0x08, 0x9c, 0x04, 0x26, 0xf7, 0x8e, 0x00, 0x7f, 0xce, 0xc3, 0xe9, 0xdf, 0x00, 0x86, 0x3f, 0x8d, 0x92, 0x8d, 0x43, 0x0e, 0x8d,
    0xce, 0x4d, 0x2a, 0x10, 0x8d, 0xc9, 0x84, 0x03, 0x27, 0x23, 0x4a, 0x27, 0xd8, 0x4a, 0x27, 0xc8, 0xde, 0x06, 0x6e, 0x00, 0x8d, 0x0c, 0x97, 0x06, 0x8d, 0x06, 0x97, 0x07, 0x8d, 0x23, 0x20, 0xdf, 0x8d, 0xad, 0x48, 0x48, 0x48, 0x48, 0x97, 0x0f,
    0x8d, 0xa5, 0x9b, 0x0f, 0x39, 0x8d, 0x12, 0xde, 0x06, 0x8d, 0x25, 0x8d, 0x9a, 0x4d, 0x2b, 0x04, 0x8d, 0xe8, 0xa7, 0x00, 0x08, 0xdf, 0x06, 0x20, 0xec, 0x86, 0x10, 0x8d, 0x2b, 0xce, 0x01, 0xc8, 0x86, 0xff, 0xbd, 0xc0, 0x7d, 0xce, 0x00, 0x06,
    0x8d, 0x06, 0x08, 0x8d, 0x03, 0x8d, 0x15, 0x39, 0xa6, 0x00, 0x36, 0x44, 0x44, 0x44, 0x44, 0x8d, 0x01, 0x32, 0xdf, 0x12, 0xbd, 0xc1, 0x93, 0xc6, 0x05, 0xbd, 0xc2, 0x24, 0x86, 0x04, 0x9b, 0x2e, 0x97, 0x2e, 0x86, 0x1a, 0x97, 0x2f, 0xde, 0x12,
    0x39, 0x7a, 0x00, 0x20, 0x7a, 0x00, 0x21, 0x7d, 0x80, 0x12, 0x3b, 0xde, 0x00, 0x6e, 0x00, 0x00, 0xc3, 0xf3, 0x00, 0x80, 0x00, 0x83, 0xc3, 0x60
};

Dream6800::Dream6800(EmulatorHost& host, Properties& props, IChip8Emulator* other)
    : Chip8RealCoreBase(host)
    , _impl(new Private(host, *this, props))
{
    if(_impl->_options.romName == "CHIPOSLO") {
        std::memcpy(_impl->_rom.data(), dream6800ChipOslo, sizeof(dream6800ChipOslo));
        _impl->_properties[PROP_ROM_NAME].setAdditionalInfo(fmt::format("(sha1: {})", calculateSha1(dream6800ChipOslo, 512).to_hex().substr(0,8)));
    }
    else {
        std::memcpy(_impl->_rom.data(), dream6800Rom, sizeof(dream6800Rom));
        _impl->_properties[PROP_ROM_NAME].setAdditionalInfo(fmt::format("(sha1: {})", calculateSha1(dream6800Rom, 512).to_hex().substr(0,8)));
    }
    _impl->_pia.irqAOutputHandler = [this](bool level) {
        if(!level)
            _impl->_cpu.irq();
    };
    _impl->_pia.irqBOutputHandler = [this](bool level) {
        if(!level)
            _impl->_cpu.irq();
    };
    _impl->_pia.portAOutputHandler = [this](uint8_t data, uint8_t mask) {
        _impl->_keyMatrix.setCols(data & 0xF, mask & 0xF);
        _impl->_keyMatrix.setRows(data >> 4, mask >> 4);
    };
    _impl->_pia.portBOutputHandler = [this](uint8_t data, uint8_t mask) {
        if(mask & 0x40) {
            _impl->_soundEnabled = data & 0x40;
        }
        //if(mask & 0x01) {
        //    _impl->_lowFreq = data & 1;
        //}
    };
    _impl->_pia.portAInputHandler = [this](uint8_t mask) -> MC682x::InputWithConnection {
        if(mask & 0xF) {
            auto [value, conn] = _impl->_keyMatrix.getCols(mask & 0xF);
            return {uint8_t(value & mask), uint8_t(conn & mask)};
        }
        if(mask & 0xF0) {
            auto [value, conn] = _impl->_keyMatrix.getRows(mask >> 4);
            return {uint8_t((value<<4) & mask), uint8_t((conn<<4) & mask)};
        }
        return {0, 0};
    };
    _impl->_pia.pinCA1InputHandler = [this]() -> bool {
        auto [value, conn] = _impl->_keyMatrix.getCols(0xF);
        return 0xF == (((value & conn) | ~conn) & 0xF) ? false : true;
    };
    Dream6800::reset();
    if(other) {
        /*
        std::memcpy(_impl->_ram.data() + 0x200, other->memory() + 0x200, std::min(_impl->_ram.size() - 0x200, (size_t)other->memSize()));
        for(size_t i = 0; i < 16; ++i) {
            _state.v[i] = other->getV(i);
        }
        _state.i = other->getI();
        _state.pc = other->getPC();
        _state.sp = other->getSP();
        _state.dt = other->delayTimer();
        _state.st = other->soundTimer();
        std::memcpy(_state.s.data(), other->stackElements(), stackSize() * sizeof(uint16_t));
        forceState();
        */
    }
}

Dream6800::~Dream6800()
{

}

void Dream6800::reset()
{
    if(_impl->_options.traceLog)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.cycles(), {_frames, frameCycle()}, fmt::format("--- RESET ---", _impl->_cpu.cycles(), frameCycle()).c_str());
    if(_impl->_properties[PROP_CLEAN_RAM].getBool()) {
        std::fill(_impl->_ram.begin(), _impl->_ram.end(), 0);
    }
    else {
        ghc::RandomLCG rnd(42);
        std::generate(_impl->_ram.begin(), _impl->_ram.end(), rnd);
    }
    _impl->_screen.setAll(0);
    _impl->_cpu.reset();
    _impl->_ram[0x006] = 0xC0;
    _impl->_ram[0x007] = 0x00;
    setExecMode(eRUNNING);
    while(!executeM6800() && (_impl->_cpu.registerByName("SR").value & CadmiumM6800::I));
    flushScreen();
    M6800State state;
    _impl->_ram[0x026] = 0x00;
    _impl->_ram[0x027] = 0x00;
    std::memset(&_impl->_ram[0x30], 0, 16);
    _impl->_cpu.getState(state);
    state.pc = 0xC000;
    state.sp = 0x007f;
    _impl->_cpu.setState(state);
    _cycles = 0;
    _frames = 0;
    _impl->_nextFrame = 0;
    _cpuState = eNORMAL;
    while(!executeM6800() || getPC() != 0x200); // fast-forward to fetch/decode loop
    setExecMode(_impl->_host.isHeadless() ? eRUNNING : ePAUSED);
    if(_impl->_options.traceLog)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.cycles(), {_frames, frameCycle()}, fmt::format("End of reset: {}/{}", _impl->_cpu.cycles(), frameCycle()).c_str());
}

std::string Dream6800::name() const
{
    return "DREAM6800";
}

bool Dream6800::updateProperties(Properties& props, Property& changed)
{
    if(fuzzyAnyOf(changed.getName(), {"TraceLog", "InstructionsPerFrame", "FrameRate"})) {
        _impl->_options = Dream6800Options::fromProperties(props);
        return false;
    }
    return true;
}

Properties& Dream6800::getProperties()
{
    return _impl->_properties;
}

int64_t Dream6800::frames() const
{
    return _frames;
}

unsigned Dream6800::stackSize() const
{
    return 16;
}

GenericCpu::StackContent Dream6800::stack() const
{
    return {2, eNATIVE, eUPWARDS, std::span(reinterpret_cast<const uint8_t*>(_state.s.data()), reinterpret_cast<const uint8_t*>(_state.s.data() + _state.s.size()))};
}

size_t Dream6800::numberOfExecutionUnits() const
{
    return _impl->_options.romName == "CHIPOS" && _impl->_options.ramSize != 4096 ? 1 : 2;
}

GenericCpu* Dream6800::executionUnit(size_t index)
{
    if(index >= numberOfExecutionUnits())
        return nullptr;
    if(_impl->_options.romName == "CHIPOS" && _impl->_options.ramSize != 4096) {
        return &_impl->_cpu;
    }
    return index == 0 ? static_cast<GenericCpu*>(this) : static_cast<GenericCpu*>(&_impl->_cpu);
}

void Dream6800::setFocussedExecutionUnit(GenericCpu* unit)
{
    _execChip8 = !(_impl->_options.romName == "CHIPOS" && _impl->_options.ramSize != 4096) && dynamic_cast<IChip8Emulator*>(unit);
}

GenericCpu* Dream6800::focussedExecutionUnit()
{
    return _execChip8 ? static_cast<GenericCpu*>(this) : static_cast<GenericCpu*>(&_impl->_cpu);
}

uint32_t Dream6800::defaultLoadAddress() const
{
    return _impl->_options.startAddress;
}

bool Dream6800::loadData(std::span<const uint8_t> data, std::optional<uint32_t> loadAddress)
{
    auto offset = loadAddress ? *loadAddress : 0x200; //_impl->_options.startAddress;
    if(offset < _impl->_options.ramSize) {
        auto size = std::min(_impl->_options.ramSize - offset, data.size());
        std::memcpy(_impl->_ram.data() + offset, data.data(), size);
        return true;
    }
    return false;
}

emu::GenericCpu::ExecMode Dream6800::execMode() const
{
    auto backendMode = _impl->_cpu.execMode();
    if(backendMode == ePAUSED || _execMode == ePAUSED)
        return ePAUSED;
    if(backendMode == eRUNNING)
        return _execMode;
    return backendMode;
}

void Dream6800::setExecMode(ExecMode mode)
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

void Dream6800::fetchState()
{
    _state.cycles = _cycles;
    _state.frameCycle = frameCycle();
    std::memcpy(_state.v.data(), &_impl->_ram[0x30], 16);
    _state.i = (_impl->_ram[0x26]<<8) | _impl->_ram[0x27];
    _state.pc = (_impl->_ram[0x22]<<8) | _impl->_ram[0x23];
    _state.sp = (0x05F - ((_impl->_ram[0x24]<<8) | _impl->_ram[0x25])) >> 1;
    _state.dt = _impl->_ram[0x20];
    _state.st = _impl->_ram[0x21];
    for(int i = 0; i < stackSize() && i < _state.sp; ++i) {
        _state.s[i] = (_impl->_ram[0x05F - i*2 - 1] << 8) | _impl->_ram[0x05F - i*2];
    }
}

void Dream6800::forceState()
{
    _state.cycles = _cycles;
    _state.frameCycle = frameCycle();
    std::memcpy(&_impl->_ram[0x30], _state.v.data(), 16);
    _impl->_ram[0x26] = (_state.i >> 8); _impl->_ram[0x27] = _state.i & 0xFF;
    _impl->_ram[0x22] = (_state.pc >> 8); _impl->_ram[0x22] = _state.pc & 0xFF;
    auto sp = 0x5f - _state.sp * 2;
    _impl->_ram[0x24] = (sp >> 8); _impl->_ram[0x25] = sp & 0xFF;
    _impl->_ram[0x20] = _state.dt;
    _impl->_ram[0x21] = _state.st;
    for(int i = 0; i < stackSize() && i < _state.sp; ++i) {
        _impl->_ram[sp - i*2 - 1] = _state.s[i] >> 8;
        _impl->_ram[sp - i*2] = _state.s[i] & 0xFF;
    }
}

int64_t Dream6800::machineCycles() const
{
    return _impl->_cpu.cycles();
}

int Dream6800::executeVDG()
{
    static int lastFC = 312*64 + 1;
    auto fc = frameCycle();
    if(fc < lastFC) {
        flushScreen();
        // CPU is halted for 124*64 Cycles while video frame is generated
        _impl->_cpu.addCycles(128*64);
        ++_frames;
        // Trigger RTC/VSYNC on PIA (Will trigger IRQ on CPU)
        _impl->_pia.pinCB1(true);
        _impl->_pia.pinCB1(false);
        _impl->_keyMatrix.updateKeys(_host.getKeyStates());
        _host.vblank();
    }
    lastFC = fc;
    return fc;
}

void Dream6800::flushScreen()
{
    for(int y = 0; y < 32*4; ++y) {
        for (int i = 0; i < 8; ++i) {
            auto data = _impl->_ram[0x100 + (y>>2)*8 + i];
            for (int j = 0; j < 8; ++j) {
                _impl->_screen.setPixel(i * 8 + j, y, (data >> (7 - j)) & 1);
            }
        }
    }
    ++_frames;
}

bool Dream6800::executeM6800()
{
    static int lastFC = 0;
    auto fc = executeVDG();
    if(_impl->_options.traceLog  && _impl->_cpu.getCpuState() == CadmiumM6800::eNORMAL)
        Logger::log(Logger::eBACKEND_EMU, _impl->_cpu.cycles(), {_frames, fc}, fmt::format("{:28} ; {}", _impl->_cpu.disassembleInstructionWithBytes(-1, nullptr), _impl->_cpu.dumpRegisterState()).c_str());
    if(_impl->_cpu.getPC() == Private::FETCH_LOOP_ENTRY) {
        if(_impl->_options.traceLog)
            Logger::log(Logger::eCHIP8, _cycles, {_frames, fc}, fmt::format("CHIP8: {:30} ; {}", disassembleInstructionWithBytes(-1, nullptr), dumpStateLine()).c_str());
    }
    _impl->_cpu.executeInstruction();

    if(_impl->_cpu.getPC() == Private::FETCH_LOOP_ENTRY) {
        fetchState();
        _cycles++;
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
        if(newFrame && (nextOp & 0xF000) == 0x1000 && (opcode() & 0xFFF) == getPC()) {
            flushScreen();
            _host.updateScreen();
            setExecMode(ePAUSED);
        }
        if(hasBreakPoint(getPC())) {
            if(Dream6800::findBreakpoint(getPC())) {
                setExecMode(ePAUSED);
                _breakpointTriggered = true;
            }
        }
        return true;
    }
    else if(_impl->_cpu.execMode() == ePAUSED) {
        setExecMode(ePAUSED);
        _backendStopped = true;
    }
    return false;
}

int Dream6800::executeInstruction()
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        setExecMode(ePAUSED);
        return 0;
    }
    //std::clog << "CHIP8: " << dumpStateLine() << std::endl;
    auto start = _impl->_cpu.cycles();
    while(!executeM6800() && _execMode != ePAUSED && _impl->_cpu.cycles() - start < 19968*0x30);
    return static_cast<int>(_impl->_cpu.cycles() - start);
}

void Dream6800::executeInstructions(int numInstructions)
{
    for(int i = 0; i < numInstructions; ++i) {
        executeInstruction();
    }
}

//---------------------------------------------------------------------------
// For easier handling we shift the line/cycle counting to the start of the
// interrupt (if display is enabled)

inline int Dream6800::frameCycle() const
{
    return _impl->_cpu.cycles() % 19968;
}

inline cycles_t Dream6800::nextFrame() const
{
    return ((_impl->_cpu.cycles() + 19968) / 19968) * 19968;
}

void Dream6800::executeFrame()
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        setExecMode(ePAUSED);
        return;
    }

    auto nxtFrame = nextFrame();
    while(_execMode != ePAUSED && _impl->_cpu.cycles() < nxtFrame) {
        executeM6800();
    }
}

int64_t Dream6800::executeFor(int64_t microseconds)
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

bool Dream6800::isDisplayEnabled() const
{
    return true; //_impl->_video.isDisplayEnabled();
}

uint8_t* Dream6800::memory()
{
    return _impl->_ram.data();
}

int Dream6800::memSize() const
{
    return _impl->_memorySize;
}

uint8_t Dream6800::soundTimer() const
{
    return (_impl->_pia.portB() & 64) ? _state.st : 0;
}

/*float Dream6800::getAudioPhase() const
{
    return _impl->_wavePhase;
}

void Dream6800::setAudioPhase(float phase)
{
    _impl->_wavePhase = phase;
}*/

void Dream6800::renderAudio(int16_t* samples, size_t frames, int sampleFrequency)
{
    if(_impl->_soundEnabled) {
        const float step = (_impl->_lowFreq ? 1200.0f : 2400.0f) / sampleFrequency;
        for (int i = 0; i < frames; ++i) {
            *samples++ = (_impl->_wavePhase > 0.5f) ? 16384 : -16384;
            _impl->_wavePhase = std::fmod(_impl->_wavePhase + step, 1.0f);
        }
    }
    else {
        // Default is silence
        IEmulationCore::renderAudio(samples, frames, sampleFrequency);
    }
}

uint16_t Dream6800::getCurrentScreenWidth() const
{
    return 64;
}

uint16_t Dream6800::getCurrentScreenHeight() const
{
    return 128;
}

uint16_t Dream6800::getMaxScreenWidth() const
{
    return 64;
}

uint16_t Dream6800::getMaxScreenHeight() const
{
    return 128;
}

const VideoType* Dream6800::getScreen() const
{
    return &_impl->_screen;
}

void Dream6800::setPalette(const Palette& palette)
{
    _impl->_screen.setPalette(palette);
}

GenericCpu& Dream6800::getBackendCpu()
{
    return _impl->_cpu;
}

uint8_t Dream6800::readByte(uint16_t addr) const
{
    if(addr < _impl->_ram.size())
        return _impl->_ram[addr];
    if(addr >= 0x8010 && addr < 0x8020)
        return _impl->_pia.readByte(addr & 3);
    if(addr >= 0xC000)
        return _impl->_rom[addr & 0x3ff];
    _cpuState = eERROR;
    return 0;
}

uint8_t Dream6800::readDebugByte(uint16_t addr) const
{
    if(addr < _impl->_ram.size())
        return _impl->_ram[addr];
    if(addr >= 0x8010 && addr < 0x8020)
        return _impl->_pia.readByte(addr & 3);
    if(addr >= 0xC000)
        return _impl->_rom[addr & 0x3ff];
    return 0;
}

uint8_t Dream6800::readMemoryByte(uint32_t addr) const
{
    return Dream6800::readDebugByte(addr);
}

void Dream6800::writeByte(uint16_t addr, uint8_t val)
{
    if (addr < _impl->_ram.size())
        _impl->_ram[addr] = val;
    else if (addr >= 0x8010 && addr < 0x8020)
        _impl->_pia.writeByte(addr & 3, val);
    else {
        _cpuState = eERROR;
    }
}
const uint8_t* Dream6800::getRamPage(uint16_t addr, uint16_t pageSize) const
{
    return addr < _impl->_ram.size() ? &_impl->_ram[addr & ~(pageSize-1)] : nullptr;
}

}
