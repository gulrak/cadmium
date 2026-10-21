//---------------------------------------------------------------------------------------
// src/emulation/hp48sx.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2026, Steffen Schümann <s.schuemann@pobox.com>
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

#include <emulation/hp48sx.hpp>

#include <emulation/hardware/hp48/binary.hpp>
#include <emulation/hardware/hp48/g48sxmachine.hpp>
#include <emulation/hardware/hp48/asacpp/asacpp.hpp>
#include <resourcemanager.hpp>

#include <algorithm>
#include <fstream>
#include <span>
#include <stdexcept>
#include <stdendian/stdendian.h>

namespace emu {
namespace {

constexpr std::string_view PROPERTY_CLASS = "HP-48SX";
constexpr std::string_view PROP_INTERPRETER = "interpreter";
constexpr std::string_view PROP_ROM = "rom";
constexpr std::string_view PROP_SOURCE = "interpreterSource";
constexpr std::string_view PROP_FETCH = "fetchSymbol";
constexpr std::string_view PROP_SYMBOL_BIAS = "symbolAddressBias";
constexpr std::string_view PROP_TIMER = "timerSymbol";
constexpr std::string_view PROP_SOUND = "soundSymbol";
constexpr std::string_view PROP_STACK_POINTER = "stackPointerSymbol";
constexpr std::string_view PROP_STACK = "stackSymbol";
constexpr std::string_view PROP_KEY_TABLE = "keyTableSymbol";
constexpr std::string_view PROP_FIXED_CONTRAST = "fixedContrast";
constexpr std::string_view PROP_CONTRAST = "contrast";

enum class Interpreter {
    NONE,
    CHIP_48,
    SCHIP_1_0,
    SCHIP_1_1,
    SCHPC,
};

constexpr std::string_view INTERPRETER_FOLDER = "hp-chip8";

std::vector<uint8_t> readFile(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Unable to open HP-48 asset: " + path);
    }
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::vector<uint8_t> loadAsset(const std::string& name, std::string_view folder = {})
{
    auto& resources = ResourceManager::instance();
    const auto fileName = ghc::filesystem::path(name).filename().string();
    for (const auto& candidate : {name, folder.empty() ? fileName : std::string(folder) + "/" + fileName}) {
        if (resources.resourceAvailable(candidate)) {
            const auto resource = resources.resourceForName(candidate);
            return {resource.data(), resource.data() + resource.size()};
        }
    }
    return readFile(name);
}

chip8::VariantSet variantFor(Interpreter interpreter)
{
    switch (interpreter) {
        case Interpreter::CHIP_48:
            return chip8::Variant::CHIP_48;
        case Interpreter::SCHIP_1_0:
            return chip8::Variant::SCHIP_1_0;
        case Interpreter::SCHIP_1_1:
            return chip8::Variant::SCHIP_1_1;
        case Interpreter::SCHPC:
            return chip8::Variant::SCHIPC;
        case Interpreter::NONE:
            return chip8::Variant::NONE;
    }
    return chip8::Variant::NONE;
}

struct Hp48sxOptions
{
    Interpreter interpreter{Interpreter::NONE};
    std::string rom{"sxrom-rev-e"};
    std::string source;
    std::string fetchSymbol{"dispatch"};
    int symbolAddressBias{-16};
    std::string timerSymbol;
    std::string soundSymbol;
    std::string stackPointerSymbol;
    std::string stackSymbol;
    std::string keyTableSymbol{"keytab"};
    bool fixedContrast{false};
    int contrast{hp48::Hp48Display::DEFAULT_CONTRAST};

    static Properties& registeredPrototype()
    {
        using namespace std::string_literals;
        auto& prototype = Properties::getProperties(PROPERTY_CLASS);
        if (!prototype) {
            prototype.registerProperty({std::string(PROP_INTERPRETER), "Interpreter", Property::Combo{"NONE"s, "CHIP-48"s, "SCHIP-1.0"s, "SCHIP-1.1"s, "SCHPC"s}, "HP-48 interpreter preset"});
            prototype.registerProperty({std::string(PROP_ROM), "ROM", "sxrom-rev-e"s, "HP-48SX ROM asset"});
            // The calculator settings (ON + [+]/[-]) can't be reached without its keyboard, so this replaces them
            prototype.registerProperty({std::string(PROP_FIXED_CONTRAST), "Fixed contrast", false, "Use the Contrast setting instead of the contrast the calculator sets", PropertyFlags::eWritable});
            prototype.registerProperty({std::string(PROP_CONTRAST), "Contrast", Property::Integer{hp48::Hp48Display::DEFAULT_CONTRAST, hp48::Hp48Display::MIN_CONTRAST, hp48::Hp48Display::MAX_CONTRAST}, "Display contrast (0-31, the calculator default is 11)", PropertyFlags::eWritable});
            prototype.registerProperty({std::string(PROP_SOURCE), "Interpreter Source", ""s, "ASAP source asset", PropertyFlags::eInvisible});
            prototype.registerProperty({std::string(PROP_FETCH), "Fetch Symbol", "dispatch"s, "CHIP-8 fetch loop symbol", PropertyFlags::eInvisible});
            prototype.registerProperty({std::string(PROP_SYMBOL_BIAS), "Symbol Address Bias", Property::Integer{-16, -65536, 65536}, "Nibble bias from ASAP symbols to the loaded HP object", PropertyFlags::eInvisible});
            prototype.registerProperty({std::string(PROP_TIMER), "Timer Symbol", ""s, "Delay timer offset symbol", PropertyFlags::eInvisible});
            prototype.registerProperty({std::string(PROP_SOUND), "Sound Symbol", ""s, "Sound timer offset symbol", PropertyFlags::eInvisible});
            prototype.registerProperty({std::string(PROP_STACK_POINTER), "Stack Pointer Symbol", ""s, "Stack pointer offset symbol", PropertyFlags::eInvisible});
            prototype.registerProperty({std::string(PROP_STACK), "Stack Symbol", ""s, "Stack offset symbol", PropertyFlags::eInvisible});
            prototype.registerProperty({std::string(PROP_KEY_TABLE), "Key Table Symbol", "keytab"s, "Keyboard mapping symbol", PropertyFlags::eInvisible});
        }
        return prototype;
    }

    Properties asProperties() const
    {
        auto properties = registeredPrototype();
        properties[std::string(PROP_INTERPRETER)].setSelectedIndex(static_cast<size_t>(interpreter));
        properties[std::string(PROP_ROM)].setString(rom);
        properties[std::string(PROP_SOURCE)].setString(source);
        properties[std::string(PROP_FETCH)].setString(fetchSymbol);
        properties[std::string(PROP_SYMBOL_BIAS)].setInt(symbolAddressBias);
        properties[std::string(PROP_TIMER)].setString(timerSymbol);
        properties[std::string(PROP_SOUND)].setString(soundSymbol);
        properties[std::string(PROP_STACK_POINTER)].setString(stackPointerSymbol);
        properties[std::string(PROP_STACK)].setString(stackSymbol);
        properties[std::string(PROP_KEY_TABLE)].setString(keyTableSymbol);
        properties[std::string(PROP_FIXED_CONTRAST)].setBool(fixedContrast);
        properties[std::string(PROP_CONTRAST)].setInt(contrast);
        return properties;
    }

    static Hp48sxOptions fromProperties(const Properties& properties)
    {
        Hp48sxOptions options;
        options.interpreter = static_cast<Interpreter>(properties[std::string(PROP_INTERPRETER)].getSelectedIndex());
        options.rom = properties[std::string(PROP_ROM)].getString();
        options.source = properties[std::string(PROP_SOURCE)].getString();
        options.fetchSymbol = properties[std::string(PROP_FETCH)].getString();
        options.symbolAddressBias = properties[std::string(PROP_SYMBOL_BIAS)].getInt();
        options.timerSymbol = properties[std::string(PROP_TIMER)].getString();
        options.soundSymbol = properties[std::string(PROP_SOUND)].getString();
        options.stackPointerSymbol = properties[std::string(PROP_STACK_POINTER)].getString();
        options.stackSymbol = properties[std::string(PROP_STACK)].getString();
        options.keyTableSymbol = properties[std::string(PROP_KEY_TABLE)].getString();
        options.fixedContrast = properties[std::string(PROP_FIXED_CONTRAST)].getBool();
        options.contrast = properties[std::string(PROP_CONTRAST)].getInt();
        return options;
    }
};

using SetupInfo = CoreRegistry::SetupInfo<Hp48sxOptions>;
const std::array<SetupInfo, 5> PRESETS{{
    {"NONE", "Raw HP-48SX", ".hp;.hp48", chip8::Variant::NONE, {.interpreter = Interpreter::NONE}},
    {"CHIP-48",
     "Original CHIP-48 on HP-48SX",
     ".ch48;.c48",
     chip8::Variant::CHIP_48,
     {.interpreter = Interpreter::CHIP_48, .source = "hp-chip8/chip48.asap", .timerSymbol = "ofs_timer", .soundSymbol = "ofs_sound", .stackPointerSymbol = "ofs_csp", .stackSymbol = "ofs_cstack"}},
    {"SCHIP-1.0",
     "SUPER-CHIP 1.0 on HP-48SX",
     ".sc10",
     chip8::Variant::SCHIP_1_0,
     {.interpreter = Interpreter::SCHIP_1_0, .source = "hp-chip8/schip10.asap", .symbolAddressBias = 10, .timerSymbol = "o_timer", .soundSymbol = "o_sound", .stackPointerSymbol = "o_csp", .stackSymbol = "o_cstack"}},
    {"SCHIP-1.1",
     "SUPER-CHIP 1.1 on HP-48SX",
     ".sc8;.sc11",
     chip8::Variant::SCHIP_1_1,
     {.interpreter = Interpreter::SCHIP_1_1, .source = "hp-chip8/schip11.asap", .symbolAddressBias = 10, .timerSymbol = "o_timer", .soundSymbol = "o_sound", .stackPointerSymbol = "o_csp", .stackSymbol = "o_cstack"}},
    {"SCHPC",
     "SUPER-CHIP compatibility interpreter on HP-48SX",
     ".sc8;.scc",
     chip8::Variant::SCHIPC,
     {.interpreter = Interpreter::SCHPC, .source = "hp-chip8/schpc.asap", .symbolAddressBias = 10, .timerSymbol = "o_timer", .soundSymbol = "o_sound", .stackPointerSymbol = "o_csp", .stackSymbol = "o_cstack"}},
}};

struct Factory final : CoreRegistry::FactoryInfo<Hp48sxEmulationCore, SetupInfo, Hp48sxOptions>
{
    Factory()
        : FactoryInfo(140, PRESETS, "Hardware emulation of an HP-48SX")
    {
    }
    std::string prefix() const override { return "HP48"; }
    VariantIndex variantIndex(const Properties& properties) const override
    {
        const auto index = properties[std::string(PROP_INTERPRETER)].getSelectedIndex();
        return {index, PRESETS[index].options.asProperties() == properties};
    }
    void applyRules(Properties& props, std::string_view) const override
    {
        props.at(std::string(PROP_CONTRAST)).setState(props[std::string(PROP_FIXED_CONTRAST)].getBool() ? PropertyState::ACTIVE : PropertyState::INACTIVE);
    }
};

const bool REGISTERED = CoreRegistry::registerFactory(std::string(PROPERTY_CLASS), std::make_unique<Factory>());

uint32_t readPacked(const hp48::G48sxMachine& machine, uint32_t address, unsigned nibbles)
{
    uint32_t result = 0;
    for (unsigned i = 0; i < nibbles; ++i) {
        result |= static_cast<uint32_t>(machine.readNibble(address + i)) << (i * 4U);
    }
    return result;
}

}  // namespace

struct Hp48sxEmulationCore::Private
{
    Private(Properties& properties)
        : properties(properties)
        , options(Hp48sxOptions::fromProperties(properties))
        , memory(4096)
        , syncedMemory(4096)
    {
        (void)REGISTERED;
        auto rom = loadAsset(options.rom);
        machine = std::make_unique<hp48::G48sxMachine>(hp48::G48SX_RECOVERY_RESET_KEY_HOLD_INSTRUCTIONS, std::move(rom), "Cadmium HP-48SX", hp48::G48sxTimingProfile::COMPARE);
        powerOnState = machine->saveStateBytes(false);
        idleStack.push(std::string("No program loaded"));
        applyContrast();
    }

    void applyContrast()
    {
        const auto fixed = options.fixedContrast ? std::optional<int>(options.contrast) : std::nullopt;
        machine->setFixedContrast(fixed);
        idleDisplay.setContrast(fixed.value_or(hp48::Hp48Display::DEFAULT_CONTRAST));
    }

    Properties& properties;
    Hp48sxOptions options;
    std::unique_ptr<hp48::G48sxMachine> machine;
    std::vector<uint8_t> memory;
    std::vector<uint8_t> syncedMemory;
    std::vector<uint8_t> startState;
    std::vector<uint8_t> powerOnState;
    std::vector<uint8_t> loadedData;
    std::array<uint32_t, 8> startReturnStack{};
    uint16_t startCrc{};
    std::array<uint8_t, 32> stackBytes{};
    std::array<bool, 16> keyStates{};
    uint32_t fetchAddress{};
    uint32_t keyTableAddress{};
    uint32_t timerOffset{};
    uint32_t soundOffset{};
    uint32_t stackPointerOffset{};
    uint32_t stackOffset{};
    uint32_t backendStepOverSP{};
    bool loaded{};
    std::array<uint8_t, 64 * 17> idleFrameBuffer{};
    hp48::ObjectStack idleStack;
    std::vector<std::string> idleMenu{"EDIT", "VIEW", "RCL", "STO", "PURGE", "CLEAR"};
    hp48::Hp48Display idleDisplay{{idleFrameBuffer.data(), idleFrameBuffer.size()}, idleStack, idleMenu};
};

Hp48sxEmulationCore::Hp48sxEmulationCore(EmulatorHost& host, Properties& properties, IEmulationCore* other)
    : Chip8RealCoreBase(host)
    , _impl(std::make_unique<Private>(properties))
{
    _execChip8 = _impl->options.interpreter != Interpreter::NONE;
    _isHybridChipMode = _execChip8;
    if (other) {
        if (auto chip8 = other->chip8Core()) {
            swapBreakpoints(*chip8);
        }
        if (other->numberOfExecutionUnits() > 1) {
            _impl->machine->cpu().swapBreakpoints(*other->executionUnit(1));
        }
    }
    setExecMode(ePAUSED);
}

Hp48sxEmulationCore::~Hp48sxEmulationCore() = default;

bool Hp48sxEmulationCore::updateProperties(Properties& props, Property& changed)
{
    // The contrast is only a matter of rendering, so it is applied live
    if (changed.getId() == PROP_FIXED_CONTRAST || changed.getId() == PROP_CONTRAST) {
        _impl->options = Hp48sxOptions::fromProperties(props);
        _impl->applyContrast();
        return false;
    }
    return true;
}
std::string Hp48sxEmulationCore::name() const
{
    return "HP-48SX";
}
chip8::VariantSet Hp48sxEmulationCore::supportedChip8Variants() const
{
    return variantFor(_impl->options.interpreter);
}
size_t Hp48sxEmulationCore::numberOfExecutionUnits() const
{
    return _impl->options.interpreter == Interpreter::NONE ? 1 : 2;
}

GenericCpu* Hp48sxEmulationCore::executionUnit(size_t index)
{
    if (index >= numberOfExecutionUnits())
        return nullptr;
    if (numberOfExecutionUnits() == 1)
        return &_impl->machine->cpu();
    return index == 0 ? static_cast<GenericCpu*>(this) : static_cast<GenericCpu*>(&_impl->machine->cpu());
}

void Hp48sxEmulationCore::setFocussedExecutionUnit(GenericCpu* unit)
{
    _execChip8 = numberOfExecutionUnits() > 1 && dynamic_cast<IChip8Emulator*>(unit);
}

GenericCpu* Hp48sxEmulationCore::focussedExecutionUnit()
{
    return _execChip8 ? static_cast<GenericCpu*>(this) : static_cast<GenericCpu*>(&_impl->machine->cpu());
}

uint32_t Hp48sxEmulationCore::defaultLoadAddress() const
{
    return _impl->options.interpreter == Interpreter::NONE ? 0 : 0x200;
}

bool Hp48sxEmulationCore::loadData(std::span<const uint8_t> data, std::optional<uint32_t>)
{
    _errorMessage.clear();
    try {
        if (data.empty() && _impl->options.interpreter != Interpreter::NONE) {
            _impl->startState.clear();
            _impl->loadedData.clear();
            restoreStartState();
            return true;
        }
        // Reuse prepared machine state instead of assembling and bootstrapping it again.
        if (!_impl->startState.empty() && std::ranges::equal(data, _impl->loadedData)) {
            restoreStartState();
            return true;
        }
        _impl->startState.clear();
        _impl->loadedData.clear();
        _impl->loaded = false;
        _impl->keyStates.fill(false);
        _impl->machine->loadState(_impl->powerOnState);
        _impl->machine->setTimingProfile(hp48::G48sxTimingProfile::COMPARE);
        const bool prepared = _impl->options.interpreter == Interpreter::NONE
            ? prepareRawObject(data)
            : prepareInterpreter(data);
        if (prepared) {
            captureStartState();
            _impl->loadedData.assign(data.begin(), data.end());
            restoreStartState();
        }
        return prepared;
    }
    catch (const std::exception& error) {
        _cpuState = eERROR;
        _errorMessage = error.what();
        setExecMode(ePAUSED);
        return false;
    }
}

bool Hp48sxEmulationCore::prepareRawObject(std::span<const uint8_t> object)
{
    hp48::Binary binary(object);
    const std::array objects{hp48::G48sxStackObject::hpObject(binary)};
    _impl->machine->prepareRecoveredStackForEval(objects);
    _impl->machine->setTimingProfile(hp48::G48sxTimingProfile::REAL);
    _impl->loaded = true;
    _cpuState = eNORMAL;
    setExecMode(_host.isHeadless() ? eRUNNING : ePAUSED);
    return true;
}

bool Hp48sxEmulationCore::prepareInterpreter(std::span<const uint8_t> program)
{
    if (program.size() > _impl->memory.size() - defaultLoadAddress()) {
        throw std::runtime_error("CHIP-8 program is too large for the HP-48 interpreter");
    }
    AsaCpp assembler;
    const auto source = loadAsset(_impl->options.source, INTERPRETER_FOLDER);
    assembler.compile(_impl->options.source, std::span<const char>(reinterpret_cast<const char*>(source.data()), source.size()));
    if (assembler.isError()) {
        throw std::runtime_error("Unable to assemble HP-48 interpreter: " + assembler.compileResult().errorMessage);
    }
    auto required = [&](const std::string& symbol) -> uint32_t {
        const auto value = assembler.definedInteger(symbol);
        if (!value || *value < 0)
            throw std::runtime_error("Missing HP-48 interpreter symbol: " + symbol);
        return static_cast<uint32_t>(*value);
    };
    const uint32_t fetchOffset = required(_impl->options.fetchSymbol);
    const uint32_t keyTableOffset = required(_impl->options.keyTableSymbol);
    const bool isChip48 = _impl->options.interpreter == Interpreter::CHIP_48;
    const uint32_t restartOffset = isChip48 ? required("restart") : 0;
    const uint32_t controlKeysOffset = isChip48 ? required("ofs_ckeys") : 0;
    _impl->timerOffset = required(_impl->options.timerSymbol);
    _impl->soundOffset = required(_impl->options.soundSymbol);
    _impl->stackPointerOffset = required(_impl->options.stackPointerSymbol);
    _impl->stackOffset = required(_impl->options.stackSymbol);

    hp48::Binary interpreter(assembler.code());
    const auto interpreterObject = hp48::G48sxStackObject::hpObject(interpreter);
    const std::array objects{
        hp48::G48sxStackObject::stringObject(program),
        interpreterObject,
    };
    const auto addresses = _impl->machine->prepareRecoveredStackForEvalWithAddresses(objects);
    _impl->machine->setTimingProfile(hp48::G48sxTimingProfile::REAL);
    uint32_t restartAddress{};
    const auto setInterpreterAddress = [&](uint32_t address) {
        const int64_t base = static_cast<int64_t>(address) + _impl->options.symbolAddressBias;
        if (base < 0 || base + fetchOffset > 0xfffff || base + keyTableOffset > 0xfffff || (isChip48 && base + restartOffset > 0xfffff)) {
            throw std::runtime_error("HP-48 interpreter symbols resolve outside the Saturn address space");
        }
        _impl->fetchAddress = static_cast<uint32_t>(base + fetchOffset);
        _impl->keyTableAddress = static_cast<uint32_t>(base + keyTableOffset);
        restartAddress = static_cast<uint32_t>(base + restartOffset);
    };
    setInterpreterAddress(addresses[1]);
    constexpr uint64_t ON_DELAY_INSTRUCTIONS = 1'000;
    constexpr uint64_t EVAL_DELAY_INSTRUCTIONS = 100'000;
    constexpr uint64_t KEY_HOLD_INSTRUCTIONS = 20'000;
    constexpr uint64_t STARTUP_INSTRUCTION_LIMIT = 11'000'000;
    const auto runInterpreter = [&] {
        const uint64_t startupBase = _impl->machine->instructionIndex();
        _impl->machine->scheduleKeyPress("ON", startupBase + ON_DELAY_INSTRUCTIONS, KEY_HOLD_INSTRUCTIONS);
        _impl->machine->scheduleKeyPress("EVAL", startupBase + EVAL_DELAY_INSTRUCTIONS, KEY_HOLD_INSTRUCTIONS);
        const uint64_t deadline = startupBase + STARTUP_INSTRUCTION_LIMIT;
        while (_impl->machine->running() && _impl->machine->instructionIndex() < deadline && _impl->machine->cpu().getPC() != _impl->fetchAddress) {
            if (isChip48 && _impl->machine->cpu().getPC() == restartAddress) {
                // CHIP-48 does not initialize its key-release mask. Stale
                // calculator RAM here can abort before the first dispatch.
                const auto variables = static_cast<uint32_t>(_impl->machine->cpu().getState().r4 & 0xfffffU);
                for (uint32_t nibble = 0; nibble < 4; ++nibble)
                    _impl->machine->writeNibble(variables + controlKeysOffset + nibble, 0);
            }
            _impl->machine->stepInstruction();
        }
        return _impl->machine->cpu().getPC() == _impl->fetchAddress;
    };
    if (!runInterpreter() && _impl->machine->running()) {
        const std::array retryObjects{interpreterObject};
        const auto retryAddresses = _impl->machine->pushStackObjectsForEval(retryObjects);
        setInterpreterAddress(retryAddresses.front());
        runInterpreter();
    }
    if (!_impl->machine->running()) {
        throw std::runtime_error(_impl->machine->errorMessage().empty() ? "HP-48 interpreter stopped during startup" : _impl->machine->errorMessage());
    }
    if (_impl->machine->cpu().getPC() != _impl->fetchAddress) {
        throw std::runtime_error("HP-48 interpreter did not reach its fetch loop");
    }
    _impl->loaded = true;
    _cpuState = eNORMAL;
    _cycles = 0;
    fetchState();
    setExecMode(_host.isHeadless() ? eRUNNING : ePAUSED);
    return true;
}

void Hp48sxEmulationCore::handleReset(bool)
{
    try {
        if (_impl->startState.empty() && _impl->options.interpreter == Interpreter::NONE) {
            _impl->machine->setTimingProfile(hp48::G48sxTimingProfile::COMPARE);
            _impl->machine->resetColdStart();
            _impl->machine->scheduleKeyPress("A", hp48::G48SX_RECOVERY_KEY_INSTRUCTION,
                                            hp48::G48SX_RECOVERY_KEY_HOLD_INSTRUCTIONS);
            _impl->machine->stepTo(hp48::G48SX_RECOVERY_STACK_INSTRUCTIONS);
            if (!_impl->machine->running())
                throw std::runtime_error("Unable to initialize HP-48 calculator");
            _impl->machine->setTimingProfile(hp48::G48sxTimingProfile::REAL);
            captureStartState();
        }
        restoreStartState();
    }
    catch (const std::exception& error) {
        _impl->loaded = false;
        _cpuState = eERROR;
        _errorMessage = error.what();
        setExecMode(ePAUSED);
    }
}

void Hp48sxEmulationCore::captureStartState()
{
    // Startup can reach dispatch before the scheduled EVAL release. Do not
    // capture a held calculator key whose release has already run after reset.
    _impl->machine->release("ON");
    _impl->machine->release("EVAL");
    const auto cpuState = _impl->machine->cpu().getState();
    _impl->startReturnStack = cpuState.rstk;
    _impl->startCrc = cpuState.crc;
    _impl->startState = _impl->machine->saveStateBytes(false);
}

void Hp48sxEmulationCore::restoreStartState()
{
    _impl->loaded = !_impl->startState.empty();
    _impl->machine->loadState(_impl->loaded ? _impl->startState : _impl->powerOnState);
    if (_impl->loaded) {
        // The library's state loader does not restore the complete Saturn return
        // stack or CRC. Refill the circular stack in its saved logical order.
        for (auto it = _impl->startReturnStack.rbegin(); it != _impl->startReturnStack.rend(); ++it)
            _impl->machine->cpu().pushRSTK(*it);
        *_impl->machine->cpu().crcRegister() = _impl->startCrc;
    }
    _impl->machine->setTimingProfile(hp48::G48sxTimingProfile::REAL);
    _impl->keyStates.fill(false);
    _impl->backendStepOverSP = 0;
    _cycles = 0;
    _frames = 0;
    _state = {};
    _backendStopped = false;
    _breakpointTriggered = false;
    _cpuState = eNORMAL;
    _errorMessage.clear();
    if (_impl->loaded && _isHybridChipMode)
        fetchState();
    else if (_isHybridChipMode) {
        _state.pc = defaultLoadAddress();
        std::ranges::fill(_impl->memory, 0);
        _impl->syncedMemory = _impl->memory;
    }
    setExecMode(_host.isHeadless() ? eRUNNING : ePAUSED);
}

GenericCpu::ExecMode Hp48sxEmulationCore::execMode() const
{
    return _execMode == ePAUSED || _impl->machine->paused() ? ePAUSED : _execMode;
}

void Hp48sxEmulationCore::setExecMode(ExecMode mode)
{
    if (!_impl->loaded)
        mode = ePAUSED;
    GenericCpu::setExecMode(mode);
    _impl->machine->setPaused(mode == ePAUSED);
    if (!_execChip8 && mode == eSTEPOVER) {
        _impl->backendStepOverSP = _impl->machine->cpu().getSP();
    }
    _impl->machine->cpu().setExecMode(mode == ePAUSED ? ePAUSED : (_execChip8 ? eRUNNING : mode));
}

bool Hp48sxEmulationCore::executeSaturnInstruction()
{
    updateKeys();
    if (!_impl->machine->stepInstruction()) {
        _cpuState = eERROR;
        _errorMessage = _impl->machine->errorMessage();
        setExecMode(ePAUSED);
        return false;
    }
    auto& backend = _impl->machine->cpu();
    if (!_execChip8) {
        const auto backendMode = backend.execMode();
        if (backendMode == eSTEP || (backendMode == eSTEPOVER && backend.getSP() <= _impl->backendStepOverSP) || backend.tryTriggerBreakpoint(backend.getPC())) {
            _backendStopped = true;
            setExecMode(ePAUSED);
        }
    }
    if (_isHybridChipMode && _impl->loaded && _impl->machine->cpu().getPC() == _impl->fetchAddress) {
        ++_cycles;
        fetchState();
        if (_execMode == eSTEP || (_execMode == eSTEPOVER && getSP() <= _stepOverSP)) {
            setExecMode(ePAUSED);
        }
        if (tryTriggerBreakpoint(getPC())) {
            _breakpointTriggered = true;
            setExecMode(ePAUSED);
        }
        return true;
    }
    return false;
}

int Hp48sxEmulationCore::executeInstruction()
{
    if (_execMode == ePAUSED || _cpuState == eERROR || !_impl->loaded)
        return 0;
    flushMemory();
    const auto start = _impl->machine->cycles();
    if (_execChip8) {
        const auto instructionLimit = start + _impl->machine->cyclesPerEmulatedFrame() * 14U;
        while (_execMode != ePAUSED && _impl->machine->cycles() < instructionLimit && !executeSaturnInstruction()) {
        }
        if (_execMode != ePAUSED && _impl->machine->cycles() >= instructionLimit) {
            _cpuState = eERROR;
            _errorMessage = "HP-48 interpreter did not return to its fetch loop";
            setExecMode(ePAUSED);
        }
    }
    else {
        executeSaturnInstruction();
    }
    return static_cast<int>(_impl->machine->cycles() - start);
}

void Hp48sxEmulationCore::executeInstructions(int count)
{
    while (count-- > 0 && _execMode != ePAUSED)
        executeInstruction();
}

void Hp48sxEmulationCore::executeFrame()
{
    if (_execMode == ePAUSED || _cpuState == eERROR || !_impl->loaded)
        return;
    flushMemory();
    const auto target = _impl->machine->cycles() + _impl->machine->cyclesPerEmulatedFrame();
    while (_execMode != ePAUSED && _impl->machine->cycles() < target)
        executeSaturnInstruction();
    ++_frames;
    _host.vblank();
    _host.updateScreen();
}

int Hp48sxEmulationCore::frameRate() const
{
    return static_cast<int>(hp48::G48SX_EMULATED_FRAME_HZ);
}
int64_t Hp48sxEmulationCore::machineCycles() const
{
    return static_cast<int64_t>(_impl->machine->cycles());
}

void Hp48sxEmulationCore::refreshMemory()
{
    const auto state = _impl->machine->cpu().getState();
    const uint32_t base = static_cast<uint32_t>(state.r2 & 0xfffffU);
    for (uint32_t address = 0; address < _impl->memory.size(); ++address) {
        _impl->memory[address] = static_cast<uint8_t>(readPacked(*_impl->machine, base + address * 2U, 2));
    }
    _impl->syncedMemory = _impl->memory;
}

void Hp48sxEmulationCore::flushMemory()
{
    if (!_isHybridChipMode || !_impl->loaded)
        return;
    const uint32_t base = static_cast<uint32_t>(_impl->machine->cpu().getState().r2 & 0xfffffU);
    for (uint32_t address = 0; address < _impl->memory.size(); ++address) {
        if (_impl->memory[address] != _impl->syncedMemory[address]) {
            const uint8_t value = _impl->memory[address];
            _impl->machine->writeNibble(base + address * 2U, value & 0x0fU);
            _impl->machine->writeNibble(base + address * 2U + 1U, value >> 4U);
            _impl->syncedMemory[address] = value;
        }
    }
}

void Hp48sxEmulationCore::fetchState()
{
    const auto state = _impl->machine->cpu().getState();
    const uint32_t variables = static_cast<uint32_t>(state.r4 & 0xfffffU);
    _state.cycles = _cycles;
    _state.pc = static_cast<int>(state.r3 & 0xffffU);
    _state.i = static_cast<int>(state.r0 & 0xffffU);
    for (uint32_t index = 0; index < 16; ++index) {
        _state.v[index] = static_cast<uint8_t>(readPacked(*_impl->machine, variables + index * 2U, 2));
    }
    _state.dt = static_cast<int>(readPacked(*_impl->machine, variables + _impl->timerOffset, 2));
    _state.st = static_cast<int>(readPacked(*_impl->machine, variables + _impl->soundOffset, 2));
    const uint32_t stackNibbleOffset = readPacked(*_impl->machine, variables + _impl->stackPointerOffset, 5);
    _state.sp = static_cast<int>(std::min<uint32_t>(stackNibbleOffset / 4U, 16U));
    for (int index = 0; index < _state.sp; ++index) {
        _state.s[index] = static_cast<uint16_t>(readPacked(*_impl->machine, variables + _impl->stackOffset + index * 4U, 4));
    }
    refreshMemory();
}

void Hp48sxEmulationCore::updateKeys()
{
    if (!_impl->loaded)
        return;
    static constexpr std::array<std::array<uint8_t, 2>, 16> RAW_KEY_TABLE{{
        {0x1, 0x4},
        {0x8, 0x8},
        {0x8, 0x4},
        {0x8, 0x2},
        {0x4, 0x8},
        {0x4, 0x4},
        {0x4, 0x2},
        {0x2, 0x8},
        {0x2, 0x4},
        {0x2, 0x2},
        {0x1, 0x8},
        {0x1, 0x2},
        {0x8, 0x1},
        {0x4, 0x1},
        {0x2, 0x1},
        {0x1, 0x1},
    }};
    for (uint8_t key = 0; key < 16; ++key) {
        const bool down = _host.isKeyDown(key);
        if (down == _impl->keyStates[key])
            continue;
        _impl->keyStates[key] = down;
        const uint8_t outMask = _isHybridChipMode ? _impl->machine->readNibble(_impl->keyTableAddress + key * 2U) : RAW_KEY_TABLE[key][0];
        const uint16_t inputMask = _isHybridChipMode ? _impl->machine->readNibble(_impl->keyTableAddress + key * 2U + 1U) : RAW_KEY_TABLE[key][1];
        uint8_t row = 0;
        while (row < 8 && (outMask & (1U << row)) == 0)
            ++row;
        if (down)
            _impl->machine->cpu().pressKey(row, inputMask);
        else
            _impl->machine->cpu().releaseKey(row, inputMask);
    }
}

uint8_t* Hp48sxEmulationCore::memory()
{
    return _impl->memory.data();
}
int Hp48sxEmulationCore::memSize() const
{
    return static_cast<int>(_impl->memory.size());
}
uint8_t Hp48sxEmulationCore::readMemoryByte(uint32_t address) const
{
    return address < _impl->memory.size() ? _impl->memory[address] : 0xff;
}
unsigned Hp48sxEmulationCore::stackSize() const
{
    return 16;
}

GenericCpu::StackContent Hp48sxEmulationCore::stack() const
{
    for (size_t index = 0; index < _state.s.size(); ++index) {
        _impl->stackBytes[index * 2] = static_cast<uint8_t>(_state.s[index] >> 8U);
        _impl->stackBytes[index * 2 + 1] = static_cast<uint8_t>(_state.s[index]);
    }
    return {2, eBIG, eUPWARDS, _impl->stackBytes};
}

uint16_t Hp48sxEmulationCore::getCurrentScreenWidth() const
{
    return hp48::G48sxMachine::LCD_RENDER_WIDTH;
}
uint16_t Hp48sxEmulationCore::getCurrentScreenHeight() const
{
    return hp48::G48sxMachine::LCD_RENDER_HEIGHT;
}
uint16_t Hp48sxEmulationCore::getMaxScreenWidth() const
{
    return getCurrentScreenWidth();
}
uint16_t Hp48sxEmulationCore::getMaxScreenHeight() const
{
    return getCurrentScreenHeight();
}

ghc::span2d<const uint32_t> Hp48sxEmulationCore::getRGBASpan(ghc::span2d<uint32_t> target) const
{
    if (target.width() < getCurrentScreenWidth() || target.height() < getCurrentScreenHeight())
        return {};
    auto result = target.subspan(0, 0, getCurrentScreenWidth(), getCurrentScreenHeight());
    if (!_impl->loaded && _impl->options.interpreter != Interpreter::NONE)
        _impl->idleDisplay.render(result, hp48::Hp48Display::Mode::Stack);
    else
        _impl->machine->render(result);
    std::ranges::transform(result, result.begin(), argb8888ToRgba8888);
    return result.as_const();
}

void Hp48sxEmulationCore::renderAudio(int16_t* samples, size_t frames, int sampleFrequency)
{
    _impl->machine->renderAudio(samples, frames, sampleFrequency);
}

GenericCpu& Hp48sxEmulationCore::getBackendCpu()
{
    return _impl->machine->cpu();
}
Properties& Hp48sxEmulationCore::getProperties()
{
    return _impl->properties;
}

}  // namespace emu
