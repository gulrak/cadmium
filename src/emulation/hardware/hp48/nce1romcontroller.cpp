//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/nce1romcontroller.cpp
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

#include "nce1romcontroller.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace {

constexpr std::size_t SX_ROM_PACKED_BYTES = 0x40000;
constexpr std::size_t SX_ROM_NIBBLES = 0x80000;

std::vector<std::uint8_t> normalizeRomBytes(std::vector<std::uint8_t> romBytes, const std::string& romName)
{
    if (romBytes.size() == SX_ROM_PACKED_BYTES) {
        return romBytes;
    }
    if (romBytes.size() == SX_ROM_NIBBLES) {
        std::vector<std::uint8_t> packed;
        packed.reserve(SX_ROM_PACKED_BYTES);
        for (std::size_t i = 0; i < romBytes.size(); i += 2) {
            const std::uint8_t lo = romBytes[i];
            const std::uint8_t hi = romBytes[i + 1];
            if (lo > 0x0f || hi > 0x0f) {
                throw std::runtime_error("nibble ROM contains values outside 0..15: " + romName);
            }
            packed.push_back(static_cast<std::uint8_t>(lo | (hi << 4U)));
        }
        return packed;
    }
    throw std::runtime_error("HP48SX ROM must be 256 KiB packed or 512 KiB nibble-expanded: " + romName);
}

}

namespace hp48 {

Nce1RomController::Nce1RomController(std::vector<std::uint8_t> romBytes, std::string romName)
    : _ownedRomBytes(normalizeRomBytes(std::move(romBytes), romName.empty() ? std::string("custom ROM") : romName))
    , _romBytes(_ownedRomBytes.data())
    , _romByteCount(_ownedRomBytes.size())
    , _romName(romName.empty() ? "custom ROM" : std::move(romName))
{
}

std::uint8_t Nce1RomController::readNibble(std::uint32_t address)
{
    // The 20-bit address bus naturally wraps around the 256 KB physical ROM.
    // 256 KB is exactly 524,288 (0x80000) nibbles.
    auto bi = (address >> 1) % _romByteCount; // /2
    const bool high = (address & 1u) != 0u; // odd => high nibble
    return high ? (_romBytes[bi] >> 4) : _romBytes[bi] & 0xF;
}

}
