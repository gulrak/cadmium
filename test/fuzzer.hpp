//---------------------------------------------------------------------------------------
// tests/fuzzer.hpp
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

#include <stdexcept>

#include <fmt/format.h>
#include <nlohmann/json.hpp>


namespace fuzz {

using json = nlohmann::ordered_json;

extern void rndSeed(uint64_t seed);
extern uint8_t rndByte();
extern uint16_t rndWord();

class FuzzerException : public std::runtime_error
{
public:
    FuzzerException(const std::string& what = "") : std::runtime_error(what) {}
};

struct FuzzerMemory
{
    enum CompareType { eMEMONLY, eWRITECYCLES, eALLCYCLES};
    enum AccessType { eNONE, eREAD, eWRITE, eADDITIONAL_READ, eADDITIONAL_WRITE };
    struct MemEntry {
        uint16_t addr;
        uint8_t data;
    };
    using MemEntries = std::vector<MemEntry>;
    struct BusCycle {
        uint16_t addr;
        uint8_t data;
        AccessType type;
    };
    using BusCycles = std::vector<BusCycle>;
    FuzzerMemory(uint8_t opcode)
        : isGenerating(true)
        , startByte(opcode)
    {
        initialRam.reserve(32);
        currentRam.reserve(32);
    }
    FuzzerMemory(const json& init)
        : isGenerating(false)
    {
        currentRam.reserve(32);
        for (const auto& entry : init) {
            uint16_t addr = entry[0];
            uint8_t value = entry[1];
            initialRam.push_back({addr, value});
        }
    }
    FuzzerMemory(const MemEntries& init)
        : isGenerating(false)
    {
        initialRam = init;
        currentRam = init;
        currentRam.reserve(32);
    }
    void reset()
    {
        initialRam.clear();
        currentRam.clear();
        cycles.clear();
        isGenerating = true;
    }
    void prepare(const FuzzerMemory& other)
    {
        isGenerating = false;
        initialRam = other.initialRam;
        currentRam = other.initialRam;
        cycles.clear();
    }
    uint8_t readByte(uint16_t addr, bool hidden = false)
    {
        AccessType type = eREAD;
        const uint8_t* data = findAddr(addr, currentRam);
        uint8_t value;
        if(data) {
            value = *data;
        }
        else {
            if(!isGenerating)
                type = eADDITIONAL_READ;

            value = initialRam.empty() ? startByte : rndByte();
            initialRam.push_back({addr, value});
            currentRam.push_back({addr, value});
        }
        if(!hidden)
            cycles.push_back({addr, value, type});
        return value;
    }
    void passiveRead(uint16_t addr)
    {
        cycles.push_back({addr, 0, eNONE});
    }
    void writeByte(uint16_t addr, uint8_t val)
    {
        static int c = 0;
        AccessType type = eWRITE;
        uint8_t* data = findAddr(addr, currentRam);
        if(data) {
            *data = val;
            if(!isGenerating)
                ++c;
        }
        else {
            if(!isGenerating)
                type = eADDITIONAL_WRITE;
            initialRam.push_back({addr, val});
            currentRam.push_back({addr, val});
        }
        cycles.push_back({addr, val, type});
    }
    bool compareToReference(const FuzzerMemory& reference, CompareType comp)
    {
        for(const auto& entryRef : reference.currentRam) {
            bool found = false;
            for(const auto& entryTst : currentRam) {
                if(entryRef.addr == entryTst.addr) {
                    if(entryRef.data != entryTst.data)
                        throw FuzzerException("ram mismatch");
                    found = true;
                    break;
                }
             }
             if(!found)
                throw FuzzerException("ram mismatch");
        }
        if(comp != eMEMONLY) {
             auto iterTest = cycles.begin();
             auto iterRef = reference.cycles.begin();
             while (iterTest != cycles.end() && iterRef != reference.cycles.end()) {
                if (comp == eALLCYCLES) {
                    if (iterTest->addr != iterRef->addr || iterTest->type != iterRef->type || iterTest->data != iterRef->data)
                        throw FuzzerException("cycles mismatch");
                    iterTest++;
                    iterRef++;
                }
                else {
                    skipNonWrites(iterTest, cycles.end());
                    skipNonWrites(iterRef, reference.cycles.end());
                    /*while (iterTest != cycles.end() && (iterTest->type == eREAD || iterTest->type == eADDITIONAL_READ || iterTest->type == eNONE))
                        iterTest++;
                    while (iterRef != reference.cycles.end() && (iterRef->type == eREAD || iterRef->type == eADDITIONAL_READ || iterRef->type == eNONE))
                        iterRef++;*/
                    if (iterTest != cycles.end() && iterRef != reference.cycles.end()) {
                        if (iterTest->addr != iterRef->addr || iterTest->data != iterRef->data)
                            throw FuzzerException("write cycles mismatch");
                        iterTest++;
                        iterRef++;
                    }
                    skipNonWrites(iterTest, cycles.end());
                    skipNonWrites(iterRef, reference.cycles.end());
                    if (iterTest == cycles.end() || iterRef == reference.cycles.end())
                        break;
                }
             }
             if (iterTest != cycles.end() || iterRef != reference.cycles.end())
                throw FuzzerException("write cycles mismatch");
        }
        return true;
    }
    template<typename Iter>
    void skipNonWrites(Iter& iter, const Iter& end)
    {
        while (iter != end && iter->type != eWRITE && iter->type != eADDITIONAL_WRITE)
             iter++;
    }
    uint8_t* findAddr(uint16_t addr, MemEntries& entries)
    {
        for(auto& entry : entries) {
            if(entry.addr == addr)
                return &entry.data;
        }
        return nullptr;
    }
    bool isGenerating = false;
    uint8_t startByte{};
    MemEntries initialRam;
    MemEntries currentRam;
    BusCycles cycles;
};

inline void to_json(json& j, const FuzzerMemory::MemEntry& entry) {
    j = json::array({entry.addr, entry.data});
}

inline void from_json(const json& j, FuzzerMemory::MemEntry& entry) {
    j.at(0).get_to(entry.addr);
    j.at(1).get_to(entry.data);
}

inline void to_json(json& j, const FuzzerMemory::BusCycle& entry) {
    if(entry.type == FuzzerMemory::eNONE)
        j = json::array({"n", entry.addr});
    else
        j = json::array({entry.type == FuzzerMemory::eREAD || entry.type == FuzzerMemory::eADDITIONAL_READ ? "r" : "w", entry.addr, entry.data});
}

inline void from_json(const json& j, FuzzerMemory::BusCycle& entry) {
    std::string type;
    j.at(0).get_to(type);
    j.at(1).get_to(entry.addr);
    if(type != "n") {
        j.at(2).get_to(entry.data);
        entry.type = (type == "r" ? FuzzerMemory::eREAD : FuzzerMemory::eWRITE);
    }
    else {
        entry.data = 0;
        entry.type = FuzzerMemory::eNONE;
    }
}

}
