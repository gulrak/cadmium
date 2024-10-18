//---------------------------------------------------------------------------------------
// src/configuration.hpp
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
#pragma once

#include <emulation/properties.hpp>
#include <nlohmann/json_fwd.hpp>
#include <sha1/sha1.hpp>
#include <map>
#include <string>

struct CadmiumConfiguration
{
    float volume{};
    uint16_t guiHue{200};
    uint8_t guiSat{80};
    std::string workingDirectory;
    std::string databaseDirectory;
    emu::Properties emuProperties;
    std::map<Sha1::Digest,emu::Properties> romConfigs;
    bool load(const std::string& filepath);
    bool save(const std::string& filepath);
};

void to_json(nlohmann::json& j, const CadmiumConfiguration& cc);
void from_json(const nlohmann::json& j, CadmiumConfiguration& cc);
void to_json(nlohmann::json& j, const Sha1::Digest& d);
void from_json(const nlohmann::json& j, Sha1::Digest& d);