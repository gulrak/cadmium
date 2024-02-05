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
#include <stdendian/stdendian.h>
#include <fmt/format.h>
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
        case eCHIP8TE: return Chip8Variant::CHIP_8;
        case eCHIP10: return Chip8Variant::CHIP_10;
        case eCHIP8E: return Chip8Variant::CHIP_8E;
        case eCHIP8X: return Chip8Variant::CHIP_8X;
        case eCHIP48: return Chip8Variant::CHIP_48;
        case eSCHIP10: return Chip8Variant::SCHIP_1_0;
        case eSCHIP11: return Chip8Variant::SCHIP_1_1;
        case eSCHPC: return Chip8Variant::SCHIPC_GCHIPC;
        case eSCHIP_MODERN: return Chip8Variant::SCHIPC_GCHIPC;
        case eMEGACHIP: return Chip8Variant::MEGA_CHIP;
        case eXOCHIP: return Chip8Variant::XO_CHIP;
        case eCHIP8VIP: return Chip8Variant::CHIP_8;
        case eCHIP8VIP_TPD: return Chip8Variant::CHIP_8_TPD;
        case eCHIP8VIP_FPD: return Chip8Variant::HI_RES_CHIP_8;
        case eCHIP8EVIP: return Chip8Variant::CHIP_8E;
        case eCHIP8XVIP: return Chip8Variant::CHIP_8X;
        case eCHIP8XVIP_TPD: return Chip8Variant::CHIP_8X_TPD;
        case eCHIP8XVIP_FPD: return Chip8Variant::HI_RES_CHIP_8X;
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
        case eCHIP8TE: return "CHIP-8-STRICT";
        case eCHIP10: return "CHIP-10";
        case eCHIP8E: return "CHIP-8E";
        case eCHIP8X: return "CHIP-8X";
        case eCHIP48: return "CHIP-48";
        case eSCHIP10: return "SUPER-CHIP 1.0";
        case eSCHIP11: return "SUPER-CHIP 1.1";
        case eSCHPC: return "SUPER-CHIP-COMPATIBILITY";
        case eSCHIP_MODERN: return "SUPER-CHIP-OCTO";
        case eMEGACHIP: return "MEGACHIP8";
        case eXOCHIP: return "XO-CHIP";
        case eCHIP8VIP: return "VIP-CHIP-8";
        case eCHIP8VIP_TPD: return "VIP-CHIP-8 64x64";
        case eCHIP8VIP_FPD: return "VIP-HI-RES-CHIP-8";
        case eCHIP8EVIP: return "VIP-CHIP-8E";
        case eCHIP8XVIP: return "VIP-CHIP-8X";
        case eCHIP8XVIP_TPD: return "VIP-CHIP-8X-TPD";
        case eCHIP8XVIP_FPD: return "VIP-HI-RES-CHIP-8X";
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
        case eCHIP8TE: return "CHIP8ST";
        case eCHIP10: return "CHIP10";
        case eCHIP8E: return "CHIP8E";
        case eCHIP8X: return "CHIP8X";
        case eCHIP48: return "CHIP48";
        case eSCHIP10: return "SCHIP10";
        case eSCHIP11: return "SCHIP11";
        case eSCHPC: return "SCHIPC";
        case eSCHIP_MODERN: return "SCHIPOCTO";
        case eMEGACHIP: return "MCHIP8";
        case eXOCHIP: return "XOCHIP";
        case eCHIP8VIP: return "VIPCHIP8";
        case eCHIP8VIP_TPD: return "VIPCHIP8TPD";
        case eCHIP8VIP_FPD: return "VIPCHIP8FPD";
        case eCHIP8EVIP: return "VIPCHIP8E";
        case eCHIP8XVIP: return "VIPCHIP8X";
        case eCHIP8XVIP_TPD: return "VIPCHIP8XTPD";
        case eCHIP8XVIP_FPD: return "VIPCHIP8XFPD";
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
    {"chip8st", Opts::eCHIP8TE},
    {"chip8strict", Opts::eCHIP8TE},
    {"chip8te", Opts::eCHIP8TE},
    {"chip8timing", Opts::eCHIP8TE},
    {"chip10", Opts::eCHIP10},
    {"chip8e", Opts::eCHIP8EVIP},
    {"chip8x", Opts::eCHIP8X},
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
    {"schipm", Opts::eSCHIP_MODERN},
    {"schipmodern", Opts::eSCHIP_MODERN},
    {"schipocto", Opts::eSCHIP_MODERN},
    {"modernschip", Opts::eSCHIP_MODERN},
    {"modernsuperchip", Opts::eSCHIP_MODERN},
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
    {"chip8tpdvip", Opts::eCHIP8VIP_TPD},
    {"chip8vip64x64", Opts::eCHIP8VIP_TPD},
    {"vipchip864x64", Opts::eCHIP8VIP_TPD},
    {"vipchip8fpd", Opts::eCHIP8VIP_FPD},
    {"chip8vipfpd", Opts::eCHIP8VIP_FPD},
    {"chip8fpdvip", Opts::eCHIP8VIP_FPD},
    {"chip8vip64x128", Opts::eCHIP8VIP_FPD},
    {"vipchip864x128", Opts::eCHIP8VIP_FPD},
    {"viphireschip8", Opts::eCHIP8VIP_FPD},
    {"hireschip8vip", Opts::eCHIP8VIP_FPD},
    {"vipchip8e", Opts::eCHIP8EVIP},
    {"vipchip8x", Opts::eCHIP8XVIP},
    {"chip8evip", Opts::eCHIP8EVIP},
    {"chip8xvip", Opts::eCHIP8XVIP},
    {"chip8vipx", Opts::eCHIP8XVIP},
    {"chip8xtpdvip", Opts::eCHIP8XVIP_TPD},
    {"chip8xviptpd", Opts::eCHIP8XVIP_TPD},
    {"vipchip8xtpd", Opts::eCHIP8XVIP_TPD},
    {"chip8xfpdvip", Opts::eCHIP8XVIP_FPD},
    {"chip8xvipfpd", Opts::eCHIP8XVIP_FPD},
    {"vipchip8xfpd", Opts::eCHIP8XVIP_FPD},
    {"hireschip8xvip", Opts::eCHIP8XVIP_FPD},
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
    {Opts::eCHIP8TE, R"({})"},
    {Opts::eCHIP10, R"({"optAllowHires":true,"optOnlyHires":true})"},
    {Opts::eCHIP8E, R"({})"},
    {Opts::eCHIP8X, R"({"startAddress":768,"instructionsPerFrame":18,"advanced":{"palette":["#000080","#000000","#008000","#800000","#181818","#FF0000","#0000FF","#FF00FF","#00FF00","#FFFF00","#00FFFF","#FFFFFF","#000000","#000000","#000000","#000000"]}})"},
    {Opts::eCHIP48, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreIncIByX":true,"optInstantDxyn":false,"optJump0Bxnn":true,"instructionsPerFrame":15,"frameRate":64})"},
    {Opts::eSCHIP10, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreIncIByX":true,"optInstantDxyn":false,"optLoresDxy0Is8x16":true,"optSCLoresDrawing":true,"optJump0Bxnn":true,"optAllowHires":true,"instructionsPerFrame":15,"frameRate":64})"},
    {Opts::eSCHIP11, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreDontIncI":true,"optInstantDxyn":false,"optLoresDxy0Is8x16":true,"optSCLoresDrawing":true,"optSC11Collision":true,"optHalfPixelScroll":true,"optJump0Bxnn":true,"optAllowHires":true,"instructionsPerFrame":30,"frameRate":64})"},
    {Opts::eSCHPC, R"({"optDontResetVf":true,"optInstantDxyn":true,"optLoresDxy0Is16x16":true,"optModeChangeClear":true,"optAllowHires":true,"instructionsPerFrame":30})"},
    {Opts::eSCHIP_MODERN, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreDontIncI":true,"optInstantDxyn":true,"optJump0Bxnn":true,"optLoresDxy0Is16x16":true,"optModeChangeClear":true,"optAllowHires":true,"instructionsPerFrame":30})"},
    {Opts::eMEGACHIP, R"({"optJustShiftVx":true,"optDontResetVf":true,"optLoadStoreDontIncI":true,"optInstantDxyn":true,"optLoresDxy0Is8x16":true,"optSC11Collision":true,"optModeChangeClear":true,"optAllowHires":true,"optHas16BitAddr":true,"instructionsPerFrame":3000,"frameRate":50})"},
    {Opts::eXOCHIP, R"({"optDontResetVf":true,"optWrapSprites":true,"optInstantDxyn":true,"optLoresDxy0Is16x16":true,"optModeChangeClear":true,"optAllowHires":true,"optAllowColors":true,"optHas16BitAddr":true,"optXOChipSound":true,"instructionsPerFrame":1000})"},
    {Opts::eCHICUEYI, R"({"optDontResetVf":true,"optWrapSprites":true,"optInstantDxyn":true,"optLoresDxy0Is16x16":true,"optModeChangeClear":true,"optAllowHires":true,"optAllowColors":true,"optHas16BitAddr":true,"optChicueyiSound":true,"instructionsPerFrame":1000})"},
    {Opts::eCHIP8VIP, R"({})"},
    {Opts::eCHIP8VIP_TPD, R"({"startAddress":608,"advanced":{"interpreter":"CHIP8TPD"}})"},
    {Opts::eCHIP8VIP_FPD, R"({"startAddress":580,"advanced":{"interpreter":"CHIP8FPD"}})"},
    {Opts::eCHIP8EVIP, R"({"advanced":{"interpreter":"CHIP8E"}})"},
    {Opts::eCHIP8XVIP, R"({"startAddress":768,"advanced":{"interpreter":"CHIP8X"}})"},
    {Opts::eCHIP8XVIP_TPD, R"({"startAddress":768,"advanced":{"interpreter":"CHIP8XTPD"}})"},
    {Opts::eCHIP8XVIP_FPD, R"({"startAddress":768,"advanced":{"interpreter":"CHIP8XFPD"}})"},
    {Opts::eCHIP8DREAM, R"({"frameRate":50})"},
    {Opts::eC8D68CHIPOSLO, R"({"frameRate":50,"advanced":{"kernel":"chiposlo"}})"}
};

Chip8EmulatorOptions::~Chip8EmulatorOptions() = default;

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
    else if(variant == chip8::Variant::CHIP_8E) return eCHIP8EVIP;
    else if(variant == chip8::Variant::CHIP_8X) return eCHIP8XVIP;
    else if(variant == chip8::Variant::CHIP_8X_TPD) return eCHIP8XVIP_TPD;
    else if(variant == chip8::Variant::HI_RES_CHIP_8X) return eCHIP8XVIP_FPD;
    else if(variant == chip8::Variant::CHIP_8_D6800) return eCHIP8DREAM;
    else if(variant == chip8::Variant::CHIP_8_D6800_LOP) return eC8D68CHIPOSLO;
    else if(variant == chip8::Variant::GENERIC_CHIP_8) return ePORTABLE;
    return eCHIP8;
}

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

Chip8EmulatorOptions Chip8EmulatorOptions::clone() const
{
    auto result = *this;
    result.advanced = nlohmann::json::parse(advanced.dump());
    return result;
}

bool Chip8EmulatorOptions::operator==(const Chip8EmulatorOptions& other) const
{
    return behaviorBase == other.behaviorBase && startAddress == other.startAddress && optJustShiftVx == other.optJustShiftVx && optDontResetVf == other.optDontResetVf && optLoadStoreIncIByX == other.optLoadStoreIncIByX &&
           optLoadStoreDontIncI == other.optLoadStoreDontIncI && optWrapSprites == other.optWrapSprites && optInstantDxyn == other.optInstantDxyn && optLoresDxy0Is8x16 == other.optLoresDxy0Is8x16 && optLoresDxy0Is16x16 == other.optLoresDxy0Is16x16 &&
           optSC11Collision == other.optSC11Collision && optSCLoresDrawing == other.optSCLoresDrawing && optHalfPixelScroll == other.optHalfPixelScroll && optModeChangeClear == other.optModeChangeClear && optJump0Bxnn == other.optJump0Bxnn &&
           optAllowHires == other.optAllowHires && optOnlyHires == other.optOnlyHires && optAllowColors == other.optAllowColors && optHas16BitAddr == other.optHas16BitAddr && optCyclicStack == other.optCyclicStack &&
           optXOChipSound == other.optXOChipSound && optExtendedVBlank == other.optExtendedVBlank && optChicueyiSound == other.optChicueyiSound && optTraceLog == other.optTraceLog && instructionsPerFrame == other.instructionsPerFrame &&
           frameRate == other.frameRate && advancedDump == other.advancedDump;
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
    SET_IF_CHANGED(obj, optSCLoresDrawing);
    SET_IF_CHANGED(obj, optHalfPixelScroll);
    SET_IF_CHANGED(obj, optModeChangeClear);
    SET_IF_CHANGED(obj, optJump0Bxnn);
    SET_IF_CHANGED(obj, optAllowHires);
    SET_IF_CHANGED(obj, optOnlyHires);
    SET_IF_CHANGED(obj, optAllowColors);
    SET_IF_CHANGED(obj, optHas16BitAddr);
    SET_IF_CHANGED(obj, optCyclicStack);
    SET_IF_CHANGED(obj, optXOChipSound);
    SET_IF_CHANGED(obj, optTraceLog);
    SET_IF_CHANGED(obj, instructionsPerFrame);
    SET_IF_CHANGED(obj, frameRate);
    j = obj;
    if(!o.advanced.empty()) {
        j["advanced"] = o.advanced;
    }
}

#define GET_OR_DEFAULT(o, j, n) o.n = j.value(#n, defaultOpts.n);
void from_json(const nlohmann::json& j, Chip8EmulatorOptions& o)
{
    auto variantName = j.value("behaviorBase",o.nameOfPreset(o.behaviorBase));
    o.behaviorBase = Chip8EmulatorOptions::presetForName(variantName);
    const Opts defaultOpts = Chip8EmulatorOptions::optionsOfPreset(o.behaviorBase);
    GET_OR_DEFAULT(o, j, startAddress);
    GET_OR_DEFAULT(o, j, optJustShiftVx);
    GET_OR_DEFAULT(o, j, optDontResetVf);
    GET_OR_DEFAULT(o, j, optLoadStoreIncIByX);
    GET_OR_DEFAULT(o, j, optLoadStoreDontIncI);
    GET_OR_DEFAULT(o, j, optWrapSprites);
    GET_OR_DEFAULT(o, j, optInstantDxyn);
    GET_OR_DEFAULT(o, j, optJump0Bxnn);
    GET_OR_DEFAULT(o, j, optAllowHires);
    GET_OR_DEFAULT(o, j, optOnlyHires);
    GET_OR_DEFAULT(o, j, optAllowColors);
    GET_OR_DEFAULT(o, j, optHas16BitAddr);
    GET_OR_DEFAULT(o, j, optCyclicStack);
    GET_OR_DEFAULT(o, j, optXOChipSound);
    GET_OR_DEFAULT(o, j, optTraceLog);
    GET_OR_DEFAULT(o, j, optLoresDxy0Is8x16);
    GET_OR_DEFAULT(o, j, optLoresDxy0Is16x16);
    GET_OR_DEFAULT(o, j, optSC11Collision);
    GET_OR_DEFAULT(o, j, optSCLoresDrawing);
    GET_OR_DEFAULT(o, j, optModeChangeClear);
    GET_OR_DEFAULT(o, j, optHalfPixelScroll);
    GET_OR_DEFAULT(o, j, instructionsPerFrame);
    GET_OR_DEFAULT(o, j, frameRate);
    if(j.contains("advanced")) {
        o.advanced = j.at("advanced");
        o.unifyColors();
        o.updatedAdvanced();
    }
}

void Chip8EmulatorOptions::unifyColors()
{
    std::array<uint32_t,256> palette;
    int maxIdx = -1;
    if(!advanced.empty()) {
        if(advanced.contains("backgroundColor")) {
            palette[0] = (std::strtoul(advanced.at("backgroundColor").get<std::string>().c_str() + 1, nullptr, 16) << 8) + 255;
            advanced.erase("backgroundColor");
            maxIdx = 0;
        }
        if(advanced.contains("col0")) {
            palette[0] =(std::strtoul(advanced.at("col0").get<std::string>().c_str() + 1, nullptr, 16) << 8) + 255;
            advanced.erase("col0");
            maxIdx = 0;
        }
        if(advanced.contains("fillColor")) {
            palette[1] = (std::strtoul(advanced.at("fillColor").get<std::string>().c_str() + 1, nullptr, 16) << 8) + 255;
            advanced.erase("fillColor");
            maxIdx = 1;
        }
        if(advanced.contains("col1")) {
            palette[1] = (std::strtoul(advanced.at("col1").get<std::string>().c_str() + 1, nullptr, 16) << 8) + 255;
            advanced.erase("col1");
            maxIdx = 1;
        }
        if(advanced.contains("fillColor2")) {
            palette[2] = (std::strtoul(advanced.at("fillColor2").get<std::string>().c_str() + 1, nullptr, 16) << 8) + 255;
            advanced.erase("fillColor2");
            maxIdx = 2;
        }
        if(advanced.contains("col2")) {
            palette[2] = (std::strtoul(advanced.at("col2").get<std::string>().c_str() + 1, nullptr, 16) << 8) + 255;
            advanced.erase("col2");
            maxIdx = 2;
        }
        if(advanced.contains("blendColor")) {
            palette[3] = (std::strtoul(advanced.at("blendColor").get<std::string>().c_str() + 1, nullptr, 16) << 8) + 255;
            advanced.erase("blendColor");
            maxIdx = 3;
        }
        if(advanced.contains("col3")) {
            palette[3] = (std::strtoul(advanced.at("col3").get<std::string>().c_str() + 1, nullptr, 16) << 8) + 255;
            advanced.erase("col3");
            maxIdx = 3;
        }
        if(advanced.contains("palette") && advanced.at("palette").is_array()) {
            auto pal = advanced.at("palette");
            size_t idx = 0;
            for(const auto& val : pal) {
                if(idx >= palette.size())
                    break;
                if(val.is_string()) {
                    auto str = val.get<std::string>();
                    if(str.length()>1 && str[0] == '#')
                        palette[idx] = (std::strtoul(str.data() + 1, nullptr, 16) << 8) + 255;
                }
                else if(val.is_number_unsigned()) {
                    palette[idx] = (val.get<uint32_t>() << 8) + 255;
                }
                maxIdx = idx++;
            }
        }
    }
    if(maxIdx >= 0 && (!advanced.contains("palette") || !advanced.at("palette").is_array())) {
        std::vector<std::string> pal(maxIdx + 1, "");
        for (size_t i = 0; i <= maxIdx; ++i) {
            pal[i] = fmt::format("#{:06x}", palette[i] >> 8);
        }
        advanced["palette"] = pal;
    }
}

void Chip8EmulatorOptions::updateColors(std::array<uint32_t,256>& palette)
{
    if(advanced.contains("palette") && advanced.at("palette").is_array()) {
        auto pal = advanced.at("palette");
        size_t idx = 0;
        for(const auto& val : pal) {
            if(idx >= palette.size())
                break;
            if(val.is_string()) {
                auto str = val.get<std::string>();
                if(str.length()>1 && str[0] == '#')
                    palette[idx] = (std::strtoul(str.data() + 1, nullptr, 16) << 8) + 255;
            }
            else if(val.is_number_unsigned()) {
                palette[idx] = (val.get<uint32_t>() << 8) + 255;
            }
            ++idx;
        }
    }
}

bool Chip8EmulatorOptions::hasColors() const
{
    return advanced.contains("palette") && advanced.at("palette").is_array();
}

void Chip8EmulatorOptions::updatedAdvanced()
{
    advancedDump = advanced.dump();
}

}
