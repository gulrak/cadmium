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
#include <fmt/format.h>
#include <ghc/bit.hpp>
#include <stylemanager.hpp>
#include "debugger.hpp"
#include <emulation/logger.hpp>

void Debugger::setExecMode(ExecMode mode)
{
    _core->setExecMode(mode);
}

void Debugger::updateCore(emu::IEmulationCore* core)
{
    _core = core;
    if (_core) {
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

static std::pair<const char*, bool> formatStackElement(const emu::GenericCpu::StackContent& stack, size_t index, const std::vector<uint8_t>& backupData)
{
    static char buffer[64];
    if(stack.content.empty() || stack.entrySize <= 0 || (index + 1) * stack.entrySize > stack.content.size()) {
        buffer[0] = 0;
        return {buffer, false};
    }
    auto offset = stack.stackDirection == emu::GenericCpu::eUPWARDS ? index * stack.entrySize : stack.content.size() - (index + 1) * stack.entrySize;
    const uint8_t* entry = &stack.content[offset];
    // the backup can be from a time with a different stack size, then nothing counts as changed
    const uint8_t* backup = offset + stack.entrySize <= backupData.size() ? backupData.data() + offset : entry;
    uint32_t value = 0, oldValue = 0;
    int digits = 0;
    switch(stack.entrySize) {
        case 1:
            value = readStackEntry<uint8_t>(entry, stack.endianness);
            oldValue = readStackEntry<uint8_t>(backup, stack.endianness);
            digits = 2;
            break;
        case 2:
            value = readStackEntry<uint16_t>(entry, stack.endianness);
            oldValue = readStackEntry<uint16_t>(backup, stack.endianness);
            digits = 4;
            break;
        case 4:
            value = readStackEntry<uint32_t>(entry, stack.endianness);
            oldValue = readStackEntry<uint32_t>(backup, stack.endianness);
            digits = 6;
            break;
        default:
            buffer[0] = 0;
            return {buffer, false};
    }
    *fmt::format_to_n(buffer, sizeof(buffer) - 1, "{:0{}X}", value, digits).out = 0;
    const bool changed = value != oldValue;
    return {buffer, changed};
}

void Debugger::render(Font& font, const std::function<void(Rectangle,int)>& drawScreen)
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
    SetNextWidth(270 + 2);
    Begin();
    SetSpacing(0);
    BeginPanel("Video", {1, 0});
    {
        auto height = (float)(megaChipVideo ? 192 : 148) + 6;
        video = {GetCurrentPos().x, GetCurrentPos().y, (float)270, (float)height};
        Space(int(height));
    }
    EndPanel();
    drawScreen(video, debugScale);
    if(_core->numberOfExecutionUnits() > 1) {
        if(_realCore->hasBackendStopped())
            _activeInstructionsTab = 1;
    }
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
    if (BeginTab("Breakpoints", {5, 0})) {
        showBreakpoints(font, lineSpacing);
        EndTab();
    }
    EndTabView();
    End();
    // Select CHIP-8 side-by-side or backend-CPU top/bottom display
    auto* visibleUnit = _core->executionUnit(_visibleExecUnit);
    if(!visibleUnit || dynamic_cast<emu::IChip8Emulator*>(visibleUnit)) {
        auto& unit = visibleUnit ? *visibleUnit : *_core->focussedExecutionUnit();
        SetNextWidth(44);
        BeginPanel("Regs");
        {
            auto pos = GetCurrentPos();
            auto area = GetContentAvailable();
            Space(area.height);
            showGenericRegs(unit, _cpuStates[_visibleExecUnit], _cpuStatesBackup[_visibleExecUnit], font, lineSpacing, pos);
        }
        EndPanel();
        SetNextWidth(44);
        BeginPanel("Stack");
        {
            auto pos = GetCurrentPos();
            auto area = GetContentAvailable();
            Space(area.height);
            auto stack = unit.stack();
            for (unsigned i = 0; i < unit.stackSize(); ++i) {
                auto element = formatStackElement(stack, i, _stackBackup[_visibleExecUnit]);
                DrawTextEx(font, TextFormat("%X %s", i & 0xF, element.first), {pos.x, pos.y + i * lineSpacing}, 8, 0, element.second ? yellowCol : lightgrayCol);
            }
        }
        EndPanel();
    }
    else {
        SetNextWidth(44 + 44 - 1);
        showCpuPanels(*visibleUnit, font, lineSpacing);
    }
    static Vector2 memScroll{0,0};
    static uint8_t memPage{0};
    BeginPanel(memPage ? TextFormat("Memory [%02X....]", memPage) : "Memory", {0,0});
    {
        auto pos = GetCurrentPos();
        auto area = GetContentAvailable();
        GuiCheckBox({pos.x + area.width - 56, pos.y - 13, 10, 10}, "Follow", &_memViewFollow);
        pos.x += 6;
        pos.y -= lineSpacing / 2;
        SetStyle(DEFAULT, BORDER_WIDTH, 0);
        static auto lastExecMode = emu::GenericCpu::eRUNNING;
        if(_memViewFollow && chip8Core() && (_core->focussedExecutionUnit()->execMode() != emu::GenericCpu::ePAUSED || lastExecMode != emu::GenericCpu::ePAUSED))
            memScroll.y = -(float)(chip8Core()->getI() / 16) * lineSpacing;
        lastExecMode = _core->focussedExecutionUnit()->execMode();
        auto bytesPerLine = 16;
        BeginScrollPanel(area.height, {0,0,area.width-6, (float)(_core->memSize()/bytesPerLine + 1) * lineSpacing}, &memScroll);
#if 0
        auto addr = int(-memScroll.y / lineSpacing) * 8 - 8;
        memPage = addr < 0 ? 0 : addr >> 16;
        for (int i = 0; i < area.height/lineSpacing + 1; ++i) {
            if(addr + i * 8 >= 0 && addr + i * 8 < _core->memSize()) {
                DrawTextClipped(font, TextFormat("%06X", (addr + i * 8)), {pos.x, pos.y + i * lineSpacing}, lightgrayCol);
                for (int j = 0; j < 8; ++j) {
                    if (!showChipCPU || addr + i * 8 + j > _core->memSize() || _core->memory()[addr + i * 8 + j] == _memBackup[addr + i * 8 + j]) {
                        DrawTextClipped(font, TextFormat("%02X", _core->memory()[addr + i * 8 + j]), {pos.x + 42 + j * 16, pos.y + i * lineSpacing}, j & 1 ? lightgrayCol : grayCol);
                    }
                    else {
                        DrawTextClipped(font, TextFormat("%02X", _core->memory()[addr + i * 8 + j]), {pos.x + 42 + j * 16, pos.y + i * lineSpacing}, j & 1 ? yellowCol : brownCol);
                    }
                }
            }
        }
#else
        auto addr = int(-memScroll.y / lineSpacing) * bytesPerLine - bytesPerLine;
        memPage = addr < 0 ? 0 : addr >> 16;
        for (int i = 0; i < area.height/lineSpacing + 1; ++i) {
            if(addr + i * bytesPerLine >= 0 && addr + i * bytesPerLine < _core->memSize()) {
                DrawTextClipped(font, TextFormat("%06X", (addr + i * bytesPerLine)), {pos.x - 1, pos.y + i * lineSpacing}, lightgrayCol);
                for (int j = 0; j < bytesPerLine; ++j) {
                    auto offset = j >= 8 ? 49.0f : 43.0f;
                    if (!showChipCPU || addr + i * bytesPerLine + j > _core->memSize() || _core->memory()[addr + i * bytesPerLine + j] == _memBackup[addr + i * bytesPerLine + j]) {
                        DrawTextClipped(font, TextFormat("%02X", _core->memory()[addr + i * bytesPerLine + j]), {pos.x + offset + j * 14, pos.y + i * lineSpacing}, j & 1 ? lightgrayCol : grayCol);
                    }
                    else {
                        DrawTextClipped(font, TextFormat("%02X", _core->memory()[addr + i * bytesPerLine + j]), {pos.x + offset + j * 14, pos.y + i * lineSpacing}, j & 1 ? yellowCol : brownCol);
                    }
                }
            }
        }
#endif
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
    auto lightgrayCol = /*StyleManager::getStyleColor(Style::TEXT_COLOR_FOCUSED);//*/ StyleManager::mappedColor(LIGHTGRAY);
    auto yellowCol = StyleManager::mappedColor(YELLOW);
    auto area = GetContentAvailable();
    Space(area.height);
    bool mouseInPanel = false;
    auto& instructionOffset = _instructionOffset[_visibleExecUnit];
    auto pc = cpu.getPC();
    // Some hardware backends (notably Saturn) have PCs wider than 16 bits.
    // Preserve the existing range for 16-bit cores and include the current
    // 64K address page for wider cores.
    const uint32_t maxAddress = pc | 0xffffU;
    if (!GuiIsLocked() && CheckCollisionPointRec(GetMousePosition(), GetLastWidgetRect())) {
        auto wheel = GetMouseWheelMoveV();
        mouseInPanel = true;
        if (std::fabs(wheel.y) >= 0.5f) {
            if (instructionOffset < 0) {
                instructionOffset = pc;
            }
            int step = 0;
            if (wheel.y >= 0.5f)
                step = 2;
            else if (wheel.y <= 0.5f)
                step = -2;
            instructionOffset = std::clamp(instructionOffset - step, 0, &cpu == _backend ? static_cast<int>(maxAddress) : 4096 - 9 * 2);
        }
    }
    auto dialect = Dialect::eCHIP8OCTO;
    switch (cpu.cpuID()) {
        case 1802: dialect = Dialect::eCDP1802; break;
        case 6800: dialect = Dialect::eM6800;
        default: break;
    }
    setDialect(dialect);
    auto visibleInstructions = int(area.height / lineSpacing);
    auto extraLines = visibleInstructions / 2 + 1;
    auto insOff = instructionOffset >= 0 ? instructionOffset : pc;
    auto yposPC = area.y + int(area.height / 2) - 4;
    const auto& prefix = disassembleNLinesBackwardsGeneric(cpu, insOff, extraLines);
    BeginClipping({area.x - 4, area.y, area.width + 8, area.height});
    auto pcColor = cpu.inErrorState() ? RED : StyleManager::getStyleColor(gui::Style::BORDER_COLOR_NORMAL);
    bool inIf = false;
    auto disassemblyOffset = cpu.disassemblyPrefixSize();
    for (int i = 0; i < extraLines && i < prefix.size(); ++i) {
        auto [addr, line] = prefix[prefix.size() - 1 - i];
        inIf = i < prefix.size() - 2 && prefix[prefix.size() - 2 - i].second.find(" if ") != std::string::npos;
        if (mouseInPanel && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {area.x, yposPC - (i + 1) * lineSpacing, area.width, 8}))
            toggleBreakpoint(cpu, addr);
        const auto* bpi = cpu.findBreakpoint(addr);
        if (inIf)
            line.insert(_backend ? 12 : 16, "  ");
        if (pc == addr)
            DrawRectangleClipped(area.x - 5, yposPC - (i + 1) * lineSpacing - 2, area.width + 10, 11, pcColor);
        if (line.size() >= disassemblyOffset) {
            line[disassemblyOffset - 1] = 0;
            DrawTextClipped(font, line.c_str(), {area.x, yposPC - (i + 1) * lineSpacing}, lightgrayCol);
            drawHighlightedTextLine(font, line.data() + disassemblyOffset, line.data() + disassemblyOffset, line.data() + line.size(), {area.x + disassemblyOffset * COLUMN_WIDTH, yposPC - (i + 1) * lineSpacing}, area.width, 0, 10);
        }
        // DrawTextClipped(font, line.c_str(), {area.x, yposPC - (i+1)*lineSpacing}, pc == addr ? pcColor : lightgrayCol);
        if (bpi)
            GuiDrawIcon(bpi->isEnabled ? ICON_BREAKPOINT : ICON_BREAKPOINT_OFF, area.x + 24, yposPC - (i + 1) * lineSpacing - 5, 1, RED);
    }
    inIf = !prefix.empty() && prefix.back().second.find(" if ") != std::string::npos;
    uint32_t addr = insOff;
    for (int i = 0; i <= extraLines && addr <= maxAddress; ++i) {
        if (mouseInPanel && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), {area.x, yposPC + i * lineSpacing, area.width, 8}))
            toggleBreakpoint(cpu, addr);
        int bytes = 0;
        auto line = cpu.disassembleInstructionWithBytes(addr, &bytes);
        const auto* bpi = cpu.findBreakpoint(addr);
        if (inIf)
            line.insert(_backend ? 12 : 16, "  ");
        if (pc == addr)
            DrawRectangleClipped(area.x - 5, yposPC + i * lineSpacing - 2, area.width + 10, 11, pcColor);
        inIf = line.find(" if ") != std::string::npos;
        if (line.size() >= disassemblyOffset) {
            line[disassemblyOffset - 1] = 0;
            DrawTextClipped(font, line.c_str(), {area.x, yposPC + i * lineSpacing}, lightgrayCol);
            drawHighlightedTextLine(font, line.c_str() + disassemblyOffset, line.c_str() + disassemblyOffset, line.data() + line.size(), {area.x + disassemblyOffset * COLUMN_WIDTH, yposPC + i * lineSpacing}, area.width, 0, 10);
        }
        // DrawTextClipped(font, line.c_str(), {area.x, yposPC + i * lineSpacing}, pc == addr ? pcColor : lightgrayCol);
        if (bpi)
            GuiDrawIcon(bpi->isEnabled ? ICON_BREAKPOINT : ICON_BREAKPOINT_OFF, area.x + 24, yposPC + i * lineSpacing - 5, 1, RED);
        addr += bytes;
    }
    EndClipping();
}

void Debugger::showBreakpoints(Font& font, const int lineSpacing)
{
    using namespace gui;
    static Vector2 scroll{0, 0};
    auto area = GetContentAvailable();
    Space(2);
    BeginTableView(area.height - (10 * 14.0f + 6), 3, &scroll);
    SetRowHeight(11);
    int count = -1;
    bool foundTriggeredBP = false;
    for (const auto& [address, bpinfo] : _breakpointCache) {
        auto execUnit = _core->executionUnit(bpinfo->unit);
        auto isChip8 = dynamic_cast<emu::IChip8Emulator*>(execUnit) != nullptr;
        ++count;
        //if (bpinfo->isEnabled && _triggeredBreakpointAddress == address && execUnit->getPC() == address)
        //    _selectedBreakpoint = count;
        TableNextRow(11.0f, count == _selectedBreakpoint ? StyleManager::getStyleColor(gui::Style::BORDER_COLOR_NORMAL): Color{0,0,0,0});
        TableNextColumn(24, [&](Rectangle rect){
            GuiDrawIcon(address == _triggeredBreakpointAddress ? ICON_ARROW_RIGHT_FILL_SMALL : ICON_NONE, rect.x - 2, rect.y - 3, 1, YELLOW);
            if (address == _triggeredBreakpointAddress && execUnit->getPC() == address)
                foundTriggeredBP = true;
            if (!GuiIsLocked() && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), rect)) {
                bpinfo->isEnabled = !bpinfo->isEnabled;
                selectBreakpoint(count);
            }
            GuiDrawIcon(bpinfo->isEnabled ? ICON_BREAKPOINT : ICON_BREAKPOINT_OFF, rect.x + 6, rect.y - 3, 1, RED);
        });
        TableNextColumn(32, [&](Rectangle rect){
            if (!GuiIsLocked() && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), rect)) {
                selectBreakpoint(count);
            }
            DrawTextClipped(font, fmt::format("{:04x}", address).c_str(), {rect.x + 2, rect.y + 2}, WHITE);
        });
        TableNextColumn(area.width - 50, [&](Rectangle rect){
            if (!GuiIsLocked() && IsMouseButtonPressed(0) && CheckCollisionPointRec(GetMousePosition(), rect)) {
                selectBreakpoint(count);
            }
            DrawTextClipped(font, fmt::format("{} breakpoint", isChip8 ? "CHIP-8" : execUnit->name()).c_str(), {rect.x + 2, rect.y + 2}, WHITE);
        });
    }
    if (!foundTriggeredBP) {
        setBreakpointTriggered(false);
    }
    EndTableView();
    SetRowHeight(14);
    if (_selectedBreakpoint >= 0 && _selectedBreakpoint < _breakpointCache.size()) {
        auto [address, bpinfo] = _breakpointCache[_selectedBreakpoint];
        auto execUnit = _core->executionUnit(bpinfo->unit);
        auto isChip8 = dynamic_cast<emu::IChip8Emulator*>(execUnit) != nullptr;
        Label("Breakpoint Config:");
        BeginColumns();
        auto colNorm = GetStyle(LABEL, TEXT_COLOR_NORMAL);
        auto colHigh = GetStyle(LABEL, TEXT_COLOR_FOCUSED);
        SetNextWidth(9*6); Label("Address:");
        SetStyle(LABEL, TEXT_COLOR_NORMAL, colHigh);
        SetNextWidth(6*6); Label(fmt::format("{:04X}", address).c_str());
        SetStyle(LABEL, TEXT_COLOR_NORMAL, colNorm);
        SetNextWidth(6*6); Label("Unit:");
        SetStyle(LABEL, TEXT_COLOR_NORMAL, colHigh);
        Label(fmt::format("{}", isChip8 ? "CHIP-8" : execUnit->name()).c_str());
        SetStyle(LABEL, TEXT_COLOR_NORMAL, colNorm);
        EndColumns();
        Label("Break Condition:");
        auto colTBnorm = GetStyle(TEXTBOX, TEXT_COLOR_NORMAL);
        auto colTBfocus = GetStyle(TEXTBOX, TEXT_COLOR_FOCUSED);
        if (!bpinfo->condition.empty() && !bpinfo->conditionExpr.first) {
            SetStyle(TEXTBOX, TEXT_COLOR_NORMAL, ColorToInt(RED));
            SetStyle(TEXTBOX, TEXT_COLOR_PRESSED, ColorToInt(RED));
        }
        auto prevCondition = bpinfo->condition;
        if (TextBox(bpinfo->condition, 256) || prevCondition != bpinfo->condition) {
            bpinfo->conditionExpr = execUnit->parseExpression(bpinfo->condition);
            DEBUG_LOG("Parsing breakpoint condition result: {}", bpinfo->conditionExpr.first ? "Okay" : bpinfo->conditionExpr.second.c_str());
        }
        SetStyle(TEXTBOX, TEXT_COLOR_NORMAL, colTBnorm);
        SetStyle(TEXTBOX, TEXT_COLOR_PRESSED, colTBfocus);

        GuiDisable();
        Label("Log Format:");
        TextBox(bpinfo->label, 256);

        Label("Log Condition:");
        if (!bpinfo->logCondition.empty() && !bpinfo->logConditionExpr.first) {
            SetStyle(TEXTBOX, TEXT_COLOR_NORMAL, ColorToInt(RED));
            SetStyle(TEXTBOX, TEXT_COLOR_PRESSED, ColorToInt(RED));
        }
        if (TextBox(bpinfo->logCondition, 256)) {
            bpinfo->logConditionExpr = execUnit->parseExpression(bpinfo->logCondition);
            DEBUG_LOG("Parsing log condition result: {}", bpinfo->logConditionExpr.first ? "Okay" : bpinfo->logConditionExpr.second.c_str());
        }
        SetStyle(TEXTBOX, TEXT_COLOR_NORMAL, colTBnorm);
        SetStyle(TEXTBOX, TEXT_COLOR_PRESSED, colTBfocus);

        Label("Hit Count:");
        TextBox(_bpHitCount, 64);
        GuiEnable();
    }
    Space(GetContentAvailable().height);
}

void Debugger::selectBreakpoint(int idx)
{
    if (idx < 0 || idx >= _breakpointCache.size()) {
        _selectedBreakpoint = -1;
        _bpHitCount = "";
        return;
    }
    _selectedBreakpoint = idx;
    _bpHitCount = fmt::format("{}", _breakpointCache[idx].second->numHits);
}

void Debugger::showGenericRegs(emu::GenericCpu& cpu, const RegPack& regs, const RegPack& oldRegs, Font& font, const int lineSpacing, const Vector2& pos) const
{
    using namespace gui;
    auto lightgrayCol = StyleManager::getStyleColor(Style::TEXT_COLOR_FOCUSED);  // StyleManager::mappedColor(LIGHTGRAY);
    auto yellowCol = StyleManager::mappedColor(YELLOW);
    int i, line = 0, lastSize = 0;
    for (i = 0; i < cpu.numRegisters(); ++i, ++line) {
        const auto& reg = regs[i];
        if (i && reg.size != lastSize)
            ++line;
        auto col = reg.value == oldRegs[i].value ? lightgrayCol : yellowCol;
        switch (reg.size) {
            case 1:
            case 4:
                DrawTextEx(font, TextFormat("%2s  %X", cpu.registerNames()[i].c_str(), static_cast<unsigned>(reg.value)), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            case 8:
                DrawTextEx(font, TextFormat("%2s  %02X", cpu.registerNames()[i].c_str(), static_cast<unsigned>(reg.value)), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            case 12:
                DrawTextEx(font, TextFormat("%2s  %03X", cpu.registerNames()[i].c_str(), static_cast<unsigned>(reg.value)), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            case 16:
                if (cpu.registerNames()[i].size() == 1)
                    DrawTextEx(font, TextFormat("%s %04X", cpu.registerNames()[i].c_str(), static_cast<unsigned>(reg.value)), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                else {
                    DrawTextEx(font, TextFormat("%2s ", cpu.registerNames()[i].c_str()), {pos.x, pos.y + line++ * lineSpacing}, 8, 0, col);
                    DrawTextEx(font, TextFormat("  %04X", static_cast<unsigned>(reg.value)), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                }
                break;
            case 24:
                DrawTextEx(font, TextFormat("%2s ", cpu.registerNames()[i].c_str()), {pos.x, pos.y + line++ * lineSpacing}, 8, 0, col);
                DrawTextEx(font, TextFormat("%06X", static_cast<unsigned>(reg.value)), {pos.x, pos.y + line * lineSpacing}, 8, 0, col);
                break;
            default:
                DrawTextEx(font, TextFormat("%2s %X", cpu.registerNames()[i].c_str(), static_cast<unsigned>(reg.value)), {pos.x, pos.y + line * lineSpacing}, 8, 0, MAGENTA);
                break;
        }
        lastSize = reg.size;
    }
    ++line;
    // DrawTextEx(font, TextFormat("Scr: %s", rcb->isDisplayEnabled() ? "ON" : "OFF"), {pos.x, pos.y + line * lineSpacing}, 8, 0, LIGHTGRAY);
}

// Layout of the registers of a backend CPU in blocks of registers that are either narrow (two or more share
// a line) or wide (one per line, values that are wider than the panel get split into chunks of 8 or 4
// digits). Returns the number of lines used.
int Debugger::showCpuRegisters(emu::GenericCpu& cpu, const RegPack& regs, const RegPack& oldRegs, Font& font, const int lineSpacing, const Vector2& pos, const int maxChars) const
{
    using namespace gui;
    const auto normalCol = StyleManager::getStyleColor(Style::TEXT_COLOR_FOCUSED);
    const auto changedCol = StyleManager::mappedColor(YELLOW);
    const auto charWidth = MeasureTextEx(font, "0", 8, 0).x;
    const auto& names = cpu.registerNames();
    const auto count = std::min({cpu.numRegisters(), names.size(), regs.size()});
    auto digitsOf = [&regs](size_t i) { return std::max(1, (static_cast<int>(regs[i].size) + 3) / 4); };
    auto isNarrow = [&](size_t i) { return 2 * (static_cast<int>(names[i].size()) + 1 + digitsOf(i)) + 1 <= maxChars; };
    int line = 0;
    for (size_t first = 0; first < count;) {
        const bool narrow = isNarrow(first);
        size_t end = first;
        int nameWidth = 0, valueWidth = 0;
        for (; end < count && isNarrow(end) == narrow; ++end) {
            nameWidth = std::max(nameWidth, static_cast<int>(names[end].size()));
            valueWidth = std::max(valueWidth, digitsOf(end));
        }
        if (first)
            ++line;
        const int cellWidth = nameWidth + 1 + valueWidth;
        const int columns = narrow ? std::max(1, (maxChars + 1) / (cellWidth + 1)) : 1;
        for (size_t i = first; i < end; ++i) {
            const auto column = static_cast<int>(i - first) % columns;
            if (column == 0 && i != first)
                ++line;
            const auto col = i < oldRegs.size() && regs[i].value == oldRegs[i].value ? normalCol : changedCol;
            const auto value = fmt::format("{:0{}X}", regs[i].value, digitsOf(i));
            const int available = std::max(1, maxChars - nameWidth - 1);
            const int chunk = static_cast<int>(value.size()) <= available ? static_cast<int>(value.size()) : (available >= 8 ? 8 : (available >= 4 ? 4 : available));
            const float x = pos.x + static_cast<float>(column * (cellWidth + 1)) * charWidth;
            for (size_t offset = 0; offset < value.size(); offset += chunk) {
                if (offset)
                    ++line;
                auto text = fmt::format("{:<{}} {}", offset ? "" : names[i], nameWidth, value.substr(offset, chunk));
                DrawTextEx(font, text.c_str(), {x, pos.y + static_cast<float>(line * lineSpacing)}, 8, 0, col);
            }
        }
        ++line;
        first = end;
    }
    return line;
}

void Debugger::showCpuPanels(emu::GenericCpu& cpu, Font& font, const int lineSpacing)
{
    using namespace gui;
    const auto normalCol = StyleManager::getStyleColor(Style::TEXT_COLOR_FOCUSED);
    const auto lightgrayCol = StyleManager::mappedColor(LIGHTGRAY);
    const auto yellowCol = StyleManager::mappedColor(YELLOW);
    const auto charWidth = MeasureTextEx(font, "0", 8, 0).x;
    // the 1802 has no stack, but its X register points to the data its instructions work on
    enum class Detail { NONE, STACK, X_DATA };
    const auto detail = cpu.cpuID() == 1802 ? Detail::X_DATA : (cpu.stackSize() > 0 ? Detail::STACK : Detail::NONE);
    Begin();
    SetSpacing(0);
    BeginPanel(cpu.name().c_str(), {3, 4});
    {
        auto pos = GetCurrentPos();
        auto area = GetContentAvailable();
        const int maxChars = static_cast<int>(area.width / charWidth);
        const int lines = showCpuRegisters(cpu, _cpuStates[_visibleExecUnit], _cpuStatesBackup[_visibleExecUnit], font, lineSpacing, pos, maxChars);
        Space(detail == Detail::NONE ? static_cast<int>(area.height) : lines * lineSpacing);
    }
    EndPanel();
    if (detail != Detail::NONE) {
        BeginPanel(detail == Detail::STACK ? "Stack" : "X Data", {3, 4});
        auto pos = GetCurrentPos();
        auto area = GetContentAvailable();
        Space(area.height);
        const int maxLines = std::max(0, static_cast<int>(area.height) / lineSpacing);
        if (detail == Detail::STACK) {
            auto stack = cpu.stack();
            const int entries = std::min(static_cast<int>(cpu.stackSize()), maxLines);
            for (int i = 0; i < entries; ++i) {
                auto element = formatStackElement(stack, i, _stackBackup[_visibleExecUnit]);
                DrawTextEx(font, TextFormat("%X %s", i & 0xF, element.first), {pos.x, pos.y + i * lineSpacing}, 8, 0, element.second ? yellowCol : lightgrayCol);
            }
        }
        else {
            const auto rx = static_cast<uint32_t>(cpu.registerbyIndex(cpu.registerByName("X").value).value);
            for (int offset = 0; offset < maxLines; ++offset) {
                DrawTextEx(font, TextFormat("%02X: %02X", offset, cpu.readMemoryByte((rx + offset) & 0xffff)), {pos.x, pos.y + offset * lineSpacing}, 8, 0, normalCol);
            }
        }
        EndPanel();
    }
    End();
}

void Debugger::refreshBreakpoints()
{
    _breakpointCache.clear();
    for(size_t i = 0; i < _core->numberOfExecutionUnits(); ++i) {
        auto& unit = *_core->executionUnit(i);
        for (size_t bidx = 0; bidx < unit.numBreakpoints(); ++bidx) {
            _breakpointCache.push_back(unit.nthBreakpoint(bidx));
        }
    }
    std::ranges::sort(_breakpointCache,
        [](const std::pair<uint32_t, emu::GenericCpu::BreakpointInfo*>& a, const std::pair<uint32_t, emu::GenericCpu::BreakpointInfo*>& b) {
            return a.first < b.first;
        }
    );
    selectBreakpoint(_breakpointCache.empty() ? -1 : 0);
}

const std::vector<std::pair<uint32_t,std::string>>& Debugger::disassembleNLinesBackwardsGeneric(emu::GenericCpu& cpu, uint32_t addr, int n)
{
    static std::vector<std::pair<uint32_t,std::string>> disassembly;
    n *= 4;
    uint32_t start = n > addr ? 0 : addr - n;
    disassembly.clear();
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
    if (auto* bpi = cpu.findBreakpoint(address)) {
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            if(bpi->type != emu::GenericCpu::BreakpointInfo::eCODED)
                cpu.removeBreakpoint(address);
        }
        else {
            bpi->isEnabled = !bpi->isEnabled;
        }
    }
    else {
        cpu.setBreakpoint(address, {.label = fmt::format("BP@{:x}", address), .type = emu::GenericCpu::BreakpointInfo::eTRANSIENT, .isEnabled = true, .unit = static_cast<uint8_t>(_activeInstructionsTab)});
    }
    selectBreakpoint(-1);
    refreshBreakpoints();
}

void Debugger::updateOctoBreakpoints(const emu::OctoCompiler& compiler)
{
    if(auto core = chip8Core(); core) {
        for(uint32_t addr = 0; addr < std::min(core->memSize(), 65536); ++addr) {
            const auto bpn = compiler.breakpointForAddr(addr);
            if(!bpn.empty()) {
                auto label = std::string(bpn);
                auto compiled = core->parseExpression(label);
                if(compiled.first) {
                    core->setBreakpoint(addr, {.condition = label, .label = label, .type = emu::GenericCpu::BreakpointInfo::eCODED, .isEnabled = true, .conditionExpr = std::move(compiled)});
                }
                else {
                    core->setBreakpoint(addr, {.label = label, .type = emu::GenericCpu::BreakpointInfo::eCODED, .isEnabled = true});
                }
            }
            else {
                if (auto* bpi = core->findBreakpoint(addr); bpi && bpi->type == emu::GenericCpu::BreakpointInfo::eCODED)
                    core->removeBreakpoint(addr);
            }
        }
    }
    selectBreakpoint(-1);
    refreshBreakpoints();
}

bool Debugger::supportsStepOver() const
{
    return _core->focussedExecutionUnit()->cpuID() != 1802;
}

void Debugger::setBreakpointTriggered(bool triggered)
{
    _isBreakpointTriggered = triggered;
    if (triggered) {
        int count = -1;
        for (const auto& [address, bpinfo] : _breakpointCache) {
            auto execUnit = _core->executionUnit(bpinfo->unit);
            auto isChip8 = dynamic_cast<emu::IChip8Emulator*>(execUnit) != nullptr;
            ++count;
            if (bpinfo->isEnabled && execUnit->getPC() == address) {
                _triggeredBreakpointAddress = address;
                _selectedBreakpoint = count;
            }
        }
    }
    else {
        _triggeredBreakpointAddress = ~0;
    }
}
