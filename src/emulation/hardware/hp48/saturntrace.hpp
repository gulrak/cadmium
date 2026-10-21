//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/saturntrace.hpp
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

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

#include <emulation/hardware/hp48/saturndecoder.hpp>
#include <emulation/hardware/hp48/saturnstate.hpp>

namespace hp48 {

struct SaturnCompareTraceEntry {
    std::uint64_t instructionIndex = 0;
    std::uint64_t cycles = 0;
    std::uint32_t pc = 0;
    std::string_view opcodeNibbles;
    std::string_view instruction;
    SaturnState state {};
    std::uint16_t out = 0;
    std::uint16_t in = 0;
    bool shutdn = false;
};

std::string formatAsapOperand(std::uint32_t value, VariableType type, std::uint8_t arg, std::uint32_t instructionAddress);
std::string formatAsapInstruction(const DecodedInstruction& decoded, std::uint32_t instructionAddress);
void writeSaturnCompareTraceLine(std::ostream& out, const SaturnCompareTraceEntry& entry);

} // namespace hp48
