//---------------------------------------------------------------------------------------
// src/emulation/chip8rsm8k1.hpp
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
// This implementation is based on a Ghidra-analysis of the original RS-M8001
// implementation by Martijn Wanting, Revival-Studios, 2007. The emulation
// behavior is functionally reimplemented in C++ by this code.
//---------------------------------------------------------------------------------------
#pragma once

#include <emulation/chip8genericbase.hpp>
#include <emulation/emulatorhost.hpp>
#include <emulation/chip8rsm8k1detail.hpp>
#include <memory>

namespace emu {
struct Chip8RSM8k1Options {
    bool vipMode = false;
    bool traceLog = false;
    Palette palette;
    Properties asProperties() const;
    static Chip8RSM8k1Options fromProperties(const Properties& props);
    static Properties& registeredPrototype();
};

// Guest addresses are exposed to Cadmium; the interpreter stores ROM offsets.
// 0x000..0x1ff are debugger padding, never accessible as emulated RAM.
class Chip8RSM8k1 final : public Chip8GenericBase {
public:
    Chip8RSM8k1(EmulatorHost& host, Properties& props, IEmulationCore* other = nullptr);
    ~Chip8RSM8k1() override;
    void reset(bool cold = true) { GenericCpu::reset(cold); }
    std::string name() const override { return "RS-M8K1"; }
    uint32_t cpuID() const override { return 0x52534d38; }
    chip8::VariantSet supportedChip8Variants() const override;
    bool updateProperties(Properties& props, Property& changed) override;
    bool loadData(std::span<const uint8_t> data, std::optional<uint32_t> address) override;
    uint32_t defaultLoadAddress() const override { return 0x200; }
    uint8_t readMemoryByte(uint32_t address) const override;
    int executeInstruction() override;
    void executeInstructions(int count) override;
    void executeFrame() override;
    void handleTimer() override;
    int frameRate() const override { return 50; }
    int64_t machineCycles() const override { return cycles(); }
    unsigned stackSize() const override { return 16; }
    StackContent stack() const override;
    bool restoreState(const Chip8State& state) override;
    uint16_t getCurrentScreenWidth() const override;
    uint16_t getCurrentScreenHeight() const override;
    uint16_t getMaxScreenWidth() const override { return 256; }
    uint16_t getMaxScreenHeight() const override { return 192; }
    int getMaxColors() const override { return 256; }
    PixelRatio getPixelRatio() const override;
    const VideoType* getScreen() const override;
    const VideoRGBAType* getScreenRGBA() const override;
    const VideoRGBAType* getWorkRGBA() const override;
    void setPalette(const Palette& palette) override;
    void renderAudio(int16_t* samples, size_t frames, int sampleFrequency) override;
    // Deterministic wall-clock input for single-step behavioral tests.
    void advanceTime(uint32_t milliseconds);
    const rsm8k1detail::State& nativeState() const;
protected:
    void handleReset(bool cold) override;
private:
    struct Impl;
    void syncToInterpreter();
    void syncFromInterpreter();
    void errorHalt(const std::string& message, uint32_t pc);
    void refreshIndexedScreen() const;
    EmulatorHost& _host;
    Chip8RSM8k1Options _options;
    std::unique_ptr<Impl> _impl;
    mutable VideoType _screen{256, 192};
    VideoRGBAType _front{256, 192};
    mutable VideoRGBAType _work{256, 192};
    mutable std::array<uint8_t, 64> _guestStack{};
};
}
