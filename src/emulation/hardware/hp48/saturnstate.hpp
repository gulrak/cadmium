//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/saturnstate.hpp
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

#include <array>
#include <cstdint>
#include <iosfwd>
#include <nlohmann/json_fwd.hpp>
#include <random>

namespace hp48 {

struct SaturnState {
    // 64-bit registers
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
    uint64_t r0;
    uint64_t r1;
    uint64_t r2;
    uint64_t r3;
    uint64_t r4;

    // 20-bit data pointers
    uint32_t d0;
    uint32_t d1;

    // Program counter (20-bit)
    uint32_t pc;

    // Pointer register (4-bit)
    uint8_t p;

    // Flags and status
    bool carry;
    bool hexMode;

    // Program Status (ST register, 12-16 bits used)
    // Legacy x48 uses PSTAT array of 16 bytes.
    uint16_t pstat;

    // Hardware status bits (XM, SB, SR, MP)
    uint8_t xm : 1;
    uint8_t sb : 1;
    uint8_t sr : 1;
    uint8_t mp : 1;

    // Return stack
    std::array<uint32_t, 8> rstk;

    uint16_t crc;

    static SaturnState random(std::mt19937_64& gen);

    bool compare(const SaturnState& other, const SaturnState& initial, std::ostream& errorStream) const;
};

void to_json(nlohmann::json& j, const SaturnState& s);
void to_json(nlohmann::ordered_json& j, const SaturnState& s);
void from_json(const nlohmann::json& j, SaturnState& s);
void from_json(const nlohmann::ordered_json& j, SaturnState& s);

} // namespace hp48
