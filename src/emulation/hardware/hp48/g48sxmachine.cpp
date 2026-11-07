//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/g48sxmachine.cpp
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

#include "g48sxmachine.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>

#include <chiplet/sha1.hpp>
#include <nlohmann/json.hpp>

#include <emulation/hardware/hp48/g48statefile.hpp>
#include <emulation/hardware/hp48/binary.hpp>

namespace hp48 {

namespace fs = ghc::filesystem;

namespace {

struct KeyMatrixEntry {
    std::string_view name;
    std::uint8_t row;
    std::uint16_t inputMask;
};

constexpr KeyMatrixEntry KEY_MATRIX[] = {
    {"ON", 0, 0x8000},
    {"A", 1, 0x10},
    {"B", 8, 0x10},
    {"C", 8, 0x08},
    {"D", 8, 0x04},
    {"E", 8, 0x02},
    {"F", 8, 0x01},
    {"MTH", 2, 0x10},
    {"PRG", 7, 0x10},
    {"CST", 7, 0x08},
    {"VAR", 7, 0x04},
    {"UP", 7, 0x02},
    {"NXT", 7, 0x01},
    {"STO", 6, 0x10},
    {"EVAL", 6, 0x08},
    {"LEFT", 6, 0x04},
    {"DOWN", 6, 0x02},
    {"RIGHT", 6, 0x01},
    {"SIN", 3, 0x10},
    {"COS", 5, 0x10},
    {"TAN", 5, 0x08},
    {"SQRT", 5, 0x04},
    {"Yx", 5, 0x02},
    {"YX", 5, 0x02},
    {"POW", 5, 0x02},
    {"PWR", 5, 0x02},
    {"POWER", 5, 0x02},
    {"1/x", 5, 0x01},
    {"INV", 5, 0x01},
    {"ENTER", 4, 0x10},
    {"+/-", 4, 0x08},
    {"NEG", 4, 0x08},
    {"EEX", 4, 0x04},
    {"DEL", 4, 0x02},
    {"BKSP", 4, 0x01},
    {"BACKSPACE", 4, 0x01},
    {"ALPHA", 3, 0x20},
    {"7", 3, 0x08},
    {"N7", 3, 0x08},
    {"8", 3, 0x04},
    {"N8", 3, 0x04},
    {"9", 3, 0x02},
    {"N9", 3, 0x02},
    {"DIV", 3, 0x01},
    {"/", 3, 0x01},
    {"BKKEY", 2, 0x20},
    {"YELLOW", 2, 0x20},
    {"LSHIFT", 2, 0x20},
    {"4", 2, 0x08},
    {"N4", 2, 0x08},
    {"5", 2, 0x04},
    {"N5", 2, 0x04},
    {"6", 2, 0x02},
    {"N6", 2, 0x02},
    {"MULT", 2, 0x01},
    {"MUL", 2, 0x01},
    {"*", 2, 0x01},
    {"FWDKEY", 1, 0x20},
    {"BLUE", 1, 0x20},
    {"RSHIFT", 1, 0x20},
    {"ALPHA_A", 1, 0x10},
    {"1", 1, 0x08},
    {"N1", 1, 0x08},
    {"2", 1, 0x04},
    {"N2", 1, 0x04},
    {"3", 1, 0x02},
    {"N3", 1, 0x02},
    {"-", 1, 0x01},
    {"MINUS", 1, 0x01},
    {"'", 0, 0x10},
    {"QUOTE", 0, 0x10},
    {"0", 0, 0x08},
    {"N0", 0, 0x08},
    {".", 0, 0x04},
    {"DOT", 0, 0x04},
    {"SPC", 0, 0x02},
    {"SPACE", 0, 0x02},
    {"+", 0, 0x01},
    {"PLUS", 0, 0x01},
};

constexpr std::uint32_t SX_TEMPTOP = 0x7056f;
constexpr std::uint32_t SX_RSKTOP = 0x70574;
constexpr std::uint32_t SX_DSKTOP = 0x70579;
constexpr std::uint32_t SX_EDITLINE = 0x7057e;
constexpr std::uint32_t SX_AVMEM = 0x7066e;
constexpr std::int16_t BUZZER_AMPLITUDE = 16384;
constexpr std::uint32_t BUZZER_HIGH_PASS_CUTOFF_HZ = 80;
constexpr std::uint32_t BUZZER_LOW_PASS_CUTOFF_HZ = 10000;

std::optional<KeyMatrixEntry> lookupKey(std::string_view name)
{
    for (const KeyMatrixEntry& entry : KEY_MATRIX) {
        if (entry.name == name) {
            return entry;
        }
    }
    return std::nullopt;
}

void appendWordNibbles(std::vector<std::uint8_t>& nibbles, std::uint32_t value, int count = 5)
{
    for (int i = 0; i < count; ++i) {
        nibbles.push_back(static_cast<std::uint8_t>((value >> (i * 4)) & 0x0fU));
    }
}

std::vector<std::uint8_t> bytesToNibbles(std::span<const std::uint8_t> bytes)
{
    std::vector<std::uint8_t> nibbles;
    nibbles.reserve(bytes.size() * 2U);
    for (std::uint8_t byte : bytes) {
        nibbles.push_back(byte & 0x0fU);
        nibbles.push_back((byte >> 4U) & 0x0fU);
    }
    return nibbles;
}

bool lengthFieldObject(std::uint32_t prolog)
{
    switch (prolog) {
        case ObjectProlog::string:
        case ObjectProlog::grob:
        case ObjectProlog::library:
        case ObjectProlog::backup:
        case ObjectProlog::ext1:
        case ObjectProlog::code:
            return true;
        default:
            return false;
    }
}

std::vector<std::string> base64EncodeLines(std::span<const std::uint8_t> bytes)
{
    static constexpr char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    static constexpr std::size_t LINE_WIDTH = 76;
    std::string encoded;
    encoded.reserve(((bytes.size() + 2) / 3) * 4);

    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const std::uint32_t b0 = bytes[i];
        const std::uint32_t b1 = i + 1 < bytes.size() ? bytes[i + 1] : 0;
        const std::uint32_t b2 = i + 2 < bytes.size() ? bytes[i + 2] : 0;
        const std::uint32_t value = (b0 << 16U) | (b1 << 8U) | b2;
        encoded.push_back(ALPHABET[(value >> 18U) & 0x3fU]);
        encoded.push_back(ALPHABET[(value >> 12U) & 0x3fU]);
        encoded.push_back(i + 1 < bytes.size() ? ALPHABET[(value >> 6U) & 0x3fU] : '=');
        encoded.push_back(i + 2 < bytes.size() ? ALPHABET[value & 0x3fU] : '=');
    }

    std::vector<std::string> lines;
    lines.reserve((encoded.size() + LINE_WIDTH - 1) / LINE_WIDTH);
    for (std::size_t pos = 0; pos < encoded.size(); pos += LINE_WIDTH) {
        lines.push_back(encoded.substr(pos, LINE_WIDTH));
    }
    return lines;
}

std::uint8_t base64Value(char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<std::uint8_t>(ch - 'A');
    }
    if (ch >= 'a' && ch <= 'z') {
        return static_cast<std::uint8_t>(ch - 'a' + 26);
    }
    if (ch >= '0' && ch <= '9') {
        return static_cast<std::uint8_t>(ch - '0' + 52);
    }
    if (ch == '+') {
        return 62;
    }
    if (ch == '/') {
        return 63;
    }
    throw std::runtime_error("invalid base64 character in state file");
}

std::vector<std::uint8_t> base64DecodeLines(const std::vector<std::string>& lines, std::size_t expectedBytes)
{
    std::string encoded;
    for (const std::string& line : lines) {
        encoded += line;
    }
    if (encoded.size() % 4 != 0) {
        throw std::runtime_error("base64 state payload length is not divisible by four");
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve((encoded.size() / 4) * 3);
    for (std::size_t i = 0; i < encoded.size(); i += 4) {
        const bool pad2 = encoded[i + 2] == '=';
        const bool pad3 = encoded[i + 3] == '=';
        const std::uint32_t value =
            (static_cast<std::uint32_t>(base64Value(encoded[i])) << 18U)
            | (static_cast<std::uint32_t>(base64Value(encoded[i + 1])) << 12U)
            | (pad2 ? 0U : static_cast<std::uint32_t>(base64Value(encoded[i + 2])) << 6U)
            | (pad3 ? 0U : static_cast<std::uint32_t>(base64Value(encoded[i + 3])));
        bytes.push_back(static_cast<std::uint8_t>((value >> 16U) & 0xffU));
        if (!pad2) {
            bytes.push_back(static_cast<std::uint8_t>((value >> 8U) & 0xffU));
        }
        if (!pad3) {
            bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
        }
    }
    if (bytes.size() != expectedBytes) {
        throw std::runtime_error("base64 state payload has wrong decoded size");
    }
    return bytes;
}

std::string configStateName(Nce2RamController::ConfigState state)
{
    switch (state) {
        case Nce2RamController::ConfigState::AwaitingSize:
            return "awaiting-size";
        case Nce2RamController::ConfigState::AwaitingAddress:
            return "awaiting-address";
        case Nce2RamController::ConfigState::Configured:
            return "configured";
    }
    throw std::runtime_error("unknown RAM config state");
}

Nce2RamController::ConfigState parseRamConfigState(const std::string& state)
{
    if (state == "awaiting-size") {
        return Nce2RamController::ConfigState::AwaitingSize;
    }
    if (state == "awaiting-address") {
        return Nce2RamController::ConfigState::AwaitingAddress;
    }
    if (state == "configured") {
        return Nce2RamController::ConfigState::Configured;
    }
    throw std::runtime_error("unknown RAM config state in state file");
}

std::string configStateName(EmptyPortController::ConfigState state)
{
    switch (state) {
        case EmptyPortController::ConfigState::AwaitingSize:
            return "awaiting-size";
        case EmptyPortController::ConfigState::AwaitingAddress:
            return "awaiting-address";
        case EmptyPortController::ConfigState::Configured:
            return "configured";
    }
    throw std::runtime_error("unknown port config state");
}

EmptyPortController::ConfigState parsePortConfigState(const std::string& state)
{
    if (state == "awaiting-size") {
        return EmptyPortController::ConfigState::AwaitingSize;
    }
    if (state == "awaiting-address") {
        return EmptyPortController::ConfigState::AwaitingAddress;
    }
    if (state == "configured") {
        return EmptyPortController::ConfigState::Configured;
    }
    throw std::runtime_error("unknown port config state in state file");
}

std::uint32_t timer2CyclesPerTickForProfile(G48sxTimingProfile timingProfile)
{
    switch (timingProfile) {
        case G48sxTimingProfile::COMPARE:
            return HdwController::COMPARE_TIMER2_CYCLES_PER_TICK;
        case G48sxTimingProfile::REAL:
            return G48SX_REAL_TIMER2_CYCLES_PER_TICK;
    }
    return HdwController::COMPARE_TIMER2_CYCLES_PER_TICK;
}

std::string configStateName(HdwController::ConfigState state)
{
    switch (state) {
        case HdwController::ConfigState::AwaitingAddress:
            return "awaiting-address";
        case HdwController::ConfigState::Configured:
            return "configured";
    }
    throw std::runtime_error("unknown HDW config state");
}

HdwController::ConfigState parseHdwConfigState(const std::string& state)
{
    if (state == "awaiting-address") {
        return HdwController::ConfigState::AwaitingAddress;
    }
    if (state == "configured") {
        return HdwController::ConfigState::Configured;
    }
    throw std::runtime_error("unknown HDW config state in state file");
}

std::vector<std::uint8_t> readU8Vector(const nlohmann::json& json, std::string_view field)
{
    std::vector<std::uint8_t> result;
    result.reserve(json.size());
    for (const auto& entry : json) {
        const auto value = entry.get<unsigned>();
        if (value > 0x0fU && field == "nibbles") {
            throw std::runtime_error("nibble array value is out of range");
        }
        if (value > 0xffU) {
            throw std::runtime_error(std::string(field) + " array value is out of range");
        }
        result.push_back(static_cast<std::uint8_t>(value));
    }
    return result;
}

nlohmann::ordered_json portJson(const EmptyPortController::Snapshot& snapshot)
{
    return {
        {"configState", configStateName(snapshot.state)},
        {"baseAddress", snapshot.baseAddress},
        {"configuredSize", snapshot.configuredSize},
    };
}

EmptyPortController::Snapshot readPortSnapshot(const nlohmann::json& json)
{
    return {
        .state = parsePortConfigState(json.at("configState").get<std::string>()),
        .baseAddress = json.at("baseAddress").get<std::uint32_t>(),
        .configuredSize = json.at("configuredSize").get<std::uint32_t>(),
    };
}

} // namespace

G48sxStackObject G48sxStackObject::hpObject(const Binary& binary)
{
    if (!binary.isLoaded()) {
        throw std::runtime_error("cannot prepare an unloaded HP48 binary object");
    }

    G48sxStackObject object;
    object.nibbles = bytesToNibbles(binary.rawBytes());
    if (lengthFieldObject(binary.prolog())) {
        const std::uint32_t objectNibbles = 5U + binary.objectLength();
        if (objectNibbles > object.nibbles.size()) {
            throw std::runtime_error("HP48 binary length field exceeds object data");
        }
        object.nibbles.resize(objectNibbles);
    }
    return object;
}

G48sxStackObject G48sxStackObject::stringObject(std::span<const std::uint8_t> bytes)
{
    G48sxStackObject object;
    object.nibbles.reserve(10U + bytes.size() * 2U);
    appendWordNibbles(object.nibbles, ObjectProlog::string);
    appendWordNibbles(object.nibbles, static_cast<std::uint32_t>(bytes.size() * 2U + 5U));
    std::vector<std::uint8_t> payload = bytesToNibbles(bytes);
    object.nibbles.insert(object.nibbles.end(), payload.begin(), payload.end());
    return object;
}

bool isG48sxKeyLabel(std::string_view label)
{
    return lookupKey(label).has_value();
}

G48sxMachine::G48sxMachine(std::uint64_t resetKeyHoldInstructions,
                           std::vector<std::uint8_t> romBytes,
                           std::string romName,
                           G48sxTimingProfile timingProfile)
    : _hdw(timer2CyclesPerTickForProfile(timingProfile))
    , _ram(0x10000)
    , _port1(0xFF005, 0x000F6)
    , _port2(0xFF007, 0x000F8)
    , _nce3(0xFF001, 0x000F2)
    , _romCtrl(std::make_shared<Nce1RomController>(std::move(romBytes), std::move(romName)))
    , _bus({&_hdw, &_ram, &_port1, &_port2, &_nce3},
           {&_hdw, &_ram, &_port1, &_port2, &_nce3},
           _romCtrl)
    , _cpu(_bus)
    , _display(_frameBuffer, _dummyStack, _dummyMenu)
    , _timingProfile(timingProfile)
{
    _hdw.setCrcRegister(_cpu.crcRegister());
    _hdw.setCycleCounter(_cpu.cycleCounter());
    _cpu.setOutChangedCallback([this](std::uint16_t out) {
        buzzerChanged((out & 0x0800U) != 0);
    });
    _resetKeyHoldInstructions = resetKeyHoldInstructions;
    resetColdStart();
}

void G48sxMachine::resetColdStart()
{
    _cpu.reset();
    _instructionIndex = 0;
    _running = true;
    _paused = false;
    _errorMessage.clear();
    for (G48sxScheduledKeyPress& keyPress : _scheduledKeyPresses) {
        keyPress.pressed = false;
        keyPress.released = false;
    }
    press("ON");
    press("A");
    press("F");
    _resetKeysHeld = true;
    _cpu.clearPendingInterrupts();
    resetBuzzerState();
}

void G48sxMachine::scheduleKeyPress(G48sxScheduledKeyPress keyPress)
{
    _scheduledKeyPresses.push_back(std::move(keyPress));
}

void G48sxMachine::scheduleKeyPress(std::string label, std::uint64_t instruction, std::uint64_t holdInstructions)
{
    scheduleKeyPress(G48sxScheduledKeyPress {
        .label = std::move(label),
        .instruction = instruction,
        .holdInstructions = holdInstructions,
    });
}

void G48sxMachine::stepTo(std::uint64_t instructionTarget)
{
    while (_running && !_paused && _instructionIndex < instructionTarget) {
        stepOne();
    }
}

bool G48sxMachine::stepInstruction()
{
    if (!_running || _paused) {
        return false;
    }
    stepOne();
    return _running;
}

void G48sxMachine::stepFrame()
{
    const std::uint64_t frameCycles = cyclesPerEmulatedFrame();
    const std::uint64_t currentCycles = cycles();
    const std::uint64_t nextFrameCycles =
        ((currentCycles / frameCycles) + 1U) * frameCycles;
    stepCycles(nextFrameCycles - currentCycles);
}

void G48sxMachine::stepCycles(std::uint64_t cycleBudget)
{
    const std::uint64_t target = cycles() + cycleBudget;
    while (_running && !_paused && cycles() < target) {
        stepOne();
    }
}

void G48sxMachine::setTimingProfile(G48sxTimingProfile timingProfile)
{
    _timingProfile = timingProfile;
    _hdw.setTimer2CyclesPerTick(timer2CyclesPerTickForProfile(timingProfile));
}

std::uint64_t G48sxMachine::cyclesPerEmulatedFrame() const
{
    return static_cast<std::uint64_t>(timer2CyclesPerTick()) * G48SX_TIMER2_TICKS_PER_EMULATED_FRAME;
}

std::uint64_t G48sxMachine::cyclesPerSecond() const
{
    return static_cast<std::uint64_t>(timer2CyclesPerTick()) * HdwController::TIMER2_HZ;
}

std::vector<std::uint32_t> G48sxMachine::prepareRecoveredStackForEvalWithAddresses(
    std::span<const G48sxStackObject> objects)
{
    if (objects.empty()) {
        throw std::runtime_error("cannot prepare an empty HP48 stack");
    }

    const std::uint64_t oldResetKeyHoldInstructions = _resetKeyHoldInstructions;
    _scheduledKeyPresses.clear();
    _resetKeyHoldInstructions = G48SX_RECOVERY_RESET_KEY_HOLD_INSTRUCTIONS;
    resetColdStart();
    scheduleKeyPress("A", G48SX_RECOVERY_KEY_INSTRUCTION, G48SX_RECOVERY_KEY_HOLD_INSTRUCTIONS);
    stepTo(G48SX_RECOVERY_STACK_INSTRUCTIONS);
    _scheduledKeyPresses.clear();
    _resetKeyHoldInstructions = oldResetKeyHoldInstructions;
    if (!_running) {
        throw std::runtime_error("failed to recover HP48SX stack before object preparation");
    }

    std::vector<std::uint32_t> objectAddresses = pushStackObjectsForEval(objects);
    const std::vector<std::uint8_t> responsiveState = saveStateBytes(false);

    scheduleKeyPress("ON", _instructionIndex + 1000U, 20000U);
    scheduleKeyPress("VAR", _instructionIndex + 300000U, 20000U);
    stepTo(_instructionIndex + 4000000U);
    _scheduledKeyPresses.clear();
    if (!_running) {
        throw std::runtime_error("failed to redraw HP48SX stack after object preparation");
    }

    const DisplayRamLayout sourceLayout = displayRamLayout();
    const std::vector<std::uint8_t> displayRam = readDisplayRam(sourceLayout);
    loadState(responsiveState);
    const DisplayRamLayout targetLayout = displayRamLayout();
    if (!compatibleDisplayRamLayouts(sourceLayout, targetLayout)) {
        throw std::runtime_error("HP48SX display layout changed while preparing visible stack");
    }
    writeDisplayRam(targetLayout, displayRam);
    return objectAddresses;
}

std::vector<std::uint32_t> G48sxMachine::pushStackObjectsForEval(
    std::span<const G48sxStackObject> objects)
{
    if (objects.empty()) {
        throw std::runtime_error("cannot push an empty HP48 stack object list");
    }
    std::vector<std::uint32_t> objectAddresses;
    objectAddresses.reserve(objects.size());
    for (const G48sxStackObject& object : objects) {
        const std::uint32_t objectAddress = createTempObject(object.nibbles);
        objectAddresses.push_back(objectAddress);
        pushStackObject(objectAddress);
    }
    syncSavedRplRegisters();
    return objectAddresses;
}

std::uint32_t G48sxMachine::insertStackObjectForEval(
    const G48sxStackObject& object,
    std::uint32_t level)
{
    const std::uint32_t objectAddress = createTempObject(object.nibbles);
    pushStackObject(objectAddress, level);
    syncSavedRplRegisters();
    return objectAddress;
}

void G48sxMachine::prepareRecoveredStackForEval(std::span<const G48sxStackObject> objects)
{
    (void)prepareRecoveredStackForEvalWithAddresses(objects);
}

void G48sxMachine::prepareRecoveredStackForEval(std::span<const Binary> binaries)
{
    std::vector<G48sxStackObject> objects;
    objects.reserve(binaries.size());
    for (const Binary& binary : binaries) {
        objects.push_back(G48sxStackObject::hpObject(binary));
    }
    prepareRecoveredStackForEval(objects);
}

void G48sxMachine::render(ghc::span2d<std::uint32_t> output, int scale)
{
    updateFrameBufferFromLcdMemory();
    updateDisplayAnnunciators();
    _display.setContrast(_fixedContrast.value_or(registerContrast()));
    _display.render(output, Hp48Display::Mode::FrameBuffer, scale);
}

int G48sxMachine::registerContrast() const
{
    return readNibble(HDW_BASE + 0x01) | ((readNibble(HDW_BASE + 0x02) & 0x01U) << 4U);
}

void G48sxMachine::renderAudio(std::int16_t* sampleBuffer, std::size_t frames, int sampleFrequency)
{
    if (sampleBuffer == nullptr || frames == 0) {
        return;
    }

    std::lock_guard lock(_audioMutex);
    if (sampleFrequency > 0 && sampleFrequency != _audioSampleFrequency) {
        configureBuzzerFilters(sampleFrequency);
    }
    updateBuzzerSamples(_buzzerState, _audioCurrentCycle.load(std::memory_order_acquire));

    const std::size_t copied = _audioSamples.pop(sampleBuffer, frames);
    for (std::size_t i = copied; i < frames; ++i) {
        sampleBuffer[i] = filteredBuzzerSample(_buzzerState);
    }
}

void G48sxMachine::writePbm(const std::string& path)
{
    updateFrameBufferFromLcdMemory();
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open PBM output");
    }
    out << "P1\n131 64\n";
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 131; ++x) {
            const bool lit = (_frameBuffer[y * 17 + (x >> 3)] & (0x80U >> (x & 7))) != 0;
            out << (lit ? '1' : '0') << (x == 130 ? '\n' : ' ');
        }
    }
}

std::vector<std::uint8_t> G48sxMachine::saveStateBytes(bool compressed)
{
    const auto ramSnapshot = _ram.snapshot();
    const auto hdwSnapshot = _hdw.snapshot();
    const auto port1Snapshot = _port1.snapshot();
    const auto port2Snapshot = _port2.snapshot();
    const auto nce3Snapshot = _nce3.snapshot();
    const auto runtime = _cpu.getRuntimeState();

    nlohmann::ordered_json state;
    state["$schema"] = G48SX_STATE_SCHEMA;
    state["format"] = "G48sx state";
    state["version"] = G48SX_STATE_FILE_VERSION;
    state["machine"] = "HP48SX";
    state["rom"] = {
        {"name", _romCtrl->romName()},
        {"sha1", currentRomSha1()},
        {"packedBytes", G48SX_ROM_NIBBLES / 2},
    };
    state["timing"] = {
        {"timer2Hz", HdwController::TIMER2_HZ},
        {"timer2CyclesPerTick", timer2CyclesPerTick()},
        {"emulatedFrameHz", G48SX_EMULATED_FRAME_HZ},
        {"cyclesPerEmulatedFrame", cyclesPerEmulatedFrame()},
    };
    state["emulator"] = {
        {"instructionIndex", _instructionIndex},
    };
    state["cpu"] = {
        {"registers", _cpu.getState()},
        {"runtime", {
            {"keyboardRows", runtime.keyboardRows},
            {"in", runtime.in},
            {"out", runtime.out},
            {"pcOpcode", runtime.pcOpcode},
            {"cycles", runtime.cycles},
            {"shutdown", runtime.shutdown},
            {"softInterrupt", runtime.softInterrupt},
            {"interruptsEnabled", runtime.interruptsEnabled},
            {"keyboardInterruptsEnabled", runtime.keyboardInterruptsEnabled},
            {"keyboardInterruptPending", runtime.keyboardInterruptPending},
            {"shutdownWakePending", runtime.shutdownWakePending},
            {"onKeyPressed", runtime.onKeyPressed},
        }},
    };
    state["hardware"] = {
        {"hdw", {
            {"configState", configStateName(hdwSnapshot.state)},
            {"baseAddress", hdwSnapshot.baseAddress},
            {"registers", hdwSnapshot.registers},
            {"timerCounter", hdwSnapshot.timerCounter},
            {"timer1Counter", hdwSnapshot.timer1Counter},
            {"timer2LowReadCount", hdwSnapshot.timer2LowReadCount},
            {"timer1CycleRef", hdwSnapshot.timer1CycleRef},
            {"timer2CycleRef", hdwSnapshot.timer2CycleRef},
            {"timersStarted", hdwSnapshot.timersStarted},
            {"shutdownActive", hdwSnapshot.shutdownActive},
        }},
        {"ram", {
            {"configState", configStateName(ramSnapshot.state)},
            {"baseAddress", ramSnapshot.baseAddress},
            {"configuredSize", ramSnapshot.configuredSize},
            {"physicalNibbles", _ram.physicalNibbles()},
            {"packedBytes", ramSnapshot.packedBytes.size()},
            {"encoding", "base64-lines"},
            {"data", base64EncodeLines(ramSnapshot.packedBytes)},
        }},
        {"ports", {
            {"port1", portJson(port1Snapshot)},
            {"port2", portJson(port2Snapshot)},
            {"nce3", portJson(nce3Snapshot)},
        }},
    };

    return g48statefile::encodeStateJson(state, compressed);
}

void G48sxMachine::saveState(const fs::path& path, bool compressed)
{
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path());
    }
    const std::vector<std::uint8_t> bytes = saveStateBytes(compressed);
    std::ofstream out(path.string(), std::ios::binary);
    if (!out) {
        throw std::runtime_error("failed to open G48sx state file for writing");
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        throw std::runtime_error("failed to write G48sx state file");
    }
}

void G48sxMachine::loadState(std::span<const std::uint8_t> stateBytes)
{
    const nlohmann::json state = g48statefile::stateJsonFromBytes(stateBytes);
    if (state.at("version").get<std::uint32_t>() != G48SX_STATE_FILE_VERSION) {
        throw std::runtime_error("unsupported G48sx state file version");
    }
    if (state.at("machine").get<std::string>() != "HP48SX") {
        throw std::runtime_error("G48sx state file is not for HP48SX");
    }
    const std::string stateRomSha1 = state.at("rom").at("sha1").get<std::string>();
    const std::string localRomSha1 = currentRomSha1();
    if (stateRomSha1 != localRomSha1) {
        throw std::runtime_error("G48sx state ROM SHA1 does not match current ROM");
    }
    if (state.at("rom").at("packedBytes").get<std::size_t>() != G48SX_ROM_NIBBLES / 2) {
        throw std::runtime_error("G48sx state ROM packed byte count does not match");
    }

    const auto& hardware = state.at("hardware");
    const auto& ramJson = hardware.at("ram");
    Nce2RamController::Snapshot ramSnapshot;
    ramSnapshot.state = parseRamConfigState(ramJson.at("configState").get<std::string>());
    ramSnapshot.baseAddress = ramJson.at("baseAddress").get<std::uint32_t>();
    ramSnapshot.configuredSize = ramJson.at("configuredSize").get<std::uint32_t>();
    if (ramJson.at("physicalNibbles").get<std::size_t>() != _ram.physicalNibbles()) {
        throw std::runtime_error("G48sx state RAM physical nibble count does not match");
    }
    if (ramJson.at("encoding").get<std::string>() != "base64-lines") {
        throw std::runtime_error("unsupported G48sx state RAM encoding");
    }
    ramSnapshot.packedBytes = base64DecodeLines(ramJson.at("data").get<std::vector<std::string>>(),
                                                ramJson.at("packedBytes").get<std::size_t>());
    _ram.restoreSnapshot(ramSnapshot);

    const auto& hdwJson = hardware.at("hdw");
    HdwController::Snapshot hdwSnapshot;
    hdwSnapshot.state = parseHdwConfigState(hdwJson.at("configState").get<std::string>());
    hdwSnapshot.baseAddress = hdwJson.at("baseAddress").get<std::uint32_t>();
    hdwSnapshot.registers = readU8Vector(hdwJson.at("registers"), "nibbles");
    // Older state files also have "waitAsserted" and "waitPenaltyCycles" from an unused CPU stall draft, they are ignored
    hdwSnapshot.timerCounter = hdwJson.at("timerCounter").get<std::uint32_t>();
    hdwSnapshot.timer1Counter = hdwJson.at("timer1Counter").get<std::uint8_t>();
    hdwSnapshot.timer2LowReadCount = hdwJson.at("timer2LowReadCount").get<std::uint32_t>();
    hdwSnapshot.timer1CycleRef = hdwJson.at("timer1CycleRef").get<std::uint64_t>();
    hdwSnapshot.timer2CycleRef = hdwJson.at("timer2CycleRef").get<std::uint64_t>();
    hdwSnapshot.timersStarted = hdwJson.at("timersStarted").get<bool>();
    hdwSnapshot.shutdownActive = hdwJson.at("shutdownActive").get<bool>();
    _hdw.restoreSnapshot(hdwSnapshot);

    const auto& ports = hardware.at("ports");
    _port1.restoreSnapshot(readPortSnapshot(ports.at("port1")));
    _port2.restoreSnapshot(readPortSnapshot(ports.at("port2")));
    _nce3.restoreSnapshot(readPortSnapshot(ports.at("nce3")));

    _cpu.setState(state.at("cpu").at("registers").get<SaturnState>());
    const auto& runtimeJson = state.at("cpu").at("runtime");
    SaturnL2::RuntimeState runtime;
    runtimeJson.at("keyboardRows").get_to(runtime.keyboardRows);
    runtime.in = runtimeJson.at("in").get<std::uint16_t>();
    runtime.out = runtimeJson.at("out").get<std::uint16_t>();
    runtime.pcOpcode = runtimeJson.at("pcOpcode").get<std::uint32_t>();
    runtime.cycles = runtimeJson.at("cycles").get<std::uint64_t>();
    runtime.shutdown = runtimeJson.at("shutdown").get<bool>();
    runtime.softInterrupt = runtimeJson.at("softInterrupt").get<bool>();
    runtime.interruptsEnabled = runtimeJson.at("interruptsEnabled").get<bool>();
    runtime.keyboardInterruptsEnabled = runtimeJson.at("keyboardInterruptsEnabled").get<bool>();
    runtime.keyboardInterruptPending = runtimeJson.at("keyboardInterruptPending").get<bool>();
    runtime.shutdownWakePending = runtimeJson.at("shutdownWakePending").get<bool>();
    runtime.onKeyPressed = runtimeJson.at("onKeyPressed").get<bool>();
    _cpu.setRuntimeState(runtime);

    _instructionIndex = state.at("emulator").at("instructionIndex").get<std::uint64_t>();
    _resetKeysHeld = false;
    _running = true;
    _paused = false;
    resetBuzzerState();
}

void G48sxMachine::loadState(const fs::path& path)
{
    const std::vector<std::uint8_t> bytes = g48statefile::readFileBytes(path);
    loadState(bytes);
}

void G48sxMachine::press(std::string_view label)
{
    if (const auto key = lookupKey(label)) {
        _cpu.pressKey(key->row, key->inputMask);
    }
}

void G48sxMachine::release(std::string_view label)
{
    if (const auto key = lookupKey(label)) {
        _cpu.releaseKey(key->row, key->inputMask);
    }
}

std::uint8_t G48sxMachine::readNibble(std::uint32_t address) const
{
    return const_cast<SystemBus&>(_bus).readNibble(address) & 0x0fU;
}

std::uint8_t G48sxMachine::readByte(std::uint32_t byteAddress) const
{
    const std::uint32_t nibbleAddress = byteAddress * 2U;
    return readNibble(nibbleAddress)
        | static_cast<std::uint8_t>(readNibble(nibbleAddress + 1U) << 4U);
}

void G48sxMachine::writeNibble(std::uint32_t address, std::uint8_t value)
{
    _bus.writeNibble(address, value);
}

void G48sxMachine::writeByte(std::uint32_t byteAddress, std::uint8_t value)
{
    const std::uint32_t nibbleAddress = byteAddress * 2U;
    writeNibble(nibbleAddress, value & 0x0fU);
    writeNibble(nibbleAddress + 1U, value >> 4U);
}

std::string G48sxMachine::currentRomSha1()
{
    std::vector<std::uint8_t> packedRom(G48SX_ROM_NIBBLES / 2);
    for (std::uint32_t byteIndex = 0; byteIndex < packedRom.size(); ++byteIndex) {
        const std::uint32_t nibbleAddress = byteIndex * 2;
        const std::uint8_t lo = _romCtrl->readNibble(nibbleAddress) & 0x0fU;
        const std::uint8_t hi = _romCtrl->readNibble(nibbleAddress + 1) & 0x0fU;
        packedRom[byteIndex] = static_cast<std::uint8_t>(lo | (hi << 4U));
    }
    Sha1 sha1;
    sha1.add(packedRom.data(), static_cast<std::uint32_t>(packedRom.size())).finalize();
    return static_cast<Sha1::Digest>(sha1).to_hex();
}

void G48sxMachine::buzzerChanged(bool state)
{
    const std::uint64_t currentCycle = cycles();
    _audioCurrentCycle.store(currentCycle, std::memory_order_release);
    std::lock_guard lock(_audioMutex);
    updateBuzzerSamples(state, currentCycle);
}

void G48sxMachine::resetBuzzerState()
{
    const std::uint64_t currentCycle = cycles();
    _audioCurrentCycle.store(currentCycle, std::memory_order_release);
    std::lock_guard lock(_audioMutex);
    _audioSamples.reset();
    _buzzerState = (_cpu.getOUT() & 0x0800U) != 0;
    _buzzerCycle = currentCycle;
    _buzzerSampleRemainder = 0;
    if (_audioSampleFrequency > 0) {
        configureBuzzerFilters(_audioSampleFrequency);
    }
}

void G48sxMachine::updateBuzzerSamples(bool state, std::uint64_t currentCycle)
{
    if (currentCycle < _buzzerCycle) {
        _buzzerCycle = currentCycle;
        _buzzerSampleRemainder = 0;
        _buzzerState = state;
        return;
    }

    if (_audioSampleFrequency <= 0) {
        _buzzerCycle = currentCycle;
        _buzzerSampleRemainder = 0;
        _buzzerState = state;
        return;
    }

    const std::uint64_t deltaCycles = currentCycle - _buzzerCycle;
    const std::uint64_t scaledSamples = deltaCycles * static_cast<std::uint64_t>(_audioSampleFrequency)
                                      + _buzzerSampleRemainder;
    const std::uint64_t sampleCount = scaledSamples / cyclesPerSecond();
    _buzzerSampleRemainder = scaledSamples % cyclesPerSecond();

    for (std::uint64_t i = 0; i < sampleCount; ++i) {
        if (!_audioSamples.push(filteredBuzzerSample(_buzzerState))) {
            break;
        }
    }

    _buzzerCycle = currentCycle;
    _buzzerState = state;
}

std::int16_t G48sxMachine::filteredBuzzerSample(bool state)
{
    std::int16_t sample = state ? BUZZER_AMPLITUDE : 0;
    if (_buzzerLowPassFilter) {
        sample = (*_buzzerLowPassFilter)(sample);
    }
    if (_buzzerHighPassFilter) {
        sample = (*_buzzerHighPassFilter)(sample);
    }
    return sample;
}

void G48sxMachine::configureBuzzerFilters(int sampleFrequency)
{
    if (sampleFrequency <= 0) {
        _audioSampleFrequency = 0;
        _buzzerLowPassFilter.reset();
        _buzzerHighPassFilter.reset();
        return;
    }

    _audioSampleFrequency = sampleFrequency;
    _buzzerSampleRemainder = 0;
    _buzzerCycle = _audioCurrentCycle.load(std::memory_order_acquire);

    const auto rate = static_cast<std::uint32_t>(sampleFrequency);
    const std::int16_t steadySample = _buzzerState ? BUZZER_AMPLITUDE : 0;
    _buzzerLowPassFilter.emplace(rate, BUZZER_LOW_PASS_CUTOFF_HZ);
    _buzzerLowPassFilter->reset(steadySample);
    _buzzerHighPassFilter.emplace(rate, BUZZER_HIGH_PASS_CUTOFF_HZ);
    _buzzerHighPassFilter->reset(steadySample);
}

void G48sxMachine::stepOne()
{
    if (_resetKeysHeld && _instructionIndex >= _resetKeyHoldInstructions) {
        release("ON");
        release("A");
        release("F");
        _resetKeysHeld = false;
    }
    for (G48sxScheduledKeyPress& keyPress : _scheduledKeyPresses) {
        if (!keyPress.pressed && _instructionIndex >= keyPress.instruction) {
            press(keyPress.label);
            keyPress.pressed = true;
        }
        if (keyPress.pressed && !keyPress.released
            && _instructionIndex >= keyPress.instruction + keyPress.holdInstructions) {
            release(keyPress.label);
            keyPress.released = true;
        }
    }

    while (_cpu.servicePendingInterrupt()) {
    }

    try {
        _cpu.executeInstruction();
    } catch (const UnimplementedInstruction& error) {
        _audioCurrentCycle.store(cycles(), std::memory_order_release);
        std::cerr << "[G48sx] Unimplemented \"" << error.where()
                  << "\" at i=" << _instructionIndex
                  << " pc=0x" << std::hex << _cpu.getPC() << std::dec << '\n';
        _running = false;
        _errorMessage = std::string("Unimplemented ") + error.where();
        return;
    }
    _audioCurrentCycle.store(cycles(), std::memory_order_release);
    ++_instructionIndex;
}

std::uint8_t G48sxMachine::readHdw(std::uint32_t offset)
{
    return _bus.readNibble(HDW_BASE + offset) & 0x0fU;
}

std::uint32_t G48sxMachine::readPacked(std::uint32_t address, int count)
{
    std::uint32_t value = 0;
    for (int i = 0; i < count; ++i) {
        value |= static_cast<std::uint32_t>(_bus.readNibble(address + i) & 0x0fU) << (i * 4);
    }
    return value;
}

std::uint32_t G48sxMachine::readPackedHdw(std::uint32_t offset, int count)
{
    return readPacked(HDW_BASE + offset, count);
}

void G48sxMachine::writePacked(std::uint32_t address, std::uint32_t value, int count)
{
    for (int i = 0; i < count; ++i) {
        _bus.writeNibble(address + static_cast<std::uint32_t>(i), static_cast<std::uint8_t>((value >> (i * 4)) & 0x0fU));
    }
}

void G48sxMachine::writeNibbles(std::uint32_t address, std::span<const std::uint8_t> nibbles)
{
    for (std::size_t i = 0; i < nibbles.size(); ++i) {
        _bus.writeNibble(address + static_cast<std::uint32_t>(i), nibbles[i] & 0x0fU);
    }
}

void G48sxMachine::moveNibbles(std::uint32_t source, std::uint32_t destination, std::uint32_t count)
{
    if (count == 0 || source == destination) {
        return;
    }
    if (destination > source) {
        for (std::uint32_t i = count; i > 0; --i) {
            _bus.writeNibble(destination + i - 1U, _bus.readNibble(source + i - 1U) & 0x0fU);
        }
    } else {
        for (std::uint32_t i = 0; i < count; ++i) {
            _bus.writeNibble(destination + i, _bus.readNibble(source + i) & 0x0fU);
        }
    }
}

std::uint32_t G48sxMachine::stackDepth()
{
    const std::uint32_t dskTop = readPacked(SX_DSKTOP, 5);
    const std::uint32_t editLine = readPacked(SX_EDITLINE, 5);
    if (editLine <= dskTop || ((editLine - dskTop) % 5U) != 0) {
        throw std::runtime_error("HP48SX stack control words are not in the expected shape");
    }
    return ((editLine - dskTop) / 5U) - 1U;
}

std::uint32_t G48sxMachine::createTempObject(std::span<const std::uint8_t> object)
{
    const std::uint32_t objectNibbles = static_cast<std::uint32_t>(object.size());
    const std::uint32_t allocationNibbles = objectNibbles + 6U;
    const std::uint32_t tempTop = readPacked(SX_TEMPTOP, 5);
    const std::uint32_t rskTop = readPacked(SX_RSKTOP, 5);
    const std::uint32_t dskTop = readPacked(SX_DSKTOP, 5);

    if (rskTop < tempTop) {
        throw std::runtime_error("HP48SX temp/return-stack control words are inconsistent");
    }
    if (rskTop + allocationNibbles > dskTop) {
        throw std::runtime_error(
            "not enough HP48SX memory for prepared stack object (need "
            + std::to_string(allocationNibbles) + " nibbles, have "
            + std::to_string(dskTop - rskTop) + ")");
    }

    moveNibbles(tempTop, tempTop + allocationNibbles, rskTop - tempTop);
    writePacked(SX_TEMPTOP, tempTop + allocationNibbles, 5);
    writePacked(SX_RSKTOP, rskTop + allocationNibbles, 5);
    writePacked(SX_AVMEM, (dskTop - rskTop - allocationNibbles) / 5U, 5);
    writePacked(tempTop + allocationNibbles - 5U, allocationNibbles, 5);

    const std::uint32_t objectAddress = tempTop + 1U;
    writeNibbles(objectAddress, object);
    return objectAddress;
}

void G48sxMachine::pushStackObject(std::uint32_t objectAddress, std::uint32_t level)
{
    if (level == 0) {
        throw std::runtime_error("HP48SX stack level must be one or greater");
    }
    const std::uint32_t depth = stackDepth();
    if (level > depth + 1U) {
        throw std::runtime_error("HP48SX stack level is deeper than DEPTH+1");
    }

    std::uint32_t avMem = readPacked(SX_AVMEM, 5);
    if (avMem == 0) {
        throw std::runtime_error("no free HP48SX memory words remain for stack push");
    }
    writePacked(SX_AVMEM, avMem - 1U, 5);

    const std::uint32_t oldDskTop = readPacked(SX_DSKTOP, 5);
    const std::uint32_t newDskTop = oldDskTop - 5U;
    if (newDskTop < readPacked(SX_RSKTOP, 5)) {
        throw std::runtime_error("not enough HP48SX memory for another stack pointer");
    }
    writePacked(SX_DSKTOP, newDskTop, 5);

    std::uint32_t slot = newDskTop;
    for (std::uint32_t i = 1; i < level; ++i) {
        writePacked(slot, readPacked(slot + 5U, 5), 5);
        slot += 5U;
    }
    writePacked(slot, objectAddress, 5);
}

void G48sxMachine::syncSavedRplRegisters()
{
    const std::uint32_t dskTop = readPacked(SX_DSKTOP, 5);
    const std::uint32_t rskTop = readPacked(SX_RSKTOP, 5);
    const std::uint32_t avMem = readPacked(SX_AVMEM, 5);
    if (dskTop < 5U || avMem == 0) {
        throw std::runtime_error("cannot sync saved HP48SX RPL registers from invalid control words");
    }

    SaturnState state = _cpu.getState();
    constexpr std::uint64_t MASK_5_NIBBLES = 0xfffffULL;
    state.b = (state.b & ~MASK_5_NIBBLES) | rskTop;
    state.d = (state.d & ~MASK_5_NIBBLES) | (avMem - 1U);
    state.d1 = dskTop - 5U;
    _cpu.setState(state);
}

G48sxMachine::DisplayRamLayout G48sxMachine::displayRamLayout()
{
    const std::uint8_t displayControl = readHdw(0x00);
    DisplayRamLayout layout;
    layout.enabled = (displayControl & 0x08U) != 0;
    layout.bitOffset = displayControl & 0x07U;
    layout.displayStart = readPackedHdw(0x20, 5) & 0xffffeU;
    const std::int32_t lineOffset = signExtend12(readPackedHdw(0x25, 3));
    const std::uint32_t lineCounter = (readHdw(0x28) & 0x0fU) | ((readHdw(0x29) & 0x03U) << 4U);
    layout.mainLines = lineCounter == 0 ? LCD_HEIGHT : std::min<std::uint32_t>(lineCounter + 1U, LCD_HEIGHT);
    const std::int32_t rowWidth = static_cast<std::int32_t>((34 + lineOffset + (layout.bitOffset / 4U) * 2U) & 0xfffffffeU);
    if (rowWidth <= 0) {
        throw std::runtime_error("HP48SX LCD row width is not positive");
    }
    layout.rowWidth = static_cast<std::uint32_t>(rowWidth);
    layout.menuStart = readPackedHdw(0x30, 5) & 0xffffeU;
    return layout;
}

std::vector<std::uint8_t> G48sxMachine::readDisplayRam(const DisplayRamLayout& layout)
{
    if (!layout.enabled) {
        throw std::runtime_error("HP48SX LCD is disabled");
    }
    const std::uint32_t mainNibbles = layout.mainLines * layout.rowWidth;
    const std::uint32_t menuNibbles = (LCD_HEIGHT - layout.mainLines) * MENU_NIBBLES_PER_ROW;
    std::vector<std::uint8_t> nibbles;
    nibbles.reserve(mainNibbles + menuNibbles);
    for (std::uint32_t i = 0; i < mainNibbles; ++i) {
        nibbles.push_back(_bus.readNibble(layout.displayStart + i) & 0x0fU);
    }
    for (std::uint32_t i = 0; i < menuNibbles; ++i) {
        nibbles.push_back(_bus.readNibble(layout.menuStart + i) & 0x0fU);
    }
    return nibbles;
}

void G48sxMachine::writeDisplayRam(const DisplayRamLayout& layout, std::span<const std::uint8_t> nibbles)
{
    if (!layout.enabled) {
        throw std::runtime_error("HP48SX LCD is disabled");
    }
    const std::uint32_t mainNibbles = layout.mainLines * layout.rowWidth;
    const std::uint32_t menuNibbles = (LCD_HEIGHT - layout.mainLines) * MENU_NIBBLES_PER_ROW;
    if (nibbles.size() != mainNibbles + menuNibbles) {
        throw std::runtime_error("HP48SX display RAM payload has the wrong size");
    }
    writeNibbles(layout.displayStart, nibbles.subspan(0, mainNibbles));
    writeNibbles(layout.menuStart, nibbles.subspan(mainNibbles, menuNibbles));
}

bool G48sxMachine::compatibleDisplayRamLayouts(const DisplayRamLayout& lhs, const DisplayRamLayout& rhs)
{
    return lhs.enabled == rhs.enabled
        && lhs.bitOffset == rhs.bitOffset
        && lhs.displayStart == rhs.displayStart
        && lhs.rowWidth == rhs.rowWidth
        && lhs.mainLines == rhs.mainLines
        && lhs.menuStart == rhs.menuStart;
}

std::int32_t G48sxMachine::signExtend12(std::uint32_t value)
{
    value &= 0x0fffU;
    return (value & 0x0800U) != 0 ? static_cast<std::int32_t>(value) - 0x1000 : static_cast<std::int32_t>(value);
}

void G48sxMachine::setFramePixel(std::uint32_t x, std::uint32_t y, bool lit)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return;
    }
    const std::size_t index = y * 17 + (x >> 3);
    const std::uint8_t mask = static_cast<std::uint8_t>(0x80U >> (x & 7));
    if (lit) {
        _frameBuffer[index] |= mask;
    } else {
        _frameBuffer[index] &= static_cast<std::uint8_t>(~mask);
    }
}

bool G48sxMachine::readDisplayPixel(std::uint32_t address, std::uint32_t x)
{
    const std::uint8_t nibble = _bus.readNibble(address + (x >> 2)) & 0x0fU;
    return (nibble & (1U << (x & 3))) != 0;
}

void G48sxMachine::updateFrameBufferFromLcdMemory()
{
    _frameBuffer.fill(0);

    const std::uint8_t displayControl = readHdw(0x00);
    if ((displayControl & 0x08U) == 0) {
        return;
    }

    const std::uint32_t bitOffset = displayControl & 0x07U;
    const std::uint32_t displayStart = readPackedHdw(0x20, 5) & 0xffffeU;
    const std::int32_t lineOffset = signExtend12(readPackedHdw(0x25, 3));
    const std::uint32_t lineCounter = (readHdw(0x28) & 0x0fU) | ((readHdw(0x29) & 0x03U) << 4);
    const std::uint32_t mainLines = lineCounter == 0 ? LCD_HEIGHT : std::min<std::uint32_t>(lineCounter + 1, LCD_HEIGHT);
    const std::int32_t rowWidth = static_cast<std::int32_t>((34 + lineOffset + (bitOffset / 4U) * 2U) & 0xfffffffeU);
    const std::uint32_t menuStart = readPackedHdw(0x30, 5) & 0xffffeU;

    if (rowWidth > 0) {
        for (std::uint32_t y = 0; y < mainLines; ++y) {
            const std::uint32_t rowAddress = (displayStart + y * static_cast<std::uint32_t>(rowWidth)) & 0xfffffU;
            for (std::uint32_t x = 0; x < LCD_WIDTH; ++x) {
                const std::uint32_t sourceX = x + bitOffset;
                const bool lit = sourceX < MAIN_OFFSCREEN_WIDTH && readDisplayPixel(rowAddress, sourceX);
                setFramePixel(x, y, lit);
            }
        }
    }

    for (std::uint32_t y = mainLines; y < LCD_HEIGHT; ++y) {
        const std::uint32_t rowAddress = (menuStart + (y - mainLines) * MENU_NIBBLES_PER_ROW) & 0xfffffU;
        for (std::uint32_t x = 0; x < LCD_WIDTH; ++x) {
            setFramePixel(x, y, readDisplayPixel(rowAddress, x));
        }
    }
}

void G48sxMachine::updateDisplayAnnunciators()
{
    const std::uint8_t annLow = readHdw(0x0b);
    const std::uint8_t annHigh = readHdw(0x0c);
    std::uint8_t annunciators = static_cast<std::uint8_t>(annLow | (annHigh << 4U));
    const std::uint8_t timer2Control = readHdw(0x2f);
    if ((annunciators & ANN_ON) == 0 || (timer2Control & TIMER2_RUN) == 0) {
        annunciators = 0;
    }

    _display.setAnnunciators({
        .leftShift = (annunciators & ANN_SHIFT) != 0,
        .rightShift = (annunciators & ANN_ALT_SHIFT) != 0,
        .alpha = (annunciators & ANN_ALPHA) != 0,
        .alarm = (annunciators & ANN_ALARM) != 0,
        .busy = (annunciators & ANN_BUSY) != 0,
        .transfer = (annunciators & ANN_TRANSFER) != 0,
    });
}

} // namespace hp48
