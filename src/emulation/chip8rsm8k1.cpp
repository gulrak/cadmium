//---------------------------------------------------------------------------------------
// src/emulation/chip8rsm8k1.cpp
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
// This implementation is based on a Ghidra-analysis of the original RS-M8001
// implementation by Martijn Wanting, Revival-Studios, 2007. The emulation
// behavior is functionally reimplemented in C++ by this code.
//---------------------------------------------------------------------------------------
#include <emulation/chip8rsm8k1.hpp>
#include <emulation/coreregistry.hpp>
#include <stdexcept>

namespace emu {

namespace {

const std::string PROP_CLASS = "RS-M8K1";
const std::string PROP_VIP = "VIP Mode";
const std::string PROP_TRACE = "Trace Log";

}

Properties& Chip8RSM8k1Options::registeredPrototype()
{
    auto& props = Properties::getProperties(PROP_CLASS);
    if (!props) {
        props.registerProperty({PROP_VIP, false, "Original mode-1 behavior, including double shifts", PropertyFlags::eWritable});
        props.registerProperty({PROP_TRACE, false, "Enable opcode trace", PropertyFlags::eWritable});
    }
    return props;
}

Properties Chip8RSM8k1Options::asProperties() const
{
    auto props = registeredPrototype();
    props[PROP_VIP].setBool(vipMode);
    props[PROP_TRACE].setBool(traceLog);
    props.palette() = palette;
    return props;
}

Chip8RSM8k1Options Chip8RSM8k1Options::fromProperties(const Properties& props)
{
    return {props[PROP_VIP].getBool(), props[PROP_TRACE].getBool(), props.palette()};
}

namespace {

// clang-format off
using Setup = CoreRegistry::SetupInfo<Chip8RSM8k1Options>;
const Setup PRESETS[] = {
    {
        "RS-MEGACHIP",
        "2007 RS-M8001 implementation (original bugs, checked bounds)",
        ".ch8;.mc8", chip8::Variant::CHIP_8 | chip8::Variant::SCHIP_1_1 | chip8::Variant::MEGA_CHIP,
        {.palette = {{"#001020", "#A0C0F0"}}}
    }
};
// clang-format oon

struct Factory final : CoreRegistry::FactoryInfo<Chip8RSM8k1, Setup, Chip8RSM8k1Options>
{
    Factory()
        : FactoryInfo(25, PRESETS, "Faithful RS-M8001 interpreter with checked bounds")
    {
    }
    std::string prefix() const override { return ""; }
    VariantIndex variantIndex(const Properties& props) const override { return {0, props != PRESETS[0].options.asProperties()}; }
};

const bool REGISTERED = CoreRegistry::registerFactory(PROP_CLASS, std::make_unique<Factory>());

}  // namespace

struct Chip8RSM8k1::Impl final : rsm8k1detail::Platform
{
    Chip8RSM8k1& core;
    uint32_t milliseconds = 0;
    uint64_t audioCursor = 0;
    uint64_t audioOutputPhase = 0;
    bool audioPlaying = false;
    bool frameInProgress = false;
    rsm8k1detail::Interpreter interpreter;
    explicit Impl(Chip8RSM8k1& owner)
        : core(owner)
        , interpreter(*this)
    {
    }
    uint32_t millis() override { return milliseconds; }
    rsm8k1detail::HostInput pollInput() override
    {
        rsm8k1detail::HostInput input{};
        for (size_t key = 0; key < 16; ++key)
            if (core._host.isKeyDown(key))
                input.chipKeys |= 1U << key;
        return input;
    }
    uint32_t pollKeycode() override
    {
        // Cadmium exposes virtual keypad state, not the original single Win32
        // keycode slot. A key wait yields until all virtual keys are released.
        return pollInput().chipKeys;
    }
    void setDisplaySize(int width, int height) override { core._work.setMode(width, height); }
    void drawPixel(int x, int y, uint32_t rgb) override { core._work.setPixel(x, y, be32((rgb << 8) | 255)); }
    void present() override
    {
        core._front = core._work;
        core.refreshIndexedScreen();
        core._host.updateScreen();
    }
    void playMegaChipSound(uint16_t, uint32_t length, std::span<const uint8_t>) override
    {
        audioCursor = audioOutputPhase = 0;
        audioPlaying = length != 0;
    }
    void stopMegaChipSound() override { audioPlaying = false; }
};

Chip8RSM8k1::Chip8RSM8k1(EmulatorHost& host, Properties& props, IEmulationCore* other)
    : Chip8GenericBase(chip8::Variant::MEGA_CHIP, 1000)
    , _host(host)
    , _options(Chip8RSM8k1Options::fromProperties(props))
    , _impl(std::make_unique<Impl>(*this))
{
    (void)REGISTERED;
    _memory.resize(0x100200);
    _impl->interpreter.state().memory = std::span(_memory).subspan(0x200);
    _rV = _impl->interpreter.state().vreg.data();
    if (other && other->chip8Core())
        swapBreakpoints(*other->chip8Core());
    handleReset(true);
}

Chip8RSM8k1::~Chip8RSM8k1() = default;

chip8::VariantSet Chip8RSM8k1::supportedChip8Variants() const
{
    return PRESETS[0].supportedChip8Variants;
}

bool Chip8RSM8k1::updateProperties(Properties& props, Property& changed)
{
    _options = Chip8RSM8k1Options::fromProperties(props);
    _impl->interpreter.state().emulationMode = _options.vipMode;
    setPalette(_options.palette);
    return false;
}

const rsm8k1detail::State& Chip8RSM8k1::nativeState() const
{
    return _impl->interpreter.state();
}

void Chip8RSM8k1::syncFromInterpreter()
{
    const auto& state = nativeState();
    _rPC = state.programCounter + 0x200;
    _rI = state.charset ? state.iregister : state.iregister + 0x200;
    _rSP = state.currentStack / 2;
    _rDT = state.delayTimer;
    _rST = state.soundTimer;
}

void Chip8RSM8k1::syncToInterpreter()
{
    auto& state = _impl->interpreter.state();
    state.programCounter = _rPC - 0x200;
    // I's font selector is not a memory mapping. It survives debugger writes
    // unless the value changes, just as it survives Fx1E and long-I opcodes.
    if (_rI != (state.charset ? state.iregister : state.iregister + 0x200))
        state.iregister = state.charset ? _rI : _rI - 0x200;
    state.currentStack = _rSP * 2;
    state.delayTimer = _rDT;
    state.soundTimer = _rST;
}

void Chip8RSM8k1::handleReset(bool cold)
{
    // Original CPU reset and reload retain RAM and the timer epoch.
    _impl->interpreter.resetCpu();
    _impl->interpreter.state().emulationMode = _options.vipMode;
    _impl->audioPlaying = false;
    _impl->frameInProgress = false;
    _cycleCounter = _frameCounter = 0;
    _systemTime.reset();
    _screen.setAll(0);
    _front.setAll(0);
    _work.setAll(0);
    _cpuState = eNORMAL;
    _errorMessage.clear();
    _execMode = _host.isHeadless() ? eRUNNING : ePAUSED;
    syncFromInterpreter();
}

bool Chip8RSM8k1::loadData(std::span<const uint8_t> data, std::optional<uint32_t> address)
{
    const auto guestAddress = address.value_or(0x200);
    if (guestAddress < 0x200 || guestAddress > _memory.size() || data.size() > _memory.size() - guestAddress) {
        errorHalt("ROM LOAD OUTSIDE RS-M8K1 RAM", _rPC);
        return false;
    }
    if (guestAddress == 0x200)
        _impl->interpreter.loadRom(data);
    else
        std::copy(data.begin(), data.end(), _memory.begin() + guestAddress);
    syncFromInterpreter();
    return true;
}

uint8_t Chip8RSM8k1::readMemoryByte(uint32_t address) const
{
    // Debugger reads do not mutate CPU state. Executed accesses are checked by
    // the interpreter and become eERROR instead of returning this sentinel.
    return address >= 0x200 && address < _memory.size() ? _memory[address] : 0xff;
}

void Chip8RSM8k1::errorHalt(const std::string& message, uint32_t pc)
{
    _cpuState = eERROR;
    _execMode = ePAUSED;
    _errorMessage = message;
    _rPC = pc;
}

int Chip8RSM8k1::executeInstruction()
{
    if (_execMode == ePAUSED || _cpuState == eERROR || _cpuState == eHALT)
        return 0;
    const auto pc = _rPC;
    try {
        if (pc < 0x200 || pc + uint64_t(1) >= _memory.size())
            throw std::out_of_range("PC OUTSIDE RS-M8K1 RAM");
        syncToInterpreter();
        const auto opcode = (uint16_t(_memory[pc]) << 8) | _memory[pc + 1];
        if (opcode == 0x00e0 && nativeState().megaChipEnabled)
            _host.preClear();
        if (_options.traceLog)
            Logger::log(Logger::eCHIP8, _cycleCounter, frameTime(), dumpStateLine().c_str());
        _impl->interpreter.runCpu(1);
        syncFromInterpreter();
        if (_impl->interpreter.waiting()) {
            _cpuState = eWAIT;
            return 0;
        }
        _cpuState = nativeState().exit ? eHALT : eNORMAL;
        ++_cycleCounter;
        if (_cpuState == eHALT || _execMode == eSTEP || (_execMode == eSTEPOVER && _rSP <= _stepOverSP) || (_execMode == eSTEPOUT && opcode == 0x00ee))
            _execMode = ePAUSED;
        if (tryTriggerBreakpoint(_rPC)) {
            _execMode = ePAUSED;
            _breakpointTriggered = true;
        }
        return 1;
    }
    catch (const std::exception& e) {
        errorHalt(std::string("RS-M8K1: ") + e.what(), pc);
        return 0;
    }
}

void Chip8RSM8k1::executeInstructions(int count)
{
    while (count-- > 0 && executeInstruction()) {
    }
}

void Chip8RSM8k1::advanceTime(uint32_t milliseconds)
{
    _impl->milliseconds += milliseconds;
    _systemTime.addCycles(milliseconds);
}

void Chip8RSM8k1::handleTimer()
{
    if (_cpuState == eERROR || _execMode == ePAUSED)
        return;
    advanceTime(20);
    if (!_impl->interpreter.waiting()) {
        _impl->interpreter.updateTimers();
        syncFromInterpreter();
    }
}

void Chip8RSM8k1::executeFrame()
{
    if (_execMode == ePAUSED || _cpuState == eERROR || _cpuState == eHALT)
        return;
    advanceTime(20);
    auto& state = _impl->interpreter.state();
    if (!_impl->frameInProgress)
        state.currentTick = 0;
    _impl->frameInProgress = true;
    do {
        if (!executeInstruction())
            break;
        ++state.currentTick;
    } while (_execMode != ePAUSED && state.exit == 0 && state.currentTick < state.ticksPerFrame);
    if (_cpuState != eERROR) {
        try {
            if (!state.megaChipEnabled && !_impl->interpreter.waiting())
                _impl->interpreter.updateScreen();
            if (!_impl->interpreter.waiting())
                _impl->interpreter.getKeys();
        }
        catch (const std::exception& e) {
            errorHalt(e.what(), _rPC);
        }
    }
    if (!_impl->interpreter.waiting()) {
        _impl->frameInProgress = false;
        ++_frameCounter;
        _host.vblank();
    }
}

GenericCpu::StackContent Chip8RSM8k1::stack() const
{
    const auto& state = nativeState();
    for (size_t n = 0; n < 16; ++n) {
        const uint32_t address = uint16_t(state.stack[n * 2] | (state.stack[n * 2 + 1] << 8)) + 0x202;
        for (size_t byte = 0; byte < 4; ++byte)
            _guestStack[n * 4 + byte] = address >> (byte * 8);
    }
    return {4, eLITTLE, eUPWARDS, _guestStack};
}

bool Chip8RSM8k1::restoreState(const Chip8State& state)
{
    if (state.i < 0 || uint32_t(state.i) >= _memory.size() || state.pc < 0x200 || uint32_t(state.pc) + 1 >= _memory.size() || state.sp < 0 || state.sp > 16 || state.dt < 0 || state.dt > 255 || state.st < 0 || state.st > 255)
        return false;
    for (int n = 0; n < state.sp; ++n)
        if (state.s[n] < 0x202)
            return false;
    restoreRegisters(state);
    auto& native = _impl->interpreter.state();
    native.charset = 0;
    for (int n = 0; n < state.sp; ++n) {
        const auto offset = state.s[n] - 0x202;
        native.stack[n * 2] = offset;
        native.stack[n * 2 + 1] = offset >> 8;
    }
    syncToInterpreter();
    return true;
}

uint16_t Chip8RSM8k1::getCurrentScreenWidth() const
{
    return nativeState().megaChipEnabled ? 256 : nativeState().screenWidth;
}

uint16_t Chip8RSM8k1::getCurrentScreenHeight() const
{
    return nativeState().megaChipEnabled ? 192 : nativeState().screenHeight;
}

IEmulationCore::PixelRatio Chip8RSM8k1::getPixelRatio() const
{
    return nativeState().megaChipEnabled ? PixelRatio{1, 1} : PixelRatio{2, 3};
}

void Chip8RSM8k1::refreshIndexedScreen() const
{
    const auto& state = nativeState();
    _screen.setMode(getCurrentScreenWidth(), getCurrentScreenHeight());
    std::array<uint32_t, 256> palette{};
    if (state.megaChipEnabled) {
        for (size_t i = 0; i < 256; ++i)
            palette[i] = (state.megaChipPalette[i] << 8) | 255;
    }
    else {
        palette[0] = (state.backgroundColor << 8) | 255;
        palette[1] = (state.foregroundColor << 8) | 255;
    }
    _screen.setPalette(palette);
    for (unsigned y = 0; y < getCurrentScreenHeight(); ++y)
        for (unsigned x = 0; x < getCurrentScreenWidth(); ++x) {
            const unsigned bit = y * state.screenWidth + x;
            _screen.setPixel(x, y, state.megaChipEnabled ? state.megaChipBuffer[y * 256 + x] : bit / 8 < state.screen.size() ? (state.screen[bit / 8] >> (7 - (x & 7))) & 1 : 0);
        }
}

const VideoType* Chip8RSM8k1::getScreen() const
{
    if (nativeState().megaChipEnabled)
        return nullptr;
    refreshIndexedScreen();
    return &_screen;
}

const VideoRGBAType* Chip8RSM8k1::getScreenRGBA() const
{
    return nativeState().megaChipEnabled ? &_front : nullptr;
}

const VideoRGBAType* Chip8RSM8k1::getWorkRGBA() const
{
    const auto& state = nativeState();
    if (!state.megaChipEnabled)
        return nullptr;
    _work.setMode(256, 192);
    for (int y = 0; y < 192; ++y)
        for (int x = 0; x < 256; ++x)
            _work.setPixel(x, y, be32((state.megaChipPalette[state.megaChipBuffer[y * 256 + x]] << 8) | 255));
    return &_work;
}

void Chip8RSM8k1::setPalette(const Palette& palette)
{
    if (palette.colors.size() >= 2) {
        auto& state = _impl->interpreter.state();
        state.backgroundColor = palette.colors[0].toRGBAInt() >> 8;
        state.foregroundColor = palette.colors[1].toRGBAInt() >> 8;
    }
    refreshIndexedScreen();
}

void Chip8RSM8k1::renderAudio(int16_t* samples, size_t frames, int sampleFrequency)
{
    const auto& sound = nativeState().megaChipSound;
    if (!_impl->audioPlaying || !sound.sampleLength || sampleFrequency <= 0) {
        std::fill_n(samples, frames, 0);
        return;
    }
    // Generate the original 44100Hz nearest-neighbor stream first; output-rate
    // adaptation is separate from its quantized 8.8 sample cursor.
    for (size_t n = 0; n < frames; ++n) {
        samples[n] = (int(nativeState().memory[sound.dataOffset + (_impl->audioCursor >> 8)]) - 128) * 256;
        _impl->audioOutputPhase += 44100;
        while (_impl->audioOutputPhase >= uint64_t(sampleFrequency)) {
            _impl->audioOutputPhase -= sampleFrequency;
            _impl->audioCursor = (_impl->audioCursor + sound.step) % (uint64_t(sound.sampleLength) << 8);
        }
    }
}

}
