//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/display.hpp
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
#include <span>

#include <ghc/span2d.hpp>

#include "stack.hpp"

namespace hp48 {

class Hp48Display
{
public:
    enum Mode { Stack, FrameBuffer };
    // The display contrast is a 5 bit value (HDW 0x101 and bit 0 of 0x102), the ROM sets 11 after a reset
    static constexpr int MIN_CONTRAST = 0;
    static constexpr int MAX_CONTRAST = 31;
    static constexpr int DEFAULT_CONTRAST = 11;
    struct Annunciators {
        bool leftShift = false;
        bool rightShift = false;
        bool alpha = false;
        bool alarm = false;
        bool busy = false;
        bool transfer = false;
    };

    explicit Hp48Display(std::span<uint8_t> frameBuffer, const ObjectStack& stack, const std::vector<std::string>& menuRef);

    void setAnnunciators(Annunciators annunciators);
    void setContrast(int contrast);
    [[nodiscard]] int contrast() const { return _contrast; }
    void setError(std::pair<std::string, std::string> error);
    void clearError();
    void render(ghc::span2d<uint32_t> pixelBuffer, Mode mode, int scale = 4);

private:
    static uint32_t lerpU32Bytes(uint32_t a, uint32_t b, uint8_t mix);
    static uint32_t mixColor(uint32_t from, uint32_t to, float amount);
    std::span<uint8_t> _frameBuffer;
    const ObjectStack& _stack;
    const std::vector<std::string>& _menuRef;
    std::pair<std::string, std::string> _error;
    Annunciators _annunciators;
    int _contrast{DEFAULT_CONTRAST};
};

}  // namespace hp48
