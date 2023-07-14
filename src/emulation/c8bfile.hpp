//---------------------------------------------------------------------------------------
// src/emulation/chipsound.hpp
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

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <map>
#include <vector>

class C8BFile
{
public:
    enum Chip8Variant {
        // based on https://chip-8.github.io/extensions
        C8V_CHIP_8 = 0x01,             // CHIP-8
        C8V_CHIP_8_1_2 = 0x02,         // CHIP-8 1/2
        C8V_CHIP_8_I = 0x03,           // CHIP-8I
        C8V_CHIP_8_II = 0x04,          // CHIP-8 II aka. Keyboard Kontrol
        C8V_CHIP_8_III = 0x05,         // CHIP-8III
        C8V_CHIP_8_TPD = 0x06,         // Two-page display for CHIP-8
        C8V_CHIP_8C = 0x07,            // CHIP-8C
        C8V_CHIP_10 = 0x08,            // CHIP-10
        C8V_CHIP_8_SRV = 0x09,         // CHIP-8 modification for saving and restoring variables
        C8V_CHIP_8_SRV_I = 0x0A,       // Improved CHIP-8 modification for saving and restoring variables
        C8V_CHIP_8_RB = 0x0B,          // CHIP-8 modification with relative branching
        C8V_CHIP_8_ARB = 0x0C,         // Another CHIP-8 modification with relative branching
        C8V_CHIP_8_FSD = 0x0D,         // CHIP-8 modification with fast, single-dot DXYN
        C8V_CHIP_8_IOPD = 0x0E,        // CHIP-8 with I/O port driver routine
        C8V_CHIP_8_8BMD = 0x0F,        // CHIP-8 8-bit multiply and divide
        C8V_HI_RES_CHIP_8 = 0x10,      // HI-RES CHIP-8 (four-page display)
        C8V_HI_RES_CHIP_8_IO = 0x11,   // HI-RES CHIP-8 with I/O
        C8V_HI_RES_CHIP_8_PS = 0x12,   // HI-RES CHIP-8 with page switching
        C8V_CHIP_8E = 0x13,            // CHIP-8E
        C8V_CHIP_8_IBNNN = 0x14,       // CHIP-8 with improved BNNN
        C8V_CHIP_8_SCROLL = 0x15,      // CHIP-8 scrolling routine
        C8V_CHIP_8X = 0x16,            // CHIP-8X
        C8V_CHIP_8X_TPD = 0x17,        // Two-page display for CHIP-8X
        C8V_HI_RES_CHIP_8X = 0x18,     // Hi-res CHIP-8X
        C8V_CHIP_8Y = 0x19,            // CHIP-8Y
        C8V_CHIP_8_CtS = 0x1A,         // CHIP-8 “Copy to Screen”
        C8V_CHIP_BETA = 0x1B,          // CHIP-BETA
        C8V_CHIP_8M = 0x1C,            // CHIP-8M
        C8V_MULTIPLE_NIM = 0x1D,       // Multiple Nim interpreter
        C8V_DOUBLE_ARRAY_MOD = 0x1E,   // Double Array Modification
        C8V_CHIP_8_D6800 = 0x1F,       // CHIP-8 for DREAM 6800 (CHIPOS)
        C8V_CHIP_8_D6800_LOP = 0x20,   // CHIP-8 with logical operators for DREAM 6800 (CHIPOSLO)
        C8V_CHIP_8_D6800_JOY = 0x21,   // CHIP-8 for DREAM 6800 with joystick
        C8V_2K_CHIPOS_D6800 = 0x22,    // 2K CHIPOS for DREAM 6800
        C8V_CHIP_8_ETI660 = 0x23,      // CHIP-8 for ETI-660
        C8V_CHIP_8_ETI660_COL = 0x24,  // CHIP-8 with color support for ETI-660
        C8V_CHIP_8_ETI660_HR = 0x25,   // CHIP-8 for ETI-660 with high resolution
        C8V_CHIP_8_COSMAC_ELF = 0x26,  // CHIP-8 for COSMAC ELF
        C8V_CHIP_8_ACE_VDU = 0x27,     // CHIP-VDU / CHIP-8 for the ACE VDU
        C8V_CHIP_8_AE = 0x28,          // CHIP-8 AE (ACE Extended)
        C8V_CHIP_8_DC_V2 = 0x29,       // Dreamcards Extended CHIP-8 V2.0
        C8V_CHIP_8_AMIGA = 0x2A,       // Amiga CHIP-8 interpreter
        C8V_CHIP_48 = 0x2B,            // CHIP-48
        C8V_SCHIP_1_0 = 0x2C,          // SUPER-CHIP 1.0
        C8V_SCHIP_1_1 = 0x2D,          // SUPER-CHIP 1.1
        C8V_GCHIP = 0x2E,              // GCHIP
        C8V_SCHIPC_GCHIPC = 0x2F,      // SCHIP Compatibility (SCHPC) and GCHIP Compatibility (GCHPC)
        C8V_VIP2K_CHIP_8 = 0x30,       // VIP2K CHIP-8
        C8V_SCHIP_1_1_SCRUP = 0x31,    // SUPER-CHIP with scroll up
        C8V_CHIP8RUN = 0x32,           // chip8run
        C8V_MEGA_CHIP = 0x33,          // Mega-Chip
        C8V_XO_CHIP = 0x34,            // XO-CHIP
        C8V_OCTO = 0x35,               // Octo
        C8V_CHIP_8_CL_COL = 0x36       // CHIP-8 Classic / Color
    };
    enum Status { eOK, eREAD_ERROR, eINVALID_C8B, eVERSION_ERROR };
    enum KeySym { eUP, eDOWN, eLEFT, eRIGHT, eA, eB };
    enum Orientation { eNORMAL, eRIGHT90, eLEFT90, eUPSIDEDOWN };
    struct Image
    {
        uint8_t planes{};
        uint8_t widthInBytes{};
        uint8_t height{};
        const uint8_t* pixel{};
    };
    struct Color
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    C8BFile() = default;

    Status load(std::string file)
    {
        try {
            std::ifstream is(file, std::ios::binary | std::ios::ate);
            std::streamsize size = is.tellg();
            is.seekg(0, std::ios::beg);

            rawData.resize(size);
            if (!is.read((char*)rawData.data(), size))
                return eREAD_ERROR;
            size = is.gcount();
            rawData.resize(size);
            return parse();
        }
        catch(std::exception&) {
            return eINVALID_C8B;
        }
    }

    Status loadFromData(const uint8_t* data, size_t size)
    {
        rawData.assign(data, data + size);
        return parse();
    }

    Status parse()
    {
        try {
            auto  size = rawData.size();
            if (size < 6)
                return eINVALID_C8B;
            if (std::memcmp(rawData.data(), "\x43\x42\x46", 3))
                return eINVALID_C8B;
            if (rawData[3] != 0)
                return eVERSION_ERROR;
            if (rawData[4]) {
                size_t bci = rawData[4];
                while (bci < size && rawData[bci]) {
                    if (bci + 4 >= size)
                        return eINVALID_C8B;
                    size_t offset = readWord(bci + 1);
                    size_t bytecodeSize = readWord(bci + 3);
                    if(offset + bytecodeSize > size)
                        return eINVALID_C8B;
                    variantBytecode[static_cast<Chip8Variant>(rawData[bci])] = {offset, bytecodeSize};
                    bci += 5;
                }
                if (bci >= size)
                    return eINVALID_C8B;
            }
            if(rawData[5]) {
                size_t pi = rawData[5];
                while(pi < size && rawData[pi]) {
                    if(pi + 2 >= size)
                        return eINVALID_C8B;
                    uint16_t offset = readWord(pi + 1);
                    if(offset >= size)
                        return eINVALID_C8B;
                    switch(rawData[pi]) {
                        case 1:
                            executionSpeed = read24bit(offset);
                            break;
                        case 2:
                            programName = readString(offset);
                            break;
                        case 3:
                            description = readString(offset);
                            break;
                        case 4:
                            authors.push_back(readString(offset));
                            break;
                        case 5:
                            url.push_back(readString(offset));
                            break;
                        case 6:
                            releaseDate = readLong(offset);
                            break;
                        case 7:
                            if(offset + 2 >= size)
                                return eINVALID_C8B;
                            coverArt.planes = rawData[offset];
                            coverArt.widthInBytes = rawData[offset + 1];
                            coverArt.height = rawData[offset + 2];
                            if(offset + 3 + coverArt.planes * coverArt.widthInBytes * coverArt.height >= size)
                                return eINVALID_C8B;
                            coverArt.pixel = rawData.data() + offset + 3 + coverArt.planes * coverArt.widthInBytes * coverArt.height;
                            break;
                        case 8:
                            if(offset + 1 + rawData[offset]*2 >= size)
                                return eINVALID_C8B;
                            for(int i = 0; i < rawData[offset]; ++i) {
                                keyMap[static_cast<KeySym>(rawData[offset + i*2 + 1])] = rawData[offset + i*2 + 2];
                            }
                            break;
                        case 9:
                            if(offset + 1 + rawData[offset]*3 >= size)
                                return eINVALID_C8B;
                            for(int i = 0; i < rawData[offset]; ++i) {
                                palette.push_back({rawData[offset + i*3 + 1], rawData[offset + i*3 + 2], rawData[offset + i*3 + 3]});
                            }
                            break;
                        case 0xA:
                            orientation = static_cast<Orientation>(rawData[offset]);
                            break;
                        case 0xB:
                            if(offset + 2 >= size)
                                return eINVALID_C8B;
                            fontAddress = readWord(offset);
                            fontDataSize = rawData[offset + 2];
                            if(offset + 2 + fontDataSize >= size)
                                return eINVALID_C8B;
                            fontDataOffset = offset + 3;
                            break;
                        case 0xC:
                            toolInfo = readString(offset);
                            break;
                        case 0xD:
                            license = readString(offset);
                            break;
                        default:
                            break;
                    }
                    pi += 3;
                }
                if(pi >= size)
                    return eINVALID_C8B;
            }
        }
        catch(std::exception&) {
            return eINVALID_C8B;
        }
        return eOK;
    }

    std::map<Chip8Variant,std::pair<uint16_t,uint16_t>>::const_iterator findBestMatch(std::initializer_list<Chip8Variant> variants)
    {
        for(auto cv : variants) {
            auto iter = variantBytecode.find(cv);
            if(iter != variantBytecode.end()) {
                return iter;
            }
        }
        return variantBytecode.end();
    }

    uint16_t readWord(uint16_t offset)
    {
        if(offset + 1 >= rawData.size())
            throw std::runtime_error("offset out of range");
        return (rawData[offset]<<8)|rawData[offset+1];
    }

    uint16_t read24bit(uint16_t offset)
    {
        if(offset + 1 >= rawData.size())
            throw std::runtime_error("offset out of range");
        return (rawData[offset]<<16)|(rawData[offset+1]<<8)|rawData[offset+2];
    }

    uint32_t readLong(uint16_t offset)
    {
        if(offset + 3 >= rawData.size())
            throw std::runtime_error("offset out of range");
        return (rawData[offset]<<24)|(rawData[offset+1]<<16)|(rawData[offset+2]<<8)|rawData[offset+3];
    }

    std::string readString(uint16_t offset)
    {
        uint16_t len = 0;
        while(offset + len < rawData.size() && rawData[offset + len])
            ++len;
        if(offset + len >= rawData.size())
            throw std::runtime_error("string out of range");
        return {rawData.data() + offset, rawData.data() + offset + len};
    }

    std::string filename;
    uint32_t executionSpeed{};
    std::string programName;
    std::string description;
    std::vector<std::string> authors;
    std::vector<std::string> url;
    uint32_t releaseDate{};
    Image coverArt{0,0,0,{}};
    std::map<KeySym,uint8_t> keyMap;
    std::vector<Color> palette;
    Orientation orientation{};
    uint16_t fontAddress{};
    uint8_t fontDataSize{};
    uint16_t fontDataOffset{};
    std::string toolInfo;
    std::string license;
    std::map<Chip8Variant,std::pair<uint16_t,uint16_t>> variantBytecode;
    std::vector<uint8_t> rawData;
};