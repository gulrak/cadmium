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

#include <fmt/format.h>
//#define STB_IMAGE_IMPLEMENTATION
#include <nothings/stb_image.h>

#include <unordered_set>

namespace emu {

template<class... Ts> struct visitor : Ts... { using Ts::operator()...;  };
template<class... Ts> visitor(Ts...) -> visitor<Ts...>;

static std::unordered_set<std::string> _preprocessor = {
    ":include", ":segment", ":if", ":else", ":end", ":unless", ":dump-options"
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

void OctoCompiler::compile(const std::string& filename, const char* source, const char* end)
{
    Lexer lex;
    lex.setRange(filename, source, end);
    //_lexStack.push(lex);
}

#if 0
std::string OctoCompiler::preprocess(const std::string& includePath)
{
#ifdef _WIN32
    auto paths = split(includePath, ';');
#else
    auto paths = split(includePath, ':');
#endif
    return std::string();
}
#endif

void OctoCompiler::Lexer::setRange(const std::string& filename, const char* source, const char* end)
{
    _filename = filename;
    _srcPtr = source;
    _srcEnd = end;
    _token.line = 1;
    _token.column = 1;
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

void OctoCompiler::Lexer::skipWhitespace(bool preproc)
{
    auto start = _srcPtr;
    while (_srcPtr < _srcEnd && std::isspace(peek()) || peek() == '#')  {
        char c = get();
        if(c == '#') {
            while (c && c != '\n')
                c = get();
        }
        if(c == '\n') {
            ++_token.line;
            _token.column = 1;
            if(preproc) {
                start = _srcPtr;
                preproc = false;
            }
        }
    }
    _token.prefix = {start, size_t(_srcPtr - start)};
}

OctoCompiler::Token::Type OctoCompiler::Lexer::nextToken(bool preproc)
{
    skipWhitespace(preproc);

    if (peek() == '"') {
        return parseString();
    }
    else {
        const char* start = _srcPtr;
        while (peek() && !std::isspace(peek())) get();
        if (!peek())
            return Token::eEOF;

        char* end{};
        uint32_t len = _srcPtr - start;
        _token.text = std::string(start, _srcPtr);
        _token.raw = {start, size_t(_srcPtr - start)};
        _token.number = std::strtod(start, &end);
        if (end && end != start && end != _srcPtr) {
            if (*start == '0' && len > 2) {
                if (*(start + 1) == 'x')
                    _token.number = (double)std::strtol(start+2, &end, 16);
                if (*(start + 1) == 'b')
                    _token.number = (double)std::strtol(start+2, &end, 2);
            }
            else if (*start == '-' && len > 3 && *(start + 1) == '0') {
                if (*(start + 2) == 'x')
                    _token.number = -(double)std::strtol(start+3, &end, 16);
                if (*(start + 2) == 'b')
                    _token.number = -(double)std::strtol(start+3, &end, 2);
            }
            else if((_token.number == 8 || _token.number == 16) && *end == 'x') {
                return Token::eSPRITESIZE;
            }
        }
        if (end == _srcPtr)
            return Token::eNUMBER;
        else if (std::isdigit(*start))
            return Token::eERROR;
        if(*start == ':') {
            if (_directives.count(_token.text))
                return Token::eDIRECTIVE;
            else if (_preprocessor.count(_token.text)) {
                while(!_token.prefix.empty() && (_token.prefix.back() == ' ' || _token.prefix.back() == '\t'))
                       _token.prefix.remove_suffix(1);
                return Token::ePREPROCESSOR;
            }
            else if (len > 1 && *(start + 1) != '=')
                return error("Unknown directive: " + _token.text);
        }
        if(*start == '{')
            return Token::eLCURLY;
        if(*start == '}')
            return Token::eRCURLY;
        if(std::strchr("+-*/%", *start))
            return Token::eOPERATOR;
        if(_reserved.count(_token.text)) {
            Token::Type type = len > 1 && std::isalpha(*(start + 1)) ? Token::eKEYWORD : Token::eOPERATOR;
            return type;
        }
        for(int i = 0; i < len; ++i)
            if(!std::isalnum((uint8_t)*(start + i)) && *(start + i) != '-' && *(start + i) != '_')
                return error("Invalid identifier: " + _token.text);
        return Token::eIDENTIFIER;
    }
}

OctoCompiler::Token::Type OctoCompiler::Lexer::parseString()
{
    auto start = _srcPtr;
    auto quote = *_srcPtr++;
    std::string result;
    while(_srcPtr != _srcEnd && *_srcPtr != quote) {
        if(*_srcPtr == '\\') {
            if(++_srcPtr != _srcEnd) {
                auto c = *_srcPtr;
                if(c == 'n') {
                    c = 10;
                }
                else if(c == 'r') {
                    c = 13;
                }
                else if(c == 't') {
                    c = 9;
                }
                result.push_back(c);
            }
            else {
                return error("Unexpected end after escaping backslash", uint32_t(_srcPtr - start));
            }
        }
        else if(*_srcPtr == 10 || *_srcPtr == 13) {
            return error("Expecting ending quote at end of string", uint32_t(_srcPtr - start));
        }
        else {
            result.push_back(*_srcPtr);
        }
        ++_srcPtr;
    }
    if(_srcPtr == _srcEnd) {
        _token.length = _srcPtr - start;
        return error("Expecting ending quote at end of string", uint32_t(_srcPtr - start));
    }
    ++_srcPtr;
    _token.text = result;
    _token.raw = {start, size_t(_srcPtr - start)};
    return Token::eSTRING;
}

std::string OctoCompiler::Lexer::errorLocation()
{
    auto* parent = _parent;
    std::string includes;
    while (parent) {
        includes = fmt::format("INFO: Included from \"{}\":{}:{}:\n", parent->_filename, parent->_token.line, parent->_token.column) + includes;
        parent = parent->_parent;
    }
    return fmt::format("{}ERROR: File \"{}\":{}:{}", includes, _filename, _token.line, _token.column);
}

OctoCompiler::Token::Type OctoCompiler::Lexer::error(std::string msg, size_t length)
{
    _token.text = fmt::format("{}: {}", errorLocation(), msg);
    if(length)
        _token.length = length;
    return Token::eERROR;
}

bool OctoCompiler::Lexer::expect(const std::string_view& literal) const
{
    return _token.raw == literal;
}

void OctoCompiler::reset()
{
    _codeSegments.clear();
    _dataSegments.clear();
    _symbols.clear();
    _collect.str("");
    _collect.clear();
    _currentSegment = eCODE;
}

void OctoCompiler::preprocessFile(const std::string& inputFile, const char* source, const char* end, emu::OctoCompiler::Lexer* parentLexer)
{
    emu::OctoCompiler::Lexer lex(parentLexer);
    lex.setRange(inputFile, source, end);
    _currentSegment = eCODE;
    writeLineMarker(lex);
    auto token = lex.nextToken();
    while(true) {
        if(token == Token::eEOF)
            break;
        if(token == Token::eERROR)
            throw std::runtime_error(lex.token().text);
        if(token == Token::ePREPROCESSOR) {
            write(lex.token().prefix);
            if (lex.expect(":include")) {
                auto next = lex.nextToken();
                if (next != Token::eSTRING)
                    throw std::runtime_error("expected string after :include");
                auto newFile = fs::absolute(inputFile).parent_path() / lex.token().text;
                auto extension = toLower(newFile.extension().string());
                if (isImage(extension)) {
                    token = includeImage(lex, newFile);
                }
                else {
                    flushSegment();
                    auto oldSeg = _currentSegment;
                    preprocessFile(newFile.string(), &lex);
                    _currentSegment = oldSeg;
                    token = lex.nextToken(true);
                }
            }
            else if (lex.expect(":segment")) {
                auto next = lex.nextToken();
                if (next != Token::eIDENTIFIER || (lex.token().raw != "data" && lex.token().raw != "code"))
                    throw std::runtime_error(fmt::format("{}: expected data or code after :segment", lex.errorLocation()));
                flushSegment();
                _currentSegment = (lex.token().raw == "code" ? eCODE : eDATA);
                token = lex.nextToken(true);
            }
            else if (lex.expect(":if")) {
                auto option = lex.nextToken();
                if(option != Token::eIDENTIFIER)
                    throw std::runtime_error(fmt::format("{}: identifier expected after :if", lex.errorLocation()));
                if(!_emitCode.empty() && _emitCode.top() != eACTIVE) {
                    _emitCode.push(eSKIP_ALL);
                }
                else {
                    _emitCode.push( isTrue(lex.token().raw) ? eACTIVE : eINACTIVE);
                }
                token = lex.nextToken(true);
            }
            else if (lex.expect(":unless")) {
                auto option = lex.nextToken();
                if(option != Token::eIDENTIFIER)
                    throw std::runtime_error(fmt::format("{}: identifier expected after :if", lex.errorLocation()));
                if(!_emitCode.empty() && _emitCode.top() != eACTIVE) {
                    _emitCode.push(eSKIP_ALL);
                }
                else {
                    _emitCode.push( !isTrue(lex.token().raw) ? eACTIVE : eINACTIVE);
                }
                token = lex.nextToken(true);
            }
            else if (lex.expect(":else")) {
                if (_emitCode.empty())
                    throw std::runtime_error(fmt::format("{}: use of :else without :if or :unless", lex.errorLocation()));
                _emitCode.top() = _emitCode.top() == eINACTIVE ? eACTIVE : eSKIP_ALL;
                token = lex.nextToken(true);
            }
            else if (lex.expect(":end")) {
                if (_emitCode.empty())
                    throw std::runtime_error(fmt::format("{}: use of :end without :if or :unless", lex.errorLocation()));
                _emitCode.pop();
                token = lex.nextToken(true);
            }
            else if (lex.expect(":dump-options")) {
                // ignored for now
                token = lex.nextToken(true);
            }
            writeLineMarker(lex);
        }
        else if(token == Token::eDIRECTIVE && lex.expect(":const") && (_emitCode.empty() || _emitCode.top() == eACTIVE)) {
            write(lex.token().prefix);
            write(lex.token().raw);
            auto nameToken = lex.nextToken();
            if(nameToken != Token::eIDENTIFIER)
                throw std::runtime_error(fmt::format("{}: identifier expected after :const", lex.errorLocation()));
            auto constName = lex.token().raw;
            write(lex.token().prefix);
            write(lex.token().raw);
            auto value = lex.nextToken();
            if(value != Token::eIDENTIFIER && value != Token::eNUMBER)
                throw std::runtime_error(fmt::format("{}: number or identifier expected after :const name", lex.errorLocation()));
            write(lex.token().prefix);
            write(lex.token().raw);
            if(value == Token::eNUMBER) {
                define(std::string(constName), lex.token().number);
            }
            token = lex.nextToken();
        }
        else {
            write(fmt::format("{}{}", lex.token().prefix, lex.token().raw));
            token = lex.nextToken();
        }
    }
    flushSegment();
}

void OctoCompiler::preprocessFile(const std::string& inputFile, emu::OctoCompiler::Lexer* parentLexer)
{
    auto content = loadTextFile(inputFile);
    preprocessFile(inputFile, content.data(), content.data()+content.size(), parentLexer);
}

void OctoCompiler::write(const std::string_view& text)
{
    if(!text.empty()) {
        if (_emitCode.empty() || _emitCode.top() == eACTIVE)
            _collect << _lineMarker << text;
        _lineMarker.clear();
    }
}

void OctoCompiler::writeLineMarker(Lexer& lex)
{
    if(_generateLineInfos)
        _lineMarker = fmt::format("#@line[{},{}]\n", lex.token().line, lex.filename());
}

void OctoCompiler::flushSegment()
{
    if(_currentSegment == eCODE)
        _codeSegments.push_back(_collect.str());
    else
        _dataSegments.push_back(_collect.str());
    _collect.str("");
    _collect.clear();
}

bool OctoCompiler::isImage(const std::string& extension)
{
    return extension == ".png" || extension == ".gif" || extension == ".bmp" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga";
}

OctoCompiler::Token::Type OctoCompiler::includeImage(Lexer& lex, std::string filename)
{
    int width,height,numChannels;
    int widthHint = -1, heightHint = -1;
    bool genLabels = true;
    bool debug = false;
    auto token = lex.nextToken(true);
    while(true) {
        if (token == Token::eSPRITESIZE) {
            auto sizes = split(lex.token().text, 'x');
            if(sizes.size() != 2)
                throw std::runtime_error(fmt::format("{}: bad sprite size for image include: {}", lex.errorLocation(), lex.token().text));
            widthHint = std::stoi(sizes[0]);
            heightHint = std::stoi(sizes[1]);
        }
        else if(token == Token::eIDENTIFIER && lex.token().text == "no-labels")
        {
            genLabels = false;
        }
        else if(token == Token::eIDENTIFIER && lex.token().text == "debug")
        {
            debug = true;
        }
        else {
            break;
        }
        token = lex.nextToken(true);
    }
    auto* data = stbi_load(filename.c_str(), &width, &height, &numChannels, 1);
    if(!data) {
        throw std::runtime_error(fmt::format("{}: Could not load image: {}", lex.errorLocation(), filename));
    }
    int spriteWidth, spriteHeight;
    if(widthHint > 0) {
        spriteWidth = widthHint;
        spriteHeight = heightHint;
    }
    else if ( width == 16 && height == 16 ) {
        spriteWidth = spriteHeight = 16;
    } else {
        int numRows = 1;
        while ( height % numRows != 0 || height / numRows >= 16 )
            numRows++;
        spriteWidth = 8;
        spriteHeight = height / numRows;
    }
    auto name = fs::path(filename).filename().stem().string();
    if(width % spriteWidth != 0)
        throw std::runtime_error(fmt::format("{}: Image needs to be divisible by {}", lex.errorLocation(), spriteWidth));
    for (int y = 0; y < height; y += spriteHeight) {
        for (int x = 0; x < width; x += spriteWidth) {
            int index = y * width + x;
            if(genLabels)
                write(fmt::format("\n: {}-{}-{}\n", name, x/8, y/spriteHeight));
            for (int rows = 0; rows < spriteHeight; rows++) {
                write(" ");
                for (int cols = 0; cols < spriteWidth / 8; cols++) {
                    uint8_t val = 0;
                    for (uint8_t bit = 0x80, i = 0; bit > 0; bit >>= 1, ++i) {
                        auto pixel = data[index + rows * width + cols * 8 + i];
                        if(pixel > 0)
                            val |= bit;
                        //std::clog << (pixel > 0 ? "*" : ".");
                    }
                    write(fmt::format(" 0b{:08b}", val));
                }
                write("\n");
            }
        }
    }
    stbi_image_free(data);
    return token;
}

void OctoCompiler::dumpSegments(std::ostream& output)
{
    for(auto& segment : _codeSegments) {
        if(!segment.empty()) {
            output << segment;
            if (segment.back() != '\n')
                output << '\n';
        }
    }
    output << '\n';
    for(auto& segment : _dataSegments) {
        if(!segment.empty()) {
            output << segment;
            if(!segment.empty() && segment.back() != '\n')
                output << '\n';
        }
    }
}

void OctoCompiler::define(std::string name, Value val)
{
    _symbols[name] = val;
}

bool OctoCompiler::isTrue(const std::string_view& name) const
{
    auto iter = _symbols.find(name);
    if(iter != _symbols.end()) {
        return std::visit(visitor{
            [](int val) { return val != 0; },
            [](double val) { return std::fabs(val) > 0.0000001; },
            [](const std::string& val) { return !val.empty(); }
        }, iter->second);
    }
    return false;
}

} // namespace emu
