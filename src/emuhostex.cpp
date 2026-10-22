//---------------------------------------------------------------------------------------
// src/emuhostex.cpp
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

#include <algorithm>
#include <sstream>
#include <ghc/span.hpp>

namespace emu {

#ifdef CADMIUM_WITH_DATABASE
std::unique_ptr<Database> EmuHostEx::_database;
#endif

EmuHostEx::EmuHostEx(CadmiumConfiguration& cfg)
    : _cfg(cfg)
    , _librarian(_cfg)
#ifndef PLATFORM_WEB
    , _threadPool(6)
#endif
{
#ifndef PLATFORM_WEB
    _currentDirectory = _cfg.workingDirectory.empty() ? fs::current_path().string() : _cfg.workingDirectory;
    //_databaseDirectory = _cfg.libraryPath.front();
#ifdef CADMIUM_WITH_DATABASE
    if (!_instanceNum) {
        _database = std::make_unique<Database>(_cores, _cfg, _threadPool, dataPath());
    }
#endif
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

void EmuHostEx::setPalette(const std::vector<uint32_t>& colors, size_t offset, bool setAsDefault)
{
    if (_colorPalette.size() < colors.size() + offset)
        _colorPalette.colors.resize(colors.size() + offset);
    for (size_t i = 0; i < colors.size() && i + offset < _colorPalette.size(); ++i) {
        _colorPalette.colors[i + offset] = Palette::Color::fromRGBA(colors[i]);
    }
    if (setAsDefault) {
        _defaultPalette = _colorPalette;
    }
    if (_chipEmu) {
        if (_colorPalette.size() > _chipEmu->getMaxColors()) {
            _colorPalette.colors.resize(_chipEmu->getMaxColors());
        }
        _chipEmu->setPalette(_colorPalette);
    }
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
    auto [variantName, emuCore] = CoreRegistry::create(*this, properties, iother);
    if (emuCore) {
        _variantName = variantName;
        return std::move(emuCore);
    }
    return {};
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

void EmuHostEx::resetAllBreakpoints()
{
    if(!_chipEmu) return;
    for (auto& unit : *_chipEmu) {
        unit.removeAllBreakpoints();
    }
}

void EmuHostEx::updateEmulatorOptions(const Properties& properties)
{
    RequestScope scope(*this);
    requestConfig(properties);
    scope.finish();
}

void EmuHostEx::reloadProgram()
{
    RequestScope scope(*this);
    requestReload();
    scope.finish();
}

EmuHostEx::RequestScope::~RequestScope()
{
    if (!_finished && --_host._requestDepth == 0) {
        // The operation was aborted
        _host._pendingConfig.reset();
        _host._pendingProgram.reset();
        _host._pendingReload = false;
    }
}

bool EmuHostEx::RequestScope::finish()
{
    _finished = true;
    if (--_host._requestDepth == 0)
        return _host.applyRequests();
    return true;
}

void EmuHostEx::requestConfig(const Properties& properties)
{
    if (properties)
        _pendingConfig = properties;
}

void EmuHostEx::requestProgram(Program program)
{
    _pendingProgram = std::move(program);
}

void EmuHostEx::requestReload()
{
    _pendingReload = true;
}

void EmuHostEx::applyPendingConfig()
{
    if (!_pendingConfig)
        return;
    auto properties = std::move(*_pendingConfig);
    _pendingConfig.reset();
    // before the comparison, the running configuration has the rules applied too
    CoreRegistry::applyRules(properties);
    if (_previousProperties == properties && _chipEmu)
        return;
    if (!_properties || _properties->propertyClass().empty() || _properties->propertyClass() != properties.propertyClass()) {
        auto pIter = _propertiesByClass.find(properties.propertyClass());
        if (pIter == _propertiesByClass.end()) {
            pIter = _propertiesByClass.emplace(properties.propertyClass(), properties).first;
        }
        _properties = &pIter->second;
    }
    *_properties = properties;
    _previousProperties = properties;
    if (auto core = create(*_properties, _chipEmu.get())) {
        _chipEmu = std::move(core);
        _coreChanged = true;
        ++_lifecycleCounters.coreCreations;
    }
}

bool EmuHostEx::applyRequests()
{
    applyPendingConfig();
    if (!_chipEmu) {
        // Without a usable configuration (e.g. first start without config) fall back to a default core,
        // so a host always has a core after applying the requests.
        _pendingConfig = CoreRegistry::propertiesForPreset("chip-8");
        applyPendingConfig();
        if (!_chipEmu)
            throw std::runtime_error("Could not create the default emulation core");
    }
    // Take the requests out first, so requests made by the hooks below start a new round.
    auto program = std::exchange(_pendingProgram, std::nullopt);
    bool programOk = true;
    bool programChanged = false;
    if (program) {
        if (programFits(*program)) {
            adoptProgram(*program);
            programChanged = true;
        }
        else {
            TraceLog(LOG_WARNING, "Program `%s` does not fit into the selected core", program->name.c_str());
            programOk = false;
        }
    }
    const bool coreChanged = std::exchange(_coreChanged, false);
    const bool reload = std::exchange(_pendingReload, false);
    if (coreChanged || programChanged || reload) {
        _chipEmu->reset();
        ++_lifecycleCounters.resets;
        if (!_romImage.empty()) {
            if (loadRomImage())
                ++_lifecycleCounters.programLoads;
            else
                programOk = false;
        }
    }
    if (coreChanged || programChanged)
        syncPalette();
    if (coreChanged)
        whenEmuChanged(*_chipEmu);
    if (coreChanged || programChanged || reload)
        whenMachineReset();
    if (programChanged && _romOrigin != ProgramOrigin::EDITOR)
        whenRomLoaded(_romName, program->autoRun, program->compiler.get(), program->source);
    return programOk;
}

bool EmuHostEx::replaceProgramFromEditor(std::vector<uint8_t> image, const Sha1::Digest& sha1)
{
    RequestScope scope(*this);
    Program program;
    program.name = _romName;
    program.image = std::move(image);
    program.sha1 = sha1;
    program.isKnown = _romIsWellKnown;
    program.origin = ProgramOrigin::EDITOR;
    requestProgram(std::move(program));
    return scope.finish();
}

bool EmuHostEx::programFits(const Program& program) const
{
    const auto memSize = static_cast<uint64_t>(_chipEmu->memSize());
    if (program.hexFile) {
        return std::ranges::all_of(program.hexFile->ranges(), [memSize](const auto& range) {
            return range.first < memSize && range.second.size() <= memSize - range.first;
        });
    }
    if (program.image.empty())
        return false;
    const uint64_t address = program.loadAddress ? *program.loadAddress : Librarian::isPrefixedTPDRom(program.image) ? 0x200 : _chipEmu->defaultLoadAddress();
    return address < memSize && program.image.size() <= memSize - address;
}

void EmuHostEx::adoptProgram(Program& program)
{
    // Only the description of the program moves into the host, source and compiler stay
    // with the pending request until whenRomLoaded was notified.
    _romName = program.name;
    _romImage = std::move(program.image);
    _romHexFile = std::move(program.hexFile);
    _romLoadAddress = program.loadAddress;
    _romSha1 = program.sha1 == Sha1::Digest{} ? calculateSha1(_romImage) : program.sha1;
    _romIsWellKnown = program.isKnown;
    _romOrigin = program.origin;
    _romHasSource = !program.source.empty();
}

std::optional<EmuHostEx::DisassemblyKey> EmuHostEx::disassemblyKey() const
{
    if (_romImage.empty() || _romHasSource || _romOrigin == ProgramOrigin::EDITOR || !_chipEmu)
        return {};
    DisassemblyKey key;
    if (_romHexFile) {
        auto* chip8 = _chipEmu->chip8Core();
        if (!chip8)
            return {};
        key.variants = chip8->supportedChip8Variants();
        key.address = _romHexFile->ranges().front().first;
    }
    else {
        if (_romImage.size() >= 8192 * 1024)
            return {};
        auto startAddress = _properties->get<Property::Integer>(propid::START_ADDRESS);
        auto chip8Emu = dynamic_cast<emu::IChip8Emulator*>(_chipEmu->executionUnit(0));
        key.variants = chip8Emu ? chip8Emu->supportedChip8Variants() : chip8::Variant::NONE;
        key.address = startAddress ? startAddress->intValue : 0;
    }
    key.sha1 = _romSha1;
    return key;
}

std::optional<EmuHostEx::DisassemblyRequest> EmuHostEx::disassemblyRequest() const
{
    auto key = disassemblyKey();
    if (!key)
        return {};
    return DisassemblyRequest{*key, _romName, _romImage};
}

std::string EmuHostEx::disassemble(const DisassemblyRequest& request)
{
    std::scoped_lock decompilerLock(Librarian::decompilerMutex());
    std::stringstream os;
    Chip8Decompiler decomp{request.image, request.key.address};
    decomp.setVariants(request.key.variants, true);
    TraceLog(LOG_INFO, "About to decompile...");
    decomp.decompile(request.name, request.key.address, &os, false, true);
    return os.str();
}

Palette EmuHostEx::completedPalette(Palette palette) const
{
    const auto targetSize = static_cast<size_t>(_chipEmu->getMaxColors());
    const auto currentSize = palette.colors.size();
    if (currentSize < targetSize) {
        palette.colors.reserve(targetSize);
        for (size_t i = currentSize; i < targetSize; ++i) {
            palette.colors.push_back(i < _defaultPalette.colors.size() ? _defaultPalette.colors[i] : Palette::Color{0, 0, 0});
        }
    }
    else if (currentSize > targetSize) {
        palette.colors.resize(targetSize);
    }
    return palette;
}

void EmuHostEx::syncPalette()
{
    // The palette always has exactly as many colors as the core supports, missing ones come from the default palette.
    auto palette = completedPalette(_properties->palette());
    _properties->palette() = palette;
    _colorPalette = palette;
    _chipEmu->setPalette(_colorPalette);
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

bool EmuHostEx::loadRom(std::string_view filename, LoadOptions loadOpt)
{
    std::error_code ec;
    if (filename.size() < 4095 && fs::exists(filename, ec)) {
        if (fs::path(filename).extension() == ".hex")
            return loadHexFile(filename, loadOpt);
        _customPalette = false;
        // TODO: Refactor this
        if (filename.ends_with(".gif")) {
            auto cart = emu::OctoCartridge(std::string(filename));
            if (cart.loadCartridge()) {
                RequestScope scope(*this);
                requestConfig(CoreRegistry::propertiesFromOctoOptions(cart.getOptions()));
                applyPendingConfig();
                auto source = cart.getSource();
                auto c8c = std::make_shared<emu::OctoCompiler>();
                if (c8c->compile(filename, source).resultType == emu::CompileResult::eOK) {
                    Program program;
                    program.name = filename;
                    program.image.assign(c8c->code(), c8c->code() + c8c->codeSize());
                    program.sha1 = c8c->sha1();
                    program.loadAddress = _chipEmu->defaultLoadAddress();
                    program.autoRun = loadOpt & LoadOptions::SetToRun;
                    program.source = source;
                    program.compiler = c8c;
                    requestProgram(std::move(program));
                    if (scope.finish())
                        return true;
                }
                else {
                    scope.finish();
                }
                _romName = filename;
                _romOrigin = ProgramOrigin::FILE;
                whenRomLoaded(_romName, false, c8c.get(), source);
                return true;
            }
        }
        auto fileData = loadFile(filename, Librarian::MAX_ROM_SIZE);
        if (fileData) {
            resetAllBreakpoints();
            return loadBinary(filename, *fileData, loadOpt);
        }
    }
    return false;
}

bool EmuHostEx::loadHexFile(std::string_view filename, LoadOptions loadOpt, std::string_view sourcePath)
{
    try {
        HexFile hex(std::string(sourcePath.empty() ? filename : sourcePath), HexFile::Mode::READ);
        if (hex.ranges().empty())
            return false;
        const auto digest = hex.sha1Digest();
        const bool isKnown = _librarian.isKnownFile(digest);
        RequestScope scope(*this);
        if (!(loadOpt & DontChangeOptions) && isKnown && !_librarian.isGenericChip8(digest))
            requestConfig(_librarian.getPropertiesForFile(digest));

        // Keep a canonical image for the editor and binary export. Only the
        // actual HEX ranges are written to memory, including on reload.
        Program program;
        const auto firstAddress = hex.ranges().front().first;
        const auto& [lastAddress, lastData] = hex.ranges().back();
        program.image.assign(lastAddress + lastData.size() - firstAddress, 0);
        for (const auto& [address, data] : hex.ranges())
            std::copy(data.begin(), data.end(), program.image.begin() + address - firstAddress);
        program.hexFile.emplace(std::move(hex));
        program.name = filename;
        program.sha1 = digest;
        program.isKnown = isKnown;
        program.autoRun = loadOpt & SetToRun;
        _customPalette = false;
        resetAllBreakpoints();
        requestProgram(std::move(program));
        if (!scope.finish())
            throw std::runtime_error("HEX data exceeds the selected core's memory");
        return true;
    }
    catch (const std::exception& error) {
        TraceLog(LOG_WARNING, "Could not load HEX file `%s`: %s", std::string(filename).c_str(), error.what());
        return false;
    }
}

bool EmuHostEx::loadRomImage()
{
    if (_romHexFile) {
        for (const auto& [address, data] : _romHexFile->ranges()) {
            if (address >= _chipEmu->memSize() || data.size() > _chipEmu->memSize() - address)
                return false;
        }
        for (const auto& [address, data] : _romHexFile->ranges()) {
            if (!_chipEmu->loadData(data, address))
                return false;
        }
        return true;
    }
    if (_romLoadAddress)
        return _chipEmu->loadData(_romImage, *_romLoadAddress);
    return _chipEmu->loadData(_romImage, Librarian::isPrefixedTPDRom(_romImage)
                                              ? 0x200 : _chipEmu->defaultLoadAddress());
}

#if 0
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
#endif

bool EmuHostEx::loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, LoadOptions loadOpt)
{
    if (fs::path(filename).extension() == ".hex") {
        // HexFile currently reads from a path. Stage browser uploads in the
        // temporary filesystem, then retain the parsed ranges for reload.
        std::error_code error;
        const auto directory = fs::temp_directory_path(error);
        if (error)
            return false;
        const auto path = directory / fmt::format("cadmium-hex-{}-{}.hex", _instanceNum,
                                                  std::chrono::steady_clock::now().time_since_epoch().count());
        if (!writeFile(path.string(), binary.data(), binary.size()))
            return false;
        const bool loaded = loadHexFile(filename, loadOpt, path.string());
        fs::remove(path, error);
        return loaded;
    }
    auto fileData = std::vector(binary.data(), binary.data() + binary.size());
    auto isKnown = _librarian.isKnownFile(fileData);
    auto isGenericChip8 = _librarian.isGenericChip8(fileData);
    TraceLog(LOG_INFO, "Loading %s file with sha1: %s", isKnown ? "known" : "unknown", calculateSha1(fileData).to_hex().c_str());
    RequestScope scope(*this);
    Program program;
    program.name = filename;
    program.autoRun = loadOpt & LoadOptions::SetToRun;
    if (endsWith(filename, ".8o")) {
        program.source.assign((const char*)fileData.data(), fileData.size());
        auto c8c = std::make_shared<emu::OctoCompiler>();
        if (c8c->compile(filename).resultType != emu::CompileResult::eOK) {
            _romName = filename;
            _romOrigin = ProgramOrigin::FILE;
            whenRomLoaded(_romName, false, c8c.get(), program.source);
            return true;
        }
        program.image.assign(c8c->code(), c8c->code() + c8c->codeSize());
        program.sha1 = c8c->sha1();
        program.loadAddress = _chipEmu->defaultLoadAddress();
        program.compiler = c8c;
        if ((loadOpt & DontChangeOptions) == 0) {
            isKnown = _librarian.isKnownFile(program.image);
        }
    }
    else {
        if (Librarian::isPrefixedTPDRom(binary))
            program.loadAddress = 0x200;
        if ((loadOpt & DontChangeOptions) == 0) {
            if (isKnown) {
                if (!isGenericChip8)
                    requestConfig(_librarian.getPropertiesForFile(fileData));
            }
            else if (auto extensionProps = CoreRegistry::propertiesForExtension(fs::path(filename).extension().string())) {
                requestConfig(extensionProps);
            }
            else {
                return false; // unknown file type
            }
        }
        program.image = std::move(fileData);
    }
    program.isKnown = isKnown;
    requestProgram(std::move(program));
    return scope.finish();
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
        const auto loadAddress = Librarian::isPrefixedTPDRom(_romImage.data(), _romImage.size())
            ? 512U
            : _chipEmu->defaultLoadAddress();
        if(!_chipEmu->loadData(_romImage, loadAddress))
            return false;
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
}

bool EmuHostEx::loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, const Properties& props, const bool isKnown)
{
    RequestScope scope(*this);
    if (props)
        requestConfig(props);
    if (fs::path(filename).extension() == ".hex") {
        if (!loadBinary(filename, binary, DontChangeOptions))
            return false;
        return scope.finish();
    }
    _customPalette = false;
    Program program;
    program.name = filename;
    program.image.assign(binary.data(), binary.data() + binary.size());
    program.isKnown = isKnown;
    program.origin = ProgramOrigin::DATABASE;
    if (auto startAddress = props.get<Property::Integer>(propid::START_ADDRESS))
        program.loadAddress = startAddress->intValue;
    if (isKnown) {
        _romWellKnownProperties = props;
    }
    requestProgram(std::move(program));
    return scope.finish();
}

#ifdef CADMIUM_WITH_BACKGROUND_EMULATION
ThreadedBackgroundHost::ThreadedBackgroundHost(CadmiumConfiguration& cfg, double initialFrameRate)
: EmuHostEx(cfg)
, _workerThread(&ThreadedBackgroundHost::worker, this)
{
    setFrameRate(initialFrameRate);
    _screen = GenImageColor(emu::SUPPORTED_SCREEN_WIDTH, emu::SUPPORTED_SCREEN_HEIGHT, BLACK);
}

ThreadedBackgroundHost::ThreadedBackgroundHost(CadmiumConfiguration& cfg, const Properties& options, double initialFrameRate)
    : EmuHostEx(cfg)
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
    UnloadImage(_screen);
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
    ImageClearBackground(&_screen, BLACK);
}


void ThreadedBackgroundHost::updateEmulatorOptions(const Properties& properties)
{
    std::unique_lock guard(_mutex);
    EmuHostEx::updateEmulatorOptions(properties);
}

bool ThreadedBackgroundHost::applyRequests()
{
    std::unique_lock guard(_mutex);
    return EmuHostEx::applyRequests();
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
        auto ratio = _chipEmu->getScreen() ? _chipEmu->getScreen()->ratio() : (_chipEmu->getScreenRGBA() ? _chipEmu->getScreenRGBA()->ratio() : 1);
        auto videoScaleY = ratio ? videoScaleX / ratio : videoScaleX;
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
    // REVIEW: converting here instead of in vblank() also shows what was drawn after the last vblank,
    // e.g. before a program ends in a jump-to-self that pauses the core and stops further vblanks.
    // The worker holds the mutex for a whole executeFrame(), so this always sees a state between frames.
    std::unique_lock guard(_mutex);
    if (_chipEmu) {
        if (auto* pixel = static_cast<uint32_t*>(_screen.data)) {
            if (const auto* screen = _chipEmu->getScreen()) {
                screen->convert(pixel, _screen.width, 255, nullptr);
            }
            else {
                if (const auto* screenRgb = _chipEmu->getScreenRGBA()) {
                    screenRgb->convert(pixel, _screen.width, _chipEmu->getScreenAlpha(), _chipEmu->getWorkRGBA());
                }
            }
        }
    }
    if (!_screenTexture.id)
        _screenTexture = LoadTextureFromImage(_screen);
    UpdateTexture(_screenTexture, _screen.data);
    return {&_screenTexture, {0, 0, 0, 0}};
}

bool ThreadedBackgroundHost::loadBinary(std::string_view filename, ghc::span<const uint8_t> binary, const Properties& props, const bool isKnown)
{
    std::unique_lock guard(_mutex);
    auto rc = EmuHostEx::loadBinary(filename, binary, props, isKnown);
    auto frameRate = props.get<Property::Integer>(propid::FRAME_RATE);
    setFrameRate(frameRate ? frameRate->intValue : 60);
    return rc;
}
#endif

}
