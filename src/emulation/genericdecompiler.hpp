//---------------------------------------------------------------------------------------
// src/emulation/mc6800.hpp
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

#include <emulation/config.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace emu {

template<typename Core>
class GenericDecompiler
{
public:
    enum UsageType { eNONE = 0, eJUMP = 1, eCALL = 2, eSPRITE = 4, eLOAD = 8, eSTORE = 16, eREAD = 32, eWRITE = 64, eAUDIO = 128 };
    struct Chunk {
        uint32_t startAddr() const { return offset; }
        uint32_t endAddr() const { return offset + size(); }
        uint32_t size() const { return uint32_t(end - start); }
        uint32_t offset{};
        const uint8_t* start{};
        const uint8_t* end{};
        uint8_t usageType{};
    };
    struct LabelInfo {
        UsageType type{};
        int index{-1};
    };

    GenericDecompiler() = default;
    void decompile(std::string filename, const uint8_t* code, uint16_t offset, uint32_t size, uint16_t entry, std::ostream* os, bool analyzeOnly = false, bool quiet = false)
    {

    }

    uint32_t analyseCodeChunk(Chunk& chunk, uint16_t addr, const std::function<void(const Core&, uint16_t, int next)>& preCallback = {})
    {
        //std::clog << "    analysing chunk " << fmt::format("[0x{:04X}, 0x{:04X}, {}),  entry 0x{:04X}", chunk.offset, chunk.offset + chunk.size(), chunk.usageType, addr) << std::endl;
        auto* start = chunk.start + (addr - chunk.offset);
        auto code = start;
        uint32_t result = 0;

        Core ec(addr);
        while( code + 1 < chunk.end) {
            if(ec.rPC & 1)
                _oddPcAccess = true;
            auto opcode = readOpcode(code);
            Chip8Variant mask = (Chip8Variant)0;
            for(auto info : mappedOpcodeInfo[opcode])
                if(info) {
                    if(info->variants != Chip8Variant::MEGA_CHIP || opcode == 0x0011)
                        mask |= info->variants;
                }
            //const OpcodeInfo* info = mappedOpcodeInfo[opcode].front();
            if((uint64_t)mask) {
                possibleVariants &= mask;
                //if (!(uint64_t)possibleVariants)
                //    std::cerr << "huuuu" << std::endl;
            }
            //auto category = opcode >> 12;
            code += 2;
            ec.rPC += 2;
            int next = -1;
            if(opcode == 0xF000) {
                next = readOpcode(code);
                code += 2;
                ec.rPC += 2;
            }
            else if((opcode & 0xFF00) == 0x0100 && supportsVariant(C8V::MEGA_CHIP)) {
                next = readOpcode(code);
                code += 2;
                ec.rPC += 2;
            }
            else if(opcode == 0x0011 && supportsVariant(C8V::MEGA_CHIP))
                _megaChipEnabled = true;
            if(preCallback)
                preCallback(ec, opcode, next);
            if(executeSpeculative(ec, opcode, next)) {
                result = code - start;
                break;
            }
        }
        if(!result)
            result = chunk.end - chunk.start;
        //std::clog << "        found code of size " << result << std::endl;
        return result;
    }
private:
    Core _core;
    std::map<uint16_t, Chunk> chunks;
    std::map<uint16_t, LabelInfo> label;
};

}
