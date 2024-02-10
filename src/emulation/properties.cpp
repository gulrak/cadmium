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

#include <nlohmann/json.hpp>


namespace emu {

std::map<std::string_view,Properties> Properties::propertyRegistry{};
std::unordered_map<std::string,std::string> Properties::registeredKeys{};
std::unordered_map<std::string,std::string> Properties::registeredJsonKeys{};

void to_json(nlohmann::json& j, const Properties& props)
{
    j = nlohmann::json::object();
    for(size_t i = 0; i < props.numProperties(); ++i) {
        const auto& prop = props[i];
        auto name = Properties::makeJsonKey(prop.getName());
        std::visit(emu::visitor{
                       [&](nullptr_t) { },
                       [&](bool val) { j[name] = val; },
                       [&](const emu::Property::Integer& val) { j[name] = val.intValue; },
                       [&](const std::string& val) { j[name] = val; },
                       [&](const emu::Property::Combo& val) { j[name] = val.selectedText(); }
                   }, prop.getValue());
    }
}

void from_json(const nlohmann::json& j, Properties& props)
{
    if(j.is_object()) {

    }
}


}
