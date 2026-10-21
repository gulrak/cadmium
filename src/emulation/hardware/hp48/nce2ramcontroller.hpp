//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/nce2ramcontroller.hpp
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
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include "imemorycontroller.hpp"
#include "nibblememory.hpp"

namespace hp48 {

class Nce2RamController final : public IMemoryController {
public:
    enum class ConfigState { AwaitingSize, AwaitingAddress, Configured };

    struct Snapshot {
        ConfigState state = ConfigState::AwaitingSize;
        std::uint32_t baseAddress = 0;
        std::uint32_t configuredSize = 0;
        std::vector<std::uint8_t> packedBytes;
    };

private:
    
    ConfigState _state = ConfigState::AwaitingSize;
    std::uint32_t _baseAddress = 0;
    std::uint32_t _configuredSize = 0;
    hp48::NibbleMemory _ramData;
    static constexpr std::uint32_t SIZE_ID = 0xFF003;
    static constexpr std::uint32_t ADDRESS_ID = 0x000F4;

public:
    explicit Nce2RamController(std::size_t physicalNibbles) : _ramData(physicalNibbles) {}

    void reset() override {
        _state = ConfigState::AwaitingSize;
        _baseAddress = 0;
        _configuredSize = 0;
    }

    void configure(std::uint32_t base, std::uint32_t size) {
        _baseAddress = base & 0xFFFFF;
        _configuredSize = size;
        _state = ConfigState::Configured;
    }

    [[nodiscard]] Snapshot snapshot() const {
        return Snapshot {
            .state = _state,
            .baseAddress = _baseAddress,
            .configuredSize = _configuredSize,
            .packedBytes = _ramData.packedBytes(),
        };
    }

    void restoreSnapshot(const Snapshot& snapshot) {
        if (snapshot.packedBytes.size() != _ramData.packedBytes().size()) {
            throw std::runtime_error("RAM snapshot has wrong packed byte size");
        }
        _state = snapshot.state;
        _baseAddress = snapshot.baseAddress & 0xFFFFF;
        _configuredSize = snapshot.configuredSize;
        _ramData.packedBytes() = snapshot.packedBytes;
    }

    [[nodiscard]] std::size_t physicalNibbles() const noexcept {
        return _ramData.size();
    }

    [[nodiscard]] bool isConfigured() const override {
        return _state == ConfigState::Configured;
    }

    std::optional<std::uint32_t> handleIdRequest() override {
        if (_state == ConfigState::AwaitingSize) {
            return (_configuredSize & 0xFF000) ^ SIZE_ID;
        }
        if (_state == ConfigState::AwaitingAddress) {
            return (_baseAddress & 0xFF000) ^ ADDRESS_ID;
        }
        return std::nullopt;
    }

    bool handleConfig(std::uint32_t cRegister) override {
        if (_state == ConfigState::AwaitingSize) {
            // Mathematical size computation: #100000 - C
            std::uint32_t requestedSize = 0x100000 - (cRegister & 0xFFFFF);
            _configuredSize = requestedSize;
            _state = ConfigState::AwaitingAddress;
            return true; // Successfully intercepted the CONFIG command
        } else if (_state == ConfigState::AwaitingAddress) {
            _baseAddress = cRegister & 0xFFFFF;
            _state = ConfigState::Configured;
            return true; 
        }
        return false; // Already configured, pass the signal down the wire
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
        std::uint32_t localOffset = address - _baseAddress;
        // Handle mirroring if the logical window is larger than physical memory
        return _ramData.at(localOffset & 0xFFFF);
    }

    void writeNibble(std::uint32_t address, std::uint8_t value) override {
        std::uint32_t localOffset = address - _baseAddress;
        _ramData.at(localOffset & 0xFFFF) = value;
    }
};

}
