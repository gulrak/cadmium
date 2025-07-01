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

static Palette g_1861Palette{{ std::string("#000000"), std::string("#FFFFFF") }};
static Palette g_cdp1862Palette{{"#181818","#FF0000","#0000FF","#FF00FF","#00FF00","#FFFF00","#00FFFF","#FFFFFF"}, {"#000080","#000000","#008000","#800000"}};
Cdp186x::Cdp186x(Type type, Cdp1802& cpu, bool traceLog)
: _cpu(cpu)
, _type(type)
, VIDEO_FIRST_VISIBLE_LINE(80)
, VIDEO_FIRST_INVISIBLE_LINE(type == eCDP1864 ? VIDEO_FIRST_VISIBLE_LINE + 192 : VIDEO_FIRST_VISIBLE_LINE + 128)
, VIDEO_CYCLES_PER_FRAME(type == eCDP1864 ? 4368 : 3668)
, _traceLog(traceLog)
{
    _screen.setMode(256, 192, _type == eCDP1861_C10 ? 1 : 4); // actual resolution doesn't matter, just needs to be bigger than max resolution, but ratio matters
    reset();
}

void Cdp186x::reset()
{
    if(_type == eVP590) {
        _subMode = eVP590_DEFAULT;
        _screen.setPalette(g_cdp1862Palette);
        _backgroundColor = 0;
        _screen.setBackgroundPal(_backgroundColor);
    }
    else {
        _screen.setPalette(g_1861Palette);
    }
    _frameCounter = 0;
    _displayEnabledLatch = _displayEnabled = _type == eCDP1864;
    disableDisplay();
}

void Cdp186x::enableDisplay()
{
    if(_type != eCDP1864) {
        _displayEnabled = true;
    }
}

void Cdp186x::disableDisplay()
{
    if(_type != eCDP1864) {
        _screen.setAll(0);
        _displayEnabled = false;
    }
}

bool Cdp186x::getNEFX() const
{
    return ((_frameCycle >= (VIDEO_FIRST_VISIBLE_LINE - 4) * 14 && _frameCycle < VIDEO_FIRST_VISIBLE_LINE * 14) || (_frameCycle >= (VIDEO_FIRST_INVISIBLE_LINE - 4) * 14 && _frameCycle < VIDEO_FIRST_INVISIBLE_LINE * 14));
}

const Cdp186x::VideoType& Cdp186x::getScreen() const
{
    return _screen;
}

void Cdp186x::setPalette(const Palette& palette)
{
    _screen.setPalette(palette);
}

std::pair<int, bool> Cdp186x::executeStep()
{
    auto fc = static_cast<int>((_cpu.cycles() >> 3) % VIDEO_CYCLES_PER_FRAME);
    bool vsync = false;
    if (fc < _frameCycle) {
        vsync = true;
        ++_frameCounter;
    }
    _frameCycle = fc;
    auto lineCycle = _frameCycle % 14;
    if (_traceLog) {
        if (vsync)
            Logger::log(Logger::eBACKEND_EMU, _cpu.cycles(), {_frameCounter, _frameCycle}, fmt::format("{:24} ; {}", "--- VSYNC ---", _cpu.dumpStateLine()).c_str());
        else if (lineCycle == 0)
            Logger::log(Logger::eBACKEND_EMU, _cpu.cycles(), {_frameCounter, _frameCycle}, fmt::format("{:24} ; {}", "--- HSYNC ---", _cpu.dumpStateLine()).c_str());
    }
    if (_frameCycle > VIDEO_FIRST_INVISIBLE_LINE * 14 || _frameCycle < (VIDEO_FIRST_VISIBLE_LINE - 2) * 14)
        return {_frameCycle, vsync};
    if (_frameCycle < VIDEO_FIRST_VISIBLE_LINE * 14 && _frameCycle >= (VIDEO_FIRST_VISIBLE_LINE - 2) * 14 + 2 && _cpu.getIE()) {
        _displayEnabledLatch = _displayEnabled;
        if (_displayEnabledLatch) {
            if (_traceLog)
                Logger::log(Logger::eBACKEND_EMU, _cpu.cycles(), {_frameCounter, _frameCycle}, fmt::format("{:24} ; {}", "--- IRQ ---", _cpu.dumpStateLine()).c_str());
            _cpu.triggerInterrupt();
        }
    }
    else if (_frameCycle >= VIDEO_FIRST_VISIBLE_LINE * 14 && _frameCycle < VIDEO_FIRST_INVISIBLE_LINE * 14) {
        auto line = _frameCycle / 14;
        if (lineCycle == 4 || lineCycle == 5) {
            auto dmaStart = _cpu.getR(0);
            auto colorBits = 0;
            auto mask = _type == eVP590 && _subMode != eVP590_DEFAULT ? (_subMode == eVP590_HIRES ? 0xFF : 0xE7) : 0;
            if (_subMode == eVP590_DEFAULT)
                colorBits = 7;
            for (int i = 0; i < 8; ++i) {
                auto [data, addr] = _displayEnabledLatch ? _cpu.executeDMAOut() : std::make_pair((uint8_t)0, (uint16_t)0);
                if (mask)
                    colorBits = _cpu.readByteDMA(0xD000 | (addr & mask)) << 4;
                /*if (_traceLog) {
                    if (mask)
                        Logger::log(Logger::eBACKEND_EMU, _cpu.cycles(), {_frameCounter, _frameCycle}, fmt::format("{:04x}/{:04x} = {:02x}", addr, 0xD000 | (addr & mask), highBits).c_str());
                    else
                        Logger::log(Logger::eBACKEND_EMU, _cpu.cycles(), {_frameCounter, _frameCycle}, fmt::format("{:04x} = {:02x}", addr, data).c_str());
                }*/
                // std::cout << fmt::format("{:04x}/{:04x} = {:02x}", addr, 0xD000 | (addr & mask), highBits) << std::endl;
                auto y = (line - VIDEO_FIRST_VISIBLE_LINE);
                auto x = 0;
                if (_type == eVP590) {
                    for (int j = 0; j < 8; ++j) {
                        if (((data >> (7 - j)) & 1)) {
                            _screen.setPixel(x + i * 8 + j, y, 0x80 | colorBits);
                        }
                        else {
                            _screen.setPixel(x + i * 8 + j, y, 0);
                        }
                    }
                }
                else {
                    if (_type == eCDP1861_C10) {
                        if (y & 1)
                            x = 64;
                        y >>= 1;
                    }
                    for (int j = 0; j < 8; ++j) {
                        uint8_t pixel = (data >> (7 - j)) & 1;
                        _screen.setPixel(x + i * 8 + j, y, pixel);
                        if (_type == eCDP1861_C10 && y == 63) {
                            _screen.setPixel(x + i * 8 + j, 0, pixel);
                        }
                    }
                }
            }
            if (_displayEnabledLatch) {
                if (_traceLog)
                    Logger::log(Logger::eBACKEND_EMU, _cpu.cycles(), {_frameCounter, _frameCycle}, fmt::format("DMA: line {:03d} 0x{:04x}-0x{:04x}", line, dmaStart, _cpu.getR(0) - 1).c_str());
            }
        }
    }
    return {(_cpu.cycles() >> 3) % VIDEO_CYCLES_PER_FRAME, vsync};
}

void Cdp186x::setTrace(bool traceLog)
{
    _traceLog = traceLog;
}

void Cdp186x::incrementBackground()
{
    _backgroundColor = (_backgroundColor + 1) & 3;
    _screen.setBackgroundPal(_backgroundColor);
}

}
