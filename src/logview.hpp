//---------------------------------------------------------------------------------------
// src/logview.hpp
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

#include <emulation/config.hpp>
#include <emulation/logger.hpp>

#include <raylib.h>

class LogView : public emu::Logger
{
public:
    static constexpr size_t HISTORY_SIZE = 1024;
    static constexpr int LINE_SIZE = 12;
    static constexpr int COLUMN_WIDTH = 6;
    LogView();
    ~LogView();

    void clear();

    void doLog(Source source, emu::cycles_t cycle, emu::cycles_t frameCycle, const char* msg) override;
    void draw(Font& font, Rectangle rect);

    static LogView* instance();

private:
    Rectangle drawToolArea();
    void drawTextLine(Font& font, int logLine, Vector2 position, float width, int columnOffset);
    struct LogEntry {
        emu::cycles_t _cycle{0};
        emu::cycles_t _frameCycle{0};
        uint64_t _hash{0};
        Source _source{eHOST};
        std::string _line;
    };
    std::vector<LogEntry> _logBuffer;
    std::string _filter;
    bool _invertedFilter{false};
    Rectangle _totalArea{};
    Rectangle _textArea{};
    Rectangle _toolArea{};
    size_t _writeIndex;
    size_t _usedSlots;
    int _tosLine{0};
    int _losCol{0};
    uint32_t _visibleLines{0};
    uint32_t _visibleCols{0};
    uint32_t _longestLineSize{256};
    Vector2 _scrollPos;
};

