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

//#include <emulation/chip8options.hpp>
#include <emulation/properties.hpp>
#include <chiplet/chip8variants.hpp>
#include <sha1/sha1.hpp>
#include <configuration.hpp>

#include <chrono>
#include <mutex>
#include <string>
#include <vector>

struct KnownRomInfo {
    Sha1::Digest sha1;
    const char* preset;
    const char* name;
    const char* options;
    const char* url;
};

class Librarian
{
public:
    static constexpr size_t MAX_ROM_SIZE = 16 * 1024 * 1024 - 512;
    struct Info
    {
        enum Type { eDIRECTORY, eUNKNOWN_FILE, eROM_FILE, eOCTO_SOURCE };
        std::string filePath;
        Type type;
        std::string variant;
        size_t fileSize;
        std::chrono::system_clock::time_point changeDate;
        //------ available after analyzed == true ------------
        bool analyzed{false};
        bool isKnown{false};
        Sha1::Digest sha1sum;
        emu::chip8::VariantSet possibleVariants{};
        std::string minimumOpcodeProfile() const;
        //emu::Chip8EmulatorOptions::SupportedPreset minimumOpcodePreset() const;
    };
    struct Screenshot
    {
        int width{0};
        int height{0};
        int pixelAspect{1};
        std::vector<uint32_t> pixel;
    };
    explicit Librarian(const CadmiumConfiguration& cfg);
    std::string currentDirectory() const { return _currentPath; }
    std::string fullPath(std::string file) const;
    bool fetchDir(std::string directory);
    bool intoDir(std::string subDirectory);
    bool parentDir();
    bool update(const emu::Properties& properties);

    size_t numEntries() const { return _directoryEntries.size(); }
    const Info& getInfo(size_t index) { return _directoryEntries[index]; }
    void select(int index) { _activeEntry = index; }
    int getSelectedIndex() const { return _activeEntry; }
    bool isKnownFile(std::span<const uint8_t> data) const;
    bool isKnownFile(const Sha1::Digest& sha1) const;
    bool isGenericChip8(std::span<const uint8_t> data) const;
    bool isGenericChip8(const Sha1::Digest& sha1) const;
    std::string getPresetForFile(const Sha1::Digest& sha1) const;
    std::string getPresetForFile(std::span<const uint8_t> data) const;
    std::string getEstimatedPresetForFile(std::string_view filename, std::string_view currentPreset, std::span<const uint8_t> data) const;
    emu::Properties getPropertiesForFile(std::span<const uint8_t> data) const;
    emu::Properties getPropertiesForFile(const Sha1::Digest& sha1) const;
    static emu::Properties getPropertiesForSha1(const Sha1::Digest& sha1);
    Screenshot genScreenshot(const Info& info, const std::array<uint32_t, 256> palette) const;
    static bool isPrefixedTPDRom(std::span<const uint8_t> data);
    static std::mutex& decompilerMutex();
    static bool isPrefixedRSTDPRom(std::span<const uint8_t> dat);
    static size_t numKnownRoms();
    static const KnownRomInfo& getRomInfo(size_t index);
    static const KnownRomInfo* getKnownRoms();
    static const KnownRomInfo* findKnownRom(const Sha1::Digest& sha1);
    static size_t findKnownRoms(const Sha1::Digest& sha1, std::vector<const KnownRomInfo*>& outKnownRoms);
private:
    int _activeEntry{-1};
    std::string _currentPath;
    std::vector<Info> _directoryEntries;
    const CadmiumConfiguration& _cfg;
    bool _analyzing{false};
};
