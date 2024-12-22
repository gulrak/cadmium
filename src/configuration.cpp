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

#include <configuration.hpp>
#include <nlohmann/json.hpp>
#include <fstream>

void to_json(nlohmann::json& j, const Sha1::Digest& d)
{
    j = d.to_hex();
}

void from_json(const nlohmann::json& j, Sha1::Digest& d)
{
    d = Sha1::Digest(j.get<std::string>());
}

template <typename T> inline
void to_json(nlohmann::json& j, const std::map<Sha1::Digest, T>& m)
{
    using std::to_string;
    for (const auto& e : m)
    {
        j[e.first.to_hex()] = e.second;
    }
}

template <typename T> inline
void from_json(const nlohmann::json& j, std::map<Sha1::Digest, T>& m)
{
    for (const auto& [k, v] : j.items())
    {
        Sha1::Digest d;
        from_json(k, d);
        v.get_to(m[d]);
    }
}

void to_json(nlohmann::json& j, const CadmiumConfiguration& cc) {
    j = nlohmann::json{
        {"volume", cc.volume},
        {"guiHue", cc.guiHue},
        {"guiSaturation", cc.guiSat},
        {"workingDirectory", cc.workingDirectory},
        {"libraryPath", cc.libraryPath},
        {"emuProperties", cc.emuProperties},
        {"romConfigs", cc.romConfigs}
    };
}

void from_json(const nlohmann::json& j, CadmiumConfiguration& cc) {
    j.at("workingDirectory").get_to(cc.workingDirectory);
    if (!j.contains("libraryPath") && j.contains("databaseDirectory")) {
        cc.libraryPath = j.value("databaseDirectory", "");
    }
    else {
        cc.libraryPath = j.value("libraryPath", "");
    }
    cc.volume = j.value("volume", 0.5f);
    cc.guiHue = j.value("guiHue", 192);
    cc.guiSat = j.value("guiSaturation", 90);
    try {
        j.at("emuProperties").get_to(cc.emuProperties);
    }
    catch(...) {}
    try {
        j.at("romConfigs").get_to(cc.romConfigs);
    }
    catch(...) {}
}

bool CadmiumConfiguration::load(const std::string& filepath)
{
#ifndef PLATFORM_WEB
    std::ifstream ifs(filepath);
    if(ifs.fail())
        return false;
    try {
        auto cfg = nlohmann::json::parse(ifs);
        cfg.get_to(*this);
    }
    catch(...) {
        return false;
    }
#endif
    return true;
}

bool CadmiumConfiguration::save(const std::string& filepath)
{
#ifndef PLATFORM_WEB
    std::ofstream ofs(filepath);
    if(ofs.fail())
        return false;
    try {
        nlohmann::json j = *this;
        ofs << j.dump(4);
    }
    catch(...) {
        return false;
    }
#endif
    return true;
}