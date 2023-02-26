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
#include <emulation/chip8compiler.hpp>
#include <emulation/utility.hpp>

#include <fmt/format.h>
//#define STB_IMAGE_IMPLEMENTATION
#include <nothings/stb_image.h>

#include <algorithm>
#include <charconv>
#include <fstream>
#include <unordered_set>

namespace {

class TextFile {
    std::ifstream file_stream;
    std::vector<std::streampos> linebegins;
    TextFile& operator=(TextFile& b) = delete;
public:
    TextFile(std::string filename)
        :file_stream(filename)
    {
        //this chunk stolen from Armen's,
        std::string s;
        //for performance
        s.reserve(128);
        while(file_stream) {
            linebegins.push_back(file_stream.tellg());
            std::getline(file_stream, s);
        }
    }
    TextFile(TextFile&& b)
        : file_stream(std::move(b.file_stream))
        , linebegins(std::move(b.linebegins))
    {}
    TextFile& operator=(TextFile&& b)
    {
        file_stream = std::move(b.file_stream);
        linebegins = std::move(b.linebegins);
        return *this;
    }
    std::string ReadNthLine(int N) {
        if (N >= linebegins.size()-1)
            throw std::runtime_error("File doesn't have that many lines!");
        std::string s;
        // clear EOF and error flags
        file_stream.clear();
        file_stream.seekg(linebegins[N]);
        std::getline(file_stream, s);
        return s;
    }
};

static std::string getNthLine(std::string file, int line)
{
    static std::unordered_map<std::string, TextFile> fileMap;
    auto iter = fileMap.find(file);
    if(iter == fileMap.end()) {
        iter = fileMap.emplace(file, file).first;
    }
    return iter->second.ReadNthLine(line);
}

}

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

OctoCompiler::OctoCompiler() = default;
OctoCompiler::~OctoCompiler() = default;

const CompileResult& OctoCompiler::compile(const std::string& filename)
{
    std::vector<std::string> files;
    files.push_back(filename);
    return compile(files);
}

struct FilePos {
    std::string file;
    int depth{0};
    int line{0};
};

static FilePos extractFilePos(std::string_view info)
{
    int depth{0};
    int line{0};
    const auto* start = info.data() + 7; // 7 für '#@line['
    auto result = std::from_chars(start, info.data() + info.size(), depth);
    if(result.ptr == start || *result.ptr != ',')
        return {};
    start = result.ptr + 1;
    result = std::from_chars(start, info.data() + info.size(), line);
    if(result.ptr == start || *result.ptr != ',')
        return {};
    return {std::string(result.ptr + 1, (info.data() + info.size()) - result.ptr - 1), depth, line};
}

const CompileResult& OctoCompiler::compile(const std::string& filename, const char* source, const char* end, bool needsPreprocess)
{
    std::string preprocessed;
    if(needsPreprocess) {
        preprocessFile(filename, source, end);
        if(_compileResult.resultType != CompileResult::eOK)
            return _compileResult;
        std::ostringstream preprocessedStream;
        dumpSegments(preprocessedStream);
        preprocessed = preprocessedStream.str();
        source = preprocessed.data();
        end = preprocessed.data() + preprocessed.size() + 1;
    }
    std::string_view sourceCode = {source, size_t(end - source)};
    _compiler.reset(new Chip8Compiler);
    if(_progress) _progress(1, "compiling ...");
    _compiler->compile(source, end);
    if(_compiler->isError()) {
        if(_generateLineInfos) {
            std::stack<FilePos> filePosStack;
            FilePos ep;
            int line = 1;
            int fileLine = 1;
            for(auto iter = sourceCode.begin(); iter != sourceCode.end() && line != _compiler->errorLine(); ++iter) {
                if(*iter == '\n') {
                    line++;
                    fileLine++;
                }
                if(sourceCode.end() - iter > 10 && *(iter + 1) == '#' && *(iter + 2) == '@') {
                    auto iter2 = iter + 1;
                    while(iter2 != sourceCode.end() && *iter2 != '\n' && *iter2 != ']')
                        ++iter2;
                    if(*iter2 == ']') {
                        ep = extractFilePos({iter+1, size_t(iter2 - iter - 1)});
                        if(!filePosStack.empty())
                            filePosStack.top().line = fileLine;
                        if(ep.line) {
                            while(!filePosStack.empty() && filePosStack.top().depth > ep.depth)
                                filePosStack.pop();
                            if(filePosStack.empty() || filePosStack.top().depth < ep.depth) {
                                filePosStack.push(ep);
                            }
                            else {
                                filePosStack.top() = ep;
                            }
                            fileLine = ep.line - 1;
                        }
                    }
                }
            }
            if(!ep.file.empty()) {
                int i = 0;
                while(!filePosStack.empty()) {
                    _compileResult.locations.push_back({
                        filePosStack.top().file,
                        i ? filePosStack.top().line : fileLine,
                        i ? 0 : _compiler->errorCol(),
                        i ? CompileResult::Location::eINCLUDED : CompileResult::Location::eROOT
                    });
                    filePosStack.pop();
                    ++i;
                }
                _compileResult.errorMessage = _compiler->rawErrorMessage();
                _compileResult.resultType = CompileResult::eERROR;
                return _compileResult;
            }
        }
        _compileResult.resultType = CompileResult::eERROR;
        _compileResult.errorMessage = _compiler->rawErrorMessage();
        _compileResult.locations = {{filename, _compiler->errorLine(), _compiler->errorCol(), CompileResult::Location::eROOT}};
        return _compileResult;
    }
    else {
        if(_progress) _progress(1, fmt::format("generated {} bytes of output", codeSize()));
    }
    _compileResult.reset();
    return _compileResult;
}

const CompileResult& OctoCompiler::compile(const std::vector<std::string>& files)
{
    for(const auto& file : files) {
        preprocessFile(file);
        if(_compileResult.resultType != CompileResult::eOK)
            return _compileResult;
    }
    std::string preprocessed;
    {
        std::ostringstream preprocessedStream;
        dumpSegments(preprocessedStream);
        preprocessed = preprocessedStream.str();
    }
    return compile(files.front(), preprocessed.data(), preprocessed.data() + preprocessed.size() + 1, false);
}

const CompileResult& OctoCompiler::preprocessFiles(const std::vector<std::string>& files)
{
    for(const auto& file : files) {
        preprocessFile(file);
        if(_compileResult.resultType != CompileResult::eOK)
            break;
    }
    return _compileResult;
}

void OctoCompiler::setIncludePaths(const std::vector<std::string>& paths)
{
    _includePaths.clear();
    for(const auto& path : paths)
        _includePaths.push_back(path);
}

uint32_t OctoCompiler::codeSize() const
{
    return _compiler ? _compiler->codeSize() : 0;
}

const uint8_t* OctoCompiler::code() const
{
    return _compiler ? _compiler->code() : nullptr;
}

const std::string& OctoCompiler::sha1Hex() const
{
    static const std::string dummy;
    return _compiler ? _compiler->sha1Hex() : dummy;
}
std::pair<uint32_t, uint32_t> OctoCompiler::addrForLine(uint32_t line) const
{
    return _compiler ? _compiler->addrForLine(line) : std::make_pair(0xFFFFFFFFu, 0xFFFFFFFFu);;
}

uint32_t OctoCompiler::lineForAddr(uint32_t addr) const
{
    return _compiler ? _compiler->lineForAddr(addr) : 0xFFFFFFFF;
}

const char* OctoCompiler::breakpointForAddr(uint32_t addr) const
{
    return _compiler ? _compiler->breakpointForAddr(addr) : nullptr;
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
    _token.prefixLine = _token.line;
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
                _token.prefixLine = _token.line;
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
            error(fmt::format("The number could not be parsed: {}", _token.raw));
        if(*start == ':') {
            if (_directives.count(_token.text))
                return Token::eDIRECTIVE;
            else if (_preprocessor.count(_token.text)) {
                // remove whitespace before preproc from prefix
                while(!_token.prefix.empty() && (_token.prefix.back() == ' ' || _token.prefix.back() == '\t'))
                       _token.prefix.remove_suffix(1);
                return Token::ePREPROCESSOR;
            }
            else if (len > 1 && *(start + 1) != '=')
                error(fmt::format("Unknown directive: {}", _token.raw));
        }
        if(*start == '{')
            return Token::eLCURLY;
        if(*start == '}')
            return Token::eRCURLY;
        if(std::strchr("+-*/%@|<>^!.=", *start))
            return Token::eOPERATOR;
        if(_reserved.count(_token.text)) {
            Token::Type type = len > 1 && std::isalpha(*(start + 1)) ? Token::eKEYWORD : Token::eOPERATOR;
            return type;
        }
        for(int i = 0; i < len; ++i) {
            if (!std::isalnum((uint8_t) * (start + i)) && *(start + i) != '-' && *(start + i) != '_') {
                _token.text = _token.raw;
                return Token::eSTRING;
            }
        }
        return Token::eIDENTIFIER;
    }
}

void OctoCompiler::Lexer::consumeRestOfLine()
{
    // remove whitespace and comments at end of preproc
    while(_srcPtr != _srcEnd && (*_srcPtr == ' ' || *_srcPtr == '\t'))
        _srcPtr++;
    if(_srcPtr != _srcEnd && *_srcPtr == '#') {
        while(_srcPtr != _srcEnd && *_srcPtr != '\n')
            _srcPtr++;
    }
    if(_srcPtr != _srcEnd && *_srcPtr == '\n') {
        _srcPtr++;
        _token.line++;
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
                error("Unexpected end after escaping backslash");
            }
        }
        else if(*_srcPtr == 10 || *_srcPtr == 13) {
            error("Expecting ending quote at end of string");
        }
        else {
            result.push_back(*_srcPtr);
        }
        ++_srcPtr;
    }
    if(_srcPtr == _srcEnd) {
        _token.length = _srcPtr - start;
        error("Expecting ending quote at end of string");
    }
    ++_srcPtr;
    _token.text = result;
    _token.raw = {start, size_t(_srcPtr - start)};
    return Token::eSTRING;
}

void OctoCompiler::Lexer::errorLocation(CompileResult& cr)
{
    auto* parent = _parent;
    cr.locations.clear();
    cr.locations.push_back({_filename, static_cast<int>(_token.line), static_cast<int>(_token.column), CompileResult::Location::eROOT});
    //std::string includes;
    while (parent) {
        cr.locations.push_back({parent->_filename, static_cast<int>(parent->_token.line), static_cast<int>(parent->_token.column), CompileResult::Location::eINCLUDED});
        //includes = fmt::format("{}:{}:{}: info: Included from\n", parent->_filename, parent->_token.line, parent->_token.column) + includes;
        parent = parent->_parent;
    }
    //return fmt::format("{}{}:{}:{}: ", includes, _filename, _token.line, _token.column);
}

std::vector<std::pair<int,std::string>> OctoCompiler::Lexer::locationStack() const
{
    std::vector<std::pair<int,std::string>> result;
    result.reserve(10);
    auto* parent = this;
    while (parent) {
        result.insert(result.begin(), {parent->_token.line, parent->_filename});
        parent = parent->_parent;
    }
    return result;
}

std::string OctoCompiler::Lexer::cutPrefixLines()
{
    auto lastNL = _token.prefix.find_last_of('\n');
    if(lastNL != std::string_view::npos) {
        auto result = std::string(_token.prefix.substr(0, lastNL+1));
        _token.prefix.remove_prefix(lastNL + 1);
        return result;
    }
    return {};
}

void OctoCompiler::Lexer::error(std::string msg)
{
    throw Exception(msg);
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
    _compileResult.reset();
}

void OctoCompiler::error(std::string msg)
{
    if(!_lexerStack.empty())
        lexer().errorLocation(_compileResult);
    else
        _compileResult.reset();
    _compileResult.errorMessage = msg;
    _compileResult.resultType = CompileResult::eERROR;
    throw std::runtime_error("");
}

void OctoCompiler::warning(std::string msg)
{
    if(!_lexerStack.empty())
        lexer().errorLocation(_compileResult);
    else
        _compileResult.reset();
    _compileResult.errorMessage = msg;
    _compileResult.resultType = CompileResult::eWARNING;
}

void OctoCompiler::info(std::string msg)
{
    if(!_lexerStack.empty())
        lexer().errorLocation(_compileResult);
    else
        _compileResult.reset();
    _compileResult.errorMessage = msg;
    _compileResult.resultType = CompileResult::eINFO;
}

const CompileResult& OctoCompiler::preprocessFile(const std::string& inputFile, const char* source, const char* end)
{
    try {
        _lexerStack.emplace(_lexerStack.empty() ? nullptr : &_lexerStack.top());
        std::shared_ptr<int> guard(NULL, [&](int *) { _lexerStack.pop(); });
        auto& lex = lexer();
        lex.setRange(inputFile, source, end);
        _currentSegment = eCODE;
        writeLineMarker();
        try {
            auto token = lex.nextToken();
            while (true) {
                if (token == Token::eEOF) {
                    writePrefix();
                    break;
                }
                if (token == Token::ePREPROCESSOR) {
                    writePrefix();
                    if (lex.expect(":include")) {
                        auto next = lex.nextToken();
                        if (next != Token::eSTRING)
                            error("Expected string after ':include'.");
                        auto newFile = fs::absolute(inputFile).parent_path() / lex.token().text;
                        auto extension = toLower(newFile.extension().string());
                        if (isImage(extension)) {
                            token = includeImage(newFile);
                        }
                        else {
                            flushSegment();
                            auto oldSeg = _currentSegment;
                            preprocessFile(newFile.string());
                            _currentSegment = oldSeg;
                            token = lex.nextToken(true);
                        }
                    }
                    else if (lex.expect(":segment")) {
                        auto next = lex.nextToken();
                        if (next != Token::eIDENTIFIER || (lex.token().raw != "data" && lex.token().raw != "code"))
                            error("Expected 'data' or 'code' after ':segment'.");
                        flushSegment();
                        _currentSegment = (lex.token().raw == "code" ? eCODE : eDATA);
                        token = lex.nextToken(true);
                        writeLineMarker();
                    }
                    else if (lex.expect(":if")) {
                        auto option = lex.nextToken();
                        if (option != Token::eIDENTIFIER)
                            error("{}: Identifier expected after ':if'.");
                        if (!_emitCode.empty() && _emitCode.top() != eACTIVE) {
                            _emitCode.push(eSKIP_ALL);
                        }
                        else {
                            _emitCode.push(isTrue(lex.token().raw) ? eACTIVE : eINACTIVE);
                        }
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":unless")) {
                        auto option = lex.nextToken();
                        if (option != Token::eIDENTIFIER)
                            error("Identifier expected after ':unless'.");
                        if (!_emitCode.empty() && _emitCode.top() != eACTIVE) {
                            _emitCode.push(eSKIP_ALL);
                        }
                        else {
                            _emitCode.push(!isTrue(lex.token().raw) ? eACTIVE : eINACTIVE);
                        }
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":else")) {
                        if (_emitCode.empty())
                            error("Use of ':else' without ':if' or ':unless'.");
                        _emitCode.top() = _emitCode.top() == eINACTIVE ? eACTIVE : eSKIP_ALL;
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":end")) {
                        if (_emitCode.empty())
                            error("Use of ':end' without ':if' or ':unless'.");
                        _emitCode.pop();
                        token = lex.nextToken(true);
                    }
                    else if (lex.expect(":dump-options")) {
                        // ignored for now
                        token = lex.nextToken(true);
                    }
                }
                else if (token == Token::eDIRECTIVE && lex.expect(":const") && (_emitCode.empty() || _emitCode.top() == eACTIVE)) {
                    writePrefix();
                    write(lex.token().raw);
                    auto nameToken = lex.nextToken();
                    if (nameToken != Token::eIDENTIFIER)
                        error("Identifier expected after ':const'.");
                    auto constName = lex.token().raw;
                    writePrefix();
                    write(lex.token().raw);
                    auto value = lex.nextToken();
                    if (value != Token::eIDENTIFIER && value != Token::eNUMBER)
                        error("Number or identifier expected after ':const <name>'.");
                    writePrefix();
                    write(lex.token().raw);
                    if (value == Token::eNUMBER) {
                        define(std::string(constName), lex.token().number);
                    }
                    token = lex.nextToken();
                }
                else {
                    writePrefix();
                    write(lex.token().raw);
                    token = lex.nextToken();
                }
            }
            flushSegment();
        }
        catch(Lexer::Exception& le) {
            _compileResult.errorMessage = le.errorMessage;
            _compileResult.resultType = CompileResult::eERROR;
            lex.errorLocation(_compileResult);
            return _compileResult;
        }
    }
    catch(std::exception& ex)
    {
        return _compileResult;
    }
    return _compileResult;
}

std::string OctoCompiler::resolveFile(const fs::path& file)
{
    if(file.is_absolute()) {
        std::error_code ec;
        if(fs::exists(file, ec))
            return file;
    }
    if(!_lexerStack.empty() && !lexer().filename().empty()) {
        std::error_code ec;
        auto newPath = fs::absolute(lexer().filename(), ec).parent_path() / file;
        if(!ec && fs::exists(newPath, ec))
            return newPath;
    }
    else {
        std::error_code ec;
        if(fs::exists(file, ec))
            return file;
    }
    for(const auto& path : _includePaths) {
        std::error_code ec;
        if(fs::exists(path / file, ec))
            return (path / file).string();
    }
    error(fmt::format("File not found: '{}'", file.string()));
    return "";
}

const CompileResult& OctoCompiler::preprocessFile(const std::string& inputFile)
{
    try {
        auto file = resolveFile(inputFile);
        if (_progress)
            _progress(_lexerStack.size() + 1, "preprocessing '" + inputFile + "' ...");
        auto content = loadTextFile(inputFile);
        preprocessFile(inputFile, content.data(), content.data() + content.size());
    }
    catch(std::runtime_error& ex)
    {
    }
    return _compileResult;
}

void OctoCompiler::doWrite(const std::string_view& text, int line)
{
    auto& lex = lexer();
    if(_generateLineInfos && (_collectLocationStack.empty() || _collectLocationStack.back().first != line || lex.filename() != _collectLocationStack.back().second)) {
        auto locationStack = lex.locationStack();
        locationStack.back().first = line;
        auto iterOld = _collectLocationStack.begin();
        auto iterNew = locationStack.begin();
        while(iterOld != _collectLocationStack.end() && iterNew != locationStack.end() && *iterOld == *iterNew) {
            iterOld++;
            iterNew++;
        }
        if (_emitCode.empty() || _emitCode.top() == eACTIVE) {
            auto depth = iterNew - locationStack.begin();
            _collect << "\n";
            while (iterNew != locationStack.end()) {
                _collect << fmt::format("#@line[{},{},{}]\n", ++depth, iterNew->first, iterNew->second);
                iterNew++;
            }
        }
        std::swap(_collectLocationStack, locationStack);
    }
    _collectLocationStack.back().first += std::count(text.begin(), text.end(), '\n');
    if (_emitCode.empty() || _emitCode.top() == eACTIVE)
        _collect << text;
}

void OctoCompiler::writePrefix()
{
    if(!_lexerStack.empty() && !lexer().token().prefix.empty()) {
        doWrite(lexer().token().prefix, lexer().token().prefixLine);
    }
}

void OctoCompiler::write(const std::string_view& text)
{
    if(!text.empty()) {
        doWrite(text, lexer().token().line);
    }
}

void OctoCompiler::writeLineMarker()
{
    return;
    if(_generateLineInfos) {
        auto& lex = lexer();
        _collect << lex.cutPrefixLines() << fmt::format("#@line[{},{},{}]\n", _lexerStack.size(), lex.token().line, lex.filename());
    }
}

void OctoCompiler::flushSegment()
{
    if(_currentSegment == eCODE)
        _codeSegments.push_back(_collect.str());
    else
        _dataSegments.push_back(_collect.str());
    _collect.str("");
    _collect.clear();
    _collectLocationStack.clear();
}

bool OctoCompiler::isImage(const std::string& extension)
{
    return extension == ".png" || extension == ".gif" || extension == ".bmp" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga";
}

OctoCompiler::Token::Type OctoCompiler::includeImage(std::string filename)
{
    int width,height,numChannels;
    int widthHint = -1, heightHint = -1;
    bool genLabels = true;
    bool debug = false;
    auto& lex = lexer();
    auto token = lex.nextToken(true);
    while(true) {
        if (token == Token::eSPRITESIZE) {
            auto sizes = split(lex.token().text, 'x');
            if(sizes.size() != 2)
                error(fmt::format("Bad sprite size for image include: '{}'", lex.token().raw));
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
        error(fmt::format("Could not load image: '{}'", filename));
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
        error(fmt::format("Image needs to be divisible by {}.", spriteWidth));
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

static int whitespaceLinesAtEnd(const std::string& text)
{
    int count = 0;
    for(auto iter = text.rbegin(); iter != text.rend(); ++iter) {
        if(!std::isspace((uint8_t)*iter))
            break;
        if(*iter == '\n')
            ++count;
    }
    return count;
}

static int whitespaceLinesAtStart(const std::string& text)
{
    int count = 0;
    for(auto iter = text.begin(); iter != text.end(); ++iter) {
        if(!std::isspace((uint8_t)*iter))
            break;
        if(*iter == '\n')
            ++count;
    }
    return count;
}

void OctoCompiler::dumpSegments(std::ostream& output)
{
    int endingWSLines = 2;
    for(auto& segment : _codeSegments) {
        if(!segment.empty()) {
            if(!_generateLineInfos) {
                auto sepLines = endingWSLines + whitespaceLinesAtStart(segment);
                for (int i = 0; i < 2 - sepLines; ++i)
                    output << '\n';
            }
            output << segment;
            if (segment.back() != '\n')
                output << '\n';
            if(!_generateLineInfos)
                endingWSLines = whitespaceLinesAtEnd(segment);
        }
    }
    for(auto& segment : _dataSegments) {
        if(!segment.empty()) {
            if(!_generateLineInfos) {
                auto sepLines = endingWSLines + whitespaceLinesAtStart(segment);
                for (int i = 0; i < 2 - sepLines; ++i)
                    output << '\n';
            }
            output << segment;
            if(segment.back() != '\n')
                output << '\n';
            if(!_generateLineInfos)
                endingWSLines = whitespaceLinesAtEnd(segment);
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
