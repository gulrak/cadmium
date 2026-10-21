//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/hpsaturnl2.hpp
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
#pragma once

#include <emulation/hardware/genericcpu.hpp>
#include <emulation/hardware/hp48/saturndecoder.hpp>
#include <emulation/hardware/hp48/systembus.hpp>

#include <emulation/hardware/hp48/saturnreg64.hpp>
#include <emulation/hardware/hp48/saturnstate.hpp>
#include <cassert>
#include <functional>

#include "isaturn.hpp"

namespace hp48 {

class UnimplementedInstruction : public std::runtime_error {
public:
    explicit UnimplementedInstruction(const InstructionInfo* info)
        : std::runtime_error("unimplemented instruction"), _info(info) {}

    [[nodiscard]] const char* where() const {
        static std::string where;
        where = _info ? _info->uniqueIdentifier() : "unknown";
        return where.c_str();
    }

private:
    const InstructionInfo* _info;
};

class SaturnL2 : public emu::GenericCpu, public ISaturn {
public:
    using OutChangedCallback = std::function<void(std::uint16_t)>;

    enum StatusBit { ST_FORCED_WAKEUP = 0x1000, ST_IRQ_OCCURRED = 0x2000, ST_IRQ_PENDING = 0x4000, ST_IRQ_ENABLED = 0x8000 };
    enum HardwareStatusBit { HST_XM = 1, HST_SB = 2, HST_SR = 4, HST_MP = 8 };

    struct RuntimeState {
        std::array<std::uint16_t, 9> keyboardRows {};
        std::uint16_t in = 0;
        std::uint16_t out = 0;
        std::uint32_t pcOpcode = 0;
        std::uint64_t cycles = 0;
        bool shutdown = false;
        bool softInterrupt = false;
        bool interruptsEnabled = true;
        bool keyboardInterruptsEnabled = true;
        bool keyboardInterruptPending = false;
        bool shutdownWakePending = false;
        bool onKeyPressed = false;
    };

    SaturnL2(ISystemBus& bus);

    void handleReset(bool) override;

    uint8_t readNibble(uint32_t address) override
    {
        if (_fetchingOpcode) {
            _cycles += 1;
        } else {
            _cycles += address & 1 ? 2 : 1;
        }
        return _bus.readNibble(address);
    }

    void writeNibble(uint32_t address, uint8_t value) override
    {
        _cycles += address & 1 ? 2 : 1;
        _bus.writeNibble(address, value);
#if defined(HP48_ENABLE_INSTRUCTION_CACHE)
        invalidateInstructionCacheForWrite(address);
#endif
    }

    uint8_t readMemoryByte(uint32_t addr) const override
    {
        return (_bus.readNibble(addr * 2) & 0xF) | ((_bus.readNibble(addr * 2 + 1) & 0xF) << 4);
    }

    const std::vector<std::string>& registerNames() const override
    {
        static const std::vector<std::string> registerNames = {"A", "B", "C", "D", "R0", "R1", "R2", "R3", "R4", "IN", "OUT", "PC", "D0", "D1", "ST", "P", "H", "Carry"};
        return registerNames;
    }

    // Implement pure virtual methods from GenericCpu
    int executeInstruction() override;

    bool inErrorState() const override { return false; }

    uint32_t cpuID() const override { return 471588; }

    std::string name() const override
    {
        static constexpr std::string name = "SATURN-L2";
        return name;
    }

    size_t numRegisters() const override { return registerNames().size(); }

    RegisterValue registerbyIndex(size_t index) const override
    {
        if (index < 4)
            return {r[index]->v, 64};  // A, B, C, D
        if (index < 9)
            return {_rR[index - 4], 64};  // R0-R4
        switch (index) {
            case 9: return {_rIN, 16};
            case 10: return {_rOUT, 12};
            case 11: return {_rPC, 20};
            case 12: return {_rD0, 20};
            case 13: return {_rD1, 20};
            case 14: return {_rST, 16};
            case 15: return {_rP, 4};
            case 16: return {_rHST, 4};
            case 17: return {_rCarry ? 1u : 0u, 1};
            default: return {};
        }
    }

    void setRegister(size_t index, uint32_t value) override;

    uint32_t getPC() const override { return _rPC; }
    uint32_t getSP() const override { return 0; }  // Saturn doesn't have traditional SP
    uint16_t getIN() const { return _rIN; }
    uint16_t getOUT() const { return _rOUT; }
    void setOutChangedCallback(OutChangedCallback callback);
    void setIN(uint16_t value);
    void pressKey(uint8_t row, uint16_t inputMask);
    void releaseKey(uint8_t row, uint16_t inputMask);
    void pressOnKey();
    void releaseOnKey();
    void clearPendingInterrupts();
    bool servicePendingInterrupt();
    bool isShutdown() const { return _shutdn; }
    uint16_t* crcRegister() { return &_crc; }
    const uint64_t* cycleCounter() const { return &_cycles; }
    int64_t cycles() const override { return _cycles; }
    const emu::ClockedTime& time() const override { return _clockedTime; }
    unsigned stackSize() const override { return 8; }
    emu::GenericCpu::StackContent stack() const override;

    size_t disassemblyPrefixSize() const override { return 18; }
    std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const override;

    SaturnState getState() const override;
    void setState(const SaturnState& state) override;
    RuntimeState getRuntimeState() const;
    void setRuntimeState(const RuntimeState& state);

    void pushRSTK(uint32_t address);
    uint32_t popRSTK();

private:
    mutable std::unique_ptr<SaturnDecoder> _disassembler;
    using Handler = void (SaturnL2::*)(const DecodedInstruction&, const InstructionInfo&);
#define OP(name) void op_##name(const DecodedInstruction& decoded, const InstructionInfo& instr);
#include "hpsaturnl2op.hpp"
#undef OP
    void registerHandlers();
    void initExpressionist() override {}
    void updateCrc(uint8_t nibble) { _crc = (_crc >> 4) ^ (((_crc ^ nibble) & 0xF) * 0x1081); }
    void updateKeyboardInterrupt(uint16_t previousInput, bool resetPending);
    void scanKeyboard(bool active, bool resetPending);
    void requestSoftInterrupt();
    void setOutRegister(std::uint16_t value, bool notify);
#if defined(HP48_ENABLE_INSTRUCTION_CACHE)
    static constexpr std::uint32_t ADDRESS_SPACE_NIBBLES = 0x100000;
    static constexpr std::uint32_t ADDRESS_MASK = ADDRESS_SPACE_NIBBLES - 1;
    static constexpr std::uint32_t MAX_INSTRUCTION_NIBBLES = 24;
    static DecodedInstruction emptyInstructionCacheEntry()
    {
        return DecodedInstruction {nullptr, 0, {0, 0, 0}, 0};
    }
    void ensureInstructionCache();
    void clearInstructionCache();
    void invalidateInstructionCacheForWrite(std::uint32_t address);
    DecodedInstruction decodeCached(std::uint32_t& address);
#endif
    static int nibbleAccessCycles(uint32_t address) { return address & 1 ? 2 : 1; }
    static int nibbleAccessCycles(uint32_t address, int count)
    {
        int cycles = 0;
        for (int i = 0; i < count; ++i) {
            cycles += nibbleAccessCycles(address + i);
        }
        return cycles;
    }
    uint8_t readDataNibble(uint32_t address)
    {
        const uint8_t value = readNibble(address);
        if (_bus.readUpdatesCrc(address)) {
            updateCrc(value);
        }
        return value;
    }
    void goYes(uint32_t distance);
    void retYes();

    struct Pair { Reg64* src; Reg64* dst; };

    Pair decodeRegPair4(uint8_t pairId)
    {
        assert(pairId < 4);
        // {"a,b", "b,c", "c,a", "d,c"};
        static constexpr std::array<std::pair<uint8_t,uint8_t>, 4> lut{
        {
            {0,1}, // a,b
            {1,2}, // b,c
            {2,0}, // c,a
            {3,2} // d,c
        }};
        auto [si, di] = lut[pairId];
        return { r[si], r[di] };
    }
    Pair decodeRegPair8(uint8_t pairId) {
        assert(pairId < 8);
        // (srcIndex, dstIndex) for each pair_id, per your list
        static constexpr std::array<std::pair<uint8_t,uint8_t>, 8> lut{{
            {1,0}, // 0: b,a  => B -> A
            {2,1}, // 1: c,b  => C -> B
            {0,2}, // 2: a,c  => A -> C
            {2,3}, // 3: c,d  => C -> D
            {0,1}, // 8: a,b  => A -> B
            {1,2}, // 9: b,c  => B -> C
            {2,0}, // 10:c,a  => C -> A
            {3,2}, // 11:d,c  => D -> C
        }};
        auto [si, di] = lut[pairId];
        return { r[si], r[di] };
    }
    Pair decodeRegPair8Split(uint8_t pairId) {
        assert(pairId < 4 || (pairId > 7 && pairId < 12));
        // {"b,a", "c,b", "a,c", "c,d", {}, {}, {}, {}, "a,b", "b,c", "c,a", "d,c"}
        // (srcIndex, dstIndex) for each pair_id, per your list
        static constexpr std::array<std::pair<uint8_t,uint8_t>, 12> lut{{
            {1,0}, // 0: b,a  => B -> A
            {2,1}, // 1: c,b  => C -> B
            {0,2}, // 2: a,c  => A -> C
            {2,3}, // 3: c,d  => C -> D
            {0,0},
            {0,0},
            {0,0},
            {0,0},
            {0,1}, // 8: a,b  => A -> B
            {1,2}, // 9: b,c  => B -> C
            {2,0}, // 10:c,a  => C -> A
            {3,2}, // 11:d,c  => D -> C
        }};
        auto [si, di] = lut[pairId];
        return { r[si], r[di] };
    }
    Pair decodeRegPair12(uint8_t pairId) {
        assert(pairId < 12);
        // (srcIndex, dstIndex) for each pair_id, per your list
        static constexpr std::array<std::pair<uint8_t,uint8_t>, 12> lut{{
            {1,0}, // 0: b,a  => B -> A
            {2,1}, // 1: c,b  => C -> B
            {0,2}, // 2: a,c  => A -> C
            {2,3}, // 3: c,d  => C -> D
            {0,0}, // 4: a,a  => A -> A
            {1,1}, // 5: b,b  => B -> B
            {2,2}, // 6: c,c  => C -> C
            {3,3}, // 7: d,d  => D -> D
            {0,1}, // 8: a,b  => A -> B
            {1,2}, // 9: b,c  => B -> C
            {2,0}, // 10:c,a  => C -> A
            {3,2}, // 11:d,c  => D -> C
        }};
        auto [si, di] = lut[pairId];
        return { r[si], r[di] };
    }
    ISystemBus& _bus;
    Reg64 _rA{};
    Reg64 _rB{};
    Reg64 _rC{};
    Reg64 _rD{};
    std::array<Reg64*, 4> r{ &_rA, &_rB, &_rC, &_rD };
    std::array<uint64_t,5> _rR{};
    std::array<uint32_t,8> _rRSTK{};
    std::array<uint16_t,9> _keyboardRows{};
    uint16_t _rIN{};    // 16 bit; bit 15 is the ON key input line.
    uint16_t _rOUT{};   // 12 bit; bit 11 drives the beeper.
    uint32_t _rPC{};
    uint32_t _rPCOpcode{}; // The PC of the current opcode
    uint32_t _rD0{};
    uint32_t _rD1{};
    uint16_t _rST{};
    uint8_t _rP{};
    uint8_t _rHST{};
    uint8_t _rRSP{};
    NibbleMode _nibbleMode{NibbleMode::Hex};
    bool _rCarry{false};
    uint16_t _crc{0};
    mutable uint64_t _cycles{};
    bool _fetchingOpcode{false};
    bool _shutdn{false};
    bool _softInterrupt{false};
    bool _interruptsEnabled{true};
    bool _keyboardInterruptsEnabled{true};
    bool _keyboardInterruptPending{false};
    bool _shutdownWakePending{false};
    bool _onKeyPressed{false};
    emu::ClockedTime _clockedTime{2000000}; // Default 2MHz for HP48
    using HandlerFn = void(*)(SaturnL2&, const DecodedInstruction&, const InstructionInfo&);
    std::vector<Handler> _handlers;
    OutChangedCallback _outChangedCallback;
#if defined(HP48_ENABLE_INSTRUCTION_CACHE)
    std::vector<DecodedInstruction> _instructionCache;
#endif
};

}
