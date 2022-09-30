//---------------------------------------------------------------------------------------
// src/editor.hpp
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

#include <emulation/chip8compiler.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_set>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include <raylib.h>

std::string GetClipboardTextX();
void SetClipboardTextX(std::string text);

class Editor
{
public:
    static constexpr float INACTIVITY_DELAY{1.0f};
    enum { eNORMAL, eNUMBER, eSTRING, eOPCODE, eREGISTER, eLABEL, eDIRECTIVE, eCOMMENT };
    inline static Color _colors[]{{200, 200, 200, 255}, {33, 210, 242, 255}, {238, 205, 51, 255}, {247, 83, 20, 255}, {219, 167, 39, 255}, {66, 176, 248, 255}, {183, 212, 247, 255}, {115, 154, 202, 255}};
    inline static Color selected{100, 100, 120, 255};
    Editor() : _alphabetKeys(26,0) { updateLineInfo(); }

    bool isEmpty() const { return _text.empty(); }
    const std::string& getText() const { return _text; }
    void setText(std::string text)
    {
        _selectionStart = 0; _selectionEnd = _text.size();
        insert(text);
        updateLineInfo();
        _losCol = _tosLine = _cursorX = _cursorY = 0;
        _cursorChanged = true;
    }

    uint32_t topOfScreen() const { return _tosLine; }
    uint32_t totalLines() const { return _lines.size(); }
    uint32_t line() const { return _cursorY + 1; }
    uint32_t column() const { return _cursorX + 1; }
    uint32_t editId() const { return _editId; }

    void updateLineInfo(uint32_t fromLine = 0);
    void update();

    void cursorLeft(int steps = 1);
    void cursorRight(int steps = 1);
    void cursorUp(int steps = 1);
    void cursorDown(int steps = 1);
    void cursorHome();
    void cursorEnd();
    void deleteSelectedText();
    void deleteText(uint32_t offset, uint32_t length);

    bool isKeyActivated(int key)
    {
        bool activated = IsKeyPressed(key) || (_isRepeat && IsKeyDown(key));
        if(activated)
            _blinkTimer = BLINK_RATE, _repeatTimer = IsKeyPressed(key) ? REPEAT_DELAY : REPEAT_RATE;
        return activated;
    }

    char* lineStart(uint32_t line)
    {
        return _text.data() + (line < _lines.size() ? _lines[line] : 0);
    }

    char* lineEnd(uint32_t line)
    {
        return _text.data() + (line + 1 < _lines.size() ? _lines[line+1] : _text.size());
    }

    char* pointerFromOffset(uint32_t offset);
    uint32_t offsetFromCursor();
    std::pair<int,int> cursorFromOffset(uint32_t offset);
    uint32_t insert(const std::string& text);
    int lineLength(uint32_t line);
    void undo();
    void redo();

    const Chip8Compiler& compiler() const { return _compiler; }

    std::pair<uint32_t, uint32_t> selection() const
    {
        return {_selectionStart > _selectionEnd ? _selectionEnd : _selectionStart, _selectionStart > _selectionEnd ? _selectionStart : _selectionEnd};
    }

    void draw(Font& font, Rectangle rect);
    void highlightLine(const char* text, const char* end);
    void drawTextLine(Font& font, const char* text, const char* end, Vector2 position, float width, int columnOffset);

protected:
    void safeInsert(uint32_t offset, const std::string& text);
    void safeErase(uint32_t offset, uint32_t length);
    template <typename F>
    bool cursorWrapper(F&& f)
    {
        bool shiftPressed = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if(shiftPressed && _selectionStart == _selectionEnd)
            _selectionStart = offsetFromCursor();
        f();
        if(shiftPressed)
            _selectionEnd = offsetFromCursor();
        return shiftPressed;
    }
    Rectangle verticalScrollHandle();
    void updateAlphaKeys();
    bool isAlphaPressed(char alpha) const
    {
        return alpha >= 'A' && alpha <= 'Z' && _alphabetKeys[alpha - 'A'] != 0;
    }

    struct ColorPair {
        Color front;
        Color back;
    };
    enum Operation { eINSERT, eDELETE };
    struct EditInfo {
        Operation operation;
        uint32_t id;
        uint32_t cursorPos;
        uint32_t startOffset;
        uint32_t endOffset;
        std::string text;
    };
    static constexpr int LINE_SIZE = 12;
    static constexpr int COLUMN_WIDTH = 6;
    const float BLINK_RATE = 0.8f;
    const float REPEAT_DELAY = 0.5f;
    const float REPEAT_RATE = 0.05f;
    std::string _text;
    std::vector<uint32_t> _lines;
    std::vector<ColorPair> _highlighting;
    std::stack<EditInfo> _undoStack;
    std::stack<EditInfo> _redoStack;
    std::vector<char> _alphabetKeys{};
    int _tosLine{0};
    int _losCol{0};
    int _cursorX{0};
    int _cursorVirtX{0};
    int _cursorY{0};
    uint32_t _visibleLines{0};
    uint32_t _visibleCols{0};
    uint32_t _selectionStart{0};
    uint32_t _selectionEnd{0};
    uint32_t _editId{0};
    uint32_t _longestLineSize{0};
    Rectangle _rect;
    Vector2 _scrollPos;
    float _blinkTimer{BLINK_RATE};
    float _repeatTimer{-1};
    bool _isRepeat{false};
    bool _cursorChanged{false};
    bool _mouseDownInText{false};
    Chip8Compiler _compiler;
    uint32_t _lastEditId{~0u};
    std::string _editedTextSha1Hex;
    std::string _compiledSourceSha1Hex;
    float _inactiveEditTimer{0};
    static std::unordered_set<std::string> _opcodes;
    static std::unordered_set<std::string> _directives;
};


