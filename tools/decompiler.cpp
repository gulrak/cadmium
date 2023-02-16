//---------------------------------------------------------------------------------------
// src/decompiler.cpp
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
#include <ghc/cli.hpp>
#include <sha1/sha1.hpp>
#include <emulation/chip8decompiler.hpp>
#include <emulation/chip8compiler.hpp>
#include <emulation/utility.hpp>
#include <filesystem.hpp>

#include <chrono>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

enum WorkMode { eDISASSEMBLE, eANALYSE, eSEARCH };
static std::unordered_map<std::string, std::string> fileMap;
static std::vector<std::string> opcodesToFind;
static bool fullPath = false;
static bool withUsage = false;
static int foundFiles = 0;
static bool roundTrip = false;
static int errors = 0;

std::string fileOrPath(const std::string& file)
{
    return fullPath ? file : fs::path(file).filename().string();
}

void workFile(WorkMode mode, const std::string& file, const std::vector<uint8_t>& data)
{
    if(data.empty())
        return;

    uint16_t startAddress = endsWith(file, ".c8x") ? 0x300 : 0x200;
    emu::Chip8Decompiler dec;
    switch(mode) {
        case eDISASSEMBLE:
            if(roundTrip) {
                std::stringstream os;
                emu::Chip8Compiler comp;
                dec.decompile(file, data.data(), startAddress, data.size(), startAddress, &os, false, true);
                if(comp.compile(os.str())) {
                    if(comp.codeSize() != data.size()) {
                        std::cerr << "    " << fileOrPath(file) << ": Compiled size doesn't match! (" << data.size() << " bytes)" << std::endl;
                        workFile(eANALYSE, file, data);
                        writeFile(fs::path(file).filename().string().c_str(), comp.code(), comp.codeSize());
                        ++errors;
                    }
                    else if(comp.sha1Hex() != calculateSha1Hex(data.data(), data.size())) {
                        std::cerr << "    " << fileOrPath(file) << ": Compiled code doesn't match! (" << data.size() << " bytes)" << std::endl;
                        workFile(eANALYSE, file, data);
                        writeFile(fs::path(file).filename().string().c_str(), comp.code(), comp.codeSize());
                        ++errors;
                    }
                }
                else {
                    std::cerr << "    " << fileOrPath(file) << ": Source doesn't compile: " << comp.errorMessage() << std::endl;
                    workFile(eANALYSE, file, data);
                    ++errors;
                }
            }
            else {
                dec.decompile(file, data.data(), startAddress, data.size(), startAddress, &std::cout);
            }
            break;
        case eANALYSE:
            std::cout << fileOrPath(file);
            dec.decompile(file, data.data(), startAddress, data.size(), startAddress, &std::cout, true);
            if((uint64_t)dec.possibleVariants) {
                auto mask = static_cast<uint64_t>(dec.possibleVariants & (emu::C8V::CHIP_8|emu::C8V::CHIP_10|emu::C8V::CHIP_48|emu::C8V::SCHIP_1_0|emu::C8V::SCHIP_1_1|emu::C8V::MEGA_CHIP|emu::C8V::XO_CHIP));
                bool first = true;
                while(mask) {
                    auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
                    mask &= mask - 1;
                    std::cout << (first ? "    possible variants: " : ", ") << dec.chipVariantName(cv).first;
                    first = false;
                }
                std::cout << std::endl;
            }
            else {
                std::clog << "    Doesn't seem to be supported by any know variant." << std::endl;
            }
            if(dec._oddPcAccess)
                std::clog << "    Uses odd PC access." << std::endl;
            break;
        case eSEARCH: {
            dec.decompile(file, data.data(), startAddress, data.size(), startAddress, &std::cout, true, true);
            bool found = false;
            for (const auto& pattern : opcodesToFind) {
                for (const auto& [opcode, count] : dec.fullStats) {
                    std::string hex = fmt::format("{:04X}", opcode);
                    if (comparePattern(pattern, hex)) {
                        if(withUsage) {
                            if(!found)
                                std::cout << fileOrPath(file) << ":" << std::endl;
                            dec.listUsages(opcodeFromPattern(pattern), maskFromPattern(pattern), std::cout);
                            found = true;
                        }
                        else {
                            if (found)
                                std::cout << ", ";
                            std::cout << pattern;
                            found = true;
                            break;
                        }
                    }
                }
            }
            if(found) {
                ++foundFiles;
                if (!withUsage)
                    std::cout << ": " << fileOrPath(file) << std::endl;
            }
            break;
        }
    }
}

std::pair<bool, std::string> checkDouble(const std::string& file, const std::vector<uint8_t>& data)
{
    char hex[SHA1_HEX_SIZE];
    sha1 sum;
    sum.add(data.data(), data.size());
    sum.finalize();
    sum.print_hex(hex);
    std::string digest = hex;
    auto iter = fileMap.find(digest);
    if(iter == fileMap.end()) {
        fileMap[digest] = file;
        return {false,""};
    }
    return {true, iter->second};
}

bool isChipRom(const std::string& name)
{
    return endsWith(name, ".ch8") || endsWith(name, ".c8x") || endsWith(name, ".ch10") || endsWith(name, ".sc8") || endsWith(name, ".xo8") || endsWith(name, ".mc8");
}

int main(int argc, char* argv[])
{
    ghc::CLI cli(argc, argv);
    bool scan = false;
    bool dumpDoubles = false;
    std::vector<std::string> inputList;

    cli.option({"-s", "--scan"}, scan, "scan files or directories for chip roms and analyze them, giving some information");
    cli.option({"-f", "--find"}, opcodesToFind, "search for use of opcodes");
    cli.option({"-u", "--opcode-use"}, withUsage, "show usage of found opcodes");
    cli.option({"-p", "--full-path"}, fullPath, "print file names with path");
    cli.option({"--list-duplicates"}, dumpDoubles, "show found duplicates while scanning directories");
    cli.option({"--round-trip"}, roundTrip, "decompile and assemble and compare the result");
    cli.positional(inputList, "Files or directories to work on");
    cli.parse();

    auto start = std::chrono::steady_clock::now();
    uint64_t files = 0;
    uint64_t doubles = 0;
    WorkMode mode = (opcodesToFind.empty() & !scan) ? eDISASSEMBLE : scan ? eANALYSE : eSEARCH;
    for(const auto& input : inputList) {
        if(!fs::exists(input))
            continue;
        if(fs::is_directory(input)) {
            for(const auto& de : fs::recursive_directory_iterator(input, fs::directory_options::skip_permission_denied)) {
                if(de.is_regular_file() && isChipRom(de.path().extension().string())) {
                    auto file = loadFile(de.path().string());
                    auto [isDouble, firstName] = checkDouble(de.path().string(), file);
                    if(isDouble) {
                        ++doubles;
                        if(dumpDoubles)
                            std::clog << "File '" << de.path().string() << "' is identical to '" << firstName << "'" << std::endl;
                    }
                    else {
                        ++files;
                        workFile(mode, de.path().string(), file);
                    }
                }
            }
        }
        else if(fs::is_regular_file(input) && isChipRom(input)) {
            auto file = loadFile(input);
            auto [isDouble, firstName] = checkDouble(input, file);
            if(isDouble) {
                ++doubles;
                if(dumpDoubles)
                    std::clog << "File '" << input << "' is identical to '" << firstName << "'" << std::endl;
            }
            else {
                ++files;
                workFile(mode, input, file);
            }
        }
    }
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    if(scan) {
        std::clog << "Used opcodes:" << std::endl;
        for(const auto& [opcode, num] : emu::Chip8Decompiler::totalStats) {
            std::clog << fmt::format("{:04X}: {}", opcode, num) << std::endl;
        }
    }
    std::clog << "Done scanning/decompiling " << files << " files";
    if(doubles)
        std::clog << ", not counting " << doubles << " redundant copies";
    if(foundFiles)
        std::clog << ", found opcodes in " << foundFiles << " files";
    if(errors)
        std::clog << ", round trip errors: " << errors;
    std::clog << " (" << duration << "ms)" <<std::endl;
    return 0;
}
