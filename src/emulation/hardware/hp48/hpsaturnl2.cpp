//---------------------------------------------------------------------------------------
// src/emulation/hardware/hp48/hpsaturnl2.cpp
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

#include "hpsaturnl2.hpp"
#include "saturntrace.hpp"

#include <algorithm>
#include <iostream>
#include <utility>

using std::string_literals::operator""s;

namespace hp48 {

namespace {

// Debugger reads must not pass through SaturnL2::readNibble(), which
// charges execution cycles even while the calculator is paused.
class DisassemblyDecoder final : public SaturnDecoder {
public:
    explicit DisassemblyDecoder(ISystemBus& bus) : _bus(bus) {}

protected:
    uint8_t readNibble(uint32_t address) override
    {
        return _bus.readNibble(address & 0xfffffU) & 0xfU;
    }
    void writeNibble(uint32_t, uint8_t) override {}

private:
    ISystemBus& _bus;
};

} // namespace

SaturnL2::SaturnL2(ISystemBus& bus)
    : _bus(bus)
{
    registerHandlers();
}

void SaturnL2::handleReset(bool)
{
    _rPC = 0;
    _cycles = 0;
    _rCarry = false;
    _rP = 0;
    _rRSP = 0;
    _rST = 0;
    _rHST = 0;
    _rD0 = 0;
    _rD1 = 0;
    _rIN = 0;
    _rOUT = 0;
    _rA.v = _rB.v = _rC.v = _rD.v = 0;
    for (auto& r : _rR)
        r = 0;
    _rRSTK.fill(0);
    _keyboardRows.fill(0);
    _rPCOpcode = 0;
    _nibbleMode = NibbleMode::Hex;
    _crc = 0;
    _fetchingOpcode = false;
    _shutdn = false;
    _softInterrupt = false;
    _interruptsEnabled = true;
    _keyboardInterruptsEnabled = true;
    _keyboardInterruptPending = false;
    _shutdownWakePending = false;
    _onKeyPressed = false;
#if defined(HP48_ENABLE_INSTRUCTION_CACHE)
    clearInstructionCache();
#endif
}

void SaturnL2::setRegister(size_t index, uint32_t value)
{
    if (index < 4)
        (&_rA)[index].v = value;
    else if (index < 9)
        _rR[index - 4] = value;
    else if (index == 9)
        _rIN = value & 0xFFFF;
    else if (index == 10)
        setOutRegister(value, true);
    else if (index == 11)
        _rPC = value;
    else if (index == 12)
        _rD0 = value;
    else if (index == 13)
        _rD1 = value;
    else if (index == 14)
        _rST = value & 0xFFFF;
    else if (index == 15)
        _rP = value & 0x0F;
    else if (index == 16)
        _rHST = value & 0x0F;
    else if (index == 17)
        _rCarry = (value != 0);
}

void SaturnL2::setOutChangedCallback(OutChangedCallback callback)
{
    _outChangedCallback = std::move(callback);
}

void SaturnL2::setIN(uint16_t value)
{
    _rIN = value;
}

void SaturnL2::pressKey(uint8_t row, uint16_t inputMask)
{
    if ((inputMask & 0x8000U) != 0) {
        _onKeyPressed = true;
    } else if (row < _keyboardRows.size()) {
        _keyboardRows[row] |= inputMask;
    }
    scanKeyboard(false, false);
}

void SaturnL2::releaseKey(uint8_t row, uint16_t inputMask)
{
    if ((inputMask & 0x8000U) != 0) {
        _onKeyPressed = false;
    } else if (row < _keyboardRows.size()) {
        _keyboardRows[row] &= static_cast<uint16_t>(~inputMask);
    }
    scanKeyboard(false, false);
}

void SaturnL2::pressOnKey()
{
    pressKey(0, 0x8000U);
}

void SaturnL2::releaseOnKey()
{
    releaseKey(0, 0x8000U);
}

void SaturnL2::clearPendingInterrupts()
{
    _softInterrupt = false;
    _keyboardInterruptPending = false;
    _shutdownWakePending = false;
}

bool SaturnL2::servicePendingInterrupt()
{
    bool inputWakePending = _shutdownWakePending;
    const bool busInterruptPending = _bus.hasInterruptRequest();
    if (busInterruptPending) {
        requestSoftInterrupt();
    }

    if (_shutdn) {
        if (_softInterrupt) {
            _shutdownWakePending = true;
            inputWakePending = inputWakePending || !busInterruptPending;
        }
        if (_shutdownWakePending) {
            _shutdownWakePending = false;
            _shutdn = false;
            if (inputWakePending) {
                _bus.executeWakeFromShutdown();
            }
        } else {
            return false;
        }
    }

    if (!_softInterrupt) {
        return false;
    }

    _softInterrupt = false;
    if (!_interruptsEnabled) {
        return false;
    }

    _interruptsEnabled = false;
    pushRSTK(_rPC);
    _rPC = 0x0000f;
    return true;
}

void SaturnL2::requestSoftInterrupt()
{
    _softInterrupt = true;
}

void SaturnL2::updateKeyboardInterrupt(uint16_t previousInput, bool resetPending)
{
    const bool onKeyPressed = (_rIN & 0x8000U) != 0;
    bool keyboardInterrupt = (_rIN != 0 && (previousInput & 0x01ffU) == 0) || onKeyPressed || resetPending;

    _keyboardInterruptPending = _keyboardInterruptPending || (keyboardInterrupt && !_keyboardInterruptsEnabled);
    keyboardInterrupt = keyboardInterrupt && _keyboardInterruptsEnabled;
    keyboardInterrupt = keyboardInterrupt || onKeyPressed;
    keyboardInterrupt = keyboardInterrupt && _interruptsEnabled;

    if (_rIN != 0) {
        if (keyboardInterrupt) {
            requestSoftInterrupt();
        }
        if (_shutdn) {
            _shutdownWakePending = true;
        }
    } else {
        _keyboardInterruptPending = false;
    }
}

void SaturnL2::scanKeyboard(bool active, bool resetPending)
{
    if (!(active || _shutdn || _onKeyPressed || _keyboardInterruptsEnabled)) {
        return;
    }

    const uint16_t previousInput = _rIN;
    uint16_t input = 0;
    for (std::size_t row = 0; row < _keyboardRows.size(); ++row) {
        if ((_rOUT & (1U << row)) != 0) {
            input |= _keyboardRows[row];
        }
    }
    if (_onKeyPressed) {
        input |= 0x8000U;
    }
    _rIN = input;
    updateKeyboardInterrupt(previousInput, resetPending);
}

void SaturnL2::setOutRegister(std::uint16_t value, bool notify)
{
    static constexpr std::uint16_t OUT_MASK = 0x0fffU;
    static constexpr std::uint16_t BEEPER_MASK = 0x0800U;

    const std::uint16_t previous = _rOUT;
    _rOUT = value & OUT_MASK;
    if (notify && ((previous ^ _rOUT) & BEEPER_MASK) != 0 && _outChangedCallback) {
        _outChangedCallback(_rOUT);
    }
}

emu::GenericCpu::StackContent SaturnL2::stack() const
{
    // the 20 bit entries are stored as uint32_t in host byte order
    return {sizeof(uint32_t), emu::GenericCpu::eNATIVE, emu::GenericCpu::eUPWARDS, {reinterpret_cast<const uint8_t*>(_rRSTK.data()), _rRSTK.size() * sizeof(uint32_t)}};
}

std::string SaturnL2::disassembleInstructionWithBytes(int32_t pc, int* bytes) const
{
    if (!_disassembler)
        _disassembler = std::make_unique<DisassemblyDecoder>(_bus);
    const uint32_t address = (pc < 0 ? _rPC : static_cast<uint32_t>(pc)) & 0xfffffU;
    uint32_t nextAddress = address;
    const auto decoded = _disassembler->decode(nextAddress);
    // "bytes" counts address units: Saturn instructions are nibble-addressed.
    const uint32_t length = decoded.info ? std::max(1U, static_cast<uint32_t>(decoded.length)) : 1U;
    if (bytes)
        *bytes = static_cast<int>(length);
    std::string nibbles;
    nibbles.reserve(length);
    for (uint32_t i = 0; i < length; ++i)
        nibbles += "0123456789ABCDEF"[_bus.readNibble((address + i) & 0xfffffU) & 0xfU];
    // Keep room for operands in Cadmium's narrow debugger pane. A '+' marks
    // opcode nibbles continuing beyond the displayed column.
    if (nibbles.size() > 9) {
        nibbles.resize(8);
        nibbles += '+';
    }
    return fmt::format("{:05X}  {:<9}  {}", address, nibbles, formatAsapInstruction(decoded, address));
}

int SaturnL2::executeInstruction()
{
    _rPCOpcode = _rPC;
    _fetchingOpcode = true;
#if defined(HP48_ENABLE_INSTRUCTION_CACHE)
    auto decoded = decodeCached(_rPC);
#else
    auto decoded = decode(_rPC);
#endif
    _fetchingOpcode = false;

    if (decoded.info) {
        auto infos = SaturnDecoder::instructionInfos();
        size_t index = decoded.info - infos.data();
        if (index < _handlers.size() && _handlers[index]) {
            (this->*_handlers[index])(decoded, *decoded.info);
        }
    }

    return 0;  // Cycles are updated by readNibble, writeNibble or inside the handler
}

#if defined(HP48_ENABLE_INSTRUCTION_CACHE)
void SaturnL2::ensureInstructionCache()
{
    if (_instructionCache.empty()) {
        _instructionCache.assign(ADDRESS_SPACE_NIBBLES, emptyInstructionCacheEntry());
    }
}

void SaturnL2::clearInstructionCache()
{
    if (!_instructionCache.empty()) {
        std::fill(_instructionCache.begin(), _instructionCache.end(), emptyInstructionCacheEntry());
    }
}

void SaturnL2::invalidateInstructionCacheForWrite(std::uint32_t address)
{
    if (_instructionCache.empty()) {
        return;
    }

    const std::uint32_t maskedAddress = address & ADDRESS_MASK;
    const DecodedInstruction empty = emptyInstructionCacheEntry();
    for (std::uint32_t offset = 0; offset < MAX_INSTRUCTION_NIBBLES; ++offset) {
        _instructionCache[(maskedAddress - offset) & ADDRESS_MASK] = empty;
    }
}

DecodedInstruction SaturnL2::decodeCached(std::uint32_t& address)
{
    ensureInstructionCache();

    const std::uint32_t cacheAddress = address & ADDRESS_MASK;
    DecodedInstruction decoded = _instructionCache[cacheAddress];
    if (decoded.info != nullptr) {
        address += decoded.length;
        _cycles += decoded.length;
        return decoded;
    }

    decoded = decode(address);
    if (decoded.info != nullptr) {
        _instructionCache[cacheAddress] = decoded;
    }
    return decoded;
}
#endif

void SaturnL2::registerHandlers()
{
    _handlers.assign(instructionCount, nullptr);

    std::unordered_map<std::string_view, Handler> registry;
#define OP(name) registry[#name] = &SaturnL2::op_##name;
#include "hpsaturnl2op.hpp"
#undef OP

    for (size_t i = 0; i < instructionCount; ++i) {
        if (auto it = registry.find(instructions[i].uniqueIdentifier()); it != registry.end()) {
            _handlers[i] = it->second;
        }
    }
}

#undef REG

void SaturnL2::pushRSTK(uint32_t address)
{
    _rRSTK[_rRSP] = address;
    _rRSP = (_rRSP + 1) & 0x7;
}

uint32_t SaturnL2::popRSTK()
{
    _rRSP = (_rRSP - 1) & 0x7;
    const uint32_t address = _rRSTK[_rRSP];
    _rRSTK[_rRSP] = 0;
    return address;
}

void SaturnL2::goYes(uint32_t distance)
{
    if (_rCarry) {
        _rPC = _rPCOpcode + static_cast<int32_t>(distance);
        _cycles += 7;
    }
}

void SaturnL2::retYes()
{
    if (_rCarry) {
        _rPC = popRSTK() & 0xFFFFF;
        _cycles += 8;
    }
}

SaturnState SaturnL2::getState() const
{
    SaturnState s;
    s.a = _rA.v;
    s.b = _rB.v;
    s.c = _rC.v;
    s.d = _rD.v;
    s.r0 = _rR[0];
    s.r1 = _rR[1];
    s.r2 = _rR[2];
    s.r3 = _rR[3];
    s.r4 = _rR[4];
    s.d0 = _rD0;
    s.d1 = _rD1;
    s.pc = _rPC;
    s.p = _rP;
    s.carry = _rCarry;
    s.hexMode = (_nibbleMode == NibbleMode::Hex);
    s.pstat = _rST;
    s.xm = (_rHST & HST_XM) ? 1 : 0;
    s.sb = (_rHST & HST_SB) ? 1 : 0;
    s.sr = (_rHST & HST_SR) ? 1 : 0;
    s.mp = (_rHST & HST_MP) ? 1 : 0;
    s.crc = _crc;
    s.rstk.fill(0);
    for (size_t i = 0; i < 8; ++i) {
        auto val = _rRSTK[(_rRSP - i - 1) & 7] & 0xFFFFF;
        s.rstk[i] = val;
    }
    return s;
}

void SaturnL2::setState(const SaturnState& s)
{
    _rA.v = s.a;
    _rB.v = s.b;
    _rC.v = s.c;
    _rD.v = s.d;
    _rR[0] = s.r0;
    _rR[1] = s.r1;
    _rR[2] = s.r2;
    _rR[3] = s.r3;
    _rR[4] = s.r4;
    _rD0 = s.d0 & 0xFFFFF;
    _rD1 = s.d1 & 0xFFFFF;
    _rPC = s.pc & 0xFFFFF;
    _rP = s.p & 0xF;
    _rCarry = s.carry;
    _nibbleMode = s.hexMode ? NibbleMode::Hex : NibbleMode::BCD;
    _rST = s.pstat;
    _rHST = (s.xm ? HST_XM : 0) | (s.sb ? HST_SB : 0) | (s.sr ? HST_SR : 0) | (s.mp ? HST_MP : 0);
    _rRSTK.fill(0);
    _rRSP = 0;
    for (int i = 7; i >= 0 && !s.rstk[i]; --i, ++_rRSP) {
    }
    for (int i = 0; i < _rRSP; ++i) {
        _rRSTK[_rRSP - i - 1] = s.rstk[i];
    }
    _rRSP = _rRSP & 0x7;
#if defined(HP48_ENABLE_INSTRUCTION_CACHE)
    clearInstructionCache();
#endif
}

SaturnL2::RuntimeState SaturnL2::getRuntimeState() const
{
    return RuntimeState {
        .keyboardRows = _keyboardRows,
        .in = _rIN,
        .out = _rOUT,
        .pcOpcode = _rPCOpcode,
        .cycles = _cycles,
        .shutdown = _shutdn,
        .softInterrupt = _softInterrupt,
        .interruptsEnabled = _interruptsEnabled,
        .keyboardInterruptsEnabled = _keyboardInterruptsEnabled,
        .keyboardInterruptPending = _keyboardInterruptPending,
        .shutdownWakePending = _shutdownWakePending,
        .onKeyPressed = _onKeyPressed,
    };
}

void SaturnL2::setRuntimeState(const RuntimeState& state)
{
    _keyboardRows = state.keyboardRows;
    _rIN = state.in;
    setOutRegister(state.out, false);
    _rPCOpcode = state.pcOpcode & 0xfffffU;
    _cycles = state.cycles;
    _fetchingOpcode = false;
    _shutdn = state.shutdown;
    _softInterrupt = state.softInterrupt;
    _interruptsEnabled = state.interruptsEnabled;
    _keyboardInterruptsEnabled = state.keyboardInterruptsEnabled;
    _keyboardInterruptPending = state.keyboardInterruptPending;
    _shutdownWakePending = state.shutdownWakePending;
    _onKeyPressed = state.onKeyPressed;
}

void SaturnL2::op_add_a_p_1_c_809(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // C+P+1 / C=C+P+1: increment A-field of C by (P+1), HEX mode, adjusts Carry
    // CHIP-48 used instruction
    const uint32_t imm = uint32_t(_rP) + 1u;
    const Span spA = fieldSpan(Field::A);
    _rCarry = _rC.addConst(spA, imm, NibbleMode::Hex);
    _cycles += 5;
}

void SaturnL2::op_add_a_L_cL(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // A=A+B, B=B+C, C=C+A, D=D+C, B=B+A, C=C+B, A=A+C, C=C+D
    // CHIP-48 used instruction
    auto pair = decodeRegPair12(decoded.operands[0]);
    const Span sp = fieldSpan(Field::A, _rP);
    _rCarry = (pair.dst)->addField(*pair.src, sp, false, _nibbleMode);
    _cycles += 5;
}

void SaturnL2::op_add_G_L_aGL(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair12(decoded.operands[1]);
    const Span sp = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rCarry = (pair.dst)->addField(*pair.src, sp, false, _nibbleMode);
    _cycles += sp.size();
}

void SaturnL2::op_add_a_S_M_818fMS(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto reg = r[decoded.operands[0]];
    const Span sp = fieldSpan(Field::A, _rP);
    _rCarry = reg->addConst(sp, decoded.operands[1], NibbleMode::Hex);
    _cycles += 4;
}

void SaturnL2::op_add_G_S_M_818GMS(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto field = static_cast<Field>(decoded.operands[0]);
    auto reg = r[decoded.operands[1]];
    const Span sp = fieldSpan(field, _rP);
    uint32_t constant = decoded.operands[2];

    // Saturn hardware bug: for single-nibble fields (P, XS, S), carry from adding
    // a constant propagates through the full register with 17-step wrap-around (Ninc16).
    int nF_l = (sp.size() == 1) ? 17 : sp.size();

    uint32_t v = reg->getNibble(sp.lo) + constant;
    reg->setNibble(sp.lo, uint8_t(v & 0xF));
    bool carry = (v >= 16);

    if (carry) {
        for (int idx = 1; idx < nF_l; ++idx) {
            int ni = (sp.lo + idx) & 0xF;
            uint8_t n = reg->getNibble(ni) + 1;
            if (n < 16) {
                reg->setNibble(ni, n);
                carry = false;
                break;
            }
            reg->setNibble(ni, 0);
        }
    }

    _rCarry = carry;
    _cycles += sp.hi - sp.lo;
}

void SaturnL2::op_add_a_S_d0_16S(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rD0 += decoded.operands[0];
    if (_rD0 > 0xFFFFF) {
        _rD0 &= 0xFFFFF;
        _rCarry = true;
    }
    else {
        _rCarry = false;
    }
    _cycles += 4;
}

void SaturnL2::op_add_a_S_d1_17S(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rD1 += decoded.operands[0];
    if (_rD1 > 0xFFFFF) {
        _rD1 &= 0xFFFFF;
        _rCarry = true;
    }
    else {
        _rCarry = false;
    }
    _cycles += 4;
}

void SaturnL2::op_and_a_J_0efJ(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair8(decoded.operands[0]);
    pair.dst->andField(*pair.src, fieldSpan(Field::A, 0));
    _cycles += 5;
}

void SaturnL2::op_and_G_J_0eGJ(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair8(decoded.operands[1]);
    pair.dst->andField(*pair.src, span);
    _cycles += span.size();
}

void SaturnL2::op_nop3_420(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // 3 nibble nop
    _cycles += 5;
}

void SaturnL2::op_nop4_6300(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // 4 nibble nop
    _cycles += 7;
}

void SaturnL2::op_nop5_64000(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // 5 nibble nop
    _cycles += 6;
}

void SaturnL2::op_brbc_R_a_U_8086RUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rCarry = _rA.isBitClear(decoded.operands[0]);
    _cycles += 2;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brbc_R_c_U_808aRUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rCarry = _rC.isBitClear(decoded.operands[0]);
    _cycles += 2;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brbc_R_st_U_86RUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = (_rST & (1 << decoded.operands[0])) == 0;
    _cycles += 2;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brbc_V_U_83VUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = (_rHST & decoded.operands[0]) == 0;
    _cycles += 1;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brbs_R_a_U_8087RUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rCarry = _rA.isBitSet(decoded.operands[0]);
    _cycles += 2;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brbs_R_c_U_808bRUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = _rC.isBitSet(decoded.operands[0]);
    _cycles += 2;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brbs_R_st_U_87RUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = (_rST & (1 << decoded.operands[0])) != 0;
    _cycles += 2;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_retbc_R_a_8086R00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = _rA.isBitClear(decoded.operands[0]);
    _cycles += _rCarry ? 1 : 2;
    retYes();
}

void SaturnL2::op_retbc_R_c_808aR00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = _rC.isBitClear(decoded.operands[0]);
    _cycles += _rCarry ? 1 : 2;
    retYes();
}

void SaturnL2::op_retbc_R_st_86R00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = (_rST & (1 << decoded.operands[0])) == 0;
    _cycles += 1;
    retYes();
}

void SaturnL2::op_retbc_V_83V00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = (_rHST & decoded.operands[0]) == 0;
    _cycles += 2;
    retYes();
}

void SaturnL2::op_retbs_R_a_8087R00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rCarry = _rA.isBitSet(decoded.operands[0]);
    _cycles += _rCarry ? 1 : 2;
    retYes();
}

void SaturnL2::op_retbs_R_c_808bR00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = _rC.isBitSet(decoded.operands[0]);
    _cycles += _rCarry ? 1 : 2;
    retYes();
}

void SaturnL2::op_retbs_R_st_87R00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = (_rST & (1 << decoded.operands[0])) != 0;
    _cycles += 1;
    retYes();
}

void SaturnL2::op_brcc_U_5UU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    if (!_rCarry) {
        _rPC = _rPCOpcode + static_cast<int32_t>(decoded.operands[0]);
        _cycles += 7;
    }
}

void SaturnL2::op_brcs_U_4UU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _cycles += 0;
    goYes(decoded.operands[0]);
}

void SaturnL2::op_retcc_500(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    if (!_rCarry) {
        _rPC = popRSTK() & 0xFFFFF;
        _cycles += 7;
    }
}

void SaturnL2::op_retcs_400(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    if (_rCarry) {
        _rPC = popRSTK() & 0xFFFFF;
        _cycles += 7;
    }
}

void SaturnL2::op_breq_1_p_R_U_89RUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = _rP == decoded.operands[0];
    _cycles += 1;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brne_1_p_R_U_88RUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = _rP != decoded.operands[0];
    _cycles += 1;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_reteq_1_p_R_89R00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = _rP == decoded.operands[0];
    retYes();
}

void SaturnL2::op_retne_1_p_R_88R00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = _rP != decoded.operands[0];
    retYes();
}

void SaturnL2::op_breq_a_H_U_8aHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) == 0;
    _cycles += 6;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_breq_G_H_U_9GHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair4(decoded.operands[1]);
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rCarry = pair.src->cmpField(*pair.dst, span) == 0;
    _cycles += span.size() + 1;
    goYes(decoded.operands[2]);
}

void SaturnL2::op_brne_a_H_U_8aHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) != 0;
    _cycles += 6;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brne_G_H_U_9GHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) != 0;
    _cycles += span.size() + 1;
    goYes(decoded.operands[2]);
}

void SaturnL2::op_brz_a_M_U_8aMUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rCarry = r[decoded.operands[0]]->cmpField(Reg64{0}, fieldSpan(Field::A, 0)) == 0;
    _cycles += 6;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brz_G_M_U_9GMUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rCarry = r[decoded.operands[1]]->cmpField(Reg64{0}, span) == 0;
    _cycles += span.size() + 1;
    goYes(decoded.operands[2]);
}

void SaturnL2::op_brnz_a_M_U_8aMUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = r[decoded.operands[0]]->cmpField(Reg64{0}, fieldSpan(Field::A, 0)) != 0;
    _cycles += 6;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brnz_G_M_U_9GMUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rCarry = r[decoded.operands[1]]->cmpField(Reg64{0}, span) != 0;
    _cycles += span.size() + 1;
    goYes(decoded.operands[2]);
}

void SaturnL2::op_brgt_a_H_U_8bHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) > 0;
    _cycles += 6;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brgt_G_H_U_9GHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) > 0;
    _cycles += span.size() + 1;
    goYes(decoded.operands[2]);
}

void SaturnL2::op_brlt_a_H_U_8bHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) < 0;
    _cycles += 6;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brlt_G_H_U_9GHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) < 0;
    _cycles += span.size() + 1;
    goYes(decoded.operands[2]);
}

void SaturnL2::op_brge_a_H_U_8bHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) >= 0;
    _cycles += 6;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brge_G_H_U_9GHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) >= 0;
    _cycles += span.size() + 1;
    goYes(decoded.operands[2]);
}

void SaturnL2::op_brle_a_H_U_8bHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) <= 0;
    _cycles += 6;
    goYes(decoded.operands[1]);
}

void SaturnL2::op_brle_G_H_U_9GHUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) <= 0;
    _cycles += span.size() + 1;
    goYes(decoded.operands[2]);
}

void SaturnL2::op_reteq_a_H_8aH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) == 0;
    _cycles += 5 + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_reteq_G_H_9GH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) == 0;
    _cycles += span.size() + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retne_a_H_8aH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) != 0;
    _cycles += 5 + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retne_G_H_9GH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) != 0;
    _cycles += span.size() + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retz_a_M_8aM00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rCarry = r[decoded.operands[0]]->cmpField(Reg64{0}, fieldSpan(Field::A, 0)) == 0;
    _cycles += 5 + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retz_G_M_9GM00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rCarry = r[decoded.operands[1]]->cmpField(Reg64{0}, span) == 0;
    _cycles += span.size() + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retnz_a_M_8aM00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = r[decoded.operands[0]]->cmpField(Reg64{0}, fieldSpan(Field::A, 0)) != 0;
    _cycles += 5 + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retnz_G_M_9GM00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rCarry = r[decoded.operands[1]]->cmpField(Reg64{0}, span) != 0;
    _cycles += span.size() + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retgt_a_H_8bH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) > 0;
    _cycles += 5 + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retgt_G_H_9GH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) > 0;
    _cycles += span.size() + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retlt_a_H_8bH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) < 0;
    _cycles += 5 + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retlt_G_H_9GH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) < 0;
    _cycles += span.size() + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retge_a_H_8bH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) >= 0;
    _cycles += 5 + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retge_G_H_9GH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) >= 0;
    _cycles += span.size() + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retle_a_H_8bH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto pair = decodeRegPair4(decoded.operands[0]);
    _rCarry = pair.src->cmpField(*pair.dst, fieldSpan(Field::A, 0)) <= 0;
    _cycles += 5 + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_retle_G_H_9GH00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    _rCarry = pair.src->cmpField(*pair.dst, span) <= 0;
    _cycles += span.size() + (_rCarry ? 0 : 1);
    retYes();
}

void SaturnL2::op_buscb_8083(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // Used for HP48GII and HP49 extensions, so it's NOP for now
    _cycles += 4;
}

void SaturnL2::op_buscc_80b(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // Used for HP48GII and HP49 extensions, so it's NOP for now
    _cycles += 4;
}

void SaturnL2::op_buscd_808d(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // Used for HP48GII and HP49 extensions, so it's NOP for now
    _cycles += 4;
}

void SaturnL2::op_uncnfg_804(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _bus.executeUnconfigOpcode(_rC.v & 0xFFFFF);
#if defined(HP48_ENABLE_INSTRUCTION_CACHE)
    clearInstructionCache();
#endif
    _cycles += 9;
}

void SaturnL2::op_config_805(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _bus.executeConfigOpcode(_rC.v & 0xFFFFF);
#if defined(HP48_ENABLE_INSTRUCTION_CACHE)
    clearInstructionCache();
#endif
    _cycles += 8;
}

void SaturnL2::op_shutdn_807(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    scanKeyboard(true, false);
    const bool shutdownAllowed = _bus.executeShutdownOpcode();
    if ((_rIN & 0x7fffU) == 0 && shutdownAllowed) {
        _shutdn = true;
    }
    _cycles += 3;
}

void SaturnL2::op_reset_80a(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _bus.executeResetOpcode();
#if defined(HP48_ENABLE_INSTRUCTION_CACHE)
    clearInstructionCache();
#endif
    _cycles += 3;
}

void SaturnL2::op_sreq_80e(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rC.setNibble(0, 0);
    _cycles += 4;
}

void SaturnL2::op_call_3_U_7UUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto dest = static_cast<int32_t>(decoded.operands[0]);
    pushRSTK(_rPC);
    _rPC = (_rPCOpcode + dest) & 0xFFFFF;
    _cycles += 8;
}

void SaturnL2::op_call_4_U_8eUUUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto dest = static_cast<int32_t>(decoded.operands[0]);
    pushRSTK(_rPC);
    _rPC = (_rPCOpcode + dest) & 0xFFFFF;
    _cycles += 8;
}

void SaturnL2::op_call_a_R_8fRRRRR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto dest = decoded.operands[0];
    pushRSTK(_rPC);
    _rPC = dest;
    _cycles += 8;
}

void SaturnL2::op_clr_a_M_dM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    r[decoded.operands[0]]->clearField(Field::A, 0);
    _cycles += 5;
}

void SaturnL2::op_clr_G_M_aGM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    r[decoded.operands[1]]->clearField(static_cast<Field>(decoded.operands[0]), _rP);
    _cycles += fieldSpan(static_cast<Field>(decoded.operands[0]), _rP).size();
}

void SaturnL2::op_clrb_R_a_8084R(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rA.clearBit(decoded.operands[0]);
    _cycles += 1;
}

void SaturnL2::op_clrb_R_c_8088R(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rC.clearBit(decoded.operands[0]);
    _cycles += 1;
}

void SaturnL2::op_clrb_R_st_84R(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rST &= ~(1 << decoded.operands[0]);
    _cycles += 1;
}

void SaturnL2::op_clrb_V_82V(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rHST &= ~decoded.operands[0];
}

void SaturnL2::op_clr_x_st_08(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rST &= 0xF000;
    _cycles += 4;
}

void SaturnL2::op_dec_1_p_0d(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = _rP == 0;
    _rP = (_rP - 1) & 0x0f;
    _cycles += 1;
}

void SaturnL2::op_dec_a_M_cM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto sp = fieldSpan(Field::A, 0);
    _rCarry = r[decoded.operands[0]]->subField(Reg64{0}, sp, 1, _nibbleMode) != 0;
    _cycles += 5;
}

void SaturnL2::op_dec_G_M_aGM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rCarry = r[decoded.operands[1]]->subField(Reg64{0}, span, 1, _nibbleMode) != 0;
    _cycles += span.size();
}

void SaturnL2::op_in_4_a_802(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    scanKeyboard(true, false);
    _rA.v = (_rA.v & ~0xffffULL) | (_rIN & 0xffffU);
    _cycles += 4;
}

void SaturnL2::op_in_4_c_803(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    scanKeyboard(true, false);
    _rC.v = (_rC.v & ~0xffffULL) | (_rIN & 0xffffU);
    _cycles += 4;
}

void SaturnL2::op_inc_1_p_0c(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = _rP == 0x0f;
    _rP = (_rP + 1) & 0x0f;
    _cycles += 1;
}

void SaturnL2::op_inc_a_M_eM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto sp = fieldSpan(Field::A, 0);
    _rCarry = r[decoded.operands[0]]->addField(Reg64{0}, sp, 1, _nibbleMode) != 0;
    _cycles += 5;
}

void SaturnL2::op_inc_G_M_bGM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rCarry = r[decoded.operands[1]]->addField(Reg64{0}, span, 1, _nibbleMode) != 0;
    _cycles += span.size();
}

void SaturnL2::op_intoff_808f(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _keyboardInterruptsEnabled = false;
    _cycles += 1;
}

void SaturnL2::op_inton_8080(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _keyboardInterruptsEnabled = true;
    scanKeyboard(false, false);
    if (_keyboardInterruptPending || (_rIN & 0x8000U) != 0) {
        _keyboardInterruptPending = false;
        requestSoftInterrupt();
    }
    _cycles += 1;
}

void SaturnL2::op_rsi_80810(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    scanKeyboard(true, true);
    if (_rIN != 0 && !_interruptsEnabled) {
        _keyboardInterruptPending = true;
    }
    _cycles += 1;
}

void SaturnL2::op_jump_a_a_808c(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    uint32_t addr = _rA.v & 0xFFFFF;
    const int dataCycles = nibbleAccessCycles(addr, 5);
    uint32_t target = 0;
    for (int i = 0; i < 5; ++i)
        target |= static_cast<uint32_t>(readDataNibble(addr + i)) << (i * 4);
    _rPC = target & 0xFFFFF;
    _cycles += 23 - 4 - dataCycles;
}

void SaturnL2::op_jump_a_c_808e(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    uint32_t addr = _rC.v & 0xFFFFF;
    const int dataCycles = nibbleAccessCycles(addr, 5);
    uint32_t target = 0;
    for (int i = 0; i < 5; ++i) {
        target |= static_cast<uint32_t>(readDataNibble(addr + i)) << (i * 4);
    }
    _rPC = target & 0xFFFFF;
    _cycles += 23 - 4 - dataCycles;
}

void SaturnL2::op_jump_a_a_81b2(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rPC = _rA.v & 0xFFFFF;
    _cycles += 12;
}

void SaturnL2::op_jump_a_c_81b3(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rPC = _rC.v & 0xFFFFF;
    _cycles += 12;
}

void SaturnL2::op_move_a_pc_a_81b4(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rA.v = (_rA.v & ~0xFFFFFULL) | (_rPC & 0xFFFFF);
    _cycles += 9;
}

void SaturnL2::op_move_a_pc_c_81b5(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rC.v = (_rC.v & ~0xFFFFFULL) | (_rPC & 0xFFFFF);
    _cycles += 9;
}

void SaturnL2::op_swap_a_a_pc_81b6(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    const std::uint32_t target = _rA.v & 0xFFFFF;
    _rA.v = (_rA.v & ~0xFFFFFULL) | (_rPC & 0xFFFFF);
    _rPC = target;
    _cycles += 12;
}

void SaturnL2::op_swap_a_c_pc_81b7(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    const std::uint32_t target = _rC.v & 0xFFFFF;
    _rC.v = (_rC.v & ~0xFFFFFULL) | (_rPC & 0xFFFFF);
    _rPC = target;
    _cycles += 12;
}

void SaturnL2::op_jump_3_U_6UUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto dest = static_cast<int32_t>(decoded.operands[0]);
    _rPC = (_rPCOpcode + dest) & 0xFFFFF;
    _cycles += 7;
}

void SaturnL2::op_jump_4_U_8cUUUU(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto dest = static_cast<int32_t>(decoded.operands[0]);
    _rPC = (_rPCOpcode + dest) & 0xFFFFF;
    _cycles += 8;
}

void SaturnL2::op_jump_a_R_8dRRRRR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rPC = decoded.operands[0] & 0xFFFFF;
    _cycles += 7;
}

void SaturnL2::op_move_a_id_c_806(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _bus.executeCIdOpcode(_rC.v);
    _cycles += 8;
}

void SaturnL2::op_move_a_J_dJ(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair8(decoded.operands[0]);
    pair.dst->copyField(*pair.src, fieldSpan(Field::A, 0));
    _cycles += 5;
}

void SaturnL2::op_move_G_J_aGJ(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair8(decoded.operands[1]);
    pair.dst->copyField(*pair.src, span);
    _cycles += span.size();
}

void SaturnL2::op_swap_a_I_dI(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair4(decoded.operands[0]);
    auto sp = fieldSpan(Field::A, 0);
    Reg64 temp;
    temp.copyField(*pair.src, sp);
    pair.src->copyField(*pair.dst, sp);
    pair.dst->copyField(temp, sp);
    _cycles += 5;
}

void SaturnL2::op_swap_G_I_aGI(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair4(decoded.operands[1]);
    Reg64 temp;
    temp.copyField(*pair.src, span);
    pair.src->copyField(*pair.dst, span);
    pair.dst->copyField(temp, span);
    _cycles += span.size();
}

void SaturnL2::op_move_a_N_14N(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto* reg = (decoded.operands[0] & 4) ? &_rC : &_rA;
    auto& ptr = (decoded.operands[0] & 1) ? _rD1 : _rD0;
    const Span sp = fieldSpan(Field::A, 0);
    const int dataCycles = nibbleAccessCycles(ptr + sp.lo, sp.size());
    if (decoded.operands[0] & 2) {
        _cycles += 18 - 3 - dataCycles;
        for (int i = sp.lo; i <= sp.hi; ++i)
            reg->setNibble(i, readDataNibble(ptr + i));
    } else {
        _cycles += 17 - 3 - dataCycles;
        for (int i = sp.lo; i <= sp.hi; ++i)
            writeNibble(ptr + i, reg->getNibble(i));
    }
}

void SaturnL2::op_move_b_N_14N(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto* reg = (decoded.operands[0] & 4) ? &_rC : &_rA;
    auto& ptr = (decoded.operands[0] & 1) ? _rD1 : _rD0;
    const Span sp = fieldSpan(Field::B, 0);
    const int dataCycles = nibbleAccessCycles(ptr + sp.lo, sp.size());
    if (decoded.operands[0] & 2) {
        _cycles += 15 - 3 - dataCycles;
        for (int i = sp.lo; i <= sp.hi; ++i)
            reg->setNibble(i, readDataNibble(ptr + i));
    } else {
        _cycles += 14 - 3 - dataCycles;
        for (int i = sp.lo; i <= sp.hi; ++i)
            writeNibble(ptr + i, reg->getNibble(i));
    }
}

void SaturnL2::op_move_G_N_15NG(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto* reg = (decoded.operands[0] & 4) ? &_rC : &_rA;
    auto& ptr = (decoded.operands[0] & 1) ? _rD1 : _rD0;
    const Span sp = fieldSpan(static_cast<Field>(decoded.operands[1]), _rP);
    const int dataCycles = nibbleAccessCycles(ptr, sp.size());
    if (decoded.operands[0] & 2) {
        _cycles += 17 + sp.size() - 4 - dataCycles;
        for (int i = sp.lo; i <= sp.hi; ++i)
            reg->setNibble(i, readDataNibble(ptr + (i - sp.lo)));
    } else {
        _cycles += 16 + sp.size() - 4 - dataCycles;
        for (int i = sp.lo; i <= sp.hi; ++i)
            writeNibble(ptr + (i - sp.lo), reg->getNibble(i));
    }
}

void SaturnL2::op_move_S_N_15NS(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto* reg = (decoded.operands[0] & 4) ? &_rC : &_rA;
    auto& ptr = (decoded.operands[0] & 1) ? _rD1 : _rD0;
    auto count = static_cast<int>(decoded.operands[1]);
    const int dataCycles = nibbleAccessCycles(ptr, count);
    if (decoded.operands[0] & 2) {
        _cycles += 17 + count - 4 - dataCycles;
        for (int i = 0; i < count; ++i)
            reg->setNibble(i, readDataNibble(ptr + i));
    } else {
        _cycles += 16 + count - 4 - dataCycles;
        for (int i = 0; i < count; ++i)
            writeNibble(ptr + i, reg->getNibble(i));
    }
}

void SaturnL2::op_move_a_O_13O(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto* reg = (decoded.operands[0] & 4) ? &_rC : &_rA;
    auto& ptr = (decoded.operands[0] & 1) ? _rD1 : _rD0;
    ptr = reg->v & 0xFFFFF;
    _cycles += 5;
}

void SaturnL2::op_move_4_O_13O(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto* reg = (decoded.operands[0] & 4) ? &_rC : &_rA;
    auto& ptr = (decoded.operands[0] & 1) ? _rD1 : _rD0;
    ptr = (ptr & 0xF0000U) | (reg->v & 0x0FFFFU);
    _cycles += 5;
}

void SaturnL2::op_swap_a_O_13O(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto* reg = (decoded.operands[0] & 4) ? &_rC : &_rA;
    auto& ptr = (decoded.operands[0] & 1) ? _rD1 : _rD0;
    auto old_ptr = ptr;
    ptr = reg->v & 0xFFFFF;
    reg->v = (reg->v & ~0xFFFFFULL) | old_ptr;
    _cycles += 5;
}

void SaturnL2::op_swap_4_O_13O(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto* reg = (decoded.operands[0] & 4) ? &_rC : &_rA;
    auto& ptr = (decoded.operands[0] & 1) ? _rD1 : _rD0;
    const std::uint32_t oldPtr = ptr;
    ptr = (ptr & 0xF0000U) | (reg->v & 0x0FFFFU);
    reg->v = (reg->v & ~0x0FFFFULL) | (oldPtr & 0x0FFFFU);
    _cycles += 5;
}

void SaturnL2::op_move_w_a_Q_10Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rR[decoded.operands[0]] = _rA.v;
    _cycles += 16;
}

void SaturnL2::op_swap_w_a_Q_12Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    const auto temp = _rR[decoded.operands[0]];
    _rR[decoded.operands[0]] = _rA.v;
    _rA.v = temp;
    _cycles += 16;
}

void SaturnL2::op_move_w_c_Q_10Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rR[decoded.operands[0]] = _rC.v;
    _cycles += 16;
}

void SaturnL2::op_swap_w_c_Q_12Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    const auto temp = _rR[decoded.operands[0]];
    _rR[decoded.operands[0]] = _rC.v;
    _rC.v = temp;
    _cycles += 16;
}

void SaturnL2::op_move_w_Q_a_11Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rA.v = _rR[decoded.operands[0]];
    _cycles += 16;
}

void SaturnL2::op_move_w_Q_c_11Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rC.v = _rR[decoded.operands[0]];
    _cycles += 16;
}

void SaturnL2::op_move_a_a_Q_81af0Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto sp = fieldSpan(Field::A, 0);
    Reg64 rn{_rR[decoded.operands[0]]};
    rn.copyField(_rA, sp);
    _rR[decoded.operands[0]] = rn.v;
    _cycles += sp.size();
}

void SaturnL2::op_swap_a_a_Q_81af2Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto sp = fieldSpan(Field::A, 0);
    Reg64 rn{_rR[decoded.operands[0]]};
    Reg64 temp;
    temp.copyField(rn, sp);
    rn.copyField(_rA, sp);
    _rA.copyField(temp, sp);
    _rR[decoded.operands[0]] = rn.v;
    _cycles += sp.size();
}

void SaturnL2::op_move_a_c_Q_81af0Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto sp = fieldSpan(Field::A, 0);
    Reg64 rn{_rR[decoded.operands[0]]};
    rn.copyField(_rC, sp);
    _rR[decoded.operands[0]] = rn.v;
    _cycles += sp.size();
}

void SaturnL2::op_swap_a_c_Q_81af2Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto sp = fieldSpan(Field::A, 0);
    Reg64 rn{_rR[decoded.operands[0]]};
    Reg64 temp;
    temp.copyField(rn, sp);
    rn.copyField(_rC, sp);
    _rC.copyField(temp, sp);
    _rR[decoded.operands[0]] = rn.v;
    _cycles += sp.size();
}

void SaturnL2::op_move_a_Q_a_81af1Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto sp = fieldSpan(Field::A, 0);
    _rA.copyField(Reg64{_rR[decoded.operands[0]]}, sp);
    _cycles += sp.size();
}

void SaturnL2::op_move_a_Q_c_81af1Q(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto sp = fieldSpan(Field::A, 0);
    _rC.copyField(Reg64{_rR[decoded.operands[0]]}, sp);
    _cycles += sp.size();
}

void SaturnL2::op_move_G_a_Q_81aGQ0(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto sp = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    Reg64 rn{_rR[decoded.operands[1]]};
    rn.copyField(_rA, sp);
    _rR[decoded.operands[1]] = rn.v;
    _cycles += sp.size();
}

void SaturnL2::op_swap_G_a_Q_81aGQ0(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto sp = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    Reg64 rn{_rR[decoded.operands[1]]};
    Reg64 temp;
    temp.copyField(rn, sp);
    rn.copyField(_rA, sp);
    _rA.copyField(temp, sp);
    _rR[decoded.operands[1]] = rn.v;
    _cycles += sp.size();
}

void SaturnL2::op_move_G_c_Q_81aGQ0(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto sp = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    Reg64 rn{_rR[decoded.operands[1]]};
    rn.copyField(_rC, sp);
    _rR[decoded.operands[1]] = rn.v;
    _cycles += sp.size();
}

void SaturnL2::op_swap_G_c_Q_81aGQ0(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto sp = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    Reg64 rn{_rR[decoded.operands[1]]};
    Reg64 temp;
    temp.copyField(rn, sp);
    rn.copyField(_rC, sp);
    _rC.copyField(temp, sp);
    _rR[decoded.operands[1]] = rn.v;
    _cycles += sp.size();
}

void SaturnL2::op_move_G_Q_a_81aGQ0(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto sp = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rA.copyField(Reg64{_rR[decoded.operands[1]]}, sp);
    _cycles += sp.size();
}

void SaturnL2::op_move_G_Q_c_81aGQ0(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto sp = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rC.copyField(Reg64{_rR[decoded.operands[1]]}, sp);
    _cycles += sp.size();
}

void SaturnL2::op_move_pS_T_c_3ST(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    // decoded.operands[1] only holds 8 nibbles (uint32); read all n nibbles from instruction stream
    auto n = decoded.operands[0];  // count (1..16, from nzconst)
    uint32_t constAddr = _rPCOpcode + instr.varOffsets[1];
    for (uint32_t i = 0; i < n; ++i)
        _rC.setNibble((_rP + i) & 0xF, _bus.readNibble(constAddr + i));
}

void SaturnL2::op_move_pS_T_a_8082ST(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    // Matches e48 LAHEX behavior: the wrap-around loop starts at i=d (not i=0),
    // leaving A[0..d-1] unwritten when wrapping occurs.
    auto n = decoded.operands[0];  // count (1..16, from nzconst)
    uint32_t constAddr = _rPCOpcode + instr.varOffsets[1];
    uint32_t d = 16 - _rP;
    if (n <= d) {
        for (uint32_t i = 0; i < n; ++i)
            _rA.setNibble(_rP + i, _bus.readNibble(constAddr + i));
    } else {
        for (uint32_t i = 0; i < d; ++i)
            _rA.setNibble(_rP + i, _bus.readNibble(constAddr + i));
        for (uint32_t i = d; i < n - d; ++i)
            _rA.setNibble(i, _bus.readNibble(constAddr + i + d));
    }
}

void SaturnL2::op_move_2_R_d0_19RR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rD0 = (_rD0 & 0xFFF00) | (decoded.operands[0] & 0x000FF);
}

void SaturnL2::op_move_4_R_d0_1aRRRR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rD0 = (_rD0 & 0xF0000) | (decoded.operands[0] & 0x0FFFF);
}

void SaturnL2::op_move_5_R_d0_1bRRRRR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rD0 = decoded.operands[0] & 0xFFFFF;
}

void SaturnL2::op_move_2_R_d1_1dRR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rD1 = (_rD1 & 0xFFF00) | (decoded.operands[0] & 0x000FF);
}

void SaturnL2::op_move_4_R_d1_1eRRRR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rD1 = (_rD1 & 0xF0000) | (decoded.operands[0] & 0x0FFFF);
}

void SaturnL2::op_move_5_R_d1_1fRRRRR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rD1 = decoded.operands[0] & 0xFFFFF;
}

void SaturnL2::op_move_1_R_p_2R(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rP = static_cast<uint8_t>(decoded.operands[0]);
}

void SaturnL2::op_move_1_p_c_R_80cR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rC.setNibble(static_cast<int>(decoded.operands[0]), _rP);
    _cycles += 2;
}

void SaturnL2::op_move_1_c_R_p_80dR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rP = _rC.getNibble(static_cast<int>(decoded.operands[0]));
    _cycles += 2;
}

void SaturnL2::op_move_x_st_c_09(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rC.v = (_rC.v & ~0xFFF) | (_rST & 0xFFF);
    _cycles += 3;
}

void SaturnL2::op_move_x_c_st_0a(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rST = (_rST & ~0xFFF) | (_rC.v & 0xFFF);
    _cycles += 3;
}

void SaturnL2::op_swap_x_c_st_0b(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto st = _rST & 0xFFF;
    _rST = (_rST & ~0xFFF) | (_rC.v & 0xFFF);
    _rC.v = (_rC.v & ~0xFFF) | st;
    _cycles += 3;
}

void SaturnL2::op_neg_a_M_fM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rCarry = r[decoded.operands[0]]->negField(Field::A, 0, _nibbleMode);
    _cycles += 5;
}

void SaturnL2::op_neg_G_M_bGM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    _rCarry = r[decoded.operands[1]]->negField(span, _nibbleMode);
    _cycles += span.size();
}

void SaturnL2::op_not_a_M_fM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    r[decoded.operands[0]]->notField(Field::A, 0, _nibbleMode);
    _rCarry = false;
    _cycles += 5;
}

void SaturnL2::op_not_G_M_bGM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    r[decoded.operands[1]]->notField(span, _nibbleMode);
    _rCarry = false;
    _cycles += span.size();
}

void SaturnL2::op_or_a_J_0efJ(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto pair = decodeRegPair8(decoded.operands[0]);
    pair.dst->orField(*pair.src, fieldSpan(Field::A, 0));
    _cycles += 5;
}

void SaturnL2::op_or_G_J_0eGJ(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair8(decoded.operands[1]);
    pair.dst->orField(*pair.src, span);
    _cycles += span.size();
}

void SaturnL2::op_out_s_c_800(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    setOutRegister(static_cast<std::uint16_t>((_rOUT & 0xff0U) | (_rC.v & 0x00fU)), true);
    scanKeyboard(false, false);
    _cycles += 4;
}

void SaturnL2::op_out_x_c_801(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction - write C X-field (nibbles 0..2) to OUT register
    setOutRegister(static_cast<std::uint16_t>(_rC.v & 0xFFFU), true);
    scanKeyboard(false, false);
    _cycles += 3;
}

void SaturnL2::op_push_a_c_06(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    pushRSTK(_rC.v & 0xFFFFF);
    _cycles += 6;
}

void SaturnL2::op_pop_a_c_07(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rC.v = (_rC.v & ~0xFFFFFULL) | (popRSTK() & 0xFFFFF);
    _cycles += 6;
}

void SaturnL2::op_ret_01(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rPC = popRSTK();
    _cycles += 7;
}

void SaturnL2::op_retsetc_02(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rPC = popRSTK();
    _rCarry = true;
    _cycles += 7;
}

void SaturnL2::op_retclrc_03(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rPC = popRSTK();
    _rCarry = false;
    _cycles += 7;
}

void SaturnL2::op_reti_0f(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rPC = popRSTK();
    _interruptsEnabled = true;
    if ((_keyboardInterruptPending && _keyboardInterruptsEnabled) || (_rIN & 0x8000U) != 0) {
        _keyboardInterruptPending = false;
        requestSoftInterrupt();
    }
    _cycles += 7;
}

void SaturnL2::op_retsetxm_00(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rPC = popRSTK();
    _rHST |= HST_XM;
    _cycles += 7;
}

void SaturnL2::op_rln_w_M_81M(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    r[decoded.operands[0]]->rotateFieldLeft(Field::W, 0, 4);
    _cycles += 18;
}

void SaturnL2::op_rrn_w_M_81M(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto& reg = *r[decoded.operands[0]];
    reg.rotateFieldRight(Field::W, 0, 4);
    if (reg.getNibble(15) != 0) {
        _rHST |= HST_SB;
    }
    _cycles += 18;
}

void SaturnL2::op_setb_R_a_8085R(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rA.setBit(decoded.operands[0]);
    _cycles += 1;
}

void SaturnL2::op_setb_R_c_8089R(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rC.setBit(decoded.operands[0]);
    _cycles += 1;
}

void SaturnL2::op_setb_R_st_85R(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rST |= (1 << decoded.operands[0]);
    _cycles += 1;
}

void SaturnL2::op_setdec_05(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _nibbleMode = NibbleMode::BCD;
    _cycles += 1;
}

void SaturnL2::op_sethex_04(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _nibbleMode = NibbleMode::Hex;
    _cycles += 1;
}

void SaturnL2::op_sln_a_M_fM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    r[decoded.operands[0]]->shiftFieldLeft(Field::A, 0, 4);
    _cycles += 6;
}

void SaturnL2::op_sln_G_M_bGM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto field = static_cast<Field>(decoded.operands[0]);
    auto span = fieldSpan(field, _rP);
    r[decoded.operands[1]]->shiftFieldLeft(span, 4);
    _cycles += span.size() + 1;
}

void SaturnL2::op_srn_a_M_fM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    if (r[decoded.operands[0]]->shiftFieldRight(Field::A, 0, 4)) {
        _rHST |= HST_SB;
    }
    _cycles += 6;
}

void SaturnL2::op_srn_G_M_bGM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto field = static_cast<Field>(decoded.operands[0]);
    auto span = fieldSpan(field, _rP);
    if (r[decoded.operands[1]]->shiftFieldRight(span, 4)) {
        _rHST |= HST_SB;
    }
    _cycles += span.size() + 1;
}

void SaturnL2::op_srb_w_M_81M(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    if (r[decoded.operands[0]]->shiftFieldRight(Field::W, 0, 1)) {
        _rHST |= HST_SB;
    }
    _cycles += 17;
}

void SaturnL2::op_srb_a_M_819fM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    if (r[decoded.operands[0]]->shiftFieldRight(Field::A, 0, 1)) {
        _rHST |= HST_SB;
    }
    _cycles += 6;
}

void SaturnL2::op_srb_G_M_819GM(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    if (r[decoded.operands[1]]->shiftFieldRight(span, 1)) {
        _rHST |= HST_SB;
    }
    _cycles += span.size() + 1;
}

void SaturnL2::op_sub_G_K_bGK(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair8Split(decoded.operands[1]);
    _rCarry = pair.dst->subField(*pair.src, span, 0, _nibbleMode) != 0;
    _cycles += span.size();
}

void SaturnL2::op_sub_a_K_eK(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto pair = decodeRegPair8Split(decoded.operands[0]);
    _rCarry = pair.dst->subField(*pair.src, fieldSpan(Field::A, 0), 0, _nibbleMode) != 0;
    _cycles += 5;
}

void SaturnL2::op_subn_G_I_bGI(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto span = fieldSpan(static_cast<Field>(decoded.operands[0]), _rP);
    auto pair = decodeRegPair8(decoded.operands[1]);
    Reg64 result = *pair.src;
    _rCarry = result.subField(*pair.dst, span, 0, _nibbleMode) != 0;
    pair.dst->copyField(result, span);
    _cycles += span.size();
}

void SaturnL2::op_subn_a_I_eI(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto pair = decodeRegPair8(decoded.operands[0]);
    const Span sp = fieldSpan(Field::A, 0);
    Reg64 result = *pair.src;
    _rCarry = result.subField(*pair.dst, sp, 0, _nibbleMode) != 0;
    pair.dst->copyField(result, sp);
    _cycles += sp.size();
}

void SaturnL2::op_sub_a_S_M_818fMS(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    auto reg = r[decoded.operands[0]];
    const Span sp = fieldSpan(Field::A, 0);
    Reg64 src;
    src.v = decoded.operands[1];
    _rCarry = reg->subField(src, sp, 0, NibbleMode::Hex) != 0;
    _cycles += 4;
}

void SaturnL2::op_sub_G_S_M_818GMS(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    auto field = static_cast<Field>(decoded.operands[0]);
    auto reg = r[decoded.operands[1]];
    const Span sp = fieldSpan(field, _rP);
    uint32_t constant = decoded.operands[2];

    // Match the single-nibble field wrap behavior used by the corresponding
    // add-immediate form.
    int nF_l = (sp.size() == 1) ? 17 : sp.size();

    int32_t v = int32_t(reg->getNibble(sp.lo)) - int32_t(constant);
    reg->setNibble(sp.lo, uint8_t(v & 0xF));
    bool borrow = (v < 0);

    if (borrow) {
        for (int idx = 1; idx < nF_l; ++idx) {
            int ni = (sp.lo + idx) & 0xF;
            uint8_t n = reg->getNibble(ni);
            if (n > 0) {
                reg->setNibble(ni, n - 1);
                borrow = false;
                break;
            }
            reg->setNibble(ni, 0xF);
        }
    }

    _rCarry = borrow;
    _cycles += sp.hi - sp.lo;
}

void SaturnL2::op_sub_a_S_d0_18S(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    _rD0 -= decoded.operands[0];
    if (_rD0 > 0xFFFFF) {
        _rD0 &= 0xFFFFF;
        _rCarry = true;
    }
    else {
        _rCarry = false;
    }
    _cycles += 4;
}

void SaturnL2::op_sub_a_S_d1_1cS(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    _rD1 -= decoded.operands[0];
    if (_rD1 > 0xFFFFF) {
        _rD1 &= 0xFFFFF;
        _rCarry = true;
    }
    else {
        _rCarry = false;
    }
    _cycles += 4;
}

void SaturnL2::op_swap_1_p_c_R_80fR(const DecodedInstruction& decoded, const InstructionInfo& instr)
{
    // CHIP-48 used instruction
    int n = decoded.operands[0] & 0x0F;
    auto p = _rP & 0x0F;
    _rP = _rC.getNibble(n);
    _rC.setNibble(n, p);
    _cycles += 2;
}

}  // namespace hp48
