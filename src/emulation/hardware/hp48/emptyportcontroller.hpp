//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/emptyportcontroller.hpp
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

#include <cstdint>
#include <optional>

#include "imemorycontroller.hpp"
#include "nibblememory.hpp"

namespace hp48 {

class EmptyPortController final : public IMemoryController {
public:
    enum class ConfigState { AwaitingSize, AwaitingAddress, Configured };

    struct Snapshot {
        ConfigState state = ConfigState::AwaitingSize;
        std::uint32_t baseAddress = 0;
        std::uint32_t configuredSize = 0;
    };

private:
    
    ConfigState _state = ConfigState::AwaitingSize;
    std::uint32_t _baseAddress = 0;
    std::uint32_t _configuredSize = 0;
    
    const std::uint32_t _sizeId;
    const std::uint32_t _addressId;
    hp48::NibbleMemory _data;

public:
    EmptyPortController(std::uint32_t sizeId, std::uint32_t addressId, std::uint32_t physicalNibbles = 0x100000)
        : _sizeId(sizeId)
        , _addressId(addressId)
        , _data(physicalNibbles) {}

    void reset() override {
        _state = ConfigState::AwaitingSize;
        _baseAddress = 0;
        _configuredSize = 0;
    }

    [[nodiscard]] Snapshot snapshot() const {
        return Snapshot {
            .state = _state,
            .baseAddress = _baseAddress,
            .configuredSize = _configuredSize,
        };
    }

    void restoreSnapshot(const Snapshot& snapshot) {
        _state = snapshot.state;
        _baseAddress = snapshot.baseAddress & 0xFFFFF;
        _configuredSize = snapshot.configuredSize;
    }

    [[nodiscard]] bool isConfigured() const override {
        return _state == ConfigState::Configured;
    }

    std::optional<std::uint32_t> handleIdRequest() override {
        if (_state == ConfigState::AwaitingSize) {
            return (_configuredSize & 0xFF000) ^ _sizeId;
        }
        if (_state == ConfigState::AwaitingAddress) {
            return (_baseAddress & 0xFF000) ^ _addressId;
        }
        return std::nullopt;
    }

    bool handleConfig(std::uint32_t cRegister) override {
        if (_state == ConfigState::AwaitingSize) {
            // Calculate size using the standard HP-48 2's complement math
            _configuredSize = 0x100000 - (cRegister & 0xFFFFF);
            _state = ConfigState::AwaitingAddress;
            return true; // Command intercepted
        } else if (_state == ConfigState::AwaitingAddress) {
            // Second config call sets the logical base address
            _baseAddress = cRegister & 0xFFFFF;
            _state = ConfigState::Configured;
            return true; // Command intercepted
        }
        return false; // Already configured, pass down the daisy chain
    }

    void handleUnconfig(std::uint32_t targetAddress) override {
        if (_state == ConfigState::Configured && _baseAddress == targetAddress) {
            reset();
        }
    }

    [[nodiscard]] bool coversAddress(std::uint32_t address) const override {
        if (!isConfigured()) return false;
        return address >= _baseAddress && address < (_baseAddress + _configuredSize);
    }

    [[nodiscard]] std::uint8_t readNibble(std::uint32_t address) override {
        const std::uint32_t localOffset = address - _baseAddress;
        return _data.at(localOffset % _data.size());
    }

    void writeNibble(std::uint32_t address, std::uint8_t value) override {
        const std::uint32_t localOffset = address - _baseAddress;
        _data.at(localOffset % _data.size()) = value;
    }
};

} // namespace hp48
