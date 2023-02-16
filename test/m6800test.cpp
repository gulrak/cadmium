//
// Created by Steffen Schümann on 15.01.23.
//
#include <emulation/hardware/m6800.hpp>

#include "m6800mock.hpp"
#ifdef M6800_EXTERN_CORE
    #include "cores/m6800/exorsim/exorsimcore.hpp"
    #include "cores/m6800/bikenomad/bikenomadcore.hpp"
    using M6800TestCore = emu::ExorSimCore;
    #define M6800_EXTERN_CORE_NAME "ExorSim"
#else
    using M6800TestCore = emu::M6800Mock;
#endif

#include "fuzzer.hpp"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <random>
#include <stdexcept>

#include <nlohmann/json.hpp>
#include <ghc/cli.hpp>

using json = nlohmann::ordered_json;

static const uint8_t dream6800Rom[] = {
    0x8d, 0x77, 0xce, 0x02, 0x00, 0xdf, 0x22, 0xce, 0x00, 0x5f, 0xdf, 0x24, 0xde, 0x22, 0xee, 0x00, 0xdf, 0x28, 0xdf, 0x14, 0xbd, 0xc0, 0xd0, 0x96, 0x14, 0x84, 0x0f, 0x97, 0x14, 0x8d, 0x21, 0x97, 0x2e, 0xdf, 0x2a, 0x96, 0x29, 0x44, 0x44, 0x44,
    0x44, 0x8d, 0x15, 0x97, 0x2f, 0xce, 0xc0, 0x48, 0x96, 0x28, 0x84, 0xf0, 0x08, 0x08, 0x80, 0x10, 0x24, 0xfa, 0xee, 0x00, 0xad, 0x00, 0x20, 0xcc, 0xce, 0x00, 0x2f, 0x08, 0x4a, 0x2a, 0xfc, 0xa6, 0x00, 0x39, 0xc0, 0x6a, 0xc0, 0xa2, 0xc0, 0xac,
    0xc0, 0xba, 0xc0, 0xc1, 0xc0, 0xc8, 0xc0, 0xee, 0xc0, 0xf2, 0xc0, 0xfe, 0xc0, 0xcc, 0xc0, 0xa7, 0xc0, 0x97, 0xc0, 0xf8, 0xc2, 0x1f, 0xc0, 0xd7, 0xc1, 0x5f, 0xd6, 0x28, 0x26, 0x25, 0x96, 0x29, 0x81, 0xe0, 0x27, 0x05, 0x81, 0xee, 0x27, 0x0e,
    0x39, 0x4f, 0xce, 0x01, 0x00, 0xa7, 0x00, 0x08, 0x8c, 0x02, 0x00, 0x26, 0xf8, 0x39, 0x30, 0x9e, 0x24, 0x32, 0x97, 0x22, 0x32, 0x97, 0x23, 0x9f, 0x24, 0x35, 0x39, 0xde, 0x14, 0x6e, 0x00, 0x96, 0x30, 0x5f, 0x9b, 0x15, 0x97, 0x15, 0xd9, 0x14,
    0xd7, 0x14, 0xde, 0x14, 0xdf, 0x22, 0x39, 0xde, 0x14, 0xdf, 0x26, 0x39, 0x30, 0x9e, 0x24, 0x96, 0x23, 0x36, 0x96, 0x22, 0x36, 0x9f, 0x24, 0x35, 0x20, 0xe8, 0x96, 0x29, 0x91, 0x2e, 0x27, 0x10, 0x39, 0x96, 0x29, 0x91, 0x2e, 0x26, 0x09, 0x39,
    0x96, 0x2f, 0x20, 0xf0, 0x96, 0x2f, 0x20, 0xf3, 0xde, 0x22, 0x08, 0x08, 0xdf, 0x22, 0x39, 0xbd, 0xc2, 0x97, 0x7d, 0x00, 0x18, 0x27, 0x07, 0xc6, 0xa1, 0xd1, 0x29, 0x27, 0xeb, 0x39, 0xc6, 0x9e, 0xd1, 0x29, 0x27, 0xd0, 0x20, 0xd5, 0x96, 0x29,
    0x20, 0x3b, 0x96, 0x29, 0x9b, 0x2e, 0x20, 0x35, 0x8d, 0x38, 0x94, 0x29, 0x20, 0x2f, 0x96, 0x2e, 0xd6, 0x29, 0xc4, 0x0f, 0x26, 0x02, 0x96, 0x2f, 0x5a, 0x26, 0x02, 0x9a, 0x2f, 0x5a, 0x26, 0x02, 0x94, 0x2f, 0x5a, 0x5a, 0x26, 0x0a, 0x7f, 0x00,
    0x3f, 0x9b, 0x2f, 0x24, 0x03, 0x7c, 0x00, 0x3f, 0x5a, 0x26, 0x0a, 0x7f, 0x00, 0x3f, 0x90, 0x2f, 0x25, 0x03, 0x7c, 0x00, 0x3f, 0xde, 0x2a, 0xa7, 0x00, 0x39, 0x86, 0xc0, 0x97, 0x2c, 0x7c, 0x00, 0x2d, 0xde, 0x2c, 0x96, 0x0d, 0xab, 0x00, 0xa8,
    0xff, 0x97, 0x0d, 0x39, 0x07, 0xc1, 0x79, 0x0a, 0xc1, 0x7d, 0x15, 0xc1, 0x82, 0x18, 0xc1, 0x85, 0x1e, 0xc1, 0x89, 0x29, 0xc1, 0x93, 0x33, 0xc1, 0xde, 0x55, 0xc1, 0xfa, 0x65, 0xc2, 0x04, 0xce, 0xc1, 0x44, 0xc6, 0x09, 0xa6, 0x00, 0x91, 0x29,
    0x27, 0x09, 0x08, 0x08, 0x08, 0x5a, 0x26, 0xf4, 0x7e, 0xc3, 0x60, 0xee, 0x01, 0x96, 0x2e, 0x6e, 0x00, 0x96, 0x20, 0x20, 0xb0, 0xbd, 0xc2, 0xc4, 0x20, 0xab, 0x97, 0x20, 0x39, 0x16, 0x7e, 0xc2, 0xe1, 0x5f, 0x9b, 0x27, 0x97, 0x27, 0xd9, 0x26,
    0xd7, 0x26, 0x39, 0xce, 0xc1, 0xbc, 0x84, 0x0f, 0x08, 0x08, 0x4a, 0x2a, 0xfb, 0xee, 0x00, 0xdf, 0x1e, 0xce, 0x00, 0x08, 0xdf, 0x26, 0xc6, 0x05, 0x96, 0x1e, 0x84, 0xe0, 0xa7, 0x04, 0x09, 0x86, 0x03, 0x79, 0x00, 0x1f, 0x79, 0x00, 0x1e, 0x4a,
    0x26, 0xf7, 0x5a, 0x26, 0xeb, 0x39, 0xf6, 0xdf, 0x49, 0x25, 0xf3, 0x9f, 0xe7, 0x9f, 0x3e, 0xd9, 0xe7, 0xcf, 0xf7, 0xcf, 0x24, 0x9f, 0xf7, 0xdf, 0xe7, 0xdf, 0xb7, 0xdf, 0xd7, 0xdd, 0xf2, 0x4f, 0xd6, 0xdd, 0xf3, 0xcf, 0x93, 0x4f, 0xde, 0x26,
    0xc6, 0x64, 0x8d, 0x06, 0xc6, 0x0a, 0x8d, 0x02, 0xc6, 0x01, 0xd7, 0x0e, 0x5f, 0x91, 0x0e, 0x25, 0x05, 0x5c, 0x90, 0x0e, 0x20, 0xf7, 0xe7, 0x00, 0x08, 0x39, 0x0f, 0x9f, 0x12, 0x8e, 0x00, 0x2f, 0xde, 0x26, 0x20, 0x09, 0x0f, 0x9f, 0x12, 0x9e,
    0x26, 0x34, 0xce, 0x00, 0x30, 0xd6, 0x2b, 0xc4, 0x0f, 0x32, 0xa7, 0x00, 0x08, 0x7c, 0x00, 0x27, 0x5a, 0x2a, 0xf6, 0x9e, 0x12, 0x0e, 0x39, 0xd6, 0x29, 0x7f, 0x00, 0x3f, 0xde, 0x26, 0x86, 0x01, 0x97, 0x1c, 0xc4, 0x0f, 0x26, 0x02, 0xc6, 0x10,
    0x37, 0xdf, 0x14, 0xa6, 0x00, 0x97, 0x1e, 0x7f, 0x00, 0x1f, 0xd6, 0x2e, 0xc4, 0x07, 0x27, 0x09, 0x74, 0x00, 0x1e, 0x76, 0x00, 0x1f, 0x5a, 0x26, 0xf5, 0xd6, 0x2e, 0x8d, 0x28, 0x96, 0x1e, 0x8d, 0x15, 0xd6, 0x2e, 0xcb, 0x08, 0x8d, 0x1e, 0x96,
    0x1f, 0x8d, 0x0b, 0x7c, 0x00, 0x2f, 0xde, 0x14, 0x08, 0x33, 0x5a, 0x26, 0xcb, 0x39, 0x16, 0xe8, 0x00, 0xaa, 0x00, 0xe7, 0x00, 0x11, 0x27, 0x04, 0x86, 0x01, 0x97, 0x3f, 0x39, 0x96, 0x2f, 0x84, 0x1f, 0x48, 0x48, 0x48, 0xc4, 0x3f, 0x54, 0x54,
    0x54, 0x1b, 0x97, 0x1d, 0xde, 0x1c, 0x39, 0xc6, 0xf0, 0xce, 0x80, 0x10, 0x6f, 0x01, 0xe7, 0x00, 0xc6, 0x06, 0xe7, 0x01, 0x6f, 0x00, 0x39, 0x8d, 0xee, 0x7f, 0x00, 0x18, 0x8d, 0x55, 0xe6, 0x00, 0x8d, 0x15, 0x97, 0x17, 0xc6, 0x0f, 0x8d, 0xe1,
    0xe6, 0x00, 0x54, 0x54, 0x54, 0x54, 0x8d, 0x07, 0x48, 0x48, 0x9b, 0x17, 0x97, 0x17, 0x39, 0xc1, 0x0f, 0x26, 0x02, 0xd7, 0x18, 0x86, 0xff, 0x4c, 0x54, 0x25, 0xfc, 0x39, 0xdf, 0x12, 0x8d, 0xbf, 0xa6, 0x01, 0x2b, 0x07, 0x48, 0x2a, 0xf9, 0x6d,
    0x00, 0x20, 0x07, 0x8d, 0xc2, 0x7d, 0x00, 0x18, 0x26, 0xec, 0x8d, 0x03, 0xde, 0x12, 0x39, 0xc6, 0x04, 0xd7, 0x21, 0xc6, 0x41, 0xf7, 0x80, 0x12, 0x7d, 0x00, 0x21, 0x26, 0xfb, 0xc6, 0x01, 0xf7, 0x80, 0x12, 0x39, 0x8d, 0x00, 0x37, 0xc6, 0xc8,
    0x5a, 0x01, 0x26, 0xfc, 0x33, 0x39, 0xce, 0x80, 0x12, 0xc6, 0x3b, 0xe7, 0x01, 0xc6, 0x7f, 0xe7, 0x00, 0xa7, 0x01, 0xc6, 0x01, 0xe7, 0x00, 0x39, 0x8d, 0x13, 0xa6, 0x00, 0x2b, 0xfc, 0x8d, 0xdd, 0xc6, 0x09, 0x0d, 0x69, 0x00, 0x46, 0x8d, 0xd3,
    0x5a, 0x26, 0xf7, 0x20, 0x17, 0xdf, 0x12, 0xce, 0x80, 0x12, 0x39, 0x8d, 0xf8, 0x36, 0x6a, 0x00, 0xc6, 0x0a, 0x8d, 0xbf, 0xa7, 0x00, 0x0d, 0x46, 0x5a, 0x26, 0xf7, 0x32, 0xde, 0x12, 0x39, 0x20, 0x83, 0x86, 0x37, 0x8d, 0xb9, 0xde, 0x02, 0x39,
    0x8d, 0xf7, 0xa6, 0x00, 0x8d, 0xdd, 0x08, 0x9c, 0x04, 0x26, 0xf7, 0x20, 0x0b, 0x8d, 0xea, 0x8d, 0xb7, 0xa7, 0x00, 0x08, 0x9c, 0x04, 0x26, 0xf7, 0x8e, 0x00, 0x7f, 0xce, 0xc3, 0xe9, 0xdf, 0x00, 0x86, 0x3f, 0x8d, 0x92, 0x8d, 0x43, 0x0e, 0x8d,
    0xce, 0x4d, 0x2a, 0x10, 0x8d, 0xc9, 0x84, 0x03, 0x27, 0x23, 0x4a, 0x27, 0xd8, 0x4a, 0x27, 0xc8, 0xde, 0x06, 0x6e, 0x00, 0x8d, 0x0c, 0x97, 0x06, 0x8d, 0x06, 0x97, 0x07, 0x8d, 0x23, 0x20, 0xdf, 0x8d, 0xad, 0x48, 0x48, 0x48, 0x48, 0x97, 0x0f,
    0x8d, 0xa5, 0x9b, 0x0f, 0x39, 0x8d, 0x12, 0xde, 0x06, 0x8d, 0x25, 0x8d, 0x9a, 0x4d, 0x2b, 0x04, 0x8d, 0xe8, 0xa7, 0x00, 0x08, 0xdf, 0x06, 0x20, 0xec, 0x86, 0x10, 0x8d, 0x2b, 0xce, 0x01, 0xc8, 0x86, 0xff, 0xbd, 0xc0, 0x7d, 0xce, 0x00, 0x06,
    0x8d, 0x06, 0x08, 0x8d, 0x03, 0x8d, 0x15, 0x39, 0xa6, 0x00, 0x36, 0x44, 0x44, 0x44, 0x44, 0x8d, 0x01, 0x32, 0xdf, 0x12, 0xbd, 0xc1, 0x93, 0xc6, 0x05, 0xbd, 0xc2, 0x24, 0x86, 0x04, 0x9b, 0x2e, 0x97, 0x2e, 0x86, 0x1a, 0x97, 0x2f, 0xde, 0x12,
    0x39, 0x7a, 0x00, 0x20, 0x7a, 0x00, 0x21, 0x7d, 0x80, 0x12, 0x3b, 0xde, 0x00, 0x6e, 0x00, 0x00, 0xc3, 0xf3, 0x00, 0x80, 0x00, 0x83, 0xc3, 0x60
};


std::array<uint8_t, 4096> dreamRAM;

inline std::string trim(std::string s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    std::string::iterator end = std::unique(s.begin(), s.end(), [](char lhs, char rhs){ return (lhs == rhs) && (lhs == ' '); });
    s.erase(end, s.end());
    return s;
}

struct M6k8Bus : public emu::M6800Bus<>
{
    ByteType readByte(WordType addr) const override
    {
        if(!emu::isValidInt(addr))
            return {};
        if(addr < 4096)
            return dreamRAM[addr];
        if(addr >= 0xC000)
            return dream6800Rom[addr & 0x3FF];
        return 0;
    }

    void writeByte(WordType addr, ByteType val) override
    {
        if(emu::isValidInt(addr) && addr < 4096)
            dreamRAM[addr] = emu::asNativeInt(val);
    }
};

namespace emu {

struct FuzzState
{
    FuzzState(uint8_t opcode)
        : refMemory(opcode)
        , testMemory(opcode)
    {}
    FuzzState(const json& test);
    void reset()
    {
        refMemory.reset();
        testMemory.reset();
    }
    std::string name;
    M6800State initialState;
    M6800State finalState;
    fuzz::FuzzerMemory refMemory;
    fuzz::FuzzerMemory testMemory;
};


void to_json(json& j, const FuzzState& state) {

    j["name"] = state.name;
    json::object_t is;
    is["pc"] = state.initialState.pc;
    is["sp"] = state.initialState.sp;
    is["a"] = state.initialState.a;
    is["b"] = state.initialState.b;
    is["x"] = state.initialState.ix;
    is["sr"] = state.initialState.cc;
    is["ram"] = state.refMemory.initialRam;
    j["initial"] = is;
    json::object_t fs;
    fs["pc"] = state.finalState.pc;
    fs["sp"] = state.finalState.sp;
    fs["a"] = state.finalState.a;
    fs["b"] = state.finalState.b;
    fs["x"] = state.finalState.ix;
    fs["sr"] = state.finalState.cc;
    fs["ram"] = state.refMemory.currentRam;
    j["final"] = fs;
    j["cycles"] = state.refMemory.cycles;
}

void from_json(const json& j, FuzzState& state)
{
    state.reset();
    j.at("name").get_to(state.name);
    auto is = j.at("initial");
    is.at("pc").get_to(state.initialState.pc);
    is.at("sp").get_to(state.initialState.sp);
    is.at("a").get_to(state.initialState.a);
    is.at("b").get_to(state.initialState.b);
    is.at("x").get_to(state.initialState.ix);
    is.at("sr").get_to(state.initialState.cc);
    is.at("ram").get_to(state.refMemory.initialRam);
    auto fs = j.at("final");
    fs.at("pc").get_to(state.finalState.pc);
    fs.at("sp").get_to(state.finalState.sp);
    fs.at("a").get_to(state.finalState.a);
    fs.at("b").get_to(state.finalState.b);
    fs.at("x").get_to(state.finalState.ix);
    fs.at("sr").get_to(state.finalState.cc);
    fs.at("ram").get_to(state.refMemory.currentRam);
    j.at("cycles").get_to(state.refMemory.cycles);
    state.finalState.cycles = state.refMemory.cycles.size();
    state.finalState.instruction = 1;
}

FuzzState::FuzzState(const json& test)
    : refMemory(0)
    , testMemory(0)
{
    from_json(test, *this);
}

template<class CpuRef, class CpuTest, int MaxInstructionLength = 3>
class M6800Fuzzer : public emu::M6800Bus<>
{
public:
    M6800Fuzzer(uint8_t opcode, fuzz::FuzzerMemory::CompareType strictness = fuzz::FuzzerMemory::eMEMONLY)
        : _cpuRef(*this)
        , _cpuTest(*this)
        , _strictness(strictness)
        , _state(opcode)
    {
    }
    M6800Fuzzer(const json& test, fuzz::FuzzerMemory::CompareType strictness = fuzz::FuzzerMemory::eMEMONLY)
        : _cpuRef(*this)
        , _cpuTest(*this)
        , _state(test)
    {
    }
    bool execute(uint8_t opcode, bool verify = true)
    {
        _opcode = opcode;
        try {
            reset();
            generateStep();
            if(verify && !std::is_same_v<CpuTest,emu::M6800Mock>)
                testStep();
        }
        catch(fuzz::FuzzerException& fe)
        {
            auto j = json(_state);
            std::cerr << "name:        " << j.at("name") << std::endl;
            std::cerr << "initial:     " << j.at("initial") << std::endl;
            std::cerr << "final:       " << j.at("final") << std::endl;
            std::cerr << "test ram:    " << json(_state.testMemory.currentRam) << std::endl;
            std::cerr << "ref cycles:  " << j.at("cycles") << std::endl;
            std::cerr << "test cycles: " << json(_state.testMemory.cycles) << std::endl;
            std::cerr << fe.what() << std::endl;
            exportTestCase();
            return true;
        }
        return false;
    }
    bool execute()
    {
        _opcode = _state.refMemory.readByte(_state.initialState.pc, true);
        try {
            reset();
            testStep();
        }
        catch(fuzz::FuzzerException& fe)
        {
            auto j = json(_state);
            std::cerr << "name:        " << j.at("name") << std::endl;
            std::cerr << "initial:     " << j.at("initial") << std::endl;
            std::cerr << "final:       " << j.at("final") << std::endl;
            std::cerr << "test ram:    " << json(_state.testMemory.currentRam) << std::endl;
            std::cerr << "ref cycles:  " << j.at("cycles") << std::endl;
            std::cerr << "test cycles: " << json(_state.testMemory.cycles) << std::endl;
            std::cerr << fe.what() << std::endl;
            return true;
        }
        return false;
    }
    const FuzzState& refState() const
    {
        return _state;
    }
private:
    enum BusMode { eRESET, eGENERATE, eTEST, eDISASSEMBLE };
    static uint8_t rndByte() { return _randomByte(_rand); }
    static uint16_t rndWord() { return _randomWord(_rand); }
    void reset()
    {
        _mode = eRESET;
        _cpuRef.reset();
        _cpuTest.reset();
        _cycle = 0;
    }
    void generateStep()
    {
        _mode = eGENERATE;
        _state.reset();
        _currentMemory = &_state.refMemory;
        _cycle = 0;
        _state.initialState = {
            rndByte(), rndByte(), rndWord(), rndWord(), rndWord(), (uint8_t)(rndByte() | 0xC0), 0, 0
        };
        _cpuRef.setState(_state.initialState);
        {
            _mode = eDISASSEMBLE;
            _state.name = trim(_cpuTest.disassembleInstructionWithBytes(-1, nullptr));
            _mode = eGENERATE;
        }
        _cpuRef.executeInstruction();
        _cpuRef.getState(_state.finalState);
    }
    void testStep()
    {
        _mode = eTEST;
        _cycle = 0;
        _state.testMemory.prepare(_state.refMemory);
        _currentMemory = &_state.testMemory;
        _cpuTest.setState(_state.initialState);
        if(_state.name.empty()) {
            _mode = eDISASSEMBLE;
            _state.name = trim(_cpuTest.disassembleInstructionWithBytes(-1, nullptr));
            _mode = eTEST;
        }
        _cpuTest.executeInstruction();
        M6800State state;
        _cpuTest.getState(state);
        if(!(_state.finalState == state)) {
            throw fuzz::FuzzerException(fmt::format("States don't match:\nRef: {}\nTst: {}", _state.finalState.toString(true), state.toString(true)));
        }
        if(!_state.testMemory.compareToReference(_state.refMemory, _strictness)) {
            throw fuzz::FuzzerException("Memory doesn't match!");
        }
    }
    void exportTestCase()
    {
        std::ostringstream sstr;
        auto j = json(_state);
        sstr << j;
        auto hash = fmt::format("{:016x}",fnv1a64(sstr.str())).substr(0,10);
        std::ofstream os(fmt::format("m6800_test_{}.json", hash));
        os << "[{" << std::endl;
        os << "  \"name\":    " << j.at("name") << "," << std::endl;
        os << "  \"initial\": " << j.at("initial") << "," << std::endl;
        os << "  \"final\":   " << j.at("final") << "," << std::endl;
        os << "  \"cycles\":  " << j.at("cycles") << std::endl;
        os << "}]";
    }
    uint8_t readByte(uint16_t addr) const override
    {
        return _currentMemory ? _currentMemory->readByte(addr) : 0;
    }
    void dummyRead(uint16_t addr) const override
    {
        if(_currentMemory)
            _currentMemory->passiveRead(addr);
    }
    uint8_t readDebugByte(uint16_t addr) const override
    {
        return _currentMemory ? _currentMemory->readByte(addr, true) : 0;
    }
    void writeByte(uint16_t addr, uint8_t data) override
    {
        if(_currentMemory)
            _currentMemory->writeByte(addr, data);
    }
    static uint64_t fnv1a64(const uint8_t *data, size_t l)
    {
        uint64_t hash = 0xcbf29ce484222325ULL;
        for (size_t i = 0; i < l; i++) {
            hash ^= data[i];
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }
    static uint64_t fnv1a64(const std::string& str)
    {
        return fnv1a64(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }
    //inline static std::mt19937 _rand{std::random_device()()};
    inline static std::seed_seq _seed{3457,236};
    inline static std::mt19937 _rand{_seed};
    inline static std::uniform_int_distribution<uint8_t> _randomByte{0,0xFF};
    inline static std::uniform_int_distribution<uint16_t> _randomWord{0,0xFFFF};
    inline static uint8_t _opcode{};
    CpuRef _cpuRef;
    CpuTest _cpuTest;
    mutable int _cycle;
    BusMode _mode{eRESET};
    fuzz::FuzzerMemory::CompareType _strictness{fuzz::FuzzerMemory::eMEMONLY};
    mutable FuzzState _state;
    mutable fuzz::FuzzerMemory* _currentMemory{nullptr};
};

}
inline static uint64_t g_testCaseCount{};

template<typename RefCore, typename TestCore>
void testOpcode(uint8_t opcode, uint64_t numRounds = 10000, fuzz::FuzzerMemory::CompareType strictness = fuzz::FuzzerMemory::eMEMONLY, std::string outputDir = "")
{
    //emu::M6800Fuzzer<emu::BikeNomadCore, emu::M6800<>> fuzzer;
    //emu::M6800Fuzzer<emu::M6800<>, M6800TestCore> fuzzer(opcode);
    emu::M6800Fuzzer<RefCore, TestCore> fuzzer(opcode, strictness);
    //emu::M6800Fuzzer<emu::ExorSimCore, emu::M6800<>> fuzzer(opcode);
    uint64_t rounds = 0;
    auto testSet = json::array();
    while(rounds < numRounds) {
        //if(opcode == 0x36)
        //    std::cerr << "BREAK" << std::endl;
        if(fuzzer.execute(opcode)) {
            std::cerr << "Error after " << rounds << " rounds in opcode " << fmt::format("0x{:02X}", opcode) << "." << std::endl;
            std::clog << g_testCaseCount << " tests run." << std::endl;
            exit(1);
        }
        else if(!outputDir.empty()) {
            testSet.push_back(json(fuzzer.refState()));
        }
        ++g_testCaseCount;
        ++rounds;
    }
    if(!outputDir.empty()) {
        std::ostream* pos = &std::cout;
        std::unique_ptr<std::ofstream> ofs;
        if(outputDir != "-") {
            if (!std::filesystem::exists(outputDir)) {
                std::filesystem::create_directories(outputDir);
            }
            ofs = std::make_unique<std::ofstream>(std::filesystem::path(outputDir) / fmt::format("{:02X}.json", opcode));
            pos = ofs.get();
            (*pos) << "[" << std::endl;
        }
        int count = 0;
        for(const auto& test : testSet) {
            count++;
            (*pos) << test << (count == testSet.size() ? "" : ",") << std::endl;
        }
        if(outputDir != "-")
            (*pos) << "]";
    }
}

/*
struct FakeBus : public emu::M6800Bus<emu::SpeculativeM6800::ByteType, emu::SpeculativeM6800::WordType>
{
    ByteType readByte(WordType addr) const override { return ByteType(0); }
    void dummyRead(WordType addr) const override {}
    ByteType readDebugByte(WordType addr) const override { return readByte(addr); }
    void writeByte(WordType addr, ByteType val) override {}
};
*/
int main(int argc, char* argv[])
{
    //FakeBus fb;
    //emu::SpeculativeM6800 scpu(fb);
    ghc::CLI cli(argc, argv);
    std::string testFile;
    std::string outputDir;
    std::string refCoreName;
    std::string tstCoreName;
    std::string strictness;
    fuzz::FuzzerMemory::CompareType strictMode{fuzz::FuzzerMemory::eMEMONLY};
    int64_t rounds = 10000;
    int64_t opcodeToTest = -1;
    bool listCores = false;
    bool version = false;
    bool testRef = false;

    cli.option({"-V", "--version"}, version, "display program version");
    cli.option({"-n", "--rounds"}, rounds, "rounds per opcode");
    cli.option({"-o", "--output-dir"}, outputDir, "export test cases to output dir");
    cli.option({"-t", "--test-file"}, testFile, "load JSON test file and run tests");
    cli.option({"-l", "--list-cores"}, listCores, "list embedded test cores and exit");
    cli.option({"--test-reference"}, testRef, "run tests against the reference core");
    cli.option({"--strictness"}, strictness, "validating strictness besides cycles/state (memonly, writes, full)");
    cli.option({"--opcode"}, opcodeToTest, "generate and test given opcode (default: all)");
    //cli.option({"-r", "--reference"}, refCoreName, "reference core to use (cadmium (default) or exorsim)");
    //cli.option({"-c", "--check"}, refCoreName, "core to check (cadmium or exorsim (default))");
    cli.parse();

    if(version) {
        std::cout << "M6800Test v" << M6800TEST_VERSION << " [" <<  M6800TEST_GIT_HASH << "]" << std::endl;
        std::cout << "(C) 2023 by Steffen 'Gulrak' Schümann" << std::endl;
        exit(0);
    }
    if(listCores) {
        std::cout << "Available M6800 test cores:" << std::endl;
        std::cout << "    CadmiumM6800 (default core)" << std::endl;
#ifdef M6800_EXTERN_CORE
        std::cout << "    " << M6800_EXTERN_CORE_NAME << " (thirdparty)" << std::endl;
#endif
        exit(0);
    }

    if(!strictness.empty()) {
        if(strictness == "memonly")
            strictMode = fuzz::FuzzerMemory::eMEMONLY;
        else if(strictness == "writes")
            strictMode = fuzz::FuzzerMemory::eWRITECYCLES;
        else if(strictness == "full")
            strictMode = fuzz::FuzzerMemory::eALLCYCLES;
        else {
            std::cerr << "Unknown strictness (memonly, writes, full): " << strictness << std::endl;
            exit(1);
        }
    }

    if(!testFile.empty()) {
        std::ifstream f(testFile);
        if(!f) {
            std::cerr << "Couldn't read test file: '" << testFile << "'" << std::endl;
            exit(1);
        }
        auto data = json::parse(f);
        uint64_t count = 0;
        auto start = std::chrono::steady_clock::now();
        for(const auto& element : data) {
            if(testRef || std::is_same_v<M6800TestCore, emu::M6800Mock>) {
                emu::M6800Fuzzer<emu::M6800<>, emu::M6800<>> fuzzer(element, strictMode);
                if (fuzzer.execute()) {
                    std::cerr << "Stopped on error." << std::endl;
                    std::clog << count << " tests run." << std::endl;
                    exit(1);
                }
            }
            else {
                emu::M6800Fuzzer<emu::M6800<>, M6800TestCore> fuzzer(element, strictMode);
                if (fuzzer.execute()) {
                    std::cerr << "Stopped on error." << std::endl;
                    std::clog << count << " tests run." << std::endl;
                    exit(1);
                }
            }
            count++;
        }
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::cout << fmt::format("Executed {} test cases successfully. [{:.2f}s]", count, (double)duration / 1000.0) << std::endl;
    }
    else {
        std::clog << "Running opcode fuzzing tests, 100000 fuzzed tests each, skipping invalid opcodes..." << std::endl;
        uint8_t firstOpcode = opcodeToTest >= 0 ? opcodeToTest : 0;
        uint8_t lastOpcode = opcodeToTest >= 0 ? opcodeToTest : 255;
        auto start = std::chrono::steady_clock::now();
        if(outputDir == "-")
            std::cout << "[" << std::endl;
        for (uint16_t opcode = firstOpcode; opcode <= lastOpcode; ++opcode) {
            std::clog << fmt::format("    Opcode: {:02x}\r", opcode);
            std::clog.flush();
            if (emu::M6800<>::isValidOpcode(opcode)) {
                if (testRef) {
                    testOpcode<emu::M6800<>, emu::M6800<>>(opcode, rounds, strictMode, outputDir);
                }
                else {
                    testOpcode<emu::M6800<>, M6800TestCore>(opcode, rounds, strictMode, outputDir);
                }
            }
        }
        if(outputDir == "-")
            std::cout << "]" << std::endl;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::clog << fmt::format("{} tests run, no errors. [{:.2f}s]", g_testCaseCount, (double)duration / 1000.0) << std::endl;
    }
    exit(0);

#if 0
    M6k8Bus bus;
    emu::ExorSimCore cpu(bus);
    cpu.setMemory(dream6800Rom, 0xC000, sizeof(dream6800Rom));
    cpu.reset();
    emu::M6800<> cpu2(bus);
    emu::M6800State s1, s2;
    cpu2.reset();
    for(int i = 0; i < 2000; ++i) {
        cpu.executeInstruction();
        std::cout << cpu2.executeInstructionTraced() << " | " << cpu.dumpStateLine() << std::endl;
        cpu.getState(s1);
        cpu2.getState(s2);
        if(!(s1 == s2))
            break;
    }
    exit(0);
    const uint8_t* code = dream6800Rom;
    const uint8_t* end = code + sizeof(dream6800Rom);
    uint16_t addr = 0xC000;
    while (code < end) {
        auto [size, instruction] = emu::M6800<uint8_t>::disassembleInstruction(code, end, addr);
        std::cout << fmt::format("{:04X}: ", addr);
        switch(size) {
            case 1: std::cout << fmt::format("{:02X}       ", *code); break;
            case 2: std::cout << fmt::format("{:02X} {:02X}    ", *code, *(code+1)); break;
            case 3: std::cout << fmt::format("{:02X} {:02X} {:02X} ", *code, *(code+1), *(code+2)); break;
        }
        std::cout << instruction << std::endl;
        code += size;
        addr += size;
    }
    return 0;
#endif
}