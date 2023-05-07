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

#include <emulation/chip8emulatorbase.hpp>
#include <emulation/utility.hpp>
#include <emulation/c8bfile.hpp>
#include <systemtools.hpp>
#include <configuration.hpp>

#include <raylib.h>


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
    setPalette({
        be32(0x1a1c2cff), be32(0xf4f4f4ff), be32(0x94b0c2ff), be32(0x333c57ff),
        be32(0xb13e53ff), be32(0xa7f070ff), be32(0x3b5dc9ff), be32(0xffcd75ff),
        be32(0x5d275dff), be32(0x38b764ff), be32(0x29366fff), be32(0x566c86ff),
        be32(0xef7d57ff), be32(0x73eff7ff), be32(0x41a6f6ff), be32(0x257179ff)
    });
    _defaultPalette = _colorPalette;
}

void Chip8EmuHostEx::setPalette(const std::vector<uint32_t>& colors, size_t offset)
{
    for(size_t i = 0; i < colors.size() && i + offset < _colorPalette.size(); ++i) {
        _colorPalette[i + offset] = colors[i];
    }
    if(_chipEmu)
        _chipEmu->setPalette(_colorPalette);
}


void Chip8EmuHostEx::updateEmulatorOptions(Chip8EmulatorOptions options)
{
    if(_previousOptions != options || !_chipEmu) {
        _previousOptions = _options = options;
        if (_options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8VIP || _options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8VIP_TPD)
            _chipEmu = emu::Chip8EmulatorBase::create(*this, emu::IChip8Emulator::eCHIP8VIP, _options, _chipEmu.get());
        else if (_options.behaviorBase == emu::Chip8EmulatorOptions::eCHIP8DREAM)
            _chipEmu = emu::Chip8EmulatorBase::create(*this, emu::IChip8Emulator::eCHIP8DREAM, _options, _chipEmu.get());
        else
            _chipEmu = emu::Chip8EmulatorBase::create(*this, emu::IChip8Emulator::eCHIP8MPT, _options, _chipEmu.get());
        //
        if(_chipEmu->getScreen())
            _chipEmu->getScreen();
        whenEmuChanged(*_chipEmu);
        //_debugger.updateCore(_chipEmu.get());
    }
}

bool Chip8EmuHostEx::loadRom(const char* filename, bool andRun)
{
    bool valid = false;
    std::error_code ec;
    std::unique_ptr<emu::OctoCompiler> c8c;
    std::string romSha1Hex;
    std::vector<uint8_t> romImage;
    if (strlen(filename) < 4095 && fs::exists(filename, ec)) {
        unsigned int size = 0;
        _customPalette = false;
#ifdef WITH_EDITOR
        _editor.setText("");
        _editor.setFilename("");
#endif
        auto fileData = loadFile(filename);
        auto isKnown = _librarian.isKnownFile(fileData.data(), fileData.size());
        //TraceLog(LOG_INFO, "Loading %s file with sha1: %s", isKnown ? "known" : "unknown", calculateSha1Hex(fileData.data(), fileData.size()).c_str());
        auto knownOptions = _librarian.getOptionsForFile(fileData.data(), fileData.size());
        if(endsWith(filename, ".8o")) {
            c8c = std::make_unique<emu::OctoCompiler>();
            if(c8c->compile(filename).resultType == emu::CompileResult::eOK)
            {
                if(c8c->codeSize() < _chipEmu->memSize() - 512) {
                    romImage.assign(c8c->code(), c8c->code() + c8c->codeSize());
                    romSha1Hex = c8c->sha1Hex();
#ifdef WITH_EDITOR
                    _editor.setText(loadTextFile(filename));
                    _editor.setFilename(filename);
                    _mainView = eEDITOR;
#endif
                    valid = true;
                }
            }
        }
        else if(isKnown) {
            if(_options.behaviorBase != emu::Chip8EmulatorOptions::ePORTABLE)
                updateEmulatorOptions(knownOptions);
            if(_options.advanced) {
                _options.updateColors(_colorPalette);
            }
            romImage = fileData;
            valid = true;
        }
        else if(endsWith(filename, ".ch8")) {
            auto estimate = _librarian.getEstimatedPresetForFile(_options.behaviorBase, fileData.data(), fileData.size());
            if(_options.behaviorBase != estimate)
                updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(estimate));
            if (size < _chipEmu->memSize() - 512) {
                romImage = fileData;
                valid = true;
            }
        }
        else if(endsWith(filename, ".ch10")) {
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP10));
            if (size < _chipEmu->memSize() - 512) {
                romImage = fileData;
                valid = true;
            }
        }
        else if(endsWith(filename, ".hc8")) {
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8VIP));
            if (size < _chipEmu->memSize() - 512) {
                romImage = fileData;
                valid = true;
            }
        }
        else if(endsWith(filename, ".c8tp")) {
            if (size < _chipEmu->memSize() - 512) {
                romImage = fileData;
                valid = true;
            }
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8VIP_TPD));
        }
        else if(endsWith(filename, ".sc8")) {
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eSCHIP11));
            if (size < _chipEmu->memSize() - 512) {
                romImage = fileData;
                valid = true;
            }
        }
        else if(endsWith(filename, ".mc8")) {
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eMEGACHIP));
            if (size < _chipEmu->memSize() - 512) {
                romImage = fileData;
                valid = true;
            }
        }
        else if(endsWith(filename, ".xo8")) {
            updateEmulatorOptions(Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eXOCHIP));
            if (size < _chipEmu->memSize() - 512) {
                romImage = fileData;
                valid = true;
            }
        }
        else if(endsWith(filename, ".c8b")) {
            C8BFile c8b;
            if(c8b.load(filename) == C8BFile::eOK) {
                uint16_t codeOffset = 0;
                uint16_t codeSize = 0;
                auto iter = c8b.findBestMatch({C8BFile::C8V_XO_CHIP, C8BFile::C8V_MEGA_CHIP, C8BFile::C8V_SCHIP_1_1, C8BFile::C8V_SCHIP_1_0, C8BFile::C8V_CHIP_48, C8BFile::C8V_CHIP_10, C8BFile::C8V_CHIP_8});
                if(iter != c8b.variantBytecode.end()) {
                    if(!c8b.palette.empty()) {
                        _customPalette = true;
                        auto numCol = std::min(c8b.palette.size(), (size_t)16);
                        for(size_t i = 0; i < numCol; ++i) {
                            _colorPalette[i] = be32(ColorToInt({c8b.palette[i].r, c8b.palette[i].g, c8b.palette[i].b, 255}));
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
                    if(codeSize < _chipEmu->memSize() - 512) {
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
            _romImage = std::move(romImage);
            _romSha1Hex = romSha1Hex.empty() ? calculateSha1Hex(_romImage.data(), _romImage.size()) : romSha1Hex;
            _romName = filename;
            _romIsWellKnown = isKnown;
            if(isKnown)
                _romWellKnownOptions = _options;
            _chipEmu->reset();
            std::memcpy(_chipEmu->memory() + 512, _romImage.data(), std::min(_romImage.size(),size_t(_chipEmu->memSize() - 512)));
            _chipEmu->removeAllBreakpoints();
            setPalette({
                be32(0x1a1c2cff), be32(0xf4f4f4ff), be32(0x94b0c2ff), be32(0x333c57ff),
                be32(0xb13e53ff), be32(0xa7f070ff), be32(0x3b5dc9ff), be32(0xffcd75ff),
                be32(0x5d275dff), be32(0x38b764ff), be32(0x29366fff), be32(0x566c86ff),
                be32(0xef7d57ff), be32(0x73eff7ff), be32(0x41a6f6ff), be32(0x257179ff)
            });
            if(_options.advanced)
                _options.updateColors(_colorPalette);
            _chipEmu->setPalette(_colorPalette);
            auto p = fs::path(_romName).parent_path();
            if(fs::exists(p) && fs::is_directory(p))  {
                _currentDirectory = fs::path(_romName).parent_path().string();
                _librarian.fetchDir(_currentDirectory);
            }
            std::string source;
#ifdef WITH_EDITOR
            if(_editor.isEmpty() && _romImage.size() < 65536) {
                std::stringstream os;
                emu::Chip8Decompiler decomp;
                decomp.setVariant(_options.presetAsVariant());
                decomp.decompile(filename, _romImage.data(), 0x200, _romImage.size(), 0x200, &os, false, true);
                source = os.str();
            }
#endif
            whenRomLoaded(fs::path(_romName).replace_extension(".8o").string(), andRun, c8c.get(), source);
        }
        //memory[0x1FF] = 3;
    }
    return valid;
}

}
