//---------------------------------------------------------------------------------------
// tools/c8sst-run.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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
#include "c8sst/runner.hpp"
#include <ghc/cli.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

int main(int argc, char* argv[])
{
    try {
        ghc::CLI cli(argc, argv);
        std::string preset = "strict-chip-8";
        std::string testsPath;
        std::string family;
        std::string caseId;
        std::string reportPath;
        int maxFailures = 20;
        bool listPresets = false;
        bool failFast = false;
        cli.option({"-p", "--preset"}, preset, "target preset (default: strict-chip-8)");
        cli.option({"-t", "--tests"}, testsPath, "corpus directory or profile.json");
        cli.option({"--family"}, family, "run only a named instruction family");
        cli.option({"--case"}, caseId, "run only an exact case ID");
        cli.option({"-o", "--report"}, reportPath, "write a JSON result report to a new file");
        cli.option({"--max-failures"}, maxFailures, "maximum detailed console failures (default: 20; 0: summary only)");
        cli.option({"--fail-fast"}, failFast, "stop after the first conformance failure");
        cli.option({"--list-presets"}, listPresets, "list implemented target adapters");
        cli.parse();
        if (listPresets) {
            std::cout << "vip-chip-8\nstrict-chip-8\nchip-8\n";
            return 0;
        }
        if (!c8sst::isRunnerPreset(preset)) throw std::runtime_error("Unsupported target preset '" + preset + "'");
        if (testsPath.empty()) throw std::runtime_error("--tests is required");
        if (maxFailures < 0) throw std::runtime_error("--max-failures must be nonnegative");
        if (!reportPath.empty() && std::filesystem::exists(reportPath)) throw std::runtime_error("Report file already exists");
        const auto corpus = c8sst::loadCorpus(testsPath);
        size_t selected = 0;
        for (const auto& test : corpus.cases)
            if ((family.empty() || family == test.family) && (caseId.empty() || caseId == test.id)) ++selected;
        if (!selected) throw std::runtime_error("Filters selected no tests");
        c8sst::Json report{{"format", "chip8-sst-results"}, {"version", 1}, {"profile", corpus.profile},
                          {"preset", preset}, {"selected", selected}, {"failures", c8sst::Json::array()}};
        std::map<std::string, std::pair<size_t, size_t>> families;
        size_t passed = 0;
        size_t failed = 0;
        for (const auto& test : corpus.cases) {
            if ((!family.empty() && family != test.family) || (!caseId.empty() && caseId != test.id)) continue;
            auto result = c8sst::runCase(test, corpus, preset);
            if (result["passed"].get<bool>()) {
                ++passed;
                ++families[test.family].first;
            }
            else {
                ++failed;
                ++families[test.family].second;
                if (failed <= static_cast<size_t>(maxFailures))
                    std::cerr << "FAIL " << test.id << " " << result["differences"].dump() << '\n';
                report["failures"].push_back(std::move(result));
                if (failFast) break;
            }
        }
        report["tested"] = passed + failed;
        report["passed"] = passed;
        report["failed"] = failed;
        report["complete"] = passed + failed == selected;
        report["families"] = c8sst::Json::object();
        for (const auto& [name, counts] : families) {
            report["families"][name] = {{"passed", counts.first}, {"failed", counts.second}};
            std::cout << name << ": " << counts.first << " passed, " << counts.second << " failed\n";
        }
        if (!reportPath.empty()) {
            std::ofstream stream(reportPath);
            stream.exceptions(std::ios::failbit | std::ios::badbit);
            stream << report.dump(2) << '\n';
        }
        std::cout << preset << " against " << corpus.profile << ": " << passed << " passed, " << failed << " failed";
        if (passed + failed < selected) std::cout << ", " << selected - passed - failed << " not run";
        std::cout << '\n';
        return failed ? 1 : 0;
    }
    catch (const std::exception& error) {
        std::cerr << "c8sst-run: " << error.what() << '\n';
        return 2;
    }
}
