//---------------------------------------------------------------------------------------
// src/emulation/properties.hpp
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

#include "properties.hpp"

#include <chiplet/utility.hpp>
#include <nlohmann/json.hpp>
#include <utility>

#include "c8db/database.hpp"

namespace emu {

std::map<std::string_view,Properties> Properties::propertyRegistry{};


Property::Property(const std::string& name, Value val, std::string description, std::string additionalInfo, PropertyAccess access_)
    : _name(name)
    , _jsonKey(Properties::makeJsonKey(name))
    , _optionName(toOptionName(name))
    , _value(std::move(val))
    , _description(std::move(description))
    , _additionalInfo(std::move(additionalInfo))
    , _access(access_)
{}

Property::Property(const std::string& name, Value val, std::string description, PropertyAccess access_)
    : _name(name)
    , _jsonKey(Properties::makeJsonKey(name))
    , _optionName(toOptionName(name))
    , _value(std::move(val))
    , _description(std::move(description))
    , _access(access_)
{}

Property::Property(const NameAndKeyName& nameAndKey, Value val, std::string description, PropertyAccess access_)
    : _name(nameAndKey.name)
    , _jsonKey(Properties::makeJsonKey(nameAndKey.keyName))
    , _optionName(toOptionName(nameAndKey.keyName))
    , _value(std::move(val))
    , _description(std::move(description))
    , _access(access_)
{}

Property::Property(const NameAndKeyName& nameAndKey, Value val, PropertyAccess access_)
    : _name(nameAndKey.name)
    , _jsonKey(Properties::makeJsonKey(nameAndKey.keyName))
    , _optionName(toOptionName(nameAndKey.keyName))
    , _value(std::move(val))
    , _description(nameAndKey.name)
    , _access(access_)
{}

Property::Property(const Property& other) = default;

Properties::RegistryMaps& Properties::getRegistryMaps()
{
    static RegistryMaps regMaps;
    return regMaps;
}

nlohmann::json Properties::createDiff(const Properties& other) const
{
    nlohmann::json result;
    if (other._class != _class)
        return {};
    for (const auto& key : _valueList) {
        if (auto iter = other._valueMap.find(key); iter != other._valueMap.end()) {
            if (iter->second != _valueMap.at(key)) {
                const auto& prop = iter->second;
                const auto& name = prop.getJsonKey();
                std::visit(emu::visitor{
                   [&](std::nullptr_t) { },
                   [&](bool val) { result[name] = val; },
                   [&](const emu::Property::Integer& val) { result[name] = val.intValue; },
                   [&](const std::string& val) { result[name] = val; },
                   [&](const emu::Property::Combo& val) { result[name] = val.selectedText(); }
               }, prop.getValue());
            }
        }
    }
    if(_palette != other._palette) {
        result["palette"] = other._palette;
    }
    return result;
}
void Properties::applyDiff(const nlohmann::json& diff)
{
    for (const auto& [key, value] : diff.items()) {
        if (auto* propPtr = find(key); propPtr != nullptr) {
            auto& prop = *propPtr;
            std::visit(visitor{
        [&](std::nullptr_t&) { },
        [&](bool& val) { val = value.get<bool>(); },
        [&](Property::Integer& val) { val.intValue = value.get<int>(); },
        [&](std::string& val) { val = value.get<std::string>(); },
        [&](Property::Combo& val) { val.setSelectedToText(value.get<std::string>()); }
            }, prop.getValue());
        }
    }
    if(diff.contains("palette")) {
        from_json(diff.at("palette"), _palette);
    }
}

void to_json(nlohmann::json& j, const Properties& props)
{
    j = nlohmann::json::object();
    j["class"] = props.propertyClass();
    for(size_t i = 0; i < props.numProperties(); ++i) {
        const auto& prop = props[i];
        const auto& name = prop.getJsonKey();
        std::visit(emu::visitor{
                       [&](std::nullptr_t) { },
                       [&](bool val) { j[name] = val; },
                       [&](const emu::Property::Integer& val) { j[name] = val.intValue; },
                       [&](const std::string& val) { j[name] = val; },
                       [&](const emu::Property::Combo& val) { j[name] = val.selectedText(); }
                   }, prop.getValue());
    }
    if(!props.palette().empty()) {
        j["palette"] = props.palette();
    }
}

void from_json(const nlohmann::json& j, Properties& props)
{
    if(j.is_object()) {
        auto cls = j.value("class", "CHIP-8 GENERIC");
        props = Properties::getProperties(cls);
        if(props) {
            for(size_t i = 0; i < props.numProperties(); ++i) {
                auto& prop = props[i];
                const auto& name = prop.getJsonKey();
                std::visit(visitor{
                    [&](std::nullptr_t&) { },
                    [&](bool& val) { val = j.value(name, val); },
                    [&](Property::Integer& val) { val.intValue = j.value(name, val.intValue); },
                    [&](std::string& val) { val = j.value(name, val); },
                    [&](Property::Combo& val) { val.setSelectedToText(j.value(name, val.selectedText())); }
                }, prop.getValue());
            }
        }
        if(j.contains("palette")) {
            j["palette"].get_to(props.palette());
        }
    }
}

void to_json(nlohmann::json& j, const Palette::Color& col)
{
    j = col.toStringRGB();
}

void from_json(const nlohmann::json& j, Palette::Color& col)
{
    if(j.is_string()) {
        col = Palette::Color(j.get<std::string>());
    }
    else {
        col = {0,0,0};
    }
}

void to_json(nlohmann::json& j, const Palette& pal)
{
    if(pal.borderColor || pal.signalColor || !pal.backgroundColors.empty()) {
        j["colors"] = pal.colors;
        if(pal.borderColor) {
            j["border"] = pal.borderColor.value();
        }
        if(pal.signalColor) {
            j["signal"] = pal.signalColor.value();
        }
        j["background"] = pal.backgroundColors;
    }
    else {
        j = pal.colors;
    }
}

void from_json(const nlohmann::json& j, Palette& pal)
{
    if(j.is_array()) {
        pal.colors.clear();
        pal.colors.reserve(j.size());
        for(const std::string col : j) {
            pal.colors.emplace_back(col);
        }
        pal.borderColor = {};
        pal.signalColor = {};
    }
    else if(j.is_object()) {
        pal.colors.clear();
        pal.colors.reserve(j.size());
        for(const std::string col : j["colors"]) {
            pal.colors.emplace_back(col);
        }
        if(j.count("border")) {
            pal.borderColor = Palette::Color(j["border"].get<std::string>());
        }
        if(j.count("signal")) {
            pal.signalColor = Palette::Color(j["signal"].get<std::string>());
        }
        if (j.count("background")) {
            for (const std::string col : j["background"]) {
                pal.backgroundColors.emplace_back(col);
            }
        }
    }
}

}
