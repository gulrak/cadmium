//---------------------------------------------------------------------------------------
// test/schip_strict_tests.cpp
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
// Checks the STRICT-SCHIP-1.1 core against the timing in HP48-SCHIP-1.1-Timing.md.
#include <doctest/doctest.h>
#include <emulation/schipstrict.hpp>
#include "chip8headless.hpp"

#include <limits>
#include <vector>

namespace {

constexpr int64_t NO_TICK = std::numeric_limits<int64_t>::max() / 2;

struct SchipStrictFixture
{
    explicit SchipStrictFixture(std::vector<uint8_t> program, bool ticks = false)
        : properties(emu::CoreRegistry::propertiesForPreset("strict-schip-1-1"))
        , host(properties)
        , core(host, properties)
    {
        core.IEmulationCore::reset();
        REQUIRE(core.loadData(program, {}));
        if (!ticks)
            core.setTickSchedule(NO_TICK);
    }

    int step()
    {
        const auto cycles = core.executeInstruction();
        INFO(core.errorMessage());
        REQUIRE_FALSE(core.inErrorState());
        return cycles;
    }

    // Runs until the instruction at pc has been completed, returns its cycles.
    int64_t runInstruction()
    {
        const auto start = core.machineCycles();
        do {
            step();
        } while (core.cpuState() == emu::GenericCpu::eWAIT);
        return core.machineCycles() - start;
    }

    int pixel(int x, int y) const { return core.getScreen()->getPixelIndex(x, y) ? 1 : 0; }

    emu::Properties properties;
    HeadlessTestHost host;
    emu::SchipStrictEmulator core;
};

// Renders whole frames of the core and returns the buzzer output.
std::vector<int16_t> renderFrames(SchipStrictFixture& fixture, int frames)
{
    std::vector<int16_t> output;
    double samplesDue = 0;
    for (int frame = 0; frame < frames; ++frame) {
        fixture.core.executeFrame();
        samplesDue += 44100.0 / 64.0;
        const auto samples = static_cast<size_t>(samplesDue);
        samplesDue -= static_cast<double>(samples);
        const auto offset = output.size();
        output.resize(offset + samples);
        fixture.core.renderAudio(output.data() + offset, samples, 44100);
    }
    return output;
}

struct BuzzerPhase
{
    bool high{};
    int length{};
};

// The phases between the sign changes of the output, the partial ones at the ends left out.
std::vector<BuzzerPhase> buzzerPhases(const std::vector<int16_t>& samples)
{
    std::vector<BuzzerPhase> phases;
    size_t start = 0;
    for (size_t index = 1; index < samples.size(); ++index) {
        if ((samples[index] > 0) != (samples[index - 1] > 0)) {
            if (start)
                phases.push_back({samples[index - 1] > 0, static_cast<int>(index - start)});
            start = index;
        }
    }
    return phases;
}

// The filters and the whole-sample edges move a phase end by up to two samples.
void checkPhase(const BuzzerPhase& phase, int64_t cycles)
{
    const auto expected = static_cast<double>(cycles) * 44100.0 / 1630208.0;
    CHECK(static_cast<double>(phase.length) >= expected - 2.0);
    CHECK(static_cast<double>(phase.length) <= expected + 2.0);
}

}  // namespace

TEST_CASE("STRICT-SCHIP-1.1 preset is registered")
{
    auto properties = emu::CoreRegistry::propertiesForPreset("strict-schip-1-1");
    CHECK(properties.propertyClass() == "SCHIP-STRICT");
}

TEST_CASE("STRICT-SCHIP-1.1 opcode totals without ticks and sound")
{
    struct Case
    {
        std::vector<uint8_t> program;
        int skip;  // instructions to run before the measured one
        int64_t cycles;
    };
    const std::vector<Case> cases{
        {{0x12, 0x00}, 0, 518},                          // 1nnn
        {{0xA1, 0x23}, 0, 518},                          // Annn
        {{0x60, 0x12}, 0, 543},                          // 6xnn
        {{0x70, 0x12}, 0, 563},                          // 7xnn
        {{0x30, 0x00}, 0, 559 + 41},                     // 3xnn skipping
        {{0x40, 0x00}, 0, 559},                          // 4xnn not skipping
        {{0x81, 0x24}, 0, 892},                          // 8xy4
        {{0x00, 0xE0}, 0, 11765},                        // 00E0
        {{0x00, 0xC5}, 0, 17277 - 125 * 5},              // 00C5
        {{0x60, 0xFF, 0xA3, 0x00, 0xF0, 0x33}, 2, 1673}, // Fx33 of 255
        {{0x60, 0x00, 0xA3, 0x00, 0xF0, 0x33}, 2, 1025}, // Fx33 of 0
        {{0xA3, 0x00, 0xF5, 0x55}, 1, 903 + 5 * 86},     // F555
        {{0xF9, 0x75}, 0, 904 + 7 * 77},                 // F975 is limited to 8 registers
        {{0x00, 0xFF, 0xD0, 0x15}, 1, 411 + 1306 + 5 * (365 + 60 * 8)},  // hires D015 at 0,0
        {{0x00, 0xFF, 0xD0, 0x10}, 1, 411 + 1306 + 4 + 16 * (414 + 60 * 16)},  // hires D010 at 0,0
    };
    for (const auto& [program, skip, cycles] : cases) {
        CAPTURE(program[skip * 2]);
        CAPTURE(program[skip * 2 + 1]);
        SchipStrictFixture fixture(program);
        for (int index = 0; index < skip; ++index)
            fixture.runInstruction();
        CHECK(fixture.runInstruction() == cycles);
    }
}

TEST_CASE("STRICT-SCHIP-1.1 real-time chores")
{
    // The programs end in a loop of two jumps, as a jump to itself pauses the core.
    SUBCASE("sound toggling costs 103 cycles per instruction")
    {
        SchipStrictFixture fixture({0x60, 0x40, 0xF0, 0x18, 0x12, 0x06, 0x12, 0x04});
        fixture.runInstruction();
        fixture.runInstruction();
        CHECK(fixture.core.soundTimer() == 0x40);
        CHECK(fixture.runInstruction() == 518 + 103);
    }
    SUBCASE("a tick costs 385 cycles plus 12 for a running delay timer")
    {
        SchipStrictFixture fixture({0x60, 0x40, 0xF0, 0x15, 0x12, 0x06, 0x12, 0x04});
        fixture.runInstruction();
        fixture.runInstruction();
        fixture.core.setTickSchedule(fixture.core.machineCycles());
        CHECK(fixture.runInstruction() == 518 + 385 + 12);
        CHECK(fixture.core.delayTimer() == 0x3F);
        CHECK(fixture.runInstruction() == 518);
    }
    SUBCASE("at most one tick per poll, missed ticks are caught up")
    {
        SchipStrictFixture fixture({0x60, 0xFF, 0xF0, 0x15, 0x12, 0x06, 0x12, 0x04});
        fixture.runInstruction();
        fixture.runInstruction();
        fixture.core.setTickSchedule(fixture.core.machineCycles() - 2 * emu::SchipStrictEmulator::TICK_CYCLES);
        for (int index = 0; index < 3; ++index)
            CHECK(fixture.runInstruction() == 518 + 385 + 12);
        CHECK(fixture.core.delayTimer() == 0xFC);
        CHECK(fixture.runInstruction() == 518);
    }
    SUBCASE("startup handles the pending ticks first")
    {
        SchipStrictFixture fixture({0x60, 0xFF, 0xF0, 0x15, 0x12, 0x06, 0x12, 0x04}, true);
        fixture.runInstruction();
        fixture.runInstruction();
        const int alreadyHandled = 2;
        for (int index = alreadyHandled; index < emu::SchipStrictEmulator::STARTUP_PENDING_TICKS; ++index)
            fixture.runInstruction();
        CHECK(fixture.core.delayTimer() == 0xFF - (emu::SchipStrictEmulator::STARTUP_PENDING_TICKS - alreadyHandled));
    }
}

TEST_CASE("STRICT-SCHIP-1.1 lores Dxyn waits for a tick")
{
    // A005 points at the "1" of the small font (0x20, 0x60, 0x20, 0x20, 0x70)
    SchipStrictFixture fixture({0x60, 0x00, 0xA0, 0x05, 0xD0, 0x05, 0xD0, 0x05, 0x12, 0x08});
    fixture.runInstruction();
    fixture.runInstruction();
    const auto start = fixture.core.machineCycles();
    fixture.core.setTickSchedule(start + 10000);
    const auto cycles = fixture.runInstruction();
    // realtime + dispatch + 55, polls until one at or after the tick, then the draw
    CHECK(fixture.core.machineCycles() > start + 10000);
    const int lines = 5;
    const int bits = 1 + 2 + 1 + 1 + 3;
    const int64_t draw = 1232 + lines * (405 + 80 * 8) + 3 * bits;
    // polls of 169 + 7 cycles read the timer 79 cycles in until one ticks (554 + 7)
    int64_t polls = 0;
    while (169 + 242 + 55 + polls * 176 + 79 < 10000)
        ++polls;
    CHECK(cycles == 169 + 242 + 55 + polls * 176 + 561 + draw);
    CHECK(fixture.core.getV(0xF) == 0);
    // the "1" has its top pixel at lores x 2, so LCD pixels 4 and 5 in rows 0 and 1
    CHECK(fixture.pixel(4, 0) == 1);
    CHECK(fixture.pixel(5, 1) == 1);
    CHECK(fixture.pixel(3, 0) == 0);
    // drawing again erases it and collides
    fixture.core.setTickSchedule(fixture.core.machineCycles() + 1);
    fixture.runInstruction();
    CHECK(fixture.core.getV(0xF) == 1);
    CHECK(fixture.pixel(4, 0) == 0);
}

TEST_CASE("STRICT-SCHIP-1.1 hires collision counts lines and bottom clipping")
{
    // Draw the "0" (5 lines) at y 62 twice: 2 visible lines, 3 clipped.
    SchipStrictFixture fixture({0x00, 0xFF, 0x60, 0x00, 0x61, 0x3E, 0xA0, 0x00, 0xD0, 0x15, 0xD0, 0x15, 0x12, 0x0C});
    for (int index = 0; index < 4; ++index)
        fixture.runInstruction();
    CHECK(fixture.runInstruction() == 411 + 1306 + 2 + 2 * (365 + 60 * 8));
    CHECK(fixture.core.getV(0xF) == 3);
    CHECK(fixture.pixel(0, 62) == 1);
    CHECK(fixture.runInstruction() == 411 + 1306 + 2 + 2 * (365 + 60 * 8) - 2 * 2);
    CHECK(fixture.core.getV(0xF) == 2 + 3);
    CHECK(fixture.pixel(0, 62) == 0);
}

TEST_CASE("STRICT-SCHIP-1.1 Fx0A scans one key per poll")
{
    SchipStrictFixture fixture({0xF3, 0x0A, 0x12, 0x02});
    fixture.host.keyDown(5);
    // key 0 is tested first, then one key per round: 5 rounds until key 5 is found
    for (int round = 0; round < 6 + 3; ++round)
        fixture.step();
    CHECK(fixture.core.cpuState() == emu::GenericCpu::eWAIT);
    CHECK(fixture.core.soundTimer() == 4);
    fixture.host.keyUp(5);
    fixture.step();
    CHECK(fixture.core.cpuState() != emu::GenericCpu::eWAIT);
    CHECK(fixture.core.getV(3) == 5);
    CHECK(fixture.core.getPC() == 0x202);
}

TEST_CASE("STRICT-SCHIP-1.1 errors like the interpreter")
{
    SUBCASE("00C0")
    {
        SchipStrictFixture fixture({0x00, 0xC0});
        fixture.core.executeInstruction();
        CHECK(fixture.core.inErrorState());
    }
    SUBCASE("Fx1E overflow")
    {
        SchipStrictFixture fixture({0x60, 0x02, 0xAF, 0xFE, 0xF0, 0x1E});
        fixture.runInstruction();
        fixture.runInstruction();
        fixture.core.executeInstruction();
        CHECK(fixture.core.inErrorState());
        CHECK(fixture.core.getPC() == 0x204);
    }
}

TEST_CASE("STRICT-SCHIP-1.1 renders on the emulated HP-48SX display by default")
{
    SchipStrictFixture fixture({0x00, 0xFF, 0xA0, 0x00, 0xD0, 0x15});  // hires, I = font 0, draw at 0,0
    CHECK(fixture.properties.at("hp48Display").getBool());
    std::vector<uint32_t> buffer(600 * 300);
    const auto render = [&] {
        const auto span = fixture.core.getRGBASpan({buffer.data(), 600, 300});
        return std::vector<uint32_t>(span.data(), span.data() + span.width() * span.height());
    };
    fixture.runInstruction();
    fixture.runInstruction();
    const auto before = render();
    CHECK(before.size() == size_t{emu::SchipStrictEmulator::HP48_DISPLAY_WIDTH} * emu::SchipStrictEmulator::HP48_DISPLAY_HEIGHT);
    fixture.runInstruction();
    REQUIRE(fixture.pixel(0, 0) == 1);
    CHECK(render() != before);

    // switching it off is applied live, the core keeps running with the plain screen
    auto& display = fixture.properties.at("hp48Display");
    display.setBool(false);
    CHECK_FALSE(fixture.core.updateProperties(fixture.properties, display));
    CHECK(fixture.core.getRGBASpan({buffer.data(), 600, 300}).empty());
    CHECK(fixture.pixel(0, 0) == 1);
}

TEST_CASE("STRICT-SCHIP-1.1 buzzer")
{
    SUBCASE("the speaker toggles once per instruction")
    {
        // 621 cycles per jump, so the high and the low phase are 621 cycles each
        SchipStrictFixture fixture({0x60, 0x40, 0xF0, 0x18, 0x12, 0x06, 0x12, 0x04});
        const auto phases = buzzerPhases(renderFrames(fixture, 8));
        REQUIRE(phases.size() > 100);
        for (const auto& phase : phases)
            checkPhase(phase, 621);
    }
    SUBCASE("a key test ends the high phase early")
    {
        // The EX9E toggle goes high, its key test pulls it low 272 - 134 + 242 + 188 = 568
        // cycles later, the EX9E takes 911 cycles and the jump 621.
        SchipStrictFixture fixture({0x60, 0x40, 0xF0, 0x18, 0x61, 0x05, 0xE1, 0x9E, 0x12, 0x06});
        const auto phases = buzzerPhases(renderFrames(fixture, 8));
        REQUIRE(phases.size() > 50);
        for (const auto& phase : phases)
            checkPhase(phase, phase.high ? 568 : 911 + 621 - 568);
    }
    SUBCASE("a 64 Hz tick ends the high phase early")
    {
        // Instruction 5 goes high and takes the tick, its key scan pulls the buzzer low
        // 669 - 160 - 134 = 375 cycles later, the next high phase starts in instruction 7.
        SchipStrictFixture fixture({0x60, 0x40, 0xF0, 0x18, 0x12, 0x06, 0x12, 0x04});
        fixture.core.executeInstructions(5);
        fixture.core.setTickSchedule(fixture.core.machineCycles());
        const auto phases = buzzerPhases(renderFrames(fixture, 1));
        REQUIRE(phases.size() > 4);
        REQUIRE(phases[0].high);
        checkPhase(phases[0], 621);
        checkPhase(phases[1], 621);
        REQUIRE(phases[2].high);
        checkPhase(phases[2], 375);
        checkPhase(phases[3], 1018 - 509 + 621 + 134);
        checkPhase(phases[4], 621);
    }
    SUBCASE("no sound without the sound timer")
    {
        SchipStrictFixture fixture({0x12, 0x02, 0x12, 0x00});
        for (const auto sample : renderFrames(fixture, 4))
            CHECK(sample == 0);
    }
}
