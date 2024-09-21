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


namespace emu {

std::map<std::string_view,Properties> Properties::propertyRegistry{};

std::string Palette::Color::toString() const
{
    return fmt::format("#{:02x}{:02x}{:02x}", r, g, b);
}


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

void to_json(nlohmann::json& j, const Palette& pal)
{
    j = nlohmann::json::array();
    for(const auto& c : pal.colors) {
        j.push_back(c.toString());
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
    }
}

}
