//---------------------------------------------------------------------------------------
// src/editor.cpp
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
#include <GLFW/glfw3.h>
#include <rlguipp/rlguipp.hpp>
#include <cassert>
#include <cmath>
#include <editor.hpp>
#include <emulation/utility.hpp>
#include <emulation/octocompiler.hpp>
#include <ghc/utf8.hpp>

namespace utf8 = ghc::utf8;

std::unordered_set<std::string> Editor::_opcodes = {
    "!=", "&=", "+=", "-=", "-key", ":", ":=", ";", "<", "<<=", "<=", "=-", "==", ">", ">=", ">>=", "^=", "|=",
    "again", "audio", "bcd", "begin", "bighex", "buzzer", "clear", "delay", "else", "end", "hex", "hires", "if",
    "jump", "jump0", "key", "load", "loadflags", "loop", "lores", "native", "pitch", "plane", "random", "return",
    "save", "saveflags", "scroll-down", "scroll-left", "scroll-right", "scroll-up", "sprite", "then", "while"
};

std::unordered_set<std::string> Editor::_directives = {
    ":alias", ":assert", ":breakpoint", ":byte", ":calc", ":call", ":const", ":macro", ":monitor", ":next", ":org", ":pointer", ":proto", ":stringmode", ":unpack"
};

extern void copyClip(const char*);
extern const char* pasteClip();

static int getKeySymbol(int key)
{
#ifndef PLATFORM_WEB
    const char* keyName = glfwGetKeyName(key,0);
    return keyName ? *keyName : key;
#else
    return key >= KEY_A && key <= KEY_Z ? key - KEY_A + 'A' : key;
#endif
}

void Editor::cursorLeft(int steps)
{
    for (int i = 0; i < steps; ++i) {
        if (_cursorX > 0)
            --_cursorX;
        else if (_cursorY > 0)
            --_cursorY, _cursorX = lineLength(_cursorY);
    }
    _cursorVirtX = _cursorX;
    _cursorChanged = true;
}

void Editor::cursorRight(int steps)
{
    for (int i = 0; i < steps; ++i) {
        if (_cursorX < lineLength(_cursorY))
            ++_cursorX;
        else if (_cursorY + 1 < _lines.size())
            ++_cursorY, _cursorX = 0;
    }
    _cursorVirtX = _cursorX;
    _cursorChanged = true;
}

void Editor::cursorUp(int steps)
{
    for (int i = 0; i < steps; ++i) {
        if (_cursorY > 0)
            --_cursorY;
    }
    _cursorX = _cursorVirtX;
    if (_cursorX > lineLength(_cursorY))
        _cursorX = lineLength(_cursorY);
    _cursorChanged = true;
}

void Editor::cursorDown(int steps)
{
    for (int i = 0; i < steps; ++i) {
        if (_cursorY + 1 < _lines.size())
            ++_cursorY;
    }
    _cursorX = _cursorVirtX;
    if (_cursorX > lineLength(_cursorY))
        _cursorX = lineLength(_cursorY);
    _cursorChanged = true;
}

void Editor::cursorHome()
{
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
        _cursorX = _cursorY = _cursorVirtX = 0, _cursorChanged = true;
    else
        _cursorX = _cursorVirtX = 0, _cursorChanged = true;
}

void Editor::cursorEnd()
{
    if(IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        _cursorY = _lines.size() - 1;
        _cursorX = _cursorVirtX = lineLength(_cursorY);
        _cursorChanged = true;
    }
    else
        _cursorX = _cursorVirtX = lineLength(_cursorY), _cursorChanged = true;
}

char* Editor::pointerFromOffset(uint32_t offset)
{
    // find line from offset
    auto iter=std::upper_bound(_lines.begin(), _lines.end(), offset);
    if (iter!=_lines.begin()) {
        --iter;
    }
    offset -= *iter;
    return _text.data() + offset;
}

uint32_t Editor::offsetFromCursor()
{
    const char* text = _text.data() + _lines[_cursorY];
    const char* end = _text.data() + _text.size();
    unsigned i = 0;
    while(text < end && i < _cursorX && *text != '\n')
        utf8::fetchCodepoint(text, end), ++i;
    return text - _text.data();
}

std::pair<int,int> Editor::cursorFromOffset(uint32_t offset)
{
    int cx = 0, cy = 0;
    auto iter=std::upper_bound(_lines.begin(), _lines.end(), offset);
    if (iter!=_lines.begin()) {
        --iter;
    }
    cy = int(iter - _lines.begin());
    offset -= *iter;
    const char* text = lineStart(cy);
    const char* end = text + offset;
    while(text < end && *text != '\n')
        utf8::fetchCodepoint(text, end), ++cx;
    return {cx, cy};
}

int Editor::lineLength(uint32_t line)
{
    if(line >= _lines.size()) return 0;
    const char* text = _text.data() + _lines[line];
    const char* end = _text.data() + _text.size();
    int len = 0;
    while(text < end && *text != '\n')
        utf8::fetchCodepoint(text, end), ++len;
    return len;
}

void Editor::updateAlphaKeys()
{
    int key;
    for(auto& f : _alphabetKeys) f = 0;
    do {
        key = GetKeyPressed();
        if(key && key != GLFW_KEY_UNKNOWN) {
            int keySym = getKeySymbol(key);
            if (keySym >= 'A' && keySym <= 'Z') {
                _alphabetKeys[keySym - 'A'] = true;
                //TraceLog(LOG_INFO, "Alpha-Key pressed: %c", keySym);
            }
            else if (keySym >= 'a' && keySym <= 'z') {
                _alphabetKeys[keySym - 'a'] = true;
                //TraceLog(LOG_INFO, "Alpha-Key pressed: %c", keySym);
            }
        }
    }
    while(key);
}

void Editor::updateLineInfo(uint32_t fromLine)
{
    if (fromLine >= _lines.size())
        fromLine = 0;
    if (fromLine == 0)
        _longestLineSize = 0;
    const char* str = _text.data() + (fromLine ? _lines[fromLine] : 0);
    const char* end = _text.data() + _text.size();
    _lines.erase(_lines.begin() + fromLine, _lines.end());
    auto length = str - _text.data();
    auto lastOffset = length;
    _lines.push_back(length);
    while (str < end) {
        if (*str++ == '\n') {
            _lines.push_back(str - _text.data());
            length = _lines.back() - lastOffset;
            if(length > _longestLineSize)
                _longestLineSize = length;
            lastOffset = _lines.back();
        }
    }
}

void Editor::update()
{
    auto oldRepeat = _repeatTimer;
    ++_editId;
    _repeatTimer -= GetFrameTime();
    _isRepeat = (_repeatTimer <= 0 && oldRepeat > 0);
    _cursorChanged = false;
    updateAlphaKeys();
    bool altPressed = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    bool shiftPressed = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool ctrlPressed = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool superPressed = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool selectionChange = false;
/*
    if(CheckCollisionPointRec(GetMousePosition(), _rect)) {
        auto wheel = GetMouseWheelMoveV();
        if(std::fabs(wheel.y) >= 0.5f) {
            int step = 0;
            if(wheel.y >= 0.5f) step = 2;
            else if(wheel.y <= 0.5f) step = -2;
            _tosLine = std::clamp<int32_t>(_tosLine - step, 0u, _lines.size() > _visibleLines ? _lines.size() - _visibleLines : 0u);
        }
        //DrawRectangleLinesEx(GetLastWidgetRect(), 1, RED);
    }
*/
    if(IsMouseButtonDown(0) && CheckCollisionPointRec(GetMousePosition(), {_rect.x + 1, _rect.y + 1, _rect.width - 6, _rect.height - 6})) {
        if(_mouseDownInText || IsMouseButtonPressed(0)) {
            auto clickPos = GetMousePosition();
            auto cx = (clickPos.x - _rect.x - 6 * COLUMN_WIDTH) / COLUMN_WIDTH;
            auto cy = (clickPos.y - _rect.y - 4) / LINE_SIZE;
            if (cx > 0 && cy + _tosLine >= 0) {
                _cursorX = cx + _losCol;
                _cursorY = cy + _tosLine;
                if (_cursorY >= _lines.size())
                    _cursorY = _lines.size() - 1;
                if (_cursorX > lineLength(_cursorY))
                    _cursorX = lineLength(_cursorY);
                _cursorVirtX = _cursorX;
                _cursorChanged = true;
            }
        }
        if(IsMouseButtonPressed(0)) {
            _selectionEnd = _selectionStart = offsetFromCursor();
            _mouseDownInText = true;
            selectionChange = true;
        }
        else if(_mouseDownInText) {
            _selectionEnd = offsetFromCursor();
            selectionChange = true;
        }
    }
    if(IsMouseButtonUp(0))
        _mouseDownInText = false;
    if (isKeyActivated(KEY_UP)) {
        selectionChange |= cursorWrapper([this]() { cursorUp(); });
    }
    else if(isKeyActivated(KEY_DOWN)) {
        selectionChange |= cursorWrapper([this](){ cursorDown(); });
    }
    else if(isKeyActivated(KEY_LEFT)) {
        selectionChange |= cursorWrapper([this](){ cursorLeft(); });
    }
    else if(isKeyActivated(KEY_RIGHT)) {
        selectionChange |= cursorWrapper([this](){ cursorRight(); });
    }
    else if(isKeyActivated(KEY_PAGE_UP)) {
        selectionChange |= cursorWrapper([this](){ cursorUp(_visibleLines); });
    }
    else if(isKeyActivated(KEY_PAGE_DOWN)) {
        selectionChange |= cursorWrapper([this](){ cursorDown(_visibleLines); });
    }
    else if(IsKeyPressed(KEY_HOME)) {
        selectionChange |= cursorWrapper([this](){ cursorHome(); });
    }
    else if(IsKeyPressed(KEY_END)) {
        selectionChange |= cursorWrapper([this](){ cursorEnd(); });
    }
    else if(ctrlPressed && isAlphaPressed('Z')) {
        if(shiftPressed)
            redo();
        else
            undo();
    }
    else if(ctrlPressed && isAlphaPressed('C')) {
        auto [selStart, selEnd] = selection();
        SetClipboardTextX(std::string(_text.begin() + selStart, _text.begin() + selEnd).c_str());
    }
    else if(ctrlPressed && isAlphaPressed('V')) {
        insert(GetClipboardTextX());
    }
    else if(ctrlPressed && isAlphaPressed('X')) {
        auto [selStart, selEnd] = selection();
        SetClipboardTextX(std::string(_text.begin() + selStart, _text.begin() + selEnd).c_str());
        deleteSelectedText();
    }
    else if(ctrlPressed && isAlphaPressed('A')) {
        _selectionStart = 0;
        _selectionEnd = _text.size();
        selectionChange = true;
    }
    else if(IsKeyPressed(KEY_ESCAPE)) {
        _selectionStart = _selectionEnd = 0;
        selectionChange = true;
    }
    else if(isKeyActivated(KEY_BACKSPACE)) {
        if(_selectionStart != _selectionEnd) {
            deleteSelectedText();
        }
        else {
            auto end = offsetFromCursor();
            cursorLeft();
            auto start = offsetFromCursor();
            deleteText(start, end - start);
            updateLineInfo(_cursorY);
        }
    }
    else if(isKeyActivated(KEY_ENTER)) {
        insert(std::string(1, '\n'));
    }
    else {
        auto codepoint = GetCharPressed();
        if(codepoint >= 32 && codepoint < 255) {
            std::string s;
            utf8::append(s, codepoint);
            insert(s);
        }
    }
    if(_cursorChanged) {
        if (_visibleLines && _cursorY >= _tosLine + _visibleLines)
            _tosLine = _cursorY - _visibleLines + 1;
        else if (_visibleLines && _cursorY < _tosLine)
            _tosLine = _cursorY;
        if (_visibleCols && _cursorX >= _losCol + _visibleCols)
            _losCol = _cursorX - _visibleCols + 1;
        else if (_visibleCols && _cursorX < _losCol)
            _losCol = _cursorX;
        if(!selectionChange)
            _selectionStart = _selectionEnd = 0;
    }

    if(_undoStack.empty() || _undoStack.top().id == _editId) {
        _inactiveEditTimer = 0;
        _lastEditId = _editId;
    }
    else {
        _inactiveEditTimer += GetFrameTime();
        if(_inactiveEditTimer > INACTIVITY_DELAY) {
            _inactiveEditTimer = 0;
            _editedTextSha1Hex = calculateSha1Hex(_text);
            if(_editedTextSha1Hex != _compiledSourceSha1Hex) {
                _compiledSourceSha1Hex = _editedTextSha1Hex;
                if(_compiler.compile(_text)) {
                    //if(_compiler.sha1Hex() != romSha1Hex) {

                    //}
                }
            }
        }
    }
}

void Editor::draw(Font& font, Rectangle rect)
{
    int lineNumber = int(_tosLine) - 1;
    float ypos = rect.y - 4;
    _visibleLines = uint32_t((rect.height - 6) / LINE_SIZE);
    _visibleCols = uint32_t(rect.width - 6*COLUMN_WIDTH - 6) / COLUMN_WIDTH;
    _rect = rect;
    _scrollPos = {-(float)_losCol * COLUMN_WIDTH, -(float)_tosLine * LINE_SIZE};
    gui::SetStyle(DEFAULT, BORDER_WIDTH, 0);
    gui::BeginScrollPanel(-1, {0,0,std::max(rect.width, (float)(_longestLineSize+8) * COLUMN_WIDTH), (float)std::max((uint32_t)_rect.height, uint32_t(_lines.size()+1)*LINE_SIZE)}, &_scrollPos);
    gui::SetStyle(DEFAULT, BORDER_WIDTH, 1);
    //gui::Space(rect.height -50);
    DrawRectangle(5*COLUMN_WIDTH, rect.y, 1, rect.height, GetColor(0x2f7486ff));
    while(lineNumber < int(_lines.size()) && ypos < rect.y + rect.height) {
        if(lineNumber >= 0) {
            DrawTextEx(font, TextFormat("%4d", lineNumber + 1), {rect.x, ypos}, 8, 0, LIGHTGRAY);
            drawTextLine(font, lineStart(lineNumber), lineEnd(lineNumber), {rect.x + 6 * COLUMN_WIDTH, ypos}, rect.width - 6 * COLUMN_WIDTH, _losCol);
        }
        ++lineNumber;
        ypos += LINE_SIZE;
    }
    _blinkTimer -= GetFrameTime();
    if(_blinkTimer < 0)
        _blinkTimer = BLINK_RATE;
    if(_blinkTimer >= BLINK_RATE/2) {
        auto cx = (_cursorX - _losCol) * COLUMN_WIDTH;
        auto cy = (_cursorY - _tosLine) * LINE_SIZE + LINE_SIZE - 4;
        if(cx >= 0 && cx < rect.width - 6*COLUMN_WIDTH - 3 && cy >= 0 && cy + 8 < rect.height)
            DrawRectangle(rect.x + 6*COLUMN_WIDTH + cx, rect.y + cy - 2, 2, LINE_SIZE, WHITE);
    }

    auto handle = verticalScrollHandle();
    //DrawRectangleRec(handle, {128,128,128, uint8_t(CheckCollisionPointRec(GetMousePosition(), handle) ? 200 : 128)});
#ifndef NDEBUG
    static char ModBuf[16];
    char* mbp = ModBuf;
    if(IsKeyDown(KEY_LEFT_ALT)) *mbp++ = 'a';
    if(IsKeyDown(KEY_RIGHT_ALT)) *mbp++ = 'A';
    if(IsKeyDown(KEY_LEFT_CONTROL)) *mbp++ = 'c';
    if(IsKeyDown(KEY_RIGHT_CONTROL)) *mbp++ = 'C';
    if(IsKeyDown(KEY_LEFT_SHIFT)) *mbp++ = 's';
    if(IsKeyDown(KEY_RIGHT_SHIFT)) *mbp++ = 'S';
    if(IsKeyDown(KEY_LEFT_SUPER)) *mbp++ = 'x';
    if(IsKeyDown(KEY_RIGHT_SUPER)) *mbp++ = 'X';
    *mbp = 0;
    if(mbp != ModBuf)
        DrawTextEx(font, ModBuf, {rect.x, rect.y}, 8, 0, RED);
    DrawTextEx(font, TextFormat("%d:%d", _selectionStart, _selectionEnd), {rect.x, rect.y+8}, 8, 0, RED);
#endif
    gui::EndScrollPanel();
    _tosLine = -_scrollPos.y / LINE_SIZE;
    _losCol = -_scrollPos.x / COLUMN_WIDTH;
}

Rectangle Editor::verticalScrollHandle()
{
    float scrollLength = _rect.height * _visibleLines / _lines.size();
    if(scrollLength < 6) scrollLength = 6;
    float step = (_rect.height - scrollLength) / std::max(float(_lines.size()) - float(_visibleLines), 1.0f);
    return {_rect.x + _rect.width - 5, _rect.y + step * _tosLine, 4, float(scrollLength)};
}

void Editor::highlightLine(const char* text, const char* end)
{
    static emu::OctoCompiler::Lexer lexer;
    lexer.setRange(text, end);
    _highlighting.resize(end - text);
    size_t index = 0;
    bool wasColon = false;
    const char* token;
    while(text < end && *text != '\n') {
        token = text;
        uint32_t cp = utf8::fetchCodepoint(text, end);
        if(cp == ' ')
            ++index;
        else if (cp == '#') {
            while(text < end)
                utf8::fetchCodepoint(text, end), _highlighting[index++].front = _colors[eCOMMENT];
        }
        else {
            auto start = index++;
            bool isColon = false;
            while(text < end && *text > ' ')
                utf8::fetchCodepoint(text, end), ++index;
            auto len = index - start;
            Color col = _colors[eNORMAL];
            if(cp == ':' && len == 1 || wasColon)
                col = _colors[eLABEL], isColon = true;
            else if(cp >= '0' && cp <= '9')
                col = _colors[eNUMBER];
            else if(len == 1 && (cp == 'i' || cp == 'I'))
                col = _colors[eREGISTER];
            else if(len == 2 && (cp == 'v' || cp == 'V') && isHexDigit(*(token + 1)))
                col = _colors[eREGISTER];
            else if(_opcodes.count(std::string(token, text - token)))
                col = _colors[eOPCODE];
            else if(_directives.count(std::string(token, text - token)))
                col = _colors[eDIRECTIVE];
            while(start < index)
                _highlighting[start++].front = col;
            wasColon = isColon;
        }
    }
}

void Editor::drawTextLine(Font& font, const char* text, const char* end, Vector2 position, float width, int columnOffset)
{
    uint32_t selStart = _selectionStart > _selectionEnd ? _selectionEnd : _selectionStart;
    uint32_t selEnd = _selectionStart > _selectionEnd ? _selectionStart : _selectionEnd;
    highlightLine(text, end);
    float textOffsetX = 0.0f;
    size_t index = 0;
    while(text < end && textOffsetX < width && *text != '\n') {
        uint32_t offset = text - _text.data();
        int codepointByteCount = 0;
        int codepoint = (int)utf8::fetchCodepoint(text, end);
        if(columnOffset <= 0) {
            if(offset >= selStart && offset < selEnd)
                DrawRectangleRec({position.x + textOffsetX, position.y - 2, 6, (float)LINE_SIZE}, selected);
            if ((codepoint != ' ') && (codepoint != '\t')) {
                DrawTextCodepoint(font, codepoint, (Vector2){position.x + textOffsetX, position.y}, 8, _highlighting[index].front);
            }
        }
        --columnOffset;
        if(columnOffset < 0)
            textOffsetX += COLUMN_WIDTH;
        text += codepointByteCount;
        ++index;
    }
    uint32_t offset = text - _text.data();
    if(textOffsetX < width && offset >= selStart && offset < selEnd)
        DrawRectangleRec({position.x + textOffsetX, position.y - 2, width - textOffsetX, (float)LINE_SIZE}, selected);
}

void Editor::safeInsert(uint32_t offset, const std::string& text)
{
    assert(("Text offset is actually in text", offset <= _text.size()));
    if(offset > _text.size()) {
        TraceLog(LOG_ERROR, "Trying to insert after end at offset: %d, (size: %d)", offset, (uint32_t)_text.size());
    }
    else {
        _text.insert(offset, text);
    }
}

void Editor::safeErase(uint32_t offset, uint32_t length)
{
    assert(("Text offset is actually in text", offset <= _text.size()));
    if(offset > _text.size()) {
        TraceLog(LOG_ERROR, "Trying to erase after end at offset: %d, (size: %d)", offset, (uint32_t)_text.size());
        return;
    }
    else if(offset + length > _text.size()) {
        TraceLog(LOG_WARNING, "Trying to erase until after end at offset: %d, length: %d, (size: %d)", offset, length, (uint32_t)_text.size());
    }
    _text.erase(offset, length);
}

void Editor::deleteSelectedText()
{
    if(_selectionStart != _selectionEnd) {
        uint32_t selStart = _selectionStart > _selectionEnd ? _selectionEnd : _selectionStart;
        uint32_t selEnd = _selectionStart > _selectionEnd ? _selectionStart : _selectionEnd;
        deleteText(selStart, selEnd - selStart);
    }
}

void Editor::deleteText(uint32_t offset, uint32_t length)
{
    if(length) {
        auto [cx, cy] = cursorFromOffset(offset);
        _undoStack.push({eDELETE, _editId, offsetFromCursor(), offset, offset + length, std::string(_text.begin() + offset, _text.begin() + offset + length)});
        safeErase(offset, length);
        _cursorX = _cursorVirtX = cx;
        _cursorY = cy;
        _selectionStart = _selectionEnd = 0;
        updateLineInfo(cy);
        _redoStack = std::stack<EditInfo>();
    }
}

uint32_t Editor::insert(const std::string& text)
{
    deleteSelectedText();
    auto offset = offsetFromCursor();
    _undoStack.push({eINSERT, _editId, offsetFromCursor(), offset, offset, text});
    safeInsert(offset, text);
    updateLineInfo(_cursorY);
    cursorRight(utf8::length(text));
    _redoStack = std::stack<EditInfo>();
    return text.size();
}

void Editor::undo()
{
    if(_undoStack.empty()) return;
    auto id = _undoStack.top().id;
    while(!_undoStack.empty() && _undoStack.top().id == id) {
        auto op = _undoStack.top().operation;
        if(op == eDELETE) {
            safeInsert(_undoStack.top().startOffset, _undoStack.top().text);
            auto startLine = cursorFromOffset(_undoStack.top().startOffset).second;
            auto [cx, cy] = cursorFromOffset(_undoStack.top().cursorPos);
            _cursorX = _cursorVirtX = cx; _cursorY = cy;
            _cursorChanged = true;
            updateLineInfo(startLine);
        }
        else if(op == eINSERT) {
            safeErase(_undoStack.top().startOffset, _undoStack.top().text.length());
            auto [cx, cy] = cursorFromOffset(_undoStack.top().cursorPos);
            _cursorX = _cursorVirtX = cx; _cursorY = cy;
            _cursorChanged = true;
            updateLineInfo(_cursorY);
        }
        _redoStack.push(_undoStack.top());
        _undoStack.pop();
    }
}

void Editor::redo()
{
    if(_redoStack.empty()) return;
    auto id = _redoStack.top().id;
    while(!_redoStack.empty() && _redoStack.top().id == id) {
        auto op = _redoStack.top().operation;
        if(op == eDELETE) {
            safeErase(_redoStack.top().startOffset, _redoStack.top().endOffset - _redoStack.top().startOffset);
            auto [cx, cy] = cursorFromOffset(_redoStack.top().startOffset);
            _cursorX = _cursorVirtX = cx; _cursorY = cy;
            _cursorChanged = true;
            updateLineInfo(_cursorY);
        }
        else if(op == eINSERT) {
            safeInsert(_redoStack.top().startOffset, _redoStack.top().text);
            auto startLine = cursorFromOffset(_redoStack.top().startOffset).second;
            auto [cx, cy] = cursorFromOffset(_redoStack.top().startOffset + _redoStack.top().text.length());
            _cursorX = _cursorVirtX = cx; _cursorY = cy;
            _cursorChanged = true;
            updateLineInfo(startLine);
        }
        _undoStack.push(_redoStack.top());
        _undoStack.top().id = _editId;
        _redoStack.pop();
    }
}
