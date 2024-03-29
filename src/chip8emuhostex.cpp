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
#include <emulation/chip8emulatorbase.hpp>
#include <emulation/chip8strict.hpp>
#include <emulation/utility.hpp>
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


void Chip8EmuHostEx::updateEmulatorOptions(Chip8EmulatorOptions options)
{
    if(_previousOptions != options || !_chipEmu) {
        _previousOptions = _options = options;
        if (_options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8VIP || _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8VIP_TPD || _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8VIP_FPD ||
            _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8EVIP ||
            _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8XVIP || _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8XVIP_TPD || _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8XVIP_FPD)
            _chipEmu = emu::Chip8EmulatorBase::create(*this, emu::IChip8Emulator::eCHIP8VIP, _options, _chipEmu.get());
        else if (_options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8DREAM ||_options.behaviorBase == emu::Chip8EmulatorOptions::eC8D68CHIPOSLO)
            _chipEmu = emu::Chip8EmulatorBase::create(*this, emu::IChip8Emulator::eCHIP8DREAM, _options, _chipEmu.get());
        else if(_options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8TE)
            _chipEmu = std::make_unique<emu::Chip8StrictEmulator>(*this, _options, _chipEmu.get());
        else
            _chipEmu = emu::Chip8EmulatorBase::create(*this, emu::IChip8Emulator::eCHIP8MPT, _options, _chipEmu.get());
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

bool Chip8EmuHostEx::loadRom(const char* filename, bool andRun)
{
    std::error_code ec;
    if (strlen(filename) < 4095 && fs::exists(filename, ec)) {
        unsigned int size = 0;
        _customPalette = false;
        _colorPalette = _defaultPalette;
#ifdef WITH_EDITOR
        _editor.setText("");
        _editor.setFilename("");
#endif
        auto fileData = loadFile(filename, Librarian::MAX_ROM_SIZE);
        return loadBinary(filename, fileData.data(), fileData.size(), andRun);
        //memory[0x1FF] = 3;
    }
    return false;
}

bool Chip8EmuHostEx::loadBinary(std::string filename, const uint8_t* data, size_t size, bool andRun)
{
    bool valid = false;
    std::unique_ptr<emu::OctoCompiler> c8c;
    std::string romSha1Hex;
    std::vector<uint8_t> romImage;
    auto fileData = std::vector<uint8_t>(data, data + size);
    auto isKnown = _librarian.isKnownFile(fileData.data(), fileData.size());
    bool wasFromSource = false;
    TraceLog(LOG_INFO, "Loading %s file with sha1: %s", isKnown ? "known" : "unknown", calculateSha1Hex(fileData.data(), fileData.size()).c_str());
    auto knownOptions = _librarian.getOptionsForFile(fileData.data(), fileData.size());
    if(endsWith(filename, ".8o")) {
        c8c = std::make_unique<emu::OctoCompiler>();
        if(c8c->compile(filename).resultType == emu::CompileResult::eOK)
        {
            if(c8c->codeSize() < _chipEmu->memSize() - _options.startAddress) {
                romImage.assign(c8c->code(), c8c->code() + c8c->codeSize());
                romSha1Hex = c8c->sha1Hex();
#ifdef WITH_EDITOR
                _editor.setText(loadTextFile(filename));
                _editor.setFilename(filename);
                _mainView = eEDITOR;
#endif
                valid = true;
                wasFromSource = true;
            }
        }
    }
    else if(isKnown) {
        if(_options.behaviorBase != emu::Chip8EmulatorOptions::ePORTABLE && knownOptions.behaviorBase != Chip8EmulatorOptions::ePORTABLE)
            updateEmulatorOptions(knownOptions);
        if(_options.hasColors()) {
            _options.updateColors(_colorPalette);
        }
        romImage = fileData;
        valid = true;
    }
    else if(endsWith(filename, ".ch10")) {
        updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP10));
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
    }
    else if(endsWith(filename, ".hc8") || Librarian::isPrefixedRSTDPRom(fileData.data(), fileData.size())) {
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
        updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8VIP_TPD));
    }
    else if(endsWith(filename, ".c8e")) {
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
        updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8EVIP));
    }
    else if(endsWith(filename, ".c8x")) {
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
        updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8XVIP));
    }
    else if(endsWith(filename, ".sc8")) {
        updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eSCHIP11));
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
    }
    else if(endsWith(filename, ".mc8")) {
        updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eMEGACHIP));
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
    }
    else if(endsWith(filename, ".xo8")) {
        updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eXOCHIP));
        if (size < _chipEmu->memSize() - _options.startAddress) {
            romImage = fileData;
            valid = true;
        }
    }
    else if(endsWith(filename, ".ch8")) {
        auto estimate = _librarian.getEstimatedPresetForFile(_options.behaviorBase, fileData.data(), fileData.size());
        if(_options.behaviorBase != estimate)
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
    if (valid) {
        //TraceLog(LOG_INFO, "Found a valid rom.");
        _romImage = std::move(romImage);
        _romSha1Hex = romSha1Hex.empty() ? calculateSha1Hex(_romImage.data(), _romImage.size()) : romSha1Hex;
        _romName = filename;
        _romIsWellKnown = isKnown;
        if(isKnown && knownOptions.behaviorBase != Chip8EmulatorOptions::ePORTABLE)
            _romWellKnownOptions = _options;
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
        std::string source;
        //TraceLog(LOG_INFO, "Done with directory change.");
        if(wasFromSource) {
            source.assign((const char*)fileData.data(), fileData.size());
            //TraceLog(LOG_INFO, "Assigned source.");
        }
        else if(_romImage.size() < 8192*1024) {
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
        whenRomLoaded(fs::path(_romName).replace_extension(".8o").string(), andRun, c8c.get(), source);
    }
    return valid;
}

}
