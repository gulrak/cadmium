//---------------------------------------------------------------------------------------
// src/librarian.cpp
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
#include <emulation/chip8cores.hpp>
#include <emulation/chip8decompiler.hpp>
#include <emulation/utility.hpp>
#include <librarian.hpp>

#include <raylib.h>

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

static std::unique_ptr<emu::IChip8Emulator> minion;

template<typename TP>
inline std::chrono::system_clock::time_point convertClock(TP tp)
{
    using namespace std::chrono;
    return time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
}

std::string Librarian::Info::minimumOpcodeProfile() const
{
    auto mask = static_cast<uint64_t>(possibleVariants);
    if(mask) {
        auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
        return emu::Chip8Decompiler::chipVariantName(cv).first;
    }
    return "unknown";
}

Librarian::Librarian()
{
    fetchDir(".");
}

std::string Librarian::fullPath(std::string file) const
{
    return fs::path(_currentPath) / file;
}

bool Librarian::fetchDir(std::string directory)
{
    std::error_code ec;
    _currentPath = fs::canonical(directory, ec);
    _directoryEntries.clear();
    _activeEntry = -1;
    _analyzing = true;
    try {
        _directoryEntries.push_back({"..", Info::eDIRECTORY, emu::Chip8EmulatorOptions::eCHIP8, 0, {}});
        for(auto& de : fs::directory_iterator(directory)) {
            if(de.is_directory()) {
                _directoryEntries.push_back({de.path().filename(), Info::eDIRECTORY, emu::Chip8EmulatorOptions::eCHIP8, 0, convertClock(de.last_write_time())});
            }
            else if(de.is_regular_file()) {
                auto ext = de.path().extension();
                auto type = Info::eUNKNOWN_FILE;
                auto variant = emu::Chip8EmulatorOptions::eCHIP8;
                if(ext == ".8o") type = Info::eOCTO_SOURCE;
                else if(ext == ".ch8")
                    type = Info::eROM_FILE;
                else if(ext == ".ch10")
                    type = Info::eROM_FILE, variant = emu::Chip8EmulatorOptions::eCHIP10;
                else if(ext == ".sc8")
                    type = Info::eROM_FILE, variant = emu::Chip8EmulatorOptions::eSCHIP11;
                else if(ext == ".xo8")
                    type = Info::eROM_FILE, variant = emu::Chip8EmulatorOptions::eXOCHIP;
                else if(ext == ".c8b")
                    type = Info::eROM_FILE;
                _directoryEntries.push_back({de.path().filename(), type, variant, de.file_size(), convertClock(de.last_write_time())});
            }
        }
        std::sort(_directoryEntries.begin(), _directoryEntries.end(), [](const Info& a, const Info& b){
           if(a.type == Info::eDIRECTORY && b.type != Info::eDIRECTORY) {
               return true;
           }
           else if(a.type != Info::eDIRECTORY && b.type == Info::eDIRECTORY) {
               return false;
           }
           return a.filePath < b.filePath;
        });
    }
    catch(fs::filesystem_error& fe)
    {
        return false;
    }
    return true;
}

bool Librarian::intoDir(std::string subDirectory)
{
    return fetchDir(fs::path(_currentPath) / subDirectory);
}

bool Librarian::parentDir()
{
    return fetchDir(fs::path(_currentPath).parent_path());
}

bool Librarian::update(const emu::Chip8EmulatorOptions& options)
{
    bool foundOne = false;
    if(_analyzing) {
        for (auto& entry : _directoryEntries) {
            if (!entry.analyzed) {
                foundOne = true;
                if(entry.type == Info::eROM_FILE) {
                    if (entry.variant == emu::Chip8EmulatorOptions::eCHIP8) {
                        auto file = loadFile(fs::path(_currentPath) / entry.filePath);
                        emu::Chip8Decompiler dec;
                        uint16_t startAddress = endsWith(entry.filePath, ".c8x") ? 0x300 : 0x200;
                        dec.decompile(entry.filePath, file.data(), startAddress, file.size(), startAddress, nullptr, true, true);
                        entry.possibleVariants = dec.possibleVariants;
                        if ((uint64_t)dec.possibleVariants) {
                            if(dec.supportsVariant(options.presetAsVariant()))
                                entry.variant = options.behaviorBase;
                            else if (dec.supportsVariant(emu::Chip8Variant::XO_CHIP))
                                entry.variant = emu::Chip8EmulatorOptions::eXOCHIP;
                            else if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_1))
                                entry.variant = emu::Chip8EmulatorOptions::eSCHIP11;
                            else if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_0))
                                entry.variant = emu::Chip8EmulatorOptions::eSCHIP10;
                            else if (dec.supportsVariant(emu::Chip8Variant::CHIP_48))
                                entry.variant = emu::Chip8EmulatorOptions::eCHIP48;
                            else if (dec.supportsVariant(emu::Chip8Variant::CHIP_10))
                                entry.variant = emu::Chip8EmulatorOptions::eSCHIP10;
                            else
                                entry.variant = emu::Chip8EmulatorOptions::eCHIP8;
                        }
                        else {
                            entry.type = Info::eUNKNOWN_FILE;
                        }
                        TraceLog(LOG_DEBUG, "analyzed `%s`: %s", entry.filePath.c_str(), emu::Chip8EmulatorOptions::nameOfPreset(entry.variant).c_str());
                    }
                }
                entry.analyzed = true;
            }
        }
        if (!foundOne)
            _analyzing = false;
    }
    return foundOne;
}

