//---------------------------------------------------------------------------------------
// src/emulation/hpsaturnl2.hpp
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

#include <emulation/hardware/genericcpu.hpp>

#include <emulation/time.hpp>

#include <fmt/format.h>

#include <cstdint>



namespace emu {

class SaturnI2 : public GenericCpu {
public:
    uint32_t getCpuID() const override { return 48; }
    const std::string& getName() const override { static const std::string name = "SATURNL2"; return name; }
    const std::vector<std::string>& getRegisterNames() const override
    {
        static const std::vector<std::string> registerNames = {
            "A", "B", "C", "D",
            "R0", "R1", "R2", "R3", "R4",
            "IN", "OUT", "PC", "D0", "D1", "ST", "P", "H",
            "Carry"
        };
        return registerNames;
    }
private:
    uint64_t _rA{};
    uint64_t _rB{};
    uint64_t _rC{};
    uint64_t _rD{};
    std::array<uint64_t,5> _rR{};
    std::array<uint32_t,8> _rRSTK{};
    uint16_t _rIN{};    // 10 bit
    uint16_t _rOUT{};   // 10 bit
    uint32_t _rPC{};
    uint32_t _rD0{};
    uint32_t _rD1{};
    uint16_t _rST{};
    uint8_t _rP{};
    uint8_t _rHS{};
    bool _rCarry{false};
};

}
