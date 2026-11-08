//---------------------------------------------------------------------------------------
// src/emulation/schipstrict.cpp
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
#include <emulation/schipstrict.hpp>
#include <emulation/coreregistry.hpp>
#include <emulation/logger.hpp>

#include <algorithm>
#include <bit>
#include <cstring>

namespace emu {

static const std::string PROP_CLASS = "SCHIP-STRICT";
static const std::string PROP_TRACE_LOG = "traceLog";
static const std::string PROP_CLEAN_RAM = "cleanRam";
static const std::string PROP_Q_HP48_DISPLAY = "hp48Display";

namespace {

// Cycle costs in Saturn cycles, see docs/HP48-SCHIP-1.1-Timing.md
constexpr int64_t REALTIME_CYCLES = 169;
constexpr int64_t REALTIME_SOUND_CYCLES = 103;
constexpr int64_t REALTIME_TICK_CYCLES = 385;
constexpr int64_t REALTIME_TIMER_CYCLES = 12;
// Where the interpreter writes the OUT register, whose bit 11 drives the buzzer. Only the speaker
// write can set the bit, the key tests have it clear, so they end a high phase early.
constexpr int64_t OUT_SPEAKER_OFFSET = 134;         // from the start of the real-time chores
constexpr int64_t OUT_TICK_KEYS_BEFORE_END = 160;   // before the end of the real-time chores of a tick
constexpr int64_t OUT_KEY_OPCODE_OFFSET = 188;      // from the start of the EX9E/EXA1 execute part
constexpr int64_t OUT_KEY_WAIT_OFFSET = 268;        // from the start of the FX0A execute part
constexpr int64_t OUT_KEY_SCAN_OFFSET = 104;        // from the end of the real-time chores of a scan round
constexpr int64_t OUT_KEY_HELD_OFFSET = 89;         // from the end of the real-time chores of a held key round
constexpr int16_t BUZZER_AMPLITUDE = 16384;
constexpr int64_t TIMER_READ_OFFSET = 79;
constexpr int64_t DISPATCH_CYCLES = 242;

// The fonts as SCHIP 1.1 unpacks them to 0x000 (16 x 5 bytes) and 0x050 (10 x 10 bytes)
constexpr uint8_t SMALL_FONT[80] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70, 0xF0, 0x10, 0xF0, 0x80, 0xF0, 0xF0,
    0x10, 0xF0, 0x10, 0xF0, 0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0, 0xF0, 0x80,
    0xF0, 0x90, 0xF0, 0xF0, 0x10, 0x20, 0x40, 0x40, 0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0,
    0x10, 0xF0, 0xF0, 0x90, 0xF0, 0x90, 0x90, 0xE0, 0x90, 0xE0, 0x90, 0xE0, 0xF0, 0x80, 0x80, 0x80,
    0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0, 0xF0, 0x80, 0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80,
};
constexpr uint8_t BIG_FONT[100] = {
    0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C,  // 0
    0x18, 0x38, 0x58, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C,  // 1
    0x3E, 0x7F, 0xC3, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFF, 0xFF,  // 2
    0x3C, 0x7E, 0xC3, 0x03, 0x0E, 0x0E, 0x03, 0xC3, 0x7E, 0x3C,  // 3
    0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0xFF, 0xFF, 0x06, 0x06,  // 4
    0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFE, 0x03, 0xC3, 0x7E, 0x3C,  // 5
    0x3E, 0x7C, 0xC0, 0xC0, 0xFC, 0xFE, 0xC3, 0xC3, 0x7E, 0x3C,  // 6
    0xFF, 0xFF, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x60,  // 7
    0x3C, 0x7E, 0xC3, 0xC3, 0x7E, 0x7E, 0xC3, 0xC3, 0x7E, 0x3C,  // 8
    0x3C, 0x7E, 0xC3, 0xC3, 0x7F, 0x3F, 0x03, 0x03, 0x3E, 0x7C,  // 9
};

// Per line cost of a hires sprite by visible nibbles at the right edge, index 0 is unused
constexpr int64_t HIRES_LINE_CYCLES_8[4] = {0, 349, 358, 365};
constexpr int64_t HIRES_LINE_CYCLES_16[6] = {0, 380, 389, 398, 407, 414};

}  // namespace

Properties SchipStrictOptions::asProperties() const
{
    auto result = registeredPrototype();
    result[PROP_TRACE_LOG].setBool(traceLog);
    result[PROP_CLEAN_RAM].setBool(cleanRam);
    result[PROP_Q_HP48_DISPLAY].setBool(hp48Display);
    result.palette() = palette;
    return result;
}

SchipStrictOptions SchipStrictOptions::fromProperties(const Properties& props)
{
    SchipStrictOptions opts{};
    opts.traceLog = props[PROP_TRACE_LOG].getBool();
    opts.cleanRam = props[PROP_CLEAN_RAM].getBool();
    opts.hp48Display = props[PROP_Q_HP48_DISPLAY].getBool();
    opts.palette = props.palette();
    return opts;
}

Properties& SchipStrictOptions::registeredPrototype()
{
    auto& prototype = Properties::getProperties(PROP_CLASS);
    if (!prototype) {
        prototype.registerProperty({PROP_TRACE_LOG, "Trace Log", false, "Enable trace log", PropertyFlags::eWritable});
        prototype.registerProperty({PROP_CLEAN_RAM, "Clean RAM", true, "Delete ram on startup", PropertyFlags::eWritable});
        prototype.registerProperty({PROP_Q_HP48_DISPLAY, "HP-48SX display emulation", true, PropertyFlags::eWritable});
    }
    return prototype;
}

struct SchipStrictSetupInfo
{
    const char* presetName;
    const char* description;
    const char* defaultExtensions;
    chip8::VariantSet supportedChip8Variants{chip8::Variant::NONE};
    SchipStrictOptions options;
};

// clang-format off
static SchipStrictSetupInfo schipStrictPresets[] = {
    {
        "SCHIP-1.1",
        "Cycle exact SUPER-CHIP 1.1 as it runs on an HP-48SX, 1991",
        ".sc8;.sc11",
        chip8::Variant::SCHIP_1_1,
        {.cleanRam = true, .traceLog = false, .hp48Display = true}
    }
};
// clang-format on

struct SchipStrictFactoryInfo final : public CoreRegistry::FactoryInfo<SchipStrictEmulator, SchipStrictSetupInfo, SchipStrictOptions>
{
    explicit SchipStrictFactoryInfo(const char* description)
        : FactoryInfo(22, schipStrictPresets, description)
    {
    }
    std::string prefix() const override { return "STRICT"; }
    VariantIndex variantIndex(const Properties& props) const override { return {0, schipStrictPresets[0].options.asProperties() == props}; }
};

static bool registeredSchipStrict = CoreRegistry::registerFactory(PROP_CLASS, std::make_unique<SchipStrictFactoryInfo>("First cycle exact HLE emulation of SCHIP 1.1 on an HP-48SX"));

SchipStrictEmulator::SchipStrictEmulator(EmulatorHost& host, Properties& properties, IEmulationCore* other)
    : Chip8GenericBase(chip8::Variant::SCHIP_1_1, CPU_CLOCK_FREQUENCY)
    , _host(host)
    , _options(SchipStrictOptions::fromProperties(properties))
{
    if (other) {
        if (auto otherC8 = other->chip8Core()) {
            swapBreakpoints(*otherC8);
        }
    }
    _memory.resize(MEMORY_SIZE, 0);
    _rV = _registers.data();
    _screen.setPalette(_options.palette);
    _hp48Display.setAnnunciators({.busy = true});
}

chip8::VariantSet SchipStrictEmulator::supportedChip8Variants() const
{
    return chip8::Variant::SCHIP_1_1;
}

bool SchipStrictEmulator::updateProperties(Properties& props, Property& changed)
{
    (void)registeredSchipStrict;
    if (changed.getId() == PROP_TRACE_LOG || changed.getId() == PROP_Q_HP48_DISPLAY) {
        _options = SchipStrictOptions::fromProperties(props);
        // only the presentation changes, so redraw instead of restarting, the host doesn't while paused
        if (changed.getId() == PROP_Q_HP48_DISPLAY)
            _host.updateScreen();
        return false;
    }
    return true;
}

void SchipStrictEmulator::setPalette(const Palette& palette)
{
    _screen.setPalette(palette);
}

ghc::span2d<const uint32_t> SchipStrictEmulator::getRGBASpan(ghc::span2d<uint32_t> target) const
{
    if (!_options.hp48Display || target.width() < HP48_DISPLAY_WIDTH || target.height() < HP48_DISPLAY_HEIGHT)
        return {};
    // Pack the LCD pixels MSB first into the HP-48 frame buffer layout, the 3 extra pixels per row stay off
    for (int y = 0; y < SCREEN_HEIGHT; ++y) {
        for (int byte = 0; byte < SCREEN_WIDTH / 8; ++byte) {
            uint8_t bits = 0;
            for (int bit = 0; bit < 8; ++bit) {
                if (_lcd[y * SCREEN_WIDTH + byte * 8 + bit])
                    bits |= 0x80 >> bit;
            }
            _hp48FrameBuffer[y * 17 + byte] = bits;
        }
    }
    auto result = target.subspan(0, 0, HP48_DISPLAY_WIDTH, HP48_DISPLAY_HEIGHT);
    _hp48Display.render(result, hp48::Hp48Display::Mode::FrameBuffer);
    std::ranges::transform(result, result.begin(), argb8888ToRgba8888);
    return result.as_const();
}

GenericCpu::StackContent SchipStrictEmulator::stack() const
{
    for (size_t index = 0; index < _stack.size(); ++index) {
        _stackView[index * 2] = _stack[index] >> 8;
        _stackView[index * 2 + 1] = _stack[index] & 0xFF;
    }
    return {2, eBIG, eUPWARDS, std::span<const uint8_t>(_stackView.data(), _stackView.size())};
}

bool SchipStrictEmulator::restoreState(const Chip8State& state)
{
    if (!state.isValid(stackSize()))
        return false;
    restoreRegisters(state);
    for (int index = 0; index < state.sp; ++index)
        _stack[index] = state.s[index];
    return true;
}

void SchipStrictEmulator::handleReset(bool)
{
    _cycleCounter = 0;
    _frameCounter = 0;
    _systemTime.reset();
    if (_options.cleanRam)
        std::fill(_memory.begin(), _memory.end(), 0);
    std::memcpy(_memory.data(), SMALL_FONT, sizeof(SMALL_FONT));
    std::memcpy(_memory.data() + 0x50, BIG_FONT, sizeof(BIG_FONT));
    if (_options.traceLog)
        Logger::log(Logger::eCHIP8, _cycleCounter, {_frameCounter, 0}, "--- RESET ---");
    _registers.fill(0);
    _stack.fill(0);
    _rI = 0;
    _rPC = 0x200;
    _rSP = 0;
    _rDT = 0;
    _rST = 0;
    _hires = false;
    _lcd.fill(0);
    _screen.setAll(0);
    _machineCycles = 0;
    _lastDispatch = 0;
    _nextTick = STARTUP_TICK_PHASE - STARTUP_PENDING_TICKS * TICK_CYCLES;
    _nextFrame = TICK_CYCLES;
    _waitState = WaitState::NONE;
    {
        std::lock_guard lock(_audioMutex);
        _speakerDataHigh = false;
        _buzzerHigh = false;
        _buzzerCycle = 0;
        _buzzerSamples.reset();
        resetBuzzerOutput();
    }
    _execMode = _host.isHeadless() ? eRUNNING : ePAUSED;
    _cpuState = eNORMAL;
    _errorMessage.clear();
}

void SchipStrictEmulator::addCycles(int64_t cycles)
{
    _machineCycles += cycles;
    _systemTime.addCycles(cycles);
}

bool SchipStrictEmulator::realtime()
{
    // Speaker toggle while the sound timer runs, then the 64 Hz tick check against TIMER2.
    // At most one tick is handled per call, missed ticks are caught up on the next calls.
    int64_t cycles = REALTIME_CYCLES;
    if (_rST) {
        cycles += REALTIME_SOUND_CYCLES;
        setBuzzer(_speakerDataHigh, _machineCycles + OUT_SPEAKER_OFFSET);
        _speakerDataHigh = !_speakerDataHigh;
    }
    const auto readCycle = _machineCycles + TIMER_READ_OFFSET + (_rST ? REALTIME_SOUND_CYCLES : 0);
    const bool tick = readCycle >= _nextTick;
    if (tick) {
        _nextTick += TICK_CYCLES;
        cycles += REALTIME_TICK_CYCLES + (_rDT ? REALTIME_TIMER_CYCLES : 0) + (_rST ? REALTIME_TIMER_CYCLES : 0);
        if (_rDT)
            --_rDT;
        if (_rST)
            --_rST;
        // the tick also scans the ENTER, backstep and +/- keys
        setBuzzer(false, _machineCycles + cycles - OUT_TICK_KEYS_BEFORE_END);
    }
    addCycles(cycles);
    return tick;
}

void SchipStrictEmulator::handleFrame()
{
    {
        // The host fetches the samples of this frame in vblank().
        std::lock_guard lock(_audioMutex);
        advanceBuzzer(_nextFrame);
    }
    _nextFrame += TICK_CYCLES;
    ++_frameCounter;
    _host.vblank();
    _host.updateScreen();
}

void SchipStrictEmulator::executeFrame()
{
    if (_execMode == ePAUSED || _cpuState == eERROR) {
        Chip8GenericBase::setExecMode(ePAUSED);
        return;
    }
    const auto nextFrame = _nextFrame;
    while (_execMode != ePAUSED && _machineCycles < nextFrame) {
        SchipStrictEmulator::executeInstruction();
    }
}

void SchipStrictEmulator::executeInstructions(int numInstructions)
{
    for (int index = 0; index < numInstructions; ++index)
        SchipStrictEmulator::executeInstruction();
}

int SchipStrictEmulator::executeInstruction()
{
    if (_execMode == ePAUSED || _cpuState == eERROR)
        return 0;
    const auto startCycles = _machineCycles;
    if (_cpuState == eWAIT) {
        continueWait();
    }
    else {
        realtime();
        const auto opcode = readWord(_rPC);
        _rPC = (_rPC + 2) & 0xFFFF;
        ++_cycleCounter;
        _lastDispatch = _machineCycles;
        addCycles(DISPATCH_CYCLES);
        execute(opcode);
    }
    while (_machineCycles >= _nextFrame)
        handleFrame();
    if (_execMode == eSTEP || (_execMode == eSTEPOVER && _rSP <= _stepOverSP)) {
        if (_cpuState != eWAIT)
            _execMode = ePAUSED;
    }
    if (_cpuState != eWAIT && tryTriggerBreakpoint(_rPC)) {
        _execMode = ePAUSED;
        _breakpointTriggered = true;
    }
    return static_cast<int>(_machineCycles - startCycles);
}

void SchipStrictEmulator::errorHalt(std::string message)
{
    _execMode = ePAUSED;
    _cpuState = eERROR;
    _errorMessage = std::move(message);
    _rPC = (_rPC - 2) & 0xFFFF;
}

void SchipStrictEmulator::skipIf(bool condition, int64_t cyclesNoSkip, int64_t cyclesSkip)
{
    if (condition) {
        _rPC = (_rPC + 2) & 0xFFFF;
        addCycles(cyclesSkip);
    }
    else {
        addCycles(cyclesNoSkip);
    }
}

void SchipStrictEmulator::startWait(WaitState state, uint16_t opcode)
{
    _waitState = state;
    _waitOpcode = opcode;
    _cpuState = eWAIT;
    _rPC = (_rPC - 2) & 0xFFFF;  // keep the PC on the waiting instruction like Chip8StrictEmulator
}

void SchipStrictEmulator::endWait()
{
    _waitState = WaitState::NONE;
    _cpuState = eNORMAL;
    _rPC = (_rPC + 2) & 0xFFFF;
}

void SchipStrictEmulator::continueWait()
{
    switch (_waitState) {
        case WaitState::LORES_DRAW:
            // lores Dxyn draws right after a 64 Hz tick
            if (realtime()) {
                addCycles(7);
                endWait();
                drawLores(_waitOpcode);
            }
            else {
                addCycles(7);
            }
            break;
        case WaitState::KEY_SCAN:
            // one key is tested per round, cycling through 0..F
            realtime();
            setBuzzer(false, _machineCycles + OUT_KEY_SCAN_OFFSET);
            addCycles(210);
            _waitKey = (_waitKey + 1) & 0xF;
            if (keyDown(_waitKey)) {
                addCycles(113);
                _waitState = WaitState::KEY_HELD;
            }
            break;
        case WaitState::KEY_HELD:
            // beep while the key is held, return right after release
            _rST = 4;
            realtime();
            setBuzzer(false, _machineCycles + OUT_KEY_HELD_OFFSET);
            if (keyDown(_waitKey)) {
                addCycles(308);
            }
            else {
                addCycles(259);
                _rV[(_waitOpcode >> 8) & 0xF] = _waitKey;
                endWait();
            }
            break;
        default:
            _cpuState = eNORMAL;
            break;
    }
}

void SchipStrictEmulator::execute(uint16_t opcode)
{
    const auto x = (opcode >> 8) & 0xF;
    const auto y = (opcode >> 4) & 0xF;
    auto& vx = _rV[x];
    const auto vy = _rV[y];
    switch (opcode >> 12) {
        case 0x0:
            if (opcode == 0x00E0) {
                clearScreen();
                addCycles(11354);
            }
            else if (opcode == 0x00EE) {
                if (!_rSP) {
                    errorHalt("STACK UNDERFLOW");
                    break;
                }
                _rPC = _stack[--_rSP];
                addCycles(308);
                if (_execMode == eSTEPOUT)
                    _execMode = ePAUSED;
            }
            else if (opcode == 0x00FF) {
                _hires = true;
                addCycles(214);
            }
            else if (opcode == 0x00FE) {
                _hires = false;
                addCycles(241);
            }
            else if (opcode == 0x00FD) {
                // exits the interpreter, so just stop here
                _execMode = ePAUSED;
                _rPC = (_rPC - 2) & 0xFFFF;
            }
            else if (opcode == 0x00FC) {
                scrollLeft();
                addCycles(15450);
            }
            else if (opcode == 0x00FB) {
                scrollRight();
                addCycles(15921);
            }
            else if ((opcode & 0xFFF0) == 0x00C0 && (opcode & 0xF)) {
                scrollDown(opcode & 0xF);
                addCycles(16866 - 125 * (opcode & 0xF));
            }
            else {
                errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
            }
            break;
        case 0x1:
            if ((opcode & 0xFFF) == ((_rPC - 2) & 0xFFFF))
                _execMode = ePAUSED;
            _rPC = opcode & 0xFFF;
            addCycles(107);
            break;
        case 0x2:
            if (_rSP >= STACK_LEVELS) {
                errorHalt("STACK OVERFLOW");
                break;
            }
            _stack[_rSP++] = _rPC;
            _rPC = opcode & 0xFFF;
            addCycles(296);
            break;
        case 0x3:
            skipIf(vx == (opcode & 0xFF), 148, 189);
            break;
        case 0x4:
            skipIf(vx != (opcode & 0xFF), 148, 200);
            break;
        case 0x5:  // the low nibble is not checked
            skipIf(vx == vy, 250, 291);
            break;
        case 0x6:
            vx = opcode & 0xFF;
            addCycles(132);
            break;
        case 0x7:
            vx += opcode & 0xFF;
            addCycles(152);
            break;
        case 0x8:
            switch (opcode & 0xF) {
                case 0x0:
                    vx = vy;
                    addCycles(284);
                    break;
                case 0x1:
                    vx |= vy;
                    addCycles(315);
                    break;
                case 0x2:
                    vx &= vy;
                    addCycles(336);
                    break;
                case 0x3:
                    vx ^= vy;
                    addCycles(375);
                    break;
                case 0x4: {
                    const auto result = vx + vy;
                    vx = result & 0xFF;
                    _rV[0xF] = result >> 8;
                    addCycles(481);
                    break;
                }
                case 0x5: {
                    const uint8_t carry = vx >= vy ? 1 : 0;
                    vx = (vx - vy) & 0xFF;
                    _rV[0xF] = carry;
                    addCycles(514);
                    break;
                }
                case 0x6: {
                    const uint8_t carry = vx & 1;
                    vx >>= 1;
                    _rV[0xF] = carry;
                    addCycles(518);
                    break;
                }
                case 0x7: {
                    const uint8_t carry = vy >= vx ? 1 : 0;
                    vx = (vy - vx) & 0xFF;
                    _rV[0xF] = carry;
                    addCycles(574);
                    break;
                }
                case 0xE: {
                    const uint8_t carry = vx >> 7;
                    vx <<= 1;
                    _rV[0xF] = carry;
                    addCycles(561);
                    break;
                }
                default:
                    errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
                    break;
            }
            break;
        case 0x9:  // the low nibble is not checked
            skipIf(vx != vy, 250, 302);
            break;
        case 0xA:
            _rI = opcode & 0xFFF;
            addCycles(107);
            break;
        case 0xB:  // Bxnn jumps to xnn + Vx
            _rPC = ((opcode & 0xFFF) + vx) & 0xFFFF;
            addCycles(223);
            break;
        case 0xC:
            // The real interpreter uses the low byte of the Saturn CRC register, which
            // depends on every nibble the CPU read, so it can't be reproduced
            _randomState ^= _randomState << 13;
            _randomState ^= _randomState >> 17;
            _randomState ^= _randomState << 5;
            vx = (_randomState >> 8) & opcode & 0xFF;
            addCycles(187);
            break;
        case 0xD:
            if (_hires) {
                addCycles(drawHires(opcode));
            }
            else {
                addCycles(55);
                startWait(WaitState::LORES_DRAW, opcode);
            }
            break;
        case 0xE:
            if ((opcode & 0xFF) == 0x9E) {
                setBuzzer(false, _machineCycles + OUT_KEY_OPCODE_OFFSET);
                skipIf(vx < 16 && keyDown(vx), 397, 449);
            }
            else if ((opcode & 0xFF) == 0xA1) {
                setBuzzer(false, _machineCycles + OUT_KEY_OPCODE_OFFSET);
                skipIf(!(vx < 16 && keyDown(vx)), 409, 450);
            }
            else {
                errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
            }
            break;
        case 0xF:
            switch (opcode & 0xFF) {
                case 0x07:
                    vx = _rDT;
                    addCycles(282);
                    break;
                case 0x0A:
                    setBuzzer(false, _machineCycles + OUT_KEY_WAIT_OFFSET);
                    addCycles(374);
                    _waitKey = 0;
                    startWait(WaitState::KEY_SCAN, opcode);
                    if (keyDown(0)) {
                        addCycles(113);
                        _waitState = WaitState::KEY_HELD;
                    }
                    break;
                case 0x15:
                    _rDT = vx;
                    addCycles(304);
                    break;
                case 0x18:
                    _rST = vx;
                    addCycles(323);
                    break;
                case 0x1E:
                    if (_rI + vx > 0xFFF) {
                        errorHalt("I OVERFLOW");
                        break;
                    }
                    _rI += vx;
                    addCycles(282);
                    break;
                case 0x29:
                    _rI = (vx & 0xF) * 5;
                    addCycles(308);
                    break;
                case 0x30:
                    _rI = 0x50 + (vx & 0xF) * 10;
                    addCycles(345);
                    break;
                case 0x33:
                    if (_rI > 0xFFD) {
                        errorHalt("I OVERFLOW");
                        break;
                    }
                    writeByte(_rI, vx / 100);
                    writeByte(_rI + 1, (vx / 10) % 10);
                    writeByte(_rI + 2, vx % 10);
                    addCycles(614 + 55 * std::bit_width(static_cast<unsigned>(vx)) + 26 * std::popcount(static_cast<unsigned>(vx)));
                    break;
                case 0x55:
                case 0x65: {
                    // I is not changed, the copy stops with an error when I passes 0xFFF
                    const bool store = (opcode & 0xFF) == 0x55;
                    for (unsigned index = 0; index <= x; ++index) {
                        if (store)
                            writeByte(_rI + index, _rV[index]);
                        else
                            _rV[index] = readByte(_rI + index);
                        if (index < x && _rI + index + 1 > 0xFFF) {
                            errorHalt("I OVERFLOW");
                            return;
                        }
                    }
                    addCycles((store ? 492 : 511) + 86 * x);
                    break;
                }
                case 0x75:
                case 0x85: {
                    const auto last = std::min<unsigned>(x, 7);
                    for (unsigned index = 0; index <= last; ++index) {
                        if ((opcode & 0xFF) == 0x75)
                            _rplFlags[index] = _rV[index];
                        else
                            _rV[index] = _rplFlags[index];
                    }
                    addCycles(((opcode & 0xFF) == 0x75 ? 493 : 512) + 77 * last);
                    break;
                }
                default:
                    errorHalt(fmt::format("INVALID OPCODE: {:04X}", opcode));
                    break;
            }
            break;
        default:
            break;
    }
}

void SchipStrictEmulator::setLcdPixel(int x, int y, bool on)
{
    _lcd[y * SCREEN_WIDTH + x] = on ? 1 : 0;
    _screen.setPixel(x, y, on ? 1 : 0);
}

void SchipStrictEmulator::refreshScreen()
{
    for (int y = 0; y < SCREEN_HEIGHT; ++y) {
        for (int x = 0; x < SCREEN_WIDTH; ++x) {
            _screen.setPixel(x, y, _lcd[y * SCREEN_WIDTH + x]);
        }
    }
}

void SchipStrictEmulator::drawLores(uint16_t opcode)
{
    // Every lores pixel is two LCD pixels wide and high. The sprite is placed in a 16 pixel
    // wide window starting at x & ~7, which is cut to 8 pixels at the right edge. The new
    // content of the window is computed from the first LCD row and written to both.
    const int x = _rV[(opcode >> 8) & 0xF] & 63;
    const int y = _rV[(opcode >> 4) & 0xF] & 31;
    const int n = opcode & 0xF;
    const int lines = n == 0 ? 16 : n - std::max(0, n + y - 32);
    const int windowStart = (x & ~7) * 2;
    const int windowWidth = x >= 56 ? 16 : 32;
    const int shift = x & 7;
    int bits = 0;
    int collidingLines = 0;
    for (int line = 0; line < lines; ++line) {
        const uint8_t data = readByte(_rI + line);
        bits += std::popcount(static_cast<unsigned>(data));
        const int row = y + line;
        if (row >= 32) {
            continue;
        }
        bool collision = false;
        uint8_t window[32];
        for (int pixel = 0; pixel < windowWidth; ++pixel) {
            const int spritePixel = pixel / 2 - shift;
            const bool set = spritePixel >= 0 && spritePixel < 8 && ((data >> (7 - spritePixel)) & 1);
            const uint8_t current = _lcd[row * 2 * SCREEN_WIDTH + windowStart + pixel];
            collision |= set && current;
            window[pixel] = current ^ (set ? 1 : 0);
        }
        for (int pixel = 0; pixel < windowWidth; ++pixel) {
            setLcdPixel(windowStart + pixel, row * 2, window[pixel]);
            setLcdPixel(windowStart + pixel, row * 2 + 1, window[pixel]);
        }
        if (collision)
            ++collidingLines;
    }
    _rV[0xF] = collidingLines ? 1 : 0;
    addCycles(1232 + lines * (405 + 80 * (8 + shift)) + 3 * bits + 10 * collidingLines - (collidingLines ? 2 : 0) - (x >= 56 ? 38 * lines : 0) +
              (n != 0 && n + y >= 32 ? 2 : 0));
}

int64_t SchipStrictEmulator::drawHires(uint16_t opcode)
{
    // The sprite is placed in a window of 3 (or 5 for 16x16) nibbles starting at x & ~3,
    // cut at the right edge, there is no wrapping.
    const int x = _rV[(opcode >> 8) & 0xF] & 127;
    const int y = _rV[(opcode >> 4) & 0xF] & 63;
    const int n = opcode & 0xF;
    const bool big = n == 0;
    const int rows = big ? 16 : n;
    const int lines = rows - std::max(0, rows + y - 64);
    const int width = big ? 16 : 8;
    const int nibbles = std::min(big ? 5 : 3, 32 - (x >> 2));
    const int windowStart = x & ~3;
    const int shift = x & 3;
    int collidingLines = 0;
    for (int line = 0; line < lines; ++line) {
        const unsigned data = big ? (readByte(_rI + line * 2) << 8) | readByte(_rI + line * 2 + 1) : readByte(_rI + line);
        bool collision = false;
        for (int pixel = 0; pixel < nibbles * 4; ++pixel) {
            const int spritePixel = pixel - shift;
            if (spritePixel < 0 || spritePixel >= width || !((data >> (width - 1 - spritePixel)) & 1))
                continue;
            auto& current = _lcd[(y + line) * SCREEN_WIDTH + windowStart + pixel];
            collision |= current != 0;
            setLcdPixel(windowStart + pixel, y + line, !current);
        }
        if (collision)
            ++collidingLines;
    }
    _rV[0xF] = collidingLines + rows - lines;
    const auto lineCycles = big ? HIRES_LINE_CYCLES_16[nibbles] : HIRES_LINE_CYCLES_8[nibbles];
    return 1306 + (big ? 4 : 0) + (rows + y >= 64 ? 2 : 0) + lines * (lineCycles + 60 * (width + shift)) - 2 * collidingLines;
}

void SchipStrictEmulator::clearScreen()
{
    _lcd.fill(0);
    _screen.setAll(0);
}

void SchipStrictEmulator::scrollDown(int rows)
{
    for (int y = SCREEN_HEIGHT - 1; y >= rows; --y)
        std::memcpy(_lcd.data() + y * SCREEN_WIDTH, _lcd.data() + (y - rows) * SCREEN_WIDTH, SCREEN_WIDTH);
    std::memset(_lcd.data(), 0, rows * SCREEN_WIDTH);
    refreshScreen();
}

void SchipStrictEmulator::scrollLeft()
{
    for (int y = 0; y < SCREEN_HEIGHT; ++y) {
        auto* row = _lcd.data() + y * SCREEN_WIDTH;
        std::memmove(row, row + 4, SCREEN_WIDTH - 4);
        std::memset(row + SCREEN_WIDTH - 4, 0, 4);
    }
    refreshScreen();
}

void SchipStrictEmulator::scrollRight()
{
    for (int y = 0; y < SCREEN_HEIGHT; ++y) {
        auto* row = _lcd.data() + y * SCREEN_WIDTH;
        std::memmove(row + 4, row, SCREEN_WIDTH - 4);
        std::memset(row, 0, 4);
    }
    refreshScreen();
}

void SchipStrictEmulator::setBuzzer(bool high, int64_t cycle)
{
    if (high == _buzzerHigh)
        return;
    std::lock_guard lock(_audioMutex);
    advanceBuzzer(cycle);
    _buzzerHigh = high;
}

void SchipStrictEmulator::advanceBuzzer(int64_t cycle)
{
    // Generates the samples between the last edge and cycle, the remainder keeps the
    // fractional samples so the audio stays locked to the machine cycles.
    if (cycle <= _buzzerCycle)
        return;
    const auto scaledSamples = (cycle - _buzzerCycle) * _audioSampleFrequency + _buzzerSampleRemainder;
    const auto sampleCount = scaledSamples / static_cast<int64_t>(CPU_CLOCK_FREQUENCY);
    _buzzerSampleRemainder = scaledSamples % static_cast<int64_t>(CPU_CLOCK_FREQUENCY);
    for (int64_t index = 0; index < sampleCount; ++index) {
        if (!_buzzerSamples.push(nextBuzzerSample()))
            break;
    }
    _buzzerCycle = cycle;
}

int16_t SchipStrictEmulator::nextBuzzerSample()
{
    return _buzzerHighPass(_buzzerLowPass(_buzzerHigh ? BUZZER_AMPLITUDE : 0));
}

void SchipStrictEmulator::configureBuzzer(int sampleFrequency)
{
    const auto rate = static_cast<uint32_t>(sampleFrequency);
    _audioSampleFrequency = sampleFrequency;
    _buzzerLowPass = LowPassButterworthFilter(rate, BUZZER_LOW_PASS_CUTOFF_HZ);
    _buzzerHighPass = HighPassButterworthFilter(rate, BUZZER_HIGH_PASS_CUTOFF_HZ);
    _buzzerSamples.reset();
    resetBuzzerOutput();
}

void SchipStrictEmulator::resetBuzzerOutput()
{
    const int16_t steadySample = _buzzerHigh ? BUZZER_AMPLITUDE : 0;
    _buzzerSampleRemainder = 0;
    _buzzerLowPass.reset(steadySample);
    _buzzerHighPass.reset(steadySample);
}

void SchipStrictEmulator::renderAudio(int16_t* samples, size_t frames, int sampleFrequency)
{
    if (sampleFrequency <= 0) {
        IEmulationCore::renderAudio(samples, frames, sampleFrequency);
        return;
    }
    std::lock_guard lock(_audioMutex);
    if (sampleFrequency != _audioSampleFrequency)
        configureBuzzer(sampleFrequency);
    // Samples nobody fetched, e.g. from single stepping, would otherwise delay the sound for good.
    while (_buzzerSamples.size() > frames + BUZZER_MAX_BACKLOG)
        (void)_buzzerSamples.pop();
    const auto fetched = _buzzerSamples.pop(samples, frames);
    for (size_t index = fetched; index < frames; ++index)
        samples[index] = nextBuzzerSample();
}

}  // namespace emu
