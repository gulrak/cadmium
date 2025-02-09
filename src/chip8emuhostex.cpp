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

#include <chip8emuhostex.hpp>

#include <chiplet/chip8decompiler.hpp>
#include <emulation/chip8cores.hpp>
#include <emulation/chip8strict.hpp>
#include <emulation/chip8dream.hpp>
#include <emulation/chip8vip.hpp>
#include <chiplet/utility.hpp>
#include <chiplet/octocartridge.hpp>
#include <emulation/c8bfile.hpp>
#include <systemtools.hpp>
#include <configuration.hpp>

#include <raylib.h>
#include <nlohmann/json.hpp>

namespace emu {

Chip8EmuHostEx::Chip8EmuHostEx()
: _librarian(_cfg)
{
#ifndef PLATFORM_WEB
    _cfgPath = (fs::path(dataPath())/"config.json").string();
    if(_cfg.load(_cfgPath)) {
        _options = _cfg.emuOptions;
        _currentDirectory = _cfg.workingDirectory;
        _databaseDirectory = _cfg.databaseDirectory;
    }
    _librarian.fetchDir(_currentDirectory);
#endif
    if(_options.hasColors())
        _options.updateColors(_colorPalette);
    else {
        setPalette({0x1a1c2cff, 0xf4f4f4ff, 0x94b0c2ff, 0x333c57ff, 0xb13e53ff, 0xa7f070ff, 0x3b5dc9ff, 0xffcd75ff, 0x5d275dff, 0x38b764ff, 0x29366fff, 0x566c86ff, 0xef7d57ff, 0x73eff7ff, 0x41a6f6ff, 0x257179ff});
    }
    _defaultPalette = _colorPalette;
}

void Chip8EmuHostEx::setPalette(const std::vector<uint32_t>& colors, size_t offset)
{
    for(size_t i = 0; i < colors.size() && i + offset < _colorPalette.size(); ++i) {
        _colorPalette[i + offset] = colors[i];
    }
    if(_chipEmu)
        _chipEmu->setPalette(_colorPalette);
    std::vector<std::string> pal(16, "");
    for(size_t i = 0; i < 16; ++i) {
        pal[i] = fmt::format("#{:06x}", _colorPalette[i] >> 8);
    }
    _options.advanced["palette"] = pal;
    _options.updatedAdvanced();
}


std::unique_ptr<IChip8Emulator> Chip8EmuHostEx::create(Chip8EmulatorOptions& options, IChip8Emulator* iother)
{
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
        return std::make_unique<Chip8EmulatorFP>(*this, options, iother);
    }
    else if(engine == IChip8Emulator::eCHIP8VIP) {
        return std::make_unique<Chip8VIP>(*this, options, iother);
    }
    else if(engine == IChip8Emulator::eCHIP8DREAM) {
        return std::make_unique<Chip8Dream>(*this, options, iother);
    }
    return std::make_unique<Chip8EmulatorVIP>(*this, options, iother);
}


void Chip8EmuHostEx::updateEmulatorOptions(const Chip8EmulatorOptions& options)
{
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
        if(_chipEmu->getScreen())
            _chipEmu->getScreen();
        whenEmuChanged(*_chipEmu);
        //_debugger.updateCore(_chipEmu.get());
    }
}

bool Chip8EmuHostEx::loadRom(const char* filename, LoadOptions loadOpt)
{
    std::error_code ec;
    if (strlen(filename) < 4095 && fs::exists(filename, ec)) {
        unsigned int size = 0;
        _customPalette = false;
        _colorPalette = _defaultPalette;
        auto fileData = loadFile(filename, Librarian::MAX_ROM_SIZE);
        return loadBinary(filename, fileData.data(), fileData.size(), loadOpt);
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

bool Chip8EmuHostEx::loadBinary(std::string filename, const uint8_t* data, size_t size, LoadOptions loadOpt)
{
    bool valid = false;
    std::unique_ptr<emu::OctoCompiler> c8c;
    std::string romSha1Hex;
    std::vector<uint8_t> romImage;
    std::string source;
    auto fileData = std::vector<uint8_t>(data, data + size);
    auto isKnown = _librarian.isKnownFile(fileData.data(), fileData.size());
    bool wasFromSource = false;
    TraceLog(LOG_INFO, "Loading %s file with sha1: %s", isKnown ? "known" : "unknown", calculateSha1(fileData.data(), fileData.size()).to_hex().c_str());
    auto knownOptions = _librarian.getOptionsForFile(fileData.data(), fileData.size());
    if(endsWith(filename, ".8o")) {
        c8c = std::make_unique<emu::OctoCompiler>();
        source.assign((const char*)fileData.data(), fileData.size());
        if(c8c->compile(filename).resultType == emu::CompileResult::eOK)
        {
            if(c8c->codeSize() < _chipEmu->memSize() - _options.startAddress) {
                romImage.assign(c8c->code(), c8c->code() + c8c->codeSize());
                romSha1Hex = c8c->sha1().to_hex();
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
                    romSha1Hex = c8c->sha1().to_hex();
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
        _romSha1Hex = romSha1Hex.empty() ? calculateSha1(_romImage.data(), _romImage.size()).to_hex() : romSha1Hex;
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
    return valid;
}

}
