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
#include <emulation/utility.hpp>

#include <sstream>
#include <stack>
#include <string_view>
#include <map>
#include <memory>
#include <variant>
#include <vector>

namespace emu {

class Chip8Compiler;

class OctoCompiler
{
public:
    using ProgressHandler = std::function<void(int verbosity, std::string msg)>;
    using Value = std::variant<int,double,std::string>;
    struct Token {
        enum Type { eNONE, eNUMBER, eSTRING, eDIRECTIVE, eIDENTIFIER, eOPERATOR, eKEYWORD, ePREPROCESSOR, eSPRITESIZE, eLCURLY, eRCURLY, eEOF, eERROR };
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
    OctoCompiler();
    ~OctoCompiler();
    void reset();
    void compile(const std::string& filename, const char* source, const char* end);
    void compile(const std::string& filename);
    void compile(const std::vector<std::string>& files);
    void preprocessFile(const std::string& inputFile, const char* source, const char* end, emu::OctoCompiler::Lexer* parentLexer = nullptr);
    void preprocessFile(const std::string& inputFile, emu::OctoCompiler::Lexer* parentLexer = nullptr);
    void preprocessFiles(const std::vector<std::string>& files);
    void dumpSegments(std::ostream& output);
    void define(std::string name, Value val = 1);
    bool isTrue(const std::string_view& name) const;
    void generateLineInfos(bool value) { _generateLineInfos = value; }
    void setIncludePaths(const std::vector<std::string>& paths);
    void setProgressHandler(ProgressHandler handler) { _progress = handler; }
    uint32_t codeSize() const;
    const uint8_t* code() const;
    const std::string& sha1Hex() const;
    std::pair<uint32_t, uint32_t> addrForLine(uint32_t line) const;
    uint32_t lineForAddr(uint32_t addr) const;
    const char* breakpointForAddr(uint32_t addr) const;

private:
    enum SegmentType { eCODE, eDATA };
    enum OutputControl { eACTIVE, eINACTIVE, eSKIP_ALL };
    static bool isImage(const std::string& filename);
    Token::Type includeImage(Lexer& lex, std::string filename);
    void write(const std::string_view& text);
    void writeLineMarker(Lexer& lex);
    void error(Lexer& lex, std::string msg) const;
    void warning(Lexer& lex, std::string msg) const;
    void info(Lexer& lex, std::string msg) const;
    void flushSegment();
    std::string resolveFile(const fs::path& file, Lexer* lexer = nullptr) const;
    std::ostringstream _collect;
    SegmentType _currentSegment{eCODE};
    std::string _lineMarker;
    std::vector<std::string> _codeSegments;
    std::vector<std::string> _dataSegments;
    std::stack<OutputControl> _emitCode;
    std::map<std::string, Value, std::less<>> _symbols;
    std::vector<fs::path> _includePaths;
    std::unique_ptr<Chip8Compiler> _compiler;
    ProgressHandler _progress;
    int _includeDepth{0};
    bool _generateLineInfos{true};
};

} // namespace emu
