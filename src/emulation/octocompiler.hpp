//---------------------------------------------------------------------------------------
// src/emulation/octocompiler.hpp
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
#pragma once

#include <emulation/config.hpp>

namespace emu {

class OctoCompiler
{
public:
    struct Token {
        enum Type { eNONE, eNUMBER, eSTRING, eDIRECTIVE, eIDENTIFIER, eOPERATOR, eKEYWORD, ePREPROCESSOR, eEOF, eERROR };
        Type type;
        double number;
        std::string text;
        uint32_t line{0};
        uint32_t column{0};
        uint32_t length{0};
    };
    class Lexer {
    public:
        Lexer() = default;
        void setRange(const char* source, const char* end);
        Token nextToken();
    private:
        char peek() const { return _srcPtr < _srcEnd ? *_srcPtr : 0; }
        bool checkFor(const std::string& key) const { return _srcPtr + key.size() <= _srcEnd && std::strncmp(_srcPtr, key.data(), key.size()) == 0; }
        char get() { return _srcPtr < _srcEnd ? *_srcPtr++ : 0; }
        bool isPreprocessor() const;
        void skipWhitespace();
        const char* _srcPtr{nullptr};
        const char* _srcEnd{nullptr};
        uint32_t _line;
        uint32_t _column;
    };
    OctoCompiler() = default;
    void compile(const char* source, const char* end);
    std::string preprocess(const std::string& includePath);


private:
    Lexer _lex;
};

} // namespace emu
