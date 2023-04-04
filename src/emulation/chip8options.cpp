//---------------------------------------------------------------------------------------
// src/emulation/chip8options.cpp
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

#include <emulation/chip8options.hpp>
#include <nlohmann/json.hpp>

#include <algorithm>

namespace emu {

Chip8Variant Chip8EmulatorOptions::presetAsVariant() const
{
    switch(behaviorBase) {
        case eCHIP8: return Chip8Variant::CHIP_8;
        case eCHIP10: return Chip8Variant::CHIP_10;
        case eCHIP48: return Chip8Variant::CHIP_48;
        case eSCHIP10: return Chip8Variant::SCHIP_1_0;
        case eSCHIP11: return Chip8Variant::SCHIP_1_1;
        case eSCHPC: return Chip8Variant::SCHIPC_GCHIPC;
        case eMEGACHIP: return Chip8Variant::MEGA_CHIP;
        case eXOCHIP: return Chip8Variant::XO_CHIP;
        case eCHIP8VIP: return Chip8Variant::CHIP_8;
        case eCHIP8VIP_TPD: return Chip8Variant::CHIP_8_TPD;
        case eCHIP8DREAM: return Chip8Variant::CHIP_8_D6800;
        case eC8D68CHIPOSLO: return Chip8Variant::CHIP_8_D6800;
        case eCHICUEYI: return Chip8Variant::XO_CHIP;
        default: return Chip8Variant::CHIP_8;
    }
}

std::string Chip8EmulatorOptions::nameOfPreset(SupportedPreset preset)
{
    switch(preset) {
        case eCHIP8: return "CHIP-8";
        case eCHIP10: return "CHIP-10";
        case eCHIP48: return "CHIP-48";
        case eSCHIP10: return "SUPER-CHIP 1.0";
        case eSCHIP11: return "SUPER-CHIP 1.1";
        case eSCHPC: return "SUPER-CHIP-COMPATIBILITY";
        case eMEGACHIP: return "MEGACHIP8";
        case eXOCHIP: return "XO-CHIP";
        case eCHIP8VIP: return "VIP-CHIP-8";
        case eCHIP8VIP_TPD: return "VIP-CHIP-8 64x64";
        case eCHIP8DREAM: return "CHIP-8-DREAM";
        case eC8D68CHIPOSLO: return "CHIP-8-DREAM-CHIPOSLO";
        case eCHICUEYI: return "CHICUEYI";
        default: return "unknown";
    }
}

const char* Chip8EmulatorOptions::shortNameOfPreset(SupportedPreset preset)
{
    switch(preset) {
        case eCHIP8: return "CHIP8";
        case eCHIP10: return "CHIP10";
        case eCHIP48: return "CHIP48";
        case eSCHIP10: return "SCHIP10";
        case eSCHIP11: return "SCHIP11";
        case eSCHPC: return "SCHIPC";
        case eMEGACHIP: return "MCHIP8";
        case eXOCHIP: return "XOCHIP";
        case eCHIP8VIP: return "VIPCHIP8";
        case eCHIP8VIP_TPD: return "VIPCHIP8TDP";
        case eCHIP8DREAM: return "CHIP8DREAM";
        case eC8D68CHIPOSLO: return "D6k8CHIPOSLO";
        case eCHICUEYI: return "CHICUEYI";
        default: return "unknown";
    }
}

static std::map<std::string, emu::Chip8EmulatorOptions::SupportedPreset> presetMap = {
    {"chip8", emu::Chip8EmulatorOptions::eCHIP8},
    {"chip10", emu::Chip8EmulatorOptions::eCHIP10},
    {"chip48", emu::Chip8EmulatorOptions::eCHIP48},
    {"schip10", emu::Chip8EmulatorOptions::eSCHIP10},
    {"superchip10", emu::Chip8EmulatorOptions::eSCHIP10},
    {"schip11", emu::Chip8EmulatorOptions::eSCHIP11},
    {"superchip11", emu::Chip8EmulatorOptions::eSCHIP11},
    {"superchipcompatibility", emu::Chip8EmulatorOptions::eSCHPC},
    {"schipc", emu::Chip8EmulatorOptions::eSCHPC},
    {"schipcomp", emu::Chip8EmulatorOptions::eSCHPC},
    {"schpc", emu::Chip8EmulatorOptions::eSCHPC},
    {"gchpc", emu::Chip8EmulatorOptions::eSCHPC},
    {"mchip", emu::Chip8EmulatorOptions::eMEGACHIP},
    {"mchip8", emu::Chip8EmulatorOptions::eMEGACHIP},
    {"megachip8", emu::Chip8EmulatorOptions::eMEGACHIP},
    {"mega8", emu::Chip8EmulatorOptions::eMEGACHIP},
    {"xo", emu::Chip8EmulatorOptions::eXOCHIP},
    {"xochip", emu::Chip8EmulatorOptions::eXOCHIP},
    {"vipchip8", emu::Chip8EmulatorOptions::eCHIP8VIP},
    {"chip8vip", emu::Chip8EmulatorOptions::eCHIP8VIP},
    {"cosmac", emu::Chip8EmulatorOptions::eCHIP8VIP},
    {"cosmacvip", emu::Chip8EmulatorOptions::eCHIP8VIP},
    {"vipchip8tdp", emu::Chip8EmulatorOptions::eCHIP8VIP_TPD},
    {"chip8viptdp", emu::Chip8EmulatorOptions::eCHIP8VIP_TPD},
    {"chip8dream", emu::Chip8EmulatorOptions::eCHIP8DREAM},
    {"dreamchip8", emu::Chip8EmulatorOptions::eCHIP8DREAM},
    {"dream6800", emu::Chip8EmulatorOptions::eCHIP8DREAM},
    {"chipos", emu::Chip8EmulatorOptions::eCHIP8DREAM},
    {"chip8dreamchiposlo", emu::Chip8EmulatorOptions::eC8D68CHIPOSLO},
    {"chip8chiposlo", emu::Chip8EmulatorOptions::eC8D68CHIPOSLO},
    {"c8d6k8chiposlo", emu::Chip8EmulatorOptions::eC8D68CHIPOSLO},
    {"chiposlo", emu::Chip8EmulatorOptions::eC8D68CHIPOSLO},
    {"chicueyi", emu::Chip8EmulatorOptions::eCHICUEYI}
};

Chip8EmulatorOptions::SupportedPreset Chip8EmulatorOptions::presetForName(const std::string& name)
{
    auto presetUnified = name;
    {
        auto iter = std::remove_if(presetUnified.begin(), presetUnified.end(), [](unsigned char c) { return std::ispunct(c) || std::isspace(c); });
        presetUnified.erase(iter, presetUnified.end());
        std::transform(presetUnified.begin(), presetUnified.end(), presetUnified.begin(), [](unsigned char c){ return std::tolower(c); });
    }
    auto iter = presetMap.find(presetUnified);
    if(iter == presetMap.end()) {
        throw std::runtime_error("Unknown or unsupported chip-8 variant: " + name);
    }
    return iter->second;
}

Chip8EmulatorOptions Chip8EmulatorOptions::optionsOfPreset(SupportedPreset preset)
{
    switch(preset) {
        case eCHIP10:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = false,
                    .optDontResetVf = false,
                    .optLoadStoreIncIByX = false,
                    .optLoadStoreDontIncI = false,
                    .optWrapSprites = false,
                    .optInstantDxyn = false,
                    .optLoresDxy0Is8x16 = false,
                    .optLoresDxy0Is16x16 = false,
                    .optSC11Collision = false,
                    .optJump0Bxnn = false,
                    .optAllowHires = true,
                    .optOnlyHires = true,
                    .optAllowColors = false,
                    .optHas16BitAddr = false,
                    .optXOChipSound = false,
                    .optChicueyiSound = false,
                    .optTraceLog = false,
                    .instructionsPerFrame = 10};
        case eCHIP48:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = true,
                    .optDontResetVf = true,
                    .optLoadStoreIncIByX = true,
                    .optLoadStoreDontIncI = false,
                    .optWrapSprites = false,
                    .optInstantDxyn = true,
                    .optLoresDxy0Is8x16 = false,
                    .optLoresDxy0Is16x16 = false,
                    .optSC11Collision = false,
                    .optJump0Bxnn = true,
                    .optAllowHires = false,
                    .optOnlyHires = false,
                    .optAllowColors = false,
                    .optHas16BitAddr = false,
                    .optXOChipSound = false,
                    .optChicueyiSound = false,
                    .optTraceLog = false,
                    .instructionsPerFrame = 15};
        case eSCHIP10:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = true,
                    .optDontResetVf = true,
                    .optLoadStoreIncIByX = true,
                    .optLoadStoreDontIncI = false,
                    .optWrapSprites = false,
                    .optInstantDxyn = true,
                    .optLoresDxy0Is8x16 = true,
                    .optLoresDxy0Is16x16 = false,
                    .optSC11Collision = false,
                    .optJump0Bxnn = true,
                    .optAllowHires = true,
                    .optOnlyHires = false,
                    .optAllowColors = false,
                    .optHas16BitAddr = false,
                    .optXOChipSound = false,
                    .optChicueyiSound = false,
                    .optTraceLog = false,
                    .instructionsPerFrame = 15};
        case eSCHIP11:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = true,
                    .optDontResetVf = true,
                    .optLoadStoreIncIByX = false,
                    .optLoadStoreDontIncI = true,
                    .optWrapSprites = false,
                    .optInstantDxyn = true,
                    .optLoresDxy0Is8x16 = true,
                    .optLoresDxy0Is16x16 = false,
                    .optSC11Collision = true,
                    .optJump0Bxnn = true,
                    .optAllowHires = true,
                    .optOnlyHires = false,
                    .optAllowColors = false,
                    .optHas16BitAddr = false,
                    .optXOChipSound = false,
                    .optChicueyiSound = false,
                    .optTraceLog = false,
                    .instructionsPerFrame = 30};
        case eSCHPC:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = false,
                    .optDontResetVf = true,
                    .optLoadStoreIncIByX = false,
                    .optLoadStoreDontIncI = false,
                    .optWrapSprites = false,
                    .optInstantDxyn = true,
                    .optLoresDxy0Is8x16 = false,
                    .optLoresDxy0Is16x16 = true,
                    .optSC11Collision = false,
                    .optJump0Bxnn = false,
                    .optAllowHires = true,
                    .optOnlyHires = false,
                    .optAllowColors = false,
                    .optHas16BitAddr = false,
                    .optXOChipSound = false,
                    .optChicueyiSound = false,
                    .optTraceLog = false,
                    .instructionsPerFrame = 30};
        case eMEGACHIP:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = true,
                    .optDontResetVf = true,
                    .optLoadStoreIncIByX = false,
                    .optLoadStoreDontIncI = true,
                    .optWrapSprites = false,
                    .optInstantDxyn = true,
                    .optLoresDxy0Is8x16 = true,
                    .optLoresDxy0Is16x16 = false,
                    .optSC11Collision = false,
                    .optJump0Bxnn = false,
                    .optAllowHires = true,
                    .optOnlyHires = false,
                    .optAllowColors = false,
                    .optHas16BitAddr = true,
                    .optXOChipSound = false,
                    .optChicueyiSound = false,
                    .optTraceLog = false,
                    .instructionsPerFrame = 3000};
        case eXOCHIP:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = false,
                    .optDontResetVf = true,
                    .optLoadStoreIncIByX = false,
                    .optLoadStoreDontIncI = false,
                    .optWrapSprites = true,
                    .optInstantDxyn = true,
                    .optLoresDxy0Is8x16 = false,
                    .optLoresDxy0Is16x16 = true,
                    .optSC11Collision = false,
                    .optJump0Bxnn = false,
                    .optAllowHires = true,
                    .optOnlyHires = false,
                    .optAllowColors = true,
                    .optHas16BitAddr = true,
                    .optXOChipSound = true,
                    .optChicueyiSound = false,
                    .optTraceLog = false,
                    .instructionsPerFrame = 1000};
        case eCHICUEYI:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = false,
                    .optDontResetVf = true,
                    .optLoadStoreIncIByX = false,
                    .optLoadStoreDontIncI = false,
                    .optWrapSprites = false,
                    .optInstantDxyn = true,
                    .optLoresDxy0Is8x16 = false,
                    .optLoresDxy0Is16x16 = true,
                    .optSC11Collision = false,
                    .optJump0Bxnn = false,
                    .optAllowHires = true,
                    .optOnlyHires = false,
                    .optAllowColors = true,
                    .optHas16BitAddr = true,
                    .optXOChipSound = false,
                    .optChicueyiSound = true,
                    .optTraceLog = false,
                    .instructionsPerFrame = 1000};
        case eCHIP8VIP:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = false,
                    .optDontResetVf = false,
                    .optLoadStoreIncIByX = false,
                    .optLoadStoreDontIncI = false,
                    .optWrapSprites = false,
                    .optInstantDxyn = false,
                    .optLoresDxy0Is8x16 = false,
                    .optLoresDxy0Is16x16 = false,
                    .optSC11Collision = false,
                    .optJump0Bxnn = false,
                    .optAllowHires = false,
                    .optOnlyHires = false,
                    .optAllowColors = false,
                    .optHas16BitAddr = false,
                    .optXOChipSound = false,
                    .optChicueyiSound = false,
                    .optTraceLog = false,
                    .instructionsPerFrame = 15};
        case eCHIP8VIP_TPD:
            return {.behaviorBase = preset,
                    .startAddress = 0x260,
                    .optJustShiftVx = false,
                    .optDontResetVf = false,
                    .optLoadStoreIncIByX = false,
                    .optLoadStoreDontIncI = false,
                    .optWrapSprites = false,
                    .optInstantDxyn = false,
                    .optLoresDxy0Is8x16 = false,
                    .optLoresDxy0Is16x16 = false,
                    .optSC11Collision = false,
                    .optJump0Bxnn = false,
                    .optAllowHires = false,
                    .optOnlyHires = false,
                    .optAllowColors = false,
                    .optHas16BitAddr = false,
                    .optXOChipSound = false,
                    .optChicueyiSound = false,
                    .optTraceLog = true,
                    .instructionsPerFrame = 15,
                    .advanced = std::make_shared<nlohmann::ordered_json>(nlohmann::ordered_json::object({
                        {"interpreter", "chip8tdp"}
                    }))
            };
        case eCHIP8DREAM:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = false,
                    .optDontResetVf = false,
                    .optLoadStoreIncIByX = false,
                    .optLoadStoreDontIncI = false,
                    .optWrapSprites = false,
                    .optInstantDxyn = false,
                    .optLoresDxy0Is8x16 = false,
                    .optLoresDxy0Is16x16 = false,
                    .optSC11Collision = false,
                    .optJump0Bxnn = false,
                    .optAllowHires = false,
                    .optOnlyHires = false,
                    .optAllowColors = false,
                    .optHas16BitAddr = false,
                    .optXOChipSound = false,
                    .optChicueyiSound = false,
                    .optTraceLog = false,
                    .instructionsPerFrame = 15};
        case eC8D68CHIPOSLO:
            return {
                .behaviorBase = preset,
                .startAddress = 0x200,
                .optJustShiftVx = false,
                .optDontResetVf = false,
                .optLoadStoreIncIByX = false,
                .optLoadStoreDontIncI = false,
                .optWrapSprites = false,
                .optInstantDxyn = false,
                .optLoresDxy0Is8x16 = false,
                .optLoresDxy0Is16x16 = false,
                .optSC11Collision = false,
                .optJump0Bxnn = false,
                .optAllowHires = false,
                .optOnlyHires = false,
                .optAllowColors = false,
                .optHas16BitAddr = false,
                .optXOChipSound = false,
                .optChicueyiSound = false,
                .optTraceLog = false,
                .instructionsPerFrame = 15,
                .advanced = std::make_shared<nlohmann::ordered_json>(nlohmann::ordered_json::object({
                    {"kernel", "chiposlo"}
                }))
            };
        case eCHIP8:
        default:
            return {.behaviorBase = preset,
                    .startAddress = 0x200,
                    .optJustShiftVx = false,
                    .optDontResetVf = false,
                    .optLoadStoreIncIByX = false,
                    .optLoadStoreDontIncI = false,
                    .optWrapSprites = false,
                    .optInstantDxyn = false,
                    .optLoresDxy0Is8x16 = false,
                    .optLoresDxy0Is16x16 = false,
                    .optSC11Collision = false,
                    .optJump0Bxnn = false,
                    .optAllowHires = false,
                    .optOnlyHires = false,
                    .optAllowColors = false,
                    .optHas16BitAddr = false,
                    .optXOChipSound = false,
                    .optChicueyiSound = false,
                    .optTraceLog = false,
                    .instructionsPerFrame = 15};
    }
}

void to_json(nlohmann::json& j, const Chip8EmulatorOptions& o)
{
    using json = nlohmann::json;
    j = json{
        {"behaviorBase", o.nameOfPreset(o.behaviorBase)},
        {"startAddress", o.startAddress},
        {"optJustShiftVx", o.optJustShiftVx},
        {"optDontResetVf", o.optDontResetVf},
        {"optLoadStoreIncIByX", o.optLoadStoreIncIByX},
        {"optLoadStoreDontIncI", o.optLoadStoreDontIncI},
        {"optWrapSprites", o.optWrapSprites},
        {"optInstantDxyn", o.optInstantDxyn},
        {"optLoresDxy0Is8x16", o.optLoresDxy0Is8x16},
        {"optLoresDxy0Is16x16", o.optLoresDxy0Is16x16},
        {"optSC11Collision", o.optSC11Collision},
        {"optJump0Bxnn", o.optJump0Bxnn},
        {"optAllowHires", o.optAllowHires},
        {"optOnlyHires", o.optOnlyHires},
        {"optAllowColors", o.optAllowColors},
        {"optHas16BitAddr", o.optHas16BitAddr},
        {"optXOChipSound", o.optXOChipSound},
        {"optTraceLog", o.optTraceLog},
        {"instructionsPerFrame", o.instructionsPerFrame}
    };
    if(o.advanced) {
        j["advanced"] = *o.advanced;
    }
}

void from_json(const nlohmann::json& j, Chip8EmulatorOptions& o)
{
    auto variantName = j.at("behaviorBase").get<std::string>();
    o.behaviorBase = Chip8EmulatorOptions::presetForName(variantName);
    j.at("startAddress").get_to(o.startAddress);
    j.at("optJustShiftVx").get_to(o.optJustShiftVx);
    j.at("optDontResetVf").get_to(o.optDontResetVf);
    j.at("optLoadStoreIncIByX").get_to(o.optLoadStoreIncIByX);
    j.at("optLoadStoreDontIncI").get_to(o.optLoadStoreDontIncI);
    j.at("optWrapSprites").get_to(o.optWrapSprites);
    j.at("optInstantDxyn").get_to(o.optInstantDxyn);
    j.at("optJump0Bxnn").get_to(o.optJump0Bxnn);
    j.at("optAllowHires").get_to(o.optAllowHires);
    j.at("optOnlyHires").get_to(o.optOnlyHires);
    j.at("optAllowColors").get_to(o.optAllowColors);
    j.at("optHas16BitAddr").get_to(o.optHas16BitAddr);
    j.at("optXOChipSound").get_to(o.optXOChipSound);
    o.optTraceLog = j.value("optTraceLog", false);
    o.optLoresDxy0Is8x16 = j.value("optLoresDxy0Is8x16", false);
    o.optLoresDxy0Is16x16 = j.value("optLoresDxy0Is16x16", false);
    o.optSC11Collision = j.value("optSC11Collision", false);
    j.at("instructionsPerFrame").get_to(o.instructionsPerFrame);
    if(j.contains("advanced")) {
        o.advanced = std::make_shared<nlohmann::ordered_json>(j.at("advanced"));
    }
}

}
