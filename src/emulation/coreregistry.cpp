//---------------------------------------------------------------------------------------
// src/emulation/coreregistry.cpp
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

#include "coreregistry.hpp"

#include <chiplet/utility.hpp>
#include <chiplet/octocartridge.hpp>

#include <set>

#include <nlohmann/json.hpp>

namespace emu {

void CoreRegistry::IFactoryInfo::cacheVariantMappings() const
{
    if(presetMappings.empty()) {
        for (size_t i = 0; i < numberOfVariants(); ++i) {
            if(prefix().empty()) {
                presetMappings.emplace(toOptionName(variantName(i)), i);
            }
            else {
                presetMappings.emplace(toOptionName(prefix() + '-' + variantName(i)), i);
                // the bare prefix is an alias for the NONE variant of a hardware core
                if(variantName(i) == "NONE")
                    presetMappings.emplace(toOptionName(prefix()), i);
            }
        }
    }
}

bool CoreRegistry::IFactoryInfo::hasVariant(const std::string& variant) const
{
    cacheVariantMappings();
    return presetMappings.find(toOptionName(variant)) != presetMappings.end();
}

Properties CoreRegistry::IFactoryInfo::variantProperties(const std::string& variant) const
{
    cacheVariantMappings();
    if(const auto iter = presetMappings.find(toOptionName(variant)); iter != presetMappings.end())
        return variantProperties(iter->second);
    return {};
}

static const char* font5pxOfOctoFont(OctoOptions::Font font)
{
    switch(font) {
        case OctoOptions::Font::Vip: return "VIP";
        case OctoOptions::Font::Dream6800: return "DREAM6800";
        case OctoOptions::Font::Eti660: return "ETI660";
        case OctoOptions::Font::SChip: return "SCHIP";
        case OctoOptions::Font::FishNChips: return "FISH";
        default: return "OCTO";
    }
}

static const char* font10pxOfOctoFont(OctoOptions::Font font)
{
    switch(font) {
        case OctoOptions::Font::Octo: return "OCTO";
        case OctoOptions::Font::SChip: return "SCHIP11";
        case OctoOptions::Font::FishNChips: return "FISH";
        default: return "DEFAULT";
    }
}

static OctoOptions::Font octoFontOfFont5px(const std::string& name)
{
    if(name == "VIP") return OctoOptions::Font::Vip;
    if(name == "DREAM6800") return OctoOptions::Font::Dream6800;
    if(name == "ETI660") return OctoOptions::Font::Eti660;
    if(name == "SCHIP") return OctoOptions::Font::SChip;
    if(name == "FISH") return OctoOptions::Font::FishNChips;
    return OctoOptions::Font::Octo;
}

Properties CoreRegistry::propertiesFromOctoOptions(const OctoOptions& options)
{
    Properties result;
    auto pXO = propertiesForPreset("XO-CHIP");
    auto j = nlohmann::json::object();
    to_json(j, pXO);
    j["class"] = "CHIP-8 GENERIC";
    j["behaviorBase"] = "XO-CHIP";
    j["memory"] = "65536";
    j["cleanRam"] = true;
    j["frameRate"] = 60;
    j["instructionsPerFrame"] = options.tickrate;
    j["justShiftVx"] = options.qShift;
    j["loadStoreIncIByXPlus1"] = !options.qLoadStore;
    j["loadStoreIncIByx"] = false;  // the id really is spelled with a lower case x
    j["jump0Bxnn"] = options.qJump0;
    j["dontResetVf"] = !options.qLogic;
    j["wrapSprites"] = !options.qClip;
    j["instantDxyn"] = !options.qVBlank;
    from_json(j, result);
    // Octo's quiet and buzz colors are what the border shows without and with sound.
    auto color = [&options](size_t index) { return Palette::Color::fromRGB(options.colors[index]); };
    result.palette().colors = {color(0), color(1), color(2), color(3)};
    // A palette in the cartridge has the colors of all four planes, its first four are also in colors.
    for(size_t i = 0; i < options.palette.size(); ++i) {
        const auto paletteColor = Palette::Color::fromRGB(options.palette[i]);
        if(i < result.palette().colors.size())
            result.palette().colors[i] = paletteColor;
        else
            result.palette().colors.push_back(paletteColor);
    }
    result.palette().borderColor = color(4);
    result.palette().signalColor = color(5);
    result["screenRotation"].setSelectedIndex(options.rotation / 90);
    result["font5px"].setSelectedText(font5pxOfOctoFont(options.font));
    result["font10px"].setSelectedText(font10pxOfOctoFont(options.font));
    result["touchMode"].setSelectedIndex(static_cast<size_t>(options.touchMode));
    return result;
}

OctoOptions CoreRegistry::octoOptionsFromProperties(const Properties& properties)
{
    OctoOptions result;
    auto flag = [&properties](std::string_view id, bool defaultValue) {
        const auto* prop = properties.find(id);
        return prop ? prop->getBool() : defaultValue;
    };
    result.qShift = flag("justShiftVx", result.qShift);
    result.qLoadStore = !flag("loadStoreIncIByXPlus1", !result.qLoadStore);
    result.qJump0 = flag("jump0Bxnn", result.qJump0);
    result.qLogic = !flag("dontResetVf", !result.qLogic);
    result.qClip = !flag("wrapSprites", !result.qClip);
    result.qVBlank = !flag("instantDxyn", !result.qVBlank);
    if(const auto* prop = properties.find(propid::INSTRUCTIONS_PER_FRAME)) {
        // 0 means as fast as possible here, Octo needs a real number, this is its fastest setting
        result.tickrate = prop->getInt() > 0 ? prop->getInt() : 10000;
    }
    if(const auto* prop = properties.find("screenRotation")) {
        result.rotation = static_cast<int>(prop->getSelectedIndex()) * 90;
    }
    if(const auto* prop = properties.find("font5px")) {
        result.font = octoFontOfFont5px(prop->getSelectedText());
    }
    if(const auto* prop = properties.find("touchMode")) {
        result.touchMode = static_cast<OctoOptions::Touch>(prop->getSelectedIndex());
    }
    // A program too big for this gets a higher limit when the cartridge is written.
    const auto* base = properties.find("behaviorBase");
    const auto* memory = properties.find("memory");
    const auto baseName = base ? base->getSelectedText() : std::string{};
    if(baseName == "CHIP-8") {
        result.maxRom = 3232;
    }
    else if(baseName == "CHIP-48" || baseName.starts_with("SCHIP")) {
        result.maxRom = 3583;
    }
    else {
        result.maxRom = memory && (memory->getSelectedText() == "65536" || memory->getSelectedText() == "16777216") ? 65024 : 3584;
    }
    const auto& palette = properties.palette();
    auto rgb = [&palette](size_t index) { return palette.colors[index].toRGBInt(); };
    if(palette.size() >= 2) {
        result.colors[0] = rgb(0);
        result.colors[1] = rgb(1);
        // the planes above the first one only exist with four colors
        result.colors[2] = palette.size() >= 4 ? rgb(2) : rgb(1);
        result.colors[3] = palette.size() >= 4 ? rgb(3) : rgb(1);
        // the same fallback the screen uses
        result.colors[4] = palette.borderColor ? palette.borderColor->toRGBInt() : rgb(0);
        result.colors[5] = palette.signalColor ? palette.signalColor->toRGBInt() : result.colors[4];
    }
    // More than two planes need the palette addition, Octo's four colors can't hold them.
    if(palette.size() > 4) {
        for(size_t i = 0; i < palette.size() && i < OctoOptions::MAX_PALETTE_SIZE; ++i)
            result.palette.push_back(rgb(i));
    }
    return result;
}

bool CoreRegistry::supportsCartridge(const Properties& properties)
{
    if(properties.propertyClass() != "CHIP-8 GENERIC")
        return false;
    // These have instructions Octo doesn't know, or a display it doesn't have.
    const auto* base = properties.find("behaviorBase");
    if(!base)
        return false;
    const auto& name = base->getSelectedText();
    return name == "CHIP-8" || name == "CHIP-48" || name.starts_with("SCHIP") || name == "XO-CHIP";
}

CoreRegistry::CoreRegistry()
{
    for(auto& [key, info] : factoryMap()) {
        orderedFactories.emplace_back(key, info.get());
    }
    std::sort(orderedFactories.begin(), orderedFactories.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.second->score != rhs.second->score)
            return lhs.second->score < rhs.second->score;
        return lhs.first < rhs.first;
    });
    for(auto& [key, info] : orderedFactories) {
        if(!coresCombo.empty())
            coresCombo += ";";
        coresCombo += key;
        if(info->variantsCombo.empty()) {
            for(size_t i = 0; i < info->numberOfVariants(); ++i) {
                if(!info->variantsCombo.empty()) {
                    info->variantsCombo += ";";
                }
                info->variantsCombo += info->variantName(i);
                auto defaultExtensions = split(info->variantExtensions(i), ';');
                supportedExtensions.insert(defaultExtensions.begin(), defaultExtensions.end());
            }
        }

    }
}

CoreRegistry::FactoryMap& CoreRegistry::factoryMap()
{
    static FactoryMap factories{};
    return factories;
}

} // emu
