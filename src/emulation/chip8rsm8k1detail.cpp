//---------------------------------------------------------------------------------------
// src/emulation/chip8rsm8k1detail.cpp
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
#include "chip8rsm8k1detail.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace emu::rsm8k1detail {
namespace {

constexpr std::int32_t CHIP8_TICKS_PER_FRAME = 50;
constexpr std::int32_t SUPERCHIP_TICKS_PER_FRAME = 100;
constexpr std::int32_t MEGACHIP_TICKS_PER_FRAME = 10000;

} // namespace

Interpreter::Interpreter(Platform& platform)
    : _platform(platform)
{
    resetCpu();
}

void Interpreter::resetCpu()
{
    _waitingForKey = _waitingForRelease = false;
    _state.programCounter = 0;
    _state.currentStack = 0;
    _state.iregister = 0;
    _state.soundTimer = 0;
    _state.delayTimer = 0;
    _state.stack.fill(0);
    _state.screen.fill(0);
    _state.vreg.fill(0);
    _state.keys.fill(0);
    _state.keyPressed = 0;
    _state.lastKey = 0;
    _state.extendedMode = 0;
    _state.screenWidth = 0x40;
    _state.screenHeight = 0x20;
    _state.charset = 0;
    _state.exit = 0;
    _state.randomCount = 0;
    _state.currentTick = 0;
    _state.ticksPerFrame = CHIP8_TICKS_PER_FRAME;
    _state.megaChipBuffer.fill(0);
    _state.megaChipPalette.fill(0);
    _state.megaChipSpriteWidth = 0;
    _state.megaChipSpriteHeight = 0;
    _state.megaChipAlpha = 0xff;
    _state.megaChipBlendMode = 0;
    _state.megaChipCollisionColor = 0xff;
    _state.megaChipEnabled = 0;
}

void Interpreter::loadRom(std::span<const std::uint8_t> bytes)
{
    if (bytes.size() > _state.memory.size()) {
        throw std::runtime_error("ROM is larger than the emulated memory buffer");
    }

    // The original overwrites only the loaded bytes; previous RAM survives.
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        writeByte(static_cast<std::uint32_t>(i), bytes[i]);
    }
    _state.romSize = static_cast<std::uint32_t>(bytes.size());
    if (_state.romSize < _state.rom.size()) {
        std::copy(bytes.begin(), bytes.end(), _state.rom.begin());
    }
    _state.programCounter = 0;
}

std::uint16_t Interpreter::calcChecksum() const
{
    std::uint16_t checksum = 0;
    for (std::uint32_t i = 0; i < _state.romSize; ++i) {
        checksum = static_cast<std::uint16_t>(checksum + readByte(i));
    }
    return checksum;
}

void Interpreter::setKeyMapping(std::uint16_t checksum)
{
    for (std::size_t i = 0; i < GAME_CRC.size(); ++i) {
        if (GAME_CRC[i] == checksum) {
            _state.keyMapOffset = static_cast<std::uint32_t>(i * 0x10);
            return;
        }
    }
    _state.keyMapOffset = static_cast<std::uint32_t>(GAME_CRC.size() * 16);
}

void Interpreter::keyPress(std::uint8_t key)
{
    if (_state.keyMapOffset + 16 > KEY_REMAP.size()) {
        return;
    }

    for (std::size_t i = 0; i < 0x10; ++i) {
        if (KEY_REMAP[_state.keyMapOffset + i] == key) {
            _state.keys[i] = 1;
            _state.keyPressed = static_cast<std::uint32_t>(key) + 1;
        }
    }
}

void Interpreter::getKeys()
{
    _state.keyPressed = 0;
    _state.keys.fill(0);

    const HostInput input = _platform.pollInput();
    const std::uint32_t buttons = input.buttons;
    const std::uint32_t modifiers = input.modifiers;
    const std::uint32_t chipKeys = input.chipKeys;

    for (std::uint8_t key = 0; key < _state.keys.size(); ++key) {
        if (((chipKeys >> key) & 1U) != 0) {
            _state.keys[key] = 1;
            _state.keyPressed = static_cast<std::uint32_t>(key) + 1;
        }
    }

    if (((buttons >> 2) & 1U) != 0) keyPress(4);
    if (((buttons >> 3) & 1U) != 0) keyPress(6);
    if ((buttons & 1U) != 0) keyPress(2);
    if (((buttons >> 1) & 1U) != 0) keyPress(8);
    if (((buttons >> 4) & 1U) != 0) keyPress(5);
    if (((buttons >> 5) & 1U) != 0) keyPress(0x0a);
    if (((buttons >> 6) & 1U) != 0) keyPress(0x0b);
    if (((buttons >> 7) & 1U) != 0) keyPress(0x0c);
    if (((buttons >> 0x0c) & 1U) != 0) keyPress(0x0f);

    if (buttons == 0) {
        _state.noKey = 1;
    }
    if (_state.noKey == 1) {
        if (((buttons >> 8) & 1U) != 0) {
            --_state.colorScheme;
            if (_state.colorScheme < 0) _state.colorScheme = 5;
            initColorScheme(_state.colorScheme);
            _state.noKey = 0;
        }
        if (((buttons >> 10) & 1U) != 0) {
            ++_state.colorScheme;
            if (_state.colorScheme > 5) _state.colorScheme = 0;
            initColorScheme(_state.colorScheme);
            _state.noKey = 0;
        }
        if (((modifiers >> 0x0d) & 1U) != 0) {
            _state.emulationMode = _state.emulationMode == 0 ? 1 : 0;
        }
        if (((modifiers >> 0x0f) & 1U) != 0) {
            _state.exit = 1;
        }
    }

    const std::uint32_t keycode = input.keycode;
    if (keycode == 0x31) keyPress(1);
    if (keycode == 0x32) keyPress(2);
    if (keycode == 0x33) keyPress(3);
    if (keycode == 0x34) keyPress(4);
    if (keycode == 0x35) keyPress(5);
    if (keycode == 0x36) keyPress(6);
    if (keycode == 0x37) keyPress(7);
    if (keycode == 0x38) keyPress(8);
    if (keycode == 0x39) keyPress(9);
    if (keycode == 0x30) keyPress(0);
    if (keycode == 0x51) keyPress(0x0a);
    if (keycode == 0x57) keyPress(0x0b);
    if (keycode == 0x45) keyPress(0x0c);
    if (keycode == 0x52) keyPress(0x0d);
    if (keycode == 0x54) keyPress(0x0e);
    if (keycode == 0x59) keyPress(0x0f);
    if (keycode == 0x41 && _state.ticksPerFrame > 1) --_state.ticksPerFrame;
    if (keycode == 0x53) ++_state.ticksPerFrame;

    if (input.keycodeModifiers != 0) {
        if (keycode == 0xf03b && ++_state.debugMode > 2) _state.debugMode = 0;
        if (keycode == 0xf03a) _state.emulationMode = _state.emulationMode == 0 ? 1 : 0;
        if (keycode == 0xf030) {
            --_state.colorScheme;
            if (_state.colorScheme < 0) _state.colorScheme = 5;
            initColorScheme(_state.colorScheme);
            _state.noKey = 0;
        }
        if (keycode == 0xf031) {
            ++_state.colorScheme;
            if (_state.colorScheme > 5) _state.colorScheme = 0;
            initColorScheme(_state.colorScheme);
            _state.noKey = 0;
        }
    }
}

void Interpreter::runCpu(float frameStep)
{
    if (_waitingForKey) {
        updateScreen();
        getKeys();
        if (_state.keyPressed == 0 && _state.exit == 0) return;
        _waitingForKey = false;
        _waitingForRelease = true;
    }
    if (_waitingForRelease) {
        if (_platform.pollKeycode() != 0) return;
        _waitingForRelease = false;
        _state.vreg[_waitRegister] = static_cast<std::uint8_t>(_state.keyPressed - 1);
        _state.programCounter += 2;
        ++_state.currentTick;
        updateTimers();
        return;
    }
    const std::uint8_t opcodeHigh = readByte(_state.programCounter);
    const std::uint8_t opcodeLow = readByte(_state.programCounter + 1);
    std::uint16_t opcode = static_cast<std::uint16_t>((opcodeHigh << 8) | opcodeLow);
    const std::uint16_t opcodeClass = static_cast<std::uint16_t>(opcodeHigh >> 4);
    const std::uint8_t x = opcodeHigh & 0x0f;
    const std::uint8_t y = static_cast<std::uint8_t>((opcode >> 4) & 0x0f);
    const std::uint32_t oldPc = _state.programCounter;
    const std::uint32_t oldStack = _state.currentStack;
    bool pcAlreadyChanged = false;

    switch (opcodeClass) {
    case 0:
        if (opcodeHigh == 0) {
            if ((opcodeLow & 0xf0) == 0xb0) {
                break; // Explicitly ignored in the original.
            }
            if ((opcodeLow & 0xf0) == 0xc0) {
                scrollDown(opcodeLow & 0x0f);
            } else if (opcodeLow == 0xfb) {
                scrollRight();
            } else {
                const int halfRomSize = static_cast<int>(_state.romSize) >> 1;
                if (opcodeLow < 0xfc) {
                    if (opcodeLow == 0x11) {
                        _state.megaChipEnabled = 1;
                        _state.screenWidth = 0x100;
                        _state.screenHeight = 0xc0;
                        _state.ticksPerFrame = MEGACHIP_TICKS_PER_FRAME;
                        break;
                    }
                    if (opcodeLow < 0x12) {
                        if (opcodeLow == 0x10) {
                            _state.megaChipEnabled = 0;
                            _state.screenWidth = 0x40;
                            _state.screenHeight = 0x20;
                            _state.ticksPerFrame = CHIP8_TICKS_PER_FRAME;
                            if (halfRomSize < CHIP8_TICKS_PER_FRAME) _state.ticksPerFrame = halfRomSize;
                            break;
                        }
                    } else {
                        if (opcodeLow == 0xe0) {
                            updateScreen();
                            getKeys();
                            if (_state.megaChipEnabled != 0) {
                                _state.currentTick = static_cast<std::uint32_t>(
                                    static_cast<std::int32_t>(static_cast<double>(_state.ticksPerFrame) * frameStep));
                            }
                            clearScreen();
                            break;
                        }
                        if (opcodeLow == 0xee) {
                            --_state.currentStack;
                            const std::uint8_t hi = _state.stack.at(_state.currentStack);
                            _state.currentStack = oldStack - 2;
                            const std::uint8_t lo = _state.stack.at(_state.currentStack);
                            _state.programCounter = static_cast<std::uint16_t>((hi << 8) | lo);
                            break;
                        }
                    }
                } else {
                    if (opcodeLow == 0xfd) {
                        _state.exit = 1;
                        break;
                    }
                    if (opcodeLow < 0xfd) {
                        scrollLeft();
                        break;
                    }
                    if (opcodeLow == 0xfe) {
                        _state.extendedMode = 0;
                        _state.screenWidth = 0x40;
                        _state.screenHeight = 0x20;
                        _state.ticksPerFrame = CHIP8_TICKS_PER_FRAME;
                        if (halfRomSize < CHIP8_TICKS_PER_FRAME) _state.ticksPerFrame = halfRomSize;
                        break;
                    }
                    if (opcodeLow == 0xff) {
                        _state.extendedMode = 1;
                        _state.screenWidth = 0x80;
                        _state.screenHeight = 0x40;
                        _state.ticksPerFrame = SUPERCHIP_TICKS_PER_FRAME;
                        if (halfRomSize < SUPERCHIP_TICKS_PER_FRAME) _state.ticksPerFrame = halfRomSize;
                        break;
                    }
                }
                _platform.debugLog("not supported, code: %04x.\n", opcode);
                resetCpu();
            }
        } else {
            switch (opcodeHigh) {
            case 1:
                _state.programCounter += 2;
                opcode = static_cast<std::uint16_t>((static_cast<std::uint16_t>(readByte(oldPc + 2)) << 8) |
                                                    readByte(oldPc + 3));
                _state.iregister = ((static_cast<std::uint32_t>(opcodeLow) << 16) | opcode) - 0x200;
                break;
            case 2: {
                for (std::uint8_t i = 0; i < opcodeLow; ++i) {
                    const std::uint32_t source = _state.iregister + static_cast<std::uint32_t>(i) * 4;
                    _state.megaChipPalette[i + 1] =
                        (static_cast<std::uint32_t>(readByte(source)) << 24) |
                        (static_cast<std::uint32_t>(readByte(source + 1)) << 16) |
                        (static_cast<std::uint32_t>(readByte(source + 2)) << 8) |
                        static_cast<std::uint32_t>(readByte(source + 3));
                }
                _state.megaChipPalette[0xff] = 0xffffff;
                break;
            }
            case 3:
                _state.megaChipSpriteWidth = opcodeLow == 0 ? 0x100 : opcodeLow;
                break;
            case 4:
                _state.megaChipSpriteHeight = opcodeLow == 0 ? 0x100 : opcodeLow;
                break;
            case 5:
                _state.megaChipAlpha = opcodeLow;
                break;
            case 6: {
                const std::uint16_t frequency =
                    static_cast<std::uint16_t>((static_cast<std::uint16_t>(readByte(_state.iregister)) << 8) |
                                               readByte(_state.iregister + 1));
                const std::uint32_t sampleLength =
                    (static_cast<std::uint32_t>(readByte(_state.iregister + 2)) << 16) |
                    (static_cast<std::uint32_t>(readByte(_state.iregister + 3)) << 8) |
                    static_cast<std::uint32_t>(readByte(_state.iregister + 4));
                _state.megaChipSound.frequency = frequency;
                _state.megaChipSound.sampleLength = sampleLength;
                _state.megaChipSound.remaining = sampleLength;
                _state.megaChipSound.step = (static_cast<std::uint32_t>(frequency) << 8) / 44100;
                _state.megaChipSound.cursor = 0;
                // megachip.exe sets the playback pointer to _chip_memory + I + 6.
                const std::uint64_t dataOffset = static_cast<std::uint64_t>(_state.iregister) + 6;
                if (dataOffset > std::numeric_limits<std::uint32_t>::max()) {
                    throw std::out_of_range("Chip8 memory read out of bounds at " + std::to_string(dataOffset));
                }
                _state.megaChipSound.dataOffset = static_cast<std::uint32_t>(dataOffset);
                if (sampleLength != 0) {
                    const std::uint64_t lastByte = dataOffset + sampleLength - 1;
                    if (lastByte > std::numeric_limits<std::uint32_t>::max() ||
                        lastByte >= _state.memory.size()) {
                        throw std::out_of_range("Chip8 memory read out of bounds at " + std::to_string(lastByte));
                    }
                }
                const auto sampleData =
                    sampleLength == 0
                        ? std::span<const std::uint8_t>{}
                        : std::span<const std::uint8_t>(_state.memory).subspan(_state.megaChipSound.dataOffset,
                                                                                sampleLength);
                _platform.playMegaChipSound(frequency, sampleLength, sampleData);
                break;
            }
            case 7:
                _platform.stopMegaChipSound();
                break;
            case 8:
                _state.megaChipBlendMode = opcodeLow & 0x0f;
                break;
            case 9:
                _state.megaChipCollisionColor = opcodeLow;
                break;
            default:
                break;
            }
        }
        break;
    case 1:
        _state.programCounter = opcodeNnn(opcode) - 0x200;
        pcAlreadyChanged = true;
        break;
    case 2:
        _state.stack.at(_state.currentStack++) = static_cast<std::uint8_t>(_state.programCounter);
        _state.stack.at(_state.currentStack++) = static_cast<std::uint8_t>(_state.programCounter >> 8);
        _state.programCounter = opcodeNnn(opcode) - 0x200;
        pcAlreadyChanged = true;
        break;
    case 3:
        if (_state.vreg[x] == opcodeLow) _state.programCounter += 2;
        break;
    case 4:
        if (_state.vreg[x] != opcodeLow) _state.programCounter += 2;
        break;
    case 5:
        if (_state.vreg[x] == _state.vreg[y]) _state.programCounter += 2;
        break;
    case 6:
        _state.vreg[x] = opcodeLow;
        break;
    case 7:
        _state.vreg[x] = static_cast<std::uint8_t>(_state.vreg[x] + opcodeLow);
        break;
    case 8:
        switch (opcodeLow & 0x0f) {
        case 0:
            _state.vreg[x] = _state.vreg[y];
            break;
        case 1:
            _state.vreg[x] = _state.vreg[y] | _state.vreg[x];
            break;
        case 2:
            _state.vreg[x] = _state.vreg[y] & _state.vreg[x];
            break;
        case 3:
            _state.vreg[x] = _state.vreg[y] ^ _state.vreg[x];
            break;
        case 4:
        {
            const std::uint16_t result = static_cast<std::uint16_t>(_state.vreg[x] + _state.vreg[y]);
            _state.vreg[x] = static_cast<std::uint8_t>(result);
            _state.vreg[0x0f] = static_cast<std::uint8_t>(result >> 8);
            break;
        }
        case 5:
        {
            const std::uint8_t vx = _state.vreg[x];
            const std::uint8_t vy = _state.vreg[y];
            const std::uint8_t flag = vy <= vx ? 1 : 0;
            _state.vreg[x] = static_cast<std::uint8_t>(vx - vy);
            _state.vreg[0x0f] = flag;
            break;
        }
        case 6:
        {
            if (_state.emulationMode == 1) {
                const std::uint8_t vy = _state.vreg[y];
                _state.vreg[x] = static_cast<std::uint8_t>(vy >> 1);
                _state.vreg[0x0f] = vy & 1;
            }
            const std::uint8_t vx = _state.vreg[x];
            _state.vreg[x] = static_cast<std::uint8_t>(vx >> 1);
            _state.vreg[0x0f] = vx & 1;
            break;
        }
        case 7:
        {
            const std::uint8_t vx = _state.vreg[x];
            const std::uint8_t vy = _state.vreg[y];
            const std::uint8_t flag = vx <= vy ? 1 : 0;
            _state.vreg[x] = static_cast<std::uint8_t>(vy - vx);
            _state.vreg[0x0f] = flag;
            break;
        }
        case 0x0e:
        {
            if (_state.emulationMode == 1) {
                const std::uint8_t vy = _state.vreg[y];
                _state.vreg[x] = static_cast<std::uint8_t>(vy * 2);
                _state.vreg[0x0f] = vy >> 7;
            }
            const std::uint8_t vx = _state.vreg[x];
            _state.vreg[x] = static_cast<std::uint8_t>(vx * 2);
            _state.vreg[0x0f] = vx >> 7;
            break;
        }
        default:
            break;
        }
        break;
    case 9:
        if (_state.vreg[x] != _state.vreg[y]) _state.programCounter += 2;
        break;
    case 0x0a: {
        const bool charset = opcodeNnn(opcode) < 0x100;
        _state.iregister = charset ? opcodeNnn(opcode) : opcodeNnn(opcode) - 0x200;
        _state.charset = charset ? 1 : 0;
        _state.iregAddr = _state.programCounter;
        break;
    }
    case 0x0b:
        _state.programCounter = opcodeNnn(opcode) + _state.vreg[0];
        pcAlreadyChanged = true;
        break;
    case 0x0c:
        _state.vreg[x] = opcodeLow & RANDOM_TABLE[_state.randomCount++];
        break;
    case 0x0d:
        if (_state.megaChipEnabled == 0) {
            if (((opcodeLow & 0x0f) != 0) || _state.extendedMode != 0) {
                drawSprite(_state.vreg[x], _state.vreg[y], opcodeLow & 0x0f);
            }
            if (_state.emulationMode == 1) {
                _state.currentTick = static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(static_cast<double>(_state.ticksPerFrame) * frameStep));
            }
        } else {
            drawSprite8(_state.vreg[x], _state.vreg[y], opcodeLow & 0x0f);
        }
        break;
    case 0x0e:
        getKeys();
        // No nibble mask in the executable. Out-of-array guest reads are
        // reported rather than silently redirected to another key.
        if (opcodeLow == 0x9e && _state.keys.at(_state.vreg[x]) != 0) _state.programCounter += 2;
        if (opcodeLow == 0xa1 && _state.keys.at(_state.vreg[x]) == 0) _state.programCounter += 2;
        break;
    case 0x0f:
        if (opcodeLow == 0x29) {
            _state.iregister = (_state.vreg[x] & 0x0f) * 5;
            _state.charset = 1;
        } else if (opcodeLow < 0x2a) {
            if (opcodeLow == 0x15) {
                _state.delayTimer = _state.vreg[x];
            } else if (opcodeLow < 0x16) {
                if (opcodeLow == 0x07) {
                    _state.vreg[x] = static_cast<std::uint8_t>(_state.delayTimer);
                } else if (opcodeLow == 0x0a) {
                    _waitRegister = x;
                    _waitingForKey = true;
                    updateScreen();
                    getKeys();
                    if (_state.keyPressed == 0 && _state.exit == 0) return;
                    _waitingForKey = false;
                    _waitingForRelease = true;
                    if (_platform.pollKeycode() != 0) return;
                    _waitingForRelease = false;
                    _state.vreg[x] = static_cast<std::uint8_t>(_state.keyPressed - 1);
                }
            } else if (opcodeLow == 0x18) {
                _state.soundTimer = _state.vreg[x];
            } else if (opcodeLow == 0x1e) {
                _state.iregister += _state.vreg[x];
            }
        } else if (opcodeLow == 0x55) {
            for (std::uint8_t i = 0; i <= x; ++i) writeByte(_state.iregister + i, _state.vreg[i]);
            if (_state.emulationMode == 1) _state.iregister += x + 1;
        } else if (opcodeLow < 0x56) {
            if (opcodeLow == 0x30) {
                _state.iregister = (_state.vreg[x] & 0x0f) * 10 + 0x50;
                _state.charset = 1;
            } else if (opcodeLow == 0x33) {
                std::uint8_t value = _state.vreg[x];
                std::uint8_t hundreds = 0;
                while (value > 99) {
                    ++hundreds;
                    value = static_cast<std::uint8_t>(value + 0x9c);
                }
                writeByte(_state.iregister, hundreds);
                std::uint8_t tens = 0;
                while (value > 9) {
                    ++tens;
                    value = static_cast<std::uint8_t>(value - 10);
                }
                writeByte(_state.iregister + 1, tens);
                writeByte(_state.iregister + 2, value);
            }
        } else if (opcodeLow == 0x75) {
            for (std::uint8_t i = 0; i <= x; ++i) writeByte(_state.iregister + i, _state.vreg[i]);
        } else if (opcodeLow < 0x76) {
            if (opcodeLow == 0x65) {
                for (std::uint8_t i = 0; i <= x; ++i) _state.vreg[i] = readByte(_state.iregister + i);
                if (_state.emulationMode == 1) _state.iregister += x + 1;
            }
        } else if (opcodeLow == 0x85) {
            for (std::uint8_t i = 0; i <= x; ++i) _state.vreg[i] = readByte(_state.iregister + i);
        }
        break;
    default:
        break;
    }

    if (!pcAlreadyChanged) {
        _state.programCounter += 2;
    }

    ++_state.currentTick;
    updateTimers();
}

void Interpreter::updateTimers()
{
    const auto now = _platform.millis();
    if (static_cast<std::uint32_t>(now - _state.chipTime) > 0x14) {
        if (static_cast<std::int32_t>(_state.delayTimer) < 1) {
            _state.delayTimer = 0;
        } else {
            --_state.delayTimer;
        }

        if (static_cast<std::int32_t>(_state.soundTimer) < 1) {
            _state.soundTimer = 0;
            _platform.stopChip8Beep();
        } else {
            _state.soundTimer = 0;
        }
        _state.chipTime = _platform.millis();
    }
}

void Interpreter::clearScreen()
{
    if (_state.megaChipEnabled == 0) {
        const auto size = (_state.screenWidth * _state.screenHeight) >> 3;
        if (size > _state.screen.size()) throw std::out_of_range("Packed screen extent exceeds original buffer");
        std::fill_n(_state.screen.begin(), size, 0);
    } else {
        const auto size = _state.screenWidth * _state.screenHeight;
        if (size > _state.megaChipBuffer.size()) throw std::out_of_range("MegaChip screen extent exceeds original buffer");
        std::fill_n(_state.megaChipBuffer.begin(), size, 0);
    }
}

void Interpreter::drawScreen()
{
    if (_state.megaChipEnabled == 0) {
        _platform.setDisplaySize(static_cast<int>(_state.screenWidth), static_cast<int>(_state.screenHeight));
        std::uint32_t src = 0;
        for (std::uint32_t row = 0; row < _state.screenHeight; ++row) {
            for (std::uint32_t col = 0; col < _state.screenWidth; col += 8) {
                const std::uint8_t packed = _state.screen.at(src++);
                for (int bit = 0; bit < 8; ++bit) {
                    const bool on = ((0x80U >> bit) & packed) != 0;
                    _platform.drawPixel(static_cast<int>(col + bit),
                                        static_cast<int>(row),
                                        on ? _state.foregroundColor : _state.backgroundColor);
                }
            }
        }
    } else {
        _platform.setDisplaySize(0x100, 0xc0);
        std::uint32_t src = 0;
        for (int row = 0; row < 0xc0; ++row) {
            for (int col = 0; col < 0x100; ++col) {
                _platform.drawPixel(col, row, _state.megaChipPalette[_state.megaChipBuffer[src++]]);
            }
        }
    }
    ++_state.currentFrame;
}

std::uint32_t Interpreter::updateScreen()
{
    drawScreen();
    if (_state.megaChipEnabled != 0) {
        _platform.stopChip8Beep();
    }
    _platform.present();
    return 1;
}

void Interpreter::scrollDown(std::uint32_t rows)
{
    if ((_state.screenWidth * _state.screenHeight) / 8 > _state.screen.size())
        throw std::out_of_range("Scroll extent exceeds original packed screen buffer");
    std::copy(_state.screen.begin(), _state.screen.end(), _state.tempScreen.begin());
    const int stride = static_cast<int>(_state.screenWidth >> 3);
    int dst = 0;
    int src = 0;
    for (int row = 0; row < static_cast<int>(_state.screenHeight); ++row) {
        if (row < static_cast<int>(rows & 0xff)) {
            std::fill_n(_state.screen.begin() + dst, stride, 0);
        } else {
            std::copy_n(_state.tempScreen.begin() + src, stride, _state.screen.begin() + dst);
            src += stride;
        }
        dst += stride;
    }
}

void Interpreter::scrollLeft()
{
    if ((_state.screenWidth * _state.screenHeight) / 8 > _state.screen.size())
        throw std::out_of_range("Scroll extent exceeds original packed screen buffer");
    const int stride = static_cast<int>(_state.screenWidth >> 3);
    int idx = 0;
    for (int row = 0; row < static_cast<int>(_state.screenHeight); ++row) {
        for (int col = 0; col < stride; ++col) {
            if (col == stride - 1) {
                _state.screen[idx] = static_cast<std::uint8_t>(_state.screen[idx] << 4);
            } else {
                _state.screen[idx] = static_cast<std::uint8_t>(((_state.screen[idx + 1] & 0xf0) >> 4) +
                                                              (_state.screen[idx] << 4));
            }
            ++idx;
        }
    }
}

void Interpreter::scrollRight()
{
    if ((_state.screenWidth * _state.screenHeight) / 8 > _state.screen.size())
        throw std::out_of_range("Scroll extent exceeds original packed screen buffer");
    const int stride = static_cast<int>(_state.screenWidth >> 3);
    for (int row = 0; row < static_cast<int>(_state.screenHeight); ++row) {
        int idx = row * stride + stride;
        for (int count = 0; count < stride; ++count) {
            const int dst = idx - 1;
            if (count == stride - 1) {
                _state.screen[dst] = 0;
            } else {
                _state.screen[dst] = static_cast<std::uint8_t>((_state.screen[dst - 1] << 4) +
                                                              ((_state.screen[dst] & 0xf0) >> 4));
            }
            idx = dst;
        }
    }
}

void Interpreter::drawSprite(std::uint8_t x, std::uint8_t y, std::uint8_t height)
{
    std::uint8_t drawHeight = height;
    _state.vreg[0x0f] = 0;
    auto readSpriteByte = [&](std::uint32_t offset) {
        return _state.charset < 1 ? readByte(_state.iregister + offset) : CHARACTERS.at(_state.iregister + offset);
    };

    std::uint8_t wrappedX = 0;
    std::uint8_t wrappedY = 0;
    if (_state.extendedMode == 0) {
        wrappedX = x & 0x3f;
        wrappedY = y & 0x1f;
    } else {
        wrappedX = x & 0x7f;
        wrappedY = y & 0x3f;
    }

    std::uint32_t dst = (wrappedY * _state.screenWidth + wrappedX) >> 3;
    const std::uint8_t bitOffset = wrappedX & 7;

    auto xorByte = [&](std::uint8_t mask, std::uint32_t index) {
        if (mask == 0) return;
        auto& target = _state.screen.at(index);
        if (_state.vreg[0x0f] == 0) {
            _state.vreg[0x0f] = ((mask | target) != (target ^ mask)) ? 1 : 0;
        }
        target ^= mask;
    };

    if (_state.extendedMode == 1 && height == 0) {
        drawHeight = 0x10;
        if (static_cast<std::uint8_t>(wrappedY + 0x10) > 0x3f) {
            drawHeight = static_cast<std::uint8_t>(0x3f - wrappedY);
        }
        for (std::uint8_t row = 0; row < drawHeight; ++row) {
            const std::uint32_t spriteOffset = static_cast<std::uint32_t>(row) * 2;
            const std::uint8_t leftByte = readSpriteByte(spriteOffset);
            xorByte(shiftedHigh(leftByte, bitOffset), dst + 0);
            xorByte(shiftedCarry(leftByte, bitOffset), dst + 1);

            const std::uint8_t rightByte = readSpriteByte(spriteOffset + 1);
            xorByte(shiftedHigh(rightByte, bitOffset), dst + 1);
            xorByte(shiftedCarry(rightByte, bitOffset), dst + 2);

            dst += 0x10;
        }
    } else {
        if (_state.screenHeight < static_cast<std::uint32_t>(wrappedY + height)) {
            drawHeight = static_cast<std::uint8_t>(_state.screenHeight - wrappedY);
        }
        for (std::uint8_t row = 0; row < drawHeight; ++row) {
            const std::uint8_t value = readSpriteByte(row);
            xorByte(shiftedHigh(value, bitOffset), dst + 0);
            xorByte(shiftedCarry(value, bitOffset), dst + 1);
            dst += _state.extendedMode == 0 ? 8 : 0x10;
        }
    }

    _state.charset = 0;
    if (_state.vreg[0x0f] != 0) ++_state.collisionPixels;
    ++_state.drawnPixels;
}

void Interpreter::drawSprite8(std::uint8_t x, std::uint8_t y, std::uint8_t height)
{
    _state.vreg[0x0f] = 0;
    int rowOffset = static_cast<int>(y) << 8;
    if (_state.charset < 1) {
        std::uint32_t source = _state.iregister;
        for (std::uint32_t row = y; static_cast<int>(row) < static_cast<int>(y + _state.megaChipSpriteHeight); ++row) {
            if (static_cast<int>(row) < 0xc0) {
                for (std::uint32_t col = x; static_cast<int>(col) < static_cast<int>(x + _state.megaChipSpriteWidth); ++col) {
                    const std::uint8_t pixel = readByte(source++);
                    if (static_cast<int>(col) < 0x100 && pixel != 0) {
                        if (_state.megaChipBuffer[col + rowOffset] == _state.megaChipCollisionColor &&
                            pixel != _state.megaChipCollisionColor) {
                            _state.vreg[0x0f] = 1;
                        }
                        _state.megaChipBuffer[col + rowOffset] = pixel;
                    }
                }
                rowOffset += 0x100;
            }
        }
    } else {
        std::uint32_t source = _state.iregister;
        for (std::uint32_t row = y; static_cast<int>(row) < static_cast<int>(y + height); ++row) {
            if (static_cast<int>(row) < 0xc0) {
                const std::uint8_t bits = CHARACTERS.at(source++);
                std::uint8_t bit = 0;
                for (std::uint32_t col = x; static_cast<int>(col) < static_cast<int>(x + 8); ++col) {
                    const std::uint32_t on = ((0x80U >> (bit & 0x1f)) & bits) != 0;
                    if (static_cast<int>(col) < 0x100 && on != 0) {
                        if (_state.megaChipBuffer[col + rowOffset] == _state.megaChipCollisionColor &&
                            on != _state.megaChipCollisionColor) {
                            _state.vreg[0x0f] = 1;
                            ++_state.collisionPixels;
                        }
                        _state.megaChipBuffer[col + rowOffset] = 0xff;
                        ++_state.drawnPixels;
                    }
                    ++bit;
                }
                rowOffset += 0x100;
            }
        }
    }
    _state.charset = 0;
}

void Interpreter::initColorScheme(std::int32_t scheme)
{
    if (scheme == 0) { _state.foregroundColor = 0xa0c0f0; _state.backgroundColor = 0x2030; }
    if (scheme == 1) { _state.foregroundColor = 0xc090f0; _state.backgroundColor = 0x300020; }
    if (scheme == 2) { _state.foregroundColor = 0x30f0c0; _state.backgroundColor = 0x2030; }
    if (scheme == 3) { _state.foregroundColor = 0xffa020; _state.backgroundColor = 0x201000; }
    if (scheme == 4) { _state.foregroundColor = 0xff20; _state.backgroundColor = 0x2004; }
    if (scheme == 5) { _state.foregroundColor = 0xffffff; _state.backgroundColor = 0; }
}

void Interpreter::runRefresh(float frameStep)
{
    _state.currentTick = 0;
    do {
        runCpu(frameStep);
        // megachip.exe increments this once in runCpu and once in coremain.
        ++_state.currentTick;
        if (static_cast<float>(_state.ticksPerFrame) * frameStep <= static_cast<float>(_state.currentTick)) {
            break;
        }
    } while (_state.exit == 0);

    if (_state.megaChipEnabled == 0) {
        updateScreen();
    }
    getKeys();
}

std::uint8_t Interpreter::readByte(std::uint32_t address) const
{
    if (address >= _state.memory.size()) {
        throw std::out_of_range("Chip8 memory read out of bounds at " + std::to_string(address));
    }
    return _state.memory[address];
}

void Interpreter::writeByte(std::uint32_t address, std::uint8_t value)
{
    if (address >= _state.memory.size()) {
        throw std::out_of_range("Chip8 memory write out of bounds at " + std::to_string(address));
    }
    _state.memory[address] = value;
}

std::uint16_t Interpreter::opcodeNnn(std::uint16_t opcode)
{
    return opcode & 0x0fff;
}

std::uint8_t Interpreter::shiftedHigh(std::uint8_t value, std::uint8_t bitOffset)
{
    if (bitOffset == 0) return value;
    return static_cast<std::uint8_t>((value & (0xffU << bitOffset)) >> bitOffset);
}

std::uint8_t Interpreter::shiftedCarry(std::uint8_t value, std::uint8_t bitOffset)
{
    if (bitOffset == 0) return 0;
    return static_cast<std::uint8_t>(value << (8 - bitOffset));
}

} // namespace emu::rsm8k1detail
