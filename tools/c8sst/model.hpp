//---------------------------------------------------------------------------------------
// tools/c8sst/model.hpp
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
#pragma once

#include <nlohmann/json.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace c8sst {
using Json = nlohmann::ordered_json;

struct MemoryRange
{
    uint32_t address;
    uint32_t size;
};

// Public behavior profile and private oracle selection are deliberately separate.
struct Profile
{
    std::string id;
    std::string preset;
    std::vector<MemoryRange> memoryRanges;
    uint32_t displayAddress;
    uint32_t displayWidth;
    uint32_t displayHeight;
    uint32_t stackCapacity;
};

struct TestCase
{
    std::string family;
    std::string name;
    uint16_t opcode;
    std::vector<uint16_t> setup;
    std::vector<std::pair<uint32_t, uint8_t>> memory;
    std::array<uint8_t, 256> display{};
    uint16_t keys{};
};

struct Snapshot
{
    Json registers;
    std::vector<uint8_t> memory;
    std::vector<uint8_t> display;
};

Profile profileForPreset(const std::string& preset);
std::vector<TestCase> classicCases(uint32_t seed, int randomCases);
std::string hexBytes(const std::vector<uint8_t>& bytes);
Json replacementRanges(const std::vector<uint8_t>& initial, const std::vector<uint8_t>& final);
Json generate(const Profile& profile, const TestCase& test, const Snapshot& baseline);
Snapshot resetSnapshot(const Profile& profile);
Json profileDocument(const Profile& profile, const Snapshot& baseline, uint32_t seed, int randomCases);
} // namespace c8sst
