//---------------------------------------------------------------------------------------
// src/emulation/chip8emulatorbase.hpp
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

#include <emulation/chip8emulatorhost.hpp>
#include <emulation/chip8options.hpp>
#include <emulation/chip8vip.hpp>
#include <emulation/chip8opcodedisass.hpp>
#include <emulation/time.hpp>
#include <emulation/videoscreen.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <cstdint>

#include <fmt/format.h>
#include <stdendian/stdendian.h>


namespace emu {

class Chip8EmulatorBase : public Chip8OpcodeDisassembler
{
public:
    enum MegaChipBlendMode { eBLEND_NORMAL = 0, eBLEND_ALPHA_25 = 1, eBLEND_ALPHA_50 = 2, eBLEND_ALPHA_75 = 3, eBLEND_ADD = 4, eBLEND_MUL = 5 };
    enum Chip8Font { C8F5_COSMAC, C8F5_ETI, C8F5_DREAM, C8F5_CHIP48 };
    enum Chip8BigFont { C8F10_SCHIP10, C8F10_SCHIP11, C8F10_XOCHIP };
    constexpr static int MAX_SCREEN_WIDTH = 256;
    constexpr static int MAX_SCREEN_HEIGHT = 192;
    constexpr static uint32_t MAX_ADDRESS_MASK = (1<<24)-1;
    constexpr static uint32_t MAX_MEMORY_SIZE = 1<<24;
    using SymbolResolver = std::function<std::string(uint16_t)>;
    Chip8EmulatorBase(Chip8EmulatorHost& host, Chip8EmulatorOptions& options, IChip8Emulator* iother)
        : Chip8OpcodeDisassembler(options)
        , _host(host)
        , _memory(options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP ? 0x1000001 : options.optHas16BitAddr ? 0x10001 : 0x1001, 0)
    {
        _mcPalette[0] = be32(0x000000FF);
        _mcPalette[1] = be32(0xFFFFFFFF);
        _execMode = ExecMode::ePAUSED;
        if(iother) {
            _cpuState = iother->cpuState();
            _rI = iother->getI();
            _rPC = iother->getPC();
            std::memcpy(_stack.data(), iother->getStackElements(), sizeof(uint16_t) * iother->stackSize());
            _rSP = iother->getSP();
            _rDT = iother->delayTimer();
            _rST.store(iother->soundTimer());
            _wavePhase.store(iother->getAudioPhase());
            for (int i = 0; i < 16; ++i) {
                _rV[i] = iother->getV(i);
            }
            std::memcpy(_memory.data(), iother->memory(), std::min(_memory.size(), (size_t)iother->memSize()));
        }
        const auto* other = dynamic_cast<const Chip8EmulatorBase*>(iother);
        if(other) {
            _labelOrAddress = other->_labelOrAddress;
            _isHires = options.optAllowHires && other->_isHires;
            _planes = other->_planes;
            _stepOverSP = other->_stepOverSP;
            _cycleCounter = other->_cycleCounter;
            _frameCounter = other->_frameCounter;
            _clearCounter = other->_clearCounter;
            _screen = other->_screen;
            _screenRGBA = other->_screenRGBA;
            _xoAudioPattern = other->_xoAudioPattern;
            _xoPitch.store(other->_xoPitch);
            _sampleStep.store(other->_sampleStep);
            _sampleStart.store(other->_sampleStart);
            _sampleLength.store(other->_sampleLength);
            _mcSamplePos.store(other->_mcSamplePos);
            _sampleLoop = other->_sampleLoop;
            _xxoPalette = other->_xxoPalette;
            _mcPalette = other->_mcPalette;
            _randomSeed = other->_randomSeed;
            std::memcpy(_breakMap.data(), other->_breakMap.data(), 4096);
            _breakpoints = other->_breakpoints;
            _systemTime = other->_systemTime;
            _spriteWidth = other->_spriteWidth;
            _spriteHeight = other->_spriteHeight;
            _collisionColor = other->_collisionColor;
            _blendMode = other->_blendMode;
        }
        else {
            _isHires = false;
            _planes = 1;
            removeAllBreakpoints();
        }
        if(!_isHires && _options.optOnlyHires) {
            _isHires = true;
        }
        _isMegaChipMode = (_options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP && other && other->_isMegaChipMode);
        _labelOrAddress = [](uint16_t addr){ return fmt::format("0x{:04X}", addr); };
    }
    ~Chip8EmulatorBase() override = default;

    bool inErrorState() const override { return _cpuState == eERROR; }
    uint32_t getCpuID() const override { return 0xC8; }
    const std::string& getName() const override { static const std::string name = "GenericChip8"; return name; }
    const std::vector<std::string>& getRegisterNames() const override {
        static const std::vector<std::string> registerNames = {
            "V0", "V1", "V2", "V3", "V4", "V5", "V6", "V7",
            "V8", "V9", "VA", "VB", "VC", "VD", "VE", "VF",
            "I", "DT", "ST", "PC", "SP"
        };
        return registerNames;
    }
    size_t getNumRegisters() const override { return 21; }
    RegisterValue getRegister(size_t index) const override
    {
        if(index < 16)
            return {_rV[index], 8};
        if(index == 16)
            return {_rI, 16};
        else if(index == 17)
            return {_rDT, 8};
        else if(index == 18)
            return {_rST, 8};
        else if(index == 19)
            return {_rPC, 16};
        return {_rSP, 8};
    }
    void setRegister(size_t index, uint32_t value) override
    {
        if(index < 16)
            _rV[index] = static_cast<uint8_t>(value);
        else if(index == 16)
            _rI = static_cast<uint16_t>(value);
        else if(index == 17)
            _rDT = static_cast<uint8_t>(value);
        else if(index == 18)
            _rST = static_cast<uint8_t>(value);
        else if(index == 19)
            _rPC = static_cast<uint16_t>(value);
        else
            _rSP = static_cast<uint8_t>(value);
    }
    uint8_t getMemoryByte(uint32_t addr) const override
    {
        return _memory[addr % _memory.size()];
    }
    std::string disassembleInstructionWithBytes(int32_t pc, int* bytes) const override
    {
        if(pc < 0) pc = _rPC;
        uint8_t code[4];
        for(size_t i = 0; i < 4; ++i) {
            code[i] = getMemoryByte(pc + i);
        }
        auto [size, instruction] = disassembleInstruction(code, code + 4);
        if(bytes)
            *bytes = size;
        if (size == 2)
            return fmt::format("{:04X}: {:04X}       {}", pc, (code[0] << 8)|code[1], instruction);
        return fmt::format("{:04X}: {:04X} {:04X}  {}", pc, (code[0] << 8)|code[1], (code[2] << 8)|code[3], instruction);
    }

    void clearScreen()
    {
        if(_options.optAllowColors) {
            _screen.binaryAND(~_planes);
        }
        else {
            _screen.setAll(0);
            if (_options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP) {
                const auto black = be32(0x000000FF);
                _screenRGBA.setAll(black);
            }
        }
    }

    std::string dumpStateLine() const override
    {
        return fmt::format("V0:{:02x} V1:{:02x} V2:{:02x} V3:{:02x} V4:{:02x} V5:{:02x} V6:{:02x} V7:{:02x} V8:{:02x} V9:{:02x} VA:{:02x} VB:{:02x} VC:{:02x} VD:{:02x} VE:{:02x} VF:{:02x} I:{:04x} SP:{:1x} PC:{:04x} O:{:04x}", _rV[0], _rV[1], _rV[2],
                           _rV[3], _rV[4], _rV[5], _rV[6], _rV[7], _rV[8], _rV[9], _rV[10], _rV[11], _rV[12], _rV[13], _rV[14], _rV[15], _rI, _rSP, _rPC, (_memory[_rPC & (memSize()-1)]<<8)|_memory[(_rPC + 1) & (memSize()-1)]);
    }


    uint8_t getV(uint8_t index) const override { return _rV[index]; }
    uint32_t getPC() const override { return _rPC; }
    uint32_t getI() const override { return _rI; }
    uint32_t getSP() const override { return _rSP; }
    uint8_t stackSize() const override { return _stack.size(); }
    const uint16_t* getStackElements() const override { return _stack.data(); }
    CpuState cpuState() const override { return _cpuState; }
    uint8_t delayTimer() const override { return _rDT; }
    uint8_t soundTimer() const override { return _rST; }
    uint8_t* memory() override { return _memory.data(); }
    int memSize() const override { return _memory.size() - 1; }
    void reset() override;
    int64_t getCycles() const override { return _cycleCounter; }
    int64_t frames() const override { return _frameCounter; }

    inline void addCycles(emu::cycles_t cycles)
    {
        _systemTime.addCycles(cycles * 8, 1760000);
    }

    inline void errorHalt()
    {
        _execMode = ePAUSED;
        _cpuState = eERROR;
        _rPC -= 2;
        --_cycleCounter;
    }

    inline void halt()
    {
        _execMode = ePAUSED;
        _rPC -= 2;
        --_cycleCounter;
    }

    void handleTimer() override
    {
        if(_execMode != ePAUSED) {
            ++_frameCounter;
            ++_randomSeed;
            if (_rDT > 0)
                --_rDT;
            if (_rST > 0)
                --_rST;
            if (!_rST)
                _wavePhase = 0;
        }
    }

    void tick(int instructionsPerFrame) override;

    bool needsScreenUpdate() override { bool rc = _screenNeedsUpdate; _screenNeedsUpdate = false; return _isMegaChipMode ? false : rc; }
    uint16_t getCurrentScreenWidth() const override { return _isMegaChipMode ? 256 : _options.optAllowHires ? 128 : 64; }
    uint16_t getCurrentScreenHeight() const override { return _isMegaChipMode ? 192 : _options.optAllowHires ? 64 : 32; }
    uint16_t getMaxScreenWidth() const override { return _options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP ? 256 : 128; }
    uint16_t getMaxScreenHeight() const override { return _options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP ? 192 : 64; }
    bool isDoublePixel() const override { return _options.behaviorBase == Chip8EmulatorOptions::eMEGACHIP ? false : (_options.optAllowHires && !_isHires); }
    const VideoType* getScreen() const override { return _isMegaChipMode ? nullptr : &_screen; }
    const VideoRGBAType* getScreenRGBA() const override { return _isMegaChipMode ? &_screenRGBA : nullptr; }
    void setPalette(std::array<uint32_t,256>& palette) override { _screen.setPalette(palette); }

    float getAudioPhase() const override { return _wavePhase; }
    void setAudioPhase(float phase) override { _wavePhase = phase; }
    const uint8_t* getXOAudioPattern() const override { return _xoAudioPattern.data(); }
    uint8_t getXOPitch() const override { return _xoPitch; }

    std::pair<const uint8_t*, size_t> getSmallFontData() const;
    std::pair<const uint8_t*, size_t> getBigFontData() const;

    static std::unique_ptr<IChip8Emulator> create(Chip8EmulatorHost& host, Engine engine, Chip8EmulatorOptions& options, IChip8Emulator* other = nullptr);

    static std::pair<const uint8_t*, size_t> smallFontData(Chip8Font font = Chip8Font::C8F5_COSMAC);
    static std::pair<const uint8_t*, size_t> bigFontData(Chip8BigFont font = Chip8BigFont::C8F10_SCHIP11);

protected:
    void fixupSafetyPad() { memory()[memSize()] = *memory(); }
    CpuState _cpuState{eNORMAL};
    bool _isHires{false};
    bool _isMegaChipMode{false};
    bool _screenNeedsUpdate{false};
    uint8_t _planes{1};
    int64_t _cycleCounter{0};
    int _frameCounter{0};
    int _clearCounter{0};
    uint32_t _rI{};
    uint32_t _rPC{};
    std::array<uint16_t,16> _stack{};
    uint8_t _rSP{};
    uint8_t _rDT{};
    std::atomic_uint8_t _rST{};
    std::atomic<float> _wavePhase{0};
    VideoScreen<uint8_t, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT> _screen;
    VideoScreen<uint32_t, MAX_SCREEN_WIDTH, MAX_SCREEN_HEIGHT> _screenRGBA{};
    std::array<uint8_t,16> _xoAudioPattern{};
    std::atomic_uint8_t _xoPitch{};
    std::atomic<float> _sampleStep{0};
    std::atomic_uint32_t _sampleStart{0};
    std::atomic_uint32_t _sampleLength{0};
    bool _sampleLoop{true};
    std::atomic<double> _mcSamplePos{0};
    std::array<uint8_t,16> _xxoPalette{};
    std::array<uint32_t,256> _mcPalette{};
    std::array<uint8_t,16> _rV{};
    uint16_t _spriteWidth{0};
    uint16_t _spriteHeight{0};
    uint8_t _collisionColor{1};
    MegaChipBlendMode _blendMode{eBLEND_NORMAL};

    Chip8EmulatorHost& _host;
    uint16_t _randomSeed{0};
    std::vector<uint8_t> _memory{};
    Time _systemTime{};
    /*
    inline static uint8_t _chip8font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
        0x20, 0x60, 0x20, 0x20, 0x70,  // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,  // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,  // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
        0xF0, 0x80, 0xF0, 0x80, 0x80,  // F
        0x3C, 0x7E, 0xE7, 0xC3, 0xC3, 0xC3, 0xC3, 0xE7, 0x7E, 0x3C, // big 0
        0x18, 0x38, 0x58, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, // big 1
        0x3E, 0x7F, 0xC3, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xFF, 0xFF, // big 2
        0x3C, 0x7E, 0xC3, 0x03, 0x0E, 0x0E, 0x03, 0xC3, 0x7E, 0x3C, // big 3
        0x06, 0x0E, 0x1E, 0x36, 0x66, 0xC6, 0xFF, 0xFF, 0x06, 0x06, // big 4
        0xFF, 0xFF, 0xC0, 0xC0, 0xFC, 0xFE, 0x03, 0xC3, 0x7E, 0x3C, // big 5
        0x3E, 0x7C, 0xE0, 0xC0, 0xFC, 0xFE, 0xC3, 0xC3, 0x7E, 0x3C, // big 6
        0xFF, 0xFF, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x60, // big 7
        0x3C, 0x7E, 0xC3, 0xC3, 0x7E, 0x7E, 0xC3, 0xC3, 0x7E, 0x3C, // big 8
        0x3C, 0x7E, 0xC3, 0xC3, 0x7F, 0x3F, 0x03, 0x03, 0x3E, 0x7C  // big 9
    };
    */
    inline static const uint8_t _chip8_cosmac_vip[0x200] = {
        0x91, 0xbb, 0xff, 0x01, 0xb2, 0xb6, 0xf8, 0xcf, 0xa2, 0xf8, 0x81, 0xb1, 0xf8, 0x46, 0xa1, 0x90, 0xb4, 0xf8, 0x1b, 0xa4, 0xf8, 0x01, 0xb5, 0xf8, 0xfc, 0xa5, 0xd4, 0x96, 0xb7, 0xe2, 0x94, 0xbc, 0x45, 0xaf, 0xf6, 0xf6, 0xf6, 0xf6, 0x32, 0x44,
        0xf9, 0x50, 0xac, 0x8f, 0xfa, 0x0f, 0xf9, 0xf0, 0xa6, 0x05, 0xf6, 0xf6, 0xf6, 0xf6, 0xf9, 0xf0, 0xa7, 0x4c, 0xb3, 0x8c, 0xfc, 0x0f, 0xac, 0x0c, 0xa3, 0xd3, 0x30, 0x1b, 0x8f, 0xfa, 0x0f, 0xb3, 0x45, 0x30, 0x40, 0x22, 0x69, 0x12, 0xd4, 0x00,
        0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00, 0x7c, 0x75, 0x83, 0x8b, 0x95, 0xb4, 0xb7, 0xbc, 0x91, 0xeb, 0xa4, 0xd9, 0x70, 0x99, 0x05, 0x06, 0xfa, 0x07, 0xbe, 0x06, 0xfa, 0x3f, 0xf6,
        0xf6, 0xf6, 0x22, 0x52, 0x07, 0xfa, 0x1f, 0xfe, 0xfe, 0xfe, 0xf1, 0xac, 0x9b, 0xbc, 0x45, 0xfa, 0x0f, 0xad, 0xa7, 0xf8, 0xd0, 0xa6, 0x93, 0xaf, 0x87, 0x32, 0xf3, 0x27, 0x4a, 0xbd, 0x9e, 0xae, 0x8e, 0x32, 0xa4, 0x9d, 0xf6, 0xbd, 0x8f, 0x76,
        0xaf, 0x2e, 0x30, 0x98, 0x9d, 0x56, 0x16, 0x8f, 0x56, 0x16, 0x30, 0x8e, 0x00, 0xec, 0xf8, 0xd0, 0xa6, 0x93, 0xa7, 0x8d, 0x32, 0xd9, 0x06, 0xf2, 0x2d, 0x32, 0xbe, 0xf8, 0x01, 0xa7, 0x46, 0xf3, 0x5c, 0x02, 0xfb, 0x07, 0x32, 0xd2, 0x1c, 0x06,
        0xf2, 0x32, 0xce, 0xf8, 0x01, 0xa7, 0x06, 0xf3, 0x5c, 0x2c, 0x16, 0x8c, 0xfc, 0x08, 0xac, 0x3b, 0xb3, 0xf8, 0xff, 0xa6, 0x87, 0x56, 0x12, 0xd4, 0x9b, 0xbf, 0xf8, 0xff, 0xaf, 0x93, 0x5f, 0x8f, 0x32, 0xdf, 0x2f, 0x30, 0xe5, 0x00, 0x42, 0xb5,
        0x42, 0xa5, 0xd4, 0x8d, 0xa7, 0x87, 0x32, 0xac, 0x2a, 0x27, 0x30, 0xf5, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45, 0xa3, 0x98, 0x56, 0xd4, 0xf8, 0x81, 0xbc, 0xf8, 0x95, 0xac, 0x22, 0xdc, 0x12, 0x56, 0xd4, 0x06, 0xb8, 0xd4,
        0x06, 0xa8, 0xd4, 0x64, 0x0a, 0x01, 0xe6, 0x8a, 0xf4, 0xaa, 0x3b, 0x28, 0x9a, 0xfc, 0x01, 0xba, 0xd4, 0xf8, 0x81, 0xba, 0x06, 0xfa, 0x0f, 0xaa, 0x0a, 0xaa, 0xd4, 0xe6, 0x06, 0xbf, 0x93, 0xbe, 0xf8, 0x1b, 0xae, 0x2a, 0x1a, 0xf8, 0x00, 0x5a,
        0x0e, 0xf5, 0x3b, 0x4b, 0x56, 0x0a, 0xfc, 0x01, 0x5a, 0x30, 0x40, 0x4e, 0xf6, 0x3b, 0x3c, 0x9f, 0x56, 0x2a, 0x2a, 0xd4, 0x00, 0x22, 0x86, 0x52, 0xf8, 0xf0, 0xa7, 0x07, 0x5a, 0x87, 0xf3, 0x17, 0x1a, 0x3a, 0x5b, 0x12, 0xd4, 0x22, 0x86, 0x52,
        0xf8, 0xf0, 0xa7, 0x0a, 0x57, 0x87, 0xf3, 0x17, 0x1a, 0x3a, 0x6b, 0x12, 0xd4, 0x15, 0x85, 0x22, 0x73, 0x95, 0x52, 0x25, 0x45, 0xa5, 0x86, 0xfa, 0x0f, 0xb5, 0xd4, 0x45, 0xe6, 0xf3, 0x3a, 0x82, 0x15, 0x15, 0xd4, 0x45, 0xe6, 0xf3, 0x3a, 0x88,
        0xd4, 0x45, 0x07, 0x30, 0x8c, 0x45, 0x07, 0x30, 0x84, 0xe6, 0x62, 0x26, 0x45, 0xa3, 0x36, 0x88, 0xd4, 0x3e, 0x88, 0xd4, 0xf8, 0xf0, 0xa7, 0xe7, 0x45, 0xf4, 0xa5, 0x86, 0xfa, 0x0f, 0x3b, 0xb2, 0xfc, 0x01, 0xb5, 0xd4, 0x45, 0x56, 0xd4, 0x45,
        0xe6, 0xf4, 0x56, 0xd4, 0x45, 0xfa, 0x0f, 0x3a, 0xc4, 0x07, 0x56, 0xd4, 0xaf, 0x22, 0xf8, 0xd3, 0x73, 0x8f, 0xf9, 0xf0, 0x52, 0xe6, 0x07, 0xd2, 0x56, 0xf8, 0xff, 0xa6, 0xf8, 0x00, 0x7e, 0x56, 0xd4, 0x19, 0x89, 0xae, 0x93, 0xbe, 0x99, 0xee,
        0xf4, 0x56, 0x76, 0xe6, 0xf4, 0xb9, 0x56, 0x45, 0xf2, 0x56, 0xd4, 0x45, 0xaa, 0x86, 0xfa, 0x0f, 0xba, 0xd4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x4b
    };

};

//---------------------------------------------------------------------------------------
// Sprite drawing related quirk flags for templating
//---------------------------------------------------------------------------------------
enum Chip8Quirks { HiresSupport = 1, MultiColor = 2, WrapSprite = 4, SChip11Collisions = 8, SChip1xLoresDraw = 16 };

}  // namespace emu

