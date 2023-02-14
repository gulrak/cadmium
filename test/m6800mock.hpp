//---------------------------------------------------------------------------------------
// tests/m6800mock.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2023, Steffen Schümann <s.schuemann@pobox.com>
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
#include <emulation/hardware/m6800.hpp>

namespace emu {

class M6800Mock
{
public:
    using Bus = M6800Bus<uint8_t,uint16_t>;
    M6800Mock(Bus& bus) {}
    void reset() {}
    void getState(M6800State& state) const {}
    void setState(const M6800State& state) {}
    void executeInstruction() {}
    void executeInstructionTraced() {}
    std::string dumpStateLine() const { return ""; }
    std::string disassembleCurrentInstruction() const { return ""; }
    std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const { if(bytes) bytes = 0; return ""; }
    std::pair<uint16_t, std::string> disassembleInstruction(const uint8_t* code, const uint8_t* end) { return {0,""}; }
};

}  // namespace emu
