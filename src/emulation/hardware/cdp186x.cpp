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

#include "cdp186x.hpp"
#include "cdp1802.hpp"
#include <emulation/logger.hpp>
#include <stdendian/stdendian.h>

#include <cstring>

namespace emu {

const uint32_t Cdp186x::_cdp1862BackgroundColors[4] = { 0x000080FF, 0x000000FF, 0x008000FF, 0x800000FF };
Cdp186x::Cdp186x(Type type, Cdp1802& cpu, const Chip8EmulatorOptions& options)
: _cpu(cpu)
, _type(type)
, _options(options)
{
    static uint32_t foregroundColors[8] = { 0x181818FF, 0xFF0000FF, 0x0000FFFF, 0xFF00FFFF, 0x00FF00FF, 0xFFFF00FF, 0x00FFFFFF, 0xFFFFFFFF };
    _screen.setMode(256, 192, 4); // actual resolution doesn't matter, just needs to be bigger than max resolution, but ratio matters
    for(int i = 0; i < 256; ++i) {
        if(i & 0xF) {
            _cdp1862Palette[i] = foregroundColors[i>>4];
        }
        else {
            _cdp1862Palette[i] = _cdp1862BackgroundColors[0];
        }
    }
    reset();
}

void Cdp186x::reset()
{
    if(_type == eVP590) {
        _subMode = eVP590_DEFAULT;
        _screen.setPalette(_cdp1862Palette);
        _backgroundColor = 0;
        for(int i = 0; i < 256; i += 16) {
            _cdp1862Palette[i] = _cdp1862BackgroundColors[_backgroundColor];
        }
        _screen.setPalette(_cdp1862Palette);
    }
    _frameCounter = 0;
    _displayEnabledLatch = false;
    disableDisplay();
}

void Cdp186x::enableDisplay()
{
    _displayEnabled = true;
}

void Cdp186x::disableDisplay()
{
    _screen.setAll(0);
    _displayEnabled = false;
}

bool Cdp186x::getNEFX() const
{
    return ((_frameCycle >= (VIDEO_FIRST_VISIBLE_LINE - 4) * 14 && _frameCycle < VIDEO_FIRST_VISIBLE_LINE * 14) || (_frameCycle >= (VIDEO_FIRST_INVISIBLE_LINE - 4) * 14 && _frameCycle < VIDEO_FIRST_INVISIBLE_LINE * 14));
}

const Cdp186x::VideoType& Cdp186x::getScreen() const
{
    return _screen;
}

int Cdp186x::executeStep()
{
    auto fc = (_cpu.getCycles() >> 3) % 3668;
    bool vsync = false;
    if(fc < _frameCycle) {
        vsync = true;
        ++_frameCounter;
    }
    _frameCycle = fc;
    auto lineCycle = _frameCycle % 14;
    if(_options.optTraceLog) {
        if (vsync)
            Logger::log(Logger::eBACKEND_EMU, _cpu.getCycles(), {_frameCounter, _frameCycle}, fmt::format("{:24} ; {}", "--- VSYNC ---", _cpu.dumpStateLine()).c_str());
        else if (lineCycle == 0)
            Logger::log(Logger::eBACKEND_EMU, _cpu.getCycles(), {_frameCounter, _frameCycle}, fmt::format("{:24} ; {}", "--- HSYNC ---", _cpu.dumpStateLine()).c_str());
    }
    if(_frameCycle > VIDEO_FIRST_INVISIBLE_LINE * 14 || _frameCycle < (VIDEO_FIRST_VISIBLE_LINE - 2) * 14)
        return _frameCycle;
    if(_frameCycle < VIDEO_FIRST_VISIBLE_LINE * 14 && _frameCycle >= (VIDEO_FIRST_VISIBLE_LINE - 2) * 14 + 2 && _cpu.getIE()) {
        _displayEnabledLatch = _displayEnabled;
        if(_displayEnabled) {
            if (_options.optTraceLog)
                Logger::log(Logger::eBACKEND_EMU, _cpu.getCycles(), {_frameCounter, _frameCycle}, fmt::format("{:24} ; {}", "--- IRQ ---", _cpu.dumpStateLine()).c_str());
            _cpu.triggerInterrupt();
        }
    }
    else if(_frameCycle >= VIDEO_FIRST_VISIBLE_LINE * 14 && _frameCycle < VIDEO_FIRST_INVISIBLE_LINE * 14) {
        auto line = _frameCycle / 14;
        if(lineCycle == 4 || lineCycle == 5) {
            auto dmaStart = _cpu.getR(0);
            auto highBits = 0;
            auto mask = _type == eVP590 && _subMode != eVP590_DEFAULT ? (_subMode == eVP590_HIRES ? 0x3FF : 0x3E7) : 0;
            if(_subMode == eVP590_DEFAULT)
                highBits = 7;
            for (int i = 0; i < 8; ++i) {
                auto [data, addr] = _displayEnabledLatch ? _cpu.executeDMAOut() : std::make_pair((uint8_t)0, (uint16_t)0);
                if(mask)
                    highBits = _cpu.readByteDMA(0xD000 | (addr & mask)) << 4;
                for (int j = 0; j < 8; ++j) {
                    _screen.setPixel(i * 8 + j, (line - VIDEO_FIRST_VISIBLE_LINE), highBits | ((data >> (7 - j)) & 1));
                }
            }
            if (_displayEnabledLatch) {
                if(_options.optTraceLog)
                    Logger::log(Logger::eBACKEND_EMU, _cpu.getCycles(), {_frameCounter, _frameCycle}, fmt::format("DMA: line {:03d} 0x{:04x}-0x{:04x}", line, dmaStart, _cpu.getR(0) - 1).c_str());
            }
        }
    }
    return (_cpu.getCycles() >> 3) % 3668;
}

void Cdp186x::incrementBackground()
{
    _backgroundColor = (_backgroundColor + 1) & 3;
    for(int i = 0; i < 256; i += 16) {
        _cdp1862Palette[i] = _cdp1862BackgroundColors[_backgroundColor];
    }
    _screen.setPalette(_cdp1862Palette);
}

}
