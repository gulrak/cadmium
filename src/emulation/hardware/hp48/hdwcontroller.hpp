//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/hdwcontroller.hpp
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

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "imemorycontroller.hpp"

namespace hp48 {

class HdwController final : public IMemoryController {
public:
    static constexpr std::uint32_t TIMER2_HZ = 8192;
    static constexpr std::uint32_t COMPARE_TIMER2_CYCLES_PER_TICK = 144;
    static constexpr std::uint32_t TIMER2_CYCLES_PER_TICK = COMPARE_TIMER2_CYCLES_PER_TICK;

    // I plan no card support, that's why there is no AwaitingSize state ;-)
    enum class ConfigState { AwaitingAddress, Configured };

    struct Snapshot {
        ConfigState state = ConfigState::AwaitingAddress;
        std::uint32_t baseAddress = 0;
        std::vector<std::uint8_t> registers;
        std::uint32_t timerCounter = 0;
        std::uint8_t timer1Counter = 0;
        std::uint32_t timer2LowReadCount = 0;
        std::uint64_t timer1CycleRef = 0;
        std::uint64_t timer2CycleRef = 0;
        bool timersStarted = false;
        bool shutdownActive = false;
    };

private:
    
    ConfigState _state = ConfigState::AwaitingAddress;
    std::uint32_t _baseAddress = 0;
    
    // HDW is strictly fixed at 64 nibbles (32 bytes)
    const std::uint32_t _configuredSize = 64; 
    
    // The specific hardware ID the ROM expects for the HDW block
    const std::uint32_t _hardwareId = 0x19; 

    // The backing array for the 64 nibbles of hardware registers
    std::vector<std::uint8_t> _registers;
    std::uint16_t* _crcRegister = nullptr;
    const std::uint64_t* _cycleCounter = nullptr;

    static constexpr std::uint32_t TIMER2_CONTROL_OFFSET = 0x2f;
    static constexpr std::uint32_t TIMER1_CONTROL_OFFSET = 0x2e;
    static constexpr std::uint32_t SRQ1_OFFSET = 0x18;
    static constexpr std::uint32_t TIMER1_OFFSET = 0x37;
    static constexpr std::uint32_t TIMER2_OFFSET = 0x38;
    static constexpr std::uint8_t TIMER_RUN = 0x01;
    static constexpr std::uint8_t TIMER_INTERRUPT = 0x02;
    static constexpr std::uint8_t TIMER_WAKE = 0x04;
    static constexpr std::uint8_t TIMER_SERVICE_REQUEST = 0x08;
    static constexpr std::uint8_t TIMER_SERVICE_LINE = 0x04;

    // Timer2 is a 32-bit down-counter returned at nibble addr 0x138-0x13F
    // (local offset 0x38-0x3F) on the SX.
    std::uint32_t _timerCounter = 0;
    std::uint8_t _timer1Counter = 0;
    std::uint32_t _timer2LowReadCount = 0;
    std::uint64_t _timer1CycleRef = 0;
    std::uint64_t _timer2CycleRef = 0;
    bool _timersStarted = false;
    bool _shutdownActive = false;
    std::uint32_t _timer2CyclesPerTick = COMPARE_TIMER2_CYCLES_PER_TICK;

    [[nodiscard]] std::uint32_t timer1CyclesPerTick() const {
        return _timer2CyclesPerTick * 512U;
    }

public:
    explicit HdwController(std::uint32_t timer2CyclesPerTick = COMPARE_TIMER2_CYCLES_PER_TICK)
        : _timer2CyclesPerTick(timer2CyclesPerTick)
    {
        _registers.resize(_configuredSize, 0x0);
        reset();
    }

    [[nodiscard]] std::uint32_t timer2CyclesPerTick() const {
        return _timer2CyclesPerTick;
    }

    void setTimer2CyclesPerTick(std::uint32_t timer2CyclesPerTick) {
        updateTimer2();
        updateTimer1();
        _timer2CyclesPerTick = timer2CyclesPerTick;
        _timer1CycleRef = currentCycles();
        _timer2CycleRef = currentCycles();
    }

    void reset() override {
        const bool preserveTimers = _timersStarted;
        const std::uint32_t timerCounter = _timerCounter;
        const std::uint8_t timer1Counter = _timer1Counter;
        const std::uint64_t timer1CycleRef = _timer1CycleRef;
        const std::uint64_t timer2CycleRef = _timer2CycleRef;
        const std::uint8_t timer1Control = _registers[TIMER1_CONTROL_OFFSET];
        const std::uint8_t timer2Control = _registers[TIMER2_CONTROL_OFFSET];

        _state = ConfigState::AwaitingAddress;
        _baseAddress = 0;
        _timerCounter = 0;
        _timer1Counter = 0;
        _timer2LowReadCount = 0;
        _timer1CycleRef = currentCycles();
        _timer2CycleRef = currentCycles();
        _timersStarted = false;
        _shutdownActive = false;
        std::ranges::fill(_registers, 0);
        _registers[0x09] = 0x01;
        _registers[0x0f] = 0x05;
        if (preserveTimers) {
            _timerCounter = timerCounter;
            _timer1Counter = timer1Counter;
            _timer1CycleRef = timer1CycleRef;
            _timer2CycleRef = timer2CycleRef;
            _timersStarted = true;
            _registers[0x00] = 0x08;
            _registers[0x01] = 0x0b;
            _registers[TIMER1_CONTROL_OFFSET] = timer1Control;
            _registers[TIMER2_CONTROL_OFFSET] = timer2Control | TIMER_RUN;
            _registers[TIMER1_OFFSET] = timer1Counter;
        }
    }

    void configure(std::uint32_t base) {
        _baseAddress = base & 0xFFFC0;
        _state = ConfigState::Configured;
    }

    [[nodiscard]] Snapshot snapshot() const {
        return Snapshot {
            .state = _state,
            .baseAddress = _baseAddress,
            .registers = _registers,
            .timerCounter = _timerCounter,
            .timer1Counter = _timer1Counter,
            .timer2LowReadCount = _timer2LowReadCount,
            .timer1CycleRef = _timer1CycleRef,
            .timer2CycleRef = _timer2CycleRef,
            .timersStarted = _timersStarted,
            .shutdownActive = _shutdownActive,
        };
    }

    void restoreSnapshot(const Snapshot& snapshot) {
        if (snapshot.registers.size() != _registers.size()) {
            throw std::runtime_error("HDW snapshot has wrong register count");
        }
        _state = snapshot.state;
        _baseAddress = snapshot.baseAddress & 0xFFFC0;
        _registers = snapshot.registers;
        _timerCounter = snapshot.timerCounter;
        _timer1Counter = snapshot.timer1Counter & 0x0fU;
        _timer2LowReadCount = snapshot.timer2LowReadCount;
        _timer1CycleRef = snapshot.timer1CycleRef;
        _timer2CycleRef = snapshot.timer2CycleRef;
        _timersStarted = snapshot.timersStarted;
        _shutdownActive = snapshot.shutdownActive;
    }

    [[nodiscard]] bool isConfigured() const override {
        return _state == ConfigState::Configured;
    }

    std::optional<std::uint32_t> handleIdRequest() override {
        return _hardwareId;
    }

    bool handleConfig(std::uint32_t cRegister) override {
        if (_state == ConfigState::AwaitingAddress) {
            // HDW skips the size configuration step entirely.
            // The ROM directly passes the target base address.
            // Enforce the 64-nibble boundary alignment mask.
            _baseAddress = cRegister & 0xFFFC0; 
            _state = ConfigState::Configured;
            return true; // Successfully intercepted the CONFIG command
        }
        return false; // Already configured, pass down the wire
    }

    void handleUnconfig(std::uint32_t targetAddress) override {
        if (_state == ConfigState::Configured && _baseAddress == targetAddress) {
            reset();
        }
    }

    [[nodiscard]] bool handleShutdownOpcode() override {
        bool shutdownAllowed = true;
        if ((_registers[TIMER1_CONTROL_OFFSET] & TIMER_WAKE) != 0) {
            updateTimer1();
            if ((_timer1Counter & 0x08) != 0) {
                acceptTimer1Wake();
                shutdownAllowed = false;
            }
        }
        if ((_registers[TIMER2_CONTROL_OFFSET] & TIMER_WAKE) != 0) {
            updateTimer2();
            if ((_timerCounter & 0x80000000U) != 0) {
                acceptTimer2Wake();
                shutdownAllowed = false;
            }
        }
        _shutdownActive = shutdownAllowed;
        return shutdownAllowed;
    }

    void handleWakeFromShutdown() override {
        _shutdownActive = false;
    }

    [[nodiscard]] bool hasInterruptRequest() override {
        updateTimer1();
        updateTimer2();
        updateTimer1Control(false);
        updateTimer2Control(false);
        return timer1InterruptRequested() || timer2InterruptRequested();
    }

    [[nodiscard]] bool coversAddress(std::uint32_t address) const override {
        if (!isConfigured()) return false;
        return address >= _baseAddress && address < (_baseAddress + _configuredSize);
    }

    [[nodiscard]] bool updatesCrcOnRead() const override {
        return false;
    }

    void setCrcRegister(std::uint16_t* crcRegister) {
        _crcRegister = crcRegister;
    }

    void setCycleCounter(const std::uint64_t* cycleCounter) {
        _cycleCounter = cycleCounter;
        _timer1CycleRef = currentCycles();
        _timer2CycleRef = currentCycles();
    }

    [[nodiscard]] std::uint8_t readNibble(std::uint32_t address) override {
        std::uint32_t localOffset = address - _baseAddress;
        if (_crcRegister != nullptr && localOffset >= 0x04 && localOffset <= 0x07) {
            return static_cast<std::uint8_t>((*_crcRegister >> ((localOffset - 0x04) * 4)) & 0x0f);
        }
        if (localOffset == TIMER1_OFFSET) {
            advanceTimer1BeforeRead();
            updateTimer1Control(true);
            return _timer1Counter;
        }
        if (localOffset == TIMER1_CONTROL_OFFSET) {
            updateTimer1();
            updateTimer1Control(true);
            return _registers[localOffset];
        }
        if (localOffset == TIMER2_CONTROL_OFFSET) {
            updateTimer2();
            updateTimer2Control(true);
            return _registers[localOffset];
        }
        if (localOffset >= TIMER2_OFFSET && localOffset <= TIMER2_OFFSET + 7) {
            advanceTimer2BeforeRead(localOffset);
            updateTimer2Control(true);
            return static_cast<std::uint8_t>((_timerCounter >> ((localOffset - TIMER2_OFFSET) * 4)) & 0x0f);
        }
        return _registers[localOffset];
    }

    void writeNibble(std::uint32_t address, std::uint8_t value) override {
        std::uint32_t localOffset = address - _baseAddress;
        value &= 0x0f;
        if (_crcRegister != nullptr && localOffset >= 0x04 && localOffset <= 0x07) {
            const std::uint16_t mask = static_cast<std::uint16_t>(0x000fU << ((localOffset - 0x04) * 4));
            *_crcRegister = static_cast<std::uint16_t>((*_crcRegister & ~mask) | (static_cast<std::uint16_t>(value) << ((localOffset - 0x04) * 4)));
            return;
        }
        if (localOffset == TIMER1_OFFSET) {
            updateTimer1();
            _timer1Counter = value;
            _timer1CycleRef = currentCycles();
            _registers[localOffset] = value;
            updateTimer1Control(false);
            return;
        }
        if (localOffset == TIMER1_CONTROL_OFFSET) {
            updateTimer1();
            _registers[localOffset] = value;
            updateTimer1Control(false);
            return;
        }
        if (localOffset == TIMER2_CONTROL_OFFSET) {
            updateTimer1();
            updateTimer2();
            _registers[localOffset] = value;
            if ((value & TIMER_RUN) != 0) {
                if (!_timersStarted) {
                    _timersStarted = true;
                    _timer1CycleRef = currentCycles();
                    _timer2CycleRef = currentCycles();
                    _timer2LowReadCount = 0;
                }
            } else {
                _timersStarted = false;
            }
            updateTimer2Control(false);
            return;
        }
        if (localOffset >= TIMER2_OFFSET && localOffset <= TIMER2_OFFSET + 7) {
            updateTimer2();
            const std::uint32_t shift = (localOffset - TIMER2_OFFSET) * 4;
            const std::uint32_t mask = 0x0fU << shift;
            _timerCounter = (_timerCounter & ~mask) | (static_cast<std::uint32_t>(value) << shift);
            _timer2LowReadCount = 0;
            _timer2CycleRef = currentCycles();
            _registers[localOffset] = value;
            updateTimer2Control(false);
            return;
        }
        _registers[localOffset] = value;
    }

private:
    void advanceTimer2BeforeRead(std::uint32_t localOffset) {
        (void)localOffset;
        updateTimer2();
    }

    void advanceTimer1BeforeRead() {
        updateTimer1();
    }

    [[nodiscard]] std::uint64_t currentCycles() const {
        return _cycleCounter != nullptr ? *_cycleCounter : 0;
    }

    void updateTimer2() {
        if (!_timersStarted || (_registers[TIMER2_CONTROL_OFFSET] & TIMER_RUN) == 0) {
            _timer2CycleRef = currentCycles();
            updateTimerSrqLine();
            return;
        }
        const std::uint64_t cycles = currentCycles();
        const std::uint64_t elapsed = cycles - _timer2CycleRef;
        const std::uint64_t ticks = elapsed / _timer2CyclesPerTick;
        if (ticks == 0) {
            return;
        }
        _timerCounter -= static_cast<std::uint32_t>(ticks);
        _timer2CycleRef += ticks * _timer2CyclesPerTick;
    }

    void updateTimer1() {
        if (!_timersStarted || (_registers[TIMER2_CONTROL_OFFSET] & TIMER_RUN) == 0) {
            _timer1CycleRef = currentCycles();
            updateTimerSrqLine();
            return;
        }
        const std::uint64_t cycles = currentCycles();
        const std::uint64_t elapsed = cycles - _timer1CycleRef;
        const std::uint64_t ticks = elapsed / timer1CyclesPerTick();
        if (ticks == 0) {
            return;
        }
        _timer1Counter = static_cast<std::uint8_t>((_timer1Counter - ticks) & 0x0fU);
        _registers[TIMER1_OFFSET] = _timer1Counter;
        _timer1CycleRef += ticks * timer1CyclesPerTick();
    }

    void updateTimer1Control(bool clearWake) {
        std::uint8_t& control = _registers[TIMER1_CONTROL_OFFSET];
        if ((_timer1Counter & 0x08U) == 0) {
            control &= static_cast<std::uint8_t>(~TIMER_SERVICE_REQUEST);
            updateTimerSrqLine();
            return;
        }
        if ((control & (TIMER_WAKE | TIMER_INTERRUPT)) != 0) {
            control |= TIMER_SERVICE_REQUEST;
        }
        if (clearWake && _shutdownActive && (control & TIMER_WAKE) != 0) {
            acceptTimer1Wake();
        }
        updateTimerSrqLine();
    }

    void updateTimer2Control(bool clearWake) {
        std::uint8_t& control = _registers[TIMER2_CONTROL_OFFSET];
        if ((_timerCounter & 0x80000000U) == 0) {
            control &= static_cast<std::uint8_t>(~TIMER_SERVICE_REQUEST);
            updateTimerSrqLine();
            return;
        }
        if ((control & (TIMER_WAKE | TIMER_INTERRUPT)) != 0) {
            control |= TIMER_SERVICE_REQUEST;
        }
        if (clearWake && _shutdownActive && (control & TIMER_WAKE) != 0) {
            acceptTimer2Wake();
        }
        updateTimerSrqLine();
    }

    void acceptTimer1Wake() {
        std::uint8_t& control = _registers[TIMER1_CONTROL_OFFSET];
        if ((control & (TIMER_WAKE | TIMER_INTERRUPT)) != 0) {
            control |= TIMER_SERVICE_REQUEST;
        }
        control &= static_cast<std::uint8_t>(~TIMER_WAKE);
        _shutdownActive = false;
        updateTimerSrqLine();
    }

    void acceptTimer2Wake() {
        std::uint8_t& control = _registers[TIMER2_CONTROL_OFFSET];
        if ((control & (TIMER_WAKE | TIMER_INTERRUPT)) != 0) {
            control |= TIMER_SERVICE_REQUEST;
        }
        control &= static_cast<std::uint8_t>(~TIMER_WAKE);
        _shutdownActive = false;
        updateTimerSrqLine();
    }

    [[nodiscard]] bool timer1InterruptRequested() const {
        return (_registers[TIMER1_CONTROL_OFFSET] & TIMER_INTERRUPT) != 0 && (_timer1Counter & 0x08U) != 0;
    }

    [[nodiscard]] bool timer2InterruptRequested() const {
        return (_registers[TIMER2_CONTROL_OFFSET] & TIMER_INTERRUPT) != 0 && (_timerCounter & 0x80000000U) != 0;
    }

    void updateTimerSrqLine() {
        if (timer1InterruptRequested() || timer2InterruptRequested()) {
            _registers[SRQ1_OFFSET] |= TIMER_SERVICE_LINE;
        } else {
            _registers[SRQ1_OFFSET] &= static_cast<std::uint8_t>(~TIMER_SERVICE_LINE);
        }
    }
};

}
