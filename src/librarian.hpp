//---------------------------------------------------------------------------------------
// src/librarian.hpp
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

#include <emulation/chip8options.hpp>
#include <emulation/chip8variants.hpp>

#include <chrono>
#include <string>
#include <vector>

class Librarian
{
public:
    struct Info
    {
        enum Type { eDIRECTORY, eUNKNOWN_FILE, eROM_FILE, eOCTO_SOURCE };
        std::string filePath;
        Type type;
        emu::Chip8EmulatorOptions::SupportedPreset variant;
        size_t fileSize;
        std::chrono::system_clock::time_point changeDate;
        //------ available after analyzed == true ------------
        bool analyzed{false};
        emu::Chip8Variant possibleVariants{};
        std::string minimumOpcodeProfile() const;
    };
    Librarian();
    std::string currentDirectory() const { return _currentPath; }
    std::string fullPath(std::string file) const;
    bool fetchDir(std::string directory);
    bool intoDir(std::string subDirectory);
    bool parentDir();
    bool update(const emu::Chip8EmulatorOptions& options);

    size_t numEntries() const { return _directoryEntries.size(); }
    const Info& getInfo(size_t index) { return _directoryEntries[index]; }
    void select(int index) { _activeEntry = index; }
    int getSelectedIndex() const { return _activeEntry; }
    static emu::Chip8EmulatorOptions::SupportedPreset getPresetForFile(std::string sha1sum);
    static emu::Chip8EmulatorOptions::SupportedPreset getPresetForFile(const uint8_t* data, size_t size);
private:
    int _activeEntry{-1};
    std::string _currentPath;
    std::vector<Info> _directoryEntries;
    bool _analyzing{false};
};
