//---------------------------------------------------------------------------------------
// src/debugger.hpp
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

#include <emulation/ichip8.hpp>
#include <emulation/chip8realcorebase.hpp>
#include <chiplet/octocompiler.hpp>

#include <raylib.h>

class Debugger
{
public:
    using ExecMode = emu::GenericCpu::ExecMode;
    using RegPack = emu::GenericCpu::RegisterPack;
    Debugger() = default;

    void setExecMode(ExecMode mode);
    void updateCore(emu::IChip8Emulator* core);
    void captureStates();
    void render(Font& font, std::function<void(Rectangle,int)> drawScreen);
    void updateOctoBreakpoints(const emu::OctoCompiler& compiler);
    bool supportsStepOver() const;
    bool isControllingChip8() const { return _backend == nullptr || _visibleCpu == CHIP8_CORE; }
private:
    void showInstructions(emu::GenericCpu& cpu, Font& font, const int lineSpacing);
    void showGenericRegs(emu::GenericCpu& cpu, const RegPack& regs, const RegPack& oldRegs, Font& font, const int lineSpacing, const Vector2& pos) const;
    const std::vector<std::pair<uint32_t,std::string>>& disassembleNLinesBackwardsGeneric(emu::GenericCpu& cpu, uint32_t addr, int n);
    void toggleBreakpoint(emu::GenericCpu& cpu, uint32_t address);
    enum Core { CHIP8_CORE, BACKEND_CORE };
    emu::IChip8Emulator* _core{nullptr};
    emu::Chip8RealCoreBase* _realCore{nullptr};
    emu::GenericCpu* _backend{nullptr};
    Core _visibleCpu{CHIP8_CORE};
    int _instructionOffset[2]{};
    int _activeInstructionsTab{0};
    bool _memViewFollow{true};
    RegPack _chip8State;
    RegPack _chip8StateBackup;
    RegPack _backendState;
    RegPack _backendStateBackup;
    std::vector<uint16_t> _chip8StackBackup;
    std::vector<uint8_t> _memBackup;
};

