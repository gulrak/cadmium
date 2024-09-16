//
// Created by Steffen Schümann on 19.11.22.
//

#include <emulation/chip8opcodedisass.hpp>
#include <chiplet/chip8meta.hpp>

#include <fmt/format.h>

namespace emu {

Chip8OpcodeDisassembler::Chip8OpcodeDisassembler()
: _opcodeSet(Chip8EmulatorOptions::variantForPreset(Chip8EmulatorOptions::eCHIP8 /*options.behaviorBase*/)) // TODO: reactivate opcode set selection
{
    _labelOrAddress = [](uint16_t addr){ return fmt::format("0x{:04X}", addr); };
}

std::tuple<uint16_t, uint16_t, std::string> Chip8OpcodeDisassembler::disassembleInstruction(const uint8_t* code, const uint8_t* end) const
{
    auto opcode = (*code << 8) | *(code + 1);
    auto next = code + 3 < end ? (*(code + 2) << 8) | *(code + 3) : 0;
#if 1
    return _opcodeSet.formatOpcode(opcode, next);
#else
    switch (opcode >> 12) {
        case 0:
            if (opcode == 0x0010 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, "megaoff"};
            if (opcode == 0x0011 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, "megaon"};
            if ((opcode & 0xFFF0) == 0x00B0 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("scroll-up-alt {}", opcode & 0xF)};
            if ((opcode & 0xFFF0) == 0x00C0) return {2, fmt::format("scroll-down {}", opcode & 0xF)};
            if ((opcode & 0xFFF0) == 0x00D0 && _options.behaviorBase != emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("scroll-up {}", opcode & 0xF)};
            if (opcode == 0x00E0) return {2, "clear"};
            if (opcode == 0x00EE) return {2, "return"};
            if (opcode == 0x00FB) return {2, "scroll-right"};
            if (opcode == 0x00FC) return {2, "scroll-left"};
            if (opcode == 0x00FD) return {2, "exit"};
            if (opcode == 0x00FE) return {2, "lores"};
            if (opcode == 0x00FF) return {2, "hires"};
            if ((opcode & 0xFF00) == 0x0100 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {4, fmt::format("ldhi {}", _labelOrAddress(((opcode&0xFF)<<16)|next))};
            if ((opcode & 0xFF00) == 0x0200 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("ldpal {}", opcode & 0xFF)};
            if ((opcode & 0xFF00) == 0x0300 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("sprw {}", opcode & 0xFF)};
            if ((opcode & 0xFF00) == 0x0400 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("sprh {}", opcode & 0xFF)};
            if ((opcode & 0xFF00) == 0x0500 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("alpha {}", opcode & 0xFF)};
            if ((opcode & 0xFFF0) == 0x0600 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("digisnd {}", opcode & 0xF)};
            if (opcode == 0x0700 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, "stopsnd"};
            if ((opcode & 0xFFF0) == 0x0800 && _options.behaviorBase == emu::Chip8EmulatorOptions::eMEGACHIP) return {2, fmt::format("bmode {}", opcode & 0xF)};
            return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
        case 1: return {2, fmt::format("jump {}", _labelOrAddress(opcode & 0xFFF))};
        case 2: return {2, fmt::format(":call {}", _labelOrAddress(opcode & 0xFFF))};
        case 3: return {2, fmt::format("if v{:X} != 0x{:02X} then", (opcode >> 8) & 0xF, opcode & 0xFF)};
        case 4: return {2, fmt::format("if v{:X} == 0x{:02X} then", (opcode >> 8) & 0xF, opcode & 0xFF)};
        case 5:
            if ((opcode & 0xF) == 0)
                return {2, fmt::format("if v{:X} != v{:X} then", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
            else if((opcode & 0xF) == 2)
                return {2, fmt::format("save v{:X} - v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
            else if((opcode & 0xF) == 3)
                return {2, fmt::format("load v{:X} - v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
            else
                return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
        case 6: return {2, fmt::format("v{:X} := 0x{:02X}", (opcode >> 8) & 0xF, opcode & 0xff)};
        case 7: return {2, fmt::format("v{:X} += 0x{:02X}", (opcode >> 8) & 0xF, opcode & 0xff)};
        case 8:
            switch (opcode & 0xF) {
                case 0: return {2, fmt::format("v{:X} := v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 1: return {2, fmt::format("v{:X} |= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 2: return {2, fmt::format("v{:X} &= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 3: return {2, fmt::format("v{:X} ^= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 4: return {2, fmt::format("v{:X} += v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 5: return {2, fmt::format("v{:X} -= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 6: return {2, fmt::format("v{:X} >>= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 7: return {2, fmt::format("v{:X} =- v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                case 0xE: return {2, fmt::format("v{:X} <<= v{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
                default: return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
            }
            break;
        case 9:
            if((opcode & 0xF) == 0)
                return {2, fmt::format("if v{:X} == v{:X} then", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF)};
            else
                return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
        case 0xA: return {2, fmt::format("i := {}", _labelOrAddress(opcode & 0xFFF))};
        case 0xB: return {2, fmt::format("jump0 {}", _labelOrAddress(opcode & 0xFFF))};
        case 0xC: return {2, fmt::format("v{:X} := random 0x{:02X}", (opcode >> 8) & 0xF, opcode & 0xff)};
        case 0xD: return {2, fmt::format("sprite v{:X} v{:X} 0x{:X}", (opcode >> 8) & 0xF, (opcode >> 4) & 0xF, opcode & 0xF)};
        case 0xE:
            switch (opcode & 0xFF) {
                case 0x9E: return {2, fmt::format("if v{:X} -key then", (opcode >> 8) & 0xF)};
                case 0xA1: return {2, fmt::format("if v{:X} key then", (opcode >> 8) & 0xF)};
                default: return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
            }
            break;
        case 0xF:
            switch (opcode & 0xFF) {
                case 0x00: return {4, fmt::format("i := long {}", _labelOrAddress(next))};
                case 0x01: return {2, fmt::format("plane {}", (opcode >> 8) & 0xF)};
                case 0x02:
                    if(opcode == 0xF002)
                        return {2, "audio"};
                    else
                        return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
                case 0x07: return {2, fmt::format("v{:X} := delay", (opcode >> 8) & 0xF)};
                case 0x0A: return {2, fmt::format("v{:X} := key", (opcode >> 8) & 0xF)};
                case 0x15: return {2, fmt::format("delay := v{:X}", (opcode >> 8) & 0xF)};
                case 0x18: return {2, fmt::format("sound := v{:X}", (opcode >> 8) & 0xF)};
                case 0x1E: return {2, fmt::format("i += v{:X}", (opcode >> 8) & 0xF)};
                case 0x29: return {2, fmt::format("i := hex v{:X}", (opcode >> 8) & 0xF)};
                case 0x30: return {2, fmt::format("i := bighex v{:X}", (opcode >> 8) & 0xF)};
                case 0x33: return {2, fmt::format("bcd v{:X}", (opcode >> 8) & 0xF)};
                case 0x3A: return {2, fmt::format("pitch := v{:X}", (opcode >> 8) & 0xF)};
                case 0x55: return {2, fmt::format("save v{:X}", (opcode >> 8) & 0xF)};
                case 0x65: return {2, fmt::format("load v{:X}", (opcode >> 8) & 0xF)};
                case 0x75: return {2, fmt::format("saveflags v{:X}", (opcode >> 8) & 0xF)};
                case 0x85: return {2, fmt::format("loadflags v{:X}", (opcode >> 8) & 0xF)};
                default:
                    return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
            }
            break;
        default:
            return {2, fmt::format("0x{:02X} 0x{:02X}", opcode >> 8, opcode & 0xFF)};
    }
#endif
}

}
