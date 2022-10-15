//---------------------------------------------------------------------------------------
// src/emulation/chip8decompiler.hpp
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
// * 2022-07-29 initial version
// * 2022-07-31 successful decompilation of various roms works
//---------------------------------------------------------------------------------------
#pragma once

#include <emulation/chip8meta.hpp>

#include <iostream>
#include <chrono>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <tuple>
#include <vector>
#include <regex>

#include <fmt/format.h>

namespace emu {

class Chip8Decompiler
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
    struct EmulationContext {
        explicit EmulationContext(const uint16_t addr) : rPC(addr) {}
        int rV[16]{-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
        int rI{-1};
        uint16_t rPC{};
        uint8_t rSP{};
        uint8_t stack[16]{};
        bool inSkip{};
    };

    Chip8Decompiler()
    {
        if(mappedOpcodeInfo.empty()) {
            mappedOpcodeInfo.resize(65536);
            for(uint32_t opcode = 0; opcode < 0x10000; ++opcode) {
                for(const auto& info : detail::opcodes) {
                    if((opcode & info.mask) == info.opcode) {
                        mappedOpcodeInfo[opcode].push_back(&info);
                    }
                }
                if(mappedOpcodeInfo[opcode].empty())
                    mappedOpcodeInfo[opcode].push_back(nullptr);
            }
        }
        possibleVariants = static_cast<Chip8Variant>(~uint64_t{0});
    }

    void setVariant(Chip8Variant variant)
    {
        possibleVariants = variant;
    }

    static std::pair<std::string,std::string> chipVariantName(Chip8Variant cv);

    uint16_t readOpcode(const uint8_t* ptr) const
    {
        return (*ptr<<8) + *(ptr+1);
    }

    void refLabel(uint32_t addr, UsageType type)
    {
        auto i = label.find(addr);
        if(i == label.end()) {
            label[addr] = {type, -1};
        }
        else {
            label[addr].type = static_cast<UsageType>(label[addr].type | type);
        }
    }

    Chunk* findChunk(uint32_t addr)
    {
        for(auto& [offset, chunk] : chunks) {
            if(offset <= addr && chunk.end - chunk.start + offset > addr) {
                return &chunk;
            }
        }
        return nullptr;
    }

    void splitChunk(Chunk*& chunk, const uint8_t* start, uint32_t size, UsageType type)
    {
        if(chunk->start < start) {
            // seperate prefix chunk
            Chunk remainingChunk = *chunk;
            Chunk prefix = {chunk->offset, chunk->start, chunk->start + (start - chunk->start), chunk->usageType};
            remainingChunk.start = start;
            remainingChunk.offset = chunk->offset + (start - chunk->start);
            chunks[prefix.offset] = prefix;
            chunks[remainingChunk.offset] = remainingChunk;
            chunk = &chunks[remainingChunk.offset];
        }
        if(chunk->end > start + size) {
            // seperate suffix chunk
            Chunk suffix = {uint16_t(chunk->offset + (start - chunk->start) + size), start + size, chunk->end, chunk->usageType};
            chunks[suffix.offset] = suffix;
            chunk->end = start + size;
        }
        chunk->usageType = static_cast<UsageType>(chunk->usageType | type);
    }

    std::string labelOrAddress(uint32_t addr) const
    {
        auto i = label.find(addr);
        if(i != label.end()) {
            uint32_t number = i->second.index >= 0 ? i->second.index : addr;
            if(i->second.type & eJUMP) {
                return fmt::format("label_{}", number);
            }
            else if(i->second.type & eCALL) {
                return fmt::format("sub_{}", number);
            }
            else if(i->second.type & eSPRITE) {
                return fmt::format("sprite_{}", number);
            }
            else if(i->second.type & eAUDIO) {
                return fmt::format("audio_{}", number);
            }
            return fmt::format("data_{}", number);
        }
        return fmt::format("0x{:x}", addr);
    }

    std::tuple<uint16_t, uint16_t, std::string> opcode2Str(uint16_t opcode, int next) const
    {
        switch (opcode >> 12) {
            case 0:
                if (opcode == 0x0010) return {2, opcode, "megaoff"};
                if (opcode == 0x0011 && contained(possibleVariants, C8V::MEGA_CHIP)) return {2, opcode, "megaon"};
                if ((opcode & 0xFFF0) == 0x00B0 && contained(possibleVariants, C8V::MEGA_CHIP)) return {2, opcode, fmt::format("scroll-up-alt {}", opcode & 0xF)};
                if ((opcode & 0xFFF0) == 0x00C0) return {2, opcode, fmt::format("scroll-down {}", opcode & 0xF)};
                if ((opcode & 0xFFF0) == 0x00D0) return {2, opcode, fmt::format("scroll-up {}", opcode & 0xF)};
                if (opcode == 0x00E0) return {2, opcode, "clear"};
                if (opcode == 0x00EE) return {2, opcode, "return"};
                if (opcode == 0x00FB) return {2, opcode, "scroll-right"};
                if (opcode == 0x00FC) return {2, opcode, "scroll-left"};
                if (opcode == 0x00FE) return {2, opcode, "lores"};
                if (opcode == 0x00FF) return {2, opcode, "hires"};
                if ((opcode & 0xFF00) == 0x0100 && contained(possibleVariants, C8V::MEGA_CHIP)) return {4, opcode, fmt::format("ldhi {}", labelOrAddress(((opcode&0xFF)<<16)|next))};
                if ((opcode & 0xFF00) == 0x0200 && contained(possibleVariants, C8V::MEGA_CHIP)) return {2, opcode, fmt::format("ldpal {}", opcode & 0xFF)};
                if ((opcode & 0xFF00) == 0x0300 && contained(possibleVariants, C8V::MEGA_CHIP)) return {2, opcode, fmt::format("sprw {}", opcode & 0xFF)};
                if ((opcode & 0xFF00) == 0x0400 && contained(possibleVariants, C8V::MEGA_CHIP)) return {2, opcode, fmt::format("sprh {}", opcode & 0xFF)};
                if ((opcode & 0xFF00) == 0x0500 && contained(possibleVariants, C8V::MEGA_CHIP)) return {2, opcode, fmt::format("alpha {}", opcode & 0xFF)};
                if ((opcode & 0xFFF0) == 0x0600 && contained(possibleVariants, C8V::MEGA_CHIP)) return {2, opcode, fmt::format("digisnd {}", opcode & 0xF)};
                if (opcode == 0x0700 && contained(possibleVariants, C8V::MEGA_CHIP)) return {2, opcode, "stopsnd"};
                if ((opcode & 0xFFF0) == 0x0800 && contained(possibleVariants, C8V::MEGA_CHIP)) return {2, opcode, fmt::format("bmode {}", opcode & 0xF)};
                if ((opcode & 0xFF00) == 0x0900 && contained(possibleVariants, C8V::MEGA_CHIP)) return {2, opcode, fmt::format("ccol {}", opcode & 0xFF)};
                return {2, opcode, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
            case 1: return {2, 0x1000, fmt::format("jump {}", labelOrAddress(opcode & 0xFFF))};
            case 2: return {2, 0x2000, fmt::format(":call {}", labelOrAddress(opcode & 0xFFF))};
            case 3: return {2, 0x3000, fmt::format("if v{:X} != 0x{:02X} then", (opcode >> 8) & 0xF, opcode & 0xFF)};
            case 4: return {2, 0x4000, fmt::format("if v{:X} == 0x{:02X} then", (opcode >> 8) & 0xF, opcode & 0xFF)};
            case 5:
                if ((opcode & 0xF) == 0)
                    return {2, 0x5000, fmt::format("if v{:X} != v{:X} then", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                else if((opcode & 0xF) == 2)
                    return {2, 0x5002, fmt::format("save v{:X} - v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                else if((opcode & 0xF) == 3)
                    return {2, 0x5003, fmt::format("load v{:X} - v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                else
                    return {2, opcode & 0xF00F, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
            case 6: return {2, 0x6000, fmt::format("v{:X} := 0x{:02X}", (opcode >> 8) & 0xF, opcode & 0xff)};
            case 7: return {2, 0x7000, fmt::format("v{:X} += 0x{:02X}", (opcode >> 8) & 0xF, opcode & 0xff)};
            case 8:
                switch (opcode & 0xF) {
                    case 0: return {2, opcode & 0xF00F, fmt::format("v{:X} := v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                    case 1: return {2, opcode & 0xF00F, fmt::format("v{:X} |= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                    case 2: return {2, opcode & 0xF00F, fmt::format("v{:X} &= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                    case 3: return {2, opcode & 0xF00F, fmt::format("v{:X} ^= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                    case 4: return {2, opcode & 0xF00F, fmt::format("v{:X} += v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                    case 5: return {2, opcode & 0xF00F, fmt::format("v{:X} -= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                    case 6: return {2, opcode & 0xF00F, fmt::format("v{:X} >>= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                    case 7: return {2, opcode & 0xF00F, fmt::format("v{:X} =- v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                    case 0xE: return {2, opcode & 0xF00F, fmt::format("v{:X} <<= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                    default: return {2, opcode & 0xF00F, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
                }
                break;
            case 9:
                if((opcode & 0xF) == 0)
                    return {2, opcode & 0xF00F, fmt::format("if v{:X} == v{:X} then", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                else
                    return {2, opcode & 0xF00F, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
            case 0xA: return {2, 0xA000, fmt::format("i := {}", labelOrAddress(opcode & 0xFFF))};
            case 0xB: return {2, 0xB000, fmt::format("jump0 {}", labelOrAddress(opcode & 0xFFF))};
            case 0xC: return {2, 0xC000, fmt::format("v{:X} := random 0x{:02X}", (opcode >> 8) & 0xF, opcode & 0xff)};
            case 0xD: return {2, opcode & 0xF00F, fmt::format("sprite v{:X} v{:X} 0x{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF, opcode & 0xF)};
            case 0xE:
                switch (opcode & 0xFF) {
                    case 0x9E: return {2, opcode & 0xF0FF, fmt::format("if v{:X} -key then", (opcode >> 8) & 0xF)};
                    case 0xA1: return {2, opcode & 0xF0FF, fmt::format("if v{:X} key then", (opcode >> 8) & 0xF)};
                    default: return {2, opcode & 0xFFFF, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
                }
                break;
            case 0xF:
                switch (opcode & 0xFF) {
                    case 0x00: return {4, 0xF000, fmt::format("i := long {}", labelOrAddress(next))};
                    case 0x01: return {2, opcode & 0xF0FF, fmt::format("plane {}", (opcode >> 8) & 0xF)};
                    case 0x02:
                        if(opcode == 0xF002)
                            return {2, 0xF002, "audio"};
                        else
                            return {2, opcode, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
                    case 0x07: return {2, opcode & 0xF0FF, fmt::format("v{:X} := delay", (opcode >> 8) & 0xF)};
                    case 0x0A: return {2, opcode & 0xF0FF, fmt::format("v{:X} := key", (opcode >> 8) & 0xF)};
                    case 0x15: return {2, opcode & 0xF0FF, fmt::format("delay := v{:X}", (opcode >> 8) & 0xF)};
                    case 0x18: return {2, opcode & 0xF0FF, fmt::format("buzzer := v{:X}", (opcode >> 8) & 0xF)};
                    case 0x1E: return {2, opcode & 0xF0FF, fmt::format("i += v{:X}", (opcode >> 8) & 0xF)};
                    case 0x29: return {2, opcode & 0xF0FF, fmt::format("i := hex v{:X}", (opcode >> 8) & 0xF)};
                    case 0x30: return {2, opcode & 0xF0FF, fmt::format("i := hex v{:X} 10", (opcode >> 8) & 0xF)};
                    case 0x33: return {2, opcode & 0xF0FF, fmt::format("bcd v{:X}", (opcode >> 8) & 0xF)};
                    case 0x3A: return {2, opcode & 0xF0FF, fmt::format("pitch := v{:X}", (opcode >> 8) & 0xF)};
                    case 0x55: return {2, opcode & 0xF0FF, fmt::format("save v{:X}", (opcode >> 8) & 0xF)};
                    case 0x65: return {2, opcode & 0xF0FF, fmt::format("load v{:X}", (opcode >> 8) & 0xF)};
                    case 0x75: return {2, opcode & 0xF0FF, fmt::format("saveflags v{:X}", (opcode >> 8) & 0xF)};
                    case 0x85: return {2, opcode & 0xF0FF, fmt::format("loadflags v{:X}", (opcode >> 8) & 0xF)};
                    default:
                        return {2, opcode, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
                }
                break;
            default:
                return {2, opcode, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
        }
    }

    std::tuple<uint16_t, uint16_t, std::string> opcode2Str(const uint8_t* code, const uint8_t* end) const
    {
        auto opcode = readOpcode(code);
        auto next = code + 3 < end ? readOpcode(code + 2) : 0;
        return opcode2Str(opcode, next);
    }

    bool supportsVariant(emu::Chip8Variant variant) const
    {
        return contained(possibleVariants,  variant);
    }

    void disassembleChunk(Chunk& chunk, std::ostream& os)
    {
        auto addr = chunk.offset;
        auto* code = chunk.start;
        bool inIf = false;
        if(chunk.usageType & (eJUMP | eCALL)) {
            while (code + 1 < chunk.end) {
                auto [size, opcode, instruction] = opcode2Str(code, chunk.end);
                auto labelIter = label.find(addr);
                if (labelIter != label.end()) {
                    os << fmt::format(": {}", labelOrAddress(addr)) << std::endl;
                }
                // std::cout << fmt::format("{:04x}:  {:04x}  {}", addr, readOpcode(code), instruction) << std::endl;
                if(inIf)
                    os << fmt::format("            {}", instruction) << std::endl;
                else
                    os << fmt::format("        {}", instruction) << std::endl;
                inIf = instruction.rfind("if ", 0) == 0;
                addr += size;
                code += size;
            }
        }
        else {
            bool inSpriteMode = false;
            for(int i = 0; i < chunk.end - chunk.start; ++i, ++addr, ++code) {
                auto labelIter = label.find(addr);
                if (labelIter != label.end()) {
                    os << fmt::format("\n: {}\n", labelOrAddress(addr));
                    inSpriteMode = (labelIter->second.type & eSPRITE) && possibleVariants != C8V::MEGA_CHIP;
                }
                if(inSpriteMode) {
                    os << "        " << fmt::format("0b{:08b}\n", *code);
                }
                else {
                    if (!(i % 8) || labelIter != label.end())
                        os << (i > 0 ? "\n" : "") << "       ";
                    os << fmt::format(" 0x{:02X}", *code);
                }
            }
            os << std::endl;
        }
    }

    bool executeSpeculative(EmulationContext& ec, uint16_t opcode, int next = -1)
    {
        uint8_t x = (opcode >> 8) & 0xF;
        uint8_t y = (opcode >> 4) & 0xF;
        uint8_t n = opcode & 0xF;
        uint8_t nn = opcode & 0xFF;
        uint16_t nnn = opcode & 0xFFF;
        ec.rPC = (ec.rPC + 2);
        bool inSkip = false;
        bool endsChunk = false;
        switch (opcode >> 12) {
            case 0:
                if ((opcode & 0xFF00) == 0x0100) {
                    if (next >= 0)
                        ec.rI = ((opcode & 0xFF)<<8) | next;
                    else
                        ec.rI = -1;
                    refLabel(nnn, eREAD);
                }
                else if (opcode == 0x00E0) {  // 00E0 - CLS
                }
                else if (opcode == 0x00EE) {  // 00EE - RET
                    endsChunk = !ec.inSkip;
                }
                break;
            case 1:  // 1nnn - JP addr
                refLabel(nnn, eJUMP);
                endsChunk = !ec.inSkip;
                break;
            case 2:  // 2nnn - CALL addr
                refLabel(nnn, eCALL);
                break;
            case 3:  // 3xkk - SE Vx, byte
                inSkip = true;
                if (ec.rV[x] >= 0 && ec.rV[x] == nn) {
                    ec.rPC += 2;
                }
                break;
            case 4:  // 4xkk - SNE Vx, byte
                inSkip = true;
                if (ec.rV[x] >= 0 && ec.rV[x] != nn) {
                    ec.rPC += 2;
                }
                break;
            case 5:
                switch (n) {
                    case 0: // 5xy0 - SE Vx, Vy
                        inSkip = true;
                        if (ec.rV[x] >= 0 && ec.rV[y] >= 0 && ec.rV[x] == ec.rV[y]) {
                            ec.rPC += 2;
                        }
                        break;
                    case 3: // 5xy3 - load vx - vy
                        for(int i=0; i <= std::abs(x-y); ++i)
                            ec.rV[x < y ? x + i : x - i] = -1;
                        break;
                    default:
                        break;
                }
                break;
            case 6:  // 6xnn - LD Vx, byte
                if(ec.inSkip)
                    ec.rV[x] = -1;
                else
                    ec.rV[x] = nn;
                break;
            case 7:  // 7xkk - ADD Vx, byte
                if(ec.rV[x] >= 0) {
                    if (ec.inSkip) {
                        ec.rV[x] = -1;
                    }
                    else {
                        ec.rV[x] += nn;
                    }
                }
                break;
            case 8: {
                switch (opcode & 0xF) {
                    case 0:  // 8xy0 - LD Vx, Vy
                        if(ec.inSkip)
                            ec.rV[x] = -1;
                        else
                            ec.rV[x] = ec.rV[y];
                        break;
                    case 1:  // 8xy1 - OR Vx, Vy
                        if(!ec.inSkip && ec.rV[x] >= 0 && ec.rV[y] >= 0)
                            ec.rV[x] |= ec.rV[y];
                        else
                            ec.rV[x] = -1;
                        ec.rV[0xF] = -1;
                        break;
                    case 2:  // 8xy2 - AND Vx, Vy
                        if(!ec.inSkip && ec.rV[x] >= 0 && ec.rV[y] >= 0)
                            ec.rV[x] &= ec.rV[y];
                        else
                            ec.rV[x] = -1;
                        ec.rV[0xF] = -1;
                        break;
                    case 3:  // 8xy3 - XOR Vx, Vy
                        if(!ec.inSkip && ec.rV[x] >= 0 && ec.rV[y] >= 0)
                            ec.rV[x] ^= ec.rV[y];
                        else
                            ec.rV[x] = -1;
                        ec.rV[0xF] = -1;
                        break;
                    case 4: {  // 8xy4 - ADD Vx, Vy
                        if(!ec.inSkip && ec.rV[x] >= 0 && ec.rV[y] >= 0) {
                            uint16_t result = ec.rV[x] + ec.rV[y];
                            ec.rV[x] = result & 0xFF;
                            ec.rV[0xF] = result >> 8;
                        }
                        else
                            ec.rV[x] = -1, ec.rV[0xF] = -1;
                        break;
                    }
                    case 5: {  // 8xy5 - SUB Vx, Vy
                        if(!ec.inSkip && ec.rV[x] >= 0 && ec.rV[y] >= 0) {
                            uint16_t result = ec.rV[x] - ec.rV[y];
                            ec.rV[x] = result & 0xFF;
                            ec.rV[0xF] = result > 255 ? 0 : 1;
                        }
                        else
                            ec.rV[x] = -1, ec.rV[0xF] = -1;
                        break;
                    }
                    case 6:  // 8xy6 - SHR Vx, Vy
                        ec.rV[x] = -1, ec.rV[0xF] = -1;
                        /*
                        if (!_options.optJustShiftVx) {
                            uint8_t carry = _rV[(opcode >> 4) & 0xF] & 1;
                            _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] >> 1;
                            _rV[0xF] = carry;
                        }
                        else {
                            uint8_t carry = _rV[(opcode >> 8) & 0xF] & 1;
                            _rV[(opcode >> 8) & 0xF] >>= 1;
                            _rV[0xF] = carry;
                        }*/
                        break;
                    case 7: {  // 8xy7 - SUBN Vx, Vy
                        if(!ec.inSkip && ec.rV[x] >= 0 && ec.rV[y] >= 0) {
                            uint16_t result = ec.rV[y] - ec.rV[x];
                            ec.rV[x] = result & 0xFF;
                            ec.rV[0xF] = result > 255 ? 0 : 1;
                        }
                        else
                            ec.rV[x] = -1, ec.rV[0xF] = -1;
                        break;
                    }
                    case 0xE:  // 8xyE - SHL Vx, Vy
                        ec.rV[x] = -1, ec.rV[0xF] = -1;
                        /*
                        if (!_options.optJustShiftVx) {
                            uint8_t carry = _rV[(opcode >> 4) & 0xF] >> 7;
                            _rV[(opcode >> 8) & 0xF] = _rV[(opcode >> 4) & 0xF] << 1;
                            _rV[0xF] = carry;
                        }
                        else {
                            uint8_t carry = _rV[(opcode >> 8) & 0xF] >> 7;
                            _rV[(opcode >> 8) & 0xF] <<= 1;
                            _rV[0xF] = carry;
                        }*/
                        break;
                    default:
                        break;
                }
                break;
            }
            case 9:  // 9xy0 - SNE Vx, Vy
                if(n == 0) {
                    inSkip = true;
                    if (ec.rV[x] >= 0 && ec.rV[y] >= 0 && ec.rV[x] != ec.rV[y]) {
                        ec.rPC += 2;
                    }
                }
                break;
            case 0xA:  // Annn - LD I, addr
                if(ec.inSkip)
                    ec.rI = -1;
                else
                    ec.rI = nnn;
                refLabel(nnn, eREAD);
                break;
            case 0xB:  // Bnnn - JP V0, addr
                if(ec.rV[0] >= 0)
                    refLabel(nnn + ec.rV[0], eJUMP);
                else
                    refLabel(nnn, eJUMP); // TODO: Check for possible detailed target address for new code scan
                endsChunk = !ec.inSkip;
                break;
            case 0xC: {  // Cxkk - RND Vx, byte
                ec.rV[x] = -1;
                break;
            }
            case 0xD: {  // Dxyn - DRW Vx, Vy, nibble
                if(ec.rI >= 0) {
                    refLabel(ec.rI, eSPRITE);
                }
                ec.rV[0xF] = -1;
                break;
            }
            case 0xE:
                if ((opcode & 0xff) == 0x9E) {  // Ex9E - SKP Vx
                    inSkip = true;
                }
                else if ((opcode & 0xff) == 0xA1) {  // ExA1 - SKNP Vx
                    inSkip = true;
                }
                break;
            case 0xF: {
                switch (opcode & 0xFF) {
                    case 0x00:  // i := long nnnn
                        if(opcode == 0xF000)
                            ec.rI = next;
                        break;
                    case 0x02:
                        if(opcode == 0xF002 && ec.rI >= 0)
                            refLabel(ec.rI, eAUDIO);
                        break;
                    case 0x07:  // Fx07 - LD Vx, DT
                    case 0x0A:  // Fx0A - LD Vx, K
                        ec.rV[x] = -1;
                        break;
                    case 0x15:  // Fx15 - LD DT, Vx
                    case 0x18:  // Fx18 - LD ST, Vx
                        break;
                    case 0x1E:  // Fx1E - ADD I, Vx
                        if(!ec.inSkip && ec.rI >= 0 && ec.rV[x] >= 0)
                            ec.rI += ec.rV[x];
                        else
                            ec.rI = -1;
                        break;
                    case 0x29:  // Fx29 - LD F, Vx
                        ec.rI = -1;
                        break;
                    case 0x33: {  // Fx33 - LD B, Vx
                        if(ec.rI >= 0) {
                            refLabel(ec.rI, eWRITE);
                        }
                        break;
                    }
                    case 0x55: {  // Fx55 - LD [I], Vx
                        if(ec.rI >= 0) {
                            refLabel(ec.rI, eWRITE);
                        }
                        ec.rI = -1;
                        break;
                    }
                    case 0x65: {  // Fx65 - LD Vx, [I]
                        if(ec.rI >= 0) {
                            refLabel(ec.rI, eREAD);
                        }
                        for (int i = 0; i <= x; ++i)
                            ec.rV[i] = -1;
                        ec.rI = -1;
                        break;
                    }
                    case 0x85: {  // Fx85 - loadflags Vx
                        if(ec.rI >= 0) {
                            refLabel(ec.rI, eREAD);
                        }
                        for (int i = 0; i <= x; ++i)
                            ec.rV[i] = -1;
                        ec.rI = -1;
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
        }
        ec.inSkip = inSkip;
        return endsChunk;
    }

    uint32_t analyseCodeChunk(Chunk& chunk, uint16_t addr, const std::function<void(const EmulationContext&, uint16_t, int next)>& preCallback = {})
    {
        //std::clog << "    analysing chunk " << fmt::format("[0x{:04X}, 0x{:04X}, {}),  entry 0x{:04X}", chunk.offset, chunk.offset + chunk.size(), chunk.usageType, addr) << std::endl;
        auto* start = chunk.start + (addr - chunk.offset);
        auto code = start;
        uint32_t result = 0;

        EmulationContext ec(addr);
        while( code + 1 < chunk.end) {
            if(ec.rPC & 1)
                _oddPcAccess = true;
            auto opcode = readOpcode(code);
            Chip8Variant mask = (Chip8Variant)0;
            for(auto info : mappedOpcodeInfo[opcode])
                if(info)
                    mask |= info->variants;
            //const OpcodeInfo* info = mappedOpcodeInfo[opcode].front();
            if((uint64_t)mask) {
                possibleVariants &= mask;
                if (!(uint64_t)possibleVariants)
                    std::cerr << "huuuu" << std::endl;
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

    void dumpChunks()
    {
        return;
        std::clog << "    Chunks:";
        for(auto& [offset, chunk] : chunks) {
            std::clog << fmt::format("   [0x{:04X}, 0x{:04X}, {})", offset, offset + chunk.size(), chunk.usageType);
        }
        std::clog << std::endl;

    }

    void renumerateLabels()
    {
        int jumpLabel = 0;
        int subLabel = 0;
        int dataLabel = 0;
        int spriteLabel = 0;
        int audioLabel = 0;
        for(auto& [addr, info] : label) {
            if(info.type & eJUMP) {
                info.index = jumpLabel++;
            }
            else if(info.type & eCALL) {
                info.index = subLabel++;
            }
            else if(info.type & eSPRITE) {
                info.index = spriteLabel++;
            }
            else if(info.type & eAUDIO) {
                info.index = audioLabel++;
            }
            else {
                info.index = dataLabel++;
            }
        }
    }

    void generateInfoFromChunk(Chunk& chunk)
    {
        auto addr = chunk.offset;
        auto* code = chunk.start;
        bool inIf = false;
        if(chunk.usageType & (eJUMP | eCALL)) {
            while (code + 1 < chunk.end) {
                auto [size, opcode, instruction] = opcode2Str(code, chunk.end);
                auto iter = stats.find(opcode);
                if(iter == stats.end()) {
                    stats.emplace(opcode, 1);
                }
                else
                    ++iter->second;
                auto iter2 = totalStats.find(opcode);
                if(iter2 == totalStats.end()) {
                    totalStats.emplace(opcode, 1);
                }
                else
                    ++iter2->second;
                addr += size;
                code += size;
            }
        }
    }

    void decompile(std::string filename, const uint8_t* code, uint16_t offset, uint32_t size, uint16_t entry, std::ostream* os, bool analyzeOnly = false, bool quiet = false)
    {
        auto start = std::chrono::steady_clock::now();
        _start = code;
        _size = size;
        chunks[offset] = {offset, code, code + size, eNONE};
        auto chunkSize = analyseCodeChunk(chunks[offset], entry);
        Chunk* chunk = &chunks[offset];
        splitChunk(chunk, code, chunkSize, eJUMP);
        dumpChunks();

        bool iterate;
        do {
            iterate = false;
            for (auto& [labelOffset, info] : label) {
                if(info.type & (eJUMP | eCALL)) {
                    chunk = findChunk(labelOffset);
                    if (chunk && chunk->usageType == eNONE) {
                        chunkSize = analyseCodeChunk(*chunk, labelOffset);
                        splitChunk(chunk, chunk->start + (labelOffset - chunk->offset), chunkSize, info.type);
                        dumpChunks();
                        iterate = true;
                    }
                }
            }
        } while(iterate);

        if(analyzeOnly) {
            for (auto& [chunkOffset, chunk] : chunks) {
                // std::cout << fmt::format(":org {:04X} # size: {:04X}", chunkOffset, uint32_t(chunk.end - chunk.start)) << std::endl;
                generateInfoFromChunk(chunk);
            }
            if(!quiet && os) {
                *os << ", " << stats.size() << " opcodes used";
            }
        }
        else if(os) {
            renumerateLabels();
            *os << "# This is an automatically generated source, created by the Cadmium-Decompiler\n# ROM file used: " << filename << "\n\n";
            if(possibleVariants == C8V::MEGA_CHIP) {
                *os << R"(#--------------------------------------------------------------
# MegaChip support macros
:macro megaoff { :byte 0x00  :byte 0x10 }
:macro megaon { :byte 0x00 :byte 0x11 }
:macro scroll_up n {
    :calc BN { 0xB0 + ( n & 0xF ) }
    :byte 0x00 :byte BN
}
:macro ldhi nnnnnn {
    :calc B1 { nnnnnn >> 16 }
    :calc B2 { ( nnnnnn >> 8 ) & 0xFF }
    :calc B3 { nnnnnn & 0xFF }
    :byte 0x01 :byte B1 :byte B2 :byte B3
}
:macro ldpal nn { :byte 0x02 :byte nn }
:macro sprw nn { :byte 0x03 :byte nn }
:macro sprh nn { :byte 0x04 :byte nn }
:macro alpha nn { :byte 0x05 :byte nn }
:macro digisnd n { :calc ZN { n & 0xF } :byte 0x06 :byte ZN }
:macro stopsnd { :byte 0x07 :byte 0x00 }
:macro bmode n { :calc ZN { n & 0xF } :byte 0x08 :byte ZN }
:macro ccol nn { :byte 0x09 :byte nn }
#--------------------------------------------------------------
)";
            }
            bool hasConsts = false;
            for(auto& [addr, info] : label) {
                if(!findChunk(addr)) {
                    *os << fmt::format(":const {} 0x{:04X}\n", labelOrAddress(addr), addr);
                    hasConsts = true;
                }
            }
            if(hasConsts && os) *os << "\n";
            *os << ": main" << std::endl;
            for (auto& [chunkOffset, chunk] : chunks) {
                // std::cout << fmt::format(":org {:04X} # size: {:04X}", chunkOffset, uint32_t(chunk.end - chunk.start)) << std::endl;
                disassembleChunk(chunk, *os);
            }
        }
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if(!quiet && os) *os << " (" << duration << "ms)" << std::endl;
    }

    void listUsages(uint16_t forOpcode, uint16_t mask, std::ostream& os)
    {
        static std::regex rxReg("v([0-9A-F])");
        for (auto& [chunkOffset, chunk] : chunks) {
            auto addr = chunk.offset;
            auto* code = chunk.start;
            if(chunk.usageType & (eJUMP | eCALL)) {
                analyseCodeChunk(chunk, chunk.offset, [&](const EmulationContext& ec, uint16_t opcode, int next){
                    if((opcode & mask) == forOpcode) {
                        auto [size, op, instruction] = opcode2Str(opcode, next);
                        os << "    " << instruction;
                        bool first = true;
                        for(auto i = std::sregex_iterator(instruction.begin(), instruction.end(), rxReg);
                             i != std::sregex_iterator();
                             ++i ) {
                            auto regVal = ec.rV[std::stoi((*i)[1].str(), nullptr, 16)];
                            if(regVal >= 0) {
                                if (first)
                                    os << "    #";
                                os << " " << i->str() << "=" << regVal;
                                first = false;
                            }
                        }
                        os << "\n";
                    }
                });
            }
        }
    }

    std::string _filename;
    const uint8_t* _start{};
    uint32_t _size{};
    bool _oddPcAccess{false};
    Chip8Variant possibleVariants{};
    std::map<uint16_t, Chunk> chunks;
    std::map<uint16_t, LabelInfo> label;
    std::unordered_map<uint16_t, int> stats;
    inline static std::map<uint16_t, int> totalStats;
    inline static std::vector<std::vector<const OpcodeInfo*>> mappedOpcodeInfo;
};

}

