//---------------------------------------------------------------------------------------
// tests/chip8adapter.hpp
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

#include "chip8adapter.hpp"
#include <fmt/format.h>
#include "../src/emuhostex.hpp"

#ifdef TEST_CHIP8EMULATOR_TS
#include <emulation/chip8cores.hpp>

std::unique_ptr<emu::IChip8Emulator> createChip8Instance(Chip8TestVariant variant)
{
    using namespace emu;
    static Chip8EmulatorOptions options;
    switch(variant) {
        case C8TV_GENERIC:
        case C8TV_C8:
            options = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8);
            break;
        case C8TV_C10:
            options = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP10);
            break;
        case C8TV_C48:
            options = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP48);
            break;
        case C8TV_SC10:
            options = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eSCHIP10);
            break;
        case C8TV_SC11:
            options = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eSCHIP11);
            break;
        case C8TV_XO:
            options = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eXOCHIP);
            break;
        default:
            return nullptr;
    }
    static Chip8HeadlessTestHost host(options);
    if (options.optHas16BitAddr) {
        if (options.optAllowColors) {
            if (options.optWrapSprites)
                return std::make_unique<emu::Chip8Emulator<16, MultiColor | WrapSprite>>(host, options);
            else
                return std::make_unique<Chip8Emulator<16, MultiColor>>(host, options);
        }
        else {
            if (options.optWrapSprites)
                return std::make_unique<Chip8Emulator<16, WrapSprite>>(host, options);
            else
                return std::make_unique<Chip8Emulator<16, 0>>(host, options);
        }
    }
    else {
        if (options.optAllowColors) {
            if (options.optWrapSprites)
                return std::make_unique<Chip8Emulator<12, MultiColor | WrapSprite>>(host, options);
            else
                return std::make_unique<Chip8Emulator<12, MultiColor>>(host, options);
        }
        else {
            if (options.optWrapSprites)
                return std::make_unique<Chip8Emulator<12, WrapSprite>>(host, options);
            else
                return std::make_unique<Chip8Emulator<12, 0>>(host, options);
        }
    }
}

#elif defined(TEST_CHIP8EMULATOR_STRICT)
#include <emulation/chip8cores.hpp>
#include <emulation/chip8strict.hpp>

std::unique_ptr<emu::IChip8Emulator> createChip8Instance(Chip8TestVariant variant)
{
    using namespace emu;
    static Chip8EmulatorOptions options;
    switch(variant) {
        case C8TV_GENERIC:
        case C8TV_C8:
            options = Chip8EmulatorOptions::optionsOfPreset(Chip8EmulatorOptions::eCHIP8);
            break;
        default:
            return nullptr;
    }
    static Chip8HeadlessTestHost host(options);
    return std::make_unique<emu::Chip8StrictEmulator>(host, options);
}

#elif defined(TEST_CHIP8EMULATOR_FP)
#include <emulation/chip8cores.hpp>

std::unique_ptr<emu::IChip8Emulator> createChip8Instance(Chip8TestVariant variant)
{
    static emu::Chip8EmulatorOptions options;
    switch(variant) {
        case C8TV_GENERIC:
        case C8TV_C8:
            options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eCHIP8);
            break;
        case C8TV_C10:
            options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eCHIP10);
            break;
        case C8TV_C48:
            options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eCHIP48);
            break;
        case C8TV_SC10:
            options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eSCHIP10);
            break;
        case C8TV_SC11:
            options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eSCHIP11);
            break;
        case C8TV_MC8:
            options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eMEGACHIP);
            break;
        case C8TV_XO:
            options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eXOCHIP);
            break;
    }
    static Chip8HeadlessTestHost host(options);
    return std::make_unique<emu::Chip8EmulatorFP>(host, options);
}

#elif defined(TEST_CHIP8VIP)
#include <emulation/chip8cores.hpp>
#include <emulation/chip8vip.hpp>

std::unique_ptr<emu::IChip8Emulator> createChip8Instance(Chip8TestVariant variant)
{
    static emu::Chip8EmulatorOptions options;
    switch(variant) {
        case C8TV_GENERIC:
        case C8TV_C8:
            options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eCHIP8);
            break;
        default:
            return nullptr;
    }
    //options.optTraceLog = true;
    static Chip8HeadlessTestHost host(options);
    return std::make_unique<emu::CosmacVIP>(host, options);
}

#elif defined(TEST_CHIP8DREAM)

#include <emulation/chip8cores.hpp>
#include <emulation/dream6800.hpp>

std::unique_ptr<emu::IChip8Emulator> createChip8Instance(Chip8TestVariant variant)
{
    static emu::Chip8EmulatorOptions options;
    switch(variant) {
        case C8TV_GENERIC:
        case C8TV_C8:
            options = emu::Chip8EmulatorOptions::optionsOfPreset(emu::Chip8EmulatorOptions::eC8D68CHIPOSLO);
            break;
        default:
            return nullptr;
    }
    options.optTraceLog = true;
    static Chip8HeadlessTestHost host(options);
    return std::make_unique<emu::Dream6800>(host, options);
}

#elif defined(TEST_C_OCTO)

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wnarrowing"
#pragma GCC diagnostic ignored "-Wc++11-narrowing"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-int-float-conversion"
#pragma GCC diagnostic ignored "-Wenum-compare"
#if __clang__
#pragma GCC diagnostic ignored "-Wenum-compare-conditional"
#endif
#endif  // __GNUC__

extern "C" {
#include <octo_emulator.h>
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace c_octo {

class OctoChip8 : public emu::IChip8Emulator
{
public:
    void reset() override { octo_emulator_init(&octo, (char*)"", 0, &oopt, nullptr); }
    std::string name() const override { return "JamesGriffin Chip8"; }
    void setExecMode(ExecMode mode) override {}
    ExecMode execMode() const override { return eRUNNING; }
    CpuState cpuState() const override { return eNORMAL; }
    void executeInstruction() override { octo_emulator_instruction(&octo); }
    void executeInstructions(int numInstructions) override {}
    void tick(int instructionsPerFrame) override { octo_emulator_instruction(&octo); }
    std::pair<uint16_t, std::string> disassembleInstruction(const uint8_t* code, const uint8_t* end) override { return {0,""}; }
    std::string dumpStateLine() const override
    {
        return fmt::format("V0:{:02x} V1:{:02x} V2:{:02x} V3:{:02x} V4:{:02x} V5:{:02x} V6:{:02x} V7:{:02x} V8:{:02x} V9:{:02x} VA:{:02x} VB:{:02x} VC:{:02x} VD:{:02x} VE:{:02x} VF:{:02x} I:{:04x} SP:{:1x} PC:{:04x} O:{:04x}",
                           octo.v[0], octo.v[1], octo.v[2], octo.v[3], octo.v[4], octo.v[5], octo.v[6], octo.v[7],
                           octo.v[8], octo.v[9], octo.v[10], octo.v[11], octo.v[12], octo.v[13], octo.v[14], octo.v[15],
                           octo.i, octo.rp, octo.pc, (octo.ram[octo.pc]<<8)|octo.ram[octo.pc+1]);
    }
    uint8_t getV(uint8_t index) const override { return octo.v[index]; }
    uint32_t getPC() const override { return octo.pc; }
    uint32_t getI() const override { return octo.i; }
    uint8_t getSP() const override { return octo.rp; }
    uint8_t stackSize() const override { return 16;}
    const uint16_t* getStackElements() const override { return octo.ret; }

    uint8_t* memory() override { return octo.ram; }
    uint8_t* memoryCopy() override { return octo.ram; }
    int memSize() const override { return 4096; }

    int64_t cycles() const override { return 0; }
    int64_t frames() const override { return 0; }

    void handleTimer() override {}
    uint8_t delayTimer() const override { return octo.dt; }
    uint8_t soundTimer() const override { return octo.st; }

    uint16_t getCurrentScreenWidth() const override { return 64; }
    uint16_t getCurrentScreenHeight() const override { return 32; }
    uint16_t getMaxScreenWidth() const override { return 64; }
    uint16_t getMaxScreenHeight() const override { return 32; }
    const uint8_t* getScreenBuffer() const override { return octo.px; }
private:
    octo_emulator octo;
    octo_options oopt{};
};

}

std::unique_ptr<emu::IChip8Emulator> createChip8Instance()
{
    return std::make_unique<c_octo::OctoChip8>();
}

#elif defined(TEST_JAMES_GRIFFIN_CHIP_8)

// #include <stdio.h>
// #include <stdlib.h>
#include <iostream>
#include <random>
// #include "time.h"

namespace jgchip8 {

#define private public
#include "cores/jgriffin/chip8.h"
#undef private
#include "cores/jgriffin/chip8.cpp"

class JGChip8 : public emu::IChip8Emulator
{
public:
    void reset() override { _emu.init(); }
    std::string name() const override { return "JamesGriffin Chip8"; }
    void setExecMode(ExecMode mode) override {}
    ExecMode execMode() const override { return eRUNNING; }
    CpuState cpuState() const override { return eNORMAL; }
    void executeInstruction() override { _emu.emulate_cycle(); }
    void executeInstructions(int numInstructions) override {}
    void tick(int instructionsPerFrame) override { _emu.emulate_cycle(); }
    std::pair<uint16_t, std::string> disassembleInstruction(const uint8_t* code, const uint8_t* end) override { return {0,""}; }
    std::string dumpStateLine() const override
    {
        return fmt::format("V0:{:02x} V1:{:02x} V2:{:02x} V3:{:02x} V4:{:02x} V5:{:02x} V6:{:02x} V7:{:02x} V8:{:02x} V9:{:02x} VA:{:02x} VB:{:02x} VC:{:02x} VD:{:02x} VE:{:02x} VF:{:02x} I:{:04x} SP:{:1x} PC:{:04x} O:{:04x}", _emu.V[0], _emu.V[1], _emu.V[2],
                           _emu.V[3], _emu.V[4], _emu.V[5], _emu.V[6], _emu.V[7], _emu.V[8], _emu.V[9], _emu.V[10], _emu.V[11], _emu.V[12], _emu.V[13], _emu.V[14], _emu.V[15], _emu.I, _emu.sp, _emu.pc, (_emu.memory[_emu.pc & (memSize()-1)]<<8)|_emu.memory[(_emu.pc + 1) & (memSize()-1)]);
    }
    uint8_t getV(uint8_t index) const override { return _emu.V[index]; }
    uint32_t getPC() const override { return _emu.pc; }
    uint32_t getI() const override { return _emu.I; }
    uint8_t getSP() const override { return _emu.sp; }
    uint8_t stackSize() const override { return 16;}
    const uint16_t* getStackElements() const override { return _emu.stack; }

    uint8_t* memory() override { return _emu.memory; }
    uint8_t* memoryCopy() override { return _emu.memory; }
    int memSize() const override { return 4096; }

    int64_t cycles() const override { return 0; }
    int64_t frames() const override { return 0; }

    void handleTimer() override {}
    uint8_t delayTimer() const override { return _emu.delay_timer; }
    uint8_t soundTimer() const override { return _emu.sound_timer; }

    uint16_t getCurrentScreenWidth() const override { return 64; }
    uint16_t getCurrentScreenHeight() const override { return 32; }
    uint16_t getMaxScreenWidth() const override { return 64; }
    uint16_t getMaxScreenHeight() const override { return 32; }
    const uint8_t* getScreenBuffer() const override { return _emu.gfx; }
private:
    jgchip8::Chip8 _emu;
};

}

std::unique_ptr<emu::IChip8Emulator> createChip8Instance()
{
    return std::make_unique<jgchip8::JGChip8>();
}

#elif defined(TEXT_WERNSEY_CHIP_8)

namespace wernsey {

#include "cores/wernsey/chip8.h"
#define malloc(s) (char*)malloc(s)
#include "cores/wernsey/chip8.c"
#undef malloc

class WernseyChip8 : public emu::IChip8Emulator
{
public:
    void reset() override { c8_reset(); }
    std::string name() const override { return "wernsey Chip8"; }
    void setExecMode(ExecMode mode) override {}
    ExecMode execMode() const override { return eRUNNING; }
    CpuState cpuState() const override { return eNORMAL; }
    void executeInstruction() override { c8_step(); }
    void executeInstructions(int numInstructions) override {}
    void tick(int instructionsPerFrame) override { c8_60hz_tick(); c8_step(); }
    std::pair<uint16_t, std::string> disassembleInstruction(const uint8_t* code, const uint8_t* end) override { return {0, ""}; }
    std::string dumpStateLine() const override
    {
        return fmt::format("V0:{:02x} V1:{:02x} V2:{:02x} V3:{:02x} V4:{:02x} V5:{:02x} V6:{:02x} V7:{:02x} V8:{:02x} V9:{:02x} VA:{:02x} VB:{:02x} VC:{:02x} VD:{:02x} VE:{:02x} VF:{:02x} I:{:04x} SP:{:1x} PC:{:04x} O:{:04x}", c8_get_reg(0), c8_get_reg(1),
                           c8_get_reg(2), c8_get_reg(3), c8_get_reg(4), c8_get_reg(5), c8_get_reg(6), c8_get_reg(7), c8_get_reg(8), c8_get_reg(9), c8_get_reg(10), c8_get_reg(11), c8_get_reg(12), c8_get_reg(13), c8_get_reg(14), c8_get_reg(15), I, SP, c8_get_pc(),
                           (c8_get(c8_get_pc() & (TOTAL_RAM - 1)) << 8) | c8_get((c8_get_pc() + 1) & (TOTAL_RAM - 1)));
    }
    uint8_t getV(uint8_t index) const override { return c8_get_reg(index); }
    uint32_t getPC() const override { return PC; }
    uint32_t getI() const override { return I; }
    uint8_t getSP() const override { return SP; }
    uint8_t stackSize() const override { return 16; }
    const uint16_t* getStackElements() const override { return stack; }

    uint8_t* memory() override { return RAM; }
    uint8_t* memoryCopy() override { return RAM; }
    int memSize() const override { return 4096; }

    int64_t cycles() const override { return 0; }
    int64_t frames() const override { return 0; }

    void handleTimer() override {}
    uint8_t delayTimer() const override { return DT; }
    uint8_t soundTimer() const override { return ST; }

    uint16_t getCurrentScreenWidth() const override { return 64; }
    uint16_t getCurrentScreenHeight() const override { return 32; }
    uint16_t getMaxScreenWidth() const override { return 64; }
    uint16_t getMaxScreenHeight() const override { return 32; }
    const uint8_t* getScreenBuffer() const override { return pixels; }
};

} // namespace wernsey

std::unique_ptr<emu::IChip8Emulator> createChip8Instance()
{
    return std::make_unique<wernsey::WernseyChip8>();
}

#endif
