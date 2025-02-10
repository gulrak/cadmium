//---------------------------------------------------------------------------------------
// highlighter.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2025, Steffen Schümann <s.schuemann@pobox.com>
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

#include "highlighter.hpp"
#include <chiplet/octocompiler.hpp>
#include <chiplet/utility.hpp>

#include <ghc/utf8.hpp>
#include <rlguipp/rlguipp.hpp>

namespace utf8 = ghc::utf8;

std::unordered_set<std::string> Highlighter::_opcodes = {
    "!=", "&=", "+=", "-=", "-key", ":", ":=", ";", "<", "<<=", "<=", "=-", "==", ">", ">=", ">>=", "^=", "|=",
    "again", "audio", "bcd", "begin", "bighex", "buzzer", "clear", "delay", "else", "end", "hex", "hires", "if",
    "jump", "jump0", "key", "load", "loadflags", "loop", "lores", "native", "pitch", "plane", "random", "return",
    "save", "saveflags", "scroll-down", "scroll-left", "scroll-right", "scroll-up", "sprite", "then", "while"
};

std::unordered_set<std::string> Highlighter::_directives = {
    ":alias", ":assert", ":breakpoint", ":byte", ":calc", ":call", ":const", ":macro", ":monitor", ":next", ":org", ":pointer", ":proto", ":stringmode", ":unpack"
};

void Highlighter::highlightLine(const char* text, const char* end)
{
    static emu::OctoCompiler::Lexer lexer;
    lexer.setRange("", text, end);
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

void Highlighter::drawHighlightedTextLine(Font& font, const char* textRoot, const char* text, const char* end, Vector2 position, float width, int columnOffset, int lineHeight)
{
    uint32_t selStart = _selectionStart > _selectionEnd ? _selectionEnd : _selectionStart;
    uint32_t selEnd = _selectionStart > _selectionEnd ? _selectionStart : _selectionEnd;
    highlightLine(text, end);
    float textOffsetX = 0.0f;
    size_t index = 0;
    while(text < end && textOffsetX < width && *text != '\n') {
        if(columnOffset <= 0) {
            if (_selectionStart < _selectionEnd) {
                uint32_t offset = text - textRoot;
                if(offset >= selStart && offset < selEnd)
                    DrawRectangleRec({position.x + textOffsetX, position.y - 2, 6, (float)lineHeight}, _selected);
            }
            int codepoint = (int)utf8::fetchCodepoint(text, end);
            if ((codepoint != ' ') && (codepoint != '\t')) {
                DrawTextCodepointClipped(font, codepoint, (Vector2){position.x + textOffsetX, position.y}, 8, _highlighting[index].front);
            }
        }
        --columnOffset;
        if(columnOffset < 0)
            textOffsetX += COLUMN_WIDTH;
        ++index;
    }
    uint32_t offset = text - textRoot;
    if(textOffsetX < width && offset >= selStart && offset < selEnd)
        DrawRectangleRec({position.x + textOffsetX, position.y - 2, width - textOffsetX, (float)lineHeight}, _selected);
}