//---------------------------------------------------------------------------------------
// src/emulation/chip8rsm8k1detail.hpp
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
#pragma once



#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace emu::rsm8k1detail {
struct HostInput {
    std::uint32_t buttons = 0, modifiers = 0, chipKeys = 0, keycode = 0, keycodeModifiers = 0;
};
class Platform {
public:
    virtual ~Platform() = default;
    virtual std::uint32_t millis() = 0;
    virtual HostInput pollInput() = 0;
    virtual std::uint32_t pollKeycode() = 0;
    virtual void setDisplaySize(int, int) = 0;
    virtual void drawPixel(int, int, std::uint32_t) = 0;
    virtual void present() = 0;
    virtual void stopChip8Beep() {}
    virtual void playMegaChipSound(std::uint16_t, std::uint32_t, std::span<const std::uint8_t>) = 0;
    virtual void stopMegaChipSound() = 0;
    virtual void debugLog(const char*, ...) {}
};


struct MegaChipSoundState {
    std::uint32_t sampleLength = 0;
    std::uint32_t remaining = 0;
    std::uint32_t frequency = 0;
    std::uint32_t step = 0x100;
    std::uint32_t cursor = 0;
    std::uint32_t dataOffset = 0;
};

struct State {
    std::array<std::uint8_t, 16> vreg{};
    std::uint32_t iregister = 0;
    std::uint32_t programCounter = 0;
    std::array<std::uint8_t, 32> stack{};
    std::uint32_t currentStack = 0;

    std::span<std::uint8_t> memory{};
    std::array<std::uint8_t, 0x1000> rom{};
    std::uint32_t romSize = 0;

    std::array<std::uint8_t, 0x400> screen{};
    std::array<std::uint8_t, 0x400> tempScreen{};
    std::array<std::uint8_t, 0xc000> megaChipBuffer{};
    std::array<std::uint32_t, 256> megaChipPalette{};

    std::array<std::uint8_t, 16> keys{};
    std::uint32_t keyPressed = 0;
    std::uint32_t lastKey = 0;
    std::uint32_t keyMapOffset = 0;

    std::uint32_t delayTimer = 0;
    std::uint32_t soundTimer = 0;
    std::uint32_t currentTick = 0;
    std::int32_t ticksPerFrame = 0x32;
    std::uint32_t chipTime = 0;

    std::uint32_t screenWidth = 0x40;
    std::uint32_t screenHeight = 0x20;
    std::uint32_t charset = 0;
    std::uint32_t exit = 0;
    std::uint8_t randomCount = 0;
    std::uint32_t extendedMode = 0;
    std::uint32_t megaChipEnabled = 0;
    std::uint32_t emulationMode = 0;
    std::uint32_t debugMode = 0;
    std::int32_t colorScheme = 0;
    std::uint32_t noKey = 0;

    std::uint32_t megaChipSpriteWidth = 0;
    std::uint32_t megaChipSpriteHeight = 0;
    std::uint32_t megaChipAlpha = 0xff;
    std::uint32_t megaChipBlendMode = 0;
    std::uint32_t megaChipCollisionColor = 0xff;
    MegaChipSoundState megaChipSound{};

    std::array<std::uint32_t, 512> offsets{};
    std::uint32_t ramPos = 0;
    std::uint32_t iregAddr = 0;

    std::uint32_t foregroundColor = 0xa0c0f0;
    std::uint32_t backgroundColor = 0x2030;
    std::uint32_t currentFrame = 0;
    std::uint32_t drawnPixels = 0;
    std::uint32_t collisionPixels = 0;
};

class Interpreter {
public:
    explicit Interpreter(Platform& platform);

    [[nodiscard]] State& state() { return _state; }
    [[nodiscard]] const State& state() const { return _state; }

    void resetCpu();
    void loadRom(std::span<const std::uint8_t> bytes);
    std::uint16_t calcChecksum() const;
    void setKeyMapping(std::uint16_t checksum);
    void keyPress(std::uint8_t key);
    void getKeys();
    void runCpu(float frameStep);
    void updateTimers();

    void clearScreen();
    void drawScreen();
    std::uint32_t updateScreen();
    void scrollDown(std::uint32_t rows);
    void scrollLeft();
    void scrollRight();
    void drawSprite(std::uint8_t x, std::uint8_t y, std::uint8_t height);
    void drawSprite8(std::uint8_t x, std::uint8_t y, std::uint8_t height);

    void initColorScheme(std::int32_t scheme);

    // Runs the same outer timing loop as coremain for one refresh interval.
    void runRefresh(float frameStep);

private:
    Platform& _platform;
    State _state{};
    bool _waitingForKey = false;
    bool _waitingForRelease = false;
    std::uint8_t _waitRegister = 0;
public:
    bool waiting() const { return _waitingForKey || _waitingForRelease; }

    [[nodiscard]] std::uint8_t readByte(std::uint32_t address) const;
    void writeByte(std::uint32_t address, std::uint8_t value);
    [[nodiscard]] static std::uint16_t opcodeNnn(std::uint16_t opcode);
    [[nodiscard]] static std::uint8_t shiftedHigh(std::uint8_t value, std::uint8_t bitOffset);
    [[nodiscard]] static std::uint8_t shiftedCarry(std::uint8_t value, std::uint8_t bitOffset);
};

extern const std::array<std::uint8_t, 256> CHARACTERS;
extern const std::array<std::uint8_t, 256> RANDOM_TABLE;
extern const std::array<std::uint8_t, 624> KEY_REMAP;
extern const std::array<std::uint16_t, 38> GAME_CRC;

} // namespace emu::rsm8k1detail
