//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/systembus.hpp
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
#include <array>
#include <cstdint>
#include <memory>

#include "imemorycontroller.hpp"

namespace hp48 {

class ISystemBus {
public:
    virtual ~ISystemBus() = default;

    [[nodiscard]] virtual std::uint8_t readNibble(std::uint32_t address) = 0;
    [[nodiscard]] virtual bool readUpdatesCrc(std::uint32_t address) { (void)address; return true; }
    virtual void writeNibble(std::uint32_t address, std::uint8_t value) = 0;

    virtual void executeResetOpcode() = 0;
    virtual void executeConfigOpcode(std::uint32_t cRegister) = 0;
    virtual void executeUnconfigOpcode(std::uint32_t cRegister) = 0;
    virtual void executeCIdOpcode(std::uint64_t& cRegisterRef) = 0;
    [[nodiscard]] virtual bool executeShutdownOpcode() { return true; }
    virtual void executeWakeFromShutdown() {}
    [[nodiscard]] virtual bool hasInterruptRequest() { return false; }
};

class SystemBus : public ISystemBus {
private:
    // Ordered strictly by Arbitration Priority: HDW, NCE2, CE2, CE1, NCE3
    std::array<IMemoryController*, 5> _priorityBus;
    
    // Ordered strictly by Physical Daisy Chain wiring
    std::array<IMemoryController*, 5> _daisyChain;
    
    // The unconfigurable, always-active fallback ROM
    std::shared_ptr<IMemoryController> _nce1Rom; 

public:
    SystemBus(std::array<IMemoryController*, 5> priority, 
              std::array<IMemoryController*, 5> daisy, 
              std::shared_ptr<IMemoryController> rom)
        : _priorityBus(priority), _daisyChain(daisy), _nce1Rom(std::move(rom)) {}

    [[nodiscard]] std::uint8_t readNibble(std::uint32_t address) override {
        // Enforce the 20-bit address bus physical limit
        address &= 0xFFFFF; 
        
        // Iterate through the arbitration priority layers
        for (auto* controller : _priorityBus) {
            if (controller->isConfigured() && controller->coversAddress(address)) {
                return controller->readNibble(address);
            }
        }
        // Fallback to the lowest priority NCE1 ROM if no cover exists
        return _nce1Rom->readNibble(address);
    }

    [[nodiscard]] bool readUpdatesCrc(std::uint32_t address) override {
        address &= 0xFFFFF;
        for (auto* controller : _priorityBus) {
            if (controller->isConfigured() && controller->coversAddress(address)) {
                return controller->updatesCrcOnRead();
            }
        }
        return _nce1Rom->updatesCrcOnRead();
    }

    void writeNibble(std::uint32_t address, std::uint8_t value) override {
        address &= 0xFFFFF;
        value &= 0xF;
        
        for (auto* controller : _priorityBus) {
            if (controller->isConfigured() && controller->coversAddress(address)) {
                controller->writeNibble(address, value);
                return; // Write handled by highest priority cover
            }
        }
        // Writes falling through to the ROM are inherently ignored
        _nce1Rom->writeNibble(address, value);
    }

    void executeResetOpcode() override {
        for (auto* controller : _daisyChain) {
            controller->reset();
        }
    }

    void executeConfigOpcode(std::uint32_t cRegister) override {
        // Send the CONFIG signal down the physical wire
        for (auto* controller : _daisyChain) {
            if (!controller->isConfigured()) {
                // If the controller swallows the config command, stop propagation
                if (controller->handleConfig(cRegister)) {
                    return;
                }
            }
        }
    }

    void executeUnconfigOpcode(std::uint32_t cRegister) override {
        const std::uint32_t targetAddress = cRegister & 0xFFFFF;
        for (auto* controller : _daisyChain) {
            controller->handleUnconfig(targetAddress);
        }
    }
    
    void executeCIdOpcode(std::uint64_t& cRegisterRef) override {
        static constexpr std::uint64_t A_FIELD_MASK = 0xFFFFF;
        for (auto* controller : _daisyChain) {
            if (!controller->isConfigured()) {
                if (auto id = controller->handleIdRequest()) {
                    cRegisterRef = (cRegisterRef & ~A_FIELD_MASK) | (*id & A_FIELD_MASK);
                    return;
                }
            }
        }
        cRegisterRef &= ~A_FIELD_MASK; // No unconfigured devices remaining
    }

    [[nodiscard]] bool executeShutdownOpcode() override {
        bool shutdownAllowed = true;
        for (auto* controller : _daisyChain) {
            shutdownAllowed = controller->handleShutdownOpcode() && shutdownAllowed;
        }
        return shutdownAllowed;
    }

    void executeWakeFromShutdown() override {
        for (auto* controller : _daisyChain) {
            controller->handleWakeFromShutdown();
        }
    }

    [[nodiscard]] bool hasInterruptRequest() override {
        bool interruptRequested = false;
        for (auto* controller : _daisyChain) {
            interruptRequested = controller->hasInterruptRequest() || interruptRequested;
        }
        return interruptRequested;
    }
};

} // namespace hp48
