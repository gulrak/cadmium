//---------------------------------------------------------------------------------------
// tools/c8sst/oracle.cpp
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
#include "model.hpp"
#include "execution.hpp"
#include "../../test/chip8headless.hpp"
#include <emulation/chip8realcorebase.hpp>
#include <emulation/utility.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <stdexcept>

namespace c8sst {
namespace {
constexpr uint32_t START_ADDRESS = 0x200;

Snapshot capture(const Profile& profile, emu::IChip8Emulator& core)
{
    Snapshot result;
    std::vector<uint8_t> registers;
    for (uint8_t r = 0; r < 16; ++r) registers.push_back(core.getV(r));
    std::vector<uint32_t> stack;
    if (core.getSP() > profile.stackCapacity) throw std::runtime_error("Oracle stack exceeds profile capacity");
    for (uint32_t s = 0; s < core.getSP(); ++s) stack.push_back(core.stackElement(s));
    result.registers = {{"pc", core.getPC()}, {"i", core.getI()}, {"v", registers},
                        {"stack", stack}, {"dt", core.delayTimer()}, {"st", core.soundTimer()}};
    result.memory.resize(core.memSize(), 0);
    for (auto range : profile.memoryRanges)
        std::copy_n(core.memory() + range.address, range.size, result.memory.begin() + range.address);
    auto displaySize = profile.displayWidth * profile.displayHeight / 8;
    result.display.assign(core.memory() + profile.displayAddress, core.memory() + profile.displayAddress + displaySize);
    return result;
}


Json displayState(const Profile& profile, const std::vector<uint8_t>& display)
{
    Json plane;
    if (std::all_of(display.begin(), display.end(), [&](uint8_t b) { return b == display.front(); }))
        plane = {{"fill", display.front()}};
    else
        plane = {{"data", hexBytes(display)}};
    return {{"width", profile.displayWidth}, {"height", profile.displayHeight},
            {"mode", "low"}, {"planes", Json::array({plane})}};
}
} // namespace

Profile profileForPreset(const std::string& preset)
{
    // Add a profile and its capture/initialization rules here before accepting
    // another preset: selecting an arbitrary core does not establish semantics.
    if (preset == "vip-chip-8")
        return {"chip8-vip-4k-v1", preset, {{0, 0xE90}, {0xF00, 0x100}}, 0xF00, 64, 32, 12};
    throw std::runtime_error("Unsupported preset '" + preset + "'; supported: vip-chip-8");
}

std::string hexBytes(const std::vector<uint8_t>& bytes)
{
    constexpr char HEX[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (auto byte : bytes) {
        result.push_back(HEX[byte >> 4]);
        result.push_back(HEX[byte & 15]);
    }
    return result;
}

Json replacementRanges(const std::vector<uint8_t>& initial, const std::vector<uint8_t>& final)
{
    if (initial.size() != final.size()) throw std::runtime_error("State sizes differ");
    Json ranges = Json::array();
    for (size_t offset = 0; offset < final.size();) {
        if (initial[offset] == final[offset]) { ++offset; continue; }
        const auto start = offset;
        while (offset < final.size() && initial[offset] != final[offset]) ++offset;
        ranges.push_back(Json::array({start, hexBytes({final.begin() + start, final.begin() + offset})}));
    }
    return ranges;
}

Snapshot resetSnapshot(const Profile& profile)
{
    auto [host, core, start] = createChip8Instance(profile.preset);
    if (!core) throw std::runtime_error("Preset has no CHIP-8 execution unit");
    core->reset();
    if (start != START_ADDRESS || core->memSize() != 4096 || core->getPC() != START_ADDRESS)
        throw std::runtime_error("Oracle does not match the 4 KiB VIP profile");
    return capture(profile, *core);
}

Json generate(const Profile& profile, const TestCase& test, const Snapshot& baseline)
{
    auto [host, core, start] = createChip8Instance(profile.preset);
    if (!core) throw std::runtime_error("Preset has no CHIP-8 execution unit");
    core->reset();
    // Deterministic reset and a fresh machine per case avoid hidden-state leakage.
    const auto reset = capture(profile, *core);
    if (reset.registers != baseline.registers || reset.memory != baseline.memory || reset.display != baseline.display)
        throw std::runtime_error("Oracle reset is not reproducible");
    for (uint8_t key = 0; key < 16; ++key)
        if (test.keys & (1 << key)) host->keyDown(key);
    for (auto [address, value] : test.memory) {
        const bool allowed = address >= START_ADDRESS && address < 0xE90;
        if (!allowed) throw std::runtime_error("Case data overlaps interpreter or private workspace");
        core->memory()[address] = value;
    }
    std::copy(test.display.begin(), test.display.end(), core->memory() + profile.displayAddress);
    uint32_t address = start;
    auto writeOpcode = [&](uint16_t opcode) {
        core->memory()[address++] = opcode >> 8;
        core->memory()[address++] = opcode & 0xFF;
    };
    for (auto opcode : test.setup) writeOpcode(opcode);
    auto targetAddress = address;
    writeOpcode(test.opcode);
    writeOpcode(0x6000); // Valid skipped/next instruction; it is never executed.
    writeOpcode(0x6000);
    for (size_t n = 0; n < test.setup.size(); ++n) executeHardwareInstruction(*core);
    if (core->getPC() != targetAddress || core->opcode() != test.opcode)
        throw std::runtime_error("Setup did not reach the instruction under test");
    const auto initial = capture(profile, *core);
    executeHardwareInstruction(*core);
    const auto final = capture(profile, *core);
    if (initial.registers["dt"] != 0 || initial.registers["st"] != 0 || final.registers["dt"] != 0 || final.registers["st"] != 0)
        throw std::runtime_error("Phase-dependent timers are outside this profile");
    Json initialJson = initial.registers;
    initialJson["ram"] = replacementRanges(baseline.memory, initial.memory);
    initialJson["keys"] = test.keys;
    if (initial.display != baseline.display) initialJson["display"] = displayState(profile, initial.display);
    Json finalJson = final.registers;
    auto ramChanges = replacementRanges(initial.memory, final.memory);
    if (!ramChanges.empty()) finalJson["ram"] = std::move(ramChanges);
    if (initial.display != final.display) {
        finalJson["display"] = {{"base", "initial"}, {"replace", Json::array()}};
        for (const auto& range : replacementRanges(initial.display, final.display))
            finalJson["display"]["replace"].push_back(Json::array({0, range[0], range[1]}));
    }
    return {{"id", test.family + "/" + test.name}, {"opcode", fmt::format("{:04x}", test.opcode)},
            {"initial", initialJson}, {"final", finalJson}};
}

Json profileDocument(const Profile& profile, const Snapshot& baseline, uint32_t seed, int randomCases)
{
    Json regions = Json::array();
    for (auto range : profile.memoryRanges) regions.push_back({{"address", range.address}, {"size", range.size}});
    auto defaults = baseline.registers;
    defaults["ram"] = {{"fill", 0}, {"replace", replacementRanges(std::vector<uint8_t>(baseline.memory.size(), 0), baseline.memory)}};
    defaults["display"] = displayState(profile, baseline.display);
    defaults["keys"] = 0;
    return {{"format", "chip8-sst"}, {"version", 1}, {"profile", profile.id},
            {"machine", {{"dialect", "chip8-vip"}, {"memory_size", 4096}, {"memory_regions", regions},
                         {"stack_capacity", profile.stackCapacity}, {"display_alias", {{"address", profile.displayAddress}, {"plane", 0}}},
                         {"execution", "one-completed-instruction"}, {"timers", "zero-only"}}},
            {"defaults", defaults},
            {"generator", {{"name", "c8sst-gen"}, {"version", C8SST_VERSION}, {"revision", C8SST_GIT_REVISION},
                           {"preset", profile.preset}, {"oracle", "original interpreter on emulated COSMAC VIP"},
                           {"boot_interpreter_sha1", emu::calculateSha1Hex(baseline.memory.data(), 512)},
                           {"seed", seed}, {"random_cases_per_alu_operation", randomCases}}},
            {"excluded", Json::array({"native CPU/workspace state", "Cxnn hidden random state", "Fx0a event/timing state", "nonzero timers", "arbitrary 0nnn native calls"})}};
}
} // namespace c8sst
