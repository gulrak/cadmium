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

#include <sstream>
#include <stack>
#include <string_view>
#include <map>
#include <variant>
#include <vector>

namespace emu {

class OctoCompiler
{
public:
    using Value = std::variant<int,double,std::string>;
    struct Token {
        enum Type { eNONE, eNUMBER, eSTRING, eDIRECTIVE, eIDENTIFIER, eOPERATOR, eKEYWORD, ePREPROCESSOR, eSPRITESIZE, eLCURLY, eRCURLY, eEOF, eERROR };
        Type type;
        double number;
        std::string text;
        std::string_view raw;
        std::string_view prefix;
        uint32_t line{0};
        uint32_t column{0};
        uint32_t length{0};
    };
    class Lexer {
    public:
        Lexer() = default;
        Lexer(Lexer* parent) : _parent(parent) {}
        void setRange(const std::string& filename, const char* source, const char* end);
        Token::Type nextToken(bool preproc = false);
        const Token& token() const { return _token; }
        bool expect(const std::string_view& literal) const;
        std::string errorLocation();
        const std::string& filename() const { return _filename; }
    private:
        char peek() const { return _srcPtr < _srcEnd ? *_srcPtr : 0; }
        bool checkFor(const std::string& key) const { return _srcPtr + key.size() <= _srcEnd && std::strncmp(_srcPtr, key.data(), key.size()) == 0; }
        char get() { return _srcPtr < _srcEnd ? *_srcPtr++ : 0; }
        bool isPreprocessor() const;
        Token::Type parseString();
        void skipWhitespace(bool preproc = false);
        Token::Type error(std::string msg, size_t length = 0);
        Lexer* _parent{nullptr};
        std::string _filename;
        const char* _srcPtr{nullptr};
        const char* _srcEnd{nullptr};
        Token _token;
    };
    OctoCompiler() = default;
    void reset();
    void compile(const std::string& filename, const char* source, const char* end);
    void preprocessFile(const std::string& inputFile, const char* source, const char* end, emu::OctoCompiler::Lexer* parentLexer = nullptr);
    void preprocessFile(const std::string& inputFile, emu::OctoCompiler::Lexer* parentLexer = nullptr);
    void dumpSegments(std::ostream& output);
    void define(std::string name, Value val = 1);
    bool isTrue(const std::string_view& name) const;
    void generateLineInfos(bool value) { _generateLineInfos = value; }

private:
    enum SegmentType { eCODE, eDATA };
    enum OutputControl { eACTIVE, eINACTIVE, eSKIP_ALL };
    static bool isImage(const std::string& filename);
    Token::Type includeImage(Lexer& lex, std::string filename);
    void write(const std::string_view& text);
    void writeLineMarker(Lexer& lex);
    void flushSegment();
    std::ostringstream _collect;
    SegmentType _currentSegment{eCODE};
    std::string _lineMarker;
    std::vector<std::string> _codeSegments;
    std::vector<std::string> _dataSegments;
    std::stack<OutputControl> _emitCode;
    std::map<std::string, Value, std::less<>> _symbols;
    bool _generateLineInfos{true};
};

} // namespace emu
