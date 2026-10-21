//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/imemorycontroller.hpp
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
#include <vector>
#include <array>
#include <memory>

namespace hp48 {

class IMemoryController {
public:
    virtual ~IMemoryController() = default;
    
    // Daisy chain configuration protocol methodologies
    virtual void reset() = 0;
    [[nodiscard]] virtual bool isConfigured() const = 0;
    virtual std::optional<std::uint32_t> handleIdRequest() = 0;
    virtual bool handleConfig(std::uint32_t cRegisterValue) = 0;
    virtual void handleUnconfig(std::uint32_t targetAddress) = 0;
    [[nodiscard]] virtual bool handleShutdownOpcode() { return true; }
    virtual void handleWakeFromShutdown() {}
    [[nodiscard]] virtual bool hasInterruptRequest() { return false; }

    // Memory arbitration and data access methodologies
    [[nodiscard]] virtual bool coversAddress(std::uint32_t address) const = 0;
    [[nodiscard]] virtual bool updatesCrcOnRead() const { return true; }
    [[nodiscard]] virtual std::uint8_t readNibble(std::uint32_t address) = 0;
    virtual void writeNibble(std::uint32_t address, std::uint8_t value) = 0;
};

}
