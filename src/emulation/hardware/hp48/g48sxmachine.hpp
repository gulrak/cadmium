//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/g48sxmachine.hpp
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

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <ghc/filesystem.hpp>
#include <ghc/span2d.hpp>

#include <emulation/ringbuffer.hpp>
#include <emulation/filter.hpp>

#include "display.hpp"
#include "emptyportcontroller.hpp"
#include "hdwcontroller.hpp"
#include "hpsaturnl2.hpp"
#include "nce1romcontroller.hpp"
#include "nce2ramcontroller.hpp"
#include "stack.hpp"
#include "systembus.hpp"

namespace hp48 {

class Binary;

inline constexpr std::uint64_t G48SX_EMULATED_FRAME_HZ = 64;
static_assert(HdwController::TIMER2_HZ % G48SX_EMULATED_FRAME_HZ == 0);
inline constexpr std::uint64_t G48SX_TIMER2_TICKS_PER_EMULATED_FRAME =
    HdwController::TIMER2_HZ / G48SX_EMULATED_FRAME_HZ;
inline constexpr std::uint64_t G48SX_CYCLES_PER_EMULATED_FRAME =
    HdwController::TIMER2_CYCLES_PER_TICK * G48SX_TIMER2_TICKS_PER_EMULATED_FRAME;
inline constexpr std::uint32_t G48SX_REAL_TIMER2_CYCLES_PER_TICK = 199;
inline constexpr std::uint32_t G48SX_STATE_FILE_VERSION = 1;
inline constexpr std::uint32_t G48SX_ROM_NIBBLES = 0x80000;
inline constexpr std::string_view G48SX_STATE_SCHEMA = "https://schemas.local.hp48hle/g48-state.schema.json";
inline constexpr std::uint64_t G48SX_COLD_START_PROMPT_INSTRUCTIONS = 140000;
inline constexpr std::uint64_t G48SX_COLD_START_KEY_HOLD_INSTRUCTIONS = 100000;
inline constexpr std::uint64_t G48SX_RECOVERY_STACK_INSTRUCTIONS = 5000000;
inline constexpr std::uint64_t G48SX_RECOVERY_RESET_KEY_HOLD_INSTRUCTIONS = 47000;
inline constexpr std::uint64_t G48SX_RECOVERY_KEY_INSTRUCTION = 52000;
inline constexpr std::uint64_t G48SX_RECOVERY_KEY_HOLD_INSTRUCTIONS = 5000;

struct G48sxScheduledKeyPress {
    std::string label;
    std::uint64_t instruction = 0;
    std::uint64_t holdInstructions = 200;
    bool pressed = false;
    bool released = false;
};

enum class G48sxTimingProfile {
    COMPARE,
    REAL,
};

struct G48sxStackObject {
    std::vector<std::uint8_t> nibbles;

    static G48sxStackObject hpObject(const Binary& binary);
    static G48sxStackObject stringObject(std::span<const std::uint8_t> bytes);
};

bool isG48sxKeyLabel(std::string_view label);

class G48sxMachine {
public:
    static constexpr int LCD_RENDER_SCALE = 4;
    static constexpr std::uint32_t LCD_RENDER_WIDTH = (2 + 131 + 1) * LCD_RENDER_SCALE;
    static constexpr std::uint32_t LCD_RENDER_HEIGHT = (8 + 64 + 1) * LCD_RENDER_SCALE;

    explicit G48sxMachine(std::uint64_t resetKeyHoldInstructions,
                          std::vector<std::uint8_t> romBytes,
                          std::string romName = {},
                          G48sxTimingProfile timingProfile = G48sxTimingProfile::COMPARE);

    void resetColdStart();
    void scheduleKeyPress(G48sxScheduledKeyPress keyPress);
    void scheduleKeyPress(std::string label, std::uint64_t instruction, std::uint64_t holdInstructions);
    void stepTo(std::uint64_t instructionTarget);
    bool stepInstruction();
    void stepFrame();
    void stepCycles(std::uint64_t cycleBudget);
    void prepareRecoveredStackForEval(std::span<const G48sxStackObject> objects);
    void prepareRecoveredStackForEval(std::span<const Binary> binaries);
    void render(ghc::span2d<std::uint32_t> output, int scale = LCD_RENDER_SCALE);
    void renderAudio(std::int16_t* sampleBuffer, std::size_t frames, int sampleFrequency);
    void writePbm(const std::string& path);
    [[nodiscard]] std::vector<std::uint8_t> saveStateBytes(bool compressed = false);
    void saveState(const ghc::filesystem::path& path, bool compressed = false);
    void loadState(const ghc::filesystem::path& path);
    void loadState(std::span<const std::uint8_t> stateBytes);
    void press(std::string_view label);
    void release(std::string_view label);

    [[nodiscard]] SaturnL2& cpu() { return _cpu; }
    [[nodiscard]] const SaturnL2& cpu() const { return _cpu; }
    [[nodiscard]] std::uint8_t readNibble(std::uint32_t address) const;
    [[nodiscard]] std::uint8_t readByte(std::uint32_t byteAddress) const;
    void writeNibble(std::uint32_t address, std::uint8_t value);
    void writeByte(std::uint32_t byteAddress, std::uint8_t value);
    [[nodiscard]] std::vector<std::uint32_t> prepareRecoveredStackForEvalWithAddresses(
        std::span<const G48sxStackObject> objects);
    [[nodiscard]] std::vector<std::uint32_t> pushStackObjectsForEval(
        std::span<const G48sxStackObject> objects);
    [[nodiscard]] std::uint32_t insertStackObjectForEval(
        const G48sxStackObject& object,
        std::uint32_t level = 1);
    [[nodiscard]] const std::string& errorMessage() const { return _errorMessage; }

    void setTimingProfile(G48sxTimingProfile timingProfile);
    void setPaused(bool paused) { _paused = paused; }
    // The 5 bit display contrast as set in the hardware registers (HDW 0x101 and bit 0 of 0x102)
    [[nodiscard]] int registerContrast() const;
    // A fixed contrast replaces the register value for rendering, the calculator can't change it then
    void setFixedContrast(std::optional<int> contrast) { _fixedContrast = contrast; }
    [[nodiscard]] std::optional<int> fixedContrast() const { return _fixedContrast; }
    [[nodiscard]] bool paused() const { return _paused; }
    [[nodiscard]] bool running() const { return _running; }
    [[nodiscard]] std::uint64_t instructionIndex() const { return _instructionIndex; }
    [[nodiscard]] std::uint64_t cycles() const { return static_cast<std::uint64_t>(_cpu.cycles()); }
    [[nodiscard]] std::uint32_t timer2CyclesPerTick() const { return _hdw.timer2CyclesPerTick(); }
    [[nodiscard]] std::uint64_t cyclesPerEmulatedFrame() const;
    [[nodiscard]] std::uint64_t cyclesPerSecond() const;
    [[nodiscard]] std::uint64_t frames() const { return cycles() / cyclesPerEmulatedFrame(); }

private:
    static constexpr std::uint32_t HDW_BASE = 0x00100;
    static constexpr std::uint32_t LCD_WIDTH = 131;
    static constexpr std::uint32_t LCD_HEIGHT = 64;
    static constexpr std::uint32_t MAIN_NIBBLES_PER_ROW = 36;
    static constexpr std::uint32_t MENU_NIBBLES_PER_ROW = 34;
    static constexpr std::uint32_t MAIN_OFFSCREEN_WIDTH = MAIN_NIBBLES_PER_ROW * 4;
    static constexpr std::uint8_t ANN_SHIFT = 0x01;
    static constexpr std::uint8_t ANN_ALT_SHIFT = 0x02;
    static constexpr std::uint8_t ANN_ALPHA = 0x04;
    static constexpr std::uint8_t ANN_ALARM = 0x08;
    static constexpr std::uint8_t ANN_BUSY = 0x10;
    static constexpr std::uint8_t ANN_TRANSFER = 0x20;
    static constexpr std::uint8_t ANN_ON = 0x80;
    static constexpr std::uint8_t TIMER2_RUN = 0x01;

    struct DisplayRamLayout {
        bool enabled = false;
        std::uint32_t bitOffset = 0;
        std::uint32_t displayStart = 0;
        std::uint32_t rowWidth = 0;
        std::uint32_t mainLines = 0;
        std::uint32_t menuStart = 0;
    };

    std::string currentRomSha1();
    void stepOne();
    [[nodiscard]] std::uint8_t readHdw(std::uint32_t offset);
    [[nodiscard]] std::uint32_t readPacked(std::uint32_t address, int count);
    [[nodiscard]] std::uint32_t readPackedHdw(std::uint32_t offset, int count);
    void writePacked(std::uint32_t address, std::uint32_t value, int count);
    void writeNibbles(std::uint32_t address, std::span<const std::uint8_t> nibbles);
    void moveNibbles(std::uint32_t source, std::uint32_t destination, std::uint32_t count);
    [[nodiscard]] std::uint32_t stackDepth();
    [[nodiscard]] std::uint32_t createTempObject(std::span<const std::uint8_t> object);
    void pushStackObject(std::uint32_t objectAddress, std::uint32_t level = 1);
    void syncSavedRplRegisters();
    [[nodiscard]] DisplayRamLayout displayRamLayout();
    [[nodiscard]] std::vector<std::uint8_t> readDisplayRam(const DisplayRamLayout& layout);
    void writeDisplayRam(const DisplayRamLayout& layout, std::span<const std::uint8_t> nibbles);
    static bool compatibleDisplayRamLayouts(const DisplayRamLayout& lhs, const DisplayRamLayout& rhs);
    static std::int32_t signExtend12(std::uint32_t value);
    void setFramePixel(std::uint32_t x, std::uint32_t y, bool lit);
    [[nodiscard]] bool readDisplayPixel(std::uint32_t address, std::uint32_t x);
    void updateFrameBufferFromLcdMemory();
    void updateDisplayAnnunciators();
    void buzzerChanged(bool state);
    void resetBuzzerState();
    void updateBuzzerSamples(bool state, std::uint64_t currentCycle);
    std::int16_t filteredBuzzerSample(bool state);
    void configureBuzzerFilters(int sampleFrequency);

    HdwController _hdw;
    Nce2RamController _ram;
    EmptyPortController _port1;
    EmptyPortController _port2;
    EmptyPortController _nce3;
    std::shared_ptr<Nce1RomController> _romCtrl;
    SystemBus _bus;
    SaturnL2 _cpu;

    std::array<std::uint8_t, 64 * 17> _frameBuffer {};
    ObjectStack _dummyStack;
    std::vector<std::string> _dummyMenu;
    Hp48Display _display;
    std::vector<G48sxScheduledKeyPress> _scheduledKeyPresses;

    std::uint64_t _instructionIndex = 0;
    G48sxTimingProfile _timingProfile = G48sxTimingProfile::COMPARE;
    std::uint64_t _resetKeyHoldInstructions = G48SX_COLD_START_KEY_HOLD_INSTRUCTIONS;
    bool _running = false;
    bool _paused = false;
    std::optional<int> _fixedContrast;
    bool _resetKeysHeld = false;
    emu::RingBuffer<std::int16_t> _audioSamples;
    std::mutex _audioMutex;
    std::atomic<std::uint64_t> _audioCurrentCycle {0};
    std::uint64_t _buzzerCycle = 0;
    std::uint64_t _buzzerSampleRemainder = 0;
    int _audioSampleFrequency = 0;
    bool _buzzerState = false;
    std::string _errorMessage;
    std::optional<LowPassButterworthFilter> _buzzerLowPassFilter;
    std::optional<HighPassButterworthFilter> _buzzerHighPassFilter;
};

} // namespace hp48
