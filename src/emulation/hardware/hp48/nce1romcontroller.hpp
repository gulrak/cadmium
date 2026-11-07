//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/nce1romcontroller.hpp
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
#include <string>
#include <vector>

#include "imemorycontroller.hpp"

namespace hp48 {
class Nce1RomController final : public IMemoryController {
    // 256 KB ROM equals exactly 524,288 nibbles
public:
    // The ROM is HP's and not part of Cadmium, it always has to be given.
    explicit Nce1RomController(std::vector<std::uint8_t> romBytes, std::string romName = {});

    // NCE1 is immutable and completely unaffected by the hardware RESET opcode
    void reset() override {}

    // NCE1 is hardwired to always be configured and active
    [[nodiscard]] bool isConfigured() const override {
        return true;
    }

    // NCE1 does not participate in the C=ID query chain
    std::optional<std::uint32_t> handleIdRequest() override {
        return std::nullopt;
    }

    // NCE1 has no configuration registers and ignores CONFIG commands
    bool handleConfig(std::uint32_t /*cRegister*/) override {
        return false;
    }

    // NCE1 cannot be unconfigured by the software
    void handleUnconfig(std::uint32_t /*targetAddress*/) override {}

    // As the universal fallback, it theoretically covers the entire 20-bit address space
    [[nodiscard]] bool coversAddress(std::uint32_t /*address*/) const override {
        return true;
    }

    [[nodiscard]] std::uint8_t readNibble(std::uint32_t address) override;
    [[nodiscard]] const std::string& romName() const { return _romName; }

    void writeNibble(std::uint32_t /*address*/, std::uint8_t /*value*/) override {
        // The ROM is strictly read-only; writes are physically ignored by the NCE1 controller
    }

private:
    std::vector<std::uint8_t> _ownedRomBytes;
    const std::uint8_t* _romBytes = nullptr;
    std::size_t _romByteCount = 0;
    std::string _romName;
};

}
