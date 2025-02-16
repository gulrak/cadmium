//---------------------------------------------------------------------------------------
// src/logview.cpp
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

#include "logview.hpp"

#include <rlguipp/rlguipp.hpp>
#include <ghc/utf8.hpp>
#include <fmt/format.h>
//#include <iostream>

#if !defined(NDEBUG) && defined(FULL_CONSOLE_TRACE)
#include <iostream>
#endif

namespace utf8 = ghc::utf8;

LogView::LogView()
{
    _logBuffer.resize(HISTORY_SIZE);
    clear();
    setLogger(this);
}

LogView::~LogView()
{
    setLogger(nullptr);
}

void LogView::clear()
{
    std::unique_lock guard(_logMutex);
    for(auto& entry : _logBuffer) {
        entry = {};
    }
    _writeIndex = 0;
    _usedSlots = 0;
    _tosLine = 0;
    _losCol = 0;
}

void LogView::doLog(LogView::Source source, uint64_t cycle, FrameTime frameTime, const char* msg)
{
    std::unique_lock guard(_logMutex);
    auto& logEntry = _logBuffer[_writeIndex++];
    logEntry = {cycle, frameTime, 0, source, msg};
    if (_usedSlots < _logBuffer.size())
        ++_usedSlots;
    if (_writeIndex >= _logBuffer.size())
        _writeIndex = 0;
    _tosLine = _visibleLines >= _usedSlots ? 0 : _usedSlots - _visibleLines + 1;
#if !defined(NDEBUG) && defined(FULL_CONSOLE_TRACE)
    auto content = logEntry._source != eHOST ? fmt::format("[{:02x}:{:04x}] {}", (int)logEntry._frameTime.frame, (int)logEntry._frameTime.cycle, logEntry._line) : fmt::format("[    ] {}", logEntry._line);
    std::cout << content << std::endl;
#else
    //auto content = logEntry._source != eHOST ? fmt::format("[{:02x}:{:04x}] {}", (int)logEntry._frameTime.frame, (int)logEntry._frameTime.cycle, logEntry._line) : fmt::format("[    ] {}", logEntry._line);
    //std::cout << content << std::endl;
#endif
}

void LogView::draw(Font& font, Rectangle rect)
{
    std::unique_lock guard(_logMutex);
    using namespace gui;
    int lineNumber = int(_tosLine) - 1;
    _totalArea = rect;
    _toolArea = drawToolArea();
    _textArea = {_totalArea.x, _totalArea.y + _toolArea.height, _totalArea.width, _totalArea.height - _toolArea.height};
    float ypos = _textArea.y - 4;
    _visibleLines = uint32_t((_textArea.height - 6) / LINE_SIZE);
    _visibleCols = uint32_t(_textArea.width - 6*COLUMN_WIDTH - 6) / COLUMN_WIDTH;
    _scrollPos = {-(float)_losCol * COLUMN_WIDTH, -(float)_tosLine * LINE_SIZE};
    gui::SetStyle(DEFAULT, BORDER_WIDTH, 0);
    gui::BeginScrollPanel(-1, {0,0,std::max(_textArea.width, (float)(_longestLineSize+8) * COLUMN_WIDTH), (float)std::max((uint32_t)_textArea.height, uint32_t(_usedSlots+1)*LINE_SIZE)}, &_scrollPos);
    gui::SetStyle(DEFAULT, BORDER_WIDTH, 1);
    //gui::Space(rect.height -50);
    //DrawRectangle(5*COLUMN_WIDTH, _textArea.y, 1, _textArea.height, GetColor(0x2f7486ff));
    while(lineNumber < int(_usedSlots) && ypos < _textArea.y + _textArea.height) {
        if(lineNumber >= 0) {
            //DrawTextEx(font, TextFormat("%4d", lineNumber + 1), {_textArea.x, ypos}, 8, 0, LIGHTGRAY);
            drawTextLine(font, lineNumber, {_textArea.x + 2, ypos}, _textArea.width - 2, _losCol);
        }
        ++lineNumber;
        ypos += LINE_SIZE;
    }
    gui::EndScrollPanel();
    _tosLine = -_scrollPos.y / LINE_SIZE;
    _losCol = -_scrollPos.x / COLUMN_WIDTH;
}

Rectangle LogView::drawToolArea()
{
    return Rectangle();
}

void LogView::drawTextLine(const Font& font, int logLine, Vector2 position, float width, int columnOffset)
{
    if(logLine < _usedSlots) {
        logLine = _writeIndex - _usedSlots + logLine;
        if(logLine < 0)
            logLine += HISTORY_SIZE;

        float textOffsetX = 0.0f;
        size_t index = 0;
        auto& logEntry = _logBuffer[logLine];
        auto content = logEntry._source != eHOST ? fmt::format("[{:02x}:{:03x}] {}", (int)logEntry._frameTime.frame, (int)logEntry._frameTime.cycle, logEntry._line) : fmt::format("[    ] {}", logEntry._line);
        const char* text = content.data();
        const char* end = text + content.size();
        while(text < end && textOffsetX < width && *text != '\n') {
            //uint32_t offset = text - _text.data();
            int codepointByteCount = 0;
            int codepoint = (int)utf8::fetchCodepoint(text, end);
            if(columnOffset <= 0) {
                //if(offset >= selStart && offset < selEnd)
                //    DrawRectangleRec({position.x + textOffsetX, position.y - 2, 6, (float)LINE_SIZE}, selected);
                if ((codepoint != ' ') && (codepoint != '\t')) {
                    DrawTextCodepoint(font, codepoint, (Vector2){position.x + textOffsetX, position.y}, 8, {200, 200, 200, 255}/*_highlighting[index].front*/);
                }
            }
            --columnOffset;
            if(columnOffset < 0)
                textOffsetX += COLUMN_WIDTH;
            text += codepointByteCount;
            ++index;
        }
    }
}
