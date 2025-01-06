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

#include <rlguipp/rlguipp.hpp>
#include <ghc/bit.hpp>
#include <stylemanager.hpp>
#include "debugger.hpp"

void Debugger::setExecMode(ExecMode mode)
{
    _core->setExecMode(mode);
}

void Debugger::updateCore(emu::IEmulationCore* core)
{
    _core = core;
    _realCore = dynamic_cast<emu::Chip8RealCoreBase*>(core);
    _backend = _realCore != nullptr ? &_realCore->getBackendCpu() : nullptr;
    _visibleExecUnit = 0;
    _activeInstructionsTab = 0;
    // ensure cached data has the correct size by actually forcing a capture
    _instructionOffset.resize(_core->numberOfExecutionUnits());
    _cpuStates.resize(_core->numberOfExecutionUnits());
    for(size_t i = 0; i < _core->numberOfExecutionUnits(); ++i) {
        _instructionOffset[i] = -1;
        _core->executionUnit(i)->fetchAllRegisters(_cpuStates[i]);
    }
    captureStates();
}

void Debugger::captureStates()
{
    auto unit = _core->executionUnit(0);
    _memBackup.resize(_core->memSize());
    std::memcpy(_memBackup.data(), _core->memory(), _core->memSize());
    _cpuStatesBackup.resize(_core->numberOfExecutionUnits());
    _stackBackup.resize(_core->numberOfExecutionUnits());
    for(size_t i = 0; i < _core->numberOfExecutionUnits(); ++i) {
        _core->executionUnit(i)->fetchAllRegisters(_cpuStatesBackup[i]);
        auto stack = _core->executionUnit(i)->stack();
        _stackBackup[i].assign(stack.content.begin(), stack.content.end());
    }
}

emu::IChip8Emulator* Debugger::chip8Core()
{
    return dynamic_cast<emu::IChip8Emulator*>(_core->executionUnit(0));
}

template<typename T>
T readStackEntry(const uint8_t* address, emu::GenericCpu::Endianness endianness)
{
    switch(endianness) {
        case emu::GenericCpu::eNATIVE:
            return *reinterpret_cast<const T*>(address);
        case emu::GenericCpu::eBIG: {
            T result = 0;
            for (int i = 0; i < sizeof(T); ++i) {
                result |= static_cast<T>(address[i]) << (8 * (sizeof(T) - i - 1));
            }
            return result;
        }
        case emu::GenericCpu::eLITTLE: {
            T result = 0;
            for (int i = 0; i < sizeof(T); ++i) {
                result |= static_cast<T>(address[i]) << (8 * i);
            }
            return result;
        }
    }
    return 0;
}

static std::pair<const char*, bool> formatStackElement(const emu::GenericCpu::StackContent& stack, size_t index, const uint8_t* backup)
{
    static char buffer[64];
    if(stack.content.empty() || !stack.entrySize || index * stack.entrySize > stack.content.size()) {
        buffer[0] = 0;
        return {buffer, false};
    }
    auto offset = stack.stackDirection == emu::GenericCpu::eUPWARDS ? index * stack.entrySize : stack.content.size() - (index + 1) * stack.entrySize;
    const uint8_t* entry = &stack.content[offset];
    backup += offset;
    bool changed = false;
    switch(stack.entrySize) {
        case 1: {
            auto val = readStackEntry<uint8_t>(entry, stack.endianness);
            changed = val != readStackEntry<uint8_t>(backup, stack.endianness);
            fmt::format_to_n(buffer, 64, "{:02X}\0", val); break;
        }
        case 2: {
            auto val = readStackEntry<uint16_t>(entry, stack.endianness);
            changed = val != readStackEntry<uint16_t>(backup, stack.endianness);
            fmt::format_to_n(buffer, 64, "{:04X}\0", val); break;
        }
        case 4: {
            auto val = readStackEntry<uint32_t>(entry, stack.endianness);
            changed = val != readStackEntry<uint32_t>(backup, stack.endianness);
            fmt::format_to_n(buffer, 64, "{:06X}\0", val); break;
        }
        default: buffer[0] = 0;
    }
    return {buffer, changed};
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
    auto grayCol = StyleManager::mappedColor(GRAY);
    auto lightgrayCol = StyleManager::mappedColor(LIGHTGRAY);
    auto yellowCol = StyleManager::mappedColor(YELLOW);
    auto brownCol = StyleManager::mappedColor({ 203, 199, 0, 255 });
    for(size_t i = 0; i < _core->numberOfExecutionUnits(); ++i) {
        if(_core->executionUnit(i)->execMode() != emu::GenericCpu::ePAUSED) {
            _instructionOffset[i] = -1;
        }
        _core->executionUnit(i)->fetchAllRegisters(_cpuStates[i]);
    }
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
    if(_core->numberOfExecutionUnits() > 1) {
        if(_realCore->hasBackendStopped())
            _activeInstructionsTab = 1;
        BeginTabView(&_activeInstructionsTab);
        for(size_t i = 0; i < _core->numberOfExecutionUnits(); ++i) {
            if (auto exeUnit = _core->executionUnit(i); dynamic_cast<emu::IChip8Emulator*>(exeUnit)) {
                if(BeginTab("Instructions", {5, 0})) {
                    _visibleExecUnit = i;
                    _core->setFocussedExecutionUnit(exeUnit);
                    showInstructions(*exeUnit, font, lineSpacing);
                    EndTab();
                }
            }
            else {
                if(BeginTab(exeUnit->name().c_str(), {5,0})) {
                    _visibleExecUnit = i;
                    _core->setFocussedExecutionUnit(exeUnit);
                    showInstructions(*exeUnit, font, lineSpacing);
                    EndTab();
                }
            }
        }
        EndTabView();
    }
    else {
        BeginPanel("Instructions", {5, 0});
        _visibleExecUnit = 0;
        showInstructions(*_core->focussedExecutionUnit(), font, lineSpacing);
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
        showGenericRegs(*_core->focussedExecutionUnit(), _cpuStates[_visibleExecUnit], _cpuStatesBackup[_visibleExecUnit], font, lineSpacing, pos);
    }
    EndPanel();
    SetNextWidth(44);
    if(!_realCore || _realCore->hybridChipMode()) {
        BeginPanel("Stack");
        {
            auto pos = GetCurrentPos();
            auto area = GetContentAvailable();
            pos.x += 0;
            Space(area.height);
            auto stack = _core->executionUnit(_visibleExecUnit)->stack();
            for (int i = 0; i < _core->executionUnit(_visibleExecUnit)->stackSize(); ++i) {
                auto element = formatStackElement(stack, i, _stackBackup[_visibleExecUnit].data());
                DrawTextEx(font, TextFormat("%X:%s", i & 0xF, element.first), {pos.x, pos.y + i * lineSpacing}, 8, 0, element.second ? yellowCol : lightgrayCol);
            }
        }
    }
    else {
        BeginPanel("X Data");
        {
            auto pos = GetCurrentPos();
            auto area = GetContentAvailable();
            pos.x += 0;
            Space(area.height);
            auto x = _backend->registerByName("X").value;
            auto rx = _backend->registerbyIndex(x).value;
            auto col = StyleManager::getStyleColor(Style::TEXT_COLOR_FOCUSED);
            for(uint16_t offset = 0; offset < 36; ++offset) {
                DrawTextEx(font, TextFormat("%02X: %02X", offset, _backend->readMemoryByte((rx + offset) & 0xffff)), {pos.x, pos.y + offset * lineSpacing}, 8, 0, col);
            }
        }
    }
    EndPanel();
    static Vector2 memScroll{0,0};
    static uint8_t memPage{0};
    BeginPanel(memPage ? TextFormat("Memory [%02X....]", memPage) : "Memory", {0,0});
    {
        auto pos = GetCurrentPos();
        auto area = GetContentAvailable();
        GuiCheckBox({pos.x + 108, pos.y - 13, 10, 10}, "Follow", &_memViewFollow);
        pos.x += 4;
        pos.y -= lineSpacing / 2;
        SetStyle(DEFAULT, BORDER_WIDTH, 0);
        static auto lastExecMode = emu::GenericCpu::eRUNNING;
        if(_memViewFollow && chip8Core() && (_core->focussedExecutionUnit()->execMode() != emu::GenericCpu::ePAUSED || lastExecMode != emu::GenericCpu::ePAUSED))
            memScroll.y = -(float)(chip8Core()->getI() / 8) * lineSpacing;
        lastExecMode = _core->focussedExecutionUnit()->execMode();
        BeginScrollPanel(area.height, {0,0,area.width-6, (float)(_core->memSize()/8 + 1) * lineSpacing}, &memScroll);
        auto addr = int(-memScroll.y / lineSpacing) * 8 - 8;
        memPage = addr < 0 ? 0 : addr >> 16;
        for (int i = 0; i < area.height/lineSpacing + 1; ++i) {
            if(addr + i * 8 >= 0 && addr + i * 8 < _core->memSize()) {
                DrawTextEx(font, TextFormat("%04X", (addr + i * 8) & 0xFFFF), {pos.x, pos.y + i * lineSpacing}, 8, 0, lightgrayCol);
                for (int j = 0; j < 8; ++j) {
                    if (!showChipCPU || addr + i * 8 + j > _core->memSize() || _core->memory()[addr + i * 8 + j] == _memBackup[addr + i * 8 + j]) {
                        DrawTextEx(font, TextFormat("%02X", _core->memory()[addr + i * 8 + j]), {pos.x + 30 + j * 16, pos.y + i * lineSpacing}, 8, 0, j & 1 ? lightgrayCol : grayCol);
                    }
                    else {
                        DrawTextEx(font, TextFormat("%02X", _core->memory()[addr + i * 8 + j]), {pos.x + 30 + j * 16, pos.y + i * lineSpacing}, 8, 0, j & 1 ? yellowCol : brownCol);
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
    auto lightgrayCol = StyleManager::getStyleColor(Style::TEXT_COLOR_FOCUSED);//StyleManager::mappedColor(LIGHTGRAY);
    auto yellowCol = StyleManager::mappedColor(YELLOW);
    auto area = GetContentAvailable();
    Space(area.height);
    bool mouseInPanel = false;
    auto& instructionOffset = _instructionOffset[_visibleExecUnit];
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
    auto pcColor = cpu.inErrorState() ? RED : yellowCol;
    bool inIf = false;
    for(int i = 0; i < extraLines && i < prefix.size(); ++i) {
        auto [addr, line] = prefix[prefix.size() - 1 - i];
        inIf = i < prefix.size()-2 && prefix[prefix.size() - 2 - i].second.find(" if ") != std::string::npos;
        if(mouseInPanel && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {area.x, yposPC - (i+1)*lineSpacing, area.width, 8}))
            toggleBreakpoint(cpu, addr);
        const auto* bpi = cpu.findBreakpoint(addr);
        if(inIf)
            line.insert(_backend ? 12 : 16, "  ");
        DrawTextEx(font, line.c_str(), {area.x, yposPC - (i+1)*lineSpacing}, 8, 0, pc == addr ? pcColor : lightgrayCol);
        if(bpi)
            GuiDrawIcon(ICON_BREAKPOINT, area.x + 24, yposPC - (i+1)*lineSpacing - 5, 1, RED);
    }
    inIf = !prefix.empty() && prefix.back().second.find(" if ") != std::string::npos;
    uint32_t addr = insOff;
    for (int i = 0; i <= extraLines && addr < /*cpu.memSize()*/ 0x10000; ++i) {
        if(mouseInPanel && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {area.x, yposPC + i * lineSpacing, area.width, 8}))
            toggleBreakpoint(cpu, addr);
        int bytes = 0;
        auto line = cpu.disassembleInstructionWithBytes(addr, &bytes);
        const auto* bpi = cpu.findBreakpoint(addr);
        if(inIf)
            line.insert(_backend ? 12 : 16, "  ");
        DrawTextEx(font, line.c_str(), {area.x, yposPC + i * lineSpacing}, 8, 0, pc == addr ? pcColor : lightgrayCol);
        if(bpi)
            GuiDrawIcon(ICON_BREAKPOINT, area.x + 24, yposPC + i * lineSpacing - 5, 1, RED);
        inIf = line.find(" if ") != std::string::npos;
        addr += bytes;
    }
    EndScissorMode();
}

void Debugger::showGenericRegs(emu::GenericCpu& cpu, const RegPack& regs, const RegPack& oldRegs, Font& font, const int lineSpacing, const Vector2& pos) const
{
    using namespace gui;
    auto lightgrayCol = StyleManager::getStyleColor(Style::TEXT_COLOR_FOCUSED);//StyleManager::mappedColor(LIGHTGRAY);
    auto yellowCol = StyleManager::mappedColor(YELLOW);
    int i, line = 0, lastSize = 0;
    for (i = 0; i < cpu.numRegisters(); ++i, ++line) {
        const auto& reg = regs[i];
        if(i && reg.size != lastSize)
            ++line;
        auto col = reg.value == oldRegs[i].value ? lightgrayCol : yellowCol;
        switch(reg.size) {
            case 1:
            case 4:
                DrawTextEx(font, TextFormat("%2s: %X", cpu.registerNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            case 8:
                DrawTextEx(font, TextFormat("%2s: %02X", cpu.registerNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            case 12:
                DrawTextEx(font, TextFormat("%2s: %03X", cpu.registerNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            case 16:
                DrawTextEx(font, TextFormat("%2s:%04X", cpu.registerNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            case 24:
                DrawTextEx(font, TextFormat("%2s:", cpu.registerNames()[i].c_str()), {pos.x, pos.y + line++ * lineSpacing}, 8, 0, col);
                DrawTextEx(font, TextFormat("%06X", reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            default:
                DrawTextEx(font, TextFormat("%2s:%X", cpu.registerNames()[i].c_str(), reg.value), {pos.x, pos.y + line * lineSpacing}, 8, 0, MAGENTA);
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
    if(auto core = chip8Core(); core) {
        for(uint32_t addr = 0; addr < std::min(core->memSize(), 65536); ++addr) {
            const auto* bpn = compiler.breakpointForAddr(addr);
            if(bpn)
                core->setBreakpoint(addr, {bpn, emu::GenericCpu::BreakpointInfo::eCODED, true});
            else {
                auto* bpi = core->findBreakpoint(addr);
                if(bpi && bpi->type == emu::GenericCpu::BreakpointInfo::eCODED)
                    core->removeBreakpoint(addr);
            }
        }
    }
}

bool Debugger::supportsStepOver() const
{
    return _core->focussedExecutionUnit()->cpuID() != 1802;
}
