//---------------------------------------------------------------------------------------
// tools/c8sst/runner.cpp
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
#include "execution.hpp"
#include "../../test/chip8headless.hpp"
#include <emulation/chip8generic.hpp>
#include <emulation/chip8strict.hpp>
#include <emulation/cosmacvip.hpp>
#include <fmt/format.h>
#include <stdexcept>

namespace c8sst {
namespace {
emu::Chip8State architecturalState(const Snapshot& snapshot)
{
    const auto& data = snapshot.registers;
    emu::Chip8State result;
    result.pc = data.at("pc").get<int>();
    result.i = data.at("i").get<int>();
    result.dt = data.at("dt").get<int>();
    result.st = data.at("st").get<int>();
    result.sp = data.at("stack").size();
    for (size_t n = 0; n < result.v.size(); ++n) result.v[n] = data.at("v")[n].get<uint8_t>();
    for (int n = 0; n < result.sp; ++n) result.s[n] = data.at("stack")[n].get<uint16_t>();
    return result;
}

Snapshot capture(emu::IChip8Emulator& core, bool ramDisplay)
{
    Snapshot result;
    result.registers = {{"pc", core.getPC()}, {"i", core.getI()}, {"dt", core.delayTimer()}, {"st", core.soundTimer()},
                        {"v", Json::array()}, {"stack", Json::array()}};
    for (uint8_t r = 0; r < 16; ++r) result.registers["v"].push_back(core.getV(r));
    if (core.getSP() > 16 || core.getSP() > core.stackSize())
        throw std::runtime_error("Target reported an invalid stack depth");
    for (uint32_t n = 0; n < core.getSP(); ++n) result.registers["stack"].push_back(core.stackElement(n));
    result.memory.assign(core.memory(), core.memory() + 4096);
    if (ramDisplay) {
        result.display.assign(core.memory() + 0xF00, core.memory() + 0x1000);
    }
    else {
        auto* system = dynamic_cast<emu::IEmulationCore*>(&core);
        const auto* screen = system ? system->getScreen() : nullptr;
        if (!screen || screen->width() != 64 || screen->height() != 32)
            throw std::runtime_error("Target has no canonical monochrome 64x32 display");
        result.display.assign(256, 0);
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 64; ++x)
                if (screen->getPixelIndex(x, y) & 1)
                    result.display[y * 8 + x / 8] |= 0x80 >> (x % 8);
    }
    return result;
}

Json differences(const Snapshot& expected, const Snapshot& actual, const Corpus& corpus)
{
    Json result = Json::array();
    auto mismatch = [&](const std::string& field, const Json& wanted, const Json& got) {
        if (wanted != got) result.push_back({{"field", field}, {"expected", wanted}, {"actual", got}});
    };
    for (const char* name : {"pc", "i", "dt", "st"})
        mismatch(name, expected.registers.at(name), actual.registers.at(name));
    for (size_t r = 0; r < 16; ++r)
        mismatch(fmt::format("v{:x}", r), expected.registers.at("v")[r], actual.registers.at("v")[r]);
    mismatch("stack", expected.registers.at("stack"), actual.registers.at("stack"));
    bool found = false;
    for (auto range : corpus.memoryRanges) {
        for (uint32_t offset = range.address; offset < range.address + range.size; ++offset) {
            if (expected.memory[offset] != actual.memory[offset]) {
                result.push_back({{"field", "ram"}, {"address", offset},
                                  {"expected", expected.memory[offset]}, {"actual", actual.memory[offset]}});
                found = true;
                break;
            }
        }
        if (found) break; // First RAM difference is enough to locate a failure.
    }
    for (size_t offset = 0; offset < expected.display.size(); ++offset) {
        const auto mask = expected.display[offset] ^ actual.display[offset];
        if (mask) {
            int bit = 0;
            while (!(mask & (0x80 >> bit))) ++bit;
            result.push_back({{"field", "display"}, {"plane", 0}, {"x", (offset % 8) * 8 + bit}, {"y", offset / 8},
                              {"expected", (expected.display[offset] >> (7 - bit)) & 1},
                              {"actual", (actual.display[offset] >> (7 - bit)) & 1}});
            break;
        }
    }
    return result;
}

void executeInstruction(emu::IChip8Emulator& core, uint16_t opcode)
{
    if (dynamic_cast<emu::CosmacVIP*>(&core)) {
        executeHardwareInstruction(core);
        return;
    }
    const auto initialPc = core.getPC();
    core.setExecMode(emu::GenericCpu::eRUNNING);
    for (int attempt = 0; attempt < 4096; ++attempt) {
        core.executeInstruction();
        if (core.cpuState() == emu::GenericCpu::eERROR || core.cpuState() == emu::GenericCpu::eHALT)
            throw std::runtime_error("Target entered an error/halt state");
        // Generic draw waits can rewind PC without setting eWAIT. A self-jump,
        // however, is a completed instruction even though PC has not moved.
        const bool waitingDraw = (opcode & 0xF000) == 0xD000 && core.getPC() == initialPc;
        if (core.cpuState() == emu::GenericCpu::eNORMAL && !waitingDraw) return;
        if (core.execMode() == emu::GenericCpu::ePAUSED) break;
    }
    throw std::runtime_error("Target did not complete one instruction within the step bound");
}
} // namespace

bool isRunnerPreset(const std::string& preset)
{
    return preset == "vip-chip-8" || preset == "strict-chip-8" || preset == "chip-8";
}

Json runCase(const LoadedCase& test, const Corpus& corpus, const std::string& preset)
{
    auto [host, core, start] = createChip8Instance(preset);
    if (!core || core->memSize() < 4096) throw std::runtime_error("Preset cannot import the VIP RAM image");
    core->reset();
    const bool ramDisplay = dynamic_cast<emu::CosmacVIP*>(core) || dynamic_cast<emu::Chip8StrictEmulator*>(core);
    for (auto range : corpus.memoryRanges)
        std::copy_n(test.initial.memory.begin() + range.address, range.size, core->memory() + range.address);
    if (!core->restoreState(architecturalState(test.initial))) throw std::runtime_error("Preset cannot restore the case's architectural state");
    if (!ramDisplay) {
        auto* generic = dynamic_cast<emu::Chip8GenericEmulator*>(core);
        if (!generic || !generic->restoreDisplay(test.initial.display, 64, 32))
            throw std::runtime_error("Preset cannot import the canonical display");
    }
    for (uint8_t key = 0; key < 16; ++key)
        if (test.keys & (1 << key)) host->keyDown(key);
    const auto initialDifferences = differences(test.initial, capture(*core, ramDisplay), corpus);
    if (!initialDifferences.empty()) throw std::runtime_error("State import changed the fixture: " + initialDifferences.dump());
    Json result{{"id", test.id}, {"family", test.family}, {"opcode", fmt::format("{:04x}", test.opcode)}};
    try {
        executeInstruction(*core, test.opcode);
        result["differences"] = differences(test.final, capture(*core, ramDisplay), corpus);
    }
    catch (const std::exception& error) {
        result["differences"] = Json::array({{{"field", "execution"}, {"error", error.what()}}});
    }
    result["passed"] = result["differences"].empty();
    return result;
}
} // namespace c8sst
