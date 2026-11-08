//---------------------------------------------------------------------------------------
// src/emulation/schipstrict.hpp
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
// Cycle exact high level emulation of the SCHIP 1.1 interpreter on an HP-48SX, following
// the timing documented in docs/HP48-SCHIP-1.1-Timing.md. Like Chip8StrictEmulator for the
// VIP, this is a plain switch/case interpreter for exactly one variant.
#pragma once

#include <emulation/chip8genericbase.hpp>
#include <emulation/emulatorhost.hpp>
#include <emulation/filter.hpp>
#include <emulation/hardware/hp48/display.hpp>
#include <emulation/hardware/hp48/stack.hpp>
#include <emulation/ringbuffer.hpp>
#include <emulation/videoscreen.hpp>

#include <array>
#include <mutex>
#include <string>
#include <vector>

namespace emu {

struct SchipStrictOptions
{
    Properties asProperties() const;
    static SchipStrictOptions fromProperties(const Properties& props);
    static Properties& registeredPrototype();
    bool cleanRam{};
    bool traceLog{};
    bool hp48Display{};
    Palette palette;
};

class SchipStrictEmulator : public Chip8GenericBase
{
public:
    static constexpr uint64_t CPU_CLOCK_FREQUENCY = 1630208;    // 199 cycles per 8192 Hz TIMER2 count
    static constexpr int64_t TICK_CYCLES = 25472;               // 64 Hz, 128 TIMER2 counts
    static constexpr int SCREEN_WIDTH = 128;
    static constexpr int SCREEN_HEIGHT = 64;
    static constexpr uint32_t MEMORY_SIZE = 4096;
    static constexpr int STACK_LEVELS = 16;
    static constexpr int STARTUP_PENDING_TICKS = 16;            // arbitrary middle value, the HP-48SX core lands on 9 to 31
    static constexpr int64_t STARTUP_TICK_PHASE = TICK_CYCLES / 2;
    static constexpr int HP48_DISPLAY_WIDTH = 540;              // the LCD rendered by hp48::Hp48Display at scale 4
    static constexpr int HP48_DISPLAY_HEIGHT = 296;
    static constexpr int AUDIO_SAMPLE_FREQUENCY = 44100;
    static constexpr uint32_t BUZZER_LOW_PASS_CUTOFF_HZ = 10000;  // same filters as the HP-48SX core
    static constexpr uint32_t BUZZER_HIGH_PASS_CUTOFF_HZ = 80;
    static constexpr size_t BUZZER_BUFFER_SAMPLES = 4096;
    static constexpr size_t BUZZER_MAX_BACKLOG = 1024;            // more than the longest instruction produces past a frame end

    SchipStrictEmulator(EmulatorHost& host, Properties& properties, IEmulationCore* other = nullptr);
    ~SchipStrictEmulator() override = default;

    std::string name() const override { return "SCHIP-Strict"; }
    chip8::VariantSet supportedChip8Variants() const override;
    uint32_t cpuID() const override { return 0x5C11; }
    bool updateProperties(Properties& props, Property& changed) override;

    double frameRate() const override { return 64; }
    uint32_t defaultLoadAddress() const override { return 0x200; }
    int64_t machineCycles() const override { return _machineCycles; }
    uint8_t readMemoryByte(uint32_t addr) const override { return _memory[addr & (MEMORY_SIZE - 1)]; }

    uint16_t getCurrentScreenWidth() const override { return SCREEN_WIDTH; }
    uint16_t getCurrentScreenHeight() const override { return SCREEN_HEIGHT; }
    uint16_t getMaxScreenWidth() const override { return SCREEN_WIDTH; }
    uint16_t getMaxScreenHeight() const override { return SCREEN_HEIGHT; }
    const VideoType* getScreen() const override { return &_screen; }
    ghc::span2d<const uint32_t> getRGBASpan(ghc::span2d<uint32_t> target) const override;
    void setPalette(const Palette& palette) override;

    unsigned stackSize() const override { return STACK_LEVELS; }
    StackContent stack() const override;
    bool restoreState(const Chip8State& state) override;

    void executeFrame() override;
    int executeInstruction() override;
    void executeInstructions(int numInstructions) override;
    void renderAudio(int16_t* samples, size_t frames, int sampleFrequency) override;

    // Cycle of the most recent dispatch (after the real-time chores of that instruction).
    int64_t lastDispatchCycle() const { return _lastDispatch; }
    // Places the 64 Hz tick schedule: the next tick is due at nextTickCycle, ticks that are
    // already due are handled one per poll. Used to compare against the HP-48SX core.
    void setTickSchedule(int64_t nextTickCycle) { _nextTick = nextTickCycle; }

protected:
    void handleReset(bool cold) override;

private:
    enum class WaitState { NONE, LORES_DRAW, KEY_SCAN, KEY_HELD };

    uint16_t readWord(uint32_t addr) const { return static_cast<uint16_t>((readMemoryByte(addr) << 8) | readMemoryByte(addr + 1)); }
    uint8_t readByte(uint32_t addr) const { return _memory[addr & (MEMORY_SIZE - 1)]; }
    void writeByte(uint32_t addr, uint8_t value) { _memory[addr & (MEMORY_SIZE - 1)] = value; }
    void addCycles(int64_t cycles);
    bool realtime();
    void execute(uint16_t opcode);
    void continueWait();
    void skipIf(bool condition, int64_t cyclesNoSkip, int64_t cyclesSkip);
    void errorHalt(std::string message);
    void startWait(WaitState state, uint16_t opcode);
    void endWait();
    bool keyDown(uint8_t key) const { return key < 16 && _host.isKeyDown(key); }
    void drawLores(uint16_t opcode);
    int64_t drawHires(uint16_t opcode);
    void scrollDown(int rows);
    void scrollLeft();
    void scrollRight();
    void clearScreen();
    void setLcdPixel(int x, int y, bool on);
    void refreshScreen();
    void handleFrame();
    void setBuzzer(bool high, int64_t cycle);
    void advanceBuzzer(int64_t cycle);
    int16_t nextBuzzerSample();
    void configureBuzzer(int sampleFrequency);
    void resetBuzzerOutput();

    EmulatorHost& _host;
    SchipStrictOptions _options;
    int64_t _machineCycles{};
    int64_t _nextTick{};
    int64_t _nextFrame{};
    int64_t _lastDispatch{};
    WaitState _waitState{WaitState::NONE};
    uint16_t _waitOpcode{};
    uint8_t _waitKey{};
    bool _hires{};
    std::array<uint16_t, STACK_LEVELS> _stack{};
    mutable std::array<uint8_t, STACK_LEVELS * 2> _stackView{};  // big endian copy of _stack for stack()
    std::array<uint8_t, 8> _rplFlags{};
    std::array<uint8_t, 16> _registers{};
    std::array<uint8_t, SCREEN_WIDTH * SCREEN_HEIGHT> _lcd{};
    uint32_t _randomState{0x2B7E1516};
    bool _speakerDataHigh{};  // what the next speaker write outputs, the interpreter alternates #400 and #800
    bool _buzzerHigh{};       // bit 11 of the OUT register
    int64_t _buzzerCycle{};   // the samples up to this cycle are generated
    int64_t _buzzerSampleRemainder{};
    int _audioSampleFrequency{AUDIO_SAMPLE_FREQUENCY};
    std::mutex _audioMutex;
    RingBuffer<int16_t, BUZZER_BUFFER_SAMPLES> _buzzerSamples;
    LowPassButterworthFilter _buzzerLowPass{AUDIO_SAMPLE_FREQUENCY, BUZZER_LOW_PASS_CUTOFF_HZ};
    HighPassButterworthFilter _buzzerHighPass{AUDIO_SAMPLE_FREQUENCY, BUZZER_HIGH_PASS_CUTOFF_HZ};
    VideoType _screen{SCREEN_WIDTH, SCREEN_HEIGHT};
    mutable std::array<uint8_t, 17 * SCREEN_HEIGHT> _hp48FrameBuffer{};  // 131 LCD pixels per row, 17 bytes stride
    hp48::ObjectStack _hp48Stack;
    std::vector<std::string> _hp48Menu;
    mutable hp48::Hp48Display _hp48Display{{_hp48FrameBuffer.data(), _hp48FrameBuffer.size()}, _hp48Stack, _hp48Menu};
};

}  // namespace emu
