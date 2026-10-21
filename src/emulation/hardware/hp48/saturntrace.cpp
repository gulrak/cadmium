//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/saturntrace.cpp
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

#include <emulation/hardware/hp48/saturntrace.hpp>

#include <iomanip>
#include <ostream>
#include <sstream>
#include <string>

namespace hp48 {
namespace {

std::string traceQuoted(std::string_view value)
{
    std::string out;
    out.reserve(value.size() + 2);
    out += '"';
    for (const char ch : value) {
        if (ch == '\\' || ch == '"') {
            out += '\\';
        }
        out += ch;
    }
    out += '"';
    return out;
}

std::uint8_t hstNibble(const SaturnState& state)
{
    return static_cast<std::uint8_t>((state.xm ? 1U : 0U)
        | (state.sb ? 2U : 0U)
        | (state.sr ? 4U : 0U)
        | (state.mp ? 8U : 0U));
}

void writeRegister64(std::ostream& out, std::uint64_t value)
{
    out << std::hex << std::nouppercase << std::setw(16) << std::setfill('0') << value;
}

void writeAddress20(std::ostream& out, std::uint32_t value)
{
    out << std::hex << std::nouppercase << std::setw(5) << std::setfill('0') << (value & 0xfffffU);
}

void writeReturnStack(std::ostream& out, const SaturnState& state)
{
    for (std::size_t i = 0; i < state.rstk.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        writeAddress20(out, state.rstk[i]);
    }
}

} // namespace

std::string formatAsapOperand(std::uint32_t value, VariableType type, std::uint8_t arg, std::uint32_t instructionAddress)
{
    std::ostringstream out;

    switch (type) {
    case vt_const:
    case vt_nzconst:
        if (value < 10) {
            out << std::dec << value;
        } else {
            out << '#' << std::hex << std::nouppercase << value;
        }
        break;

    case vt_pcofs:
        out << '#' << std::hex << std::nouppercase << std::setw(5) << std::setfill('0')
            << ((instructionAddress + value) & 0xfffffU);
        break;

    case vt_varconst:
        out << '#' << std::hex << std::nouppercase << value;
        break;

    case vt_hwflags:
        out << "hwflag" << std::dec << value;
        break;

    case vt_field:
    case vt_regpair4:
    case vt_regpair4rev:
    case vt_regpair8:
    case vt_regpair8split:
    case vt_regpair12:
    case vt_reg:
    case vt_mrpair:
    case vt_daregpair:
    case vt_daregpairrev:
    case vt_tempreg:
        if (type < vt_maxtables) {
            const auto& table = varTables[type];
            if (value < table.size() && !table[value].empty()) {
                return std::string(table[value]);
            }
        }
        out << "??" << std::dec << value;
        break;

    case vt_none:
        break;
    }

    (void)arg;
    return out.str();
}

std::string formatAsapInstruction(const DecodedInstruction& decoded, std::uint32_t instructionAddress)
{
    if (decoded.info == nullptr) {
        return "???";
    }

    std::string result = decoded.info->fmt;
    for (int i = 0; i < decoded.info->numVars; ++i) {
        const std::string placeholder = "{" + std::to_string(i) + "}";
        std::size_t pos = result.find(placeholder);
        while (pos != std::string::npos) {
            const std::string operand = formatAsapOperand(
                decoded.operands[i],
                decoded.info->varTypes[i],
                decoded.info->varArgs[i],
                instructionAddress & 0xfffffU);
            result.replace(pos, placeholder.length(), operand);
            pos = result.find(placeholder, pos + operand.length());
        }
    }
    return result;
}

void writeSaturnCompareTraceLine(std::ostream& out, const SaturnCompareTraceEntry& entry)
{
    const SaturnState& state = entry.state;

    out << "i=" << std::dec << entry.instructionIndex
        << " cycles=" << entry.cycles
        << " pc=";
    writeAddress20(out, entry.pc);
    out << " op=" << entry.opcodeNibbles
        << " asm=" << traceQuoted(entry.instruction)
        << " a=";
    writeRegister64(out, state.a);
    out << " b=";
    writeRegister64(out, state.b);
    out << " c=";
    writeRegister64(out, state.c);
    out << " d=";
    writeRegister64(out, state.d);
    out << " r0=";
    writeRegister64(out, state.r0);
    out << " r1=";
    writeRegister64(out, state.r1);
    out << " r2=";
    writeRegister64(out, state.r2);
    out << " r3=";
    writeRegister64(out, state.r3);
    out << " r4=";
    writeRegister64(out, state.r4);
    out << " d0=";
    writeAddress20(out, state.d0);
    out << " d1=";
    writeAddress20(out, state.d1);
    out << " p=" << std::hex << std::nouppercase << static_cast<unsigned>(state.p & 0x0f)
        << " st=" << std::setw(4) << std::setfill('0') << (state.pstat & 0xffffU)
        << " hst=" << static_cast<unsigned>(hstNibble(state))
        << " carry=" << std::dec << (state.carry ? 1 : 0)
        << " mode=" << (state.hexMode ? "hex" : "dec")
        << " rstk=";
    writeReturnStack(out, state);
    out << " out=" << std::hex << std::nouppercase << std::setw(3) << std::setfill('0') << (entry.out & 0x0fffU)
        << " in=" << std::setw(4) << (entry.in & 0xffffU)
        << " shutdn=" << std::dec << (entry.shutdn ? 1 : 0)
        << '\n';
}

} // namespace hp48
