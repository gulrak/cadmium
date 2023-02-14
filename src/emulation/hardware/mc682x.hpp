//---------------------------------------------------------------------------------------
// src/emulation/mc682x.hpp
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
#pragma once

#define M6800_STATE_BUS_ONLY
#include <emulation/hardware/m6800.hpp>

#include <functional>

namespace emu {

class MC682x : public M6800Bus<>
{
public:
    struct InputWithConnection { uint8_t value; uint8_t connections; };
    enum ControlBits { C1C0 = 1, C1C1 = 2, NDDR = 4, C2C0 = 8, C2C1 = 0x10, C2C2 = 0x20, IRQ2 = 0x40, IRQ1 = 0x80 };
    using PortInputHandler = std::function<uint8_t(uint8_t mask)>;
    using PortConnectedInputHandler = std::function<InputWithConnection(uint8_t mask)>;
    using PinInputHandler = std::function<bool()>;
    using PortOutputHandler = std::function<void(uint8_t data, uint8_t mask)>;
    using PinOutputHandler = std::function<void(bool)>;
    MC682x();
    ~MC682x() override = default;
    void reset();
    uint8_t readByte(uint16_t addr) const override;
    uint8_t readDebugByte(uint16_t addr) const override;
    void writeByte(uint16_t addr, uint8_t val) override;

    uint8_t portA() const;
    void portA(uint8_t val);
    void pinCA1(bool val);
    bool pinCA2() const;
    void pinCA2(bool val);
    uint8_t portB() const;
    void portB(uint8_t val);
    void pinCB1(bool val);
    bool pinCB2() const;
    void pinCB2(bool val);

    PortConnectedInputHandler portAInputHandler;
    PortOutputHandler portAOutputHandler;
    PinInputHandler pinCA1InputHandler;
    PinInputHandler pinCA2InputHandler;
    PinOutputHandler pinCA2OutputHandler;
    PinOutputHandler irqAOutputHandler;

    PortInputHandler portBInputHandler;
    PortOutputHandler portBOutputHandler;
    PinInputHandler pinCB1InputHandler;
    PinInputHandler pinCB2InputHandler;
    PinOutputHandler pinCB2OutputHandler;
    PinOutputHandler irqBOutputHandler;


private:
    static inline bool isC1LowHigh(uint8_t ctrl) { return !(ctrl & C1C1); }
    static inline bool isC2Output(uint8_t ctrl) { return ctrl & C2C2; }
    static inline bool isC2Strobe(uint8_t ctrl) { return !(ctrl & C2C1); }
    static inline bool isC2StrobeEReset(uint8_t ctrl) { return ctrl & C2C0; }
    static inline bool isC2LowHigh(uint8_t ctrl) { return !(ctrl & C2C1); }
    static inline bool isC2Set(uint8_t ctrl) { return (ctrl & C2C1); }
    static inline bool isC2Value(uint8_t ctrl) { return (ctrl & C2C0); }
    static inline bool isIrq1Enabled(uint8_t ctrl) { return ctrl & C1C0; }
    static inline bool isIrq2Enabled(uint8_t ctrl) { return ctrl & C2C0; }
    void updateIrq() const;
    mutable uint8_t _portAIn{0};
    uint8_t _portAOut{0};
    uint8_t _ddrA{0};
    mutable uint8_t _ctrlA{0};
    bool _ca1In{false};
    bool _ca2In{false};
    mutable bool _ca2Out{false};
    mutable bool _irqA{false};

    mutable uint8_t _portBIn{0};
    uint8_t _portBOut{0};
    uint8_t _ddrB{0};
    mutable uint8_t _ctrlB{0};
    bool _cb1In{false};
    bool _cb2In{false};
    bool _cb2Out{false};
    mutable bool _irqB{false};

    std::function<void()> triggerIRQA;
    std::function<void()> triggerIRQB;
};

}  // namespace emu

