//---------------------------------------------------------------------------------------
// tools/c8sst/codec.cpp
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
#include "runner.hpp"
#include <fstream>
#include <set>
#include <stdexcept>

namespace c8sst {
namespace {
uint32_t number(const Json& value, uint32_t maximum)
{
    if (!value.is_number_integer() || value.get<int64_t>() < 0 || value.get<uint64_t>() > maximum)
        throw std::runtime_error("Integer value out of range");
    return value.get<uint32_t>();
}

std::vector<uint8_t> decodeHex(const Json& value)
{
    if (!value.is_string()) throw std::runtime_error("Hex data must be a string");
    const auto text = value.get<std::string>();
    if (text.empty() || text.size() % 2) throw std::runtime_error("Hex data must contain complete bytes");
    auto digit = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::runtime_error("Invalid hex digit");
    };
    std::vector<uint8_t> result;
    for (size_t n = 0; n < text.size(); n += 2)
        result.push_back((digit(text[n]) << 4) | digit(text[n + 1]));
    return result;
}

void patches(std::vector<uint8_t>& bytes, const Json& ranges, bool ram)
{
    if (!ranges.is_array()) throw std::runtime_error("Replacement ranges must be an array");
    size_t previousEnd = 0;
    for (const auto& range : ranges) {
        if (!range.is_array() || range.size() != 2) throw std::runtime_error("Invalid replacement range");
        const auto offset = number(range[0], bytes.size());
        const auto data = decodeHex(range[1]);
        if (offset < previousEnd || data.size() > bytes.size() - offset)
            throw std::runtime_error("Overlapping or out-of-bounds replacement range");
        const auto end = offset + data.size();
        if (ram && !(end <= 0xE90 || (offset >= 0xF00 && end <= 0x1000)))
            throw std::runtime_error("RAM replacement touches unasserted workspace");
        std::copy(data.begin(), data.end(), bytes.begin() + offset);
        previousEnd = end;
    }
}

std::vector<uint8_t> display(const Json& value, const std::vector<uint8_t>* initial = nullptr)
{
    if (!value.is_object()) throw std::runtime_error("Display must be an object");
    if (value.contains("base")) {
        if (!initial || value.at("base") != "initial" || value.size() != 2)
            throw std::runtime_error("Invalid initial-relative display");
        auto result = *initial;
        Json ranges = Json::array();
        if (!value.at("replace").is_array()) throw std::runtime_error("Display replacements must be an array");
        for (const auto& range : value.at("replace")) {
            if (!range.is_array() || range.size() != 3 || number(range[0], 0) != 0)
                throw std::runtime_error("Invalid plane replacement");
            ranges.push_back(Json::array({range[1], range[2]}));
        }
        patches(result, ranges, false);
        return result;
    }
    if (value.at("width") != 64 || value.at("height") != 32 || value.at("mode") != "low" ||
        !value.at("planes").is_array() || value.at("planes").size() != 1)
        throw std::runtime_error("Unsupported display dimensions, mode, or planes for VIP profile");
    const auto& plane = value.at("planes")[0];
    if (!plane.is_object() || plane.size() != 1) throw std::runtime_error("Plane must contain exactly fill or data");
    if (plane.contains("fill")) return std::vector<uint8_t>(256, number(plane.at("fill"), 255));
    auto result = decodeHex(plane.at("data"));
    if (result.size() != 256) throw std::runtime_error("Wrong packed display size");
    return result;
}

Json registers(const Json& state)
{
    Json result;
    for (const char* name : {"pc", "i", "dt", "st"})
        result[name] = number(state.at(name), std::string(name) == "pc" || std::string(name) == "i" ? 65535 : 255);
    if (result["dt"] != 0 || result["st"] != 0) throw std::runtime_error("Nonzero timers are outside this profile");
    const auto& values = state.at("v");
    const auto& stack = state.at("stack");
    if (!values.is_array() || values.size() != 16 || !stack.is_array() || stack.size() > 12)
        throw std::runtime_error("Invalid register or stack array");
    result["v"] = Json::array();
    result["stack"] = Json::array();
    for (const auto& value : values) result["v"].push_back(number(value, 255));
    for (const auto& value : stack) result["stack"].push_back(number(value, 65535));
    return result;
}

Snapshot state(const Json& value, const Snapshot& base, bool final)
{
    Snapshot result{registers(value), base.memory, base.display};
    if (value.contains("ram")) patches(result.memory, value.at("ram"), true);
    if (value.contains("display")) result.display = display(value.at("display"), final ? &base.display : nullptr);
    if (!std::equal(result.display.begin(), result.display.end(), result.memory.begin() + 0xF00))
        throw std::runtime_error("RAM and display initializations/expectations contradict the VIP alias");
    return result;
}

Json readJson(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream) throw std::runtime_error("Cannot read '" + path.string() + "'");
    Json result;
    stream >> result;
    return result;
}

void validateOpcode(const LoadedCase& test)
{
    const auto pc = test.initial.registers.at("pc").get<uint32_t>();
    if (pc < 0x200 || pc + 1 >= 0xE90 ||
        ((test.initial.memory[pc] << 8) | test.initial.memory[pc + 1]) != test.opcode)
        throw std::runtime_error("Opcode does not match executable initial RAM");
    const auto op = test.opcode;
    const auto family = op >> 12;
    const auto suffix = op & 255;
    const auto x = (op >> 8) & 15;
    const auto depth = test.initial.registers.at("stack").size();
    bool valid = false;
    switch (family) {
        case 0: valid = op == 0x00E0 || (op == 0x00EE && depth > 0); break;
        case 1: case 3: case 4: case 6: case 7: case 0xA: case 0xB: case 0xD: valid = true; break;
        case 2: valid = depth < 12; break;
        case 5: case 9: valid = (op & 15) == 0; break;
        case 8: valid = (op & 15) <= 7 || (op & 15) == 14; break;
        case 0xE: valid = suffix == 0x9E || suffix == 0xA1; break;
        case 0xF:
            valid = suffix == 0x07 || suffix == 0x1E || suffix == 0x29 || suffix == 0x33 || suffix == 0x55 || suffix == 0x65 ||
                    ((suffix == 0x15 || suffix == 0x18) && test.initial.registers.at("v")[x] == 0);
            break;
    }
    if (!valid) throw std::runtime_error("Instruction/input is outside the deterministic classic profile");
    size_t span = 0;
    if (family == 0xD) span = op & 15;
    if (family == 0xF && suffix == 0x33) span = 3;
    if (family == 0xF && (suffix == 0x55 || suffix == 0x65)) span = x + 1;
    if (span) {
        const auto index = test.initial.registers.at("i").get<uint32_t>();
        if (!((index + span <= 0xE90) || (index >= 0xF00 && index + span <= 0x1000)))
            throw std::runtime_error("Instruction accesses workspace, missing ROM fixtures, or out-of-bounds RAM");
    }
}
} // namespace

Corpus loadCorpus(const std::filesystem::path& path)
{
    const auto manifestPath = std::filesystem::is_directory(path) ? path / "profile.json" : path;
    const auto manifest = readJson(manifestPath);
    if (manifest.at("format") != "chip8-sst" || manifest.at("version") != 1 || manifest.at("profile") != "chip8-vip-4k-v1")
        throw std::runtime_error("Unsupported SST format, version, or profile");
    const auto& machine = manifest.at("machine");
    const auto& regions = machine.at("memory_regions");
    const bool validRegions = regions.is_array() && regions.size() == 2 &&
        number(regions[0].at("address"), 4096) == 0 && number(regions[0].at("size"), 4096) == 0xE90 &&
        number(regions[1].at("address"), 4096) == 0xF00 && number(regions[1].at("size"), 4096) == 0x100;
    if (machine.at("dialect") != "chip8-vip" || machine.at("memory_size") != 4096 || machine.at("stack_capacity") != 12 ||
        !validRegions || machine.at("display_alias").at("address") != 0xF00 ||
        machine.at("display_alias").at("plane") != 0 || machine.at("execution") != "one-completed-instruction" ||
        machine.at("timers") != "zero-only")
        throw std::runtime_error("Machine metadata contradicts the VIP profile");
    const auto& defaults = manifest.at("defaults");
    Snapshot baseline{registers(defaults), std::vector<uint8_t>(4096, number(defaults.at("ram").at("fill"), 255)), display(defaults.at("display"))};
    patches(baseline.memory, defaults.at("ram").at("replace"), true);
    if (!std::equal(baseline.display.begin(), baseline.display.end(), baseline.memory.begin() + 0xF00))
        throw std::runtime_error("Default RAM and display contradict the VIP alias");
    const auto defaultKeys = number(defaults.at("keys"), 65535);
    Corpus result{"chip8-vip-4k-v1", {{0, 0xE90}, {0xF00, 0x100}}, {}};
    std::set<std::string> ids;
    std::set<std::string> paths;
    if (!manifest.at("files").is_array()) throw std::runtime_error("Files must be an array");
    for (const auto& file : manifest.at("files")) {
        const auto name = file.at("path").get<std::string>();
        const std::filesystem::path relative(name);
        if (relative.empty() || relative.is_absolute() || relative.has_parent_path() || relative.extension() != ".json" ||
            !paths.insert(name).second || name == "profile.json")
            throw std::runtime_error("Invalid or duplicate shard path");
        const auto tests = readJson(manifestPath.parent_path() / relative);
        if (!tests.is_array() || tests.size() != number(file.at("count"), UINT32_MAX))
            throw std::runtime_error("Shard count does not match its data");
        for (const auto& test : tests) {
            const auto id = test.at("id").get<std::string>();
            try {
                if (id.empty() || !ids.insert(id).second) throw std::runtime_error("Empty or duplicate case ID");
                const auto code = decodeHex(test.at("opcode"));
                if (code.size() != 2) throw std::runtime_error("Opcode must contain two bytes");
                auto initial = state(test.at("initial"), baseline, false);
                auto final = state(test.at("final"), initial, true);
                LoadedCase loaded{id, relative.stem().string(), static_cast<uint16_t>((code[0] << 8) | code[1]),
                                  std::move(initial), std::move(final),
                                  test.at("initial").contains("keys") ? static_cast<uint16_t>(number(test.at("initial").at("keys"), 65535)) : static_cast<uint16_t>(defaultKeys)};
                validateOpcode(loaded);
                result.cases.push_back(std::move(loaded));
            }
            catch (const std::exception& error) {
                throw std::runtime_error(id + ": " + error.what());
            }
        }
    }
    if (result.cases.empty() || result.cases.size() != number(manifest.at("test_count"), UINT32_MAX))
        throw std::runtime_error("Empty corpus or incorrect total test count");
    return result;
}
} // namespace c8sst
