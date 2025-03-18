//---------------------------------------------------------------------------------------
// src/emulation/chip8generic.cpp
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

#include <emulation/chip8generic.hpp>
#include <emulation/coreregistry.hpp>
#include <emulation/logger.hpp>
#include <ghc/random.hpp>

#include <iostream>
#include <nlohmann/json.hpp>

//#define ALIEN_INV8SION_BENCH

namespace emu
{

extern const uint8_t _chip8_cvip[0x200];

static const std::string PROP_CLASS = "CHIP-8 GENERIC";
static const std::string PROP_INSTRUCTIONS_PER_FRAME = "Instructions per frame";
static const std::string PROP_FRAME_RATE = "Frame rate";
static const std::string PROP_TRACE_LOG = "Trace Log";
static const std::string PROP_RAM = "Memory";
static const std::string PROP_CLEAN_RAM = "Clean RAM";
static const std::string PROP_BEHAVIOR_BASE = "Behavior Base";
static const std::string PROP_START_ADDRESS = "Start Address";
static const std::string PROP_Q_JUST_SHIFT_VX = "8xy6/8xyE just shift VX";
static const std::string PROP_Q_DONT_RESET_VF = "8xy1/8xy2/8xy3 don't reset VF";
static const std::string PROP_Q_LOAD_STORE_INC_I_BY_X_PLUS_ONE = "Fx55/Fx65 increment I by X + 1";
static const std::string PROP_Q_LOAD_STORE_INC_I_BY_X = "Fx55/Fx65 increment I by X";
static const std::string PROP_Q_WRAP_SPRITES = "Wrap sprite pixels";
static const std::string PROP_Q_INSTANT_DXYN = "Dxyn doesn't wait for vsync";
static const std::string PROP_Q_LORES_DXY0_IS_8X16 = "Lores Dxy0 draws 8 pixel width";
static const std::string PROP_Q_LORES_DXY0_IS_16X16 = "Lores Dxy0 draws 16 pixel width";
static const std::string PROP_Q_SC11_COLLISION = "Dxyn uses SCHIP1.1 collision";
static const std::string PROP_Q_SC_LORES_DRAWING = "HP SuperChip lores drawing";
static const std::string PROP_Q_HALF_PIXEL_SCROLL = "Half pixel scrolling";
static const std::string PROP_Q_MODE_CHANGE_CLEAR = "Mode change clear";
static const std::string PROP_Q_JUMP0_BXNN = "Bxnn/jump0 uses Vx";
static const std::string PROP_Q_ALLOW_HIRES = "128x64 hires support";
static const std::string PROP_Q_ONLY_HIRES = "Only 128x64 mode";
static const std::string PROP_Q_ALLOW_COLORS = "Multicolor support";
static const std::string PROP_Q_CYCLIC_STACK = "Cyclic stack";
static const std::string PROP_Q_HAS_16BIT_ADDR = "Has 16 bit addresses";
static const std::string PROP_Q_XO_CHIP_SOUND = "XO-CHIP sound engine";
static const std::string PROP_Q_EXTENDED_VBLANK = "Extended CHIP-8 wait emulation";
static const std::string PROP_Q_PAL_VIDEO = "PAL video format";
static const std::string PROP_SCREEN_ROTATION = "Screen rotation";
static const std::string PROP_TOUCH_INPUT_MODE = "Touch input mode";
static const std::string PROP_FONT_5PX = "Font 5px";
static const std::string PROP_FONT_10PX = "Font 10px";

Properties Chip8GenericOptions::asProperties() const
{
    auto result = registeredPrototype();
    result[PROP_BEHAVIOR_BASE].setSelectedIndex(behaviorBase);
    result[PROP_TRACE_LOG].setBool(traceLog);
    result[PROP_INSTRUCTIONS_PER_FRAME].setInt(instructionsPerFrame);
    result[PROP_FRAME_RATE].setInt(frameRate);
    result[PROP_RAM].setSelectedText(std::to_string(ramSize)); // !!!!
    result[PROP_CLEAN_RAM].setBool(cleanRam);
    result[PROP_START_ADDRESS].setInt(startAddress);
    result[PROP_Q_JUST_SHIFT_VX].setBool(optJustShiftVx);
    result[PROP_Q_DONT_RESET_VF].setBool(optDontResetVf);
    result[PROP_Q_LOAD_STORE_INC_I_BY_X_PLUS_ONE].setBool(!optLoadStoreDontIncI && !optLoadStoreIncIByX);
    result[PROP_Q_LOAD_STORE_INC_I_BY_X].setBool(optLoadStoreIncIByX);
    result[PROP_Q_WRAP_SPRITES].setBool(optWrapSprites);
    result[PROP_Q_INSTANT_DXYN].setBool(optInstantDxyn);
    result[PROP_Q_LORES_DXY0_IS_8X16].setBool(optLoresDxy0Is8x16);
    result[PROP_Q_LORES_DXY0_IS_16X16].setBool(optLoresDxy0Is16x16);
    result[PROP_Q_SC11_COLLISION].setBool(optSC11Collision);
    result[PROP_Q_SC_LORES_DRAWING].setBool(optSCLoresDrawing);
    result[PROP_Q_HALF_PIXEL_SCROLL].setBool(optHalfPixelScroll);
    result[PROP_Q_MODE_CHANGE_CLEAR].setBool(optModeChangeClear);
    result[PROP_Q_JUMP0_BXNN].setBool(optJump0Bxnn);
    result[PROP_Q_ALLOW_HIRES].setBool(optAllowHires);
    result[PROP_Q_ONLY_HIRES].setBool(optOnlyHires);
    result[PROP_Q_ALLOW_COLORS].setBool(optAllowColors);
    result[PROP_Q_CYCLIC_STACK].setBool(optCyclicStack);
    result[PROP_Q_HAS_16BIT_ADDR].setBool(optHas16BitAddr);
    result[PROP_Q_XO_CHIP_SOUND].setBool(optXOChipSound);
    result[PROP_Q_EXTENDED_VBLANK].setBool(optExtendedVBlank);
    result[PROP_Q_PAL_VIDEO].setBool(optPalVideo);
    result[PROP_SCREEN_ROTATION].setSelectedIndex(int(rotation));
    result[PROP_TOUCH_INPUT_MODE].setSelectedIndex(int(touchInputMode));
    result[PROP_FONT_5PX].setSelectedIndex(int(fontStyle5));
    result[PROP_FONT_10PX].setSelectedIndex(int(fontStyle10));
    result.palette() = palette;
    return result;
}

Chip8GenericOptions Chip8GenericOptions::fromProperties(const Properties& props)
{
    Chip8GenericOptions opts{};
    opts.behaviorBase = static_cast<SupportedPreset>(props[PROP_BEHAVIOR_BASE].getSelectedIndex());
    opts.traceLog = props[PROP_TRACE_LOG].getBool();
    opts.instructionsPerFrame = props[PROP_INSTRUCTIONS_PER_FRAME].getInt();
    opts.frameRate = props[PROP_FRAME_RATE].getInt();
    opts.ramSize = std::stoul(props[PROP_RAM].getSelectedText()); // !!!!
    opts.cleanRam = props[PROP_CLEAN_RAM].getBool();
    opts.startAddress = props[PROP_START_ADDRESS].getInt();
    opts.optJustShiftVx = props[PROP_Q_JUST_SHIFT_VX].getBool();
    opts.optDontResetVf = props[PROP_Q_DONT_RESET_VF].getBool();
    opts.optLoadStoreDontIncI = !props[PROP_Q_LOAD_STORE_INC_I_BY_X_PLUS_ONE].getBool() && !props[PROP_Q_LOAD_STORE_INC_I_BY_X].getBool();
    opts.optLoadStoreIncIByX = props[PROP_Q_LOAD_STORE_INC_I_BY_X].getBool();
    opts.optWrapSprites = props[PROP_Q_WRAP_SPRITES].getBool();
    opts.optInstantDxyn = props[PROP_Q_INSTANT_DXYN].getBool();
    opts.optLoresDxy0Is8x16 = props[PROP_Q_LORES_DXY0_IS_8X16].getBool();
    opts.optLoresDxy0Is16x16 = props[PROP_Q_LORES_DXY0_IS_16X16].getBool();
    opts.optSC11Collision = props[PROP_Q_SC11_COLLISION].getBool();
    opts.optSCLoresDrawing = props[PROP_Q_SC_LORES_DRAWING].getBool();
    opts.optHalfPixelScroll = props[PROP_Q_HALF_PIXEL_SCROLL].getBool();
    opts.optModeChangeClear = props[PROP_Q_MODE_CHANGE_CLEAR].getBool();
    opts.optJump0Bxnn = props[PROP_Q_JUMP0_BXNN].getBool();
    opts.optAllowHires = props[PROP_Q_ALLOW_HIRES].getBool();
    opts.optOnlyHires = props[PROP_Q_ONLY_HIRES].getBool();
    opts.optAllowColors = props[PROP_Q_ALLOW_COLORS].getBool();
    opts.optCyclicStack = props[PROP_Q_CYCLIC_STACK].getBool();
    opts.optHas16BitAddr = props[PROP_Q_HAS_16BIT_ADDR].getBool();
    opts.optXOChipSound = props[PROP_Q_XO_CHIP_SOUND].getBool();
    opts.optExtendedVBlank = props[PROP_Q_EXTENDED_VBLANK].getBool();
    opts.optPalVideo = props[PROP_Q_PAL_VIDEO].getBool();
    opts.rotation = static_cast<ScreenRotation>(props[PROP_SCREEN_ROTATION].getSelectedIndex());
    opts.touchInputMode = static_cast<TouchInputMode>(props[PROP_TOUCH_INPUT_MODE].getSelectedIndex());
    opts.fontStyle5 = static_cast<FontStyle5px>(props[PROP_FONT_5PX].getSelectedIndex());
    opts.fontStyle10 = static_cast<FontStyle10px>(props[PROP_FONT_10PX].getSelectedIndex());
    opts.palette = props.palette();
    return opts;
}

Properties& Chip8GenericOptions::registeredPrototype()
{
    using namespace std::string_literals;
    auto& prototype = Properties::getProperties(PROP_CLASS);
    if(!prototype) {
        prototype.registerProperty({PROP_BEHAVIOR_BASE, Property::Combo{"CHIP-8"s, "CHIP-10"s, "CHIP-8E"s, "CHIP-8X"s, "CHIP-48"s, "SCHIP-1.0"s, "SCHIP-1.1"s, "SCHIPC"s, "SCHIP-MODERN"s, "MEGACHIP"s, "XO-CHIP"s}, "CHIP-8 variant", eInvisible});
        prototype.registerProperty({PROP_TRACE_LOG, false, "Enable trace log", eWritable});
        prototype.registerProperty({PROP_INSTRUCTIONS_PER_FRAME, Property::Integer{11, 0, 1'000'000}, "Number of instructions per frame, default depends on variant", eWritable});
        prototype.registerProperty({PROP_FRAME_RATE, Property::Integer{60, 50, 100}, "Number of frames per second, default 60", eWritable});
        prototype.registerProperty({PROP_RAM, Property::Combo{"2048"s, "4096"s, "8192"s, "16384"s, "32768"s, "65536"s, "16777216"s}, "Size of ram in bytes", eWritable});
        prototype.registerProperty({PROP_START_ADDRESS, Property::Integer{0x200, 0, 0x7f0}, "Number of instructions per frame, default depends on variant", eReadOnly});
        prototype.registerProperty({PROP_CLEAN_RAM, false, "Delete ram on startup", eWritable});
        prototype.registerProperty({{PROP_Q_JUST_SHIFT_VX, "just-Shift-Vx"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_DONT_RESET_VF, "dont-Reset-Vf"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_LOAD_STORE_INC_I_BY_X_PLUS_ONE, "load-Store-Inc-I-By-X-Plus-1"}, true, eWritable});
        prototype.registerProperty({{PROP_Q_LOAD_STORE_INC_I_BY_X, "load-Store-Inc-I-ByX"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_WRAP_SPRITES, "wrap-sprites"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_INSTANT_DXYN, "instant-dxyn"}, false, eWritable});
        prototype.registerProperty({"", nullptr, ""});
        prototype.registerProperty({{PROP_Q_LORES_DXY0_IS_8X16, "lores-Dxy0-Is-8x16"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_LORES_DXY0_IS_16X16, "lores-Dxy0-Is-16x16"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_SC11_COLLISION, "schip-11-Collision"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_SC_LORES_DRAWING, "schip-Lores-Drawing"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_HALF_PIXEL_SCROLL, "half-Pixel-Scroll"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_MODE_CHANGE_CLEAR, "mode-Change-Clear"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_JUMP0_BXNN, "jump0-Bxnn"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_ALLOW_HIRES, "allow-Hires"}, false, eInvisible});
        prototype.registerProperty({{PROP_Q_ONLY_HIRES, "only-Hires"}, false, eInvisible});
        prototype.registerProperty({{PROP_Q_ALLOW_COLORS, "allow-Colors"}, false, eInvisible});
        prototype.registerProperty({{PROP_Q_CYCLIC_STACK, "cyclic-Stack"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_HAS_16BIT_ADDR, "has-16Bit-Addr"}, false, eInvisible});
        prototype.registerProperty({{PROP_Q_XO_CHIP_SOUND, "xo-Chip-Sound"}, false, eInvisible});
        prototype.registerProperty({{PROP_Q_EXTENDED_VBLANK, "extended-Vblank"}, false, eWritable});
        prototype.registerProperty({{PROP_Q_PAL_VIDEO, "pal-Video"}, false, eInvisible});

        prototype.registerProperty({{PROP_SCREEN_ROTATION, "screen-rotation"}, Property::Combo{"0°"s, "90°"s, "180°"s, "270°"s}, eInvisible});
        prototype.registerProperty({{PROP_TOUCH_INPUT_MODE, "touch-mode"}, Property::Combo{"SWIPE"s, "SEG16"s, "SEG16FILL"s, "GAMEPAD"s, "VIP"s}, eInvisible});
        prototype.registerProperty({{PROP_FONT_5PX, "font-5px"}, Property::Combo{"DEFAULT"s, "VIP"s, "DREAM6800"s, "ETI660"s, "SCHIP"s, "FISH"s, "OCTO"s, "AKOUZ1"s}, eInvisible});
        prototype.registerProperty({{PROP_FONT_10PX, "font-10px"}, Property::Combo{"DEFAULT"s, "SCHIP10"s, "SCHIP11"s, "FISH"s, "MEGACHIP"s, "OCTO"s, "AUCHIP"s}, eInvisible});
    }
    return prototype;
}

Chip8Variant Chip8GenericOptions::variant() const
{
    switch(behaviorBase) {
        case eCHIP8: return Chip8Variant::CHIP_8;
        case eCHIP10: return Chip8Variant::CHIP_10;
        case eCHIP8E: return Chip8Variant::CHIP_8E;
        case eCHIP8X: return Chip8Variant::CHIP_8X;
        case eCHIP48: return Chip8Variant::CHIP_48;
        case eSCHIP10: return Chip8Variant::SCHIP_1_0;
        case eSCHIP11: return Chip8Variant::SCHIP_1_1;
        case eSCHPC: return Chip8Variant::SCHIPC_GCHIPC;
        case eSCHIP_MODERN: return Chip8Variant::SCHIP_MODERN;
        case eMEGACHIP: return Chip8Variant::MEGA_CHIP;
        case eXOCHIP: return Chip8Variant::XO_CHIP;
        default: return Chip8Variant::CHIP_8;
    }
}

struct Chip8GenericSetupInfo
{
    const char* presetName;
    const char* description;
    const char* defaultExtensions;
    chip8::VariantSet supportedChip8Variants{chip8::Variant::CHIP_8};
    Chip8GenericOptions options;
};

// clang-format off
Chip8GenericSetupInfo genericPresets[] = {
    {
        "CHIP-8",
        "The classic CHIP-8 for the COSMAC VIP by Joseph Weisbecker, 1977",
        ".ch8",
        chip8::Variant::CHIP_8,
        {.behaviorBase = Chip8GenericOptions::eCHIP8, .optExtendedVBlank = true}
    },
    {
        "CHIP-10",
        "128x64 CHIP-8 from #VIPER-V1-I7 and #IpsoFacto-I10, by Ben H. Hutchinson, Jr., 1979",
        ".ch10",
        chip8::Variant::CHIP_10,
        {.behaviorBase = Chip8GenericOptions::eCHIP10, .optAllowHires = true, .optOnlyHires = true, .optExtendedVBlank = true}
    },
    {
        "CHIP-8E",
        "CHIP-8 rewritten and extended by Gilles Detillieux, from #VIPER-V2-8+9",
        ".c8e",
        chip8::Variant::CHIP_8E,
        {.behaviorBase = Chip8GenericOptions::eCHIP8E, .optExtendedVBlank = true}
    },
    {
        "CHIP-8X",
        "An official update to CHIP-8 by RCA, requiring the color extension VP-590 and the simple sound board VP-595, 1980",
        ".c8x",
        chip8::Variant::CHIP_8X,
        {.behaviorBase = Chip8GenericOptions::eCHIP8X, .startAddress = 768, .optExtendedVBlank = true, .instructionsPerFrame = 18,
            .palette = { {"#181818","#FF0000","#0000FF","#FF00FF","#00FF00","#FFFF00","#00FFFF","#FFFFFF"}, {"#000080","#000000","#008000","#800000"}}
        }
    },
    {
        "CHIP-48",
        "The initial CHIP-8 port to the HP-48SX by Andreas Gustafsson, 1990",
        ".ch48;.c48",
        chip8::Variant::CHIP_48,
        {.behaviorBase = Chip8GenericOptions::eCHIP48, .optJustShiftVx = true, .optDontResetVf = true, .optLoadStoreIncIByX = true, .optJump0Bxnn = true, .instructionsPerFrame = 15, .frameRate = 64, .fontStyle5 = Chip8GenericOptions::FontStyle5px::SCHIP}
    },
    {
        "SCHIP-1.0",
        "SUPER-CHIP v1.0 expansion of CHIP-48 for the HP-48SX with 128x64 hires mode by Erik Bryntse, 1991",
        ".sc10",
        chip8::Variant::SCHIP_1_0,
        {.behaviorBase = Chip8GenericOptions::eSCHIP10, .optJustShiftVx = true, .optDontResetVf = true, .optLoadStoreIncIByX = true, .optLoresDxy0Is8x16 = true, .optSCLoresDrawing = true, .optJump0Bxnn = true, .optAllowHires = true, .instructionsPerFrame = 30, .frameRate = 64, .fontStyle5 = Chip8GenericOptions::FontStyle5px::SCHIP, .fontStyle10 = Chip8GenericOptions::FontStyle10px::SCHIP10}
    },
    {
        "SCHIP-1.1",
        "SUPER-CHIP v1.1 expansion of CHIP-48 for the HP-48SX with 128x64 hires mode by Erik Bryntse, 1991",
        ".sc8;.sc11",
        chip8::Variant::SCHIP_1_1,
        {.behaviorBase = Chip8GenericOptions::eSCHIP11, .optJustShiftVx = true, .optDontResetVf = true, .optLoadStoreDontIncI = true, .optLoresDxy0Is8x16 = true, .optSC11Collision = true, .optSCLoresDrawing = true, .optHalfPixelScroll = true, .optJump0Bxnn = true, .optAllowHires = true, .instructionsPerFrame = 30, .frameRate = 64, .fontStyle5 = Chip8GenericOptions::FontStyle5px::SCHIP, .fontStyle10 = Chip8GenericOptions::FontStyle10px::SCHIP11}
    },
    {
        "SCHIPC",
        "SUPER-CHIP compatibility fix for the HP-48SX by Chromatophore, 2017",
        ".scc",
        chip8::Variant::SCHIPC,
        {.behaviorBase = Chip8GenericOptions::eSCHPC, .optDontResetVf = true, .optLoresDxy0Is8x16 = true, .optModeChangeClear = true, .optAllowHires = true, .instructionsPerFrame = 30, .frameRate = 64, .fontStyle5 = Chip8GenericOptions::FontStyle5px::SCHIP, .fontStyle10 = Chip8GenericOptions::FontStyle10px::SCHIP11}
    },
    {
        "SCHIP-MODERN",
        "Modern SUPER-CHIP interpretation as done in Octo by John Earnest, 2014",
        ".scm",
        chip8::Variant::SCHIP_MODERN,
        {.behaviorBase = Chip8GenericOptions::eSCHIP_MODERN, .optJustShiftVx = true, .optDontResetVf = true, .optLoadStoreDontIncI = true, .optInstantDxyn = true, .optLoresDxy0Is16x16 = true, .optModeChangeClear = true, .optJump0Bxnn = true, .optAllowHires = true, .instructionsPerFrame = 30, .frameRate = 64, .fontStyle5 = Chip8GenericOptions::FontStyle5px::SCHIP, .fontStyle10 = Chip8GenericOptions::FontStyle10px::SCHIP11}
    },
    {
        "MEGACHIP",
        "MegaChip as specified by Martijn Wanting, Revival-Studios, 2007",
        ".mc8",
        chip8::Variant::MEGA_CHIP,
        {.behaviorBase = Chip8GenericOptions::eMEGACHIP, .ramSize = 0x1000000, .optJustShiftVx = true, .optDontResetVf = true, .optLoadStoreDontIncI = true, .optLoresDxy0Is8x16 = true, .optSC11Collision = true, .optModeChangeClear = true, .optJump0Bxnn = true, .optAllowHires = true, .instructionsPerFrame = 3000, .frameRate = 50, .fontStyle5 = Chip8GenericOptions::FontStyle5px::SCHIP, .fontStyle10 = Chip8GenericOptions::FontStyle10px::MEGACHIP}
    },
    {
        "XO-CHIP",
        "A modern extension to SUPER-CHIP supporting colors and actual sound first implemented in Octo by John Earnest, 2014",
        ".xo8",
        chip8::Variant::XO_CHIP,
        {.behaviorBase = Chip8GenericOptions::eXOCHIP, .ramSize = 0x10000, .optDontResetVf = true, .optWrapSprites = true, .optInstantDxyn = true, .optLoresDxy0Is16x16 = true, .optModeChangeClear = true, .optAllowHires = true, .optAllowColors = true, .optHas16BitAddr = true, .optXOChipSound = true, .instructionsPerFrame = 1000, .fontStyle5 = Chip8GenericOptions::FontStyle5px::OCTO, .fontStyle10 = Chip8GenericOptions::FontStyle10px::OCTO}
    }
    /*{Opts::eCHIP10, R"({"optAllowHires":true,"optOnlyHires":true})"},
    {Opts::eCHIP8E, R"({})"},
    {Opts::eCHIP8X, R"({"startAddress":768,"instructionsPerFrame":18,"advanced":{"palette":["#000080","#000000","#008000","#800000","#181818","#FF0000","#0000FF","#FF00FF","#00FF00","#FFFF00","#00FFFF","#FFFFFF","#000000","#000000","#000000","#000000"]}})"},
    {Opts::eCHIP48, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreIncIByX":true,"optInstantDxyn":false,"optJump0Bxnn":true,"instructionsPerFrame":15,"frameRate":64})"},
    {Opts::eSCHIP10, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreIncIByX":true,"optInstantDxyn":false,"optLoresDxy0Is8x16":true,"optSCLoresDrawing":true,"optJump0Bxnn":true,"optAllowHires":true,"instructionsPerFrame":15,"frameRate":64})"},
    {Opts::eSCHIP11, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreDontIncI":true,"optInstantDxyn":false,"optLoresDxy0Is8x16":true,"optSCLoresDrawing":true,"optSC11Collision":true,"optHalfPixelScroll":true,"optJump0Bxnn":true,"optAllowHires":true,"instructionsPerFrame":30,"frameRate":64})"},
    {Opts::eSCHPC, R"({"optDontResetVf":true,"optInstantDxyn":true,"optLoresDxy0Is16x16":true,"optModeChangeClear":true,"optAllowHires":true,"instructionsPerFrame":30})"},
    {Opts::eSCHIP_MODERN, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreDontIncI":true,"optInstantDxyn":true,"optJump0Bxnn":true,"optLoresDxy0Is16x16":true,"optModeChangeClear":true,"optAllowHires":true,"instructionsPerFrame":30})"},
    {Opts::eMEGACHIP, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreDontIncI":true,"optInstantDxyn":true,"optLoresDxy0Is8x16":true,"optSC11Collision":true,"optModeChangeClear":true,"optAllowHires":true,"optHas16BitAddr":true,"instructionsPerFrame":3000,"frameRate":50})"},
    {Opts::eXOCHIP, R"({"optDontResetVf":true,"optWrapSprites":true,"optInstantDxyn":true,"optLoresDxy0Is16x16":true,"optModeChangeClear":true,"optAllowHires":true,"optAllowColors":true,"optHas16BitAddr":true,"optXOChipSound":true,"instructionsPerFrame":1000})"},
*/
};
// clang-format on

struct C8GenericFactoryInfo final : public CoreRegistry::FactoryInfo<Chip8GenericEmulator, Chip8GenericSetupInfo, Chip8GenericOptions>
{
    explicit C8GenericFactoryInfo(const char* description)
        : FactoryInfo(0, genericPresets, description)
    {}
    std::string prefix() const override
    {
        return "";
    }
    VariantIndex variantIndex(const Properties& props) const override
    {
        auto idx = props[PROP_BEHAVIOR_BASE].getSelectedIndex();
        return {idx, genericPresets[idx].options.asProperties() == props};
    }
};

static bool registeredHleC8 = CoreRegistry::registerFactory(PROP_CLASS, std::make_unique<C8GenericFactoryInfo>("Default HLE CHIP-8 emulation"));

static uint32_t rgb332To888(uint8_t c)
{
    static uint8_t b3[] = {0, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0xff};
    static uint8_t b2[] = {0, 0x60, 0xA0, 0xff};
    return (b3[(c & 0xe0) >> 5] << 16) | (b3[(c & 0x1c) >> 2] << 8) | (b2[c & 3]);
}


Chip8GenericEmulator::Chip8GenericEmulator(EmulatorHost& host, Properties& props, IChip8Emulator* other)
: Chip8GenericBase(Chip8GenericOptions::fromProperties(props).variant(), {})
, _host(host)
, _options(Chip8GenericOptions::fromProperties(props))
, _opcodeHandler(0x10000, &Chip8GenericEmulator::opInvalid)
{
    ADDRESS_MASK = _options.ramSize - 1; //_options.behaviorBase == Chip8GenericOptions::eMEGACHIP ? 0xFFFFFF : _options.ramSize>4096 ? 0xFFFF : 0xFFF;
    SCREEN_WIDTH = _options.behaviorBase == Chip8GenericOptions::eMEGACHIP ? 256 : (_options.optAllowHires ? 128 : 64);
    SCREEN_HEIGHT = _options.behaviorBase == Chip8GenericOptions::eMEGACHIP ? 192 : (_options.optAllowHires ? 64 : (_options.optPalVideo ? 48 : 32));
    _memory.resize(_options.ramSize, 0);
    if(!_options.cleanRam) {
        ghc::RandomLCG rnd(42);
        std::generate(_memory.begin(), _memory.end(), rnd);
    }
    _rV = _rVData.data();
    _screen.setMode(SCREEN_WIDTH, SCREEN_HEIGHT);
    _screenRGBA1.setMode(SCREEN_WIDTH, SCREEN_HEIGHT);
    _screenRGBA2.setMode(SCREEN_WIDTH, SCREEN_HEIGHT);
    if(!props.palette().empty()) {
        _screen.setPalette(props.palette());
    }
    setHandler();
}

GenericCpu::StackContent Chip8GenericEmulator::stack() const
{
    return {2, eNATIVE, eUPWARDS, std::span(reinterpret_cast<const uint8_t*>(_stack.data()), reinterpret_cast<const uint8_t*>(_stack.data() + _stack.size()))};
}

static Chip8GenericBase::Chip8Font getSmallFontId(Chip8GenericOptions::SupportedPreset behavior)
{
    switch (behavior) {
        case Chip8GenericOptions::eCHIP48:
        case Chip8GenericOptions::eSCHIP10:
        case Chip8GenericOptions::eSCHIP11:
        case Chip8GenericOptions::eSCHPC:
        case Chip8GenericOptions::eSCHIP_MODERN:
        case Chip8GenericOptions::eMEGACHIP:
        case Chip8GenericOptions::eXOCHIP:
            return Chip8GenericBase::C8F5_CHIP48;
        case Chip8GenericOptions::eCHIP8:
        case Chip8GenericOptions::eCHIP10:
        default:
            return Chip8GenericBase::C8F5_COSMAC;
    }
}
static Chip8GenericBase::Chip8BigFont getBigFontId(Chip8GenericOptions::SupportedPreset behavior)
{
    switch (behavior) {
        case Chip8GenericOptions::eSCHIP10:
            return Chip8GenericBase::C8F10_SCHIP10;
        case Chip8GenericOptions::eSCHIP11:
        case Chip8GenericOptions::eSCHPC:
        case Chip8GenericOptions::eSCHIP_MODERN:
            return Chip8GenericBase::C8F10_SCHIP11;
        case Chip8GenericOptions::eMEGACHIP:
            return Chip8GenericBase::C8F10_MEGACHIP;
        case Chip8GenericOptions::eXOCHIP:
            return Chip8GenericBase::C8F10_XOCHIP;
        case Chip8GenericOptions::eCHIP8:
        case Chip8GenericOptions::eCHIP10:
        case Chip8GenericOptions::eCHIP48:
        default:
            return Chip8GenericBase::C8F10_NONE;
    }
}

void Chip8GenericEmulator::handleReset()
{
    //static const uint8_t defaultPalette[16] = {37, 255, 114, 41, 205, 153, 42, 213, 169, 85, 37, 114, 87, 159, 69, 9};
    static const uint8_t defaultPalette[16] = {0, 255, 182, 109, 224, 28, 3, 252, 160, 20, 2, 204, 227, 31, 162, 22};
    //static const uint8_t defaultPalette[16] = {172, 248, 236, 100, 205, 153, 42, 213, 169, 85, 37, 114, 87, 159, 69, 9};
    _cycleCounter = 0;
    _frameCounter = 0;
    _clearCounter = 0;
    _systemTime.reset();
    if(_options.cleanRam)
        std::memset(_memory.data(), 0, _memory.size());
    if(_options.traceLog)
        Logger::log(Logger::eCHIP8, _cycleCounter, {_frameCounter, 0}, "--- RESET ---");
    _rI = 0;
    _rPC = _options.startAddress;
    std::memset(_stack.data(), 0, 16 * 2);
    _rSP = 0;
    _rDT = 0;
    _rST = 0;
    std::memset(_rV, 0, 16);
    auto [smallFont, smallSize] = smallFontData(getSmallFontId(_options.behaviorBase));
    std::memcpy(_memory.data(), smallFont, smallSize);
    auto bigFontId = getBigFontId(_options.behaviorBase);
    if(bigFontId != C8F10_NONE) {
        auto [bigFont, bigSize] = bigFontData(bigFontId);
        if(bigSize)
            std::memcpy(_memory.data() + 16*5, bigFont, bigSize);
    }
    std::memcpy(_xxoPalette.data(), defaultPalette, 16);
    std::memset(_xoAudioPattern.data(), 0, 16);
    _xoSilencePattern = true;
    _xoPitch = 64;
    _planes = 0xff;
    _screenAlpha = 0xff;
    _screenRGBA = &_screenRGBA1;
    _workRGBA = &_screenRGBA2;
    _screen.setAll(0);
    _screenRGBA1.setAll(0);
    _screenRGBA2.setAll(0);
    //_host.updatePalette(_xxoPalette);
    _execMode = _host.isHeadless() ? eRUNNING : ePAUSED;
    _cpuState = eNORMAL;
    _errorMessage.clear();
    _wavePhase = 0;
    _simpleRandState = _simpleRandSeed;
    _isHires = _options.optOnlyHires;
    _isInstantDxyn = _options.optInstantDxyn;
    _isMegaChipMode = false;
    _planes = 1;
    _spriteWidth = 0;
    _spriteHeight = 0;
    _collisionColor = 1;
    _sampleLength = 0;
    _sampleStep = 0;
    _mcSamplePos = 0;
    _blendMode = eBLEND_NORMAL;
    _simpleRandState = _simpleRandSeed;
    if(_options.behaviorBase == Chip8GenericOptions::eCHIP8X) {
        _screen.setMode(256, 192, 4); // actual resolution doesn't matter, just needs to be bigger than max resolution, but ratio matters
        _screen.setOverlayCellHeight(-1); // reset
        _chip8xBackgroundColor = 0;
        _screen.setPalette(_options.palette);
    }
    else if(_options.behaviorBase == Chip8GenericOptions::eMEGACHIP) {
        _options.palette.colors.resize(256);
        std::ranges::fill(_options.palette.colors, Palette::Color(0,0,0));
        _options.palette.colors[1] = Palette::Color(255,255,255);
        _options.palette.colors[255] = Palette::Color(255,255,255);
    }
    initExpressionist();
}

bool Chip8GenericEmulator::updateProperties(Properties& props, Property& changed)
{
    if (fuzzyAnyOf(changed.getName(), {"TraceLog", "InstructionsPerFrame", "FrameRate"})) {
        _options = Chip8GenericOptions::fromProperties(props);
        return false;
    }
    return true;
}

void Chip8GenericEmulator::setPalette(const Palette& palette)
{
    _screen.setPalette(palette);
}

int Chip8GenericEmulator::getMaxColors() const
{
    switch (_options.behaviorBase) {
        case Chip8GenericOptions::eCHIP8X: return 8;
        case Chip8GenericOptions::eMEGACHIP: return 256;
        case Chip8GenericOptions::eXOCHIP: return 16;
        default:
            return 2;
    }
}

uint32_t Chip8GenericEmulator::defaultLoadAddress() const
{
    return _options.startAddress;
}

bool Chip8GenericEmulator::loadData(std::span<const uint8_t> data, std::optional<uint32_t> loadAddress)
{
    return Chip8GenericBase::loadData(data, loadAddress ? loadAddress : _options.startAddress);
}

void Chip8GenericEmulator::setHandler()
{
    on(0xFFFF, 0x00E0, &Chip8GenericEmulator::op00E0);
    on(0xFFFF, 0x00EE, _options.optCyclicStack ? &Chip8GenericEmulator::op00EE_cyclic : &Chip8GenericEmulator::op00EE);
    on(0xF000, 0x1000, &Chip8GenericEmulator::op1nnn);
    on(0xF000, 0x2000, _options.optCyclicStack ? &Chip8GenericEmulator::op2nnn_cyclic : &Chip8GenericEmulator::op2nnn);
    on(0xF000, 0x3000, &Chip8GenericEmulator::op3xnn);
    on(0xF000, 0x4000, &Chip8GenericEmulator::op4xnn);
    on(0xF00F, 0x5000, &Chip8GenericEmulator::op5xy0);
    on(0xF000, 0x6000, &Chip8GenericEmulator::op6xnn);
    on(0xF000, 0x7000, &Chip8GenericEmulator::op7xnn);
    on(0xF00F, 0x8000, &Chip8GenericEmulator::op8xy0);
    on(0xF00F, 0x8001, _options.optDontResetVf ? &Chip8GenericEmulator::op8xy1_dontResetVf : &Chip8GenericEmulator::op8xy1);
    on(0xF00F, 0x8002, _options.optDontResetVf ? &Chip8GenericEmulator::op8xy2_dontResetVf : &Chip8GenericEmulator::op8xy2);
    on(0xF00F, 0x8003, _options.optDontResetVf ? &Chip8GenericEmulator::op8xy3_dontResetVf : &Chip8GenericEmulator::op8xy3);
    on(0xF00F, 0x8004, &Chip8GenericEmulator::op8xy4);
    on(0xF00F, 0x8005, &Chip8GenericEmulator::op8xy5);
    on(0xF00F, 0x8006, _options.optJustShiftVx ? &Chip8GenericEmulator::op8xy6_justShiftVx : &Chip8GenericEmulator::op8xy6);
    on(0xF00F, 0x8007, &Chip8GenericEmulator::op8xy7);
    on(0xF00F, 0x800E, _options.optJustShiftVx ? &Chip8GenericEmulator::op8xyE_justShiftVx : &Chip8GenericEmulator::op8xyE);
    on(0xF00F, 0x9000, &Chip8GenericEmulator::op9xy0);
    on(0xF000, 0xA000, &Chip8GenericEmulator::opAnnn);
    if(_options.behaviorBase != Chip8GenericOptions::eCHIP8X)
        on(0xF000, 0xB000, _options.optJump0Bxnn ? &Chip8GenericEmulator::opBxnn : &Chip8GenericEmulator::opBnnn);
    std::string randomGen;
    // TODO: fix this
    //if(_options.advanced.contains("random")) {
    //    randomGen = _options.advanced.at("random");
    //    _randomSeed = _options.advanced.at("seed");
    //}
    if(randomGen == "rand-lcg")
        on(0xF000, 0xC000, &Chip8GenericEmulator::opCxnn_randLCG);
    else if(randomGen == "counting")
        on(0xF000, 0xC000, &Chip8GenericEmulator::opCxnn_counting);
    else
        on(0xF000, 0xC000, &Chip8GenericEmulator::opCxnn);
    if(_options.behaviorBase == Chip8GenericOptions::eCHIP8X) {
        if(_options.optInstantDxyn)
            on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<0>);
        else
            on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn_displayWait<0>);
    }
    else if(_options.optAllowHires) {
        if(_options.optAllowColors) {
            if (_options.optWrapSprites)
                on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<HiresSupport|MultiColor|WrapSprite>);
            else
                on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<HiresSupport|MultiColor>);
        }
        else {
            if (_options.optWrapSprites)
                on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<HiresSupport|WrapSprite>);
            else {
                if (_options.optSCLoresDrawing) {
                    if (_options.optSC11Collision)
                        on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<HiresSupport|SChip1xLoresDraw|SChip11Collisions>);
                    else
                        on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<HiresSupport|SChip1xLoresDraw>);
                }
                else {
                    if (_options.optSC11Collision)
                        on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<HiresSupport|SChip11Collisions>);
                    else
                        on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<HiresSupport>);
                }
            }
        }
    }
    else {
        if(_options.optAllowColors) {
            if (_options.optWrapSprites)
                on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<MultiColor|WrapSprite>);
            else
                on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<MultiColor>);
        }
        else {
            if (_options.optWrapSprites)
                on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<WrapSprite>);
            else {
                if(_options.optInstantDxyn)
                    on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn<0>);
                else
                    on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn_displayWait<0>);
            }
        }
    }
    //on(0xF000, 0xD000, _options.optAllowHires ? &Chip8GenericEmulator::opDxyn_allowHires : &Chip8GenericEmulator::opDxyn);
    on(0xF0FF, 0xE09E, &Chip8GenericEmulator::opEx9E);
    on(0xF0FF, 0xE0A1, &Chip8GenericEmulator::opExA1);
    on(0xF0FF, 0xF007, &Chip8GenericEmulator::opFx07);
    on(0xF0FF, 0xF00A, &Chip8GenericEmulator::opFx0A);
    on(0xF0FF, 0xF015, &Chip8GenericEmulator::opFx15);
    on(0xF0FF, 0xF018, &Chip8GenericEmulator::opFx18);
    on(0xF0FF, 0xF01E, &Chip8GenericEmulator::opFx1E);
    on(0xF0FF, 0xF029, &Chip8GenericEmulator::opFx29);
    on(0xF0FF, 0xF033, &Chip8GenericEmulator::opFx33);
    on(0xF0FF, 0xF055, _options.optLoadStoreIncIByX ? &Chip8GenericEmulator::opFx55_loadStoreIncIByX : (_options.optLoadStoreDontIncI ? &Chip8GenericEmulator::opFx55_loadStoreDontIncI : &Chip8GenericEmulator::opFx55));
    on(0xF0FF, 0xF065, _options.optLoadStoreIncIByX ? &Chip8GenericEmulator::opFx65_loadStoreIncIByX : (_options.optLoadStoreDontIncI ? &Chip8GenericEmulator::opFx65_loadStoreDontIncI : &Chip8GenericEmulator::opFx65));

    switch(_options.behaviorBase) {
        case Chip8GenericOptions::eSCHIP10:
            on(0xFFFF, 0x00FD, &Chip8GenericEmulator::op00FD);
            if(_options.optModeChangeClear) {
                on(0xFFFF, 0x00FE, &Chip8GenericEmulator::op00FE_withClear);
                on(0xFFFF, 0x00FF, &Chip8GenericEmulator::op00FF_withClear);
            }
            else {
                on(0xFFFF, 0x00FE, &Chip8GenericEmulator::op00FE);
                on(0xFFFF, 0x00FF, &Chip8GenericEmulator::op00FF);
            }
            on(0xF0FF, 0xF029, &Chip8GenericEmulator::opFx29_ship10Beta);
            on(0xF0FF, 0xF075, &Chip8GenericEmulator::opFx75);
            on(0xF0FF, 0xF085, &Chip8GenericEmulator::opFx85);
            break;
        case Chip8GenericOptions::eCHIP8E:
            on(0xFFFF, 0x00ED, &Chip8GenericEmulator::op00ED_c8e);
            on(0xFFFF, 0x00F2, &Chip8GenericEmulator::opNop);
            on(0xFFFF, 0x0151, &Chip8GenericEmulator::op0151_c8e);
            on(0xFFFF, 0x0188, &Chip8GenericEmulator::op0188_c8e);
            on(0xF00F, 0x5001, &Chip8GenericEmulator::op5xy1_c8e);
            on(0xF00F, 0x5002, &Chip8GenericEmulator::op5xy2_c8e);
            on(0xF00F, 0x5003, &Chip8GenericEmulator::op5xy3_c8e);
            on(0xFF00, 0xBB00, &Chip8GenericEmulator::opBBnn_c8e);
            on(0xFF00, 0xBF00, &Chip8GenericEmulator::opBFnn_c8e);
            on(0xF0FF, 0xF003, &Chip8GenericEmulator::opNop);
            on(0xF0FF, 0xF01B, &Chip8GenericEmulator::opFx1B_c8e);
            on(0xF0FF, 0xF04F, &Chip8GenericEmulator::opFx4F_c8e);
            on(0xF0FF, 0xF0E3, &Chip8GenericEmulator::opNop);
            on(0xF0FF, 0xF0E7, &Chip8GenericEmulator::opNop);
            break;
        case Chip8GenericOptions::eCHIP8X:
            on(0xFFFF, 0x02A0, &Chip8GenericEmulator::op02A0_c8x);
            on(0xF00F, 0x5001, &Chip8GenericEmulator::op5xy1_c8x);
            on(0xF000, 0xB000, &Chip8GenericEmulator::opBxyn_c8x);
            on(0xF00F, 0xB000, &Chip8GenericEmulator::opBxy0_c8x);
            on(0xF0FF, 0xE0F2, &Chip8GenericEmulator::opExF2_c8x);
            on(0xF0FF, 0xE0F5, &Chip8GenericEmulator::opExF5_c8x);
            on(0xF0FF, 0xF0F8, &Chip8GenericEmulator::opFxF8_c8x);
            on(0xF0FF, 0xF0FB, &Chip8GenericEmulator::opFxFB_c8x);
            break;
        case Chip8GenericOptions::eSCHIP11:
        case Chip8GenericOptions::eSCHPC:
        case Chip8GenericOptions::eSCHIP_MODERN:
            on(0xFFF0, 0x00C0, &Chip8GenericEmulator::op00Cn);
            on(0xFFFF, 0x00C0, &Chip8GenericEmulator::opInvalid);
            on(0xFFFF, 0x00FB, &Chip8GenericEmulator::op00FB);
            on(0xFFFF, 0x00FC, &Chip8GenericEmulator::op00FC);
            on(0xFFFF, 0x00FD, &Chip8GenericEmulator::op00FD);
            if(_options.optModeChangeClear) {
                on(0xFFFF, 0x00FE, &Chip8GenericEmulator::op00FE_withClear);
                on(0xFFFF, 0x00FF, &Chip8GenericEmulator::op00FF_withClear);
            }
            else {
                on(0xFFFF, 0x00FE, &Chip8GenericEmulator::op00FE);
                on(0xFFFF, 0x00FF, &Chip8GenericEmulator::op00FF);
            }
            on(0xF0FF, 0xF030, &Chip8GenericEmulator::opFx30);
            on(0xF0FF, 0xF075, &Chip8GenericEmulator::opFx75);
            on(0xF0FF, 0xF085, &Chip8GenericEmulator::opFx85);
            break;
        case Chip8GenericOptions::eMEGACHIP:
            on(0xFFFF, 0x0010, &Chip8GenericEmulator::op0010);
            on(0xFFFF, 0x0011, &Chip8GenericEmulator::op0011);
            on(0xFFF0, 0x00B0, &Chip8GenericEmulator::op00Bn);
            on(0xFFF0, 0x00C0, &Chip8GenericEmulator::op00Cn);
            on(0xFFFF, 0x00E0, &Chip8GenericEmulator::op00E0_megachip);
            on(0xFFFF, 0x00FB, &Chip8GenericEmulator::op00FB);
            on(0xFFFF, 0x00FC, &Chip8GenericEmulator::op00FC);
            on(0xFFFF, 0x00FD, &Chip8GenericEmulator::op00FD);
            on(0xFFFF, 0x00FE, &Chip8GenericEmulator::op00FE_megachip);
            on(0xFFFF, 0x00FF, &Chip8GenericEmulator::op00FF_megachip);
            on(0xFF00, 0x0100, &Chip8GenericEmulator::op01nn);
            on(0xFF00, 0x0200, &Chip8GenericEmulator::op02nn);
            on(0xFF00, 0x0300, &Chip8GenericEmulator::op03nn);
            on(0xFF00, 0x0400, &Chip8GenericEmulator::op04nn);
            on(0xFF00, 0x0500, &Chip8GenericEmulator::op05nn);
            on(0xFFF0, 0x0600, &Chip8GenericEmulator::op060n);
            on(0xFFFF, 0x0700, &Chip8GenericEmulator::op0700);
            on(0xFFF0, 0x0800, &Chip8GenericEmulator::op080n);
            on(0xFF00, 0x0900, &Chip8GenericEmulator::op09nn);
            on(0xF000, 0x3000, &Chip8GenericEmulator::op3xnn_with_01nn);
            on(0xF000, 0x4000, &Chip8GenericEmulator::op4xnn_with_01nn);
            on(0xF00F, 0x5000, &Chip8GenericEmulator::op5xy0_with_01nn);
            on(0xF00F, 0x9000, &Chip8GenericEmulator::op9xy0_with_01nn);
            on(0xF000, 0xD000, &Chip8GenericEmulator::opDxyn_megaChip);
            on(0xF0FF, 0xE09E, &Chip8GenericEmulator::opEx9E_with_01nn);
            on(0xF0FF, 0xE0A1, &Chip8GenericEmulator::opExA1_with_01nn);
            on(0xF0FF, 0xF030, &Chip8GenericEmulator::opFx30);
            on(0xF0FF, 0xF075, &Chip8GenericEmulator::opFx75);
            on(0xF0FF, 0xF085, &Chip8GenericEmulator::opFx85);
            break;
        case Chip8GenericOptions::eXOCHIP:
            on(0xFFF0, 0x00C0, &Chip8GenericEmulator::op00Cn_masked);
            on(0xFFF0, 0x00D0, &Chip8GenericEmulator::op00Dn_masked);
            on(0xFFFF, 0x00FB, &Chip8GenericEmulator::op00FB_masked);
            on(0xFFFF, 0x00FC, &Chip8GenericEmulator::op00FC_masked);
            on(0xFFFF, 0x00FD, &Chip8GenericEmulator::op00FD);
            on(0xFFFF, 0x00FE, &Chip8GenericEmulator::op00FE_withClear);
            on(0xFFFF, 0x00FF, &Chip8GenericEmulator::op00FF_withClear);
            on(0xF000, 0x3000, &Chip8GenericEmulator::op3xnn_with_F000);
            on(0xF000, 0x4000, &Chip8GenericEmulator::op4xnn_with_F000);
            on(0xF00F, 0x5000, &Chip8GenericEmulator::op5xy0_with_F000);
            on(0xF00F, 0x5002, &Chip8GenericEmulator::op5xy2);
            on(0xF00F, 0x5003, &Chip8GenericEmulator::op5xy3);
            on(0xF00F, 0x9000, &Chip8GenericEmulator::op9xy0_with_F000);
            on(0xF0FF, 0xE09E, &Chip8GenericEmulator::opEx9E_with_F000);
            on(0xF0FF, 0xE0A1, &Chip8GenericEmulator::opExA1_with_F000);
            on(0xFFFF, 0xF000, &Chip8GenericEmulator::opF000);
            on(0xF0FF, 0xF001, &Chip8GenericEmulator::opFx01);
            on(0xFFFF, 0xF002, &Chip8GenericEmulator::opF002);
            on(0xF0FF, 0xF030, &Chip8GenericEmulator::opFx30);
            on(0xF0FF, 0xF03A, &Chip8GenericEmulator::opFx3A);
            on(0xF0FF, 0xF075, &Chip8GenericEmulator::opFx75);
            on(0xF0FF, 0xF085, &Chip8GenericEmulator::opFx85);
            break;
        default: break;
    }
}

Chip8GenericEmulator::~Chip8GenericEmulator()
{
#ifdef GEN_OPCODE_STATS
    std::vector<std::pair<uint16_t,int64_t>> result;
    for(const auto& p : _opcodeStats)
        result.push_back(p);
    std::sort(result.begin(), result.end(), [](const auto& p1, const auto& p2){ return p1.second < p2.second;});
    std::cout << "Opcode statistics:" << std::endl;
    for(const auto& p : result) {
        std::cout << fmt::format("{:04X}: {}", p.first, p.second) << std::endl;
    }
#endif
}

int64_t Chip8GenericEmulator::executeFor(int64_t micros)
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        setExecMode(ePAUSED);
        return 0;
    }
    if(_options.instructionsPerFrame) {
        auto startTime = _cycleCounter;
        auto microsPerCycle = 1000000.0 / ((int64_t)_options.instructionsPerFrame * _options.frameRate);
        auto endCycles = startTime + int64_t(micros/microsPerCycle);
        auto nextFrame = calcNextFrame();
        while(_execMode != ePAUSED && nextFrame <= endCycles) {
            executeInstructions(nextFrame - _cycleCounter);
            if(_cycleCounter == nextFrame) {
                handleTimer();
                nextFrame += _options.instructionsPerFrame;
            }
        }
        while (_execMode != ePAUSED && _cycleCounter < endCycles) {
            executeInstruction();
        }
        auto excessTime = int64_t((endCycles - _cycleCounter) * microsPerCycle);
        return excessTime;// > 0 ? excessTime : 0;
    }
    else {
        using namespace std::chrono;
        handleTimer();
        auto start = _cycleCounter;
        auto endTime = steady_clock::now() + microseconds(micros > 2000 ? micros * 3 / 4 : 0);
        do {
            executeInstructions(487);
        }
        while(_execMode != ePAUSED && steady_clock::now() < endTime);
        uint32_t actualIPF = _cycleCounter - start;
        _systemTime.setFrequency((_systemTime.getClockFreq() + actualIPF)>>1);
    }
    return 0;
}

void Chip8GenericEmulator::executeFrame()
{
    if(!_options.instructionsPerFrame) {
        handleTimer();
        auto start = std::chrono::steady_clock::now();
        do {
            executeInstructions(4870);
        }
#ifdef PLATFORM_WEB
        while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < 12);
#else
        while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() < 16);
#endif
    }
    else {
        auto instructionsLeft = calcNextFrame() - _cycleCounter;
        if(instructionsLeft == _options.instructionsPerFrame) {
            handleTimer();
        }
        executeInstructions(instructionsLeft);
    }
}

void Chip8GenericEmulator::handleTimer()
{
    if(_execMode != ePAUSED) {
        ++_frameCounter;
        ++_randomSeed;
        _host.vblank();
#ifdef EMU_AUDIO_DEBUG
        std::clog << "handle-timer" << std::endl;
#endif
        if (_rDT > 0)
            --_rDT;
        if (_rST > 0)
            --_rST;
        if (!_rST)
            _wavePhase = 0;
        if(_screenNeedsUpdate) {
            _host.updateScreen();
            _screenNeedsUpdate = false;
        }
    }
}

inline void Chip8GenericEmulator::executeInstructionNoBreakpoints()
{
    uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
    ++_cycleCounter;
    _rPC = (_rPC + 2) & ADDRESS_MASK;
    (this->*_opcodeHandler[opcode])(opcode);
}

void Chip8GenericEmulator::executeInstructions(int numInstructions)
{
    if(_execMode == ePAUSED)
        return;
    auto start = _cycleCounter;
    if(_isMegaChipMode) {
        if(_execMode == eRUNNING) {
            auto end = _cycleCounter + numInstructions;
            while (_execMode == eRUNNING && _cycleCounter < end) {
                if (_breakpoints.empty() && !_options.traceLog)
                    Chip8GenericEmulator::executeInstructionNoBreakpoints();
                else
                    Chip8GenericEmulator::executeInstruction();
            }
        }
        else {
            for (int i = 0; i < numInstructions; ++i)
                Chip8GenericEmulator::executeInstruction();
        }
    }
    else if(_isInstantDxyn) {
        if(_execMode ==  eRUNNING && _breakpoints.empty() && !_options.traceLog) {
            for (int i = 0; i < numInstructions; ++i) {
                uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
                _rPC = (_rPC + 2) & ADDRESS_MASK;
#ifdef GEN_OPCODE_STATS
                if((opcode & 0xF000) == 0xD000)
                    _opcodeStats[opcode & 0xF00F]++;
                else {
                    const auto* info = _opcodeSet.getOpcodeInfo(opcode);
                    if(info)
                        _opcodeStats[info->opcode]++;
                }
#endif
                (this->*_opcodeHandler[opcode])(opcode);
                if(_cpuState == eWAIT) {
                    _cycleCounter += numInstructions - i;
                    break;
                }
                _cycleCounter++;
            }
            //_cycleCounter += numInstructions;
            //    Chip8GenericEmulator::executeInstructionNoBreakpoints();
        }
        else  {
            for (int i = 0; i < numInstructions; ++i)
                Chip8GenericEmulator::executeInstruction();
        }
    }
    else {
        for (int i = 0; i < numInstructions; ++i) {
            //if (i && (((_memory[_rPC] << 8) | _memory[_rPC + 1]) & 0xF000) == 0xD000) {
            //    _cycleCounter = calcNextFrame();
            //    _systemTime.addCycles(_cycleCounter - start);
            //    return;
            //}
            if(_execMode == eRUNNING && _breakpoints.empty() && !_options.traceLog)
                Chip8GenericEmulator::executeInstructionNoBreakpoints();
            else
                Chip8GenericEmulator::executeInstruction();
        }
    }
    _systemTime.addCycles(_cycleCounter - start);
}

int Chip8GenericEmulator::executeInstruction()
{
    auto startCycle = _cycleCounter;
    if(_execMode == eRUNNING) {
        if(_options.traceLog && _cpuState != eWAIT)
            Logger::log(Logger::eCHIP8, _cycleCounter, {_frameCounter, int(_cycleCounter % 9999)}, dumpStateLine().c_str());
        uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
        _rPC = (_rPC + 2) & ADDRESS_MASK;
#ifdef GEN_OPCODE_STATS
        if((opcode & 0xF000) == 0xD000)
            _opcodeStats[opcode & 0xF00F]++;
        else {
            const auto* info = _opcodeSet.getOpcodeInfo(opcode);
            if(info)
                _opcodeStats[info->opcode]++;
        }
#endif
        (this->*_opcodeHandler[opcode])(opcode);
        ++_cycleCounter;
    }
    else {
        if (_execMode == ePAUSED || _cpuState == eERROR)
            return static_cast<int>(_cycleCounter - startCycle);
        if(_options.traceLog)
            Logger::log(Logger::eCHIP8, _cycleCounter, {_frameCounter, int(_cycleCounter % 9999)}, dumpStateLine().c_str());
        uint16_t opcode = (_memory[_rPC] << 8) | _memory[_rPC + 1];
        _rPC = (_rPC + 2) & ADDRESS_MASK;
        (this->*_opcodeHandler[opcode])(opcode);
        ++_cycleCounter;
        if (_execMode == eSTEP || (_execMode == eSTEPOVER && _rSP <= _stepOverSP)) {
            _execMode = ePAUSED;
        }
    }
    if(tryTriggerBreakpoint(_rPC)) {
        _execMode = ePAUSED;
        _breakpointTriggered = true;
    }
    return static_cast<int>(_cycleCounter - startCycle);
}

uint8_t Chip8GenericEmulator::getNextMCSample()
{
    if(_isMegaChipMode && _sampleLength>0 && _execMode == eRUNNING) {
        auto val = _memory[(_sampleStart + uint32_t(_mcSamplePos)) & ADDRESS_MASK];
        double pos = _mcSamplePos + _sampleStep;
        if(pos >= _sampleLength) {
            if(_sampleLoop)
                pos -= _sampleLength;
            else
                pos = _sampleLength = 0;
        }
        _mcSamplePos = pos;
        return val;
    }
    return 128;
}

void Chip8GenericEmulator::on(uint16_t mask, uint16_t opcode, OpcodeHandler handler)
{
    uint16_t argMask = ~mask;
    int shift = 0;
    if(argMask) {
        while((argMask & 1) == 0) {
            argMask >>= 1;
            ++shift;
        }
        uint16_t val = 0;
        do {
            _opcodeHandler[opcode | ((val & argMask) << shift)] = handler;
        }
        while(++val & argMask);
    }
    else {
        _opcodeHandler[opcode] = handler;
    }
}

void Chip8GenericEmulator::opNop(uint16_t)
{
}

void Chip8GenericEmulator::opInvalid(uint16_t opcode)
{
    errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
}

void Chip8GenericEmulator::op0010(uint16_t opcode)
{
    _isMegaChipMode = false;
    _host.preClear();
    clearScreen();
    ++_clearCounter;
}

void Chip8GenericEmulator::op0011(uint16_t opcode)
{
    _isMegaChipMode = true;
    _host.preClear();
    clearScreen();
    ++_clearCounter;
}

void Chip8GenericEmulator::op00Bn(uint16_t opcode)
{ // Scroll UP
    auto n = (opcode & 0xf);
    if(_isMegaChipMode) {
        _screen.scrollUp(n);
        _screenRGBA->scrollUp(n);
        _host.updateScreen();
    }
    else {
        _screen.scrollUp(_isHires || _options.optHalfPixelScroll ? n : (n<<1));
        _screenNeedsUpdate = true;
    }

}

void Chip8GenericEmulator::op00Cn(uint16_t opcode)
{ // Scroll DOWN
    auto n = (opcode & 0xf);
    if(_isMegaChipMode) {
        _screen.scrollDown(n);
        _screenRGBA->scrollDown(n);
        _host.updateScreen();
    }
    else {
        _screen.scrollDown(_isHires || _options.optHalfPixelScroll ? n : (n<<1));
        _screenNeedsUpdate = true;
    }
}

void Chip8GenericEmulator::op00Cn_masked(uint16_t opcode)
{ // Scroll DOWN masked
    auto n = (opcode & 0xf);
    if(!_isHires) n <<= 1;
    auto width = getCurrentScreenWidth();
    auto height = getCurrentScreenHeight();
    for(int sy = height - n - 1; sy >= 0; --sy) {
        for(int sx = 0; sx < width; ++sx) {
            _screen.movePixelMasked(sx, sy, sx, sy + n, _planes);
        }
    }
    for(int sy = 0; sy < n; ++sy) {
        for(int sx = 0; sx < width; ++sx) {
            _screen.clearPixelMasked(sx, sy, _planes);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8GenericEmulator::op00Dn(uint16_t opcode)
{ // Scroll UP
    auto n = (opcode & 0xf);
    _screen.scrollUp(_isHires || _options.optHalfPixelScroll ? n : (n<<1));
    _screenNeedsUpdate = true;
}

void Chip8GenericEmulator::op00Dn_masked(uint16_t opcode)
{ // Scroll UP masked
    auto n = (opcode & 0xf);
    if(!_isHires) n <<= 1;
    auto width = getCurrentScreenWidth();
    auto height = getCurrentScreenHeight();
    for(int sy = n; sy < height; ++sy) {
        for(int sx = 0; sx < width; ++sx) {
            _screen.movePixelMasked(sx, sy, sx, sy - n, _planes);
        }
    }
    for(int sy = height - n; sy < height; ++sy) {
        for(int sx = 0; sx < width; ++sx) {
            _screen.clearPixelMasked(sx, sy, _planes);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8GenericEmulator::op00E0(uint16_t opcode)
{
    _host.preClear();
    clearScreen();
    _screenNeedsUpdate = true;
    ++_clearCounter;
}

void Chip8GenericEmulator::op00E0_megachip(uint16_t opcode)
{
    _host.preClear();
    swapMegaSchreens();
    _host.updateScreen();
    clearScreen();
    ++_clearCounter;
    _cycleCounter = calcNextFrame() - 1;
}

void Chip8GenericEmulator::op00ED_c8e(uint16_t opcode)
{
    halt();
}

void Chip8GenericEmulator::op00EE(uint16_t opcode)
{
    if(!_rSP)
        errorHalt("STACK UNDERFLOW");
    else {
        _rPC = _stack[--_rSP];
        if (_execMode == eSTEPOUT)
            _execMode = ePAUSED;
    }
}

void Chip8GenericEmulator::op00EE_cyclic(uint16_t opcode)
{
    _rPC = _stack[(--_rSP)&0xF];
    if (_execMode == eSTEPOUT)
        _execMode = ePAUSED;
}

void Chip8GenericEmulator::op00FB(uint16_t opcode)
{ // Scroll right 4 pixel
    if(_isMegaChipMode) {
        _screen.scrollRight(4);
        _screenRGBA->scrollRight(4);
        _host.updateScreen();
    }
    else {
        _screen.scrollRight(_isHires || _options.optHalfPixelScroll ? 4 : 8);
        _screenNeedsUpdate = true;
    }
}

void Chip8GenericEmulator::op00FB_masked(uint16_t opcode)
{ // Scroll right 4 pixel masked
    auto n = 4;
    if(!_isHires) n <<= 1;
    auto width = getCurrentScreenWidth();
    auto height = getCurrentScreenHeight();
    for(int sy = 0; sy < height; ++sy) {
        for(int sx = width - n - 1; sx >= 0; --sx) {
            _screen.movePixelMasked(sx, sy, sx + n, sy, _planes);
        }
        for(int sx = 0; sx < n; ++sx) {
            _screen.clearPixelMasked(sx, sy, _planes);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8GenericEmulator::op00FC(uint16_t opcode)
{ // Scroll left 4 pixel
    if(_isMegaChipMode) {
        _screen.scrollLeft(4);
        _screenRGBA->scrollLeft(4);
        _host.updateScreen();
    }
    else {
        _screen.scrollLeft(_isHires || _options.optHalfPixelScroll ? 4 : 8);
       _screenNeedsUpdate = true;
    }
}

void Chip8GenericEmulator::op00FC_masked(uint16_t opcode)
{ // Scroll left 4 pixels masked
    auto n = 4;
    if(!_isHires) n <<= 1;
    auto width = getCurrentScreenWidth();
    auto height = getCurrentScreenHeight();
    for(int sy = 0; sy < height; ++sy) {
        for(int sx = n; sx < width; ++sx) {
            _screen.movePixelMasked(sx, sy, sx - n, sy, _planes);
        }
        for(int sx = width - n; sx < width; ++sx) {
            _screen.clearPixelMasked(sx, sy, _planes);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8GenericEmulator::op00FD(uint16_t opcode)
{
    halt();
}

void Chip8GenericEmulator::op00FE(uint16_t opcode)
{
    _host.preClear();
    _isHires = false;
    _isInstantDxyn = _options.optInstantDxyn;
}

void Chip8GenericEmulator::op00FE_withClear(uint16_t opcode)
{
    _host.preClear();
    _isHires = false;
    _isInstantDxyn = _options.optInstantDxyn;
    _screen.setAll(0);
    _screenNeedsUpdate = true;
    ++_clearCounter;
}

void Chip8GenericEmulator::op00FE_megachip(uint16_t opcode)
{
    if(_isHires && !_isMegaChipMode) {
        _host.preClear();
        _isHires = false;
        _isInstantDxyn = _options.optInstantDxyn;
        clearScreen();
        _screenNeedsUpdate = true;
        ++_clearCounter;
    }
}

void Chip8GenericEmulator::op00FF(uint16_t opcode)
{
    _host.preClear();
    _isHires = true;
    _isInstantDxyn = true;
}

void Chip8GenericEmulator::op00FF_withClear(uint16_t opcode)
{
    _host.preClear();
    _isHires = true;
    _isInstantDxyn = true;
    _screen.setAll(0);
    _screenNeedsUpdate = true;
    ++_clearCounter;
}

void Chip8GenericEmulator::op00FF_megachip(uint16_t opcode)
{
    if(!_isHires && !_isMegaChipMode) {
        _host.preClear();
        _isHires = true;
        _isInstantDxyn = true;
        clearScreen();
        _screenNeedsUpdate = true;
        ++_clearCounter;
    }
}

void Chip8GenericEmulator::op0151_c8e(uint16_t opcode)
{
    if(_rDT) {
        _rPC -= 2;
        _cpuState = eWAIT;
    }
    else {
        _cpuState = eNORMAL;
    }
}

void Chip8GenericEmulator::op0188_c8e(uint16_t opcode)
{
    _rPC = (_rPC + 2) & ADDRESS_MASK;
}

void Chip8GenericEmulator::op01nn(uint16_t opcode)
{
    _rI = ((opcode & 0xFF) << 16 | (_memory[_rPC & ADDRESS_MASK] << 8) | _memory[(_rPC + 1) & ADDRESS_MASK]) & ADDRESS_MASK;
    _rPC = (_rPC + 2) & ADDRESS_MASK;
}

void Chip8GenericEmulator::op02A0_c8x(uint16_t opcode)
{
    _chip8xBackgroundColor = (_chip8xBackgroundColor + 1) & 3;
    _screen.setBackgroundPal(_chip8xBackgroundColor);
    _screenNeedsUpdate = true;
}

void Chip8GenericEmulator::op02nn(uint16_t opcode)
{
    auto numCols = opcode & 0xFF;
    std::vector<uint32_t> cols;
    cols.reserve(256);
    size_t address = _rI;
    for(size_t i = 0; i < numCols; ++i) {
        auto a = _memory[address++ & ADDRESS_MASK];
        auto r = _memory[address++ & ADDRESS_MASK];
        auto g = _memory[address++ & ADDRESS_MASK];
        auto b = _memory[address++ & ADDRESS_MASK];
        _mcPalette[i + 1] = be32((r << 24) | (g << 16) | (b << 8) | a);
        cols.push_back(be32((r << 24) | (g << 16) | (b << 8) | a));
    }
    _host.updatePalette(cols, 1);
}

void Chip8GenericEmulator::op03nn(uint16_t opcode)
{
    _spriteWidth = opcode & 0xFF;
    if(!_spriteWidth)
        _spriteWidth = 256;
}

void Chip8GenericEmulator::op04nn(uint16_t opcode)
{
    _spriteHeight = opcode & 0xFF;
    if(!_spriteHeight)
        _spriteHeight = 256;
}

void Chip8GenericEmulator::op05nn(uint16_t opcode)
{
    _screenAlpha = opcode & 0xFF;
}

void Chip8GenericEmulator::op060n(uint16_t opcode)
{
    uint32_t frequency = (_memory[_rI & ADDRESS_MASK] << 8) | _memory[(_rI + 1) & ADDRESS_MASK];
    uint32_t length = (_memory[(_rI + 2) & ADDRESS_MASK] << 16) | (_memory[(_rI + 3) & ADDRESS_MASK] << 8) | _memory[(_rI + 4) & ADDRESS_MASK];
    _sampleStart = _rI + 6;
    _sampleStep = frequency / 44100.0f;
    _sampleLength = length;;
    _sampleLoop = (opcode & 0xf) == 0;
    _mcSamplePos = 0;
}

void Chip8GenericEmulator::op0700(uint16_t opcode)
{
    _sampleLength = 0;
    _mcSamplePos = 0;
}

void Chip8GenericEmulator::op080n(uint16_t opcode)
{
    auto bm = opcode & 0xF;
    _blendMode = bm < 6 ? MegaChipBlendMode(bm) : eBLEND_NORMAL;
}

void Chip8GenericEmulator::op09nn(uint16_t opcode)
{
    _collisionColor = opcode & 0xFF;
}

#ifdef ALIEN_INV8SION_BENCH
std::chrono::time_point<std::chrono::steady_clock> loopStart{};
int64_t loopCycles = 0;
int64_t maxCycles = 0;
int64_t minLoop = 1000000000;
int64_t maxLoop = 0;
double avgLoop = 0;
#endif

void Chip8GenericEmulator::op1nnn(uint16_t opcode)
{
    if((opcode & 0xFFF) == _rPC - 2)
        _execMode = ePAUSED;
    _rPC = opcode & 0xFFF;
#ifdef ALIEN_INV8SION_BENCH
    if(_rPC == 0x212) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - loopStart).count();
        avgLoop = (avgLoop * 63 + duration) / 64.0;
        if(minLoop > duration) minLoop = duration;
        if(maxLoop < duration) maxLoop = duration;
        auto lc = _cycleCounter - loopCycles;
        if(lc > maxCycles)
            maxCycles = lc;
        std::cout << "Loop: " <<  duration << "us (min: " << minLoop << "us, avg: " << avgLoop << "us, max:" << maxLoop << "us), " << (_cycleCounter - loopCycles) << " cycles, max: " << maxCycles << ", ips: " << (lc * 1000000 / duration) << std::endl;
    }
#endif
}

void Chip8GenericEmulator::op2nnn(uint16_t opcode)
{
    if(_rSP == 16)
        errorHalt("STACK OVERFLOW");
    else {
        _stack[_rSP++] = _rPC;
        _rPC = opcode & 0xFFF;
    }
}

void Chip8GenericEmulator::op2nnn_cyclic(uint16_t opcode)
{
    _stack[(_rSP++)&0xF] = _rPC;
    _rPC = opcode & 0xFFF;
}

void Chip8GenericEmulator::op3xnn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == (opcode & 0xff)) {
        _rPC += 2;
    }
}

#define CONDITIONAL_SKIP_DISTANCE(ifOpcode,mask) ((_memory[_rPC]&(mask>>8)) == (ifOpcode>>8) && (_memory[(_rPC + 1)]&(mask&0xff)) == (ifOpcode&0xff) ? 4 : 2)

void Chip8GenericEmulator::op3xnn_with_F000(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == (opcode & 0xff)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000,0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::op3xnn_with_01nn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == (opcode & 0xff)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100,0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::op4xnn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != (opcode & 0xFF)) {
        _rPC += 2;
    }
}

void Chip8GenericEmulator::op4xnn_with_F000(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != (opcode & 0xFF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000, 0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::op4xnn_with_01nn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != (opcode & 0xFF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100, 0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::op5xy0(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == _rV[(opcode >> 4) & 0xF]) {
        _rPC += 2;
    }
}

void Chip8GenericEmulator::op5xy0_with_F000(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == _rV[(opcode >> 4) & 0xF]) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000, 0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::op5xy0_with_01nn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] == _rV[(opcode >> 4) & 0xF]) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100, 0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::op5xy1_c8e(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] > _rV[(opcode >> 4) & 0xF]) {
        _rPC = (_rPC + 2) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::op5xy1_c8x(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = ((_rV[(opcode >> 8) & 0xF] & 0x77) + (_rV[(opcode >> 4) & 0xF] & 0x77)) & 0x77;
}

void Chip8GenericEmulator::op5xy2(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    auto l = std::abs(x-y);
    for(int i=0; i <= l; ++i)
        write(_rI + i, _rV[x < y ? x + i : x - i]);
}

void Chip8GenericEmulator::op5xy2_c8e(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    if(x < y) {
        auto l = y - x;
        for(int i=0; i <= l; ++i)
            write(_rI + i, _rV[x + i]);
        _rI = (_rI + l + 1) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::op5xy3(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    for(int i=0; i <= std::abs(x-y); ++i)
        _rV[x < y ? x + i : x - i] = read(_rI + i);
}

void Chip8GenericEmulator::op5xy3_c8e(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    if(x < y) {
        auto l = y - x;
        for(int i=0; i <= l; ++i)
            _rV[x + i] = read(_rI + i);
        _rI = (_rI + l + 1) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::op5xy4(uint16_t opcode)
{
    auto x = (opcode >> 8) & 0xF;
    auto y = (opcode >> 4) & 0xF;
    for(int i=0; i <= std::abs(x-y); ++i)
        _options.palette.colors[x < y ? x + i : x - i] = Palette::Color::fromRGB(rgb332To888(_memory[(_rI + i) & 0xFFFF]));
    _screen.setPalette(_options.palette);
}

void Chip8GenericEmulator::op6xnn(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = opcode & 0xFF;
}

void Chip8GenericEmulator::op7xnn(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] += opcode & 0xFF;
}

void Chip8GenericEmulator::op8xy0(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF];
}

void Chip8GenericEmulator::op8xy1(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] |= _rV[(opcode >> 4) & 0xF];
    _rV[0xF] = 0;
}

void Chip8GenericEmulator::op8xy1_dontResetVf(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] |= _rV[(opcode >> 4) & 0xF];
}

void Chip8GenericEmulator::op8xy2(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] &= _rV[(opcode >> 4) & 0xF];
    _rV[0xF] = 0;
}

void Chip8GenericEmulator::op8xy2_dontResetVf(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] &= _rV[(opcode >> 4) & 0xF];
}

void Chip8GenericEmulator::op8xy3(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] ^= _rV[(opcode >> 4) & 0xF];
    _rV[0xF] = 0;
}

void Chip8GenericEmulator::op8xy3_dontResetVf(uint16_t opcode)
{   
    _rV[(opcode >> 8) & 0xF] ^= _rV[(opcode >> 4) & 0xF];
}

void Chip8GenericEmulator::op8xy4(uint16_t opcode)
{   
    uint16_t result = _rV[(opcode >> 8) & 0xF] + _rV[(opcode >> 4) & 0xF];
    _rV[(opcode >> 8) & 0xF] = result;
    _rV[0xF] = result>>8;
}

void Chip8GenericEmulator::op8xy5(uint16_t opcode)
{   
    uint16_t result = _rV[(opcode >> 8) & 0xF] - _rV[(opcode >> 4) & 0xF];
    _rV[(opcode >> 8) & 0xF] = result;
    _rV[0xF] = result > 255 ? 0 : 1;
}

void Chip8GenericEmulator::op8xy6(uint16_t opcode)
{   
    uint8_t carry = _rV[(opcode >> 4) & 0xF] & 1;
    _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] >> 1;
    _rV[0xF] = carry;
}

void Chip8GenericEmulator::op8xy6_justShiftVx(uint16_t opcode)
{   
    uint8_t carry = _rV[(opcode >> 8) & 0xF] & 1;
    _rV[(opcode >> 8) & 0xF] >>= 1;
    _rV[0xF] = carry;
}

void Chip8GenericEmulator::op8xy7(uint16_t opcode)
{   
    uint16_t result = _rV[(opcode >> 4) & 0xF] - _rV[(opcode >> 8) & 0xF];
    _rV[(opcode >> 8) & 0xF] = result;
    _rV[0xF] = result > 255 ? 0 : 1;
}

void Chip8GenericEmulator::op8xyE(uint16_t opcode)
{
#if 1
    uint8_t carry = _rV[(opcode >> 4) & 0xF] >> 7;
    _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] << 1;
    _rV[0xF] = carry;
#else
    _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] << 1;
    _rV[0xF] = _rV[(opcode >> 4) & 0xF] >> 7;
#endif
}

void Chip8GenericEmulator::op8xyE_justShiftVx(uint16_t opcode)
{   
    uint8_t carry = _rV[(opcode >> 8) & 0xF] >> 7;
    _rV[(opcode >> 8) & 0xF] <<= 1;
    _rV[0xF] = carry;
}

void Chip8GenericEmulator::op9xy0(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != _rV[(opcode >> 4) & 0xF]) {
        _rPC += 2;
    }
}

void Chip8GenericEmulator::op9xy0_with_F000(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != _rV[(opcode >> 4) & 0xF]) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000, 0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::op9xy0_with_01nn(uint16_t opcode)
{
    if (_rV[(opcode >> 8) & 0xF] != _rV[(opcode >> 4) & 0xF]) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100, 0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::opAnnn(uint16_t opcode)
{
    _rI = opcode & 0xFFF;
}

void Chip8GenericEmulator::opBBnn_c8e(uint16_t opcode)
{
    _rPC = (_rPC - 2 - (opcode & 0xff)) & ADDRESS_MASK;
}

void Chip8GenericEmulator::opBFnn_c8e(uint16_t opcode)
{
    _rPC = (_rPC - 2 + (opcode & 0xff)) & ADDRESS_MASK;
}

void Chip8GenericEmulator::opBxy0_c8x(uint16_t opcode)
{
    auto rx = _rV[(opcode >> 8) & 0xF];
    auto ry = _rV[((opcode >> 8) & 0xF) + 1];
    auto xPos = rx & 0xF;
    auto width = rx >> 4;
    auto yPos = ry & 0xF;
    auto height = ry >> 4;
    auto col = _rV[(opcode >> 4) & 0xF] & 7;
    _screen.setOverlayCellHeight(4);
    for(int y = 0; y <= height; ++y) {
        for(int x = 0; x <= width; ++x) {
            _screen.setOverlayCell(xPos + x, yPos + y, col);
        }
    }
    _screenNeedsUpdate = true;
}

void Chip8GenericEmulator::opBxyn_c8x(uint16_t opcode)
{
    auto rx = _rV[(opcode >> 8) & 0xF];
    auto ry = _rV[((opcode >> 8) & 0xF) + 1];
    auto xPos = (rx >> 3) & 7;
    auto yPos = ry & 0x1F;
    auto height = opcode & 0xF;
    auto col = _rV[(opcode >> 4) & 0xF] & 7;
    _screen.setOverlayCellHeight(1);
    for(int y = 0; y < height; ++y) {
        _screen.setOverlayCell(xPos, yPos + y, col);
    }
    _screenNeedsUpdate = true;
}

void Chip8GenericEmulator::opBnnn(uint16_t opcode)
{
    _rPC = (_rV[0] + (opcode & 0xFFF)) & ADDRESS_MASK;
}

void Chip8GenericEmulator::opBxnn(uint16_t opcode)
{
    _rPC = (_rV[(opcode >> 8) & 0xF] + (opcode & 0xFFF)) & ADDRESS_MASK;
}

inline uint8_t classicRand(uint32_t& state)
{
    state = ((state * 1103515245) + 12345) & 0x7FFFFFFF;
    return state >> 16;
}

inline uint8_t countingRand(uint32_t& state)
{
    return state++;
}

void Chip8GenericEmulator::opCxnn(uint16_t opcode)
{
    if(_options.behaviorBase < emu::Chip8GenericOptions::eSCHIP10) {
        ++_randomSeed;
        uint16_t val = _randomSeed >> 8;
        val += _chip8_cvip[0x100 + (_randomSeed & 0xFF)];
        uint8_t result = val;
        val >>= 1;
        val += result;
        _randomSeed = (_randomSeed & 0xFF) | (val << 8);
        result = val & (opcode & 0xFF);
        _rV[(opcode >> 8) & 0xF] = result;
    }
    else {
        _rV[(opcode >> 8) & 0xF] = (rand() >> 4) & (opcode & 0xFF);
    }
}

void Chip8GenericEmulator::opCxnn_randLCG(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = classicRand(_simpleRandState) & (opcode & 0xFF);
}

void Chip8GenericEmulator::opCxnn_counting(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = countingRand(_simpleRandState) & (opcode & 0xFF);
}

static void blendColorsAlpha(uint32_t* dest, const uint32_t* col, uint8_t alpha)
{
    int a = alpha;
    auto* dst = (uint8_t*)dest;
    const auto* c1 = dst;
    const auto* c2 = (const uint8_t*)col;
    *dst++ = (a * *c2++ + (255 - a) * *c1++) >> 8;
    *dst++ = (a * *c2++ + (255 - a) * *c1++) >> 8;
    *dst++ = (a * *c2 + (255 - a) * *c1) >> 8;
    *dst = 255;
}

static void blendColorsAdd(uint32_t* dest, const uint32_t* col)
{
    auto* dst = (uint8_t*)dest;
    const auto* c1 = dst;
    const auto* c2 = (const uint8_t*)col;
    *dst++ = std::min((int)*c1++ + *c2++, 255);
    *dst++ = std::min((int)*c1++ + *c2++, 255);
    *dst++ = std::min((int)*c1 + *c2, 255);
    *dst = 255;
}

static void blendColorsMul(uint32_t* dest, const uint32_t* col)
{
    auto* dst = (uint8_t*)dest;
    const auto* c1 = dst;
    const auto* c2 = (const uint8_t*)col;
    *dst++ = (int)*c1++ * *c2++ / 255;
    *dst++ = (int)*c1++ * *c2++ / 255;
    *dst++ = (int)*c1 * *c2 / 255;
    *dst = 255;
}

void Chip8GenericEmulator::opDxyn_megaChip(uint16_t opcode)
{
    if(!_isMegaChipMode)
        opDxyn<HiresSupport>(opcode);
    else {
        int xpos = _rV[(opcode >> 8) & 0xF];
        int ypos = _rV[(opcode >> 4) & 0xF];
        _rV[0xF] = 0;
        if(_rI < 0x100) {
            int lines = opcode & 0xf;
            auto byteOffset = _rI;
            for (int l = 0; l < lines && ypos + l < 192; ++l) {
                auto value = _memory[byteOffset++];
                for (unsigned b = 0; b < 8 && xpos + b < 256 && value; ++b, value <<= 1) {
                    if (value & 0x80) {
                        uint8_t* pixelBuffer = &_screen.getPixelRef(xpos + b, ypos + l);
                        uint32_t* pixelBuffer32 = &_workRGBA->getPixelRef(xpos + b, ypos + l);
                        if (*pixelBuffer) {
                            _rV[0xf] = 1;
                            *pixelBuffer = 0;
                            *pixelBuffer32 = 0;
                        }
                        else {
                            *pixelBuffer = 255;
                            *pixelBuffer32 = 0xffffffff;
                        }
                    }
                }
            }
        }
        else {
            for (int y = 0; y < _spriteHeight; ++y) {
                int yy = ypos + y;
                if(_options.optWrapSprites) {
                    yy = (uint8_t)yy;
                    if(yy >= 192)
                        continue;
                }
                else {
                    if(yy >= 192)
                        break;
                }
                uint8_t* pixelBuffer = &_screen.getPixelRef(xpos, yy);
                uint32_t* pixelBuffer32 = &_workRGBA->getPixelRef(xpos, yy);
                for (int x = 0; x < _spriteWidth; ++x, ++pixelBuffer, ++pixelBuffer32) {
                    int xx = xpos + x;
                    if(xx > 255) {
                        if(_options.optWrapSprites) {
                            xx &= 0xff;
                            pixelBuffer = &_screen.getPixelRef(xx, yy);
                            pixelBuffer32 = &_workRGBA->getPixelRef(xx, yy);
                        }
                        else {
                            continue;
                        }
                    }
                    auto col = _memory[_rI + y * _spriteWidth + x];
                    if (col) {
                        if (*pixelBuffer == _collisionColor)
                            _rV[0xF] = 1;
                        *pixelBuffer = col;
                        switch (_blendMode) {
                            case eBLEND_ALPHA_25:
                                blendColorsAlpha(pixelBuffer32, &_mcPalette[col], 63);
                                break;
                            case eBLEND_ALPHA_50:
                                blendColorsAlpha(pixelBuffer32, &_mcPalette[col], 127);
                                break;
                            case eBLEND_ALPHA_75:
                                blendColorsAlpha(pixelBuffer32, &_mcPalette[col], 191);
                                break;
                            case eBLEND_ADD:
                                blendColorsAdd(pixelBuffer32, &_mcPalette[col]);
                                break;
                            case eBLEND_MUL:
                                blendColorsMul(pixelBuffer32, &_mcPalette[col]);
                                break;
                            case eBLEND_NORMAL:
                            default:
                                *pixelBuffer32 = _mcPalette[col];
                                break;
                        }
                    }
                }
            }
        }
    }
}

void Chip8GenericEmulator::opEx9E(uint16_t opcode)
{
    if (_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC += 2;
    }
}

void Chip8GenericEmulator::opEx9E_with_F000(uint16_t opcode)
{
    if (_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000, 0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::opEx9E_with_01nn(uint16_t opcode)
{
    if (_host.isKeyDown(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100, 0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::opExA1(uint16_t opcode)
{
    if (_host.isKeyUp(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC += 2;
    }
}

void Chip8GenericEmulator::opExA1_with_F000(uint16_t opcode)
{
    if (_host.isKeyUp(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0xF000, 0xFFFF)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::opExA1_with_01nn(uint16_t opcode)
{
    if (_host.isKeyUp(_rV[(opcode >> 8) & 0xF] & 0xF)) {
        _rPC = (_rPC + CONDITIONAL_SKIP_DISTANCE(0x0100, 0xFF00)) & ADDRESS_MASK;
    }
}

void Chip8GenericEmulator::opExF2_c8x(uint16_t opcode)
{
    // still nop
}

void Chip8GenericEmulator::opExF5_c8x(uint16_t opcode)
{
    _rPC += 2;
}

void Chip8GenericEmulator::opF000(uint16_t opcode)
{
    _rI = ((_memory[_rPC & ADDRESS_MASK] << 8) | _memory[(_rPC + 1) & ADDRESS_MASK]) & ADDRESS_MASK;
    _rPC = (_rPC + 2) & ADDRESS_MASK;
}

void Chip8GenericEmulator::opFx01(uint16_t opcode)
{
    _planes = (opcode >> 8) & 0xF;
}

void Chip8GenericEmulator::opF002(uint16_t opcode)
{
    uint8_t anyBit = 0;
#ifndef EMU_AUDIO_DEBUG
    for(int i = 0; i < 16; ++i) {
        _xoAudioPattern[i] = _memory[(_rI + i) & ADDRESS_MASK];
        anyBit |= _xoAudioPattern[i];
    }
#else
    std::clog << "pattern: ";
    for(int i = 0; i < 16; ++i) {
        _xoAudioPattern[i] = _memory[(_rI + i) & ADDRESS_MASK];
        anyBit |= _xoAudioPattern[i];
        std::clog << (i ? ", " : "") << fmt::format("0x{:02x}", _xoAudioPattern[i]);
    }
    std::clog << std::endl;
#endif
    _xoSilencePattern = anyBit != 0;
}

void Chip8GenericEmulator::opFx07(uint16_t opcode)
{
    _rV[(opcode >> 8) & 0xF] = _rDT;
#ifdef ALIEN_INV8SION_BENCH
    if(!_rDT) {
        loopCycles = _cycleCounter;
        loopStart = std::chrono::steady_clock::now();
    }
#endif
}

void Chip8GenericEmulator::opFx0A(uint16_t opcode)
{
    auto key = _host.getKeyPressed();
    if (key > 0) {
        _rV[(opcode >> 8) & 0xF] = key - 1;
        _cpuState = eNORMAL;
    }
    else {
        // keep waiting...
        _rPC -= 2;
        if(key < 0)
            _rST = 4;
        //--_cycleCounter;
        if(_isMegaChipMode && _cpuState != eWAIT)
            _host.updateScreen();
        _cpuState = eWAIT;
    }
}

void Chip8GenericEmulator::opFx15(uint16_t opcode)
{
    _rDT = _rV[(opcode >> 8) & 0xF];
}

void Chip8GenericEmulator::opFx18(uint16_t opcode)
{
    _rST = _rV[(opcode >> 8) & 0xF];
    if(!_rST) _wavePhase = 0;
#ifdef EMU_AUDIO_DEBUG
    std::clog << fmt::format("st := {}", (int)_rST) << std::endl;
#endif
}

void Chip8GenericEmulator::opFx1B_c8e(uint16_t opcode)
{
    _rPC = (_rPC + _rV[(opcode >> 8) & 0xF]) & ADDRESS_MASK;
}

void Chip8GenericEmulator::opFx1E(uint16_t opcode)
{
    _rI = (_rI + _rV[(opcode >> 8) & 0xF]) & ADDRESS_MASK;
}

void Chip8GenericEmulator::opFx29(uint16_t opcode)
{
    _rI = (_rV[(opcode >> 8) & 0xF] & 0xF) * 5;
}

void Chip8GenericEmulator::opFx29_ship10Beta(uint16_t opcode)
{
    auto n = _rV[(opcode >> 8) & 0xF];
    _rI = (n >= 10 && n <=19) ? (n-10) * 10 + 16*5 : (n & 0xF) * 5;
}

void Chip8GenericEmulator::opFx30(uint16_t opcode)
{
    _rI = (_rV[(opcode >> 8) & 0xF] & 0xF) * 10 + 16*5;
}

void Chip8GenericEmulator::opFx33(uint16_t opcode)
{
    uint8_t val = _rV[(opcode >> 8) & 0xF];
    write(_rI, val / 100);
    write(_rI + 1, (val / 10) % 10);
    write(_rI + 2, val % 10);
}

void Chip8GenericEmulator::opFx3A(uint16_t opcode)
{
    _xoPitch = _rV[(opcode >> 8) & 0xF];
#ifdef EMU_AUDIO_DEBUG
    std::clog << "pitch: " << (int)_xoPitch.load() << std::endl;
#endif
}

void Chip8GenericEmulator::opFx4F_c8e(uint16_t opcode)
{
    if(_cpuState != eWAIT) {
        _rDT = _rV[(opcode >> 8) & 0xF];
        _cpuState = eWAIT;
    }
    if(_rDT && (_cpuState == eWAIT)) {
        _rPC -= 2;
    }
    else {
        _cpuState = eNORMAL;
    }
}

void Chip8GenericEmulator::opFx55(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        write(_rI + i, _rV[i]);
    }
    _rI = (_rI + upto + 1) & ADDRESS_MASK;
}

void Chip8GenericEmulator::opFx55_loadStoreIncIByX(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        write(_rI + i, _rV[i]);
    }
    _rI = (_rI + upto) & ADDRESS_MASK;
}

void Chip8GenericEmulator::opFx55_loadStoreDontIncI(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        write(_rI + i, _rV[i]);
    }
}

void Chip8GenericEmulator::opFx65(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = read(_rI + i);
    }
    _rI = (_rI + upto + 1) & ADDRESS_MASK;
}

void Chip8GenericEmulator::opFx65_loadStoreIncIByX(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = read(_rI + i);
    }
    _rI = (_rI + upto) & ADDRESS_MASK;
}

void Chip8GenericEmulator::opFx65_loadStoreDontIncI(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = read(_rI + i);
    }
}

static uint8_t registerSpace[16]{};

void Chip8GenericEmulator::opFx75(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        registerSpace[i] = _rV[i];
    }
}

void Chip8GenericEmulator::opFx85(uint16_t opcode)
{
    uint8_t upto = (opcode >> 8) & 0xF;
    for (int i = 0; i <= upto; ++i) {
        _rV[i] = registerSpace[i];
    }
}

void Chip8GenericEmulator::opFxF8_c8x(uint16_t opcode)
{
    uint8_t val = _rV[(opcode >> 8) & 0xF];
    _vp595Frequency = val ? val : 0x80; // Emulate VP-595 using a CD4002 and a CD4011 to force 0x80 into the CDP1863 latch when 0 is written.
}

void Chip8GenericEmulator::opFxFB_c8x(uint16_t opcode)
{
    // still nop
}

static uint16_t g_hp48Wave[] = {
    0x99,   0x4cd,  0x2df,  0xfbc3, 0xf1e3, 0xe747, 0xddef, 0xd866, 0xda5c, 0xdef1, 0xe38e, 0xe664, 0xe9eb, 0xefd3, 0xf1fe, 0xf03a, 0xef66, 0xf1aa, 0xf7d1, 0x13a,  0xadd,  0x102d, 0xe8d,  0xb72,  0xa58,  0xe80,  0x17af, 0x21d1, 0x2718, 0x2245, 0x15f3,
    0x5a0,  0xfc82, 0xfef5, 0x6f7,  0xd5f,  0xac7,  0xfe89, 0xef7c, 0xe961, 0xef4e, 0xfba7, 0x440,  0x452,  0xfc8a, 0xf099, 0xe958, 0xeceb, 0xf959, 0x6f3,  0xcfd,  0x92f,  0x3c8,  0x2cd,  0x733,  0xd94,  0x12f0, 0x1531, 0x1147, 0x73d,  0xfbaf, 0xf3fb,
    0xf2e5, 0xf8d1, 0x2e,   0x3fb,  0x25c,  0xfc35, 0xf222, 0xe88f, 0xe260, 0xdf64, 0xe0f0, 0xe306, 0xe5e6, 0xe965, 0xed55, 0xf203, 0xf662, 0xfb37, 0x12c,  0x926,  0xf66,  0x10ac, 0xdd5,  0xa2b,  0xb84,  0x13b6, 0x1fe4, 0x2bef, 0x3168, 0x2dfc, 0x2380,
    0x1859, 0x1368, 0x14d1, 0x18ab, 0x190d, 0x141f, 0xa63,  0xfd36, 0xee1f, 0xe39e, 0xe201, 0xe4dc, 0xe7dd, 0xe748, 0xe452, 0xde58, 0xd77d, 0xd3e4, 0xd695, 0xde34, 0xe593, 0xec3e, 0xf229, 0xf714, 0xf841, 0xf93b, 0xfcdd, 0x671,  0x1661, 0x24fb, 0x2c00,
    0x27ce, 0x1dcb, 0x11bb, 0xb89,  0xfc6,  0x1991, 0x219c, 0x1fa7, 0x132d, 0x278,  0xf9df, 0xfd50, 0x566,  0x8c5,  0x33f,  0xf846, 0xeb34, 0xe28b, 0xe365, 0xeda5, 0xfb18, 0x1b3,  0xfe67, 0xf754, 0xf34f, 0xf63e, 0xff4c, 0x997,  0xea5,  0xb0c,  0x247,
    0xf98f, 0xf5af, 0xf914, 0x2e8,  0xd0b,  0x10ab, 0xbab,  0x145,  0xf7db, 0xf1ab, 0xedf7, 0xec64, 0xebb5, 0xea7b, 0xea61, 0xeb9b, 0xebad, 0xea86, 0xec28, 0xf2c9, 0xfc97, 0x688,  0xb10,  0x80e,  0xfff8, 0xfa73, 0xfd43, 0xa97,  0x20a1, 0x3393, 0x3a6d,
    0x3376, 0x256e, 0x1b72, 0x1a9f, 0x200a, 0x2470, 0x23bc, 0x1c60, 0x1091, 0x45,   0xee38, 0xe370, 0xe2d0, 0xe694, 0xe851, 0xe591, 0xdf8c, 0xd829, 0xd063, 0xcc6c, 0xcf8e, 0xd7ed, 0xdf45, 0xe306, 0xe752, 0xed90, 0xf362, 0xf85d, 0xfed5, 0x8df,  0x17dd,
    0x2691, 0x2daa, 0x2a67, 0x2132, 0x1755, 0x1288, 0x1816, 0x220b, 0x2981, 0x262f, 0x17f0, 0x6d2,  0xfc48, 0xfecb, 0x722,  0xc3d,  0x6e6,  0xf975, 0xe96f, 0xdd92, 0xdd6b, 0xe701, 0xf560, 0xfd48, 0xfa18, 0xf1db, 0xec67, 0xeea1, 0xf8c0, 0x5df,  0xdb2,
    0xbcb,  0x2f4,  0xfa82, 0xf691, 0xf960, 0x24d,  0xceb,  0x12a4, 0x1085, 0x82f,  0xfdc7, 0xf5dc, 0xf073, 0xed9d, 0xebec, 0xea65, 0xea44, 0xec13, 0xed4b, 0xeb5e, 0xeaa6, 0xeef3, 0xf8dd, 0x488,  0xc0c,  0xb48,  0x3b5,  0xfc88, 0xfd06, 0x881,  0x1dfb,
    0x32fb, 0x3c79, 0x37b2, 0x2964, 0x1d15, 0x19bd, 0x1e2d, 0x22d7, 0x22a8, 0x1c7a, 0x113a, 0x1aa,  0xef17, 0xe247, 0xdf2c, 0xe10d, 0xe1af, 0xdf86, 0xdb90, 0xd5bc, 0xcf35, 0xcb60, 0xcdd2, 0xd420, 0xdbff, 0xe438, 0xed32, 0xf5f9, 0xfb2e, 0xfcdb, 0xff15,
    0x77d,  0x183c, 0x2b67, 0x3764, 0x366f, 0x298d, 0x19d5, 0xfc3,  0x1274, 0x1e3b, 0x2745, 0x2505, 0x1596, 0x3d0,  0xfa58, 0xfc12, 0x1aa,  0x321,  0xfe2b, 0xf496, 0xe971, 0xe181, 0xe1c4, 0xe94d, 0xf25e, 0xf450, 0xf102, 0xeea0, 0xf1b1, 0xf932, 0x189,
    0x947,  0xcb3,  0xa84,  0x358,  0xfcac, 0xfa52, 0xff5b, 0x81f,  0xe37,  0xf9b,  0xbf3,  0x549,  0xfd0a, 0xf663, 0xf073, 0xecb1, 0xe9fc, 0xe70a, 0xe615, 0xe874, 0xec79, 0xecc6, 0xec80, 0xef6d, 0xf711, 0x108,  0x8e9,  0xb25,  0x6a4,  0x1a8,  0x2bf,
    0xd5b,  0x20d1, 0x33c0, 0x3b9c, 0x36bc, 0x293d, 0x1e71, 0x1c18, 0x2000, 0x245c, 0x22dd, 0x1b4f, 0xe5c,  0xff5d, 0xee97, 0xe1d2, 0xdd18, 0xdcb1, 0xdd6f, 0xdc59, 0xda44, 0xd6ad, 0xd1de, 0xce00, 0xcf2d, 0xd481, 0xdbc7, 0xe3d1, 0xec8a, 0xf597, 0xfb18,
    0xfdaa, 0x2b,   0x7bc,  0x173c, 0x29ba, 0x35c2, 0x3574, 0x2a46, 0x1bd4, 0x11ee, 0x1326, 0x1e20, 0x2725, 0x2582, 0x1618, 0x2f3,  0xf88a, 0xfa7b, 0x18e,  0x36b,  0xfde8, 0xf3a2, 0xe8ad, 0xe077, 0xe02d, 0xe784, 0xf15b, 0xf4a5, 0xf147, 0xee3a, 0xf029,
    0xf7cf, 0x8f,   0x90b,  0xdce,  0xd5e,  0x739,  0xff63, 0xfb1a, 0xfdc8, 0x66c,  0xd8e,  0x1090, 0xe3e,  0x834,  0xff66, 0xf71d, 0xf009, 0xeb4d, 0xe950, 0xe6f7, 0xe60f, 0xe79b, 0xebe7, 0xecd2, 0xebe0, 0xee31, 0xf4ed, 0xff03, 0x747,  0xaa4,  0x743,
    0x28c,  0x301,  0xc51,  0x1ecd, 0x3286, 0x3c24, 0x38de, 0x2bdf, 0x1ff0, 0x1c87, 0x1f36, 0x23e7, 0x2371, 0x1d31, 0x1172, 0x268,  0xf09b, 0xe118, 0xdacd, 0xda75, 0xdc8e, 0xdccd, 0xdb5f, 0xd81e, 0xd297, 0xccc6, 0xcba7, 0xd022, 0xd7a6, 0xe132, 0xeb51,
    0xf532, 0xfb7c, 0xfe72, 0xaf,   0x67c,  0x144a, 0x26f7, 0x3551, 0x37ff, 0x2eaa, 0x1fcb, 0x13e2, 0x1243, 0x1c21, 0x2667, 0x276f, 0x1a44, 0x660,  0xf95a, 0xf943, 0x64,   0x31c,  0xfe5e, 0xf4b4, 0xea60, 0xe1b4, 0xdf56, 0xe45a, 0xed0c, 0xf19c, 0xefb1,
    0xed9f, 0xef71, 0xf730, 0x0a,   0x806,  0xc69,  0xc0c,  0x6af,  0xff72, 0xfb45, 0xfd51, 0x5e8,  0xdc1,  0x118e, 0xfb9,  0x9f1,  0x176,  0xf949, 0xf26f, 0xed26, 0xeaf5, 0xe82b, 0xe6fe, 0xe86b, 0xed04, 0xeec0, 0xeda5, 0xef61, 0xf512, 0xfe8a, 0x6d3,
    0xada,  0x81d,  0x36e,  0x3d4,  0xcca,  0x1e53, 0x30fb, 0x3a79, 0x381e, 0x2c2b, 0x1f66, 0x1bbc, 0x1f93, 0x23c7, 0x1f81, 0x1567, 0x881,  0xfa5b, 0xec75, 0xe003, 0xd911, 0xd540, 0xd3de, 0xd1cc, 0xcfaa, 0xd06d, 0xd255, 0xd551, 0xda96, 0xe16d, 0xe908,
    0xef9c, 0xf3f2, 0xf659, 0xf6db, 0xfc6e, 0x8c4,  0x1911, 0x2a0c, 0x3669, 0x386e, 0x2e5c, 0x1f11, 0x1075, 0xab4,  0x1117, 0x1e06, 0x264a, 0x21df, 0x1021, 0xfb78, 0xf08e, 0xf1ee, 0xfc98, 0x69b,  0xb1d,  0x359,  0xef05, 0xda37, 0xd05a, 0xd614, 0xe2f4,
    0xee1b, 0xf226, 0xf0d5, 0xeead, 0xee2d, 0xf1d0, 0xf8ec, 0x38a,  0xd39,  0x100b, 0xc8e,  0x7f9,  0x60c,  0x7a9,  0xc0b,  0x1125, 0x15bc, 0x1847, 0x162b, 0xfb9,  0x8b7,  0x421,  0x98,   0xfbca, 0xf691, 0xf1cd, 0xeda5, 0xeb83, 0xeba0, 0xed32, 0xef40,
    0xf0b5, 0xf25b, 0xf4e8, 0xf71e, 0xf9bf, 0xfdf7, 0x255,  0x6f6,  0xc7a,  0xfc6,  0xdfc,  0x8d1,  0x727,  0xbf5,  0x1648, 0x1ef6, 0x1e58, 0x1419, 0x58e,  0xfb3a, 0xf7a7, 0xfe29, 0x8f0,  0xe36,  0xbd2,  0x1ec,  0xf764, 0xf2c7, 0xf5d6, 0xfa03, 0xf84e,
    0xf2ce, 0xedbb, 0xe9ee, 0xe59c, 0xe3eb, 0xe7b5, 0xed9d, 0xf2c8, 0xf6af, 0xfac1
};

static uint16_t g_vipWave[] = {
    0x004c, 0xe0c2, 0x010b, 0x106f, 0xf770, 0xf70d, 0xe05a, 0xb19e, 0xb314, 0xe184, 0x08a9, 0x22ed, 0x30c4, 0x0c02, 0xd8cb, 0xdf29, 0x06a1, 0x11ed, 0xfbf9, 0xf8c1, 0x0915, 0x0192, 0x160c, 0x4306, 0x4076, 0x1cd1, 0xf74b, 0xdc7f, 0xcf59, 0xf34c, 0x306f,
    0x333a, 0x0362, 0xe26a, 0x0055, 0x11bb, 0xf816, 0xf771, 0xe33b, 0xb386, 0xb210, 0xdf18, 0x07eb, 0x227f, 0x3188, 0x0f06, 0xda28, 0xde52, 0x05c1, 0x12bd, 0xfd99, 0xf937, 0x09ad, 0x02a7, 0x1583, 0x428e, 0x41f0, 0x1f26, 0xf9a3, 0xdef2, 0xd11d, 0xf35d,
    0x3172, 0x3660, 0x06e1, 0xe496, 0x0134, 0x1483, 0xfbbc, 0xf9c0, 0xe668, 0xb744, 0xb3da, 0xe080, 0x0a2f, 0x2484, 0x33f8, 0x127e, 0xdc62, 0xdf59, 0x07c2, 0x15c8, 0x0143, 0xfb7e, 0x0c4b, 0x05ea, 0x1770, 0x45bd, 0x466d, 0x23e0, 0xfd5a, 0xe27e, 0xd48a,
    0xf4cf, 0x337c, 0x3a91, 0x0bf7, 0xe780, 0x0206, 0x1681, 0xfdf0, 0xfb78, 0xe96e, 0xb9ff, 0xb4ea, 0xe08c, 0x0a3a, 0x2527, 0x355c, 0x159d, 0xdec5, 0xdefa, 0x06c4, 0x15f5, 0x01e0, 0xfb57, 0x0c16, 0x0659, 0x1713, 0x4567, 0x476d, 0x24ee, 0xfe8f, 0xe3dc,
    0xd4ce, 0xf2f5, 0x31de, 0x3b61, 0x0d51, 0xe768, 0x00f7, 0x1741, 0xfe91, 0xfb6a, 0xeb06, 0xbab0, 0xb37a, 0xdefc, 0x08fa, 0x23ac, 0x3493, 0x1670, 0xdecb, 0xdd5c, 0x050f, 0x1580, 0x029e, 0xfa9a, 0x0b0b, 0x055f, 0x1406, 0x426d, 0x465e, 0x254a, 0xfdca,
    0xe224, 0xd308, 0xefbf, 0x2f16, 0x3af9, 0x0da8, 0xe63d, 0xfec4, 0x1645, 0xff21, 0xfc73, 0xed0a, 0xbc90, 0xb328, 0xdd0b, 0x07ca, 0x236f, 0x34a3, 0x18a8, 0xe130, 0xdd4b, 0x0496, 0x1640, 0x043f, 0xfb50, 0x0b83, 0x0628, 0x133e, 0x4201, 0x4785, 0x2766,
    0x002a, 0xe456, 0xd51d, 0xef45, 0x2e29, 0x3c0a, 0x0fa7, 0xe6e4, 0xfd93, 0x163e, 0xfe63, 0xfaca, 0xed66, 0xbd2a, 0xb14d, 0xda63, 0x0589, 0x209e, 0x32c9, 0x1854, 0xe101, 0xdb0e, 0x01ff, 0x151c, 0x0358, 0xf9a0, 0x0a1b, 0x0603, 0x119e, 0x4031, 0x474c,
    0x278a, 0x00a3, 0xe4a9, 0xd569, 0xee3d, 0x2c9a, 0x3c21, 0x1113, 0xe744, 0xfc01, 0x16a5, 0xffcf, 0xfb56, 0xef8b, 0xbf7c, 0xb1fe, 0xda6a, 0x060f, 0x213f, 0x338b, 0x1a87, 0xe29a, 0xd9f0, 0x00c4, 0x155e, 0x03bd, 0xf976, 0x09d0, 0x0601, 0x1010, 0x3e9e,
    0x476d, 0x2824, 0x0198, 0xe561, 0xd5d9, 0xec9b, 0x2a9d, 0x3ca4, 0x12fe, 0xe817, 0xfae8, 0x16ff, 0x008d, 0xfb49, 0xf092, 0xc08f, 0xb1c7, 0xd8cc, 0x04a0, 0x1fd6, 0x3294, 0x1b8c, 0xe3e5, 0xda11, 0xffe2, 0x1523, 0x047e, 0xf8ff, 0x08f5, 0x0526, 0x0dfe,
    0x3d09, 0x4771, 0x28d7, 0x0272, 0xe5aa, 0xd4f5, 0xeada, 0x291c, 0x3cf6, 0x13fd, 0xe7d8, 0xf953, 0x1677, 0x0101, 0xfac9, 0xf17b, 0xc21d, 0xb122, 0xd717, 0x037d, 0x1ead, 0x31e3, 0x1c70, 0xe4c8, 0xd9a5, 0xff1b, 0x155c, 0x04f2, 0xf86c, 0x0828, 0x04b0,
    0x0c34, 0x3b1f, 0x4734, 0x28f8, 0x025b, 0xe609, 0xd54e, 0xe8fc, 0x26f2, 0x3c29, 0x147b, 0xe816, 0xf6f2, 0x1513, 0x007d, 0xf97b, 0xf1a3, 0xc2cc, 0xb097, 0xd595, 0x01ed, 0x1d87, 0x313e, 0x1d41, 0xe50a, 0xd803, 0xfddc, 0x14e5, 0x0562, 0xf7d2, 0x06fa,
    0x04a6, 0x0ab6, 0x38e5, 0x467d, 0x296b, 0x0241, 0xe546, 0xd3dc, 0xe5cb, 0x23b8, 0x3b7b, 0x14d5, 0xe67d, 0xf443, 0x13fc, 0x0049, 0xf862, 0xf1b0, 0xc37a, 0xae90, 0xd285, 0xffd7, 0x1c06, 0x30dc, 0x1ec4, 0xe657, 0xd6b9, 0xfb9f, 0x14a4, 0x06d1, 0xf857,
    0x06e9, 0x05ce, 0x0a5a, 0x37ce, 0x474c, 0x2adc, 0x03c3, 0xe640, 0xd462, 0xe4c5, 0x22ac, 0x3c48, 0x16ca, 0xe7e5, 0xf340, 0x1409, 0x0172, 0xf841, 0xf201, 0xc42d, 0xade1, 0xd064, 0xfe38, 0x1aeb, 0x2fbb, 0x1edc, 0xe706, 0xd58e, 0xf9fe, 0x14c9, 0x0784,
    0xf7c3, 0x061f, 0x059c, 0x08c9, 0x3605, 0x47d2, 0x2c9a, 0x04d2, 0xe6f5, 0xd48d, 0xe2c5, 0x2051, 0x3c3f, 0x17fa, 0xe856, 0xf1a6, 0x12e8, 0x0194, 0xf76d, 0xf26b, 0xc570, 0xad4d, 0xce94, 0xfcbd, 0x1965, 0x2e62, 0x1fa3, 0xe7ee, 0xd473, 0xf762, 0x12f1,
    0x06d8, 0xf68a, 0x04d6, 0x0536, 0x077e, 0x34a2, 0x47ff, 0x2cb1, 0x0507, 0xe6cc, 0xd38d, 0xe104, 0x1e02, 0x3b9e, 0x192c, 0xe89c, 0xefe3, 0x1241, 0x0213, 0xf6cc, 0xf2ea, 0xc685, 0xac12, 0xcbe7, 0xfa6a, 0x175b, 0x2d14, 0x2052, 0xe8a4, 0xd361, 0xf668,
    0x12e2, 0x0750, 0xf628, 0x040a, 0x04a3, 0x0596, 0x3234, 0x4705, 0x2d14, 0x0595, 0xe75a, 0xd347, 0xde5b, 0x1b24, 0x3b13, 0x19e6, 0xe890, 0xee1f, 0x1162, 0x01d5, 0xf529, 0xf29e, 0xc6da, 0xaa7a, 0xc911, 0xf81a, 0x160d, 0x2c11, 0x2122, 0xea0b, 0xd263,
    0xf3f4, 0x111a, 0x06ab, 0xf4b9, 0x0270, 0x043f, 0x03a3, 0x3009, 0x469c, 0x2d73, 0x05c1, 0xe697, 0xd2ae, 0xdccd, 0x1904, 0x3aad, 0x1b43, 0xe8c7, 0xec59, 0x10c8, 0x0281, 0xf5bf, 0xf315, 0xc82c, 0xaaaf, 0xc786, 0xf770, 0x1571, 0x2b57, 0x21d3, 0xeb38,
    0xd12e, 0xf2c0, 0x1182, 0x07c7, 0xf5a6, 0x0268, 0x0592, 0x03e3, 0x2f91, 0x47ee, 0x2f99, 0x0819, 0xe821, 0xd428, 0xdb7d, 0x16c2, 0x3b92, 0x1de2, 0xeb42, 0xec7d, 0x1190, 0x049f, 0xf6d7, 0xf4ef, 0xcb3c, 0xac7e, 0xc7e5, 0xf7f9, 0x1694, 0x2d03, 0x2479,
    0xef02, 0xd2d7, 0xf235, 0x1286, 0x098d, 0xf5f1, 0x01e0, 0x05da, 0x0334, 0x2e50, 0x47f5, 0x3052, 0x096b, 0xe8da, 0xd424, 0xda33, 0x14a1, 0x3aa4, 0x1e70, 0xec6a, 0xeae6, 0x103d, 0x04c6, 0xf579, 0xf4f0, 0xcc4c, 0xac7b, 0xc627, 0xf586, 0x1400, 0x2b7c,
    0x259f, 0xf05c, 0xd2f7, 0xf09e, 0x1167, 0x0a3e, 0xf5e3, 0x00b6, 0x0626, 0x02b1, 0x2bbd, 0x483e, 0x3212, 0x0ad0, 0xe9ee, 0xd54b, 0xda11, 0x127c, 0x3a8e, 0x1ff3, 0xed45, 0xe9a8, 0x0fa1, 0x05f0, 0xf553, 0xf526, 0xcce7, 0xabef, 0xc449, 0xf41f, 0x13df,
    0x2a68, 0x24fe, 0xf047, 0xd15c, 0xeddc, 0x0f42, 0x0984, 0xf4cc, 0xff4c, 0x0542, 0x0177, 0x299d, 0x4667, 0x31b7, 0x0abe, 0xe952, 0xd459, 0xd7af, 0x0f4f, 0x39cc, 0x20bd, 0xed46, 0xe7bd, 0x0d86, 0x0595, 0xf443, 0xf529, 0xce3b, 0xabd3, 0xc2d9, 0xf27b,
    0x125e, 0x2941, 0x25d8, 0xf1a9, 0xd040, 0xec5d, 0x0f2f, 0x09e4, 0xf486, 0xfe56, 0x055f, 0x0034, 0x2765, 0x45ad, 0x311d, 0x0a49, 0xe8d9, 0xd349, 0xd4dc, 0x0b70, 0x36d9, 0x1f8e, 0xec29, 0xe4dd, 0x0b55, 0x048c, 0xf26e, 0xf3e5, 0xce55, 0xaa3e, 0xbf8d,
    0xef1a, 0x0fd5, 0x27e5, 0x2621, 0xf2e0, 0xcfd4, 0xeb0f, 0x0dcf, 0x097e, 0xf4ac, 0xfdd3, 0x04e3, 0xffa6, 0x26da, 0x45f0, 0x3276, 0x0c6c, 0xeab0, 0xd4b6, 0xd50e, 0x0a66, 0x374e, 0x21cb, 0xee6f, 0xe427, 0x09ab, 0x050d, 0xf1b9, 0xf2b7, 0xce1f, 0xa911,
    0xbc69, 0xec0c, 0x0d59, 0x24e8, 0x2485, 0xf26e, 0xcda5, 0xe704, 0x0b1c, 0x08b5, 0xf33e, 0xfb01, 0x0370, 0xfd99, 0x2281, 0x4400, 0x3295, 0x0c3e, 0xea5e, 0xd470, 0xd3c8, 0x082e, 0x3727, 0x23f2, 0xef8d, 0xe311, 0x095b, 0x06bb, 0xf350, 0xf478, 0xd13c,
    0xaa67, 0xbc40, 0xeccd, 0x0e2f, 0x2583, 0x262e, 0xf4f9, 0xce76, 0xe6ae, 0x0b3b, 0x09d6, 0xf460, 0xfabe, 0x03dc, 0xfdec, 0x21a8, 0x43fe, 0x3470, 0x0e90, 0xebe0, 0xd5b1, 0xd3b2, 0x06cc, 0x373c, 0x25ef, 0xf192, 0xe2d1, 0x0887, 0x0715, 0xf2e3, 0xf4fc,
    0xd39a, 0xab3e, 0xba9f, 0xebac, 0x0d89, 0x2507, 0x27c1, 0xf73a, 0xcef9, 0xe61e, 0x0b72, 0x0ad9, 0xf563, 0xfb00, 0x044a, 0xfd4b, 0x2000, 0x44a2, 0x3597, 0x1013, 0xed59, 0xd6c4, 0xd37d, 0x0533, 0x37bb, 0x2718, 0xf31f, 0xe2ca, 0x0782, 0x06e9, 0xf294,
    0xf692, 0xd577, 0xabc5, 0xb95d, 0xe8f5, 0x0b56, 0x2387, 0x2791, 0xf7aa, 0xce13, 0xe385, 0x0974, 0x0a9a, 0xf515, 0xfa20, 0x0407, 0xfbe5, 0x1d0b, 0x42b0, 0x34ea, 0x0fc7, 0xec92, 0xd5fc, 0xd1df, 0x01e7, 0x35d5, 0x278a, 0xf35c, 0xe12c, 0x05fb, 0x07b1,
    0xf27f, 0xf5db, 0xd60f, 0xab78, 0xb849, 0xe871, 0x0bca, 0x2486, 0x29f4, 0xfb15, 0xcf82, 0xe371, 0x0a36, 0x0ce2, 0xf6bf, 0xfae5, 0x0618, 0xfdf9, 0x1df4, 0x4520, 0x3926, 0x1376, 0xefd1, 0xd915, 0xd435, 0x0365, 0x395e, 0x2cec, 0xf839, 0xe46b, 0x08aa,
    0x0c81, 0xf65d, 0xf969, 0xdae0, 0xaef2, 0xba07, 0xe999, 0x0dd1, 0x2636, 0x2c5f, 0xfe6d, 0xd146, 0xe389, 0x0a12, 0x0dab, 0xf765, 0xfadb, 0x0662, 0xfdae, 0x1c5f, 0x446f, 0x39a3, 0x13ee, 0xf002, 0xd807, 0xd203, 0x0061, 0x3684, 0x2c9e, 0xf849, 0xe1b6,
    0x0609, 0x0c24, 0xf620, 0xf85b, 0xdaa9, 0xadd5, 0xb770, 0xe743, 0x0c70, 0x2570, 0x2c5c, 0x00c7, 0xd225, 0xe2c8, 0x0a44, 0x0f15, 0xf94a, 0xfaf3, 0x0785, 0xfed7, 0x1be1, 0x44b1, 0x3b9b, 0x171f, 0xf1d7, 0xd9f0, 0xd2d5, 0xff6a, 0x37ab, 0x2f9a, 0xfb62,
    0xe267, 0x061a, 0x0e6b, 0xf7ac, 0xf958, 0xdd8d, 0xaff0, 0xb68f, 0xe681, 0x0be7, 0x24cd, 0x2dbf, 0x0357, 0xd329, 0xe1b2, 0x0a3d, 0x10ef, 0xfa85, 0xfab0, 0x0843, 0xffc2, 0x1b05, 0x4568, 0x3d3c, 0x188e, 0xf34f, 0xdae7, 0xd2f3, 0xfe1e, 0x376d, 0x3154,
    0xfda3, 0xe2ae, 0x051d, 0x0f09, 0xf7cb, 0xf971, 0xdf21, 0xb0a4, 0xb58b, 0xe516, 0x0b0c, 0x2416, 0x2e15, 0x04b3, 0xd36b, 0xe005, 0x0875, 0x0fcc, 0xf989, 0xf90d, 0x06fb, 0xff31, 0x1889, 0x43f7, 0x3e37, 0x1969, 0xf48d, 0xdbbd, 0xd1e3, 0xfb1d, 0x3603,
    0x3202, 0xfe97, 0xe22e, 0x03c8, 0x0f84, 0xf813, 0xf9ba, 0xe0bd, 0xb27a, 0xb5f7, 0xe47c, 0x0aeb, 0x243f, 0x2f90, 0x07c6, 0xd55b, 0xe03a, 0x0929, 0x11de, 0xfb6e, 0xf9a4, 0x07ca, 0x001f, 0x18a0, 0x440a, 0x3f19, 0x1b7c, 0xf5ec, 0xdcbc, 0xd240, 0xf9eb,
    0x350b, 0x32b4
};

void Chip8GenericEmulator::renderAudio(int16_t* samples, size_t frames, int sampleFrequency)
{
#ifdef EMU_AUDIO_DEBUG
    std::clog << fmt::format("render {} (ST:{})", frames, _rST) << std::endl;
#endif
    if(_isMegaChipMode && _sampleLength) {
        while(frames--) {
            *samples++ = ((int16_t)getNextMCSample() - 128) * 256;
        }
    }
    else if(_rST) {
        if (_options.optXOChipSound) {
            auto step = 4000 * std::pow(2.0f, (float(_xoPitch) - 64) / 48.0f) / 128 / sampleFrequency;
            for (int i = 0; i < frames; ++i) {
                auto pos = int(std::clamp(_wavePhase * 128.0f, 0.0f, 127.0f));
                *samples++ = _xoAudioPattern[pos >> 3] & (1 << (7 - (pos & 7))) ? 16384 : -16384;
                _wavePhase = std::fmod(_wavePhase + step, 1.0f);
            }
        }
        else if(_options.behaviorBase >= Chip8GenericOptions::eCHIP48 && _options.behaviorBase <= Chip8GenericOptions::eSCHPC) {
            for (int i = 0; i < frames; ++i) {
                *samples++ =  static_cast<int16_t>(g_hp48Wave[(int)_wavePhase]);
                _wavePhase = std::fmod(_wavePhase + 1, sizeof(g_hp48Wave) / 2);
            }
        }
        else if(_options.behaviorBase < Chip8GenericOptions::eCHIP8X) {
            for (int i = 0; i < frames; ++i) {
                *samples++ = static_cast<int16_t>(g_vipWave[(int)_wavePhase]);
                _wavePhase = std::fmod(_wavePhase + 1, sizeof(g_vipWave) / 2);
            }
        }
        else {
            auto audioFrequency = _options.behaviorBase == Chip8GenericOptions::eCHIP8X ? 27535.0f / ((unsigned)_vp595Frequency + 1) : 1531.555f;
            const float step = audioFrequency / sampleFrequency;
            for (int i = 0; i < frames; ++i) {
                *samples++ = (_wavePhase > 0.5f) ? 16384 : -16384;
                _wavePhase = std::fmod(_wavePhase + step, 1.0f);
            }
        }
    }
    else {
        // Default is silence
        _wavePhase = 0;
        IEmulationCore::renderAudio(samples, frames, sampleFrequency);
    }
}

}
