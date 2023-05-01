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

void to_json(nlohmann::json& j, const CadmiumConfiguration& cc) {
    j = nlohmann::json{ {"workingDirectory", cc.workingDirectory}, {"databaseDirectory", cc.databaseDirectory}, {"emuOptions", cc.emuOptions}, {"romConfigs", cc.romConfigs} };
}

void from_json(const nlohmann::json& j, CadmiumConfiguration& cc) {
    j.at("workingDirectory").get_to(cc.workingDirectory);
    cc.databaseDirectory = j.value("databaseDirectory", "");
    j.at("emuOptions").get_to(cc.emuOptions);
    j.at("romConfigs").get_to(cc.romConfigs);
}

bool CadmiumConfiguration::load(const std::string& filepath)
{
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
    return true;
}

bool CadmiumConfiguration::save(const std::string& filepath)
{
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
    return true;
}