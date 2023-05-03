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
    return variantForPreset(behaviorBase);
}

Chip8Variant Chip8EmulatorOptions::variantForPreset(SupportedPreset preset)
{
    switch(preset) {
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
        case ePORTABLE: return "PORTABLE";
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
        case ePORTABLE: return "PORTABLE";
        default: return "unknown";
    }
}

using Opts = emu::Chip8EmulatorOptions;

static std::map<std::string, emu::Chip8EmulatorOptions::SupportedPreset> presetMap = {
    {"chip8", Opts::eCHIP8},
    {"chip10", Opts::eCHIP10},
    {"chip48", Opts::eCHIP48},
    {"schip10", Opts::eSCHIP10},
    {"superchip10", Opts::eSCHIP10},
    {"schip11", Opts::eSCHIP11},
    {"superchip11", Opts::eSCHIP11},
    {"superchipcompatibility", Opts::eSCHPC},
    {"schipc", Opts::eSCHPC},
    {"schipcomp", Opts::eSCHPC},
    {"schpc", Opts::eSCHPC},
    {"gchpc", Opts::eSCHPC},
    {"mchip", Opts::eMEGACHIP},
    {"mchip8", Opts::eMEGACHIP},
    {"megachip8", Opts::eMEGACHIP},
    {"mega8", Opts::eMEGACHIP},
    {"xo", Opts::eXOCHIP},
    {"xochip", Opts::eXOCHIP},
    {"vipchip8", Opts::eCHIP8VIP},
    {"chip8vip", Opts::eCHIP8VIP},
    {"cosmac", Opts::eCHIP8VIP},
    {"cosmacvip", Opts::eCHIP8VIP},
    {"vipchip8tpd", Opts::eCHIP8VIP_TPD},
    {"chip8viptpd", Opts::eCHIP8VIP_TPD},
    {"chip8dream", Opts::eCHIP8DREAM},
    {"dreamchip8", Opts::eCHIP8DREAM},
    {"dream6800", Opts::eCHIP8DREAM},
    {"chipos", Opts::eCHIP8DREAM},
    {"chip8dreamchiposlo", Opts::eC8D68CHIPOSLO},
    {"chip8chiposlo", Opts::eC8D68CHIPOSLO},
    {"c8d6k8chiposlo", Opts::eC8D68CHIPOSLO},
    {"chiposlo", Opts::eC8D68CHIPOSLO},
    {"chicueyi", Opts::eCHICUEYI}
};

static std::map<Opts::SupportedPreset,std::string> presetOptionsProtoMap = {
    {Opts::eCHIP8, R"({})"},
    {Opts::eCHIP10, R"({"optAllowHires":true,"optOnlyHires":true})"},
    {Opts::eCHIP48, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreIncIByX":true,"optInstantDxyn":true,"optJump0Bxnn":true,"instructionsPerFrame":15})"},
    {Opts::eSCHIP10, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreIncIByX":true,"optInstantDxyn":true,"optLoresDxy0Is8x16":true,"optJump0Bxnn":true,"optAllowHires":true,"instructionsPerFrame":15})"},
    {Opts::eSCHIP11, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreDontIncI":true,"optInstantDxyn":true,"optLoresDxy0Is8x16":true,"optSC11Collision":true,"optJump0Bxnn":true,"optAllowHires":true,"instructionsPerFrame":30})"},
    {Opts::eSCHPC, R"({"optDontResetVf":true,"optInstantDxyn":true,"optLoresDxy0Is16x16":true,"optAllowHires":true,"instructionsPerFrame":30})"},
    {Opts::eMEGACHIP, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreDontIncI":true,"optInstantDxyn":true,"optLoresDxy0Is8x16":true,"optSC11Collision":true,"optAllowHires":true,"optHas16BitAddr":true,"instructionsPerFrame":3000})"},
    {Opts::eXOCHIP, R"({"optDontResetVf":true,"optWrapSprites":true,"optInstantDxyn":true,"optLoresDxy0Is16x16":true,"optAllowHires":true,"optAllowColors":true,"optHas16BitAddr":true,"optXOChipSound":true,"instructionsPerFrame":1000})"},
    {Opts::eCHICUEYI, R"({"optDontResetVf":true,"optWrapSprites":true,"optInstantDxyn":true,"optLoresDxy0Is16x16":true,"optAllowHires":true,"optAllowColors":true,"optHas16BitAddr":true,"optChicueyiSound":true,"instructionsPerFrame":1000})"},
    {Opts::eCHIP8VIP, R"({})"},
    {Opts::eCHIP8VIP_TPD, R"({"advanced":{"interpreter":"chip8tdp"}})"},
    {Opts::eCHIP8DREAM, R"({})"},
    {Opts::eC8D68CHIPOSLO, R"({"advanced":{"kernel":"chiposlo"}})"}
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

Chip8EmulatorOptions::SupportedPreset Chip8EmulatorOptions::presetForVariant(chip8::Variant variant)
{
    if(variant == chip8::Variant::CHIP_10) return eCHIP10;
    else if(variant == chip8::Variant::CHIP_48) return eCHIP48;
    else if(variant == chip8::Variant::SCHIP_1_0) return eSCHIP10;
    else if(variant == chip8::Variant::SCHIP_1_1) return eSCHIP11;
    else if(variant == chip8::Variant::SCHIPC) return eSCHPC;
    else if(variant == chip8::Variant::MEGA_CHIP) return eMEGACHIP;
    else if(variant == chip8::Variant::XO_CHIP) return eXOCHIP;
    else if(variant == chip8::Variant::CHIP_8_TPD) return eCHIP8VIP_TPD;
    else if(variant == chip8::Variant::CHIP_8_COSMAC_VIP) return eCHIP8VIP;
    else if(variant == chip8::Variant::CHIP_8_D6800) return eCHIP8DREAM;
    else if(variant == chip8::Variant::CHIP_8_D6800_LOP) return eC8D68CHIPOSLO;
    else if(variant == chip8::Variant::GENERIC_CHIP_8) return ePORTABLE;
    return eCHIP8;
}

#if 1
Chip8EmulatorOptions Chip8EmulatorOptions::optionsOfPreset(SupportedPreset preset)
{
    static std::map<Opts::SupportedPreset,Opts> presetOptionsMap;
    if(preset == Opts::eCHIP8)
        return Opts();
    if(presetOptionsMap.empty()) {
        for(const auto& [presetId,jsonString] : presetOptionsProtoMap) {
            Opts opts;
            from_json(nlohmann::json::parse(jsonString),opts);
            opts.behaviorBase = presetId;
            presetOptionsMap[presetId] = opts;
        }
    }
    auto iter = presetOptionsMap.find(preset);
    return iter != presetOptionsMap.end() ? iter->second : Opts();
}
#else
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
#endif

bool Chip8EmulatorOptions::operator==(const Chip8EmulatorOptions& other) const
{
    return behaviorBase == other.behaviorBase && startAddress == other.startAddress && optJustShiftVx == other.optJustShiftVx && optDontResetVf == other.optDontResetVf && optLoadStoreIncIByX == other.optLoadStoreIncIByX &&
           optLoadStoreDontIncI == other.optLoadStoreDontIncI && optWrapSprites == other.optWrapSprites && optInstantDxyn == other.optInstantDxyn && optLoresDxy0Is8x16 == other.optLoresDxy0Is8x16 && optLoresDxy0Is16x16 == other.optLoresDxy0Is16x16 &&
           optSC11Collision == other.optSC11Collision && optJump0Bxnn == other.optJump0Bxnn && optAllowHires == other.optAllowHires && optOnlyHires == other.optOnlyHires && optAllowColors == other.optAllowColors &&
           optHas16BitAddr == other.optHas16BitAddr && optXOChipSound == other.optXOChipSound && optChicueyiSound == other.optChicueyiSound && optTraceLog == other.optTraceLog && instructionsPerFrame == other.instructionsPerFrame &&
           advanced == other.advanced;
}

#define SET_IF_CHANGED(j, n) if(o.n != defaultOpts.n) j[#n] = o.n;
void to_json(nlohmann::json& j, const Chip8EmulatorOptions& o)
{
    const Opts defaultOpts = Chip8EmulatorOptions::optionsOfPreset(o.behaviorBase);
    using json = nlohmann::json;
    json obj;
    obj["behaviorBase"] = o.nameOfPreset(o.behaviorBase);
    SET_IF_CHANGED(obj, startAddress);
    SET_IF_CHANGED(obj, optJustShiftVx);
    SET_IF_CHANGED(obj, optDontResetVf);
    SET_IF_CHANGED(obj, optLoadStoreIncIByX);
    SET_IF_CHANGED(obj, optLoadStoreDontIncI);
    SET_IF_CHANGED(obj, optWrapSprites);
    SET_IF_CHANGED(obj, optInstantDxyn);
    SET_IF_CHANGED(obj, optLoresDxy0Is8x16);
    SET_IF_CHANGED(obj, optLoresDxy0Is16x16);
    SET_IF_CHANGED(obj, optSC11Collision);
    SET_IF_CHANGED(obj, optJump0Bxnn);
    SET_IF_CHANGED(obj, optAllowHires);
    SET_IF_CHANGED(obj, optOnlyHires);
    SET_IF_CHANGED(obj, optAllowColors);
    SET_IF_CHANGED(obj, optHas16BitAddr);
    SET_IF_CHANGED(obj, optXOChipSound);
    SET_IF_CHANGED(obj, optTraceLog);
    SET_IF_CHANGED(obj, instructionsPerFrame);
    j = obj;
    if(o.advanced) {
        j["advanced"] = *o.advanced;
    }
}

void from_json(const nlohmann::json& j, Chip8EmulatorOptions& o)
{
    auto variantName = j.value("behaviorBase",o.nameOfPreset(o.behaviorBase));
    o.behaviorBase = Chip8EmulatorOptions::presetForName(variantName);
    const Opts defaultOpts = Chip8EmulatorOptions::optionsOfPreset(o.behaviorBase);
    o.startAddress = j.value("startAddress", defaultOpts.startAddress);
    o.optJustShiftVx = j.value("optJustShiftVx", defaultOpts.optJustShiftVx);
    o.optDontResetVf = j.value("optDontResetVf", defaultOpts.optDontResetVf);
    o.optLoadStoreIncIByX = j.value("optLoadStoreIncIByX", defaultOpts.optLoadStoreIncIByX);
    o.optLoadStoreDontIncI = j.value("optLoadStoreDontIncI", defaultOpts.optLoadStoreDontIncI);
    o.optWrapSprites = j.value("optWrapSprites", defaultOpts.optWrapSprites);
    o.optInstantDxyn = j.value("optInstantDxyn", defaultOpts.optInstantDxyn);
    o.optJump0Bxnn = j.value("optJump0Bxnn", defaultOpts.optJump0Bxnn);
    o.optAllowHires = j.value("optAllowHires", defaultOpts.optAllowHires);
    o.optOnlyHires = j.value("optOnlyHires", defaultOpts.optOnlyHires);
    o.optAllowColors = j.value("optAllowColors", defaultOpts.optAllowColors);
    o.optHas16BitAddr = j.value("optHas16BitAddr", defaultOpts.optHas16BitAddr);
    o.optXOChipSound = j.value("optXOChipSound", defaultOpts.optXOChipSound);
    o.optTraceLog = j.value("optTraceLog", defaultOpts.optTraceLog);
    o.optLoresDxy0Is8x16 = j.value("optLoresDxy0Is8x16", defaultOpts.optLoresDxy0Is8x16);
    o.optLoresDxy0Is16x16 = j.value("optLoresDxy0Is16x16", defaultOpts.optLoresDxy0Is16x16);
    o.optSC11Collision = j.value("optSC11Collision", defaultOpts.optSC11Collision);
    o.instructionsPerFrame = j.value("instructionsPerFrame", defaultOpts.instructionsPerFrame);
    if(j.contains("advanced")) {
        o.advanced = std::make_shared<nlohmann::ordered_json>(j.at("advanced"));
    }
}

}
