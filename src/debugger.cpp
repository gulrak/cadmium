//---------------------------------------------------------------------------------------
// src/debugger.cpp
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

#include "icons.h"
#include <rlguipp/rlguipp.hpp>
#include "debugger.hpp"

void Debugger::setExecMode(ExecMode mode)
{
    if(!_realCore || _visibleCpu == CHIP8_CORE)
        _core->setExecMode(mode);
    else {
        _realCore->setBackendExecMode(mode);
    }
}

void Debugger::updateCore(emu::IChip8Emulator* core)
{
    _core = core;
    _realCore = dynamic_cast<emu::Chip8RealCoreBase*>(core);
    _backend = _realCore != nullptr ? &_realCore->getBackendCpu() : nullptr;
    _visibleCpu = CHIP8_CORE;
    _instructionOffset[CHIP8_CORE] = -1;
    _instructionOffset[BACKEND_CORE] = -1;
    // ensure cached data has the correct size by actually forcing a capture
    _core->fetchAllRegisters(_chip8State);
    if(_backend)
        _backend->fetchAllRegisters(_backendState);
    captureStates();
}

void Debugger::captureStates()
{
    _memBackup.resize(_core->memSize());
    std::memcpy(_memBackup.data(), _core->memory(), _core->memSize());
    _chip8StackBackup.resize(_core->stackSize());
    std::memcpy(_chip8StackBackup.data(), _core->getStackElements(), sizeof(uint16_t) * _core->stackSize());
    _core->fetchAllRegisters(_chip8StateBackup);
    if(_backend)
        _backend->fetchAllRegisters(_backendStateBackup);
}

void Debugger::render(Font& font, std::function<void(Rectangle,int)> drawScreen)
{
    using namespace gui;
    SetStyle(LISTVIEW, SCROLLBAR_WIDTH, 5);
    const int lineSpacing = 10;
    const int debugScale = 256 / _core->getCurrentScreenWidth();
    const bool megaChipVideo = _core->getMaxScreenHeight() == 192;
    Rectangle video;
    bool showChipCPU = true;
    if(_core->getExecMode() != emu::GenericCpu::ePAUSED) {
        _instructionOffset[CHIP8_CORE] = -1;
        _instructionOffset[BACKEND_CORE] = -1;
    }
    _core->fetchAllRegisters(_chip8State);
    if(_backend)
        _backend->fetchAllRegisters(_backendState);
    BeginColumns();
    SetSpacing(-1);
    SetNextWidth(256 + 2);
    Begin();
    SetSpacing(0);
    BeginPanel("Video", {1, 0});
    {
        video = {GetCurrentPos().x, GetCurrentPos().y, (float)256, (float)(megaChipVideo ? 192 : 128)};
        Space(megaChipVideo ? 192 + 1 : 128 + 1);
    }
    EndPanel();
    drawScreen(video, debugScale);
    static int activeTab = 0;
    if(_backend) {
        if(_realCore->hasBackendStopped())
            activeTab = 1;
        BeginTabView(&activeTab);
    }
    else {
        BeginPanel("Instructions", {5, 0});
    }
    if(!_backend || BeginTab("Instructions", {5, 0})) {
        _visibleCpu = CHIP8_CORE;
        showInstructions(*_core, font, lineSpacing);
        if(_backend)
            EndTab();
    }
    if(_backend) {
        //Color{3, 161, 127, 255}
        if(BeginTab(_backend->getName().c_str(), {5,0})) {
            _visibleCpu = BACKEND_CORE;
            showInstructions(*_backend, font, lineSpacing);
            EndTab();
        }
        EndTabView();
    }
    else {
        EndPanel();
    }
    End();
    SetNextWidth(50);
    BeginPanel("Regs");
    {
        auto pos = GetCurrentPos();
        auto area = GetContentAvailable();
        pos.x += 0;
        Space(area.height);
        if(_visibleCpu == CHIP8_CORE) {
            showGenericRegs(*_core, _chip8State, _chip8StateBackup, font, lineSpacing, pos);
        }
        else {
            showGenericRegs(*_backend, _backendState, _backendStateBackup, font, lineSpacing, pos);
        }
    }
    EndPanel();
    SetNextWidth(44);
    BeginPanel("Stack");
    {
        auto pos = GetCurrentPos();
        auto area = GetContentAvailable();
        pos.x += 0;
        Space(area.height);
        auto stackSize = _core->stackSize();
        const auto* stack = _core->getStackElements();
        bool fourDigitStack = !_core->isGenericEmulation() || _core->memSize() > 4096;
        for (int i = 0; i < stackSize; ++i) {
            DrawTextEx(font, fourDigitStack  ? TextFormat("%X:%04X", i, stack[i]) : TextFormat("%X: %03X", i, stack[i]), {pos.x, pos.y + i * lineSpacing}, 8, 0, stack[i] == _chip8StackBackup[i] ? LIGHTGRAY : YELLOW);
        }
    }
    EndPanel();
    static Vector2 memScroll{0,0};
    static uint8_t memPage{0};
    SetNextWidth(163);
    BeginPanel(memPage ? TextFormat("Memory [%02X....]", memPage) : "Memory", {0,0});
    {
        auto pos = GetCurrentPos();
        auto area = GetContentAvailable();
        pos.x += 4;
        pos.y -= lineSpacing / 2;
        SetStyle(DEFAULT, BORDER_WIDTH, 0);
        if(_core->getExecMode() != emu::GenericCpu::ePAUSED)
            memScroll.y = -(float)(_core->getI() / 8) * lineSpacing;
        BeginScrollPanel(area.height, {0,0,area.width-6, (float)(_core->memSize()/8 + 1) * lineSpacing}, &memScroll);
        auto addr = int(-memScroll.y / lineSpacing) * 8 - 8;
        memPage = addr < 0 ? 0 : addr >> 16;
        for (int i = 0; i < area.height/lineSpacing + 1; ++i) {
            if(addr + i * 8 >= 0 && addr + i * 8 < _core->memSize()) {
                DrawTextEx(font, TextFormat("%04X", (addr + i * 8) & 0xFFFF), {pos.x, pos.y + i * lineSpacing}, 8, 0, LIGHTGRAY);
                for (int j = 0; j < 8; ++j) {
                    if (!showChipCPU || _core->getI() + i * 8 + j > 65535 || _core->memory()[_core->getI() + i * 8 + j] == _memBackup[_core->getI() + i * 8 + j]) {
                        DrawTextEx(font, TextFormat("%02X", _core->memory()[addr + i * 8 + j]), {pos.x + 30 + j * 16, pos.y + i * lineSpacing}, 8, 0, j & 1 ? LIGHTGRAY : GRAY);
                    }
                    else {
                        DrawTextEx(font, TextFormat("%02X", _core->memory()[addr + i * 8 + j]), {pos.x + 30 + j * 16, pos.y + i * lineSpacing}, 8, 0, j & 1 ? YELLOW : BROWN);
                    }
                }
            }
        }
        EndScrollPanel();
        SetStyle(DEFAULT, BORDER_WIDTH, 1);
    }
    EndPanel();
    EndColumns();
    SetStyle(LISTVIEW, SCROLLBAR_WIDTH, 6);
}

void Debugger::showInstructions(emu::GenericCpu& cpu, Font& font, const int lineSpacing)
{
    using namespace gui;
    auto area = GetContentAvailable();
    Space(area.height);
    bool mouseInPanel = false;
    auto& instructionOffset = _instructionOffset[_visibleCpu];
    auto pc = cpu.getPC();
    if(!GuiIsLocked() && CheckCollisionPointRec(GetMousePosition(), GetLastWidgetRect())) {
        auto wheel = GetMouseWheelMoveV();
        mouseInPanel = true;
        if(std::fabs(wheel.y) >= 0.5f) {
            if(instructionOffset < 0) {
                instructionOffset = pc;
            }
            int step = 0;
            if(wheel.y >= 0.5f) step = 2;
            else if(wheel.y <= 0.5f) step = -2;
            instructionOffset = std::clamp(instructionOffset - step, 0, &cpu == _backend ? 0xFFFF : 4096 - 9*2);
        }
    }
    auto visibleInstructions = int(area.height / lineSpacing);
    auto extraLines = visibleInstructions / 2 + 1;
    auto insOff = instructionOffset >= 0 ? instructionOffset : pc;
    auto yposPC = area.y + int(area.height / 2) - 4;
    const auto& prefix = disassembleNLinesBackwardsGeneric(cpu, insOff, extraLines);
    BeginScissorMode(area.x, area.y, area.width, area.height);
    auto pcColor = cpu.inErrorState() ? RED : YELLOW;
    for(int i = 0; i < extraLines && i < prefix.size(); ++i) {
        if(mouseInPanel && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {area.x, yposPC - (i+1)*lineSpacing, area.width, 8}))
            toggleBreakpoint(cpu, prefix[prefix.size() - 1 - i].first);
        const auto* bpi = cpu.findBreakpoint(prefix[prefix.size() - 1 - i].first);
        DrawTextEx(font, prefix[prefix.size() - 1 - i].second.c_str(), {area.x, yposPC - (i+1)*lineSpacing}, 8, 0, pc == prefix[prefix.size() - 1 - i].first ? pcColor : LIGHTGRAY);
        if(bpi)
            GuiDrawIcon(ICON_BREAKPOINT, area.x + 24, yposPC - (i+1)*lineSpacing - 5, 1, RED);
    }
    bool inIf = !prefix.empty() && prefix.back().second.find(" if ") != std::string::npos;
    uint32_t addr = insOff;
    for (int i = 0; i <= extraLines && addr < /*cpu.memSize()*/ 0x10000; ++i) {
        if(mouseInPanel && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {area.x, yposPC + i * lineSpacing, area.width, 8}))
            toggleBreakpoint(cpu, addr);
        int bytes = 0;
        auto line = cpu.disassembleInstructionWithBytes(addr, &bytes);
        const auto* bpi = cpu.findBreakpoint(addr);
        if(inIf)
            line.insert(_backend ? 12 : 16, "  ");
        DrawTextEx(font, line.c_str(), {area.x, yposPC + i * lineSpacing}, 8, 0, pc == addr ? pcColor : LIGHTGRAY);
        if(bpi)
            GuiDrawIcon(ICON_BREAKPOINT, area.x + 24, yposPC + i * lineSpacing - 5, 1, RED);
        inIf = line.find(" if ") != std::string::npos;
        addr += bytes;
    }
    EndScissorMode();
}

void Debugger::showGenericRegs(emu::GenericCpu& cpu, const RegPack& regs, const RegPack& oldRegs, Font& font, const int lineSpacing, const Vector2& pos) const
{
    int i, line = 0, lastSize = 0;
    for (i = 0; i < cpu.getNumRegisters(); ++i, ++line) {
        const auto& reg = regs[i];
        if(i && reg.size != lastSize)
            ++line;
        auto col = reg.value == oldRegs[i].value ? LIGHTGRAY : YELLOW;
        switch(reg.size) {
            case 1:
            case 4:
                DrawTextEx(font, TextFormat("%2s: %X", cpu.getRegisterNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            case 8:
                DrawTextEx(font, TextFormat("%2s: %02X", cpu.getRegisterNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            case 12:
                DrawTextEx(font, TextFormat("%2s: %03X", cpu.getRegisterNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            case 16:
                DrawTextEx(font, TextFormat("%2s:%04X", cpu.getRegisterNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            default:
                DrawTextEx(font, TextFormat("%2s:%X", cpu.getRegisterNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, MAGENTA);
                break;
        }
        lastSize = reg.size;
    }
    ++line;
    //DrawTextEx(font, TextFormat("Scr: %s", rcb->isDisplayEnabled() ? "ON" : "OFF"), {pos.x, pos.y + line * lineSpacing}, 8, 0, LIGHTGRAY);
}

const std::vector<std::pair<uint32_t,std::string>>& Debugger::disassembleNLinesBackwardsGeneric(emu::GenericCpu& cpu, uint32_t addr, int n)
{
    static std::vector<std::pair<uint32_t,std::string>> disassembly;
    n *= 4;
    uint32_t start = n > addr ? 0 : addr - n;
    disassembly.clear();
    bool inIf = false;
    while (start < addr) {
        int bytes = 0;
        auto instruction = cpu.disassembleInstructionWithBytes(start, &bytes);
        disassembly.emplace_back(start, instruction);
        start += bytes;
    }
    return disassembly;
}

void Debugger::toggleBreakpoint(emu::GenericCpu& cpu, uint32_t address)
{
    auto* bpi = cpu.findBreakpoint(address);
    if(bpi) {
        if(bpi->type != emu::GenericCpu::BreakpointInfo::eCODED)
            cpu.removeBreakpoint(address);
    }
    else {
        cpu.setBreakpoint(address, {fmt::format("BP@{:x}", address), emu::GenericCpu::BreakpointInfo::eTRANSIENT, true});
    }
}

void Debugger::updateOctoBreakpoints(const emu::OctoCompiler& compiler)
{
    for(uint32_t addr = 0; addr < std::min(_core->memSize(), 65536); ++addr) {
        const auto* bpn = compiler.breakpointForAddr(addr);
        if(bpn)
            _core->setBreakpoint(addr, {bpn, emu::GenericCpu::BreakpointInfo::eCODED, true});
        else {
            auto* bpi = _core->findBreakpoint(addr);
            if(bpi && bpi->type == emu::GenericCpu::BreakpointInfo::eCODED)
                _core->removeBreakpoint(addr);
        }
    }
}

bool Debugger::supportsStepOver() const
{
    return _visibleCpu == CHIP8_CORE || !_backend || _backend->getCpuID() != 1802;
}
