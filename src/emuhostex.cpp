//---------------------------------------------------------------------------------------
// src/emulation/chip8emuhostex.cpp
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

#include <emuhostex.hpp>

#include <chiplet/chip8decompiler.hpp>
#include <chiplet/octocartridge.hpp>
#include <chiplet/utility.hpp>
#include <configuration.hpp>
#include <emulation/c8bfile.hpp>
#include <emulation/coreregistry.hpp>
//#include <emulation/chip8generic.hpp>
//#include <emulation/chip8strict.hpp>
//#include <emulation/cosmacvip.hpp>
//#include <emulation/dream6800.hpp>
#include <systemtools.hpp>

#include <raylib.h>
#include <nlohmann/json.hpp>
#include <ghc/span.hpp>

namespace emu {

std::unique_ptr<Database> EmuHostEx::_database;

EmuHostEx::EmuHostEx(CadmiumConfiguration& cfg)
    : _cfg(cfg)
    , _librarian(_cfg)
#ifndef PLATFORM_WEB
    , _threadPool(6)
#endif
{
#ifndef PLATFORM_WEB
    _currentDirectory = _cfg.workingDirectory.empty() ? fs::current_path().string() : _cfg.workingDirectory;
    _databaseDirectory = _cfg.libraryPath;
    if (!_instanceNum) {
        _database = std::make_unique<Database>(_cores, _cfg, _threadPool, dataPath());
    }
    _librarian.fetchDir(_currentDirectory);
#endif
    // TODO: Fix this
    //if(_options.hasColors())
    //    _options.updateColors(_colorPalette);
    //else {
        setPalette({0x1a1c2cff, 0xf4f4f4ff, 0x94b0c2ff, 0x333c57ff, 0xb13e53ff, 0xa7f070ff, 0x3b5dc9ff, 0xffcd75ff, 0x5d275dff, 0x38b764ff, 0x29366fff, 0x566c86ff, 0xef7d57ff, 0x73eff7ff, 0x41a6f6ff, 0x257179ff});
    //}
    _defaultPalette = _colorPalette;
}

void EmuHostEx::setPalette(const std::vector<uint32_t>& colors, size_t offset)
{
    if (_colorPalette.size() < colors.size() + offset)
        _colorPalette.colors.resize(colors.size() + offset);
    for (size_t i = 0; i < colors.size() && i + offset < _colorPalette.size(); ++i) {
        _colorPalette.colors[i + offset] = Palette::Color(colors[i]);
    }
    if (_chipEmu)
        _chipEmu->setPalette(_colorPalette);
    //std::vector<std::string> pal(16, "");
    //for (size_t i = 0; i < 16; ++i) {
    //    pal[i] = fmt::format("#{:06x}", _colorPalette[i] >> 8);
    //}
    // TODO: Fix this
    //_options.advanced["palette"] = pal;
    //_options.updatedAdvanced();
}

void EmuHostEx::setPalette(const Palette& palette)
{
    _colorPalette = palette;
    if (_properties)
        _properties->palette() = palette;
}

std::unique_ptr<IEmulationCore> EmuHostEx::create(Properties& properties, IEmulationCore* iother)
{
    auto [variantName, emuCore] = CoreRegistry::create(*this, properties);
    _variantName = variantName;
    return std::move(emuCore);
#if 0
    IChip8Emulator::Engine engine = IChip8Emulator::eCHIP8MPT;
    if (_options.behaviorBase == Chip8EmulatorOptions::eCHIP8VIP || _options.behaviorBase == Chip8EmulatorOptions::eCHIP8VIP_TPD || _options.behaviorBase == Chip8EmulatorOptions::eCHIP8VIP_FPD ||
        _options.behaviorBase == Chip8EmulatorOptions::eCHIP8EVIP ||
        _options.behaviorBase == Chip8EmulatorOptions::eCHIP8XVIP || _options.behaviorBase == Chip8EmulatorOptions::eCHIP8XVIP_TPD || _options.behaviorBase == Chip8EmulatorOptions::eCHIP8XVIP_FPD ||
        _options.behaviorBase == Chip8EmulatorOptions::eRAWVIP)
        engine = IChip8Emulator::eCHIP8VIP;
    else if (_options.behaviorBase == Chip8EmulatorOptions::eCHIP8DREAM ||_options.behaviorBase == Chip8EmulatorOptions::eC8D68CHIPOSLO)
        engine = IChip8Emulator::eCHIP8DREAM;
    else if(_options.behaviorBase == Chip8EmulatorOptions::eCHIP8TE)
        return std::make_unique<Chip8StrictEmulator>(*this, _options, _chipEmu.get());

    if(engine == emu::IChip8Emulator::eCHIP8TS) {
        if (options.optAllowHires) {
            if (options.optHas16BitAddr) {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, HiresSupport | MultiColor | WrapSprite>>(*this, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<16, HiresSupport | MultiColor>>(*this, options, iother);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, HiresSupport | WrapSprite>>(*this, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<16, HiresSupport>>(*this, options, iother);
                }
            }
            else {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, HiresSupport | MultiColor | WrapSprite>>(*this, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<12, HiresSupport | MultiColor>>(*this, options, iother);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, HiresSupport | WrapSprite>>(*this, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<12, HiresSupport>>(*this, options, iother);
                }
            }
        }
        else {
            if (options.optHas16BitAddr) {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, MultiColor | WrapSprite>>(*this, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<16, MultiColor>>(*this, options, iother);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<16, WrapSprite>>(*this, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<16, 0>>(*this, options, iother);
                }
            }
            else {
                if (options.optAllowColors) {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, MultiColor | WrapSprite>>(*this, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<12, MultiColor>>(*this, options, iother);
                }
                else {
                    if (options.optWrapSprites)
                        return std::make_unique<Chip8Emulator<12, WrapSprite>>(*this, options, iother);
                    else
                        return std::make_unique<Chip8Emulator<12, 0>>(*this, options, iother);
                }
            }
        }
    }
    else if(engine == IChip8Emulator::eCHIP8MPT) {
        return std::make_unique<Chip8GenericEmulator>(*this, options, iother);
    }
    else if(engine == IChip8Emulator::eCHIP8VIP) {
        return std::make_unique<CosmacVIP>(*this, options, iother);
    }
    else if(engine == IChip8Emulator::eCHIP8DREAM) {
        return std::make_unique<Dream6800>(*this, options, iother);
    }
    return std::make_unique<Chip8EmulatorVIP>(*this, options, iother);
#endif
}


void EmuHostEx::updateEmulatorOptions(const Properties& properties)
{
    // TODO: Fix this
    if(_previousProperties != properties || !_chipEmu) {
        if(&properties != _properties) {
            if(!_properties || _properties->propertyClass().empty() || _properties->propertyClass() != properties.propertyClass()) {

                auto pIter = _propertiesByClass.find(properties.propertyClass());
                if(pIter == _propertiesByClass.end()) {
                    pIter = _propertiesByClass.emplace(properties.propertyClass(), properties).first;
                }
                _properties = &pIter->second;
            }
            *_properties = properties;
        }
        _previousProperties = properties;
        _chipEmu = create(*_properties, _chipEmu.get());
        if(_chipEmu->getScreen())
            (void)_chipEmu->getScreen();
        whenEmuChanged(*_chipEmu);
    }
#if 0
    if(_previousOptions != options || !_chipEmu) {
        if(&options != &_options)
            _options = options;
        _previousOptions = _options.clone();
        _chipEmu = create(_options, _chipEmu.get());
        //
        auto tmpOpt = emu::Chip8EmulatorOptions::optionsOfPreset(options.behaviorBase);
        if(tmpOpt.hasColors()) {
            tmpOpt.updateColors(_colorPalette);
            _chipEmu->setPalette(_colorPalette);
            std::vector<std::string> pal(16, "");
            for(size_t i = 0; i < 16; ++i) {
                pal[i] = fmt::format("#{:06x}", _colorPalette[i] >> 8);
            }
            _options.advanced["palette"] = pal;
            _options.updatedAdvanced();
        }
        else if(_options.hasColors()) {
            _options.updateColors(_colorPalette);
            _chipEmu->setPalette(_colorPalette);
        }
        else {
            setPalette({_colorPalette.begin(), _colorPalette.end()});
        }
        //_debugger.updateCore(_chipEmu.get());
    }
#endif
}

bool EmuHostEx::loadRom(std::string_view filename, LoadOptions loadOpt)
{
    std::error_code ec;
    if (filename.size() < 4095 && fs::exists(filename, ec)) {
        unsigned int size = 0;
        _customPalette = false;
        _colorPalette = _defaultPalette;
        auto fileData = loadFile(filename, Librarian::MAX_ROM_SIZE);
        return loadBinary(filename, fileData, loadOpt);
        //memory[0x1FF] = 3;
    }
    return false;
}

static Chip8EmulatorOptions optionsFromOctoOptions(const OctoOptions& octo)
{
    Chip8EmulatorOptions result;
    if(octo.maxRom > 3584 || !octo.qClip) {
        result = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eXOCHIP);
    }
    else if(octo.qVBlank || !octo.qLoadStore || !octo.qShift) {
        result = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8);
    }
    else {
        result = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eSCHPC);
    }
    result.optJustShiftVx = octo.qShift;
    result.optLoadStoreDontIncI = octo.qLoadStore;
    result.optLoadStoreIncIByX = false;
    result.optJump0Bxnn = octo.qJump0;
    result.optDontResetVf = !octo.qLogic;
    result.optWrapSprites = !octo.qClip;
    result.optInstantDxyn = !octo.qVBlank;
    result.instructionsPerFrame = octo.tickrate;
    result.advanced["palette"] = octo.colors;
    return result;
}

bool EmuHostEx::loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, LoadOptions loadOpt)
{
    bool valid = false;
    bool wasFromSource = false;
    std::unique_ptr<emu::OctoCompiler> c8c;
    Sha1::Digest romSha1;
    std::vector<uint8_t> romImage;
    std::string source;
    // Properties knownProperties;
    auto fileData = std::vector(binary.data(), binary.data() + binary.size());
    auto isKnown = _librarian.isKnownFile(fileData.data(), fileData.size());
    TraceLog(LOG_INFO, "Loading %s file with sha1: %s", isKnown ? "known" : "unknown", calculateSha1(fileData.data(), fileData.size()).to_hex().c_str());
    auto knownProperties = _librarian.getPropertiesForFile(fileData.data(), fileData.size());
    if (endsWith(filename, ".8o")) {
        c8c = std::make_unique<emu::OctoCompiler>();
        source.assign((const char*)fileData.data(), fileData.size());
        if (c8c->compile(filename).resultType == emu::CompileResult::eOK) {
            auto startAddress = _properties->get<Property::Integer>("startAddress");
            auto loadAddress = startAddress ? startAddress->intValue : 0;
            if (c8c->codeSize() < _chipEmu->memSize() - loadAddress) {
                romImage.assign(c8c->code(), c8c->code() + c8c->codeSize());
                romSha1 = c8c->sha1();
                valid = true;
                wasFromSource = true;
                if ((loadOpt & DontChangeOptions) == 0) {
                    isKnown = _librarian.isKnownFile(romImage.data(), romImage.size());
                    // knownOptions = _librarian.getOptionsForFile(romImage.data(), romImage.size());
                    // if(knownOptions.behaviorBase != Chip8EmulatorOptions::ePORTABLE)
                    //     updateEmulatorOptions(knownOptions);
                }
            }
        }
        else {
            _romName = filename;
            whenRomLoaded(_romName, false, c8c.get(), source);
            return true;
        }
    }
    else {
        std::optional<uint32_t> loadAddress;
        if (Librarian::isPrefixedTPDRom(binary.data(), binary.size()))
            loadAddress = 0x200;
        if (loadOpt & DontChangeOptions) {
            _chipEmu->reset();
            if (_chipEmu->loadData(binary, loadAddress)) {
                romImage.assign(binary.data(), binary.data() + binary.size());
                valid = true;
            }
        }
        else {
            if (auto extensionProps = CoreRegistry::propertiesForExtension(fs::path(filename).extension().string())) {
                updateEmulatorOptions(extensionProps);
                _chipEmu->reset();
                if (_chipEmu->loadData(binary, loadAddress)) {
                    romImage.assign(binary.data(), binary.data() + binary.size());
                    valid = true;
                }
            }
        }
    }
    if (valid) {
        // TraceLog(LOG_INFO, "Found a valid rom.");
        _romImage = std::move(romImage);
        _romSha1 = romSha1 == Sha1::Digest{} ? calculateSha1(_romImage.data(), _romImage.size()) : romSha1;
        _romName = filename;
        _romIsWellKnown = isKnown;
        // if(isKnown && knownOptions.behaviorBase != Chip8EmulatorOptions::ePORTABLE)
        //     _romWellKnownProperties = knownProperties;
        //_chipEmu->reset();
        if (!wasFromSource && _romImage.size() < 8192 * 1024) {
            // TraceLog(LOG_INFO, "Setting up decompiler.");
            std::stringstream os;
            // TraceLog(LOG_INFO, "Setting instance.");
            auto startAddress = _properties->get<Property::Integer>("startAddress");
            auto loadAddress = startAddress ? startAddress->intValue : 0;
            emu::Chip8Decompiler decomp{_romImage, static_cast<uint32_t>(loadAddress)};
            decomp.setVariant(Chip8Variant::CHIP_8 /*_options.presetAsVariant()*/, true);
            // TraceLog(LOG_INFO, "Setting variant.");
            // decomp.setVariant(Chip8Variant::CHIP_8, true);
            // TraceLog(LOG_INFO, "About to decompile...");
            decomp.decompile(filename, loadAddress, &os, false, true);
            source = os.str();
        }
        whenRomLoaded(fs::path(_romName).replace_extension(".8o").string(), loadOpt & LoadOptions::SetToRun, c8c.get(), source);
    }
    // TODO: Fix this
#if 0
    std::unique_ptr<emu::OctoCompiler> c8c;
    std::string romSha1Hex;
    std::vector<uint8_t> romImage;
    std::string source;
    auto fileData = std::vector<uint8_t>(data, data + size);
    auto isKnown = _librarian.isKnownFile(fileData.data(), fileData.size());
    TraceLog(LOG_INFO, "Loading %s file with sha1: %s", isKnown ? "known" : "unknown", calculateSha1Hex(fileData.data(), fileData.size()).c_str());
    auto knownOptions = _librarian.getOptionsForFile(fileData.data(), fileData.size());
    if(endsWith(filename, ".8o")) {
        c8c = std::make_unique<emu::OctoCompiler>();
        source.assign((const char*)fileData.data(), fileData.size());
        if(c8c->compile(filename).resultType == emu::CompileResult::eOK)
        {
            if(c8c->codeSize() < _chipEmu->memSize() - _options.startAddress) {
                romImage.assign(c8c->code(), c8c->code() + c8c->codeSize());
                romSha1Hex = c8c->sha1Hex();
                valid = true;
                wasFromSource = true;
                if((loadOpt & LoadOptions::DontChangeOptions) == 0) {
                    isKnown = _librarian.isKnownFile(romImage.data(), romImage.size());
                    knownOptions = _librarian.getOptionsForFile(romImage.data(), romImage.size());
                    if(_options.behaviorBase != emu::Chip8EmulatorOptions::ePORTABLE && knownOptions.behaviorBase != Chip8EmulatorOptions::ePORTABLE)
                        updateEmulatorOptions(knownOptions);
                }
            }
        }
        else {
            _romName = filename;
            whenRomLoaded(_romName, false, c8c.get(), source);
            return true;
        }
    }
    else if(endsWith(filename, ".gif")) {
        emu::OctoCartridge cart(fileData);
        cart.loadCartridge();
        source = cart.getSource();
        if(!source.empty()) {
            c8c = std::make_unique<emu::OctoCompiler>();
            if(c8c->compile(filename, source.data(), source.data() + source.size(), false).resultType == emu::CompileResult::eOK)
            {
                if((loadOpt & LoadOptions::DontChangeOptions) == 0) {
                    auto octoOpt = cart.getOptions();
                    if((loadOpt & LoadOptions::DontChangeOptions) == 0) {
                        auto options = optionsFromOctoOptions(octoOpt);
                        updateEmulatorOptions(options);
                    }
                }
                if(c8c->codeSize() < _chipEmu->memSize() - _options.startAddress) {
                    romImage.assign(c8c->code(), c8c->code() + c8c->codeSize());
                    romSha1Hex = c8c->sha1Hex();
                    valid = true;
                    wasFromSource = true;
                }
            }
        }
    }
    else if(isKnown) {
        if((loadOpt & LoadOptions::DontChangeOptions) == 0 && _options.behaviorBase != emu::Chip8EmulatorOptions::ePORTABLE && knownOptions.behaviorBase != Chip8EmulatorOptions::ePORTABLE)
            updateEmulatorOptions(knownOptions);
        if(_options.hasColors()) {
            _options.updateColors(_colorPalette);
        }
        romImage = fileData;
        valid = true;
    }
    else if(endsWith(filename, ".ch10")) {
        if((loadOpt & LoadOptions::DontChangeOptions) == 0)
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP10));
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
    }
    else if(endsWith(filename, ".hc8") || Librarian::isPrefixedRSTDPRom(fileData.data(), fileData.size())) {
        if((loadOpt & LoadOptions::DontChangeOptions) == 0)
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8VIP));
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
    }
    else if(endsWith(filename, ".c8tp") || Librarian::isPrefixedTPDRom(fileData.data(), fileData.size())) {
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
        if((loadOpt & LoadOptions::DontChangeOptions) == 0)
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8VIP_TPD));
    }
    else if(endsWith(filename, ".c8e")) {
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
        if((loadOpt & LoadOptions::DontChangeOptions) == 0)
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8EVIP));
    }
    else if(endsWith(filename, ".c8x")) {
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
        if((loadOpt & LoadOptions::DontChangeOptions) == 0)
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8XVIP));
    }
    else if(endsWith(filename, ".sc8")) {
        if((loadOpt & LoadOptions::DontChangeOptions) == 0)
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eSCHIP11));
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
    }
    else if(endsWith(filename, ".mc8")) {
        if((loadOpt & LoadOptions::DontChangeOptions) == 0)
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eMEGACHIP));
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
    }
    else if(endsWith(filename, ".xo8")) {
        if((loadOpt & LoadOptions::DontChangeOptions) == 0)
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eXOCHIP));
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
    }
    else if(endsWith(filename, ".ch8")) {
        auto estimate = _librarian.getEstimatedPresetForFile(_options.behaviorBase, fileData.data(), fileData.size());
        if((loadOpt & LoadOptions::DontChangeOptions) == 0 && _options.behaviorBase != estimate)
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(estimate));
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
    }
    else if(endsWith(filename, ".c8b")) {
        C8BFile c8b;
        if(c8b.loadFromData(fileData.data(), fileData.size()) == C8BFile::eOK) {
            uint16_t codeOffset = 0;
            uint16_t codeSize = 0;
            auto iter = c8b.findBestMatch({C8BFile::C8V_XO_CHIP, C8BFile::C8V_MEGA_CHIP, C8BFile::C8V_SCHIP_1_1, C8BFile::C8V_SCHIP_1_0, C8BFile::C8V_CHIP_48, C8BFile::C8V_CHIP_10, C8BFile::C8V_CHIP_8});
            if(iter != c8b.variantBytecode.end()) {
                if(!c8b.palette.empty()) {
                    _customPalette = true;
                    auto numCol = std::min(c8b.palette.size(), (size_t)16);
                    for(size_t i = 0; i < numCol; ++i) {
                        _colorPalette[i] = ColorToInt({c8b.palette[i].r, c8b.palette[i].g, c8b.palette[i].b, 255});
                    }
                }
                codeOffset = iter->second.first;
                codeSize = iter->second.second;
                if((loadOpt & LoadOptions::DontChangeOptions) == 0) {
                    switch (iter->first) {
                        case C8BFile::C8V_XO_CHIP:
                            updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eXOCHIP));
                            break;
                        case C8BFile::C8V_MEGA_CHIP:
                            updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eMEGACHIP));
                            break;
                        case C8BFile::C8V_SCHIP_1_1:
                            updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eSCHIP11));
                            break;
                        case C8BFile::C8V_SCHIP_1_0:
                            updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eSCHIP10));
                            break;
                        case C8BFile::C8V_CHIP_48:
                            updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP48));
                            break;
                        case C8BFile::C8V_CHIP_10:
                            updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP10));
                            break;
                        case C8BFile::C8V_CHIP_8:
                            updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8));
                            break;
                        default:
                            updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eSCHIP11));
                            break;
                    }
                }
                if(c8b.executionSpeed > 0) {
                    _options.instructionsPerFrame = c8b.executionSpeed;
                }
                if(codeSize < _chipEmu->memSize() - _options.startAddress) {
                    romImage.assign(c8b.rawData.data() + codeOffset, c8b.rawData.data() + codeOffset + codeSize);
                    valid = true;
                }
            }
            else {
                _chipEmu->reset();
            }
        }
    }
    else if(endsWith(filename, ".bin") || endsWith(filename, ".ram") || endsWith(filename, ".vip")) {
        if(size < 32*1024) {
            if ((loadOpt & LoadOptions::DontChangeOptions) == 0) {
                updateEmulatorOptions(emu::Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eRAWVIP));
            }
            romImage = fileData;
            valid = true;
        }
    }
    if (valid) {
        //TraceLog(LOG_INFO, "Found a valid rom.");
        _romImage = std::move(romImage);
        _romSha1Hex = romSha1Hex.empty() ? calculateSha1Hex(_romImage.data(), _romImage.size()) : romSha1Hex;
        _romName = filename;
        _romIsWellKnown = isKnown;
        if(isKnown && knownOptions.behaviorBase != Chip8EmulatorOptions::ePORTABLE)
            _romWellKnownOptions = knownOptions;
        _chipEmu->reset();
        if(Librarian::isPrefixedTPDRom(_romImage.data(), _romImage.size()))
            std::memcpy(_chipEmu->memory() + 512, _romImage.data(), std::min(_romImage.size(),size_t(_chipEmu->memSize() - 512)));
        else
            std::memcpy(_chipEmu->memory() + _options.startAddress, _romImage.data(), std::min(_romImage.size(),size_t(_chipEmu->memSize() - _options.startAddress)));
        _chipEmu->removeAllBreakpoints();
        if(!_options.hasColors()) {
            setPalette({0x1a1c2cff, 0xf4f4f4ff, 0x94b0c2ff, 0x333c57ff, 0xb13e53ff, 0xa7f070ff, 0x3b5dc9ff, 0xffcd75ff, 0x5d275dff, 0x38b764ff, 0x29366fff, 0x566c86ff, 0xef7d57ff, 0x73eff7ff, 0x41a6f6ff, 0x257179ff});
        }
        else {
            _options.updateColors(_colorPalette);
            _chipEmu->setPalette(_colorPalette);
        }
        //TraceLog(LOG_INFO, "Done with palette.");
        auto p = fs::path(_romName).parent_path();
        if(fs::exists(p) && fs::is_directory(p))  {
            _currentDirectory = fs::path(_romName).parent_path().string();
            _librarian.fetchDir(_currentDirectory);
        }
        //TraceLog(LOG_INFO, "Done with directory change.");
        if(!wasFromSource && _romImage.size() < 8192*1024) {
            //TraceLog(LOG_INFO, "Setting up decompiler.");
            std::stringstream os;
            //TraceLog(LOG_INFO, "Setting instance.");
            emu::Chip8Decompiler decomp;
            decomp.setVariant(_options.presetAsVariant(), true);
            //TraceLog(LOG_INFO, "Setting variant.");
            //decomp.setVariant(Chip8Variant::CHIP_8, true);
            //TraceLog(LOG_INFO, "About to decompile...");
            decomp.decompile(filename, _romImage.data(), _options.startAddress, _romImage.size(), _options.startAddress, &os, false, true);
            source = os.str();
        }
        //TraceLog(LOG_INFO, "About to callback whenRomLoaded.");
        whenRomLoaded(fs::path(_romName).replace_extension(".8o").string(), loadOpt & LoadOptions::SetToRun, c8c.get(), source);
    }
#endif
    return valid;
}

bool EmuHostEx::loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, const Properties& props, const bool isKnown)
{
    _customPalette = false;
    _colorPalette = _defaultPalette;
    if (props) {
        updateEmulatorOptions(props);
    }
    _romImage = std::vector(binary.data(), binary.data() + binary.size());
    _romSha1 = calculateSha1(_romImage.data(), _romImage.size());
    _romName = filename;
    _romIsWellKnown = isKnown;
    if (isKnown) {
        _romWellKnownProperties = props;
    }
    _chipEmu->reset();
    auto startAddress = props.get<Property::Integer>("startAddress");
    auto loadAddress = startAddress ? startAddress->intValue : 0;
    _chipEmu->loadData(binary, loadAddress);
    return true;
}


ThreadedBackgroundHost::ThreadedBackgroundHost(double initialFrameRate)
: EmuHostEx(_cfg)
, _workerThread(&ThreadedBackgroundHost::worker, this)
{
    setFrameRate(initialFrameRate);
    _screen1 = GenImageColor(emu::SUPPORTED_SCREEN_WIDTH, emu::SUPPORTED_SCREEN_HEIGHT, BLACK);
    _screen2 = GenImageColor(emu::SUPPORTED_SCREEN_WIDTH, emu::SUPPORTED_SCREEN_HEIGHT, BLACK);
    _screen = &_screen1;
}

ThreadedBackgroundHost::ThreadedBackgroundHost(const Properties& options, double initialFrameRate)
    : EmuHostEx(_cfg)
    , _workerThread(&ThreadedBackgroundHost::worker, this)
{
    setFrameRate(initialFrameRate);
    ThreadedBackgroundHost::updateEmulatorOptions(options);
}

ThreadedBackgroundHost::~ThreadedBackgroundHost()
{
    killEmulation();
    _shutdown.store(true, std::memory_order_relaxed);
    if (_workerThread.joinable()) {
        _workerThread.join();
    }
    UnloadImage(_screen2);
    UnloadImage(_screen1);
}

void ThreadedBackgroundHost::setFrameRate(double frequency)
{
    if (frequency <= 0.0) {
        return;
    }
    using namespace std::chrono;
    duration<double> dur(1.0 / frequency);
    _frameDuration.store(
        duration_cast<steady_clock::duration>(dur),
        std::memory_order_relaxed
    );
}

void ThreadedBackgroundHost::worker()
{
    using clock = std::chrono::steady_clock;
    auto nextTick = clock::now();
    static decltype(nextTick) lastNow = clock::now() - std::chrono::microseconds(16667);

    while (!_shutdown.load(std::memory_order_relaxed))
    {
        tick();

        auto frameDuration = _frameDuration.load(std::memory_order_relaxed);
        nextTick += frameDuration;
        auto now = clock::now();
        _smaFrameTime_us.add(std::chrono::duration_cast<std::chrono::microseconds>(now - lastNow).count());
        lastNow = now;
        if (now >= nextTick + frameDuration) {
            // if we are two or more frames late, skip time
            nextTick = now + frameDuration;
        }
        std::this_thread::sleep_until(nextTick);
    }
}

void ThreadedBackgroundHost::killEmulation()
{
    std::unique_lock guard(_mutex);
    _chipEmu.reset();
    ImageClearBackground(&_screen1, BLACK);
    ImageClearBackground(&_screen2, BLACK);
}


void ThreadedBackgroundHost::updateEmulatorOptions(const Properties& properties)
{
    std::unique_lock guard(_mutex);
    EmuHostEx::updateEmulatorOptions(properties);
}

void ThreadedBackgroundHost::tick()
{
    std::unique_lock guard(_mutex);
    if (_chipEmu)
        _chipEmu->executeFrame();
}
void ThreadedBackgroundHost::drawScreen(Rectangle dest) const
{
    if (_chipEmu && _screenTexture.id) {
        const Color gridLineCol{40,40,40,255};
        int scrWidth = _chipEmu->getCurrentScreenWidth();
        //int scrHeight = crt ? 385 : (_chipEmu->isGenericEmulation() ? _chipEmu->getCurrentScreenHeight() : 128);
        int scrHeight = _chipEmu->getCurrentScreenHeight();
        auto videoScaleX = dest.width / scrWidth;
        auto videoScaleY = _chipEmu->getScreen() && _chipEmu->getScreen()->ratio() ? videoScaleX / _chipEmu->getScreen()->ratio() : videoScaleX;
        auto videoX = (dest.width - _chipEmu->getCurrentScreenWidth() * videoScaleX) / 2 + dest.x;
        auto videoY = (dest.height - _chipEmu->getCurrentScreenHeight() * videoScaleY) / 2 + dest.y;
        if(_chipEmu->getMaxScreenWidth() > 128)
            DrawRectangleRec(dest, {0,0,0,255});
        else
            DrawRectangleRec(dest, {0,12,24,255});

        DrawTexturePro(_screenTexture, {0, 0, (float)scrWidth, (float)scrHeight}, {videoX, videoY, scrWidth * videoScaleX, scrHeight * videoScaleY}, {0, 0}, 0, WHITE);
    }
    else {
        DrawRectangleRec(dest, {0,0,0,255});
    }
}

void ThreadedBackgroundHost::updateScreen()
{
}

std::pair<Texture2D*, Rectangle> ThreadedBackgroundHost::updateTexture()
{
    std::unique_lock guard(_mutex);
    if (!_screenTexture.id)
        _screenTexture = LoadTextureFromImage(*_screen);
    UpdateTexture(_screenTexture, _screen->data);
    return {&_screenTexture, {0, 0, 0, 0}};
}

void ThreadedBackgroundHost::vblank()
{
    std::unique_lock guard(_mutex);
    if (auto* pixel = static_cast<uint32_t*>(_screen->data)) {
        if (const auto* screen = _chipEmu->getScreen()) {
            screen->convert(pixel, _screen->width, 255, nullptr);
        }
        else {
            if (const auto* screenRgb = _chipEmu->getScreenRGBA()) {
                screenRgb->convert(pixel, _screen->width, _chipEmu->getScreenAlpha(), _chipEmu->getWorkRGBA());
            }
        }
    }
    _screen = _screen == &_screen1 ? &_screen2 : &_screen1;
}

bool ThreadedBackgroundHost::loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, const Properties& props, const bool isKnown)
{
    auto rc = EmuHostEx::loadBinary(filename, binary, props, isKnown);
    auto frameRate = props.get<Property::Integer>("frameRate");
    setFrameRate(frameRate ? frameRate->intValue : 60);
    return rc;
}

}
