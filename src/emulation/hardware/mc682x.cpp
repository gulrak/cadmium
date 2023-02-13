//---------------------------------------------------------------------------------------
// src/emulation/mc682x.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2023, Steffen Schümann <s.schuemann@pobox.com>
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
#include <emulation/hardware/m6800.hpp>
//---
#include <emulation/hardware/mc682x.hpp>

#include <iostream>

namespace emu {

MC682x::MC682x()
{
}

void MC682x::reset()
{
    _portAIn = 0;
    _portAOut = 0;
    _ddrA = 0;
    _ctrlA = 0;
    _ca1In = false;
    _ca2In = false;
    _ca2Out = false;
    _irqA = false;

    _portBIn = 0;
    _portBOut = 0;
    _ddrB = 0;
    _ctrlB = 0;
    _cb1In = false;
    _cb2In = false;
    _cb2Out = false;
    _irqB = false;
}

uint8_t MC682x::readDebugByte(uint16_t addr) const
{
    uint8_t val = 0;
    switch(addr & 3) {
        case 0:
            if (_ctrlA & NDDR) {
                val = (_portAOut & _ddrA) | (_portAIn & ~_ddrA);
            }
            else {
                val = _ddrA;
            }
            break;
        case 1:
            val = _ctrlA;
            break;
        case 2:
            if (_ctrlB & NDDR) {
                val = (_portBOut & _ddrB) | (_portBIn & ~_ddrB);
            }
            else {
                val = _ddrB;
            }
            break;
        case 3:
            val = _ctrlB;
            break;
    }
    return val;
}

uint8_t MC682x::readByte(uint16_t addr) const
{
    uint8_t val = 0;
    auto* self = const_cast<MC682x*>(this);
    switch(addr & 3) {
        case 0:
            if(_ctrlA & NDDR) {
                if(portAInputHandler) {
                    auto [value, connections] = portAInputHandler(~_ddrA);
                    _portAIn = (value & connections) | ~connections; // pull-up unconnected lines
                }
                val = (_portAOut & _ddrA) | (_portAIn & ~_ddrA);
                _ctrlA = (_ctrlA & ~(IRQ1|IRQ2));
                updateIrq();
                if(isC2Output(_ctrlA) && isC2Strobe(_ctrlA)) {
                    _ca2Out = false;
                    if(_ca2Out && pinCA2OutputHandler)
                        pinCA2OutputHandler(false);
                    if(isC2StrobeEReset(_ctrlA)) {
                        _ca2Out = true;
                        pinCA2OutputHandler(true);
                    }
                }
            }
            else {
                val = _ddrA;
            }
            break;
        case 1:
            if(pinCA1InputHandler)
                self->pinCA1(pinCA1InputHandler());
            if(pinCA2InputHandler)
                self->pinCA2(pinCA2InputHandler());
            val = _ctrlA;
            break;
        case 2:
            if(_ctrlB & NDDR) {
                if(portBInputHandler)
                    _portBIn = portBInputHandler(~_ddrB);
                val = (_portBOut & _ddrB) | (_portBIn & ~_ddrB);
                _ctrlB = (_ctrlB & ~(IRQ1|IRQ2));
                updateIrq();
            }
            else {
                val = _ddrB;
            }
            break;
        case 3:
            if(pinCB1InputHandler)
                self->pinCB1(pinCB1InputHandler());
            if(pinCB2InputHandler)
                self->pinCB2(pinCB2InputHandler());
            val = _ctrlB;
            break;
    }
    //std::cout << fmt::format("Read from PIA[{}] = 0x{:02X}", addr & 3, unsigned(val)) << std::endl;
    return val;
}

void MC682x::writeByte(uint16_t addr, uint8_t val)
{
    //std::cout << fmt::format("Write to PIA[{}] = 0x{:02X}", addr & 3, unsigned(val)) << std::endl;
    switch(addr & 3) {
        case 0:
            if(_ctrlA & NDDR) {
                _portAOut = val;
                if(portAOutputHandler)
                    portAOutputHandler(_portAOut & _ddrA, _ddrA);
            }
            else {
                if(_ddrA != val) {
                    _ddrA = val;
                    if(portAOutputHandler)
                        portAOutputHandler(_portAOut & _ddrA, _ddrA);
                }
            }
            break;
        case 1:
            if(isC2Output(val) && isC2Set(val)) {
                if(_ca2Out != isC2Value(val)) {
                    _ca2Out = isC2Value(val);
                    if(pinCA2OutputHandler)
                        pinCA2OutputHandler(_ca2Out);
                }
            }
            _ctrlA = (_ctrlA & 0xC0) | (val & 0x3F);
            updateIrq();
            break;
        case 2:
            if(_ctrlB & NDDR) {
                _portBOut = val;
                if(portBOutputHandler)
                    portBOutputHandler(_portBOut & _ddrB, _ddrB);
                if(isC2Output(_ctrlB) && isC2Strobe(_ctrlB)) {
                    _cb2Out = false;
                    if(pinCB2OutputHandler)
                        pinCB2OutputHandler(false);
                    if(isC2StrobeEReset(_ctrlB)) {
                        _ca2Out = true;
                        if(pinCB2OutputHandler)
                            pinCB2OutputHandler(false);
                    }
                }
            }
            else {
                if(_ddrB != val) {
                    _ddrB = val;
                    if(portBOutputHandler)
                        portBOutputHandler(_portBOut & _ddrB, _ddrB);
                }
            }
            break;
        case 3:
            if(isC2Output(val) && isC2Set(val)) {
                if(_cb2Out != isC2Value(val)) {
                    _cb2Out = isC2Value(val);
                    if(pinCB2OutputHandler)
                        pinCB2OutputHandler(_cb2Out);
                }
            }
            _ctrlB = (_ctrlB & 0xC0) | (val & 0x3F);
            updateIrq();
            break;
    }
}

void MC682x::updateIrq() const
{
    auto irqA = ((_ctrlA & IRQ1) && isIrq1Enabled(_ctrlA)) || ((_ctrlA && IRQ2) && isIrq2Enabled(_ctrlA));
    if(_irqA != irqA) {
        _irqA = irqA;
        if(irqAOutputHandler)
            irqAOutputHandler(_irqA);
    }
    auto irqB = ((_ctrlB & IRQ1) && isIrq1Enabled(_ctrlB)) || ((_ctrlB && IRQ2) && isIrq2Enabled(_ctrlB));
    if(_irqB != irqB) {
        _irqB = irqB;
        if(irqBOutputHandler)
            irqBOutputHandler(_irqB);
    }
}

uint8_t MC682x::portA() const
{
    return _portAOut & _ddrA;
}

void MC682x::portA(uint8_t val)
{
    _portAIn = (_portAIn & _ddrA) | (val & ~_ddrA);
}

void MC682x::pinCA1(bool val)
{
    if(val != _ca1In && val == isC1LowHigh(_ctrlA)) {
        _ctrlA |= IRQ1;
        updateIrq();
        if(isC2Output(_ctrlA) && isC2Strobe(_ctrlA) && !isC2StrobeEReset(_ctrlA) && !_ca2Out) {
            _ca2Out = true;
            if (pinCA2OutputHandler)
                pinCA2OutputHandler(true);
        }
    }
    _ca1In = val;
}

bool MC682x::pinCA2() const
{
    return _ca2Out;
}

void MC682x::pinCA2(bool val)
{
    if(!isC2Output(_ctrlA) && val != _ca2In && val == isC2LowHigh(_ctrlA)) {
        _ctrlA |= IRQ2;
        updateIrq();
    }
    _ca2In = val;
}

uint8_t MC682x::portB() const
{
    return _portBOut & _ddrB;
}

void MC682x::portB(uint8_t val)
{
    _portBIn = (_portBIn & _ddrB) | (val & ~_ddrB);
}

void MC682x::pinCB1(bool val)
{
    if(val != _cb1In && val == isC2LowHigh(_ctrlB)) {
        _ctrlB |= IRQ1;
        updateIrq();
        if(isC2Output(_ctrlB) && isC2Strobe(_ctrlB) && !isC2StrobeEReset(_ctrlB) && !(_ctrlB & IRQ1)) {
            if(!_cb2Out && pinCB2OutputHandler) {
                _cb2Out = true;
                pinCB2OutputHandler(true);
            }
        }
    }
}

bool MC682x::pinCB2() const
{
    return _cb2Out;
}

void MC682x::pinCB2(bool val)
{
    if(!isC2Output(_ctrlB) && _cb2In != val && val == isC2LowHigh(_ctrlB)) {
        _ctrlB |= IRQ2;
        updateIrq();
    }
}

}  // namespace emu
