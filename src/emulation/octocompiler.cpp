//---------------------------------------------------------------------------------------
// src/emulation/octocompiler.cpp
//---------------------------------------------------------------------------------------
//
// Copyright (c) 2015, Steffen Schümann <s.schuemann@pobox.com>
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

#include <emulation/octocompiler.hpp>
#include <emulation/utility.hpp>

#include <unordered_set>

namespace emu {

static std::unordered_set<std::string> _preprocessor = {
    ":include", ":segment", "#code", ":if", ":elif", ":else", ":endif", ":unless", ":dump-options"
};

static std::unordered_set<std::string> _directives = {
    ":", ":alias", ":assert", ":breakpoint", ":byte", ":calc", ":call", ":const", ":macro", ":monitor", ":next", ":org", ":pointer", ":proto", ":stringmode", ":unpack"
};

static std::unordered_set<std::string> _reserved = {
    "!=", "&=", "+=", "-=", "-key", ":=", ";", "<", "<<=", "<=", "=-", "==", ">", ">=", ">>=", "^=", "|=",
    "again", "audio", "bcd", "begin", "bighex", "buzzer", "clear", "delay", "else", "end", "hex", "hires", "if",
    "jump", "jump0", "key", "load", "loadflags", "loop", "lores", "native", "pitch", "plane", "random", "return",
    "save", "saveflags", "scroll-down", "scroll-left", "scroll-right", "scroll-up", "sprite", "then", "while"
};

void OctoCompiler::compile(const char* source, const char* end)
{
    //_lex.setRange("", source, end);
}

std::string OctoCompiler::preprocess(const std::string& includePath)
{
#ifdef _WIN32
    auto paths = split(includePath, ';');
#else
    auto paths = split(includePath, ':');
#endif
    return std::string();
}

void OctoCompiler::Lexer::setRange(const std::string& filename, const char* source, const char* end)
{
    _filename = filename;
    _srcPtr = source;
    _srcEnd = end;
    _line = 0;
    _column = 0;
}

bool OctoCompiler::Lexer::isPreprocessor() const
{
    if(peek() == ':') {
        auto src = _srcPtr + 1;
        while(src < _srcEnd && std::isalpha(*src))
            ++src;
        if(_preprocessor.count(std::string(_srcPtr, src - _srcPtr))) {
            return true;
        }
    }
    return false;
}

void OctoCompiler::Lexer::skipWhitespace()
{
    while (_srcPtr < _srcEnd && std::isspace(peek()) || (peek() == '#' && !isPreprocessor()))  {
        char c = get();
        if(c == '#') {
            while (c && c != '\n')
                c = get();
        }
        if(c == '\n') {
            ++_line;
            _column = 0;
        }
    }
}

OctoCompiler::Token OctoCompiler::Lexer::nextToken()
{
    skipWhitespace();

    if (peek() == '"') {
        return {Token::eSTRING, 0, {}, _line, _column, 0};
    }
    else {
        const char* start = _srcPtr;
        //while (peek() && !std::isspace(peek())) get();
        if (!peek())
            return {Token::eEOF, 0, {}, _line, _column, 0};

        char* end{};
        uint32_t len = _srcPtr - start;
        auto text = std::string(start, _srcPtr);
        double number = std::strtod(start, &end);
        if (end != _srcPtr) {
            if (*start == '0' && len > 2) {
                if (*(start + 1) == 'x')
                    number = (double)std::strtol(start, &end, 16);
                if (*(start + 1) == 'b')
                    number = (double)std::strtol(start, &end, 2);
            }
            else if (*start == '-' && len > 3 && *(start + 1) == '0') {
                if (*(start + 2) == 'x')
                    number = -(double)std::strtol(start, &end, 16);
                if (*(start + 2) == 'b')
                    number = -(double)std::strtol(start, &end, 2);
            }
        }
        if (end == _srcPtr)
            return {Token::eNUMBER, number, {}, _line, _column, len};
        else if (std::isdigit(*start))
            return {Token::eERROR, 0, "Bad numeric literal: " + text, _line, _column, len};
        if(peek() == ':') {
            if (_directives.count(text))
                return {Token::eDIRECTIVE, 0, text, _line, _column, len};
            else
                return {Token::eERROR, 0, "Unknown directive: " + text, _line, _column, len};
        }
        if(peek() == '#') {
            return {Token::ePREPROCESSOR, 0, text, _line, _column, len};
        }
        if(_reserved.count(text)) {
            Token::Type type = len > 1 && std::isalpha(*(start + 1)) ? Token::eKEYWORD : Token::eOPERATOR;
            return {type, 0, text, _line, _column, len};
        }
        for(int i = 0; i < len; ++i)
            if(!std::isalnum((uint8_t)*(start + i)))
                return {Token::eERROR, 0, "Invalid identifier: " + text, _line, _column, len};
        return {Token::eIDENTIFIER, 0, text, _line, _column, len};
    }
}

} // namespace emu
