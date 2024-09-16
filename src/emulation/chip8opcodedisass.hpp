//---------------------------------------------------------------------------------------
// src/emulation/chip8opcodedisass.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
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

#include <emulation/ichip8.hpp>
#include <emulation/chip8options.hpp>
#include <chiplet/chip8meta.hpp>


#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace emu {

class Chip8OpcodeDisassembler
{
public:
    using SymbolResolver = std::function<std::string(uint16_t)>;
    Chip8OpcodeDisassembler();
    virtual std::tuple<uint16_t, uint16_t, std::string> disassembleInstruction(const uint8_t* code, const uint8_t* end) const;
protected:
    SymbolResolver _labelOrAddress;
    detail::OpcodeSet _opcodeSet;
};

}
