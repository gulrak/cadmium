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
#include <chiplet/chip8decompiler.hpp>
#include <chiplet/utility.hpp>
#include <emuhostex.hpp>
#include <emulation/iemulationcore.hpp>
#include <librarian.hpp>

#include <raylib.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>

#include "emulation/coreregistry.hpp"

static std::unique_ptr<emu::IEmulationCore> minion;

template<typename TP>
inline std::chrono::system_clock::time_point convertClock(TP tp)
{
    using namespace std::chrono;
    return time_point_cast<system_clock::duration>(tp - TP::clock::now() + system_clock::now());
}
/*
static KnownRomInfo2 g_knownRoms2[] = {
    {"004fa49c91fbd387484bda62f843e8c5bd2c53d2"_sha1, "chip-8", "Lainchain (Ashton Harding, 2018)", nullptr, nullptr},
    {"0068ff5421f5d62a1ae1c814c68716ddb65cec5b"_sha1, "xo-chip", "Master B8 (Andrew James, 2021)", nullptr, nullptr},
};
*/
#include "knownfiles.ipp"

static constexpr int g_knownRomNum = sizeof(g_knownRoms) / sizeof(g_knownRoms[0]);

size_t Librarian::numKnownRoms()
{
    return g_knownRomNum;
}

const KnownRomInfo& Librarian::getRomInfo(size_t index)
{
    return index < g_knownRomNum ? g_knownRoms[index] : g_knownRoms[0];
}

const KnownRomInfo* Librarian::getKnownRoms()
{
    return g_knownRoms;
}

const KnownRomInfo* Librarian::findKnownRom(const Sha1::Digest& sha1)
{
#ifdef NEW_ROMLIST_FORMAT
    for(auto & g_knownRom : g_knownRoms) {
        if(sha1 == g_knownRom.sha1) {
            return &g_knownRom;
        }
    }
#else
    for(int i = 0; i < g_knownRomNum; ++i) {
        if(sha1.to_hex() == g_knownRoms[i].sha1) {
            return &g_knownRoms[i];
        }
    }
#endif
    return nullptr;
}

size_t Librarian::findKnownRoms(const Sha1::Digest& sha1, std::vector<const KnownRomInfo*>& outKnownRoms)
{
    outKnownRoms.clear();
    for(auto & g_knownRom : g_knownRoms) {
        if(sha1 == g_knownRom.sha1) {
            outKnownRoms.push_back(&g_knownRom);
        }
        else if(!outKnownRoms.empty()) {
            break;
        }
    }
    return outKnownRoms.size();
}

std::string Librarian::Info::minimumOpcodeProfile() const
{
    auto mask = static_cast<uint64_t>(possibleVariants);
    if(mask) {
        auto cv = static_cast<emu::Chip8Variant>(mask & -mask);
        return emu::Chip8Decompiler::chipVariantName(cv).second;
    }
    return "unknown";
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::Info::minimumOpcodePreset() const
{
    auto mask = static_cast<uint64_t>(possibleVariants);
    if(mask) {
        auto cv = (mask & -mask);
        int cnt = 0;
        while((cv & 1) == 0) {
            cv >>= 1;
            cnt++;
        }
        return emu::Chip8EmulatorOptions::presetForVariant(static_cast<emu::chip8::Variant>(cv + 1));
    }
    return emu::Chip8EmulatorOptions::eCHIP8;
}

Librarian::Librarian(const CadmiumConfiguration& cfg)
: _cfg(cfg)
{
    static bool once = false;
    if(!once) {
        once = true;
        TraceLog(LOG_INFO, "Internal database contains `%d` different program checksums.", g_knownRomNum);
    }
}

std::string Librarian::fullPath(std::string file) const
{
    return (fs::path(_currentPath) / file).string();
}

bool Librarian::fetchDir(std::string directory)
{
    std::error_code ec;
    _currentPath = fs::canonical(directory, ec).string();
    _directoryEntries.clear();
    _activeEntry = -1;
    _analyzing = true;
    try {
        _directoryEntries.push_back({"..", Info::eDIRECTORY, "", 0, {}});
        for(auto& de : fs::directory_iterator(directory)) {
            if(de.is_directory()) {
                _directoryEntries.push_back({de.path().filename().string(), Info::eDIRECTORY, "", 0, convertClock(de.last_write_time())});
            }
            else if(de.is_regular_file()) {
                auto ext = de.path().extension();
                auto type = Info::eUNKNOWN_FILE;
                auto variant = std::string("chip-8");
                if(ext == ".8o")
                    type = Info::eOCTO_SOURCE;
                else {
                    auto preset = emu::CoreRegistry::presetForExtension(ext.string());
                    if(!preset.empty()) {
                        variant = preset;
                        type = Info::eROM_FILE;
                    }
                }
                _directoryEntries.push_back({de.path().filename().string(), type, variant, (size_t)de.file_size(), convertClock(de.last_write_time())});
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
    return fetchDir((fs::path(_currentPath) / subDirectory).string());
}

bool Librarian::parentDir()
{
    return fetchDir(fs::path(_currentPath).parent_path().string());
}

bool Librarian::update(const emu::Properties& properties)
{
    bool foundOne = false;
    if(_analyzing) {
        for (auto& entry : _directoryEntries) {
            if (!entry.analyzed) {
                foundOne = true;
                if(entry.type == Info::eROM_FILE && entry.fileSize < 1024 * 1024 * 16) {
                    if (entry.variant == "chip-8") {
                        auto file = loadFile((fs::path(_currentPath) / entry.filePath).string());
                        entry.isKnown = isKnownFile(file.data(), file.size());
                        entry.sha1sum = calculateSha1(file.data(), file.size());
                        if(entry.isKnown) {
                            entry.variant = getPresetForFile(entry.sha1sum);
                        }
                        else {
                            uint16_t startAddress = endsWith(entry.filePath, ".c8x") ? 0x300 : 0x200;
                            emu::Chip8Decompiler dec{file, startAddress};
                            dec.decompile(entry.filePath, startAddress, nullptr, true, true);
                            entry.possibleVariants = dec.possibleVariants();
                            if ((uint64_t)dec.possibleVariants()) {
                                /* TODO:
                                if (dec.supportsVariant(options.presetAsVariant()))
                                    entry.variant = options.behaviorBase;
                                else */
                                if (dec.supportsVariant(emu::Chip8Variant::XO_CHIP))
                                    entry.variant = "xo-chip";
                                else if (dec.supportsVariant(emu::Chip8Variant::MEGA_CHIP))
                                    entry.variant = "megachip";
                                else if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_1))
                                    entry.variant = "schip-1.1";
                                else if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_0))
                                    entry.variant = "schip-1.0";
                                else if (dec.supportsVariant(emu::Chip8Variant::CHIP_48))
                                    entry.variant = "chip-48";
                                else if (dec.supportsVariant(emu::Chip8Variant::CHIP_10))
                                    entry.variant = "chip-10";
                                else
                                    entry.variant = "chip-8";
                            }
                            else {
                                entry.type = Info::eUNKNOWN_FILE;
                            }
                            TraceLog(LOG_DEBUG, "analyzed `%s`: %s", entry.filePath.c_str(), entry.variant.c_str());
                        }
                    }
                    else {
                        auto file = loadFile((fs::path(_currentPath) / entry.filePath).string());
                        entry.isKnown = isKnownFile(file.data(), file.size());
                        entry.sha1sum = calculateSha1(file.data(), file.size());
                        entry.variant = getPresetForFile(entry.sha1sum);
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

bool Librarian::isKnownFile(const uint8_t* data, size_t size) const
{
    auto sha1 = calculateSha1(data, size);
    return _cfg.romConfigs.contains(sha1) || findKnownRom(sha1) != nullptr;
}

bool Librarian::isKnownFile(const Sha1::Digest& sha1) const
{
    return _cfg.romConfigs.contains(sha1) || findKnownRom(sha1) != nullptr;
}

bool Librarian::isGenericChip8(const uint8_t* data, size_t size) const
{
    auto sha1 = calculateSha1(data, size);
    return isGenericChip8(sha1);
}

bool Librarian::isGenericChip8(const Sha1::Digest& sha1) const
{
    auto* romInfo = findKnownRom(sha1);
    return romInfo && std::string(romInfo->preset) == "generic-chip-8";
}

#ifdef NEW_ROMLIST_FORMAT

std::string Librarian::getPresetForFile(const Sha1::Digest& sha1) const
{
    auto cfgIter = _cfg.romConfigs.find(sha1);
    // TODO: Fix this
    //if(cfgIter != _cfg.romConfigs.end())
    //    return cfgIter->second.behaviorBase;
    const auto* romInfo = findKnownRom(sha1);
    return romInfo ? romInfo->preset : "chip-8";
}

std::string Librarian::getPresetForFile(const uint8_t* data, size_t size) const
{
    auto sha1 = calculateSha1(data, size);
    return getPresetForFile(sha1);
}

std::string Librarian::getEstimatedPresetForFile(std::string_view filename, std::string_view currentPreset, const uint8_t* data, size_t size) const
{
    // TODO: Avoid generating a CoreRegistry instance
    std::string result;
    if(fs::path(filename).extension().string() != ".ch8" && emu::CoreRegistry().getSupportedExtensions().contains(fs::path(filename).extension().string()))
        result = emu::CoreRegistry::presetForExtension(fs::path(filename).extension().string());
    if(result.empty()) {
        uint16_t startAddress = 0x200;  // TODO: endsWith(entry.filePath, ".c8x") ? 0x300 : 0x200;
        emu::Chip8Decompiler dec{{data, size}, startAddress};
        dec.decompile("", startAddress, nullptr, true, true);
        //auto possibleVariants = dec.possibleVariants();
        if ((uint64_t)dec.possibleVariants()) {
            /*if (dec.supportsVariant(emu::Chip8EmulatorOptions::variantForPreset(currentPreset))) {
                return currentPreset;
            }*/
            if (dec.supportsVariant(emu::Chip8Variant::XO_CHIP))
                return "xo-chip";
            if (dec.supportsVariant(emu::Chip8Variant::MEGA_CHIP))
                return "megachip";
            if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_1))
                return "schip-1.1";
            if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_0))
                return "schip-1.0";
            if (dec.supportsVariant(emu::Chip8Variant::CHIP_48))
                return "chip-48";
            if (dec.supportsVariant(emu::Chip8Variant::CHIP_10))
                return "chip-10";
        }
        return "chip-8";
    }
    return result;
}

emu::Properties Librarian::getPropertiesForFile(const uint8_t* data, size_t size) const
{
    auto sha1 = calculateSha1(data, size);
    return getPropertiesForFile(sha1);
}

emu::Properties Librarian::getPropertiesForFile(const Sha1::Digest& sha1) const
{
    auto cfgIter = _cfg.romConfigs.find(sha1);
    if(cfgIter != _cfg.romConfigs.end()) {
        return cfgIter->second;
    }
    return getPropertiesForSha1(sha1);
}

emu::Properties Librarian::getPropertiesForSha1(const Sha1::Digest& sha1)
{
    if (const auto* romInfo = findKnownRom(sha1)) {
        auto properties = emu::CoreRegistry::propertiesForPreset(romInfo->preset);
        if(romInfo->options) {
            properties.applyDiff(nlohmann::json::parse(std::string(romInfo->options)));
        }
        return properties;
    }
    return emu::CoreRegistry::propertiesForPreset("chip-8");
}

#else

emu::Chip8EmulatorOptions::SupportedPreset Librarian::getPresetForFile(std::string sha1sum) const
{
    auto cfgIter = _cfg.romConfigs.find(sha1sum);
    // TODO: Fix this
    //if(cfgIter != _cfg.romConfigs.end())
    //    return cfgIter->second.behaviorBase;
    const auto* romInfo = findKnownRom(sha1sum);
    return romInfo ? emu::Chip8EmulatorOptions::presetForVariant(romInfo->variant) : emu::Chip8EmulatorOptions::eCHIP8;
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::getPresetForFile(const uint8_t* data, size_t size) const
{
    auto sha1sum = calculateSha1Hex(data, size);
    return getPresetForFile(sha1sum);
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::getPresetForFile(std::string sha1sum) const
{
    auto cfgIter = _cfg.romConfigs.find(sha1sum);
    // TODO: Fix this
    //if(cfgIter != _cfg.romConfigs.end())
    //    return cfgIter->second.behaviorBase;
    const auto* romInfo = findKnownRom(sha1sum);
    return romInfo ? emu::Chip8EmulatorOptions::presetForVariant(romInfo->variant) : emu::Chip8EmulatorOptions::eCHIP8;
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::getPresetForFile(const uint8_t* data, size_t size) const
{
    auto sha1sum = calculateSha1Hex(data, size);
    return getPresetForFile(sha1sum);
}

emu::Chip8EmulatorOptions::SupportedPreset Librarian::getEstimatedPresetForFile(emu::Chip8EmulatorOptions::SupportedPreset currentPreset, const uint8_t* data, size_t size) const
{
    emu::Chip8Decompiler dec;
    uint16_t startAddress = 0x200;  // TODO: endsWith(entry.filePath, ".c8x") ? 0x300 : 0x200;
    dec.decompile("", data, startAddress, size, startAddress, nullptr, true, true);
    auto possibleVariants = dec.possibleVariants();
    if ((uint64_t)dec.possibleVariants()) {
        if (dec.supportsVariant(emu::Chip8EmulatorOptions::variantForPreset(currentPreset))) {
            return currentPreset;
        }
        else if (dec.supportsVariant(emu::Chip8Variant::XO_CHIP))
            return emu::Chip8EmulatorOptions::eXOCHIP;
        else if (dec.supportsVariant(emu::Chip8Variant::MEGA_CHIP))
            return emu::Chip8EmulatorOptions::eMEGACHIP;
        else if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_1))
            return emu::Chip8EmulatorOptions::eSCHIP11;
        else if (dec.supportsVariant(emu::Chip8Variant::SCHIP_1_0))
            return emu::Chip8EmulatorOptions::eSCHIP10;
        else if (dec.supportsVariant(emu::Chip8Variant::CHIP_48))
            return emu::Chip8EmulatorOptions::eCHIP48;
        else if (dec.supportsVariant(emu::Chip8Variant::CHIP_10))
            return emu::Chip8EmulatorOptions::eSCHIP10;
    }
    return emu::Chip8EmulatorOptions::eCHIP8;
}

emu::Chip8EmulatorOptions Librarian::getOptionsForFile(const uint8_t* data, size_t size) const
{
    auto sha1sum = calculateSha1Hex(data, size);
    return getOptionsForFile(sha1sum);
}

emu::Chip8EmulatorOptions Librarian::getOptionsForFile(const std::string& sha1sum) const
{
    // TODO: Fix this
    /*
    auto cfgIter = _cfg.romConfigs.find(sha1sum);
    if(cfgIter != _cfg.romConfigs.end()) {
        return cfgIter->second;
    }
    */
    const auto* romInfo = findKnownRom(sha1sum);
    if (romInfo) {
        auto preset = emu::Chip8EmulatorOptions::presetForVariant(romInfo->variant);
        auto options = emu::Chip8EmulatorOptions::optionsOfPreset(preset);
        if(romInfo->options) {
            emu::from_json(nlohmann::json::parse(std::string(romInfo->options)), options);
        }
        options.behaviorBase = preset;
        return options;
    }
    return emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eCHIP8);
}

emu::Chip8EmulatorOptions Librarian::getOptionsForSha1(const std::string_view& sha1)
{
    const auto* romInfo = findKnownRom(std::string(sha1));
    if (romInfo) {
        auto preset = emu::Chip8EmulatorOptions::presetForVariant(romInfo->variant);
        auto options = emu::Chip8EmulatorOptions::optionsOfPreset(preset);
        if(romInfo->options) {
            emu::from_json(nlohmann::json::parse(std::string(romInfo->options)), options);
            options.behaviorBase = preset;
        }
        return options;
    }
    return emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eCHIP8);
}

#endif

Librarian::Screenshot Librarian::genScreenshot(const Info& info, const std::array<uint32_t, 256> palette) const
{
    using namespace std::literals::chrono_literals;
    // TODO: Fix this
    if(false && info.analyzed && (info.type == Info::eROM_FILE || info.type == Info::eOCTO_SOURCE) ) {
        emu::HeadlessHost host;
        host.updateEmulatorOptions({});
        if(host.loadRom((fs::path(_currentPath) / info.filePath).string().c_str(), emu::HeadlessHost::SetToRun)) {
            auto& core = host.emuCore();
            // TODO: Fix this
            // auto options = host.options();
            auto ticks = 5000;
            auto startChip8 = std::chrono::steady_clock::now();
            auto colors = palette;
            //if(options.hasColors()) {
            //    options.updateColors(colors);
            //}
            int64_t lastCycles = -1;
            int64_t cycles = 0;
            int tickCount = 0;
            for (tickCount = 0; tickCount < ticks /* && (cycles == chipEmu.getCycles()) != lastCycles */ && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startChip8) < 100ms; ++tickCount) {
                core.executeFrame();
                lastCycles = cycles;
            }
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startChip8).count();
            TraceLog(LOG_WARNING, "executed %d cycles and %d frames in %dms for screenshot", (int)core.cycles(), tickCount, (int)duration);
            if (core.getScreen()) {
                Screenshot s;
                auto screen = *core.getScreen();
                screen.setPalette(colors);
                if (core.isDoublePixel()) {
                    s.width = core.getCurrentScreenWidth() / 2;
                    s.height = core.getCurrentScreenHeight() / 2;
                    s.pixel.resize(s.width * s.height);
                    for (int y = 0; y < core.getCurrentScreenHeight() / 2; ++y) {
                        for (int x = 0; x < core.getCurrentScreenWidth() / 2; ++x) {
                            s.pixel[y * s.width + x] = screen.getPixel(x*2, y*2);
                        }
                    }
                }
                else {
                    s.width = core.getCurrentScreenWidth();
                    s.height = core.getCurrentScreenHeight();
                    s.pixel.resize(s.width * s.height);
                    for (int y = 0; y < core.getCurrentScreenHeight(); ++y) {
                        for (int x = 0; x < core.getCurrentScreenWidth(); ++x) {
                            s.pixel[y * s.width + x] = screen.getPixel(x, y);
                        }
                    }
                }
                return s;
            }
            else if (core.getScreenRGBA()) {
                Screenshot s;
                auto screen = *core.getScreenRGBA();
                s.width = core.getCurrentScreenWidth();
                s.height = core.getCurrentScreenHeight();
                s.pixel.resize(s.width * s.height);
                for (int y = 0; y < core.getCurrentScreenHeight(); ++y) {
                    for (int x = 0; x < core.getCurrentScreenWidth(); ++x) {
                        s.pixel[y * s.width + x] = screen.getPixel(x, y);
                    }
                }
                return s;
            }
        }
    }
    return Librarian::Screenshot();
}

bool Librarian::isPrefixedTPDRom(const uint8_t* data, size_t size)
{
    static const uint8_t magic[] = {0x12, 0x60, 0x01, 0x7a, 0x42, 0x70, 0x22, 0x78};
    return size > 0x60 && std::memcmp(magic, data, 8) == 0;
}

bool Librarian::isPrefixedRSTDPRom(const uint8_t* data, size_t size)
{
    static const uint8_t magic[] = {0x9c, 0x7c, 0x00, 0xbc, 0xfb, 0x10, 0x30, 0xfc};
    return size > 0xC0 && isPrefixedTPDRom(data, size) && std::memcmp(magic, data + 0x50, 8) == 0;
}
