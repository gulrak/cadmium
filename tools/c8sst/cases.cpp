//---------------------------------------------------------------------------------------
// tools/c8sst/cases.cpp
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
#include <fmt/format.h>
#include <random>

namespace c8sst {
namespace {
constexpr uint16_t START_ADDRESS = 0x200;
constexpr uint16_t DATA_ADDRESS = 0x800;

TestCase registerCase(const std::string& family, const std::string& name, uint16_t opcode,
                      const std::array<uint8_t, 16>& values, uint16_t index = DATA_ADDRESS)
{
    TestCase test{family, name, opcode};
    for (uint16_t r = 0; r < 16; ++r)
        test.setup.push_back(0x6000 | (r << 8) | values[r]);
    test.setup.push_back(0xA000 | index);
    // A patterned sentinel catches accidental clears in non-display instructions.
    for (size_t i = 0; i < test.display.size(); ++i)
        test.display[i] = static_cast<uint8_t>((i * 37) ^ (i >> 3) ^ 0xA5);
    return test;
}

std::array<uint8_t, 16> sentinels()
{
    return {0x13, 0x27, 0x39, 0x4B, 0x5D, 0x6F, 0x71, 0x83,
            0x95, 0xA7, 0xB9, 0xCB, 0xDD, 0xEF, 0x01, 0xA5};
}
} // namespace

std::vector<TestCase> classicCases(uint32_t seed, int randomCases)
{
    std::vector<TestCase> tests;
    constexpr std::array<uint8_t, 8> VALUES{0, 1, 0x7F, 0x80, 0xFE, 0xFF, 0x55, 0xAA};
    constexpr std::array<uint8_t, 9> ALU_OPS{0, 1, 2, 3, 4, 5, 6, 7, 0xE};
    constexpr std::array<std::pair<uint8_t, uint8_t>, 6> REG_PAIRS{{{0, 1}, {1, 0}, {3, 3}, {0xF, 2}, {2, 0xF}, {0xF, 0xF}}};
    std::mt19937 random(seed);
    for (auto op : ALU_OPS) {
        for (auto [x, y] : REG_PAIRS) {
            for (auto a : VALUES) {
                for (auto b : VALUES) {
                    if (x == y && a != b) continue;
                    auto values = sentinels();
                    values[x] = a;
                    values[y] = b; // Same-register cases necessarily have one shared value.
                    auto opcode = static_cast<uint16_t>(0x8000 | (x << 8) | (y << 4) | op);
                    tests.push_back(registerCase("arithmetic", fmt::format("{:04x}-{:02x}-{:02x}", opcode, a, b), opcode, values));
                }
            }
        }
        for (int n = 0; n < randomCases; ++n) {
            auto values = sentinels();
            for (auto& value : values) value = static_cast<uint8_t>(random());
            const auto x = random() % 16;
            const auto y = random() % 16;
            auto opcode = static_cast<uint16_t>(0x8000 | (x << 8) | (y << 4) | op);
            tests.push_back(registerCase("arithmetic", fmt::format("random-{:x}-{}", op, n), opcode, values));
        }
    }
    for (uint16_t x = 0; x < 16; ++x) {
        for (auto value : VALUES) {
            for (uint16_t prefix : {0x6000, 0x7000}) {
                auto values = sentinels();
                values[x] = value;
                for (auto immediate : VALUES) {
                    auto opcode = static_cast<uint16_t>(prefix | (x << 8) | immediate);
                    tests.push_back(registerCase("immediate", fmt::format("{:04x}-{:02x}", opcode, value), opcode, values));
                }
            }
        }
    }
    for (uint16_t prefix : {0x3000, 0x4000, 0x5000, 0x9000}) {
        for (auto [x, y] : REG_PAIRS) {
            for (bool equal : {false, true}) {
                auto values = sentinels();
                values[x] = 0x80;
                values[y] = equal ? 0x80 : 0x7F;
                auto opcode = static_cast<uint16_t>(prefix | (x << 8) | (prefix < 0x5000 ? 0x80 : (y << 4)));
                tests.push_back(registerCase("skip", fmt::format("{:04x}-{}-{}", opcode, y, equal), opcode, values));
            }
        }
    }
    for (uint16_t address : {0x200, 0x222, 0x400, 0xDFE}) {
        for (uint16_t prefix : {0x1000, 0x2000, 0xA000, 0xB000}) {
            auto values = sentinels();
            values[0] = 0x1F;
            tests.push_back(registerCase("control", fmt::format("{:04x}", prefix | address), prefix | address, values));
        }
    }
    for (uint16_t depth : {1, 2, 11, 12}) {
        TestCase test{"control", fmt::format("return-depth-{}", depth), 0x00EE};
        for (uint16_t n = 0; n < depth; ++n)
            test.setup.push_back(0x2000 | (START_ADDRESS + 2 * (n + 1)));
        tests.push_back(std::move(test));
    }
    for (uint16_t x : {0, 1, 7, 15}) {
        for (uint16_t index : {0x800, 0x80F, 0xDF0}) {
            for (uint16_t suffix : {0x55, 0x65}) {
                auto opcode = static_cast<uint16_t>(0xF000 | (x << 8) | suffix);
                auto test = registerCase("memory", fmt::format("{:04x}-{:03x}", opcode, index), opcode, sentinels(), index);
                for (uint32_t n = 0; n < 18; ++n)
                    test.memory.emplace_back(index + n, static_cast<uint8_t>(n * 17 + 3));
                tests.push_back(std::move(test));
            }
        }
        for (auto value : VALUES) {
            auto values = sentinels();
            values[x] = value;
            for (uint16_t suffix : {0x33, 0x1E}) {
                for (uint16_t index : {0x800, 0xFFE}) {
                    if (suffix == 0x33 && index == 0xFFE) continue;
                    auto opcode = static_cast<uint16_t>(0xF000 | (x << 8) | suffix);
                    tests.push_back(registerCase("memory", fmt::format("{:04x}-{:02x}-{:03x}", opcode, value, index), opcode, values, index));
                }
            }
        }
        for (uint8_t digit = 0; digit < 16; ++digit) {
            auto values = sentinels();
            values[x] = digit;
            auto opcode = static_cast<uint16_t>(0xF029 | (x << 8));
            tests.push_back(registerCase("font", fmt::format("{:04x}-{:x}", opcode, digit), opcode, values));
        }
        for (uint8_t key : {0, 1, 15}) {
            for (bool pressed : {false, true}) {
                for (uint16_t suffix : {0x9E, 0xA1}) {
                    auto values = sentinels();
                    values[x] = key;
                    auto opcode = static_cast<uint16_t>(0xE000 | (x << 8) | suffix);
                    auto test = registerCase("keys", fmt::format("{:04x}-{}-{}", opcode, key, pressed), opcode, values);
                    test.keys = pressed ? (1 << key) : 0;
                    tests.push_back(std::move(test));
                }
            }
        }
        for (uint16_t suffix : {0x07, 0x15, 0x18}) {
            auto values = sentinels();
            values[x] = 0; // No phase-dependent timer decrements in this first profile.
            auto opcode = static_cast<uint16_t>(0xF000 | (x << 8) | suffix);
            tests.push_back(registerCase("timers", fmt::format("{:04x}-zero", opcode), opcode, values));
        }
    }
    for (uint8_t pattern : {0, 1, 2, 3}) {
        auto test = registerCase("display", fmt::format("clear-{}", pattern), 0x00E0, sentinels());
        if (pattern < 3) test.display.fill(pattern == 0 ? 0 : pattern == 1 ? 0xFF : 0x55);
        tests.push_back(std::move(test));
    }
    constexpr std::array<std::pair<uint8_t, uint8_t>, 8> POSITIONS{{{0, 0}, {7, 1}, {56, 17}, {63, 31}, {60, 28}, {64, 32}, {127, 63}, {255, 255}}};
    for (uint8_t height : {0, 1, 2, 8, 15}) {
        for (auto [px, py] : POSITIONS) {
            for (uint8_t pattern : {0, 1, 2, 3}) {
                auto values = sentinels();
                values[0] = px;
                values[1] = py;
                auto opcode = static_cast<uint16_t>(0xD010 | height);
                auto test = registerCase("display", fmt::format("draw-{}-{}-{}-{}", height, px, py, pattern), opcode, values);
                test.display.fill(pattern == 1 ? 0xFF : 0);
                if (pattern == 3) test.display[(py % 32) * 8 + (px % 64) / 8] = 0x80 >> (px % 8);
                for (uint32_t n = 0; n < 16; ++n)
                    test.memory.emplace_back(DATA_ADDRESS + n, pattern == 0 ? 0 : pattern == 3 ? 0x80 : static_cast<uint8_t>((n & 1) ? 0x81 : 0xFF));
                tests.push_back(std::move(test));
            }
        }
    }
    for (auto [x, y] : REG_PAIRS) {
        auto values = sentinels();
        values[x] = 63;
        values[y] = 31;
        auto opcode = static_cast<uint16_t>(0xD003 | (x << 8) | (y << 4));
        auto test = registerCase("display", fmt::format("draw-alias-{:04x}", opcode), opcode, values);
        test.memory = {{DATA_ADDRESS, 0xFF}, {DATA_ADDRESS + 1, 0x81}, {DATA_ADDRESS + 2, 0x80}};
        tests.push_back(std::move(test));
    }
    // Stores into the VIP framebuffer must appear as both RAM and display changes.
    auto alias = registerCase("display", "store-framebuffer", 0xFF55, sentinels(), 0xF00);
    tests.push_back(std::move(alias));
    auto load = registerCase("display", "load-framebuffer", 0xFF65, sentinels(), 0xF00);
    tests.push_back(std::move(load));
    return tests;
}
} // namespace c8sst
