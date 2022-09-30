//
// Created by Steffen Schümann on 02.08.22.
//
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

enum Token {
    STATEMENT = 65536, // #foo
    INTEGER, // 42
    REAL, // 3.14
    UNIT_NUMBER, // base36
    VARIABLE, // $foo
    STRING, // "foo" / 'foo' / r"foo"
    TEXT, // \w+
    SPACE,
    NEWLINE,
    EQUAL, // ==
    NOT_EQUAL, // !=
    GREATER_EQUAL, // >=
    LESS_EQUAL, // <=
    BOOL_AND, // &&
    BOOL_OR // ||
};


using Value = std::variant<std::nullptr_t, int32_t, std::string>;

namespace detail {

bool in_range(uint32_t c, uint32_t lo, uint32_t hi)
{
    return (static_cast<uint32_t>(c - lo) < (hi - lo + 1));
}

bool is_surrogate(uint32_t c)
{
    return in_range(c, 0xd800, 0xdfff);
}

bool is_high_surrogate(uint32_t c)
{
    return (c & 0xfffffc00) == 0xd800;
}

bool is_low_surrogate(uint32_t c)
{
    return (c & 0xfffffc00) == 0xdc00;
}

void appendUTF8(std::string& str, uint32_t unicode)
{
    if (unicode <= 0x7f) {
        str.push_back(static_cast<char>(unicode));
    }
    else if (unicode >= 0x80 && unicode <= 0x7ff) {
        str.push_back(static_cast<char>((unicode >> 6) + 192));
        str.push_back(static_cast<char>((unicode & 0x3f) + 128));
    }
    else if ((unicode >= 0x800 && unicode <= 0xd7ff) || (unicode >= 0xe000 && unicode <= 0xffff)) {
        str.push_back(static_cast<char>((unicode >> 12) + 224));
        str.push_back(static_cast<char>(((unicode & 0xfff) >> 6) + 128));
        str.push_back(static_cast<char>((unicode & 0x3f) + 128));
    }
    else if (unicode >= 0x10000 && unicode <= 0x10ffff) {
        str.push_back(static_cast<char>((unicode >> 18) + 240));
        str.push_back(static_cast<char>(((unicode & 0x3ffff) >> 12) + 128));
        str.push_back(static_cast<char>(((unicode & 0xfff) >> 6) + 128));
        str.push_back(static_cast<char>((unicode & 0x3f) + 128));
    }
    else {
        appendUTF8(str, 0xfffd);
    }
}

// Thanks to Bjoern Hoehrmann (https://bjoern.hoehrmann.de/utf-8/decoder/dfa/)
// and Taylor R Campbell for the ideas to this DFA approach of UTF-8 decoding;
// Generating debugging and shrinking my own DFA from scratch was a day of fun!
enum utf8_states_t { S_STRT = 0, S_RJCT = 8 };
unsigned consumeUtf8Fragment(const unsigned state, const uint8_t fragment, uint32_t& codepoint)
{
    static const uint32_t utf8_state_info[] = {
        // encoded states
        0x11111111u, 0x11111111u, 0x77777777u, 0x77777777u, 0x88888888u, 0x88888888u, 0x88888888u, 0x88888888u, 0x22222299u, 0x22222222u, 0x22222222u, 0x22222222u, 0x3333333au, 0x33433333u, 0x9995666bu, 0x99999999u,
        0x88888880u, 0x22818108u, 0x88888881u, 0x88888882u, 0x88888884u, 0x88888887u, 0x88888886u, 0x82218108u, 0x82281108u, 0x88888888u, 0x88888883u, 0x88888885u, 0u,          0u,          0u,          0u,
    };
    uint8_t category = fragment < 128 ? 0 : (utf8_state_info[(fragment >> 3) & 0xf] >> ((fragment & 7) << 2)) & 0xf;
    codepoint = (state ? (codepoint << 6) | (fragment & 0x3fu) : (0xffu >> category) & fragment);
    return state == S_RJCT ? static_cast<unsigned>(S_RJCT) : static_cast<unsigned>((utf8_state_info[category + 16] >> (state << 2)) & 0xf);
}

bool validUtf8(const std::string& utf8String)
{
    std::string::const_iterator iter = utf8String.begin();
    unsigned utf8_state = S_STRT;
    std::uint32_t codepoint = 0;
    while (iter < utf8String.end()) {
        if ((utf8_state = consumeUtf8Fragment(utf8_state, static_cast<uint8_t>(*iter++), codepoint)) == S_RJCT) {
            return false;
        }
    }
    if (utf8_state) {
        return false;
    }
    return true;
}

uint32_t utf8Increment(const char*& iter, const char* end)
{
    unsigned utf8_state = detail::S_STRT;
    std::uint32_t codepoint = 0;
    while (iter != end) {
        if ((utf8_state = detail::consumeUtf8Fragment(utf8_state, (uint8_t)*iter++, codepoint)) == detail::S_STRT) {
            return codepoint;
        }
        else if (utf8_state == detail::S_RJCT) {
            return 0xfffd;
        }
    }
    return 0xfffd;
}

}  // namespace detail

class LexerError : public std::runtime_error
{
public:
    LexerError(const std::string& msg) : std::runtime_error(msg) {}
};

class Lexer {
public:
    class Utf8Iterator
    {
    public:
        Utf8Iterator(const char*& iter, const char*& end)
            : _iter(iter), _next(iter), _to(end)
        {
            _codepoint = detail::utf8Increment(_next, _to);
        }

        Utf8Iterator(const Utf8Iterator& other)
            : _iter(other._iter), _next(other._next), _to(other._to), _codepoint(other._codepoint)
        {
        }

        Utf8Iterator& operator++()
        {
            _iter = _next;
            _codepoint = detail::utf8Increment(_next, _to);
            return *this;
        }

        Utf8Iterator operator++(int)
        {
            Utf8Iterator temp = *this;
            ++(*this);
            return temp;
        }

        char32_t operator*() const
        {
            return _codepoint;
        }

        bool operator==(const char* other) const
        {
            return _iter == other;
        }

        bool operator!=(const char* other) const
        {
            return _iter != other;
        }

        operator const char*() const
        {
            return _iter;
        }

    private:
        const char*& _iter;
        const char* _next;
        const char* _to;
        char32_t _codepoint = 0;
    };

    Lexer(const char* from, const char* to)
        : _from(from, to)
        , _to(to)
    {
    }

    Token next()
    {
        _start = _from;
        auto c = *_from;
        if(isDigit(c)) {
            parseNumber();
        }
        else if(c == '#') {
            parseStatement();
        }
        else if(c == '$') {
            parseVariable();
        }
        else if(c < 33)
        {
            parseWhitespace();
        }
        else if(c == '"' || c == '\'' || c == 'r') {
            parseString();
        }
        else if(c == '<' || c == '>' || c == '=' || c == '!') {
            ++_from;
            if(_from != _to && *_from == '=') {
                switch(c) {
                    case '<': _token = Token::LESS_EQUAL; break;
                    case '>': _token = Token::GREATER_EQUAL; break;
                    case '=': _token = Token::EQUAL; break;
                    default: _token = Token::NOT_EQUAL; break;
                }
                ++_from;
            }
            else {
                _token = static_cast<Token>(c);
            }
        }
        else if(isLetter(c) || c>255) {
            parseWord();
        }
        else {
            ++_from;
            _token = static_cast<Token>(c);
        }
        _lexeme = std::string(_start, (const char*)_from - _start);
        return _token;
    }

    bool eos() const { return _from == _to; }
    Token token() const { return _token; }
    const std::string& lexeme() const { return _lexeme; }
    const Value& value() const { return _value; }

private:
    static inline bool isDigit(char32_t c)
    {
        return c >= '0' && c <= '9';
    }
    static inline bool isBinDigit(char32_t c)
    {
        return c == '0' || c == '1';
    }
    static inline bool isLetter(char32_t c)
    {
        return (c >= 'a' && c <= 'z') || (c >='A' && c <= 'Z');
    }
    static inline bool isHexDigit(char32_t c)
    {
        return isDigit(c) || (c >= 'a' && c <= 'f') || (c >='A' && c <= 'F');
    }

    void error(const std::string& msg)
    {
        throw LexerError(msg);
    }

    void parseWord()
    {
        while(_from != _to && (isLetter(*_from) || isDigit(*_from) || *_from>255)) {
            ++_from;
        }
        _token = Token::TEXT;
    }

    void parseWhitespace()
    {
        _token = Token::SPACE;
        while(_from != _to && *_from < 33) {
            if(*_from == 10 || *_from == 13) {
                if(*_from == 10) {
                    ++_line;
                }
                _token = Token::NEWLINE;
            }
            ++_from;
        }

    }

    void parseString()
    {
        auto quote = *_from++;
        //bool isRegex = false;
        if(_from != _to && quote == 'r') {
            if(*_from != '"' && *_from != '\'') {
                parseWord();
            }
            //isRegex = true;
        }
        std::string result;
        while(_from != _to && *_from != quote) {
            if(*_from == '\\') {
                ++_from;
                if(_from != _to) {
                    auto c = *_from;
                    if(c == 'n') {
                        c = 10;
                    }
                    else if(c == 'r') {
                        c = 13;
                    }
                    else if(c == 't') {
                        c = 9;
                    }
                    ghc::detail::appendUTF8(result, c);
                }
                else {
                    error("Unerwartetes Ende nach Backslash");
                }
            }
            else if(*_from == 10 || *_from == 13) {
                error("Ungeschlossener String");
            }
            else {
                detail::appendUTF8(result, *_from);
            }
            ++_from;
        }
        if(_from == _to) {
            error("Ungeschlossener String");
        }
        ++_from;
        _value = result;
        _token = Token::STRING;
    }

    void parseVariable()
    {
        ++_from;
        while(_from != _to && isLetter(*_from)) {
            ++_from;
        }
        if((std::string::const_iterator)_from - _start > 1) {
            _token = Token::VARIABLE;
            return;
        }
        else if(_from != _to) {
            auto c = *_from;
            if(c == U'`' || c == 0xb4 || c == U'&' || c == U'+') {
                _token = Token::VARIABLE;
                return;
            }
        }
        _token = static_cast<Token>('$');
    }

    void parseReal(int64_t valInt)
    {
        double val = valInt;
        int e = 0;
        while(_from != _to && isDigit(*_from)) {
            val = val * 10 + (*_from++ - '0');
            ++e;
        }
        _value = val / std::pow(10.0, e);
        _token = Token::REAL;
    }

    void parseBase36(int64_t valInt)
    {
        while(_from != _to && isBase36(*_from)) {
            ++_from;
        }
        _value = std::string(_start, (std::string::const_iterator)_from);
        _token = Token::UNIT_NUMBER;
    }

    void parseNumber()
    {
        int64_t val = 0;
        int base = 10;
        if(*_from == '0' && _from + 1 < _to) {
            ++_from;
            if(*_from == 'x') {
                ++_from;
                while(_from != _to && isHexDigit(*_from)) {
                    val = val * 16 + (*_from++ - U'0');
                }
            }
            else if(*(_from+1) == 'b') {
                ++_from;
                base = 2;
            }
            else base = 8;
            ++_from;
        }
        while(_from != _to && isDigit(*_from)) {
            val = val * base + (*_from++ - U'0');
        }
        if(_from != _to) {
            if(*_from == U'.') {
                ++_from;
                parseReal(val);
                return;
            }
            else if(isBase36(*_from)) {
                parseBase36(val);
                return;
            }

        }
        if(val > std::numeric_limits<int32_t>::max() || val < std::numeric_limits<int32_t>::min()) {
            error("Integer out of range!");
        }
        _value = int32_t(val);
        _token = Token::INTEGER;
    }

    void parseStatement()
    {
        ++_from;
        while(_from != _to && *_from >= U'a' && *_from <= U'z') {
            ++_from;
        }
        _token = Token::STATEMENT;
    }

    Utf8Iterator _from;
    const char* _to;
    const char* _start;
    Token _token;
    std::string _lexeme;
    Value _value;
    int32_t _line;
};

