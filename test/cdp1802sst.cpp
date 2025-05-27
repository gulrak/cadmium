//
// Created by Steffen Schümann on 22.05.25.
//
#include <emulation/hardware/cdp1802.hpp>
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

#ifdef CDP1802_EMMA_CORE
#include "cores/cdp1802/emma/emma_cdp1802.hpp"
#endif

using json = nlohmann::ordered_json;

namespace emu {

struct FuzzState
{
    explicit FuzzState(uint8_t opcode)
        : refMemory(opcode)
        , testMemory(opcode)
    {}
    explicit FuzzState(const json& test);
    void reset()
    {
        refMemory.reset();
        testMemory.reset();
    }
    std::string name;
    Cdp1802State initialState;
    Cdp1802State finalState;
    fuzz::FuzzerMemory refMemory;
    fuzz::FuzzerMemory testMemory;
};


void to_json(json& j, const FuzzState& state) {

    j["name"] = state.name;
    json::object_t is;
    is["r"] = state.initialState.r;
    is["p"] = state.initialState.p;
    is["x"] = state.initialState.x;
    is["n"] = state.initialState.n;
    is["i"] = state.initialState.i;
    is["t"] = state.initialState.t;
    is["d"] = state.initialState.d;
    is["df"] = state.initialState.df ? 1 : 0;
    is["ie"] = state.initialState.ie ? 1 : 0;
    is["q"] = state.initialState.q ? 1 : 0;
    is["ram"] = state.refMemory.initialRam;
    j["initial"] = is;
    json::object_t fs;
    fs["r"] = state.finalState.r;
    fs["p"] = state.finalState.p;
    fs["x"] = state.finalState.x;
    fs["n"] = state.finalState.n;
    fs["i"] = state.finalState.i;
    fs["t"] = state.finalState.t;
    fs["d"] = state.finalState.d;
    fs["df"] = state.finalState.df ? 1 : 0;
    fs["ie"] = state.finalState.ie ? 1 : 0;
    fs["q"] = state.finalState.q ? 1 : 0;
    fs["ram"] = state.refMemory.currentRam;
    j["final"] = fs;
    j["cycles"] = state.refMemory.cycles;
}

void from_json(const json& j, FuzzState& state)
{
    state.reset();
    j.at("name").get_to(state.name);
    auto is = j.at("initial");
    is.at("r").get_to(state.initialState.r);
    state.initialState.p = is.value("p", 0) & 0xF;
    state.initialState.x = is.value("x", 0) & 0xF;
    state.initialState.n = is.value("n", 0) & 0xF;
    state.initialState.i = is.value("i", 0) & 0xF;
    is.at("t").get_to(state.initialState.t);
    is.at("d").get_to(state.initialState.d);
    state.initialState.df = is.at("df").get<int>() != 0;
    state.initialState.ie = is.at("ie").get<int>() != 0;
    state.initialState.q = is.at("q").get<int>() != 0;
    is.at("ram").get_to(state.refMemory.initialRam);
    auto fs = j.at("final");
    fs.at("r").get_to(state.finalState.r);
    state.finalState.p = fs.value("p", 0) & 0xF;
    state.finalState.x = fs.value("x", 0) & 0xF;
    state.finalState.n = fs.value("n", 0) & 0xF;
    state.finalState.i = fs.value("i", 0) & 0xF;
    fs.at("t").get_to(state.finalState.t);
    fs.at("d").get_to(state.finalState.d);
    state.finalState.df = fs.at("df").get<int>() != 0;
    state.finalState.ie = fs.at("ie").get<int>() != 0;
    state.finalState.q = fs.at("q").get<int>() != 0;
    fs.at("ram").get_to(state.refMemory.currentRam);
    state.refMemory.cycles.clear();
    state.refMemory.cycles.reserve(j.at("cycles").size());
    j.at("cycles").get_to(state.refMemory.cycles);
    state.finalState.cycles = state.refMemory.cycles.size();
    //state.finalState.instruction = 1;
}

FuzzState::FuzzState(const json& test)
    : refMemory(0)
    , testMemory(0)
{
    from_json(test, *this);
}


template<class CpuRef, class CpuTest, int MaxInstructionLength = 3>
class Cdp1802Fuzzer : public Cdp1802Bus
{
public:
    explicit Cdp1802Fuzzer(uint8_t opcode, fuzz::FuzzerMemory::CompareType strictness = fuzz::FuzzerMemory::eMEMONLY)
        : _cpuRef(*this)
        , _cpuTest(*this)
        , _strictness(strictness)
        , _state(opcode)
        , _os(fmt::format("{:02x}.json", opcode))
    {
    }
    explicit Cdp1802Fuzzer(const json& test, fuzz::FuzzerMemory::CompareType strictness = fuzz::FuzzerMemory::eMEMONLY)
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
            //if(verify && !std::is_same_v<CpuTest,emu::M6800Mock>)
            //    testStep();
            if(!verify) {
                exportTestCase();
            }
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
        _opcode = _state.refMemory.readByte(_state.initialState.r[_state.initialState.p], true);
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
        std::generate(std::begin(_state.initialState.r), std::end(_state.initialState.r),  []() { return rndWord(); });
        _state.initialState.p = rndByte() & 0xF;
        _state.initialState.x = rndByte() & 0xF;
        _state.initialState.n = rndByte() & 0xF;
        _state.initialState.i = rndByte() & 0xF;
        _state.initialState.t = rndByte();
        _state.initialState.d = rndByte();
        _state.initialState.df = (rndByte() & 0x1) != 0;
        _state.initialState.ie = (rndByte() & 0x1) != 0;
        _state.initialState.q = (rndByte() & 0x1) != 0;
        _cpuRef.setState(_state.initialState);
        _mode = eGENERATE;
        _cpuRef.executeInstruction();
        auto cycles = _cpuRef.cycles()>>3;
        while (cycles > _state.refMemory.cycles.size()) {
            _state.refMemory.cycles.push_back({0,0,fuzz::FuzzerMemory::AccessType::eNONE_PASSIVE});
        }
        _state.name = generateName();
        _cpuRef.getState(_state.finalState);
    }
    std::string generateName()
    {
        _counter++;
        uint8_t code[3];
        code[0] = _state.refMemory.cycles[0].data;
        auto disassembled = _cpuRef.disassembleInstruction(code, code+3);
        if (disassembled.size > 1) code[1] = _state.refMemory.cycles[1].data;
        if (disassembled.size > 2) code[2] = _state.refMemory.cycles[2].data;
        switch (disassembled.size) {
            case 2: return fmt::format("{:02X} {:02X}", code[0], code[1]);
            case 3: return fmt::format("{:02X} {:02X} {:02X}", code[0], code[1], code[2]);
            default: return fmt::format("{:02X} {}", code[0], _counter);
        }
    }
    void testStep()
    {
        _mode = eTEST;
        _cycle = 0;
        _state.testMemory.prepare(_state.refMemory);
        _currentMemory = &_state.testMemory;
        _cpuTest.setState(_state.initialState);
        /*if(_state.name.empty()) {
            _mode = eDISASSEMBLE;
            _state.name = trim(_cpuTest.disassembleInstructionWithBytes(-1, nullptr));
            _mode = eTEST;
        }*/
        _cpuTest.executeInstruction();
        Cdp1802State state;
        _cpuTest.getState(state);
        auto cycles = _state.refMemory.cycles.size();
        while (cycles > _state.testMemory.cycles.size()) {
            _state.testMemory.cycles.push_back({0,0,fuzz::FuzzerMemory::AccessType::eNONE_PASSIVE});
        }
        state.cycles = _state.testMemory.cycles.size();
        if(!(_state.finalState == state)) {
            throw fuzz::FuzzerException(fmt::format("States don't match:\nRef: {}\nTst: {}", _state.finalState.toString(), state.toString()));
        }
        if(!_state.testMemory.compareToReference(_state.refMemory, _strictness)) {
            throw fuzz::FuzzerException("Memory doesn't match!");
        }
        if (_state.refMemory.cycles != _state.testMemory.cycles) {
            throw fuzz::FuzzerException("Cycles don't match!");
        }
    }
    void exportTestCase()
    {
        std::ostringstream sstr;
        auto j = json(_state);
        sstr << j;
        if (_os.is_open()) {
            _os << sstr.str() << std::endl;
        }
        else {
            auto hash = fmt::format("{:016x}",fnv1a64(sstr.str())).substr(0,10);
            std::ofstream os(fmt::format("cdp1802_test_{}.json", hash));
            os << "[{" << std::endl;
            os << "  \"name\":    " << j.at("name") << "," << std::endl;
            os << "  \"initial\": " << j.at("initial") << "," << std::endl;
            os << "  \"final\":   " << j.at("final") << "," << std::endl;
            os << "  \"cycles\":  " << j.at("cycles") << std::endl;
            os << "}]";
        }
    }
    uint8_t readByte(uint16_t addr) const override
    {
        return _currentMemory ? _currentMemory->readByte(addr) : 0;
    }
    uint8_t readByteDMA(uint16_t addr) const override
    {
        return _currentMemory ? _currentMemory->readByte(addr, true) : 0;
    }
    void writeByte(uint16_t addr, uint8_t data) override
    {
        if(_currentMemory)
            _currentMemory->writeByte(addr, data);
    }
    void passiveCycle() override
    {
        if (_currentMemory)
            _currentMemory->cycles.push_back({fuzz::FuzzerMemory::AccessType::eNONE_PASSIVE});
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
    std::ofstream _os;
    mutable int _cycle{};
    mutable uint16_t _counter{};
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
    emu::Cdp1802Fuzzer<RefCore, TestCore> fuzzer(opcode, strictness);
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

template<typename RefCore, typename TestCore>
void runTests(std::string testFile, fuzz::FuzzerMemory::CompareType strictMode = fuzz::FuzzerMemory::eMEMONLY)
{
    std::ifstream f(testFile);
    if(!f) {
        std::cerr << "Couldn't read test file: '" << testFile << "'" << std::endl;
        exit(1);
    }
    auto data = json::parse(f);
    uint64_t count = 0;
    std::cout << "Running " << testFile << "... "; std::cout.flush();
    auto start = std::chrono::steady_clock::now();
    for(const auto& element : data) {
        emu::Cdp1802Fuzzer<RefCore, TestCore> fuzzer(element, strictMode);
        if (fuzzer.execute()) {
            std::cerr << "\nStopped on error." << std::endl;
            std::clog << count << " tests run." << std::endl;
            return;
        }
        g_testCaseCount++;
        count++;
    }
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    std::cout << fmt::format("executed {} test cases successfully. [{:.2f}s]", count, (double)duration / 1000.0) << std::endl;
}

void runTests(const std::string& refCoreName, const std::string& tstCoreName, std::string testFile, fuzz::FuzzerMemory::CompareType strictMode = fuzz::FuzzerMemory::eMEMONLY)
{
    if (refCoreName == "cadmium") {
        if (tstCoreName == "cadmium") {
            runTests<emu::Cdp1802, emu::Cdp1802>(testFile, strictMode);
        }
#ifdef CDP1802_EMMA_CORE
        else if (tstCoreName == "emma") {
            runTests<emu::Cdp1802, emu::EmmaCdp1802>(testFile, strictMode);
        }
#endif
    }
#ifdef CDP1802_EMMA_CORE
    else if (refCoreName == "emma") {
        if (tstCoreName == "cadmium") {
            runTests<emu::EmmaCdp1802, emu::Cdp1802>(testFile, strictMode);
        }
        else if (tstCoreName == "emma") {
            runTests<emu::EmmaCdp1802, emu::EmmaCdp1802>(testFile, strictMode);
        }
    }
#endif
}

void runTestsFromDirectory(const std::string& refCoreName, const std::string& tstCoreName, std::string testDirectory, uint64_t numRounds, fuzz::FuzzerMemory::CompareType strictMode = fuzz::FuzzerMemory::eMEMONLY)
{
    for(const auto& entry : std::filesystem::directory_iterator(testDirectory)) {
        if(entry.is_regular_file()) {
            std::string testFile = entry.path().string();
            if(testFile.substr(testFile.size()-5) == ".json") {
                runTests(refCoreName, tstCoreName, testFile, strictMode);
            }
        }
    }
}

int main(int argc, char* argv[])
{
    ghc::CLI cli(argc, argv);
    std::string testFile;
    std::string outputDir;
    std::string refCoreName;
    std::string tstCoreName;
    std::string strictness;
    fuzz::FuzzerMemory::CompareType strictMode{fuzz::FuzzerMemory::eALLCYCLES};
    int64_t rounds = 10000;
    int64_t opcodeToTest = -1;
    bool listCores = false;
    bool version = false;
    bool testRef = false;

    cli.option({"-V", "--version"}, version, "display program version");
    cli.option({"-n", "--rounds"}, rounds, "rounds per opcode");
    cli.option({"-o", "--output-dir"}, outputDir, "export test cases to output dir");
    cli.option({"-t", "--test-file"}, testFile, "load JSON test file and run tests, can be a directory for all tests");
    cli.option({"-l", "--list-cores"}, listCores, "list embedded test cores and exit");
    cli.option({"--test-reference"}, testRef, "run tests against the reference core");
    cli.option({"--strictness"}, strictness, "validating strictness besides cycles/state (memonly, writes, full)");
    cli.option({"--opcode"}, opcodeToTest, "generate and test given opcode (default: all)");
    cli.option({"-r", "--reference"}, refCoreName, "reference core to use (cadmium (default) or emma)");
    cli.option({"-c", "--check"}, tstCoreName, "core to check (cadmium or emma (default))");
    cli.parse();

    if(version) {
        std::cout << "CDP1802SST v" << CDP1802SST_VERSION << " [" <<  CDP1802SST_GIT_HASH << "]" << std::endl;
        std::cout << "(C) 2024 by Steffen 'Gulrak' Schümann" << std::endl;
        exit(0);
    }
    if(listCores) {
        std::cout << "Available CDP1802 test cores:" << std::endl;
        std::cout << "    cadmium (default core)" << std::endl;
#ifdef CDP1802_EMMA_CORE
        std::cout << "    emma" << std::endl;
#endif
        exit(0);
    }

#ifndef CDP1802_EMMA_CORE
    if (refCoreName == "emma" || tstCoreName == "emma") {
        std::cerr << "This build has no integrated emma core, sorry." << std::endl;
        exit(0);
    }
#endif

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
        if (refCoreName.empty())
            refCoreName = "cadmium";
        if (tstCoreName.empty())
            tstCoreName = "cadmium";

        std::cout << "Running tests on core: " << tstCoreName << "..." << std::endl;
        auto start = std::chrono::steady_clock::now();
        if (std::filesystem::is_directory(testFile)) {
            runTestsFromDirectory(refCoreName, tstCoreName, testFile, strictMode);
        }
        else {
            runTests(refCoreName, tstCoreName, testFile, strictMode);
        }
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::cout << fmt::format("Executed {} test cases successfully. [{:.2f}s]", g_testCaseCount, (double)duration / 1000.0) << std::endl;
    }
    else {
        std::clog << "Running opcode fuzzing tests, " << rounds << " fuzzed tests each, skipping invalid opcodes..." << std::endl;
        uint8_t firstOpcode = opcodeToTest >= 0 ? opcodeToTest : 0;
        uint8_t lastOpcode = opcodeToTest >= 0 ? opcodeToTest : 255;
        auto start = std::chrono::steady_clock::now();
        if(outputDir == "-")
            std::cout << "[" << std::endl;
        for (uint16_t opcode = firstOpcode; opcode <= lastOpcode; ++opcode) {
            std::clog << fmt::format("    Opcode: {:02x}\r", opcode);
            std::clog.flush();
            if (opcode != 0 && opcode != 0x68) {
                if (testRef) {
                    testOpcode<emu::Cdp1802, emu::Cdp1802>(opcode, rounds, strictMode, outputDir);
                }
                else {
                    //testOpcode<emu::M6800<>, M6800TestCore>(opcode, rounds, strictMode, outputDir);
                }
            }
        }
        if(outputDir == "-")
            std::cout << "]" << std::endl;
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::clog << fmt::format("\n{} tests run, no errors. [{:.2f}s]", g_testCaseCount, (double)duration / 1000.0) << std::endl;
    }

}