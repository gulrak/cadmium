//---------------------------------------------------------------------------------------
// src/emulation/cdp186x.hpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2022, Steffen Schümann <s.schuemann@pobox.com>
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

#include <emulation/cdp186x.hpp>
#include <emulation/cdp1802.hpp>
#include <emulation/logger.hpp>

#include <cstring>

namespace emu {

Cdp186x::Cdp186x(Type type, Cdp1802& cpu, const Chip8EmulatorOptions& options)
: _cpu(cpu)
, _type(type)
, _options(options)
{
    reset();
}

void Cdp186x::reset()
{
    _frameCounter = 0;
    disableDisplay();
}

void Cdp186x::enableDisplay()
{
    _displayEnabled = true;
}

void Cdp186x::disableDisplay()
{
    std::memset(_screenBuffer.data(), 0, _screenBuffer.size());
    _displayEnabled = false;
}

bool Cdp186x::getNEFX() const
{
    return _frameCycle < (VIDEO_FIRST_VISIBLE_LINE - 4) * 14 || _frameCycle >= (VIDEO_FIRST_INVISIBLE_LINE - 4) * 14;
}

const uint8_t* Cdp186x::getScreenBuffer() const
{
    return _screenBuffer.data();
}

int Cdp186x::executeStep()
{
    auto fc = (_cpu.getCycles() >> 3) % 3668;
    if(fc < _frameCycle)
        ++_frameCounter;
    _frameCycle = fc;
    if(_frameCycle > VIDEO_FIRST_INVISIBLE_LINE * 14 || _frameCycle < (VIDEO_FIRST_VISIBLE_LINE - 2) * 14)
        return _frameCycle;
    if(_displayEnabled && _frameCycle < VIDEO_FIRST_VISIBLE_LINE * 14 && _cpu.getIE()) {
        if(_options.optTraceLog)
            Logger::log(Logger::eBACKEND_EMU, _cpu.getCycles(), {_frameCounter, _frameCycle}, fmt::format("{:24} ; {}", "--- IRQ ---", _cpu.dumpStateLine()).c_str());
        _cpu.triggerInterrupt();
    }
    else if(_frameCycle >= VIDEO_FIRST_VISIBLE_LINE * 14 && _frameCycle < VIDEO_FIRST_INVISIBLE_LINE * 14) {
        auto line = _frameCycle / 14;
        auto lineCycle = _frameCycle % 14;
        if(lineCycle == 2 || lineCycle == 3) {
            auto dmaStart = _cpu.getR(0);
            for (int i = 0; i < 8; ++i) {
                uint8_t* dest = &_screenBuffer[(line - VIDEO_FIRST_VISIBLE_LINE) * 256 + i * 8];
                auto data = _displayEnabled ? _cpu.executeDMAOut() : 0;
                for (int j = 0; j < 8; ++j) {
                    dest[j] = (data >> (7 - j)) & 1;
                }
            }
            if (_displayEnabled) {
                if(_options.optTraceLog)
                    Logger::log(Logger::eBACKEND_EMU, _cpu.getCycles(), {_frameCounter, _frameCycle}, fmt::format("DMA: line {:03d} 0x{:04x}-0x{:04x}", line, dmaStart, _cpu.getR(0) - 1).c_str());
            }
        }
    }
    return (_cpu.getCycles() >> 3) % 3668;
}

}
