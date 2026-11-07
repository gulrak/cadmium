//---------------------------------------------------------------------------------------
// tools/c8sst-gen.cpp
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
#include "c8sst/model.hpp"
#include <ghc/cli.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

int main(int argc, char* argv[])
{
    try {
        ghc::CLI cli(argc, argv);
        std::string preset = "vip-chip-8";
        std::string outputDir;
        std::string family;
        int64_t seed = 0xC8;
        int randomCases = 16;
        bool listPresets = false;
        cli.option({"-p", "--preset"}, preset, "oracle preset (default: vip-chip-8)");
        cli.option({"-o", "--output-dir"}, outputDir, "new directory for profile.json and instruction-family files");
        cli.option({"--family"}, family, "generate only a named family (default: all)");
        cli.option({"--seed"}, seed, "deterministic random seed (default: 200)");
        cli.option({"-n", "--random"}, randomCases, "additional random cases per ALU operation (default: 16)");
        cli.option({"--list-presets"}, listPresets, "list profiles with implemented oracle adapters");
        cli.parse();
        if (listPresets) {
            std::cout << "vip-chip-8  chip8-vip-4k-v1\n";
            return 0;
        }
        if (randomCases < 0 || randomCases > 10000) throw std::runtime_error("--random must be between 0 and 10000");
        if (seed < 0 || seed > UINT32_MAX) throw std::runtime_error("--seed must be between 0 and 4294967295");
        if (outputDir.empty()) throw std::runtime_error("--output-dir is required");
        const auto profile = c8sst::profileForPreset(preset);
        const auto cases = c8sst::classicCases(seed, randomCases);
        std::set<std::string> families;
        for (const auto& test : cases) families.insert(test.family);
        if (!family.empty() && !families.contains(family)) throw std::runtime_error("Unknown test family '" + family + "'");
        // Refuse overwrites to avoid mixing different generations/profiles.
        if (std::filesystem::exists(outputDir)) throw std::runtime_error("Output directory already exists");
        const auto baseline = c8sst::resetSnapshot(profile);
        std::map<std::string, c8sst::Json> shards;
        std::set<std::string> ids;
        size_t count = 0;
        for (const auto& test : cases) {
            if (!family.empty() && test.family != family) continue;
            auto result = c8sst::generate(profile, test, baseline);
            if (!ids.insert(result["id"].get<std::string>()).second) throw std::runtime_error("Duplicate test ID");
            auto& shard = shards[test.family];
            if (shard.is_null()) shard = c8sst::Json::array();
            shard.push_back(std::move(result));
            if (++count % 250 == 0) std::clog << "Generated " << count << " cases\n";
        }
        auto manifest = c8sst::profileDocument(profile, baseline, seed, randomCases);
        manifest["files"] = c8sst::Json::array();
        manifest["test_count"] = count;
        for (const auto& [name, tests] : shards)
            manifest["files"].push_back({{"path", name + ".json"}, {"count", tests.size()}});
        // Only publish files after all oracle executions have succeeded.
        std::filesystem::create_directories(outputDir);
        auto save = [&](const std::string& name, const c8sst::Json& data) {
            std::ofstream stream(std::filesystem::path(outputDir) / name);
            stream.exceptions(std::ios::failbit | std::ios::badbit);
            stream << data.dump(2) << '\n';
        };
        for (const auto& [name, tests] : shards) save(name + ".json", tests);
        save("profile.json", manifest);
        std::clog << "Wrote " << count << " tests to " << outputDir << '\n';
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "c8sst-gen: " << error.what() << '\n';
        return 1;
    }
}
